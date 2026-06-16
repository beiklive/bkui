#include <bkui/ui/Button.hpp>

#include <algorithm>
#include <utility>

namespace bk
{

Button::Button(std::string text)
    : text_(std::move(text))
{
}

void Button::SetText(std::string text)
{
    text_ = std::move(text);
}

const std::string& Button::GetText() const
{
    return text_;
}

void Button::SetBackgroundColor(const Color& color)
{
    backgroundColor_ = color;
}

const Color& Button::GetBackgroundColor() const
{
    return backgroundColor_;
}

void Button::SetTextColor(const Color& color)
{
    textColor_ = color;
}

const Color& Button::GetTextColor() const
{
    return textColor_;
}

Size Button::Measure(const Size& available) const
{
    const float estimatedWidth = std::min(available.width, static_cast<float>(text_.size()) * 12.0F + 36.0F);
    const float estimatedHeight = std::min(available.height, 44.0F);
    return Size{estimatedWidth, estimatedHeight};
}

void Button::Render(RenderQueue& queue) const
{
    queue.PushRect(frame_, backgroundColor_);
    queue.PushText(
        Rect{
            frame_.x + 12.0F,
            frame_.y + 10.0F,
            std::max(0.0F, frame_.width - 24.0F),
            std::max(0.0F, frame_.height - 20.0F),
        },
        text_,
        18.0F,
        textColor_);
    View::Render(queue);
}

}
