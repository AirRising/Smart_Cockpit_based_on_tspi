#pragma once

#include <QFile>
#include <QMutex>
#include <QQueue>
#include <QString>
#include <QWaitCondition>

#include <thread>

namespace sc {

// Asynchronous file logger.
//
// - Writes to /var/log/smart-cockpit/smart-cockpit.log by default and falls
//   back to ./logs when /var/log is not writable.
// - Rotates at 10 MB per file (keeps one .1 backup).
// - Never blocks the GUI thread: log() only enqueues and wakes the writer.
class AsyncLogger
{
public:
    enum Level {
        Info = 0,
        Warn,
        Error
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
    ~AsyncLogger();
    Q_DISABLE_COPY(AsyncLogger)

    struct Entry {
        Level level;
        QString message;
        qint64 tsMs;
    };

    void writerLoop();
    void rotateIfNeeded();

    std::thread m_thread;
    mutable QMutex m_mutex;
    QWaitCondition m_cond;
    QQueue<Entry> m_queue;
    QFile m_file;
    QString m_dir;
    Level m_minLevel = Info;
    bool m_running = false;
    bool m_stopRequested = false;
};

// Routes qDebug/qWarning/qCritical/qFatal through AsyncLogger.
void installQtMessageHandler();

} // namespace sc
