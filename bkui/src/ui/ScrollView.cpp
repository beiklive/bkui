#include <bkui/core/Application.hpp>
#include <bkui/ui/ScrollView.hpp>

#include <algorithm>
#include <cmath>

namespace bk
{
namespace
{
constexpr float kScrollableMeasureExtent = 100000.0F;
constexpr float kMinThumbExtent = 28.0F;
constexpr float kScrollBarPadding = 2.0F;
constexpr float kMinInertiaVelocity = 8.0F;

float PositiveOrZero(float value)
{
    return std::max(0.0F, value);
}

bool PointInRect(const Vector2& point, const Rect& rect)
{
    return point.x >= rect.x &&
        point.y >= rect.y &&
        point.x <= rect.x + rect.width &&
        point.y <= rect.y + rect.height;
}
}

ScrollView::ScrollView(ScrollAxis axis)
    : axis_(axis)
{
    SetName("ScrollView");
}

void ScrollView::SetScrollAxis(ScrollAxis axis)
{
    if (axis_ == axis)
    {
        return;
    }

    axis_ = axis;
    ClampScrollOffset();
    InvalidateLayout();
}

ScrollAxis ScrollView::GetScrollAxis() const
{
    return axis_;
}

void ScrollView::SetContent(const std::shared_ptr<View>& content)
{
    if (content_ == content)
    {
        return;
    }

    ClearChildren();
    content_ = content;
    if (content_)
    {
        AddChild(content_);
    }

    scrollOffset_ = {};
    contentSize_ = {};
    inertiaVelocity_ = {};
    InvalidateLayout();
}

std::shared_ptr<View> ScrollView::GetContent() const
{
    return content_;
}

void ScrollView::SetScrollOffset(const Vector2& offset)
{
    Vector2 previous = scrollOffset_;
    scrollOffset_ = offset;
    ClampScrollOffset();
    if (scrollOffset_.x != previous.x || scrollOffset_.y != previous.y)
    {
        InvalidateLayout();
    }
}

Vector2 ScrollView::GetScrollOffset() const
{
    return scrollOffset_;
}

void ScrollView::SetScrollX(float offset)
{
    SetScrollOffset(Vector2{offset, scrollOffset_.y});
}

float ScrollView::GetScrollX() const
{
    return scrollOffset_.x;
}

void ScrollView::SetScrollY(float offset)
{
    SetScrollOffset(Vector2{scrollOffset_.x, offset});
}

float ScrollView::GetScrollY() const
{
    return scrollOffset_.y;
}

bool ScrollView::EnsureViewVisible(const std::shared_ptr<View>& view, float padding)
{
    if (!view || !content_ || (!ContainsInContentSubtree(view) && view != content_))
    {
        return false;
    }

    const Rect viewport = GetViewportFrame();
    const Rect target = view->GetFrame();
    Vector2 nextOffset = scrollOffset_;

    if (axis_ == ScrollAxis::Vertical || axis_ == ScrollAxis::Both)
    {
        const float visibleTop = viewport.y + padding;
        const float visibleBottom = viewport.y + viewport.height - padding;
        if (target.y < visibleTop)
        {
            nextOffset.y -= visibleTop - target.y;
        }
        else if (target.y + target.height > visibleBottom)
        {
            nextOffset.y += (target.y + target.height) - visibleBottom;
        }
    }

    if (axis_ == ScrollAxis::Horizontal || axis_ == ScrollAxis::Both)
    {
        const float visibleLeft = viewport.x + padding;
        const float visibleRight = viewport.x + viewport.width - padding;
        if (target.x < visibleLeft)
        {
            nextOffset.x -= visibleLeft - target.x;
        }
        else if (target.x + target.width > visibleRight)
        {
            nextOffset.x += (target.x + target.width) - visibleRight;
        }
    }

    if (nextOffset.x == scrollOffset_.x && nextOffset.y == scrollOffset_.y)
    {
        return false;
    }

    SetScrollOffset(nextOffset);
    Layout();
    return true;
}

Size ScrollView::GetContentSize() const
{
    return contentSize_;
}

void ScrollView::SetScrollBarsVisible(bool visible)
{
    scrollBarsVisible_ = visible;
}

bool ScrollView::AreScrollBarsVisible() const
{
    return scrollBarsVisible_;
}

void ScrollView::SetScrollBarThickness(float thickness)
{
    scrollBarThickness_ = std::max(4.0F, thickness);
}

float ScrollView::GetScrollBarThickness() const
{
    return scrollBarThickness_;
}

void ScrollView::SetScrollBarTrackColor(const ColorRGBA& color)
{
    scrollBarTrackColor_ = color;
}

const ColorRGBA& ScrollView::GetScrollBarTrackColor() const
{
    return scrollBarTrackColor_;
}

void ScrollView::SetScrollBarThumbColor(const ColorRGBA& color)
{
    scrollBarThumbColor_ = color;
}

const ColorRGBA& ScrollView::GetScrollBarThumbColor() const
{
    return scrollBarThumbColor_;
}

void ScrollView::SetInertiaEnabled(bool enabled)
{
    inertiaEnabled_ = enabled;
    if (!enabled)
    {
        inertiaVelocity_ = {};
    }
}

bool ScrollView::IsInertiaEnabled() const
{
    return inertiaEnabled_;
}

void ScrollView::SetInertiaDecay(float decayPerSecond)
{
    inertiaDecay_ = std::max(0.0F, decayPerSecond);
}

float ScrollView::GetInertiaDecay() const
{
    return inertiaDecay_;
}

bool ScrollView::ScrollByWheel(const Vector2& wheelDelta, float lineStep)
{
    const Vector2 offsetDelta{
        (axis_ == ScrollAxis::Horizontal || axis_ == ScrollAxis::Both) ? -wheelDelta.x * lineStep : 0.0F,
        (axis_ == ScrollAxis::Vertical || axis_ == ScrollAxis::Both) ? -wheelDelta.y * lineStep : 0.0F,
    };
    StopInertia();
    const Vector2 applied = ApplyScrollOffsetDelta(offsetDelta);
    return applied.x != 0.0F || applied.y != 0.0F;
}

void ScrollView::BeginGestureScroll(std::size_t contactCount)
{
    gestureScrolling_ = true;
    activeGestureTouchCount_ = std::clamp<std::size_t>(contactCount, 1, 3);
    inertiaVelocity_ = {};
}

Vector2 ScrollView::ApplyGestureDelta(const Vector2& gestureDelta)
{
    const float factor = static_cast<float>(std::max<std::size_t>(1, activeGestureTouchCount_));
    return ApplyScrollOffsetDelta(Vector2{
        -gestureDelta.x * factor,
        -gestureDelta.y * factor,
    });
}

void ScrollView::EndGestureScroll(const Vector2& offsetVelocityPerSecond)
{
    gestureScrolling_ = false;
    activeGestureTouchCount_ = 0;
    if (!inertiaEnabled_)
    {
        inertiaVelocity_ = {};
        return;
    }

    inertiaVelocity_ = offsetVelocityPerSecond;
    if (axis_ == ScrollAxis::Vertical)
    {
        inertiaVelocity_.x = 0.0F;
    }
    else if (axis_ == ScrollAxis::Horizontal)
    {
        inertiaVelocity_.y = 0.0F;
    }
}

void ScrollView::CancelGestureScroll()
{
    gestureScrolling_ = false;
    activeGestureTouchCount_ = 0;
    inertiaVelocity_ = {};
}

void ScrollView::StopInertia()
{
    inertiaVelocity_ = {};
}

bool ScrollView::BeginScrollBarDrag(const Vector2& position)
{
    activeScrollBarPart_ = HitTestScrollBar(position);
    if (activeScrollBarPart_ == ScrollBarPart::None)
    {
        return false;
    }

    inertiaVelocity_ = {};
    if (activeScrollBarPart_ == ScrollBarPart::HorizontalThumb)
    {
        const Rect thumb = GetHorizontalScrollBarThumbRect();
        scrollBarDragGrabOffset_ = Vector2{position.x - thumb.x, 0.0F};
    }
    else if (activeScrollBarPart_ == ScrollBarPart::VerticalThumb)
    {
        const Rect thumb = GetVerticalScrollBarThumbRect();
        scrollBarDragGrabOffset_ = Vector2{0.0F, position.y - thumb.y};
    }
    return true;
}

bool ScrollView::UpdateScrollBarDrag(const Vector2& position)
{
    if (activeScrollBarPart_ == ScrollBarPart::HorizontalThumb)
    {
        const Rect track = GetHorizontalScrollBarTrackRect();
        const Rect thumb = GetHorizontalScrollBarThumbRect();
        const float travel = std::max(1.0F, track.width - thumb.width);
        const float thumbX = std::clamp(
            position.x - scrollBarDragGrabOffset_.x,
            track.x,
            track.x + travel);
        const float normalized = (thumbX - track.x) / travel;
        const float maxOffset = std::max(0.0F, contentSize_.width - GetViewportFrame().width);
        SetScrollX(maxOffset * normalized);
        return true;
    }

    if (activeScrollBarPart_ == ScrollBarPart::VerticalThumb)
    {
        const Rect track = GetVerticalScrollBarTrackRect();
        const Rect thumb = GetVerticalScrollBarThumbRect();
        const float travel = std::max(1.0F, track.height - thumb.height);
        const float thumbY = std::clamp(
            position.y - scrollBarDragGrabOffset_.y,
            track.y,
            track.y + travel);
        const float normalized = (thumbY - track.y) / travel;
        const float maxOffset = std::max(0.0F, contentSize_.height - GetViewportFrame().height);
        SetScrollY(maxOffset * normalized);
        return true;
    }

    return false;
}

void ScrollView::EndScrollBarDrag()
{
    activeScrollBarPart_ = ScrollBarPart::None;
    scrollBarDragGrabOffset_ = {};
}

bool ScrollView::IsScrollBarDragging() const
{
    return activeScrollBarPart_ != ScrollBarPart::None;
}

bool ScrollView::IsScrolling() const
{
    return gestureScrolling_ ||
        IsScrollBarDragging() ||
        std::fabs(inertiaVelocity_.x) >= kMinInertiaVelocity ||
        std::fabs(inertiaVelocity_.y) >= kMinInertiaVelocity;
}

Size ScrollView::Measure(const Size& available) const
{
    if (!content_ || !content_->IsVisible())
    {
        return available;
    }

    const Rect viewport = GetViewportFrame();
    const float measureWidth =
        axis_ == ScrollAxis::Vertical ? viewport.width :
        axis_ == ScrollAxis::Horizontal ? kScrollableMeasureExtent :
        kScrollableMeasureExtent;
    const float measureHeight =
        axis_ == ScrollAxis::Horizontal ? viewport.height :
        axis_ == ScrollAxis::Vertical ? kScrollableMeasureExtent :
        kScrollableMeasureExtent;
    const Size measured = content_->MeasureLayout(Size{
        PositiveOrZero(measureWidth),
        PositiveOrZero(measureHeight),
    });

    return Size{
        std::min(available.width, measured.width),
        std::min(available.height, measured.height),
    };
}

void ScrollView::Layout()
{
    needsLayout_ = false;
    if (!content_ || !content_->IsVisible())
    {
        contentSize_ = {};
        scrollOffset_ = {};
        return;
    }

    const Rect viewport = GetViewportFrame();
    const float measureWidth =
        axis_ == ScrollAxis::Vertical ? viewport.width :
        axis_ == ScrollAxis::Horizontal ? kScrollableMeasureExtent :
        kScrollableMeasureExtent;
    const float measureHeight =
        axis_ == ScrollAxis::Horizontal ? viewport.height :
        axis_ == ScrollAxis::Vertical ? kScrollableMeasureExtent :
        kScrollableMeasureExtent;

    const Size measured = content_->MeasureLayout(Size{
        PositiveOrZero(measureWidth),
        PositiveOrZero(measureHeight),
    });
    contentSize_ = measured;
    ClampScrollOffset();

    float contentWidth = measured.width;
    float contentHeight = measured.height;
    if (axis_ == ScrollAxis::Vertical)
    {
        contentWidth = viewport.width;
    }
    else if (axis_ == ScrollAxis::Horizontal)
    {
        contentHeight = viewport.height;
    }

    content_->SetFrame(Rect{
        viewport.x - scrollOffset_.x,
        viewport.y - scrollOffset_.y,
        std::max(viewport.width, contentWidth),
        std::max(viewport.height, contentHeight),
    });
    content_->Layout();
}

void ScrollView::Paint(RenderQueue& queue) const
{
    if (!visible_)
    {
        return;
    }

    DrawShadow(queue);
    DrawBackground(queue);
    Draw(queue);
    queue.PushClipRect(GetViewportFrame());
    DrawChildren(queue);
    queue.PopClipRect();
    DrawDebugWireframe(queue);
}

void ScrollView::Update(float deltaSeconds)
{
    ApplyInertia(deltaSeconds);

    if (Application* app = Application::Active())
    {
        if (std::shared_ptr<View> focused = app->GetFocusedView())
        {
            if (focused == content_ || ContainsInContentSubtree(focused))
            {
                EnsureViewVisible(focused);
            }
        }
    }
}

void ScrollView::Draw(RenderQueue& queue) const
{
    if (!scrollBarsVisible_)
    {
        return;
    }

    if (HasHorizontalScrollBar())
    {
        const Rect track = GetHorizontalScrollBarTrackRect();
        const Rect thumb = GetHorizontalScrollBarThumbRect();
        queue.PushRoundedRect(track, scrollBarTrackColor_, track.height * 0.5F);
        queue.PushRoundedRect(thumb, scrollBarThumbColor_, thumb.height * 0.5F);
    }

    if (HasVerticalScrollBar())
    {
        const Rect track = GetVerticalScrollBarTrackRect();
        const Rect thumb = GetVerticalScrollBarThumbRect();
        queue.PushRoundedRect(track, scrollBarTrackColor_, track.width * 0.5F);
        queue.PushRoundedRect(thumb, scrollBarThumbColor_, thumb.width * 0.5F);
    }
}

Vector2 ScrollView::ApplyScrollOffsetDelta(const Vector2& offsetDelta)
{
    const Vector2 previous = scrollOffset_;
    SetScrollOffset(Vector2{
        scrollOffset_.x + offsetDelta.x,
        scrollOffset_.y + offsetDelta.y,
    });
    return Vector2{
        scrollOffset_.x - previous.x,
        scrollOffset_.y - previous.y,
    };
}

void ScrollView::ClampScrollOffset()
{
    const Rect viewport = GetViewportFrame();
    const float maxX = std::max(0.0F, contentSize_.width - viewport.width);
    const float maxY = std::max(0.0F, contentSize_.height - viewport.height);

    if (axis_ == ScrollAxis::Vertical)
    {
        scrollOffset_.x = 0.0F;
    }
    else
    {
        scrollOffset_.x = std::clamp(scrollOffset_.x, 0.0F, maxX);
    }

    if (axis_ == ScrollAxis::Horizontal)
    {
        scrollOffset_.y = 0.0F;
    }
    else
    {
        scrollOffset_.y = std::clamp(scrollOffset_.y, 0.0F, maxY);
    }
}

void ScrollView::ApplyInertia(float deltaSeconds)
{
    if (gestureScrolling_ || IsScrollBarDragging())
    {
        inertiaVelocity_ = {};
        return;
    }

    if (!inertiaEnabled_)
    {
        inertiaVelocity_ = {};
        return;
    }

    if (std::fabs(inertiaVelocity_.x) < kMinInertiaVelocity &&
        std::fabs(inertiaVelocity_.y) < kMinInertiaVelocity)
    {
        inertiaVelocity_ = {};
        return;
    }

    (void)ApplyScrollOffsetDelta(Vector2{
        inertiaVelocity_.x * deltaSeconds,
        inertiaVelocity_.y * deltaSeconds,
    });

    const float decayFactor = std::exp(-std::max(0.0F, inertiaDecay_) * std::max(0.0F, deltaSeconds));
    inertiaVelocity_ = Vector2{
        inertiaVelocity_.x * decayFactor,
        inertiaVelocity_.y * decayFactor,
    };
}

bool ScrollView::ContainsInContentSubtree(const std::shared_ptr<View>& view) const
{
    if (!content_ || !view)
    {
        return false;
    }

    if (content_ == view)
    {
        return true;
    }

    const View* current = view->GetParent();
    while (current != nullptr)
    {
        if (current == content_.get())
        {
            return true;
        }
        current = current->GetParent();
    }

    return false;
}

Rect ScrollView::GetViewportFrame() const
{
    return GetContentFrame();
}

bool ScrollView::HasHorizontalScrollBar() const
{
    return scrollBarsVisible_ &&
        (axis_ == ScrollAxis::Horizontal || axis_ == ScrollAxis::Both) &&
        contentSize_.width > GetViewportFrame().width + 0.5F;
}

bool ScrollView::HasVerticalScrollBar() const
{
    return scrollBarsVisible_ &&
        (axis_ == ScrollAxis::Vertical || axis_ == ScrollAxis::Both) &&
        contentSize_.height > GetViewportFrame().height + 0.5F;
}

Rect ScrollView::GetHorizontalScrollBarTrackRect() const
{
    const Rect viewport = GetViewportFrame();
    return Rect{
        viewport.x + kScrollBarPadding,
        viewport.y + viewport.height - scrollBarThickness_ - kScrollBarPadding,
        std::max(0.0F, viewport.width - kScrollBarPadding * 2.0F - (HasVerticalScrollBar() ? scrollBarThickness_ : 0.0F)),
        scrollBarThickness_,
    };
}

Rect ScrollView::GetVerticalScrollBarTrackRect() const
{
    const Rect viewport = GetViewportFrame();
    return Rect{
        viewport.x + viewport.width - scrollBarThickness_ - kScrollBarPadding,
        viewport.y + kScrollBarPadding,
        scrollBarThickness_,
        std::max(0.0F, viewport.height - kScrollBarPadding * 2.0F - (HasHorizontalScrollBar() ? scrollBarThickness_ : 0.0F)),
    };
}

Rect ScrollView::GetHorizontalScrollBarThumbRect() const
{
    const Rect track = GetHorizontalScrollBarTrackRect();
    const Rect viewport = GetViewportFrame();
    const float ratio = viewport.width > 0.0F ? std::clamp(viewport.width / std::max(viewport.width, contentSize_.width), 0.0F, 1.0F) : 1.0F;
    const float thumbWidth = std::max(kMinThumbExtent, track.width * ratio);
    const float maxOffset = std::max(1.0F, contentSize_.width - viewport.width);
    const float normalized = std::clamp(scrollOffset_.x / maxOffset, 0.0F, 1.0F);
    const float travel = std::max(0.0F, track.width - thumbWidth);
    return Rect{
        track.x + travel * normalized,
        track.y,
        thumbWidth,
        track.height,
    };
}

Rect ScrollView::GetVerticalScrollBarThumbRect() const
{
    const Rect track = GetVerticalScrollBarTrackRect();
    const Rect viewport = GetViewportFrame();
    const float ratio = viewport.height > 0.0F ? std::clamp(viewport.height / std::max(viewport.height, contentSize_.height), 0.0F, 1.0F) : 1.0F;
    const float thumbHeight = std::max(kMinThumbExtent, track.height * ratio);
    const float maxOffset = std::max(1.0F, contentSize_.height - viewport.height);
    const float normalized = std::clamp(scrollOffset_.y / maxOffset, 0.0F, 1.0F);
    const float travel = std::max(0.0F, track.height - thumbHeight);
    return Rect{
        track.x,
        track.y + travel * normalized,
        track.width,
        thumbHeight,
    };
}

ScrollView::ScrollBarPart ScrollView::HitTestScrollBar(const Vector2& position) const
{
    if (HasHorizontalScrollBar() && PointInRect(position, GetHorizontalScrollBarThumbRect()))
    {
        return ScrollBarPart::HorizontalThumb;
    }
    if (HasVerticalScrollBar() && PointInRect(position, GetVerticalScrollBarThumbRect()))
    {
        return ScrollBarPart::VerticalThumb;
    }
    return ScrollBarPart::None;
}

}
