#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

// Pure C++ (standard library only) asynchronous file logger. No Qt headers
// here: the Qt-facing singleton and qDebug routing live in AsyncLogger.h.

namespace sc {

class AsyncLoggerCore
{
public:
    enum Level {
        Info = 0,
        Warn,
        Error
    };

    static AsyncLoggerCore &instance();

    // Writes to `directory`/smart-cockpit.log, falling back to ./logs when the
    // requested directory is not writable. Rotates at 10 MB (keeps one .1 backup).
    void start(const std::string &directory = "/var/log/smart-cockpit",
               Level minLevel = Info);
    void stop();

    void log(Level level, const std::string &message);
    void info(const std::string &message)  { log(Info, message); }
    void warn(const std::string &message)  { log(Warn, message); }
    void error(const std::string &message) { log(Error, message); }

    std::string currentPath() const;

private:
    AsyncLoggerCore() = default;
    ~AsyncLoggerCore();
    AsyncLoggerCore(const AsyncLoggerCore &) = delete;
    AsyncLoggerCore &operator=(const AsyncLoggerCore &) = delete;

    struct Entry {
        Level level;
        std::string message;
        std::int64_t tsMs;
    };

    bool openFile(const std::string &path);
    void writerLoop();
    void rotateIfNeeded();

    std::thread m_thread;
    mutable std::mutex m_mutex;
    std::condition_variable m_cond;
    std::deque<Entry> m_queue;
    std::ofstream m_file;
    std::string m_path;
    Level m_minLevel = Info;
    bool m_running = false;
    bool m_stopRequested = false;
};

} // namespace sc
