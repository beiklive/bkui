#pragma once

#include <bkui/ui/View.hpp>

#include <string>

namespace bk
{

/// 简单按钮控件，当前提供背景与文本渲染。
class Button final : public View
{
public:
    /// 使用初始文本创建按钮。
    explicit Button(std::string text = {});

    /// 设置按钮文本。
    void SetText(std::string text);
    /// 获取按钮文本。
    [[nodiscard]] const std::string& GetText() const;

    /// 设置按钮背景色。
    void SetBackgroundColor(const Color& color);
    /// 获取按钮背景色。
    [[nodiscard]] const Color& GetBackgroundColor() const;

    /// 设置按钮文字颜色。
    void SetTextColor(const Color& color);
    /// 获取按钮文字颜色。
    [[nodiscard]] const Color& GetTextColor() const;

    /// 估算按钮占用尺寸。
    Size Measure(const Size& available) const override;
    /// 生成按钮背景与文本的绘制命令。
    void Render(RenderQueue& queue) const override;

private:
    std::string text_;
    Color backgroundColor_{0.18F, 0.45F, 0.90F, 1.0F};
    Color textColor_{1.0F, 1.0F, 1.0F, 1.0F};
};

}
