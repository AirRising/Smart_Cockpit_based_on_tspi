#include "AsyncLogger.h"

#include <QDebug>

namespace sc {

AsyncLogger &AsyncLogger::instance()
{
    static AsyncLogger logger;
    return logger;
}

void AsyncLogger::start(const QString &directory, Level minLevel)
{
    AsyncLoggerCore::instance().start(
        directory.toStdString(),
        static_cast<AsyncLoggerCore::Level>(minLevel));
}

void AsyncLogger::stop()
{
    AsyncLoggerCore::instance().stop();
}

void AsyncLogger::log(Level level, const QString &message)
{
    AsyncLoggerCore::instance().log(
        static_cast<AsyncLoggerCore::Level>(level), message.toStdString());
}

QString AsyncLogger::currentPath() const
{
    return QString::fromStdString(AsyncLoggerCore::instance().currentPath());
}

void installQtMessageHandler()
{
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &, const QString &msg) {
        switch (type) {
        case QtDebugMsg:    AsyncLogger::instance().info(msg); break;
        case QtInfoMsg:     AsyncLogger::instance().info(msg); break;
        case QtWarningMsg:  AsyncLogger::instance().warn(msg); break;
        case QtCriticalMsg:
        case QtFatalMsg:    AsyncLogger::instance().error(msg); break;
        }
    });
}

} // namespace sc
