#include <bkui/ui/Label.hpp>

#include <algorithm>
#include <utility>

namespace bk
{

Label::Label(std::string text)
    : Box(BoxDirection::Vertical)
    , text_(std::move(text))
{
}

void Label::SetText(std::string text)
{
    text_ = std::move(text);
    InvalidateLayout();
}

const std::string& Label::GetText() const
{
    return text_;
}

void Label::SetFontSize(float fontSize)
{
    fontSize_ = fontSize;
}

float Label::GetFontSize() const
{
    return fontSize_;
}

void Label::SetTextColor(const Color& color)
{
    textColor_ = color;
}

const Color& Label::GetTextColor() const
{
    return textColor_;
}

Size Label::Measure(const Size& available) const
{
    const float estimatedWidth = std::min(available.width, static_cast<float>(text_.size()) * fontSize_ * 0.55F + 2.0F);
    const float estimatedHeight = std::min(available.height, fontSize_ * 1.4F);
    return Size{estimatedWidth, estimatedHeight};
}

void Label::DrawSelf(RenderQueue& queue) const
{
    Box::DrawSelf(queue);
    queue.PushText(frame_, text_, fontSize_, textColor_);
}

}
