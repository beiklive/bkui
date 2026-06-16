#pragma once

#include <bkui/core/Types.hpp>
#include <bkui/render/RenderQueue.hpp>

#include <memory>
#include <string>
#include <vector>

namespace bk
{

/// 浮点尺寸。
struct Size
{
    float width = 0.0F;
    float height = 0.0F;
};

/// UI 视图基类，作为页面级节点组织子视图并驱动布局与绘制。
class View
{
public:
    virtual ~View() = default;

    /// 设置视图名称，便于调试与日志定位。
    void SetName(std::string name);
    /// 获取视图名称。
    [[nodiscard]] const std::string& GetName() const;

    /// 设置视图的最终布局区域。
    void SetFrame(const Rect& frame);
    /// 获取视图当前布局区域。
    [[nodiscard]] const Rect& GetFrame() const;

    /// 设置视图可见性。
    void SetVisible(bool visible);
    /// 查询视图当前是否可见。
    [[nodiscard]] bool IsVisible() const;

    /// 向当前视图添加一个子视图。
    void AddChild(std::shared_ptr<View> child);
    /// 移除指定子视图。
    void RemoveChild(const std::shared_ptr<View>& child);
    /// 清空全部子视图。
    void ClearChildren();
    /// 获取当前视图的子视图列表。
    [[nodiscard]] const std::vector<std::shared_ptr<View>>& Children() const;
    /// 获取父视图，根节点时返回空指针。
    [[nodiscard]] const View* GetParent() const;

    /// 根据可用空间估算自身期望尺寸。
    virtual Size Measure(const Size& available) const;
    /// 单帧更新入口，负责驱动布局与子树递归更新。
    virtual void Frame(float deltaSeconds = 0.0F);
    /// 递归执行布局。
    virtual void Layout();
    /// 递归输出渲染命令。
    virtual void Draw(RenderQueue& queue) const;
    /// 兼容旧接口的渲染入口。
    virtual void Render(RenderQueue& queue) const;

    /// 标记布局失效，下一帧会重新布局。
    void InvalidateLayout();
    /// 查询当前是否需要重新布局。
    [[nodiscard]] bool NeedsLayout() const;

protected:
    /// 子类可重写自身的单帧逻辑。
    virtual void Update(float deltaSeconds);
    /// 子类可重写自身绘制逻辑。
    virtual void DrawSelf(RenderQueue& queue) const;
    /// 绘制全部可见子视图。
    void DrawChildren(RenderQueue& queue) const;

    Rect frame_{};
    std::vector<std::shared_ptr<View>> children_;
    View* parent_ = nullptr;
    std::string name_{};
    bool visible_ = true;
    bool needsLayout_ = true;
};

}
