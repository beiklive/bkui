#include <bkui/ui/Button.hpp>

#include <algorithm>
#include <utility>

namespace bk
{

Button::Button(std::string text)
    : Box(BoxDirection::Vertical)
    , text_(std::move(text))
{
    SetDrawBackground(true);
    SetBackgroundColor(Color{0.18F, 0.45F, 0.90F, 1.0F});
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

void Button::SetBackgroundColor(const Color& color)
{
    Box::SetBackgroundColor(color);
}

const Color& Button::GetBackgroundColor() const
{
    return Box::GetBackgroundColor();
}

void Button::SetTextColor(const Color& color)
{
    textColor_ = color;
}

const Color& Button::GetTextColor() const
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
    const float estimatedWidth = std::min(available.width, static_cast<float>(text_.size()) * (fontSize_ * 0.66F) + 36.0F);
    const float estimatedHeight = std::min(available.height, std::max(44.0F, fontSize_ * 1.8F));
    return Size{estimatedWidth, estimatedHeight};
}

void Button::DrawSelf(RenderQueue& queue) const
{
    Box::DrawSelf(queue);
    queue.PushText(
        Rect{
            frame_.x + 12.0F,
            frame_.y + 10.0F,
            std::max(0.0F, frame_.width - 24.0F),
            std::max(0.0F, frame_.height - 20.0F),
        },
        text_,
        fontSize_,
        textColor_);
}

}
