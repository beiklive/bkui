#include <bkui/ui/Button.hpp>

#include <algorithm>
#include <utility>

namespace bk
{

Button::Button(std::string text)
    : Box(BoxDirection::Vertical)
    , text_(std::move(text))
{
    SetFocusable(true);
    SetDrawBackground(true);
    SetPadding(10.0F, 14.0F);
    SetMinHeight(44.0F);
    ApplyVisualState();
}

void Button::SetText(std::string text)
{
    text_ = std::move(text);
    InvalidateLayout();
}

const std::string& Button::GetText() const
{
    return text_;
}

void Button::SetBackgroundColor(const ColorRGBA& color)
{
    backgroundColor_ = color;
    ApplyVisualState();
}

const ColorRGBA& Button::GetBackgroundColor() const
{
    return backgroundColor_;
}

void Button::SetPressedBackgroundColor(const ColorRGBA& color)
{
    pressedBackgroundColor_ = color;
    ApplyVisualState();
}

const ColorRGBA& Button::GetPressedBackgroundColor() const
{
    return pressedBackgroundColor_;
}

void Button::SetDisabledBackgroundColor(const ColorRGBA& color)
{
    disabledBackgroundColor_ = color;
    ApplyVisualState();
}

const ColorRGBA& Button::GetDisabledBackgroundColor() const
{
    return disabledBackgroundColor_;
}

void Button::SetTextColor(const ColorRGBA& color)
{
    textColor_ = color;
    ApplyVisualState();
}

const ColorRGBA& Button::GetTextColor() const
{
    return textColor_;
}

void Button::SetDisabledTextColor(const ColorRGBA& color)
{
    disabledTextColor_ = color;
    ApplyVisualState();
}

const ColorRGBA& Button::GetDisabledTextColor() const
{
    return disabledTextColor_;
}

void Button::SetFontSize(float fontSize)
{
    fontSize_ = fontSize;
    InvalidateLayout();
}

float Button::GetFontSize() const
{
    return fontSize_;
}

void Button::SetEnabled(bool enabled)
{
    if (enabled_ == enabled)
    {
        return;
    }

    enabled_ = enabled;
    pressed_ = false;
    SetFocusable(enabled_);
    ApplyVisualState();
}

bool Button::IsEnabled() const
{
    return enabled_;
}

Button::ClickEvent& Button::OnClick()
{
    return onClick_;
}

Button::StateEvent& Button::OnPressed()
{
    return onPressed_;
}

Button::StateEvent& Button::OnReleased()
{
    return onReleased_;
}

Size Button::Measure(const Size& available) const
{
    const float estimatedWidth = std::min(available.width, static_cast<float>(text_.size()) * (fontSize_ * 0.66F) + 8.0F);
    const float estimatedHeight = std::min(available.height, fontSize_ * 1.4F);
    return Size{estimatedWidth, estimatedHeight};
}

void Button::Draw(RenderQueue& queue) const
{
    Box::Draw(queue);
    queue.PushText(GetContentFrame(), text_, fontSize_, currentTextColor_);
}

void Button::Click(const Vector2&)
{
    if (!enabled_)
    {
        return;
    }

    onClick_.Emit(*this);
}

void Button::PointerDown(const Vector2&)
{
    if (!enabled_)
    {
        return;
    }

    pressed_ = true;
    ApplyVisualState();
    onPressed_.Emit(*this);
}

void Button::PointerUp(const Vector2&)
{
    if (!pressed_)
    {
        return;
    }

    pressed_ = false;
    ApplyVisualState();
    onReleased_.Emit(*this);
}

void Button::FocusOut()
{
    if (!pressed_)
    {
        return;
    }

    pressed_ = false;
    ApplyVisualState();
}

void Button::ApplyVisualState()
{
    if (!enabled_)
    {
        Box::SetBackgroundColor(disabledBackgroundColor_);
        currentTextColor_ = disabledTextColor_;
        return;
    }

    Box::SetBackgroundColor(pressed_ ? pressedBackgroundColor_ : backgroundColor_);
    currentTextColor_ = textColor_;
}

}
