#pragma once

#include <QAtomicInteger>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QTimer>

namespace sc {

// GStreamer camera service for the reverse-view camera.
//
// Pipeline: v4l2src (OV5695) -> videoconvert -> NV12 -> appsink
// The appsink new-sample callback runs on GStreamer's streaming thread,
// converts NV12 to RGBA there, then delivers a QImage to the GUI thread via
// the queued `frameReady` signal. `videotestsrc` is used when no device is
// given (host debugging).
class CameraService : public QObject
{
    Q_OBJECT
public:
    explicit CameraService(QObject *parent = nullptr);
    ~CameraService() override;

    // devicePath empty -> videotestsrc demo source.
    bool start(const QString &devicePath = QString(), int width = 1280,
               int height = 720, int fps = 30);
    void stop();

    bool isRunning() const;
    qint64 lastFrameAgeMs() const;
    QSize resolution() const;

    // Invoked from GStreamer's streaming thread by the appsink callback.
    void handleSample(void *sample); // GstSample*

signals:
    void frameReady(const QImage &frame);
    void cameraError(const QString &message);

private:
    void pollStream();
    void resetTuning();
    void rebuildTuningLuts();

    void *m_pipeline = nullptr; // GstElement*
    void *m_appsink = nullptr;  // GstAppSink*
    void *m_bus = nullptr;      // GstBus*
    QTimer m_pollTimer;
    QSize m_resolution{1280, 720};
    QAtomicInteger<qint64> m_lastFrameMs{0};
    QAtomicInteger<quint64> m_streamGeneration{0};
    bool m_running = false;
    bool m_hardwareSource = false;
    QString m_devicePath;
    int m_fps = 30;
    int m_restartDelayMs = 2000;

    // 本板 rootfs 的 rkaiq 是 rk3588/ISP3.0 构建，在 RK3566 的 ISP2.1 上初始化
    // 即段错误，3A 守护进程起不来，ISP 因此不做白平衡也不写 gamma LUT。
    // 这里在既有的 NV12->RGBA 循环里用查找表补上灰世界白平衡与 gamma。
    quint8 m_gammaLut[256];
    quint8 m_lumaLut[256];
    quint8 m_channelLut[3][256];
    float m_wbGain[3];
    float m_lumaGain;
    quint64 m_statSum[4];
    quint32 m_statCount;
};

} // namespace sc
