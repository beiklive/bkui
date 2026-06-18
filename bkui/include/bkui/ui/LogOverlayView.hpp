#pragma once

#include <bkui/core/Logger.hpp>
#include <bkui/ui/View.hpp>

#include <deque>
#include <string>

namespace bk
{

/// 覆盖式日志调试视图，显示最近日志并在溢出后向上滚动。
class LogOverlayView final : public View
{
public:
    LogOverlayView();
    ~LogOverlayView() override;

    void SetFontSize(float fontSize);
    [[nodiscard]] float GetFontSize() const;

    void SetLineSpacing(float spacing);
    [[nodiscard]] float GetLineSpacing() const;

    void SetMaxEntries(std::size_t maxEntries);
    [[nodiscard]] std::size_t GetMaxEntries() const;

    void SetAutoScrollEnabled(bool enabled);
    [[nodiscard]] bool IsAutoScrollEnabled() const;

    void SetBackgroundOpacity(float alpha);
    [[nodiscard]] float GetBackgroundOpacity() const;

    void SetLevelColors(
        const ColorRGBA& traceColor,
        const ColorRGBA& debugColor,
        const ColorRGBA& infoColor,
        const ColorRGBA& warnColor,
        const ColorRGBA& errorColor,
        const ColorRGBA& fatalColor);

    Size Measure(const Size& available) const override;
    void Draw(RenderQueue& queue) const override;

private:
    struct LineEntry
    {
        LogLevel level = LogLevel::Info;
        std::string text;
    };

    void ReloadHistory();
    void AppendEntry(const LogEntry& entry);
    void TrimToLimit();
    [[nodiscard]] ColorRGBA ColorForLevel(LogLevel level) const;
    [[nodiscard]] float LineHeight() const;

    std::deque<LineEntry> entries_{};
    Logger::ListenerId listenerId_ = 0;
    float fontSize_ = 18.0F;
    float lineSpacing_ = 6.0F;
    std::size_t maxEntries_ = 200;
    bool autoScrollEnabled_ = true;
    float backgroundOpacity_ = 0.74F;
    ColorRGBA traceColor_{0.82F, 0.82F, 0.82F, 1.0F};
    ColorRGBA debugColor_{0.42F, 0.84F, 1.0F, 1.0F};
    ColorRGBA infoColor_{0.58F, 0.92F, 0.58F, 1.0F};
    ColorRGBA warnColor_{1.0F, 0.82F, 0.35F, 1.0F};
    ColorRGBA errorColor_{1.0F, 0.48F, 0.48F, 1.0F};
    ColorRGBA fatalColor_{1.0F, 0.46F, 0.92F, 1.0F};
};

}
