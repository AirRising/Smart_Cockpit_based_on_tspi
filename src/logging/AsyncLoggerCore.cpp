#include "AsyncLoggerCore.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <sstream>

namespace sc {

namespace {

constexpr std::uintmax_t kRotateBytes = 10 * 1024 * 1024;

std::int64_t nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
        .count();
}

std::string formatTimestamp(std::int64_t ms)
{
    const std::time_t seconds = static_cast<std::time_t>(ms / 1000);
    const int millis = static_cast<int>(ms % 1000);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &seconds);
#else
    localtime_r(&seconds, &tm);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);

    std::ostringstream oss;
    oss << buffer << '.';
    oss.fill('0');
    oss.width(3);
    oss << millis;
    return oss.str();
}

} // namespace

AsyncLoggerCore &AsyncLoggerCore::instance()
{
    static AsyncLoggerCore logger;
    return logger;
}

AsyncLoggerCore::~AsyncLoggerCore()
{
    stop();
}

bool AsyncLoggerCore::openFile(const std::string &path)
{
    m_file.open(path, std::ios::out | std::ios::app);
    return m_file.is_open();
}

void AsyncLoggerCore::start(const std::string &directory, Level minLevel)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running)
        return;

    m_minLevel = minLevel;

    std::error_code ec;
    std::filesystem::create_directories(directory, ec);

    std::string path = directory + "/smart-cockpit.log";
    if (!openFile(path)) {
        // Fall back to a writable location (e.g. host debugging without root).
        const std::string fallback =
            (std::filesystem::current_path(ec) / "logs").string();
        std::filesystem::create_directories(fallback, ec);
        path = fallback + "/smart-cockpit.log";
        if (!openFile(path)) {
            std::cerr << "AsyncLogger: cannot open log file in " << fallback
                      << std::endl;
            return;
        }
    }

    m_path = path;
    m_stopRequested = false;
    m_running = true;
    m_thread = std::thread(&AsyncLoggerCore::writerLoop, this);
}

void AsyncLoggerCore::stop()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_running)
            return;
        m_stopRequested = true;
        m_cond.notify_all();
    }
    if (m_thread.joinable())
        m_thread.join();
    m_running = false;
    if (m_file.is_open())
        m_file.close();
}

void AsyncLoggerCore::log(Level level, const std::string &message)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running || level < m_minLevel)
        return;
    m_queue.push_back({level, message, nowMs()});
    m_cond.notify_one();
}

std::string AsyncLoggerCore::currentPath() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_path;
}

void AsyncLoggerCore::writerLoop()
{
    for (;;) {
        std::deque<Entry> batch;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cond.wait(lock, [this] {
                return !m_queue.empty() || m_stopRequested;
            });
            if (m_queue.empty() && m_stopRequested)
                break;
            batch.swap(m_queue);
        }

        for (const Entry &e : batch) {
            const char *levelName =
                e.level == Error ? "ERROR" : (e.level == Warn ? "WARN" : "INFO");
            m_file << formatTimestamp(e.tsMs) << " [" << levelName << "] "
                   << e.message << '\n';
        }
        m_file.flush();
        rotateIfNeeded();
    }
}

void AsyncLoggerCore::rotateIfNeeded()
{
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(m_path, ec);
    if (ec || size < kRotateBytes)
        return;

    m_file.close();
    const std::string backup = m_path + ".1";
    std::filesystem::remove(backup, ec);
    std::filesystem::rename(m_path, backup, ec);
    openFile(m_path);
}

} // namespace sc
