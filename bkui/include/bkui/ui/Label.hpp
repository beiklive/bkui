#pragma once

#include <bkui/ui/View.hpp>

#include <string>

namespace bk
{

/// 纯文本显示控件。
class Label final : public View
{
public:
    /// 使用初始文本创建标签。
    explicit Label(std::string text = {});

    /// 设置显示文本。
    void SetText(std::string text);
    /// 获取当前文本内容。
    [[nodiscard]] const std::string& GetText() const;

    /// 设置字体大小。
    void SetFontSize(float fontSize);
    /// 获取字体大小。
    [[nodiscard]] float GetFontSize() const;

    /// 设置文本颜色。
    void SetTextColor(const Color& color);
    /// 获取文本颜色。
    [[nodiscard]] const Color& GetTextColor() const;

    /// 估算标签占用尺寸。
    Size Measure(const Size& available) const override;
    /// 生成标签对应的文本绘制命令。
    void Render(RenderQueue& queue) const override;

private:
    std::string text_;
    float fontSize_ = 20.0F;
    Color textColor_{0.1F, 0.1F, 0.1F, 1.0F};
};

}
