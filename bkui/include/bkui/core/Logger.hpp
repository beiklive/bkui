#pragma once

#include <bkui/core/Singleton.hpp>

#include <cstdio>
#include <mutex>
#include <string>
#include <string_view>

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

/// 轻量日志模块，提供彩色控制台输出与文件写入。
class Logger : public Singleton<Logger>
{
public:
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

private:
    [[nodiscard]] static const char* LevelName(LogLevel level);
    [[nodiscard]] static const char* LevelColor(LogLevel level);
    [[nodiscard]] static bool UseStdErr(LogLevel level);
    [[nodiscard]] static std::string BuildPrefix(LogLevel level);

    void WriteConsole(LogLevel level, std::string_view prefix, std::string_view message) const;
    void WriteFile(std::string_view prefix, std::string_view message);

    mutable std::mutex mutex_{};
    LoggerDesc desc_{};
    std::FILE* file_ = nullptr;
    bool initialized_ = false;
};



}

#define bklog bk::Logger::instance()

