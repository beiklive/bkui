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
    SetBackgroundColor(ColorRGBA{0.18F, 0.45F, 0.90F, 1.0F});
    SetPadding(10.0F, 14.0F);
    SetMinHeight(44.0F);
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
    Box::SetBackgroundColor(color);
}

const ColorRGBA& Button::GetBackgroundColor() const
{
    return Box::GetBackgroundColor();
}

void Button::SetTextColor(const ColorRGBA& color)
{
    textColor_ = color;
}

const ColorRGBA& Button::GetTextColor() const
{
    return textColor_;
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

Size Button::Measure(const Size& available) const
{
    const float estimatedWidth = std::min(available.width, static_cast<float>(text_.size()) * (fontSize_ * 0.66F) + 8.0F);
    const float estimatedHeight = std::min(available.height, fontSize_ * 1.4F);
    return Size{estimatedWidth, estimatedHeight};
}

void Button::Draw(RenderQueue& queue) const
{
    Box::Draw(queue);
    queue.PushText(GetContentFrame(), text_, fontSize_, textColor_);
}

}
