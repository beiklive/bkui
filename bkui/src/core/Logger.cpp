#include <bkui/core/Logger.hpp>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

namespace bk
{

namespace
{

constexpr const char* kColorReset = "\x1b[0m";

}

Logger::Logger() = default;

Logger::~Logger()
{
    Shutdown();
}

bool Logger::Initialize(const LoggerDesc& desc)
{
    LoggerDesc resolvedDesc = desc;

    if (!desc.filePath.empty())
    {
        if (!OpenFile(desc.filePath, desc.truncateFile))
        {
            resolvedDesc.filePath.clear();
            resolvedDesc.enableConsole = true;
            std::fprintf(stderr, "Logger warning: failed to open log file '%s', falling back to console only.\n", desc.filePath.c_str());
        }
    }

    std::scoped_lock lock(mutex_);
    desc_ = std::move(resolvedDesc);
    initialized_ = true;

    if (desc_.filePath.empty())
    {
        CloseFile();
    }

    return true;
}

void Logger::Shutdown()
{
    std::scoped_lock lock(mutex_);
    CloseFile();
    initialized_ = false;
    desc_ = LoggerDesc{};
}

bool Logger::IsInitialized() const
{
    std::scoped_lock lock(mutex_);
    return initialized_;
}

void Logger::SetLevel(LogLevel level)
{
    std::scoped_lock lock(mutex_);
    desc_.level = level;
}

LogLevel Logger::GetLevel() const
{
    std::scoped_lock lock(mutex_);
    return desc_.level;
}

void Logger::EnableConsole(bool enabled)
{
    std::scoped_lock lock(mutex_);
    desc_.enableConsole = enabled;
}

bool Logger::IsConsoleEnabled() const
{
    std::scoped_lock lock(mutex_);
    return desc_.enableConsole;
}

void Logger::EnableColor(bool enabled)
{
    std::scoped_lock lock(mutex_);
    desc_.enableColor = enabled;
}

bool Logger::IsColorEnabled() const
{
    std::scoped_lock lock(mutex_);
    return desc_.enableColor;
}

bool Logger::OpenFile(std::string path, bool truncate)
{
    std::FILE* newFile = std::fopen(path.c_str(), truncate ? "wb" : "ab");
    if (newFile == nullptr)
    {
        return false;
    }

    std::scoped_lock lock(mutex_);
    if (file_ != nullptr)
    {
        std::fclose(file_);
    }

    desc_.filePath = std::move(path);
    desc_.truncateFile = truncate;
    file_ = newFile;
    return true;
}

void Logger::CloseFile()
{
    if (file_ != nullptr)
    {
        std::fclose(file_);
        file_ = nullptr;
    }
}

void Logger::Log(LogLevel level, std::string_view message)
{
    std::scoped_lock lock(mutex_);
    if (!initialized_ || level < desc_.level)
    {
        return;
    }

    const std::string prefix = BuildPrefix(level);
    if (desc_.enableConsole)
    {
        WriteConsole(level, prefix, message);
    }

    if (file_ != nullptr)
    {
        WriteFile(prefix, message);
    }

    if (desc_.flushEachMessage)
    {
        std::fflush(stdout);
        std::fflush(stderr);
        if (file_ != nullptr)
        {
            std::fflush(file_);
        }
    }
}

void Logger::Trace(std::string_view message)
{
    Log(LogLevel::Trace, message);
}

void Logger::Debug(std::string_view message)
{
    Log(LogLevel::Debug, message);
}

void Logger::Info(std::string_view message)
{
    Log(LogLevel::Info, message);
}

void Logger::Warn(std::string_view message)
{
    Log(LogLevel::Warn, message);
}

void Logger::Error(std::string_view message)
{
    Log(LogLevel::Error, message);
}

void Logger::Fatal(std::string_view message)
{
    Log(LogLevel::Fatal, message);
}

void Logger::Flush()
{
    std::scoped_lock lock(mutex_);
    std::fflush(stdout);
    std::fflush(stderr);
    if (file_ != nullptr)
    {
        std::fflush(file_);
    }
}

const char* Logger::LevelName(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Trace:
        return "TRACE";
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO ";
    case LogLevel::Warn:
        return "WARN ";
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Fatal:
        return "FATAL";
    default:
        return "UNKWN";
    }
}

const char* Logger::LevelColor(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Trace:
        return "\x1b[37m";
    case LogLevel::Debug:
        return "\x1b[36m";
    case LogLevel::Info:
        return "\x1b[32m";
    case LogLevel::Warn:
        return "\x1b[33m";
    case LogLevel::Error:
        return "\x1b[31m";
    case LogLevel::Fatal:
        return "\x1b[35m";
    default:
        return "";
    }
}

bool Logger::UseStdErr(LogLevel level)
{
    return level >= LogLevel::Error;
}

std::string Logger::BuildPrefix(LogLevel level)
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &seconds);
#else
    localtime_r(&seconds, &localTime);
#endif

    std::ostringstream stream;
    stream << '['
           << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S")
           << '.'
           << std::setw(3)
           << std::setfill('0')
           << milliseconds.count()
           << "] ["
           << LevelName(level)
           << "] ";
    return stream.str();
}

void Logger::WriteConsole(LogLevel level, std::string_view prefix, std::string_view message) const
{
    std::FILE* stream = UseStdErr(level) ? stderr : stdout;
    if (desc_.enableColor)
    {
        std::fputs(LevelColor(level), stream);
    }

    std::fwrite(prefix.data(), 1, prefix.size(), stream);
    std::fwrite(message.data(), 1, message.size(), stream);

    if (desc_.enableColor)
    {
        std::fputs(kColorReset, stream);
    }

    std::fputc('\n', stream);
}

void Logger::WriteFile(std::string_view prefix, std::string_view message)
{
    std::fwrite(prefix.data(), 1, prefix.size(), file_);
    std::fwrite(message.data(), 1, message.size(), file_);
    std::fputc('\n', file_);
}

}
