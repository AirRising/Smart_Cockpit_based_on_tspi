#include <QtTest>
#include <QAtomicInteger>
#include <QImage>
#include <QSize>
#include <QTimer>

#include <gst/gst.h>
#include <gst/video/video.h>

#define private public
#include "CameraService.h"
#undef private

using namespace sc;

class TestCameraService : public QObject
{
    Q_OBJECT
private slots:
    void streamStartsAndStops();
    void terminalMessageClearsFrameAndAllowsRestart_data();
    void terminalMessageClearsFrameAndAllowsRestart();
    void stalledStreamClearsFrame();
    void queuedFrameCannotSurviveStop_data();
    void queuedFrameCannotSurviveStop();
    void failedStartAllowsRetry();
};

void TestCameraService::streamStartsAndStops()
{
    QImage lastFrame;
    CameraService camera;
    connect(&camera, &CameraService::frameReady, this,
            [&](const QImage &frame) { lastFrame = frame; });
    QVERIFY(camera.start(QString(), 64, 48, 10));
    QTRY_VERIFY(!lastFrame.isNull());
    QCOMPARE(lastFrame.size(), QSize(64, 48));
    QVERIFY(camera.isRunning());
    camera.stop();
    QVERIFY(!camera.isRunning());
    QVERIFY(lastFrame.isNull());
    QCoreApplication::processEvents();
    QVERIFY(lastFrame.isNull());
    camera.stop();
    QVERIFY(lastFrame.isNull());
}

void TestCameraService::terminalMessageClearsFrameAndAllowsRestart_data()
{
    QTest::addColumn<bool>("error");
    QTest::newRow("eos") << false;
    QTest::newRow("error") << true;
}

void TestCameraService::terminalMessageClearsFrameAndAllowsRestart()
{
    QFETCH(bool, error);
    QImage lastFrame;
    CameraService camera;
    QSignalSpy errors(&camera, &CameraService::cameraError);
    connect(&camera, &CameraService::frameReady, this,
            [&](const QImage &frame) { lastFrame = frame; });
    QVERIFY(camera.start(QString(), 64, 48, 10));
    QTRY_VERIFY(!lastFrame.isNull());
    if (error) {
        GError *failure = g_error_new_literal(GST_STREAM_ERROR, GST_STREAM_ERROR_FAILED,
                                              "test stream failure");
        GstMessage *message = gst_message_new_error(GST_OBJECT(camera.m_pipeline), failure,
                                                     "test fault injection");
        g_error_free(failure);
        QVERIFY(gst_bus_post(GST_BUS(camera.m_bus), message));
    } else {
        QVERIFY(gst_element_send_event(GST_ELEMENT(camera.m_pipeline), gst_event_new_eos()));
    }
    QTRY_VERIFY(!camera.isRunning());
    QCOMPARE(errors.count(), 1);
    QVERIFY(lastFrame.isNull());
    QCoreApplication::processEvents();
    QVERIFY(lastFrame.isNull());
    QVERIFY(camera.start(QString(), 64, 48, 10));
    QTRY_VERIFY(!lastFrame.isNull());
    QVERIFY(camera.isRunning());
    camera.stop();
}

void TestCameraService::stalledStreamClearsFrame()
{
    QImage lastFrame;
    CameraService camera;
    QSignalSpy errors(&camera, &CameraService::cameraError);
    connect(&camera, &CameraService::frameReady, this,
            [&](const QImage &frame) { lastFrame = frame; });
    QVERIFY(camera.start(QString(), 64, 48, 10));
    QTRY_VERIFY(!lastFrame.isNull());
    QVERIFY(gst_element_set_state(GST_ELEMENT(camera.m_pipeline), GST_STATE_PAUSED) !=
            GST_STATE_CHANGE_FAILURE);
    QTRY_VERIFY_WITH_TIMEOUT(!camera.isRunning(), 4000);
    QCOMPARE(errors.count(), 1);
    QVERIFY(lastFrame.isNull());
    QVERIFY(camera.start(QString(), 64, 48, 10));
    QTRY_VERIFY(!lastFrame.isNull());
    camera.stop();
}

void TestCameraService::queuedFrameCannotSurviveStop_data()
{
    QTest::addColumn<bool>("restart");
    QTest::newRow("stop") << false;
    QTest::newRow("restart") << true;
}

void TestCameraService::queuedFrameCannotSurviveStop()
{
    QFETCH(bool, restart);
    QImage lastFrame;
    bool staleFrameDelivered = false;
    CameraService camera;
    connect(&camera, &CameraService::frameReady, this, [&](const QImage &frame) {
        lastFrame = frame;
        if (frame.size() == QSize(2, 2))
            staleFrameDelivered = true;
    });
    QVERIFY(camera.start(QString(), 64, 48, 10));
    QTRY_VERIFY(!lastFrame.isNull());

    GstVideoInfo info;
    gst_video_info_init(&info);
    QVERIFY(gst_video_info_set_format(&info, GST_VIDEO_FORMAT_NV12, 2, 2));
    GstCaps *caps = gst_video_info_to_caps(&info);
    GstBuffer *buffer = gst_buffer_new_allocate(nullptr, info.size, nullptr);
    gst_buffer_memset(buffer, 0, 128, info.size);
    GstSample *sample = gst_sample_new(buffer, caps, nullptr, nullptr);
    camera.handleSample(sample);
    gst_sample_unref(sample);
    gst_buffer_unref(buffer);
    gst_caps_unref(caps);

    camera.stop();
    if (restart)
        QVERIFY(camera.start(QString(), 64, 48, 10));
    QCoreApplication::processEvents();
    QVERIFY(!staleFrameDelivered);
    if (restart)
        QTRY_VERIFY(!lastFrame.isNull());
    else
        QVERIFY(lastFrame.isNull());
    camera.stop();
}

void TestCameraService::failedStartAllowsRetry()
{
    CameraService camera;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSignalSpy errors(&camera, &CameraService::cameraError);
    camera.start(directory.filePath(QStringLiteral("missing-camera")), 64, 48, 10);
    QTRY_VERIFY(!camera.isRunning());
    QVERIFY(!errors.isEmpty());
    QVERIFY(camera.m_pipeline == nullptr);
    QVERIFY(camera.m_appsink == nullptr);
    QVERIFY(camera.m_bus == nullptr);
    QVERIFY(camera.start(QString(), 64, 48, 10));
    QSignalSpy frames(&camera, &CameraService::frameReady);
    QTRY_VERIFY(!frames.isEmpty());
    QVERIFY(!qvariant_cast<QImage>(frames.last().at(0)).isNull());
    camera.stop();
}

QTEST_GUILESS_MAIN(TestCameraService)
#include "tst_cameraservice.moc"
