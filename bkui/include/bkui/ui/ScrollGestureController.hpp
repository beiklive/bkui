#pragma once

#include <bkui/ui/ScrollView.hpp>

#include <cstddef>
#include <memory>

namespace bk
{

/// 负责把鼠标/触摸位移转换成 ScrollBox 滚动与惯性速度的轻量控制器。
class ScrollGestureController
{
public:
    /// 开始跟踪一次候选手势。
    bool Begin(
        const std::shared_ptr<ScrollView>& target,
        Vector2 pointerPosition,
        std::size_t contactCount,
        bool isTouchGesture);

    /// 更新一次手势位移；返回 true 表示本次更新已进入实际滚动态。
    bool Update(Vector2 pointerPosition, std::size_t contactCount, float deltaSeconds);

    /// 结束当前手势，并在需要时向目标注入惯性速度。
    bool End();

    /// 取消当前手势，不注入惯性。
    void Cancel();

    /// 查询当前是否正在跟踪手势。
    [[nodiscard]] bool IsTracking() const;

    /// 查询当前是否已经进入实际滚动态。
    [[nodiscard]] bool IsDragging() const;

    /// 获取当前关联的滚动控件。
    [[nodiscard]] std::shared_ptr<ScrollView> GetTarget() const;

private:
    void Reset();

    std::weak_ptr<ScrollView> target_{};
    Vector2 startPosition_{};
    Vector2 lastPosition_{};
    Vector2 velocity_{};
    std::size_t contactCount_ = 0;
    bool tracking_ = false;
    bool dragging_ = false;
    bool isTouchGesture_ = false;
};

}
