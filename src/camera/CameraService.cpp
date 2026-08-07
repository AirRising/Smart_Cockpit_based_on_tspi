#include "CameraService.h"

#include <QDateTime>
#include <QDebug>

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>

namespace sc {

namespace {

// BT.601 limited-range YUV -> RGB (the standard for camera sensors).
inline quint32 yuvToRgb(int y, int u, int v)
{
    const int c = y - 16;
    const int d = u - 128;
    const int e = v - 128;
    int r = (298 * c + 409 * e + 128) >> 8;
    int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
    int b = (298 * c + 516 * d + 128) >> 8;
    r = qBound(0, r, 255);
    g = qBound(0, g, 255);
    b = qBound(0, b, 255);
    return 0xff000000U | (quint32(r) << 16) | (quint32(g) << 8) | quint32(b);
}

// appsink new-sample callback (streaming thread). Delivers the frame to the
// service instance, which converts NV12 -> RGBA and emits `frameReady`.
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
}

CameraService::~CameraService()
{
    stop();
}

bool CameraService::start(const QString &devicePath, int width, int height, int fps)
{
    if (m_running)
        return true;

    gst_init(nullptr, nullptr);
    m_resolution = QSize(width, height);

    QString pipelineText;
    if (devicePath.isEmpty()) {
        pipelineText = QStringLiteral(
                           "videotestsrc is-live=true pattern=ball ! "
                           "video/x-raw,format=NV12,width=%1,height=%2,framerate=%3/1 ! "
                           "appsink name=sink")
                           .arg(width).arg(height).arg(fps);
    } else {
        pipelineText = QStringLiteral(
                           "v4l2src device=%1 io-mode=4 ! "
                           "video/x-raw,width=%2,height=%3,framerate=%4/1 ! "
                           "videoconvert ! video/x-raw,format=NV12 ! "
                           "appsink name=sink")
                           .arg(devicePath).arg(width).arg(height).arg(fps);
    }

    GError *error = nullptr;
    m_pipeline = gst_parse_launch(pipelineText.toUtf8().constData(), &error);
    if (!m_pipeline || error) {
        emit cameraError(QString::fromUtf8(error ? error->message : "pipeline parse failed"));
        if (error)
            g_error_free(error);
        return false;
    }

    m_appsink = gst_bin_get_by_name(GST_BIN(m_pipeline), "sink");
    if (!m_appsink) {
        emit cameraError(QStringLiteral("appsink not found in pipeline"));
        gst_object_unref(GST_OBJECT(m_pipeline));
        m_pipeline = nullptr;
        return false;
    }

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
        emit cameraError(QStringLiteral("camera pipeline failed to start"));
        stop();
        return false;
    }

    m_running = true;
    qInfo() << "Camera started" << (devicePath.isEmpty() ? QStringLiteral("(test source)")
                                                         : devicePath)
            << m_resolution;
    return true;
}

void CameraService::stop()
{
    if (!m_pipeline)
        return;

    // Take the callbacks off first so no new-sample callback can run while we
    // tear the pipeline down.
    if (m_appsink) {
        gst_app_sink_set_callbacks(GST_APP_SINK(m_appsink), nullptr, nullptr, nullptr);
        gst_object_unref(GST_OBJECT(m_appsink));
        m_appsink = nullptr;
    }
    gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(m_pipeline));
    m_pipeline = nullptr;
    m_running = false;
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

void CameraService::handleSample(void *sampleRaw)
{
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

    QImage image(w, h, QImage::Format_RGBA8888);
    auto *dst = reinterpret_cast<quint32 *>(image.bits());
    const int dstStride = image.bytesPerLine();

    for (int j = 0; j < h; ++j) {
        const guint8 *yRow = yPlane + j * yStride;
        const guint8 *uvRow = uvPlane + (j / 2) * uvStride;
        quint32 *dstRow = reinterpret_cast<quint32 *>(
            reinterpret_cast<quint8 *>(dst) + j * dstStride);
        for (int i = 0; i < w; ++i) {
            const int u = uvRow[(i / 2) * 2];
            const int v = uvRow[(i / 2) * 2 + 1];
            dstRow[i] = yuvToRgb(yRow[i], u, v);
        }
    }

    gst_video_frame_unmap(&frame);
    m_lastFrameMs.storeRelease(QDateTime::currentMSecsSinceEpoch());
    emit frameReady(image);
}

} // namespace sc
