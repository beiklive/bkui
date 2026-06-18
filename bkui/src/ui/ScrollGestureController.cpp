#include <bkui/ui/ScrollGestureController.hpp>

#include <algorithm>
#include <cmath>

namespace bk
{
namespace
{
constexpr float kGestureActivationDistance = 6.0F;
constexpr float kMinDeltaSeconds = 0.0001F;
}

bool ScrollGestureController::Begin(
    const std::shared_ptr<ScrollView>& target,
    Vector2 pointerPosition,
    std::size_t contactCount,
    bool isTouchGesture)
{
    Reset();
    if (!target)
    {
        return false;
    }

    if (isTouchGesture)
    {
        const std::size_t clampedCount = std::clamp<std::size_t>(contactCount, 1, 3);
        if (clampedCount == 0)
        {
            return false;
        }
        contactCount_ = clampedCount;
    }
    else
    {
        contactCount_ = 1;
    }

    target_ = target;
    startPosition_ = pointerPosition;
    lastPosition_ = pointerPosition;
    isTouchGesture_ = isTouchGesture;
    tracking_ = true;
    target->StopInertia();
    return true;
}

bool ScrollGestureController::Update(Vector2 pointerPosition, std::size_t contactCount, float deltaSeconds)
{
    std::shared_ptr<ScrollView> target = target_.lock();
    if (!tracking_ || !target)
    {
        Reset();
        return false;
    }

    if (isTouchGesture_ && (contactCount == 0 || contactCount > 3))
    {
        End();
        return false;
    }

    const Vector2 totalDelta{
        pointerPosition.x - startPosition_.x,
        pointerPosition.y - startPosition_.y,
    };
    const float travel = std::sqrt(totalDelta.x * totalDelta.x + totalDelta.y * totalDelta.y);
    if (!dragging_ && travel >= kGestureActivationDistance)
    {
        dragging_ = true;
        target->BeginGestureScroll(contactCount_);
    }

    const Vector2 frameDelta{
        pointerPosition.x - lastPosition_.x,
        pointerPosition.y - lastPosition_.y,
    };
    lastPosition_ = pointerPosition;

    if (!dragging_)
    {
        return false;
    }

    const Vector2 appliedOffsetDelta = target->ApplyGestureDelta(frameDelta);
    const float invDeltaSeconds = 1.0F / std::max(kMinDeltaSeconds, deltaSeconds);
    velocity_ = Vector2{
        appliedOffsetDelta.x * invDeltaSeconds,
        appliedOffsetDelta.y * invDeltaSeconds,
    };
    return true;
}

bool ScrollGestureController::End()
{
    std::shared_ptr<ScrollView> target = target_.lock();
    const bool wasDragging = dragging_;
    if (target && dragging_)
    {
        target->EndGestureScroll(velocity_);
    }
    Reset();
    return wasDragging;
}

void ScrollGestureController::Cancel()
{
    if (std::shared_ptr<ScrollView> target = target_.lock())
    {
        if (dragging_)
        {
            target->CancelGestureScroll();
        }
    }
    Reset();
}

bool ScrollGestureController::IsTracking() const
{
    return tracking_;
}

bool ScrollGestureController::IsDragging() const
{
    return dragging_;
}

std::shared_ptr<ScrollView> ScrollGestureController::GetTarget() const
{
    return target_.lock();
}

void ScrollGestureController::Reset()
{
    target_.reset();
    startPosition_ = {};
    lastPosition_ = {};
    velocity_ = {};
    contactCount_ = 0;
    tracking_ = false;
    dragging_ = false;
    isTouchGesture_ = false;
}

}
