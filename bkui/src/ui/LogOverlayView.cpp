#include <bkui/ui/LogOverlayView.hpp>

#include <algorithm>
#include <utility>

namespace bk
{

LogOverlayView::LogOverlayView()
{
    SetName("LogOverlayView");
    SetDrawBackground(true);
    SetBackgroundColor(ColorRGBA{0.0F, 0.0F, 0.0F, backgroundOpacity_});
    SetCornerRadius(6.0F);
    SetPadding(12.0F);
    SetZIndex(10000);
    ReloadHistory();
    listenerId_ = bklog.AddListener([this](const LogEntry& entry) {
        AppendEntry(entry);
    });
}

LogOverlayView::~LogOverlayView()
{
    if (listenerId_ != 0)
    {
        bklog.RemoveListener(listenerId_);
    }
}

void LogOverlayView::SetFontSize(float fontSize)
{
    fontSize_ = std::max(8.0F, fontSize);
    InvalidateLayout();
}

float LogOverlayView::GetFontSize() const
{
    return fontSize_;
}

void LogOverlayView::SetLineSpacing(float spacing)
{
    lineSpacing_ = std::max(0.0F, spacing);
    InvalidateLayout();
}

float LogOverlayView::GetLineSpacing() const
{
    return lineSpacing_;
}

void LogOverlayView::SetMaxEntries(std::size_t maxEntries)
{
    maxEntries_ = std::max<std::size_t>(1, maxEntries);
    TrimToLimit();
    InvalidateLayout();
}

std::size_t LogOverlayView::GetMaxEntries() const
{
    return maxEntries_;
}

void LogOverlayView::SetAutoScrollEnabled(bool enabled)
{
    autoScrollEnabled_ = enabled;
}

bool LogOverlayView::IsAutoScrollEnabled() const
{
    return autoScrollEnabled_;
}

void LogOverlayView::SetBackgroundOpacity(float alpha)
{
    backgroundOpacity_ = std::clamp(alpha, 0.0F, 1.0F);
    SetBackgroundColor(ColorRGBA{0.0F, 0.0F, 0.0F, backgroundOpacity_});
}

float LogOverlayView::GetBackgroundOpacity() const
{
    return backgroundOpacity_;
}

void LogOverlayView::SetLevelColors(
    const ColorRGBA& traceColor,
    const ColorRGBA& debugColor,
    const ColorRGBA& infoColor,
    const ColorRGBA& warnColor,
    const ColorRGBA& errorColor,
    const ColorRGBA& fatalColor)
{
    traceColor_ = traceColor;
    debugColor_ = debugColor;
    infoColor_ = infoColor;
    warnColor_ = warnColor;
    errorColor_ = errorColor;
    fatalColor_ = fatalColor;
}

Size LogOverlayView::Measure(const Size& available) const
{
    return Size{
        std::min(available.width, 520.0F),
        std::min(available.height, 320.0F),
    };
}

void LogOverlayView::Draw(RenderQueue& queue) const
{
    const Rect content = GetContentFrame();
    if (content.width <= 0.0F || content.height <= 0.0F || entries_.empty())
    {
        return;
    }

    queue.PushClipRect(content);

    const float lineHeight = LineHeight();
    const float totalHeight = static_cast<float>(entries_.size()) * lineHeight;
    float startY = content.y;
    if (autoScrollEnabled_ && totalHeight > content.height)
    {
        startY = content.y - (totalHeight - content.height);
    }

    for (std::size_t index = 0; index < entries_.size(); ++index)
    {
        const float y = startY + static_cast<float>(index) * lineHeight;
        if (y + lineHeight < content.y || y > content.y + content.height)
        {
            continue;
        }

        queue.PushText(
            Rect{content.x, y, content.width, lineHeight},
            entries_[index].text,
            fontSize_,
            ColorForLevel(entries_[index].level));
    }

    queue.PopClipRect();
}

void LogOverlayView::ReloadHistory()
{
    entries_.clear();
    const std::vector<LogEntry> history = bklog.GetHistorySnapshot();
    for (const LogEntry& entry : history)
    {
        AppendEntry(entry);
    }
}

void LogOverlayView::AppendEntry(const LogEntry& entry)
{
    entries_.push_back(LineEntry{
        entry.level,
        entry.prefix + entry.message,
    });
    TrimToLimit();
    InvalidateLayout();
}

void LogOverlayView::TrimToLimit()
{
    while (entries_.size() > maxEntries_)
    {
        entries_.pop_front();
    }
}

ColorRGBA LogOverlayView::ColorForLevel(LogLevel level) const
{
    switch (level)
    {
    case LogLevel::Trace:
        return traceColor_;
    case LogLevel::Debug:
        return debugColor_;
    case LogLevel::Info:
        return infoColor_;
    case LogLevel::Warn:
        return warnColor_;
    case LogLevel::Error:
        return errorColor_;
    case LogLevel::Fatal:
        return fatalColor_;
    default:
        return infoColor_;
    }
}

float LogOverlayView::LineHeight() const
{
    return fontSize_ + lineSpacing_;
}

}
