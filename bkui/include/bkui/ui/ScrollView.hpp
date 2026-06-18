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

    /// 设置是否显示滚动条。
    void SetScrollBarsVisible(bool visible);
    /// 查询滚动条是否显示。
    [[nodiscard]] bool AreScrollBarsVisible() const;

    /// 设置滚动条厚度。
    void SetScrollBarThickness(float thickness);
    /// 获取滚动条厚度。
    [[nodiscard]] float GetScrollBarThickness() const;

    /// 设置滚动条轨道颜色。
    void SetScrollBarTrackColor(const ColorRGBA& color);
    /// 获取滚动条轨道颜色。
    [[nodiscard]] const ColorRGBA& GetScrollBarTrackColor() const;

    /// 设置滚动条滑块颜色。
    void SetScrollBarThumbColor(const ColorRGBA& color);
    /// 获取滚动条滑块颜色。
    [[nodiscard]] const ColorRGBA& GetScrollBarThumbColor() const;

    /// 设置惯性滚动是否启用。
    void SetInertiaEnabled(bool enabled);
    /// 查询惯性滚动是否启用。
    [[nodiscard]] bool IsInertiaEnabled() const;

    /// 设置惯性衰减速度，数值越大停止越快。
    void SetInertiaDecay(float decayPerSecond);
    /// 获取惯性衰减速度。
    [[nodiscard]] float GetInertiaDecay() const;

    /// 应用鼠标滚轮滚动。
    bool ScrollByWheel(const Vector2& wheelDelta, float lineStep = 48.0F);
    /// 开始一次手势滚动。
    void BeginGestureScroll(std::size_t contactCount);
    /// 应用一段手势位移，返回实际写入的 scrollOffset 增量。
    [[nodiscard]] Vector2 ApplyGestureDelta(const Vector2& gestureDelta);
    /// 结束手势并注入惯性速度。
    void EndGestureScroll(const Vector2& offsetVelocityPerSecond);
    /// 取消当前手势滚动。
    void CancelGestureScroll();
    /// 停止惯性滚动。
    void StopInertia();

    /// 命中并开始拖动滚动条。
    bool BeginScrollBarDrag(const Vector2& position);
    /// 更新滚动条拖动。
    bool UpdateScrollBarDrag(const Vector2& position);
    /// 结束滚动条拖动。
    void EndScrollBarDrag();
    /// 查询当前是否在拖动滚动条。
    [[nodiscard]] bool IsScrollBarDragging() const;

    /// 查询当前是否处于用户滚动或惯性滚动中。
    [[nodiscard]] bool IsScrolling() const;

    Size Measure(const Size& available) const override;
    void Layout() override;
    void Paint(RenderQueue& queue) const override;

protected:
    void Update(float deltaSeconds) override;
    void Draw(RenderQueue& queue) const override;

private:
    enum class ScrollBarPart
    {
        None,
        HorizontalThumb,
        VerticalThumb,
    };

    [[nodiscard]] Vector2 ApplyScrollOffsetDelta(const Vector2& offsetDelta);
    void ClampScrollOffset();
    void ApplyInertia(float deltaSeconds);
    [[nodiscard]] bool ContainsInContentSubtree(const std::shared_ptr<View>& view) const;
    [[nodiscard]] Rect GetViewportFrame() const;
    [[nodiscard]] bool HasHorizontalScrollBar() const;
    [[nodiscard]] bool HasVerticalScrollBar() const;
    [[nodiscard]] Rect GetHorizontalScrollBarTrackRect() const;
    [[nodiscard]] Rect GetVerticalScrollBarTrackRect() const;
    [[nodiscard]] Rect GetHorizontalScrollBarThumbRect() const;
    [[nodiscard]] Rect GetVerticalScrollBarThumbRect() const;
    [[nodiscard]] ScrollBarPart HitTestScrollBar(const Vector2& position) const;

    ScrollAxis axis_ = ScrollAxis::Vertical;
    std::shared_ptr<View> content_{};
    Vector2 scrollOffset_{};
    Size contentSize_{};
    bool scrollBarsVisible_ = true;
    float scrollBarThickness_ = 8.0F;
    ColorRGBA scrollBarTrackColor_{0.08F, 0.10F, 0.14F, 0.55F};
    ColorRGBA scrollBarThumbColor_{0.70F, 0.76F, 0.88F, 0.88F};
    bool inertiaEnabled_ = true;
    float inertiaDecay_ = 7.5F;
    Vector2 inertiaVelocity_{};
    bool gestureScrolling_ = false;
    std::size_t activeGestureTouchCount_ = 0;
    ScrollBarPart activeScrollBarPart_ = ScrollBarPart::None;
    Vector2 scrollBarDragGrabOffset_{};
};

using ScrollBox = ScrollView;

}
