#include "CameraService.h"

#include <QDateTime>
#include <QDebug>

#include <cmath>

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>

namespace sc {

namespace {

constexpr float kTargetLuma = 40.0f;
constexpr float kGammaExponent = 1.0f / 2.2f;

// Rec.709 full-range YUV -> linear RGB. rkisp mainpath delivers full-range
// data (luma actually spans 0..255), so the limited-range BT.601 formula
// would crush the shadows and mis-scale the contrast.
inline void yuvToRgb(int y, int u, int v, int *r, int *g, int *b)
{
    const int d = u - 128;
    const int e = v - 128;
    *r = qBound(0, y + ((459 * e + 128) >> 8), 255);
    *g = qBound(0, y - ((55 * d + 136 * e + 128) >> 8), 255);
    *b = qBound(0, y + ((541 * d + 128) >> 8), 255);
}

// appsink new-sample callback (streaming thread). Delivers the frame to the
// service instance, which converts NV12 to RGBA there, then emits `frameReady`.
GstFlowReturn onNewSampleCb(GstAppSink *appsink, gpointer userData)
{
    auto *self = static_cast<CameraService *>(userData);
    GstSample *sample = gst_app_sink_pull_sample(appsink);
    if (sample) {
        self->handleSample(sample);
        gst_sample_unref(sample);
    }
    return GST_FLOW_OK;
}

} // namespace

CameraService::CameraService(QObject *parent)
    : QObject(parent)
{
    for (int i = 0; i < 256; ++i)
        m_gammaLut[i] = quint8(std::lround(255.0 * std::pow(i / 255.0, kGammaExponent)));
    resetTuning();

    m_pollTimer.setInterval(200);
    connect(&m_pollTimer, &QTimer::timeout, this, &CameraService::pollStream);
}

CameraService::~CameraService()
{
    stop();
}

bool CameraService::start(const QString &devicePath, int width, int height, int fps)
{
    if (m_running)
        return true;

    stop();
    gst_init(nullptr, nullptr);
    m_resolution = QSize(width, height);
    m_hardwareSource = !devicePath.isEmpty();
    m_devicePath = devicePath;
    m_fps = fps;
    resetTuning();

    QString pipelineText;
    if (devicePath.isEmpty()) {
        pipelineText = QStringLiteral(
                           "videotestsrc is-live=true pattern=ball ! "
                           "video/x-raw,format=NV12,width=%1,height=%2,framerate=%3/1 ! "
                           "appsink name=sink")
                           .arg(width).arg(height).arg(fps);
    } else {
        // io-mode=2 (纯 MMAP)，不是 4 (dmabuf)：实测同一条 pipeline，root 跑
        // io-mode=4 正常，普通用户跑就 "Failed to allocate required memory"，
        // 而 io-mode=2 两种身份都能出图。这里每帧都要逐像素拷进 QImage，
        // dmabuf 的零拷贝本来就没有收益。
        pipelineText = QStringLiteral(
                           "v4l2src device=%1 io-mode=2 ! "
                           "video/x-raw,width=%2,height=%3,framerate=%4/1 ! "
                           "videoconvert ! video/x-raw,format=NV12 ! "
                           "appsink name=sink")
                           .arg(devicePath).arg(width).arg(height).arg(fps);
    }

    GError *error = nullptr;
    m_pipeline = gst_parse_launch(pipelineText.toUtf8().constData(), &error);
    if (!m_pipeline || error) {
        const QString message = QString::fromUtf8(error ? error->message : "pipeline parse failed");
        if (error)
            g_error_free(error);
        stop();
        emit cameraError(message);
        return false;
    }

    m_appsink = gst_bin_get_by_name(GST_BIN(m_pipeline), "sink");
    if (!m_appsink) {
        stop();
        emit cameraError(QStringLiteral("appsink not found in pipeline"));
        return false;
    }
    m_bus = gst_element_get_bus(GST_ELEMENT(m_pipeline));
    m_lastFrameMs.storeRelease(QDateTime::currentMSecsSinceEpoch());

    // Low-latency capture settings.
    gst_app_sink_set_max_buffers(GST_APP_SINK(m_appsink), 2);
    gst_app_sink_set_drop(GST_APP_SINK(m_appsink), TRUE);
    gst_base_sink_set_sync(GST_BASE_SINK(m_appsink), FALSE);
    gst_base_sink_set_async_enabled(GST_BASE_SINK(m_appsink), TRUE);

    GstAppSinkCallbacks callbacks {};
    callbacks.new_sample = onNewSampleCb;
    gst_app_sink_set_callbacks(GST_APP_SINK(m_appsink), &callbacks, this, nullptr);

    if (gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_PLAYING) ==
        GST_STATE_CHANGE_FAILURE) {
        stop();
        emit cameraError(QStringLiteral("camera pipeline failed to start"));
        return false;
    }

