#pragma once

#include <QString>

#include "AsyncLoggerCore.h"

namespace sc {

// Thin Qt adapter over the pure C++ AsyncLoggerCore.
//
// All queuing/rotation/file I/O lives in AsyncLoggerCore (no Qt); this class
// only bridges QString and installs the qDebug/qWarning message handler.
class AsyncLogger
{
public:
    enum Level {
        Info = AsyncLoggerCore::Info,
        Warn = AsyncLoggerCore::Warn,
        Error = AsyncLoggerCore::Error
    };

    static AsyncLogger &instance();

    void start(const QString &directory = QStringLiteral("/var/log/smart-cockpit"),
               Level minLevel = Info);
    void stop();

    void log(Level level, const QString &message);
    void info(const QString &message)  { log(Info, message); }
    void warn(const QString &message)  { log(Warn, message); }
    void error(const QString &message) { log(Error, message); }

    QString currentPath() const;

private:
    AsyncLogger() = default;
    AsyncLogger(const AsyncLogger &) = delete;
    AsyncLogger &operator=(const AsyncLogger &) = delete;
};

// Routes qDebug/qWarning/qCritical/qFatal through AsyncLogger.
void installQtMessageHandler();

} // namespace sc
