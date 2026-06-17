#pragma once

#include <bkui/ui/View.hpp>

namespace bk
{

/// Box 容器的排列方向。
enum class BoxDirection
{
    Horizontal,
    Vertical,
};

/// 简单线性布局容器。
class Box : public View
{
public:
    /// 创建一个指定方向的 Box 容器。
    explicit Box(BoxDirection direction);

    /// 设置子项之间的间距。
    void SetSpacing(float spacing);
    /// 获取子项之间的间距。
    [[nodiscard]] float GetSpacing() const;

    /// 设置背景颜色。
    void SetBackgroundColor(const ColorRGBA& color);
    /// 获取背景颜色。
    [[nodiscard]] const ColorRGBA& GetBackgroundColor() const;

    /// 控制是否绘制背景。
    void SetDrawBackground(bool enabled);
    /// 查询当前是否绘制背景。
    [[nodiscard]] bool IsBackgroundEnabled() const;

    /// 计算容器所需尺寸。
    Size Measure(const Size& available) const override;
    /// 按方向和间距布置全部子项。
    void Layout() override;

protected:
    void Draw(RenderQueue& queue) const override;

    BoxDirection direction_;
    float spacing_ = 0.0F;
};

class HBox final : public Box
{
public:
    /// 创建水平排列容器。
    HBox();
};

class VBox final : public Box
{
public:
    /// 创建垂直排列容器。
    VBox();
};

}
