#pragma once

#include <bkui/core/Event.hpp>
#include <bkui/ui/View.hpp>

#include <string>

namespace bk
{

/// 常用复选框控件，支持文字标签、状态切换和回调。
class CheckBox final : public View
{
public:
    using ValueChangedEvent = Event<void(CheckBox&, bool)>;

    explicit CheckBox(std::string text = {}, bool checked = false);

    void SetText(std::string text);
    [[nodiscard]] const std::string& GetText() const;

    void SetChecked(bool checked);
    [[nodiscard]] bool IsChecked() const;

    void SetEnabled(bool enabled);
    [[nodiscard]] bool IsEnabled() const;

    void SetFontSize(float fontSize);
    [[nodiscard]] float GetFontSize() const;

    void SetIndicatorSize(float size);
    [[nodiscard]] float GetIndicatorSize() const;

    void SetSpacing(float spacing);
    [[nodiscard]] float GetSpacing() const;

    void SetTextColor(const ColorRGBA& color);
    [[nodiscard]] const ColorRGBA& GetTextColor() const;

    void SetBorderColor(const ColorRGBA& color);
    [[nodiscard]] const ColorRGBA& GetBorderColor() const;

    void SetCheckedColor(const ColorRGBA& color);
    [[nodiscard]] const ColorRGBA& GetCheckedColor() const;

    void SetUncheckedColor(const ColorRGBA& color);
    [[nodiscard]] const ColorRGBA& GetUncheckedColor() const;

    void SetDisabledColor(const ColorRGBA& color);
    [[nodiscard]] const ColorRGBA& GetDisabledColor() const;

    [[nodiscard]] ValueChangedEvent& OnValueChanged();

    Size Measure(const Size& available) const override;
    void Draw(RenderQueue& queue) const override;

protected:
    void Click(const Vector2& position) override;

private:
    std::string text_;
    ColorRGBA textColor_{0.95F, 0.97F, 1.0F, 1.0F};
    ColorRGBA borderColor_{0.43F, 0.53F, 0.67F, 1.0F};
    ColorRGBA checkedColor_{0.24F, 0.64F, 1.0F, 1.0F};
    ColorRGBA uncheckedColor_{0.12F, 0.15F, 0.20F, 1.0F};
    ColorRGBA disabledColor_{0.30F, 0.33F, 0.39F, 1.0F};
    float fontSize_ = 18.0F;
    float indicatorSize_ = 22.0F;
    float spacing_ = 10.0F;
    bool checked_ = false;
    bool enabled_ = true;
    ValueChangedEvent onValueChanged_{};
};

}
