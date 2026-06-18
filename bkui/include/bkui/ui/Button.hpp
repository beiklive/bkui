#pragma once

#include <bkui/core/Event.hpp>
#include <bkui/ui/Box.hpp>

#include <string>

namespace bk
{

/// 简单按钮控件，当前提供背景与文本渲染。
class Button : public Box
{
public:
    using ClickEvent = Event<void(Button&)>;
    using StateEvent = Event<void(Button&)>;

    /// 使用初始文本创建按钮。
    explicit Button(std::string text = {});

    /// 设置按钮文本。
    void SetText(std::string text);
    /// 获取按钮文本。
    [[nodiscard]] const std::string& GetText() const;

    /// 设置按钮背景色。
    void SetBackgroundColor(const ColorRGBA& color);
    /// 获取按钮背景色。
    [[nodiscard]] const ColorRGBA& GetBackgroundColor() const;
    /// 设置按钮按下时背景色。
    void SetPressedBackgroundColor(const ColorRGBA& color);
    /// 获取按钮按下时背景色。
    [[nodiscard]] const ColorRGBA& GetPressedBackgroundColor() const;
    /// 设置按钮禁用时背景色。
    void SetDisabledBackgroundColor(const ColorRGBA& color);
    /// 获取按钮禁用时背景色。
    [[nodiscard]] const ColorRGBA& GetDisabledBackgroundColor() const;

    /// 设置按钮文字颜色。
    void SetTextColor(const ColorRGBA& color);
    /// 获取按钮文字颜色。
    [[nodiscard]] const ColorRGBA& GetTextColor() const;
    /// 设置按钮禁用时文字颜色。
    void SetDisabledTextColor(const ColorRGBA& color);
    /// 获取按钮禁用时文字颜色。
    [[nodiscard]] const ColorRGBA& GetDisabledTextColor() const;

    /// 设置按钮字体大小。
    void SetFontSize(float fontSize);
    /// 获取按钮字体大小。
    [[nodiscard]] float GetFontSize() const;

    /// 设置按钮是否可交互。
    void SetEnabled(bool enabled);
    /// 查询按钮是否可交互。
    [[nodiscard]] bool IsEnabled() const;

    /// 获取点击事件。
    [[nodiscard]] ClickEvent& OnClick();
    /// 获取按下事件。
    [[nodiscard]] StateEvent& OnPressed();
    /// 获取释放事件。
    [[nodiscard]] StateEvent& OnReleased();

    /// 估算按钮占用尺寸。
    Size Measure(const Size& available) const override;
    /// 生成按钮背景与文本的绘制命令。
    void Draw(RenderQueue& queue) const override;

protected:
    void Click(const Vector2& position) override;
    void PointerDown(const Vector2& position) override;
    void PointerUp(const Vector2& position) override;
    void FocusOut() override;

private:
    void ApplyVisualState();

    std::string text_;
    ColorRGBA backgroundColor_{0.18F, 0.45F, 0.90F, 1.0F};
    ColorRGBA pressedBackgroundColor_{0.12F, 0.34F, 0.72F, 1.0F};
    ColorRGBA disabledBackgroundColor_{0.23F, 0.27F, 0.32F, 1.0F};
    ColorRGBA textColor_{1.0F, 1.0F, 1.0F, 1.0F};
    ColorRGBA disabledTextColor_{0.66F, 0.69F, 0.74F, 1.0F};
    ColorRGBA currentTextColor_{1.0F, 1.0F, 1.0F, 1.0F};
    float fontSize_ = 18.0F;
    bool enabled_ = true;
    bool pressed_ = false;
    ClickEvent onClick_{};
    StateEvent onPressed_{};
    StateEvent onReleased_{};
};

}
