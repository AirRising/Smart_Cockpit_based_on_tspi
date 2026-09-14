#pragma once

#include <QAtomicInteger>
#include <QImage>
#include <QObject>
#include <QSize>

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
    void *m_pipeline = nullptr; // GstElement*
    void *m_appsink = nullptr;  // GstAppSink*
    QSize m_resolution{1280, 720};
    QAtomicInteger<qint64> m_lastFrameMs{0};
    bool m_running = false;
};

} // namespace sc
