#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QImage>

#include "AsyncLogger.h"
#include "CameraService.h"
#include "CarService.h"
#include "HealthMonitor.h"
#include "MainWindow.h"
#include "MediaService.h"
#include "SignalSimulator.h"
#include "SystemState.h"
#include "Theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("smart-cockpit"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setStyle(QStringLiteral("Fusion"));
    app.setStyleSheet(sc::Theme::styleSheet());

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Smart cockpit HMI (Qt Widgets, eglfs/KMS)"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption canOption(QStringLiteral("can"),
                                 QStringLiteral("SocketCAN interface (default: vcan0)"),
                                 QStringLiteral("iface"), QStringLiteral("vcan0"));
    QCommandLineOption cameraOption(QStringLiteral("camera"),
                                    QStringLiteral("Camera device; empty = videotestsrc"),
                                    QStringLiteral("dev"), QString());
    QCommandLineOption dbcOption(QStringLiteral("dbc"),
                                 QStringLiteral("Optional DBC file overriding built-in signals"),
                                 QStringLiteral("file"));
    QCommandLineOption logOption(QStringLiteral("log-dir"),
                                 QStringLiteral("Log directory"),
                                 QStringLiteral("dir"), QStringLiteral("/var/log/smart-cockpit"));
    QCommandLineOption mediaOption(QStringLiteral("media-dir"),
                                   QStringLiteral("Media directory"),
                                   QStringLiteral("dir"), QString());
    QCommandLineOption simOption(QStringLiteral("sim"),
                                 QStringLiteral("Drive the UI from a built-in signal "
                                                "simulator (no CAN bus / vcan needed)"));
    parser.addOption(canOption);
    parser.addOption(cameraOption);
    parser.addOption(dbcOption);
    parser.addOption(logOption);
    parser.addOption(mediaOption);
    parser.addOption(simOption);
    parser.process(app);

    sc::AsyncLogger::instance().start(parser.value(logOption));
    sc::installQtMessageHandler();
    qInfo() << "Smart cockpit starting, log file:" << sc::AsyncLogger::instance().currentPath();

    qRegisterMetaType<QImage>("QImage");
    qRegisterMetaType<sc::SystemState>("sc::SystemState");
    qRegisterMetaType<sc::ClimateState>("sc::ClimateState");

    if (!parser.value(mediaOption).isEmpty())
        qputenv("SMART_COCKPIT_MEDIA_DIR", parser.value(mediaOption).toUtf8());

    sc::CarService car;
    sc::MediaService media;
    sc::CameraService camera;
    sc::HealthMonitor health(&car, &camera, &media);

    if (parser.isSet(dbcOption)) {
        if (!car.parser()->loadDbcFile(parser.value(dbcOption)))
            qWarning() << "Failed to load DBC file:" << parser.value(dbcOption);
    }

    sc::MainWindow window(&car, &media, &camera);
    QObject::connect(&health, &sc::HealthMonitor::stateChanged,
                     &window, &sc::MainWindow::onSystemStateChanged);
    QObject::connect(&health, &sc::HealthMonitor::diagnosticsChanged,
                     &window, &sc::MainWindow::onDiagnostics);
    window.showFullScreen();

    // Built-in signal simulator: drive the whole HMI with synthetic frames, no
    // CAN socket or vcan needed (useful on boards whose kernel lacks CAN).
    sc::SignalSimulator *simulator = nullptr;
    if (parser.isSet(simOption)) {
        car.setSimulatedMode(true);
        simulator = new sc::SignalSimulator(&car);
        simulator->start();
        qInfo() << "Running in built-in signal simulation mode (--sim)";
    }

    media.init();
    if (!parser.isSet(simOption))
        car.start(parser.value(canOption));
    camera.start(parser.value(cameraOption));
    health.start();

    const int exitCode = app.exec();
    health.stop();
    if (simulator) {
        simulator->stop();
        delete simulator;
    }
    car.stop();
    camera.stop();
    media.shutdown();
    sc::AsyncLogger::instance().stop();
    return exitCode;
}
