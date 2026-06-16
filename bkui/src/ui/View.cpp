#include <bkui/core/Application.hpp>
#include <bkui/ui/View.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace bk
{
namespace
{
constexpr float kFrameEpsilon = 0.001F;

bool NearlyEqual(float lhs, float rhs)
{
    return std::fabs(lhs - rhs) <= kFrameEpsilon;
}

bool SameRect(const Rect& lhs, const Rect& rhs)
{
    return NearlyEqual(lhs.x, rhs.x) &&
        NearlyEqual(lhs.y, rhs.y) &&
        NearlyEqual(lhs.width, rhs.width) &&
        NearlyEqual(lhs.height, rhs.height);
}

float PositiveOrZero(float value)
{
    return std::max(0.0F, value);
}

std::size_t NavigationIndex(NavigationDirection direction)
{
    return static_cast<std::size_t>(direction);
}

Vector2 RectCenter(const Rect& rect)
{
    return Vector2{
        rect.x + rect.width * 0.5F,
        rect.y + rect.height * 0.5F,
    };
}
}

void View::SetName(std::string name)
{
    name_ = std::move(name);
}

const std::string& View::GetName() const
{
    return name_;
}

void View::SetFrame(const Rect& frame)
{
    if (SameRect(frame_, frame))
    {
        return;
    }

    const bool sizeChanged = !NearlyEqual(frame_.width, frame.width) || !NearlyEqual(frame_.height, frame.height);
    frame_ = frame;
    if (sizeChanged)
    {
        needsLayout_ = true;
    }
}

const Rect& View::GetFrame() const
{
    return frame_;
}

void View::SetVisible(bool visible)
{
    if (visible_ == visible)
    {
        return;
    }

    visible_ = visible;
    InvalidateLayout();
}

bool View::IsVisible() const
{
    return visible_;
}

void View::SetDebugWireframeEnabled(bool enabled)
{
    debugWireframeEnabled_ = enabled;
}

bool View::IsDebugWireframeEnabled() const
{
    return debugWireframeEnabled_;
}

void View::SetZIndex(int zIndex)
{
    if (zIndex_ == zIndex)
    {
        return;
    }

    zIndex_ = zIndex;
    if (parent_ != nullptr)
    {
        parent_->InvalidateLayout();
    }
}

int View::GetZIndex() const
{
    return zIndex_;
}

void View::SetFocusable(bool focusable)
{
    if (focusable_ == focusable)
    {
        return;
    }

    focusable_ = focusable;
    if (!focusable_ && focused_)
    {
        ClearFocus();
    }
}

bool View::IsFocusable() const
{
    return focusable_;
}

bool View::HasFocus() const
{
    return focused_;
}

void View::SetMargin(const EdgeInsets& margin)
{
    margin_ = margin;
    InvalidateLayout();
}

void View::SetMarginLeft(float value)
{
    margin_.left = value;
    InvalidateLayout();
}

void View::SetMarginTop(float value)
{
    margin_.top = value;
    InvalidateLayout();
}

void View::SetMarginRight(float value)
{
    margin_.right = value;
    InvalidateLayout();
}

void View::SetMarginBottom(float value)
{
    margin_.bottom = value;
    InvalidateLayout();
}

void View::SetMargin(float all)
{
    SetMargin(EdgeInsets{all, all, all, all});
}

void View::SetMargin(float vertical, float horizontal)
{
    SetMargin(EdgeInsets{horizontal, vertical, horizontal, vertical});
}

void View::SetMargin(float top, float right, float bottom, float left)
{
    SetMargin(EdgeInsets{left, top, right, bottom});
}

const EdgeInsets& View::GetMargin() const
{
    return margin_;
}

float View::GetMarginLeft() const
{
    return margin_.left;
}

float View::GetMarginTop() const
{
    return margin_.top;
}

float View::GetMarginRight() const
{
    return margin_.right;
}

float View::GetMarginBottom() const
{
    return margin_.bottom;
}

void View::SetPadding(const EdgeInsets& padding)
{
    padding_ = padding;
    InvalidateLayout();
}

void View::SetPaddingLeft(float value)
{
    padding_.left = value;
    InvalidateLayout();
}

void View::SetPaddingTop(float value)
{
    padding_.top = value;
    InvalidateLayout();
}

void View::SetPaddingRight(float value)
{
    padding_.right = value;
    InvalidateLayout();
}

void View::SetPaddingBottom(float value)
{
    padding_.bottom = value;
    InvalidateLayout();
}

void View::SetPadding(float all)
{
    SetPadding(EdgeInsets{all, all, all, all});
}

void View::SetPadding(float vertical, float horizontal)
{
    SetPadding(EdgeInsets{horizontal, vertical, horizontal, vertical});
}

void View::SetPadding(float top, float right, float bottom, float left)
{
    SetPadding(EdgeInsets{left, top, right, bottom});
}

const EdgeInsets& View::GetPadding() const
{
    return padding_;
}

float View::GetPaddingLeft() const
{
    return padding_.left;
}

float View::GetPaddingTop() const
{
    return padding_.top;
}

float View::GetPaddingRight() const
{
    return padding_.right;
}

float View::GetPaddingBottom() const
{
    return padding_.bottom;
}

void View::SetWidth(float width)
{
    width_ = width;
    InvalidateLayout();
}

float View::GetWidth() const
{
    return width_;
}

void View::ClearWidth()
{
    width_ = kAutoValue;
    InvalidateLayout();
}

void View::SetHeight(float height)
{
    height_ = height;
    InvalidateLayout();
}

float View::GetHeight() const
{
    return height_;
}

void View::ClearHeight()
{
    height_ = kAutoValue;
    InvalidateLayout();
}

void View::SetSize(float width, float height)
{
    width_ = width;
    height_ = height;
    InvalidateLayout();
}

void View::ClearSize()
{
    width_ = kAutoValue;
    height_ = kAutoValue;
    InvalidateLayout();
}

void View::SetWidthPercentage(float percentage)
{
    widthPercentage_ = NormalizePercentage(percentage);
    InvalidateLayout();
}

float View::GetWidthPercentage() const
{
    return widthPercentage_;
}

void View::ClearWidthPercentage()
{
    widthPercentage_ = kAutoValue;
    InvalidateLayout();
}

void View::SetHeightPercentage(float percentage)
{
    heightPercentage_ = NormalizePercentage(percentage);
    InvalidateLayout();
}

float View::GetHeightPercentage() const
{
    return heightPercentage_;
}

void View::ClearHeightPercentage()
{
    heightPercentage_ = kAutoValue;
    InvalidateLayout();
}

void View::SetSizePercentage(float widthPercentage, float heightPercentage)
{
    widthPercentage_ = NormalizePercentage(widthPercentage);
    heightPercentage_ = NormalizePercentage(heightPercentage);
    InvalidateLayout();
}

void View::ClearSizePercentage()
{
    widthPercentage_ = kAutoValue;
    heightPercentage_ = kAutoValue;
    InvalidateLayout();
}

void View::SetMinWidth(float width)
{
    minWidth_ = PositiveOrZero(width);
    InvalidateLayout();
}

float View::GetMinWidth() const
{
    return minWidth_;
}

void View::SetMinHeight(float height)
{
    minHeight_ = PositiveOrZero(height);
    InvalidateLayout();
}

float View::GetMinHeight() const
{
    return minHeight_;
}

void View::SetMinSize(float width, float height)
{
    minWidth_ = PositiveOrZero(width);
    minHeight_ = PositiveOrZero(height);
    InvalidateLayout();
}

void View::SetMaxWidth(float width)
{
    maxWidth_ = width > 0.0F ? width : std::numeric_limits<float>::max();
    InvalidateLayout();
}

float View::GetMaxWidth() const
{
    return maxWidth_;
}

void View::SetMaxHeight(float height)
{
    maxHeight_ = height > 0.0F ? height : std::numeric_limits<float>::max();
    InvalidateLayout();
}

float View::GetMaxHeight() const
{
    return maxHeight_;
}

void View::SetMaxSize(float width, float height)
{
    maxWidth_ = width > 0.0F ? width : std::numeric_limits<float>::max();
    maxHeight_ = height > 0.0F ? height : std::numeric_limits<float>::max();
    InvalidateLayout();
}

void View::ClearMaxSize()
{
    maxWidth_ = std::numeric_limits<float>::max();
    maxHeight_ = std::numeric_limits<float>::max();
    InvalidateLayout();
}

void View::SetFlexGrow(float flexGrow)
{
    flexGrow_ = PositiveOrZero(flexGrow);
    InvalidateLayout();
}

float View::GetFlexGrow() const
{
    return flexGrow_;
}

void View::SetFlexShrink(float flexShrink)
{
    flexShrink_ = PositiveOrZero(flexShrink);
    InvalidateLayout();
}

float View::GetFlexShrink() const
{
    return flexShrink_;
}

void View::SetNavigationTarget(NavigationDirection direction, const std::shared_ptr<View>& target)
{
    navigationTargets_[NavigationIndex(direction)] = target;
}

void View::ClearNavigationTarget(NavigationDirection direction)
{
    navigationTargets_[NavigationIndex(direction)].reset();
}

std::shared_ptr<View> View::GetNavigationTarget(NavigationDirection direction) const
{
    return navigationTargets_[NavigationIndex(direction)].lock();
}

std::shared_ptr<View> View::FindNavigationTarget(NavigationDirection direction) const
{
    if (const std::shared_ptr<View> manualTarget = GetNavigationTarget(direction))
    {
        if (manualTarget->IsVisible() && manualTarget->IsFocusable())
        {
            return manualTarget;
        }
    }

    return ResolveAutomaticNavigationTarget(direction);
}

bool View::RequestFocus()
{
    if (!focusable_ || !visible_)
    {
        return false;
    }

    if (Application* app = Application::Active())
    {
        return app->SetFocusedView(shared_from_this());
    }

    return false;
}

bool View::RequestDefaultFocus()
{
    if (std::shared_ptr<View> target = GetDefaultFocusView())
    {
        if (target->RequestFocus())
        {
            return true;
        }

        if (target.get() != this)
        {
            return target->RequestDefaultFocus();
        }
    }

    return RequestFocus();
}

bool View::RequestLastFocus()
{
    if (std::shared_ptr<View> target = GetLastFocusedView())
    {
        if (target->RequestFocus())
        {
            return true;
        }

        if (target.get() != this)
        {
            return target->RequestDefaultFocus();
        }
    }

    return RequestDefaultFocus();
}

void View::ClearFocus()
{
    if (Application* app = Application::Active())
    {
        app->ClearFocus(shared_from_this());
    }
}

void View::ClearCurrentFocus()
{
    if (Application* app = Application::Active())
    {
        if (std::shared_ptr<View> focused = app->GetFocusedView())
        {
            if (focused.get() == this || ContainsDescendant(focused))
            {
                app->ClearFocus();
            }
        }
    }
}

void View::SetDefaultFocusView(const std::shared_ptr<View>& view)
{
    defaultFocusView_ = view;
}

std::shared_ptr<View> View::GetDefaultFocusView() const
{
    return defaultFocusView_.lock();
}

std::shared_ptr<View> View::GetLastFocusedView() const
{
    return lastFocusedView_.lock();
}

void View::AddChild(std::shared_ptr<View> child)
{
    if (child)
    {
        child->parent_ = this;
        children_.push_back(std::move(child));
        InvalidateLayout();
    }
}

void View::RemoveChild(const std::shared_ptr<View>& child)
{
    const auto it = std::remove(children_.begin(), children_.end(), child);
    if (it != children_.end())
    {
        for (auto clearIt = it; clearIt != children_.end(); ++clearIt)
        {
            if (*clearIt)
            {
                (*clearIt)->parent_ = nullptr;
            }
        }
        children_.erase(it, children_.end());
        InvalidateLayout();
    }
}

void View::ClearChildren()
{
    if (!children_.empty())
    {
        for (auto& child : children_)
        {
            if (child)
            {
                child->parent_ = nullptr;
            }
        }
        children_.clear();
        InvalidateLayout();
    }
}

const std::vector<std::shared_ptr<View>>& View::Children() const
{
    return children_;
}

const View* View::GetParent() const
{
    return parent_;
}

Size View::Measure(const Size& available) const
{
    return available;
}

Size View::MeasureLayout(const Size& available) const
{
    const float marginWidth = margin_.left + margin_.right;
    const float marginHeight = margin_.top + margin_.bottom;
    const float availableBorderWidth = PositiveOrZero(available.width - marginWidth);
    const float availableBorderHeight = PositiveOrZero(available.height - marginHeight);
    const float availableContentWidth = PositiveOrZero(availableBorderWidth - padding_.left - padding_.right);
    const float availableContentHeight = PositiveOrZero(availableBorderHeight - padding_.top - padding_.bottom);

    Size measuredContent = Measure(Size{availableContentWidth, availableContentHeight});
    measuredContent.width = PositiveOrZero(measuredContent.width);
    measuredContent.height = PositiveOrZero(measuredContent.height);

    const Size constrainedBorder = ResolveConstrainedSize(
        Size{availableBorderWidth, availableBorderHeight},
        measuredContent);
    return Size{
        constrainedBorder.width + marginWidth,
        constrainedBorder.height + marginHeight,
    };
}

void View::Frame(float deltaSeconds)
{
    if (!visible_)
    {
        return;
    }

    if (needsLayout_)
    {
        Layout();
    }

    Update(deltaSeconds);

    for (const auto& child : VisibleChildrenByZOrder())
    {
        child->Frame(deltaSeconds);
    }
}

void View::Layout()
{
    needsLayout_ = false;
    for (const auto& child : VisibleChildrenByZOrder())
    {
        child->Layout();
    }
}

void View::Paint(RenderQueue& queue) const
{
    if (!visible_)
    {
        return;
    }

    Draw(queue);
    DrawChildren(queue);
    DrawDebugWireframe(queue);
}

void View::Render(RenderQueue& queue) const
{
    Paint(queue);
}

void View::InvalidateLayout()
{
    if (needsLayout_)
    {
        if (parent_ != nullptr)
        {
            parent_->InvalidateLayout();
        }
        return;
    }

    needsLayout_ = true;
    if (parent_ != nullptr)
    {
        parent_->InvalidateLayout();
    }
}

bool View::NeedsLayout() const
{
    return needsLayout_;
}

Rect View::GetContentFrame() const
{
    return Rect{
        frame_.x + padding_.left,
        frame_.y + padding_.top,
        PositiveOrZero(frame_.width - padding_.left - padding_.right),
        PositiveOrZero(frame_.height - padding_.top - padding_.bottom),
    };
}

Rect View::GetMarginFrame() const
{
    return Rect{
        frame_.x - margin_.left,
        frame_.y - margin_.top,
        frame_.width + margin_.left + margin_.right,
        frame_.height + margin_.top + margin_.bottom,
    };
}

Rect View::ResolveFrameInSlot(const Rect& slot) const
{
    return Rect{
        slot.x + margin_.left,
        slot.y + margin_.top,
        PositiveOrZero(slot.width - margin_.left - margin_.right),
        PositiveOrZero(slot.height - margin_.top - margin_.bottom),
    };
}

bool View::ContainsPoint(const Vector2& point) const
{
    return point.x >= frame_.x &&
        point.y >= frame_.y &&
        point.x < frame_.x + frame_.width &&
        point.y < frame_.y + frame_.height;
}

std::shared_ptr<View> View::HitTest(const Vector2& point)
{
    if (!visible_ || !ContainsPoint(point))
    {
        return nullptr;
    }

    for (const auto& child : VisibleChildrenByZOrder(true))
    {
        if (std::shared_ptr<View> hit = child->HitTest(point))
        {
            return hit;
        }
    }

    return shared_from_this();
}

void View::DispatchPointerDown(const Vector2& position)
{
    PointerDown(position);
}

void View::DispatchPointerUp(const Vector2& position)
{
    PointerUp(position);
}

void View::DispatchPointerMove(const Vector2& position)
{
    PointerMove(position);
}

void View::DispatchClick(const Vector2& position)
{
    Click(position);
}

void View::DispatchKeyDown(const InputState::KeyEvent& key)
{
    KeyDown(key);
}

void View::DispatchKeyUp(const InputState::KeyEvent& key)
{
    KeyUp(key);
}

void View::Update(float)
{
}

void View::Draw(RenderQueue&) const
{
}

void View::FocusIn()
{
}

void View::FocusOut()
{
}

void View::Click(const Vector2&)
{
}

void View::PointerDown(const Vector2&)
{
}

void View::PointerUp(const Vector2&)
{
}

void View::PointerMove(const Vector2&)
{
}

void View::KeyDown(const InputState::KeyEvent&)
{
}

void View::KeyUp(const InputState::KeyEvent&)
{
}

void View::DrawChildren(RenderQueue& queue) const
{
    for (const auto& child : VisibleChildrenByZOrder())
    {
        child->Paint(queue);
    }
}

void View::DrawDebugWireframe(RenderQueue& queue) const
{
    if (!debugWireframeEnabled_)
    {
        return;
    }

    const auto pushDiagonalPair = [&](const Rect& rect, const Color& color) {
        if (rect.width <= 0.0F || rect.height <= 0.0F)
        {
            return;
        }

        queue.PushLine(
            Vector2{rect.x, rect.y},
            Vector2{rect.x + rect.width, rect.y + rect.height},
            color);
        queue.PushLine(
            Vector2{rect.x + rect.width, rect.y},
            Vector2{rect.x, rect.y + rect.height},
            color);
    };

    pushDiagonalPair(GetMarginFrame(), Color{1.0F, 0.18F, 0.18F, 0.92F});
    pushDiagonalPair(frame_, Color{0.22F, 0.48F, 1.0F, 0.92F});
    pushDiagonalPair(GetContentFrame(), Color{1.0F, 0.84F, 0.18F, 0.92F});
}

float View::NormalizePercentage(float percentage)
{
    if (percentage < 0.0F)
    {
        return kAutoValue;
    }

    if (percentage > 1.0F)
    {
        percentage /= 100.0F;
    }

    return std::clamp(percentage, 0.0F, 1.0F);
}

Size View::ResolveConstrainedSize(const Size& available, const Size& contentSize) const
{
    const float autoWidth = contentSize.width + padding_.left + padding_.right;
    const float autoHeight = contentSize.height + padding_.top + padding_.bottom;

    float resolvedWidth = autoWidth;
    if (width_ >= 0.0F)
    {
        resolvedWidth = width_;
    }
    else if (widthPercentage_ >= 0.0F)
    {
        resolvedWidth = available.width * widthPercentage_;
    }

    float resolvedHeight = autoHeight;
    if (height_ >= 0.0F)
    {
        resolvedHeight = height_;
    }
    else if (heightPercentage_ >= 0.0F)
    {
        resolvedHeight = available.height * heightPercentage_;
    }

    resolvedWidth = std::clamp(PositiveOrZero(resolvedWidth), minWidth_, maxWidth_);
    resolvedHeight = std::clamp(PositiveOrZero(resolvedHeight), minHeight_, maxHeight_);
    resolvedWidth = std::min(resolvedWidth, PositiveOrZero(available.width));
    resolvedHeight = std::min(resolvedHeight, PositiveOrZero(available.height));
    return Size{resolvedWidth, resolvedHeight};
}

std::vector<std::shared_ptr<View>> View::VisibleChildrenByZOrder(bool descending) const
{
    std::vector<std::shared_ptr<View>> ordered;
    ordered.reserve(children_.size());
    for (const auto& child : children_)
    {
        if (child && child->IsVisible())
        {
            ordered.push_back(child);
        }
    }

    std::stable_sort(
        ordered.begin(),
        ordered.end(),
        [descending](const std::shared_ptr<View>& lhs, const std::shared_ptr<View>& rhs) {
            return descending ? lhs->GetZIndex() > rhs->GetZIndex() : lhs->GetZIndex() < rhs->GetZIndex();
        });
    return ordered;
}

void View::SetFocusedState(bool focused)
{
    if (focused_ == focused)
    {
        return;
    }

    focused_ = focused;
    if (focused_)
    {
        FocusIn();
    }
    else
    {
        FocusOut();
    }
}

void View::RememberFocusedDescendant(const std::shared_ptr<View>& focusedView)
{
    lastFocusedView_ = focusedView;
}

bool View::ContainsDescendant(const std::shared_ptr<View>& view) const
{
    if (!view)
    {
        return false;
    }

    const View* current = view->GetParent();
    while (current != nullptr)
    {
        if (current == this)
        {
            return true;
        }
        current = current->GetParent();
    }

    return false;
}

std::shared_ptr<View> View::ResolveAutomaticNavigationTarget(NavigationDirection direction) const
{
    if (parent_ == nullptr)
    {
        return nullptr;
    }

    const Vector2 sourceCenter = RectCenter(frame_);
    std::shared_ptr<View> bestCandidate;
    float bestPrimaryDistance = std::numeric_limits<float>::max();
    float bestSecondaryDistance = std::numeric_limits<float>::max();
    int bestZIndex = std::numeric_limits<int>::min();

    for (const auto& sibling : parent_->VisibleChildrenByZOrder())
    {
        if (!sibling || sibling.get() == this || !sibling->IsFocusable())
        {
            continue;
        }

        const Vector2 targetCenter = RectCenter(sibling->GetFrame());
        const float deltaX = targetCenter.x - sourceCenter.x;
        const float deltaY = targetCenter.y - sourceCenter.y;

        float primaryDistance = 0.0F;
        float secondaryDistance = 0.0F;
        bool validDirection = false;
        switch (direction)
        {
        case NavigationDirection::Up:
            validDirection = deltaY < 0.0F;
            primaryDistance = -deltaY;
            secondaryDistance = std::fabs(deltaX);
            break;
        case NavigationDirection::Down:
            validDirection = deltaY > 0.0F;
            primaryDistance = deltaY;
            secondaryDistance = std::fabs(deltaX);
            break;
        case NavigationDirection::Left:
            validDirection = deltaX < 0.0F;
            primaryDistance = -deltaX;
            secondaryDistance = std::fabs(deltaY);
            break;
        case NavigationDirection::Right:
            validDirection = deltaX > 0.0F;
            primaryDistance = deltaX;
            secondaryDistance = std::fabs(deltaY);
            break;
        }

        if (!validDirection)
        {
            continue;
        }

        const bool betterPrimary = primaryDistance < bestPrimaryDistance - kFrameEpsilon;
        const bool samePrimary = NearlyEqual(primaryDistance, bestPrimaryDistance);
        const bool betterSecondary = secondaryDistance < bestSecondaryDistance - kFrameEpsilon;
        const bool sameSecondary = NearlyEqual(secondaryDistance, bestSecondaryDistance);
        const bool betterZ = sibling->GetZIndex() > bestZIndex;

        if (betterPrimary ||
            (samePrimary && betterSecondary) ||
            (samePrimary && sameSecondary && betterZ))
        {
            bestCandidate = sibling;
            bestPrimaryDistance = primaryDistance;
            bestSecondaryDistance = secondaryDistance;
            bestZIndex = sibling->GetZIndex();
        }
    }

    return bestCandidate;
}

}
