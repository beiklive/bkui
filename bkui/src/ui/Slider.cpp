#include <bkui/ui/Slider.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace bk
{
namespace
{
constexpr float kEpsilon = 0.0001F;
}

Slider::Slider(float minValue, float maxValue, float value)
    : minValue_(std::min(minValue, maxValue))
    , maxValue_(std::max(minValue, maxValue))
    , value_(value)
{
    SetName("Slider");
    SetFocusable(true);
    SetMinHeight(32.0F);
    value_ = ClampValue(value_);
}

void Slider::SetRange(float minValue, float maxValue)
{
    minValue_ = std::min(minValue, maxValue);
    maxValue_ = std::max(minValue, maxValue);
    SetValue(value_);
}

float Slider::GetMinValue() const
{
    return minValue_;
}

float Slider::GetMaxValue() const
{
    return maxValue_;
}

void Slider::SetValue(float value)
{
    const float next = ClampValue(value);
    if (std::fabs(next - value_) <= kEpsilon)
    {
        return;
    }

    value_ = next;
    onValueChanged_.Emit(*this, value_);
}

float Slider::GetValue() const
{
    return value_;
}

void Slider::SetStep(float step)
{
    step_ = std::max(0.0F, step);
    SetValue(value_);
}

float Slider::GetStep() const
{
    return step_;
}

void Slider::SetEnabled(bool enabled)
{
    enabled_ = enabled;
    if (!enabled_ && dragging_)
    {
        dragging_ = false;
        onSlidingEnded_.Emit(*this);
    }
    SetFocusable(enabled_);
}

bool Slider::IsEnabled() const
{
    return enabled_;
}

void Slider::SetTrackHeight(float height)
{
    trackHeight_ = std::max(2.0F, height);
    InvalidateLayout();
}

float Slider::GetTrackHeight() const
{
    return trackHeight_;
}

void Slider::SetThumbSize(float size)
{
    thumbSize_ = std::max(8.0F, size);
    InvalidateLayout();
}

float Slider::GetThumbSize() const
{
    return thumbSize_;
}

void Slider::SetTrackColor(const ColorRGBA& color)
{
    trackColor_ = color;
}

const ColorRGBA& Slider::GetTrackColor() const
{
    return trackColor_;
}

void Slider::SetFillColor(const ColorRGBA& color)
{
    fillColor_ = color;
}

const ColorRGBA& Slider::GetFillColor() const
{
    return fillColor_;
}

void Slider::SetThumbColor(const ColorRGBA& color)
{
    thumbColor_ = color;
}

const ColorRGBA& Slider::GetThumbColor() const
{
    return thumbColor_;
}

void Slider::SetDisabledColor(const ColorRGBA& color)
{
    disabledColor_ = color;
}

const ColorRGBA& Slider::GetDisabledColor() const
{
    return disabledColor_;
}

Slider::ValueChangedEvent& Slider::OnValueChanged()
{
    return onValueChanged_;
}

Slider::SlideEvent& Slider::OnSlidingStarted()
{
    return onSlidingStarted_;
}

Slider::SlideEvent& Slider::OnSlidingEnded()
{
    return onSlidingEnded_;
}

Size Slider::Measure(const Size& available) const
{
    return Size{
        std::min(available.width, std::max(160.0F, thumbSize_ * 4.0F)),
        std::min(available.height, std::max(thumbSize_, trackHeight_) + 8.0F),
    };
}

void Slider::Draw(RenderQueue& queue) const
{
    const Rect content = GetContentFrame();
    const float thumbRadius = thumbSize_ * 0.5F;
    const float trackTop = content.y + std::max(0.0F, (content.height - trackHeight_) * 0.5F);
    const Rect track{
        content.x + thumbRadius,
        trackTop,
        std::max(0.0F, content.width - thumbSize_),
        trackHeight_,
    };
    const float normalized = NormalizeValue(value_);
    const float fillWidth = track.width * normalized;
    const Rect fill{
        track.x,
        track.y,
        fillWidth,
        track.height,
    };
    const float thumbCenterX = track.x + fillWidth;
    const Rect thumb{
        thumbCenterX - thumbRadius,
        content.y + std::max(0.0F, (content.height - thumbSize_) * 0.5F),
        thumbSize_,
        thumbSize_,
    };

    const ColorRGBA dim = disabledColor_;
    queue.PushRoundedRect(track, enabled_ ? trackColor_ : dim, track.height * 0.5F);
    if (fill.width > 0.0F)
    {
        queue.PushRoundedRect(fill, enabled_ ? fillColor_ : dim, fill.height * 0.5F);
    }
    queue.PushRoundedRect(thumb, enabled_ ? thumbColor_ : dim, thumbRadius);
}

void Slider::Click(const Vector2& position)
{
    if (!enabled_)
    {
        return;
    }

    UpdateValueFromPosition(position);
}

void Slider::PointerDown(const Vector2& position)
{
    if (!enabled_)
    {
        return;
    }

    dragging_ = true;
    onSlidingStarted_.Emit(*this);
    UpdateValueFromPosition(position);
}

void Slider::PointerMove(const Vector2& position)
{
    if (!enabled_ || !dragging_)
    {
        return;
    }

    UpdateValueFromPosition(position);
}

void Slider::PointerUp(const Vector2&)
{
    if (!dragging_)
    {
        return;
    }

    dragging_ = false;
    onSlidingEnded_.Emit(*this);
}

void Slider::KeyDown(const InputState::KeyEvent& key)
{
    if (!enabled_)
    {
        return;
    }

    const float range = std::max(kEpsilon, maxValue_ - minValue_);
    const float delta = step_ > 0.0F ? step_ : range * 0.05F;
    if (std::strcmp(key.name, "Left") == 0 || std::strcmp(key.name, "Down") == 0)
    {
        SetValue(value_ - delta);
    }
    else if (std::strcmp(key.name, "Right") == 0 || std::strcmp(key.name, "Up") == 0)
    {
        SetValue(value_ + delta);
    }
}

void Slider::FocusOut()
{
    if (!dragging_)
    {
        return;
    }

    dragging_ = false;
    onSlidingEnded_.Emit(*this);
}

float Slider::ClampValue(float value) const
{
    float next = std::clamp(value, minValue_, maxValue_);
    if (step_ > 0.0F)
    {
        const float steps = std::round((next - minValue_) / step_);
        next = minValue_ + steps * step_;
        next = std::clamp(next, minValue_, maxValue_);
    }
    return next;
}

float Slider::NormalizeValue(float value) const
{
    const float range = maxValue_ - minValue_;
    if (range <= kEpsilon)
    {
        return 0.0F;
    }
    return std::clamp((value - minValue_) / range, 0.0F, 1.0F);
}

void Slider::UpdateValueFromPosition(const Vector2& position)
{
    const Rect content = GetContentFrame();
    const float thumbRadius = thumbSize_ * 0.5F;
    const float startX = content.x + thumbRadius;
    const float endX = content.x + std::max(thumbRadius, content.width - thumbRadius);
    const float width = std::max(kEpsilon, endX - startX);
    const float t = std::clamp((position.x - startX) / width, 0.0F, 1.0F);
    SetValue(minValue_ + (maxValue_ - minValue_) * t);
}

}
