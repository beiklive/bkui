#include <bkui/core/Application.hpp>
#include <bkui/ui/ScrollView.hpp>

#include <algorithm>

namespace bk
{
namespace
{
constexpr float kScrollableMeasureExtent = 100000.0F;

float PositiveOrZero(float value)
{
    return std::max(0.0F, value);
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
    InvalidateLayout();
}

std::shared_ptr<View> ScrollView::GetContent() const
{
    return content_;
}

void ScrollView::SetScrollOffset(const Vector2& offset)
{
    Vector2 nextOffset = offset;
    if (axis_ == ScrollAxis::Vertical)
    {
        nextOffset.x = 0.0F;
    }
    else if (axis_ == ScrollAxis::Horizontal)
    {
        nextOffset.y = 0.0F;
    }

    if (scrollOffset_.x == nextOffset.x && scrollOffset_.y == nextOffset.y)
    {
        return;
    }

    scrollOffset_ = nextOffset;
    ClampScrollOffset();
    InvalidateLayout();
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

    scrollOffset_ = nextOffset;
    ClampScrollOffset();
    Layout();
    return true;
}

Size ScrollView::GetContentSize() const
{
    return contentSize_;
}

Size ScrollView::Measure(const Size& available) const
{
    if (!content_ || !content_->IsVisible())
    {
        return available;
    }

    const Rect viewport = GetViewportFrame();
    const float measureWidth = axis_ == ScrollAxis::Vertical ? viewport.width : kScrollableMeasureExtent;
    const float measureHeight = axis_ == ScrollAxis::Horizontal ? viewport.height : kScrollableMeasureExtent;
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

    Draw(queue);
    queue.PushClipRect(GetViewportFrame());
    DrawChildren(queue);
    DrawDebugWireframe(queue);
    queue.PopClipRect();
}

void ScrollView::Update(float)
{
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

}