    m_running = true;
    m_pollTimer.start();
    m_restartDelayMs = 2000;
    qInfo() << "Camera started" << (devicePath.isEmpty() ? QStringLiteral("(test source)")
                                                         : devicePath)
            << m_resolution;
    return true;
}

void CameraService::stop()
{
    m_pollTimer.stop();
    m_running = false;
    m_streamGeneration.fetchAndAddOrdered(1);
    if (m_appsink) {
        GstAppSinkCallbacks callbacks {};
        gst_app_sink_set_callbacks(GST_APP_SINK(m_appsink), &callbacks, nullptr, nullptr);
    }
    if (m_pipeline)
        gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_NULL);
    if (m_appsink) {
        gst_object_unref(GST_OBJECT(m_appsink));
        m_appsink = nullptr;
    }
    if (m_bus) {
        gst_object_unref(GST_OBJECT(m_bus));
        m_bus = nullptr;
    }
    if (m_pipeline) {
        gst_object_unref(GST_OBJECT(m_pipeline));
        m_pipeline = nullptr;
    }
    m_lastFrameMs.storeRelease(0);
    emit frameReady(QImage());
}

void CameraService::pollStream()
{
    if (!m_running)
        return;

    QString failure;
    GstMessage *message = gst_bus_pop_filtered(
        GST_BUS(m_bus), GstMessageType(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
    if (message) {
        if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
            GError *error = nullptr;
            gchar *debug = nullptr;
            gst_message_parse_error(message, &error, &debug);
            failure = QString::fromUtf8(error ? error->message : "camera stream failed");
            if (error)
                g_error_free(error);
            g_free(debug);
        } else {
            failure = QStringLiteral("camera stream ended");
        }
        gst_message_unref(message);
    } else if (lastFrameAgeMs() >= 2500) {
        failure = QStringLiteral("camera frame timed out");
    }
    if (!failure.isEmpty()) {
        qWarning() << "Camera stopped:" << failure;
        stop();
        emit cameraError(failure);
        // cameraError 没有接收方重开流；不自己重试的话，一次瞬时失败会让
        // 倒车影像在整个进程生命周期里停在 NO SIGNAL。
        if (!m_devicePath.isEmpty()) {
            QTimer::singleShot(m_restartDelayMs, this, [this]() {
                if (m_running)
                    return;
                m_restartDelayMs = qMin(m_restartDelayMs * 2, 20000);
                start(m_devicePath, m_resolution.width(), m_resolution.height(), m_fps);
            });
        }
    }
}

bool CameraService::isRunning() const
{
    return m_running;
}

qint64 CameraService::lastFrameAgeMs() const
{
    return QDateTime::currentMSecsSinceEpoch() - m_lastFrameMs.loadAcquire();
}

QSize CameraService::resolution() const
{
    return m_resolution;
}

void CameraService::resetTuning()
{
    m_wbGain[0] = m_wbGain[1] = m_wbGain[2] = 1.0f;
    m_lumaGain = 1.0f;
    m_statSum[0] = m_statSum[1] = m_statSum[2] = m_statSum[3] = 0;
    m_statCount = 0;
    rebuildTuningLuts();
}

void CameraService::rebuildTuningLuts()
{
    for (int i = 0; i < 256; ++i) {
        m_lumaLut[i] = quint8(qBound(0, int(std::lround(i * m_lumaGain)), 255));
        for (int c = 0; c < 3; ++c) {
            const int scaled = qBound(0, int(std::lround(i * m_wbGain[c])), 255);
            m_channelLut[c][i] = m_gammaLut[scaled];
        }
    }
}

void CameraService::handleSample(void *sampleRaw)
{
    const quint64 generation = m_streamGeneration.loadAcquire();
    auto *sample = static_cast<GstSample *>(sampleRaw);
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstCaps *caps = gst_sample_get_caps(sample);

    GstVideoInfo info;
    if (!buffer || !caps || !gst_video_info_from_caps(&info, caps))
        return;

    GstVideoFrame frame;
    if (!gst_video_frame_map(&frame, &info, buffer, GST_MAP_READ))
        return;

    const int w = GST_VIDEO_INFO_WIDTH(&info);
    const int h = GST_VIDEO_INFO_HEIGHT(&info);
    const guint8 *yPlane = static_cast<const guint8 *>(GST_VIDEO_FRAME_PLANE_DATA(&frame, 0));
    const guint8 *uvPlane = static_cast<const guint8 *>(GST_VIDEO_FRAME_PLANE_DATA(&frame, 1));
    const int yStride = GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0);
    const int uvStride = GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 1);

    // OV5695 delivers a landscape frame while the MIPI panel is 800x1280
    // portrait: rotate 90 deg clockwise while converting so the image fills
    // the panel instead of letterboxing into a 800x450 strip.
    QImage image(h, w, QImage::Format_RGBA8888);
    auto *dstBase = reinterpret_cast<quint8 *>(image.bits());
    const int dstStride = image.bytesPerLine();

    quint64 acc[4] = {0, 0, 0, 0};
    quint32 accCount = 0;
    const bool tune = m_hardwareSource;

    for (int j = 0; j < h; ++j) {
        const guint8 *yRow = yPlane + j * yStride;
        const guint8 *uvRow = uvPlane + (j / 2) * uvStride;
        const bool statRow = (j & 3) == 0;
        for (int i = 0; i < w; ++i) {
            const int y = yRow[i];
            const int u = uvRow[(i / 2) * 2];
            const int v = uvRow[(i / 2) * 2 + 1];

            int r = 0;
            int g = 0;
            int b = 0;
            yuvToRgb(tune ? m_lumaLut[y] : y, u, v, &r, &g, &b);
            if (tune) {
                if (statRow && (i & 3) == 0) {
                    int rawR = 0;
                    int rawG = 0;
                    int rawB = 0;
                    yuvToRgb(y, u, v, &rawR, &rawG, &rawB);
                    acc[0] += quint32(rawR);
                    acc[1] += quint32(rawG);
                    acc[2] += quint32(rawB);
                    acc[3] += quint32(y);
                    ++accCount;
                }
                r = m_channelLut[0][r];
                g = m_channelLut[1][g];
                b = m_channelLut[2][b];
            }

            // 顺时针 90°：源 (i, j) 落到目标 (h - 1 - j, i)。
            auto *dst = reinterpret_cast<quint32 *>(
                dstBase + size_t(i) * dstStride + size_t(h - 1 - j) * 4);
            *dst = 0xff000000U | (quint32(b) << 16) | (quint32(g) << 8) | quint32(r);
        }
    }

    gst_video_frame_unmap(&frame);
    m_lastFrameMs.storeRelease(QDateTime::currentMSecsSinceEpoch());

    if (tune && accCount > 0) {
        const float meanR = float(acc[0]) / accCount;
        const float meanG = float(acc[1]) / accCount;
        const float meanB = float(acc[2]) / accCount;
        const float meanY = float(acc[3]) / accCount;
        const float grey = (meanR + meanG + meanB) / 3.0f;

        const float raw[3] = {meanR, meanG, meanB};
        for (int c = 0; c < 3; ++c) {
            const float wanted = (raw[c] > 1.0f) ? grey / raw[c] : 1.0f;
            m_wbGain[c] = 0.75f * m_wbGain[c] + 0.25f * qBound(0.5f, wanted, 2.5f);
        }
        const float wantedLuma = (meanY > 1.0f) ? kTargetLuma / meanY : 1.0f;
        m_lumaGain = 0.85f * m_lumaGain + 0.15f * qBound(1.0f, wantedLuma, 6.0f);
        rebuildTuningLuts();
    }

    // Queued frames from a stopped stream must not repaint over the no-signal state.
    QMetaObject::invokeMethod(this, [this, image, generation]() {
        if (m_running && generation == m_streamGeneration.loadAcquire())
            emit frameReady(image);
    }, Qt::QueuedConnection);
}

} // namespace sc
