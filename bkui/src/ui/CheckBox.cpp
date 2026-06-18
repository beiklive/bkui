#include <bkui/ui/CheckBox.hpp>

#include <algorithm>
#include <utility>

namespace bk
{

CheckBox::CheckBox(std::string text, bool checked)
    : text_(std::move(text))
    , checked_(checked)
{
    SetName("CheckBox");
    SetFocusable(true);
    SetMinHeight(30.0F);
}

void CheckBox::SetText(std::string text)
{
    text_ = std::move(text);
    InvalidateLayout();
}

const std::string& CheckBox::GetText() const
{
    return text_;
}

void CheckBox::SetChecked(bool checked)
{
    if (checked_ == checked)
    {
        return;
    }

    checked_ = checked;
    onValueChanged_.Emit(*this, checked_);
}

bool CheckBox::IsChecked() const
{
    return checked_;
}

void CheckBox::SetEnabled(bool enabled)
{
    enabled_ = enabled;
    SetFocusable(enabled_);
}

bool CheckBox::IsEnabled() const
{
    return enabled_;
}

void CheckBox::SetFontSize(float fontSize)
{
    fontSize_ = std::max(8.0F, fontSize);
    InvalidateLayout();
}

float CheckBox::GetFontSize() const
{
    return fontSize_;
}

void CheckBox::SetIndicatorSize(float size)
{
    indicatorSize_ = std::max(10.0F, size);
    InvalidateLayout();
}

float CheckBox::GetIndicatorSize() const
{
    return indicatorSize_;
}

void CheckBox::SetSpacing(float spacing)
{
    spacing_ = std::max(0.0F, spacing);
    InvalidateLayout();
}

float CheckBox::GetSpacing() const
{
    return spacing_;
}

void CheckBox::SetTextColor(const ColorRGBA& color)
{
    textColor_ = color;
}

const ColorRGBA& CheckBox::GetTextColor() const
{
    return textColor_;
}

void CheckBox::SetBorderColor(const ColorRGBA& color)
{
    borderColor_ = color;
}

const ColorRGBA& CheckBox::GetBorderColor() const
{
    return borderColor_;
}

void CheckBox::SetCheckedColor(const ColorRGBA& color)
{
    checkedColor_ = color;
}

const ColorRGBA& CheckBox::GetCheckedColor() const
{
    return checkedColor_;
}

void CheckBox::SetUncheckedColor(const ColorRGBA& color)
{
    uncheckedColor_ = color;
}

const ColorRGBA& CheckBox::GetUncheckedColor() const
{
    return uncheckedColor_;
}

void CheckBox::SetDisabledColor(const ColorRGBA& color)
{
    disabledColor_ = color;
}

const ColorRGBA& CheckBox::GetDisabledColor() const
{
    return disabledColor_;
}

CheckBox::ValueChangedEvent& CheckBox::OnValueChanged()
{
    return onValueChanged_;
}

Size CheckBox::Measure(const Size& available) const
{
    const float estimatedTextWidth = static_cast<float>(text_.size()) * fontSize_ * 0.56F;
    const float height = std::max(indicatorSize_, fontSize_ * 1.4F);
    return Size{
        std::min(available.width, indicatorSize_ + spacing_ + estimatedTextWidth),
        std::min(available.height, height),
    };
}

void CheckBox::Draw(RenderQueue& queue) const
{
    const Rect content = GetContentFrame();
    const float boxY = content.y + std::max(0.0F, (content.height - indicatorSize_) * 0.5F);
    const Rect indicator{
        content.x,
        boxY,
        indicatorSize_,
        indicatorSize_,
    };

    const ColorRGBA base = enabled_
        ? (checked_ ? checkedColor_ : uncheckedColor_)
        : disabledColor_;
    const ColorRGBA outline = enabled_ ? borderColor_ : disabledColor_;
    const float radius = std::min(indicator.width, indicator.height) * 0.22F;

    queue.PushRoundedRect(indicator, outline, radius);
    const Rect inner{
        indicator.x + 2.0F,
        indicator.y + 2.0F,
        std::max(0.0F, indicator.width - 4.0F),
        std::max(0.0F, indicator.height - 4.0F),
    };
    queue.PushRoundedRect(inner, base, std::max(0.0F, radius - 1.0F));

    if (checked_)
    {
        const ColorRGBA markColor{0.95F, 0.98F, 1.0F, enabled_ ? 1.0F : 0.75F};
        queue.PushLine(
            Vector2{indicator.x + indicator.width * 0.22F, indicator.y + indicator.height * 0.52F},
            Vector2{indicator.x + indicator.width * 0.45F, indicator.y + indicator.height * 0.76F},
            markColor,
            2.4F);
        queue.PushLine(
            Vector2{indicator.x + indicator.width * 0.45F, indicator.y + indicator.height * 0.76F},
            Vector2{indicator.x + indicator.width * 0.80F, indicator.y + indicator.height * 0.26F},
            markColor,
            2.4F);
    }

    const Rect textRect{
        indicator.x + indicator.width + spacing_,
        content.y,
        std::max(0.0F, content.width - indicator.width - spacing_),
        content.height,
    };
    queue.PushText(textRect, text_, fontSize_, enabled_ ? textColor_ : disabledColor_);
}

void CheckBox::Click(const Vector2&)
{
    if (!enabled_)
    {
        return;
    }

    SetChecked(!checked_);
}

}
