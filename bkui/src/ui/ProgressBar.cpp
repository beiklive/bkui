#include <bkui/ui/ProgressBar.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace bk
{
namespace
{
constexpr float kEpsilon = 0.0001F;

std::string BuildPercentText(float normalized)
{
    const int percent = static_cast<int>(std::round(std::clamp(normalized, 0.0F, 1.0F) * 100.0F));
    return std::to_string(percent) + "%";
}
}

ProgressBar::ProgressBar(float minValue, float maxValue, float value)
    : minValue_(std::min(minValue, maxValue))
    , maxValue_(std::max(minValue, maxValue))
    , value_(value)
{
    SetName("ProgressBar");
    SetMinHeight(18.0F);
    value_ = ClampValue(value_);
}

void ProgressBar::SetRange(float minValue, float maxValue)
{
    minValue_ = std::min(minValue, maxValue);
    maxValue_ = std::max(minValue, maxValue);
    value_ = ClampValue(value_);
}

float ProgressBar::GetMinValue() const
{
    return minValue_;
}

float ProgressBar::GetMaxValue() const
{
    return maxValue_;
}

void ProgressBar::SetValue(float value)
{
    value_ = ClampValue(value);
}

float ProgressBar::GetValue() const
{
    return value_;
}

void ProgressBar::SetTrackColor(const ColorRGBA& color)
{
    trackColor_ = color;
}

const ColorRGBA& ProgressBar::GetTrackColor() const
{
    return trackColor_;
}

void ProgressBar::SetFillColor(const ColorRGBA& color)
{
    fillColor_ = color;
}

const ColorRGBA& ProgressBar::GetFillColor() const
{
    return fillColor_;
}

void ProgressBar::SetTextColor(const ColorRGBA& color)
{
    textColor_ = color;
}

const ColorRGBA& ProgressBar::GetTextColor() const
{
    return textColor_;
}

void ProgressBar::SetFontSize(float fontSize)
{
    fontSize_ = std::max(8.0F, fontSize);
}

float ProgressBar::GetFontSize() const
{
    return fontSize_;
}

void ProgressBar::SetShowValueText(bool show)
{
    showValueText_ = show;
}

bool ProgressBar::IsShowingValueText() const
{
    return showValueText_;
}

Size ProgressBar::Measure(const Size& available) const
{
    const float height = showValueText_
        ? std::max(18.0F, fontSize_ * 1.5F)
        : 18.0F;
    return Size{
        std::min(available.width, 180.0F),
        std::min(available.height, height),
    };
}

void ProgressBar::Draw(RenderQueue& queue) const
{
    const Rect content = GetContentFrame();
    const float radius = content.height * 0.5F;
    const float normalized = NormalizeValue(value_);

    queue.PushRoundedRect(content, trackColor_, radius);

    if (normalized > 0.0F)
    {
        Rect fill = content;
        fill.width *= normalized;
        queue.PushRoundedRect(fill, fillColor_, radius);
    }

    if (showValueText_)
    {
        queue.PushText(content, BuildPercentText(normalized), fontSize_, textColor_);
    }
}

float ProgressBar::ClampValue(float value) const
{
    return std::clamp(value, minValue_, maxValue_);
}

float ProgressBar::NormalizeValue(float value) const
{
    const float range = maxValue_ - minValue_;
    if (range <= kEpsilon)
    {
        return 0.0F;
    }
    return std::clamp((value - minValue_) / range, 0.0F, 1.0F);
}

}
