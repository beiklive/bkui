#pragma once

#include <bkui/ui/View.hpp>

namespace bk
{

/// 滚动容器方向。
enum class ScrollAxis
{
    Horizontal,
    Vertical,
    Both,
};

/// 支持单内容视图裁剪与自动滚动的容器。
class ScrollView final : public View
{
public:
    explicit ScrollView(ScrollAxis axis = ScrollAxis::Vertical);

    /// 设置滚动方向。
    void SetScrollAxis(ScrollAxis axis);
    /// 获取滚动方向。
    [[nodiscard]] ScrollAxis GetScrollAxis() const;

    /// 设置滚动内容视图，会替换之前内容。
    void SetContent(const std::shared_ptr<View>& content);
    /// 获取滚动内容视图。
    [[nodiscard]] std::shared_ptr<View> GetContent() const;

    /// 设置滚动偏移。
    void SetScrollOffset(const Vector2& offset);
    /// 获取滚动偏移。
    [[nodiscard]] Vector2 GetScrollOffset() const;
    /// 仅设置横向偏移。
    void SetScrollX(float offset);
    /// 获取横向偏移。
    [[nodiscard]] float GetScrollX() const;
    /// 仅设置纵向偏移。
    void SetScrollY(float offset);
    /// 获取纵向偏移。
    [[nodiscard]] float GetScrollY() const;

    /// 尝试让指定视图滚动到可见区域内。
    bool EnsureViewVisible(const std::shared_ptr<View>& view, float padding = 18.0F);
    /// 获取当前内容总尺寸。
    [[nodiscard]] Size GetContentSize() const;

    Size Measure(const Size& available) const override;
    void Layout() override;
    void Paint(RenderQueue& queue) const override;

protected:
    void Update(float deltaSeconds) override;

private:
    void ClampScrollOffset();
    [[nodiscard]] bool ContainsInContentSubtree(const std::shared_ptr<View>& view) const;
    [[nodiscard]] Rect GetViewportFrame() const;

    ScrollAxis axis_ = ScrollAxis::Vertical;
    std::shared_ptr<View> content_{};
    Vector2 scrollOffset_{};
    Size contentSize_{};
};

}
