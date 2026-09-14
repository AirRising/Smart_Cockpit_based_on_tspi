#include "AsyncLogger.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>

namespace sc {

AsyncLogger &AsyncLogger::instance()
{
    static AsyncLogger logger;
    return logger;
}

AsyncLogger::~AsyncLogger()
{
    stop();
}

void AsyncLogger::start(const QString &directory, Level minLevel)
{
    QMutexLocker locker(&m_mutex);
    if (m_running)
        return;

    m_minLevel = minLevel;
    m_dir = directory;

    QDir().mkpath(m_dir);
    m_file.setFileName(QDir(m_dir).filePath(QStringLiteral("smart-cockpit.log")));
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        // Fall back to a writable location (e.g. host debugging without root).
        m_dir = QDir::current().filePath(QStringLiteral("logs"));
        QDir().mkpath(m_dir);
        m_file.setFileName(QDir(m_dir).filePath(QStringLiteral("smart-cockpit.log")));
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            qWarning() << "AsyncLogger: cannot open log file in" << m_dir;
            return;
        }
    }

    m_stopRequested = false;
    m_running = true;
    m_thread = std::thread(&AsyncLogger::writerLoop, this);
}

void AsyncLogger::stop()
{
    {
        QMutexLocker locker(&m_mutex);
        if (!m_running)
            return;
        m_stopRequested = true;
        m_cond.wakeAll();
    }
    m_thread.join();
    m_running = false;
    m_file.close();
}

void AsyncLogger::log(Level level, const QString &message)
{
    QMutexLocker locker(&m_mutex);
    if (!m_running || level < m_minLevel)
        return;
    m_queue.enqueue({level, message, QDateTime::currentMSecsSinceEpoch()});
    m_cond.wakeOne();
}

QString AsyncLogger::currentPath() const
{
    QMutexLocker locker(&m_mutex);
    return m_file.fileName();
}

void AsyncLogger::writerLoop()
{
    for (;;) {
        QList<Entry> batch;
        {
            QMutexLocker locker(&m_mutex);
            while (m_queue.isEmpty() && !m_stopRequested)
                m_cond.wait(&m_mutex);
            if (m_queue.isEmpty() && m_stopRequested)
                break;
            while (!m_queue.isEmpty())
                batch.append(m_queue.dequeue());
        }

        QTextStream stream(&m_file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        stream.setCodec("UTF-8");
#endif
        for (const Entry &e : batch) {
            const QString time = QDateTime::fromMSecsSinceEpoch(e.tsMs)
                                     .toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"));
            const char *levelName = e.level == Error ? "ERROR" : (e.level == Warn ? "WARN" : "INFO");
            stream << time << " [" << levelName << "] " << e.message << '\n';
        }
        stream.flush();
        rotateIfNeeded();
    }
}

void AsyncLogger::rotateIfNeeded()
{
    if (m_file.size() < 10 * 1024 * 1024)
        return;

    m_file.close();
    const QString path = m_file.fileName();
    QFile::remove(path + QStringLiteral(".1"));
    QFile::rename(path, path + QStringLiteral(".1"));
    m_file.setFileName(path);
    m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
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
