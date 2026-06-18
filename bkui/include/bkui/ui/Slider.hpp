#pragma once

#include <bkui/core/Event.hpp>
#include <bkui/ui/View.hpp>

namespace bk
{

/// 常用横向滑动条控件，支持范围值、步进和拖动回调。
class Slider final : public View
{
public:
    using ValueChangedEvent = Event<void(Slider&, float)>;
    using SlideEvent = Event<void(Slider&)>;

    Slider(float minValue = 0.0F, float maxValue = 1.0F, float value = 0.0F);

    void SetRange(float minValue, float maxValue);
    [[nodiscard]] float GetMinValue() const;
    [[nodiscard]] float GetMaxValue() const;

    void SetValue(float value);
    [[nodiscard]] float GetValue() const;

    void SetStep(float step);
    [[nodiscard]] float GetStep() const;

    void SetEnabled(bool enabled);
    [[nodiscard]] bool IsEnabled() const;

    void SetTrackHeight(float height);
    [[nodiscard]] float GetTrackHeight() const;

    void SetThumbSize(float size);
    [[nodiscard]] float GetThumbSize() const;

    void SetTrackColor(const ColorRGBA& color);
    [[nodiscard]] const ColorRGBA& GetTrackColor() const;

    void SetFillColor(const ColorRGBA& color);
    [[nodiscard]] const ColorRGBA& GetFillColor() const;

    void SetThumbColor(const ColorRGBA& color);
    [[nodiscard]] const ColorRGBA& GetThumbColor() const;

    void SetDisabledColor(const ColorRGBA& color);
    [[nodiscard]] const ColorRGBA& GetDisabledColor() const;

    [[nodiscard]] ValueChangedEvent& OnValueChanged();
    [[nodiscard]] SlideEvent& OnSlidingStarted();
    [[nodiscard]] SlideEvent& OnSlidingEnded();

    Size Measure(const Size& available) const override;
    void Draw(RenderQueue& queue) const override;

protected:
    void Click(const Vector2& position) override;
    void PointerDown(const Vector2& position) override;
    void PointerMove(const Vector2& position) override;
    void PointerUp(const Vector2& position) override;
    void KeyDown(const InputState::KeyEvent& key) override;
    void FocusOut() override;

private:
    [[nodiscard]] float ClampValue(float value) const;
    [[nodiscard]] float NormalizeValue(float value) const;
    void UpdateValueFromPosition(const Vector2& position);

    float minValue_ = 0.0F;
    float maxValue_ = 1.0F;
    float value_ = 0.0F;
    float step_ = 0.0F;
    float trackHeight_ = 6.0F;
    float thumbSize_ = 20.0F;
    ColorRGBA trackColor_{0.15F, 0.19F, 0.25F, 1.0F};
    ColorRGBA fillColor_{0.27F, 0.70F, 1.0F, 1.0F};
    ColorRGBA thumbColor_{0.93F, 0.97F, 1.0F, 1.0F};
    ColorRGBA disabledColor_{0.34F, 0.37F, 0.43F, 1.0F};
    bool enabled_ = true;
    bool dragging_ = false;
    ValueChangedEvent onValueChanged_{};
    SlideEvent onSlidingStarted_{};
    SlideEvent onSlidingEnded_{};
};

}
