#pragma once

#include <bkui/core/Singleton.hpp>

#include <cstdio>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace bk
{

/// 日志等级，数值越大表示等级越高。
enum class LogLevel
{
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
};

/// 日志器初始化描述。
struct LoggerDesc
{
    LogLevel level = LogLevel::Info;
    bool enableConsole = true;
    bool enableColor = true;
    bool flushEachMessage = false;
    bool truncateFile = false;
    std::string filePath;
};

struct LogEntry
{
    LogLevel level = LogLevel::Info;
    std::string prefix;
    std::string message;
};

/// 轻量日志模块，提供彩色控制台输出与文件写入。
class Logger : public Singleton<Logger>
{
public:
    using ListenerId = std::size_t;
    using LogListener = std::function<void(const LogEntry&)>;

    Logger();
    ~Logger();

    /// 初始化日志器，可重复调用以覆盖当前配置。
    bool Initialize(const LoggerDesc& desc = {});

    /// 关闭日志器并释放文件句柄。
    void Shutdown();

    /// 查询日志器是否已经初始化。
    [[nodiscard]] bool IsInitialized() const;

    /// 设置最小输出等级。
    void SetLevel(LogLevel level);

    /// 获取最小输出等级。
    [[nodiscard]] LogLevel GetLevel() const;

    /// 启用或关闭控制台输出。
    void EnableConsole(bool enabled);

    /// 查询控制台输出是否启用。
    [[nodiscard]] bool IsConsoleEnabled() const;

    /// 启用或关闭彩色输出。
    void EnableColor(bool enabled);

    /// 查询彩色输出是否启用。
    [[nodiscard]] bool IsColorEnabled() const;

    /// 打开日志文件，失败时保留当前文件状态。
    bool OpenFile(std::string path, bool truncate = false);

    /// 关闭当前日志文件。
    void CloseFile();

    /// 主日志接口。
    void Log(LogLevel level, std::string_view message);

    /// 便捷输出接口。
    void trace(std::string_view message);
    void debug(std::string_view message);
    void info(std::string_view message);
    void warn(std::string_view message);
    void error(std::string_view message);
    void fatal(std::string_view message);

    /// 立刻刷新控制台和文件缓冲。
    void Flush();

    /// 设置内存日志缓存上限。
    void SetHistoryLimit(std::size_t limit);
    /// 获取内存日志缓存上限。
    [[nodiscard]] std::size_t GetHistoryLimit() const;
    /// 获取当前缓存的日志快照。
    [[nodiscard]] std::vector<LogEntry> GetHistorySnapshot() const;
    /// 注册日志监听器，返回可用于注销的 id。
    ListenerId AddListener(LogListener listener);
    /// 注销已注册日志监听器。
    void RemoveListener(ListenerId listenerId);

private:
    [[nodiscard]] static const char* LevelName(LogLevel level);
    [[nodiscard]] static const char* LevelColor(LogLevel level);
    [[nodiscard]] static bool UseStdErr(LogLevel level);
    [[nodiscard]] static std::string BuildPrefix(LogLevel level);

    void WriteConsole(LogLevel level, std::string_view prefix, std::string_view message) const;
    void WriteFile(std::string_view prefix, std::string_view message);

    mutable std::mutex mutex_{};
    LoggerDesc desc_{};
    std::deque<LogEntry> history_{};
    std::vector<std::pair<ListenerId, LogListener>> listeners_{};
    std::size_t historyLimit_ = 256;
    ListenerId nextListenerId_ = 1;
    std::FILE* file_ = nullptr;
    bool initialized_ = false;
};



}

#define bklog bk::Logger::instance()
