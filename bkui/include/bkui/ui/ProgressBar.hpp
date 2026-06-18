#pragma once

#include <bkui/ui/View.hpp>

namespace bk
{

/// 常用进度条控件，支持范围值、配色与数值文字显示。
class ProgressBar final : public View
{
public:
    ProgressBar(float minValue = 0.0F, float maxValue = 1.0F, float value = 0.0F);

    void SetRange(float minValue, float maxValue);
    [[nodiscard]] float GetMinValue() const;
    [[nodiscard]] float GetMaxValue() const;

    void SetValue(float value);
    [[nodiscard]] float GetValue() const;

    void SetTrackColor(const ColorRGBA& color);
    [[nodiscard]] const ColorRGBA& GetTrackColor() const;

    void SetFillColor(const ColorRGBA& color);
    [[nodiscard]] const ColorRGBA& GetFillColor() const;

    void SetTextColor(const ColorRGBA& color);
    [[nodiscard]] const ColorRGBA& GetTextColor() const;

    void SetFontSize(float fontSize);
    [[nodiscard]] float GetFontSize() const;

    void SetShowValueText(bool show);
    [[nodiscard]] bool IsShowingValueText() const;

    Size Measure(const Size& available) const override;
    void Draw(RenderQueue& queue) const override;

private:
    [[nodiscard]] float ClampValue(float value) const;
    [[nodiscard]] float NormalizeValue(float value) const;

    float minValue_ = 0.0F;
    float maxValue_ = 1.0F;
    float value_ = 0.0F;
    ColorRGBA trackColor_{0.15F, 0.19F, 0.25F, 1.0F};
    ColorRGBA fillColor_{0.30F, 0.82F, 0.58F, 1.0F};
    ColorRGBA textColor_{0.95F, 0.97F, 1.0F, 1.0F};
    float fontSize_ = 14.0F;
    bool showValueText_ = true;
};

}
