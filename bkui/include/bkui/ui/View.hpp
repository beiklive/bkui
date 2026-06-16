#pragma once

#include <bkui/core/Types.hpp>
#include <bkui/render/RenderQueue.hpp>

#include <memory>
#include <vector>

namespace bk
{

/// 浮点尺寸。
struct Size
{
    float width = 0.0F;
    float height = 0.0F;
};

/// UI 视图基类，负责布局树组织与渲染命令分发。
class View
{
public:
    virtual ~View() = default;

    /// 设置视图的最终布局区域。
    void SetFrame(const Rect& frame);
    /// 获取视图当前布局区域。
    [[nodiscard]] const Rect& GetFrame() const;

    /// 向当前视图添加一个子视图。
    void AddChild(std::shared_ptr<View> child);
    /// 获取当前视图的子视图列表。
    [[nodiscard]] const std::vector<std::shared_ptr<View>>& Children() const;

    /// 根据可用空间估算自身期望尺寸。
    virtual Size Measure(const Size& available) const;
    /// 递归执行布局。
    virtual void Layout();
    /// 递归输出渲染命令。
    virtual void Render(RenderQueue& queue) const;

protected:
    Rect frame_{};
    std::vector<std::shared_ptr<View>> children_;
};

}
