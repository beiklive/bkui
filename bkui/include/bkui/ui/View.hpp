#pragma once

#include <bkui/core/Types.hpp>
#include <bkui/platform/Platform.hpp>
#include <bkui/render/RenderQueue.hpp>

#include <array>
#include <limits>
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

/// 盒模型四边距定义，可同时用于 margin 与 padding。
struct EdgeInsets
{
    float left = 0.0F;
    float top = 0.0F;
    float right = 0.0F;
    float bottom = 0.0F;
};

/// 阴影样式定义。
struct ShadowStyle
{
    Vector2 offset{};
    ColorRGBA color{0.0F, 0.0F, 0.0F, 0.24F};
    float blurRadius = 0.0F;
    float spread = 0.0F;
    bool enabled = false;
};

/// 聚焦框样式定义。Application 负责聚焦框移动逻辑，
/// 每个 View 负责声明自己的聚焦框外观。
struct FocusHighlightStyle
{
    ColorRGBA color1{0.34F, 0.76F, 1.0F, 1.0F};
    ColorRGBA color2{0.76F, 0.52F, 1.0F, 1.0F};
    float cornerRadius = -1.0F;
    float inset = 6.0F;
    float thickness = 3.0F;
    float glowThickness = 5.0F;
    bool enabled = true;
};

/// 焦点导航方向。
enum class NavigationDirection
{
    Up = 0,
    Down = 1,
    Left = 2,
    Right = 3,
};

/// UI 视图基类，作为页面级节点组织子视图并驱动布局、绘制、焦点与交互。
class View : public std::enable_shared_from_this<View>
{
public:
    static constexpr float kAutoValue = -1.0F;

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

    /// 设置该视图是否绘制调试线框。
    void SetDebugWireframeEnabled(bool enabled);
    /// 查询调试线框是否开启。
    [[nodiscard]] bool IsDebugWireframeEnabled() const;

    /// 设置视图层级 zIndex，数值越大越靠上。
    void SetZIndex(int zIndex);
    /// 获取视图层级 zIndex。
    [[nodiscard]] int GetZIndex() const;

    /// 设置该视图是否允许获取焦点。
    void SetFocusable(bool focusable);
    /// 查询该视图是否允许获取焦点。
    [[nodiscard]] bool IsFocusable() const;
    /// 查询该视图当前是否处于焦点状态。
    [[nodiscard]] bool HasFocus() const;

    /// 设置背景颜色。
    void SetBackgroundColor(const ColorRGBA& color);
    /// 获取背景颜色。
    [[nodiscard]] const ColorRGBA& GetBackgroundColor() const;
    /// 控制是否绘制背景。
    void SetDrawBackground(bool enabled);
    /// 查询当前是否绘制背景。
    [[nodiscard]] bool IsBackgroundEnabled() const;
    /// 设置圆角半径。
    void SetCornerRadius(float radius);
    /// 获取圆角半径。
    [[nodiscard]] float GetCornerRadius() const;
    /// 设置完整阴影样式。
    void SetShadowStyle(const ShadowStyle& shadow);
    /// 获取阴影样式。
    [[nodiscard]] const ShadowStyle& GetShadowStyle() const;
    /// 设置阴影是否启用。
    void SetShadowEnabled(bool enabled);
    /// 查询阴影是否启用。
    [[nodiscard]] bool IsShadowEnabled() const;
    /// 设置阴影偏移。
    void SetShadowOffset(const Vector2& offset);
    void SetShadowOffset(float x, float y);
    /// 获取阴影偏移。
    [[nodiscard]] const Vector2& GetShadowOffset() const;
    /// 设置阴影颜色。
    void SetShadowColor(const ColorRGBA& color);
    /// 获取阴影颜色。
    [[nodiscard]] const ColorRGBA& GetShadowColor() const;
    /// 设置阴影模糊半径。
    void SetShadowBlurRadius(float radius);
    /// 获取阴影模糊半径。
    [[nodiscard]] float GetShadowBlurRadius() const;
    /// 设置阴影扩张半径。
    void SetShadowSpread(float spread);
    /// 获取阴影扩张半径。
    [[nodiscard]] float GetShadowSpread() const;

    /// 设置完整聚焦框样式。
    void SetFocusHighlightStyle(const FocusHighlightStyle& style);
    /// 获取聚焦框样式。
    [[nodiscard]] const FocusHighlightStyle& GetFocusHighlightStyle() const;
    /// 设置聚焦框圆角半径，负数表示跟随视图自身圆角。
    void SetFocusHighlightCornerRadius(float radius);
    /// 获取聚焦框圆角半径。
    [[nodiscard]] float GetFocusHighlightCornerRadius() const;
    /// 设置聚焦框高亮颜色。
    void SetFocusHighlightColors(const ColorRGBA& color1, const ColorRGBA& color2);

    /// 设置视图外边距。
    void SetMargin(const EdgeInsets& margin);
    void SetMarginLeft(float value);
    void SetMarginTop(float value);
    void SetMarginRight(float value);
    void SetMarginBottom(float value);
    /// 使用统一值设置四个方向的外边距。
    void SetMargin(float all);
    /// 使用垂直/水平值设置外边距。
    void SetMargin(float vertical, float horizontal);
    /// 分别设置上右下左外边距。
    void SetMargin(float top, float right, float bottom, float left);
    /// 获取视图外边距。
    [[nodiscard]] const EdgeInsets& GetMargin() const;
    [[nodiscard]] float GetMarginLeft() const;
    [[nodiscard]] float GetMarginTop() const;
    [[nodiscard]] float GetMarginRight() const;
    [[nodiscard]] float GetMarginBottom() const;

    /// 设置视图内边距。
    void SetPadding(const EdgeInsets& padding);
    void SetPaddingLeft(float value);
    void SetPaddingTop(float value);
    void SetPaddingRight(float value);
    void SetPaddingBottom(float value);
    /// 使用统一值设置四个方向的内边距。
    void SetPadding(float all);
    /// 使用垂直/水平值设置内边距。
    void SetPadding(float vertical, float horizontal);
    /// 分别设置上右下左内边距。
    void SetPadding(float top, float right, float bottom, float left);
    /// 获取视图内边距。
    [[nodiscard]] const EdgeInsets& GetPadding() const;
    [[nodiscard]] float GetPaddingLeft() const;
    [[nodiscard]] float GetPaddingTop() const;
    [[nodiscard]] float GetPaddingRight() const;
    [[nodiscard]] float GetPaddingBottom() const;

    /// 设置固定宽度，作用于当前视图边框区域，不包含 margin。
    void SetWidth(float width);
    /// 读取固定宽度，未设置时返回 kAutoValue。
    [[nodiscard]] float GetWidth() const;
    /// 清除固定宽度设置。
    void ClearWidth();

    /// 设置固定高度，作用于当前视图边框区域，不包含 margin。
    void SetHeight(float height);
    /// 读取固定高度，未设置时返回 kAutoValue。
    [[nodiscard]] float GetHeight() const;
    /// 清除固定高度设置。
    void ClearHeight();

    /// 同时设置固定宽高。
    void SetSize(float width, float height);
    /// 清除固定宽高。
    void ClearSize();

    /// 设置宽度百分比，支持 0.0-1.0 或 0-100 两种写法。
    void SetWidthPercentage(float percentage);
    /// 获取宽度百分比，未设置时返回 kAutoValue。
    [[nodiscard]] float GetWidthPercentage() const;
    /// 清除宽度百分比设置。
    void ClearWidthPercentage();

    /// 设置高度百分比，支持 0.0-1.0 或 0-100 两种写法。
    void SetHeightPercentage(float percentage);
    /// 获取高度百分比，未设置时返回 kAutoValue。
    [[nodiscard]] float GetHeightPercentage() const;
    /// 清除高度百分比设置。
    void ClearHeightPercentage();

    /// 同时设置宽高百分比。
    void SetSizePercentage(float widthPercentage, float heightPercentage);
    /// 清除宽高百分比。
    void ClearSizePercentage();

    /// 设置最小宽度。
    void SetMinWidth(float width);
    /// 获取最小宽度。
    [[nodiscard]] float GetMinWidth() const;
    /// 设置最小高度。
    void SetMinHeight(float height);
    /// 获取最小高度。
    [[nodiscard]] float GetMinHeight() const;
    /// 同时设置最小尺寸。
    void SetMinSize(float width, float height);

    /// 设置最大宽度。
    void SetMaxWidth(float width);
    /// 获取最大宽度。
    [[nodiscard]] float GetMaxWidth() const;
    /// 设置最大高度。
    void SetMaxHeight(float height);
    /// 获取最大高度。
    [[nodiscard]] float GetMaxHeight() const;
    /// 同时设置最大尺寸。
    void SetMaxSize(float width, float height);
    /// 清除最大尺寸约束。
    void ClearMaxSize();

    /// 设置在线性容器中的扩展权重。
    void SetFlexGrow(float flexGrow);
    /// 获取扩展权重。
    [[nodiscard]] float GetFlexGrow() const;

    /// 设置在线性容器中的收缩权重。
    void SetFlexShrink(float flexShrink);
    /// 获取收缩权重。
    [[nodiscard]] float GetFlexShrink() const;

    /// 设置手动焦点导航目标。
    void SetNavigationTarget(NavigationDirection direction, const std::shared_ptr<View>& target);
    /// 清除手动焦点导航目标。
    void ClearNavigationTarget(NavigationDirection direction);
    /// 获取手动设置的导航目标。
    [[nodiscard]] std::shared_ptr<View> GetNavigationTarget(NavigationDirection direction) const;
    /// 自动或手动解析最终导航目标。
    [[nodiscard]] std::shared_ptr<View> FindNavigationTarget(NavigationDirection direction) const;

    /// 请求让当前视图成为焦点视图。
    bool RequestFocus();
    /// 请求将当前视图的默认焦点节点设为焦点。
    bool RequestDefaultFocus();
    /// 请求将当前视图记录的上一次焦点节点设为焦点。
    bool RequestLastFocus();
    /// 如果当前视图持有焦点则清除。
    void ClearFocus();
    /// 清除当前视图子树中的焦点。
    void ClearCurrentFocus();
    /// 设置当前视图的默认焦点节点。
    void SetDefaultFocusView(const std::shared_ptr<View>& view);
    /// 获取当前视图的默认焦点节点。
    [[nodiscard]] std::shared_ptr<View> GetDefaultFocusView() const;
    /// 获取当前视图记录的上一次焦点节点。
    [[nodiscard]] std::shared_ptr<View> GetLastFocusedView() const;

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

    /// 根据可用空间估算自身内容区期望尺寸。
    virtual Size Measure(const Size& available) const;
    /// 结合 margin、padding 和尺寸约束计算最终占位尺寸。
    [[nodiscard]] Size MeasureLayout(const Size& available) const;
    /// 单帧更新入口，负责驱动布局与子树递归更新。
    virtual void Frame(float deltaSeconds = 0.0F);
    /// 递归执行布局。
    virtual void Layout();
    /// 递归输出渲染命令。
    virtual void Paint(RenderQueue& queue) const;
    /// 兼容旧接口的渲染入口。
    virtual void Render(RenderQueue& queue) const;

    /// 标记布局失效，下一帧会重新布局。
    void InvalidateLayout();
    /// 查询当前是否需要重新布局。
    [[nodiscard]] bool NeedsLayout() const;
    /// 获取去除 padding 后的内容区域。
    [[nodiscard]] Rect GetContentFrame() const;
    /// 获取包含 margin 的外占位区域。
    [[nodiscard]] Rect GetMarginFrame() const;
    /// 将父容器分配的槽位换算为当前视图真实 frame。
    [[nodiscard]] Rect ResolveFrameInSlot(const Rect& slot) const;

    /// 判断点是否落在当前视图范围内。
    [[nodiscard]] bool ContainsPoint(const Vector2& point) const;
    /// 命中测试并返回最上层可见视图。
    [[nodiscard]] std::shared_ptr<View> HitTest(const Vector2& point);

    /// 分发指针按下事件。
    void DispatchPointerDown(const Vector2& position);
    /// 分发指针抬起事件。
    void DispatchPointerUp(const Vector2& position);
    /// 分发指针移动事件。
    void DispatchPointerMove(const Vector2& position);
    /// 分发点击事件。
    void DispatchClick(const Vector2& position);
    /// 分发按键按下事件。
    void DispatchKeyDown(const InputState::KeyEvent& key);
    /// 分发按键抬起事件。
    void DispatchKeyUp(const InputState::KeyEvent& key);

protected:
    /// 子类可重写自身的单帧逻辑。
    virtual void Update(float deltaSeconds);
    /// 子类可重写自身绘制逻辑。
    virtual void Draw(RenderQueue& queue) const;
    /// 焦点进入回调。
    virtual void FocusIn();
    /// 焦点离开回调。
    virtual void FocusOut();
    /// 点击回调。
    virtual void Click(const Vector2& position);
    /// 指针按下回调。
    virtual void PointerDown(const Vector2& position);
    /// 指针抬起回调。
    virtual void PointerUp(const Vector2& position);
    /// 指针移动回调。
    virtual void PointerMove(const Vector2& position);
    /// 按键按下回调。
    virtual void KeyDown(const InputState::KeyEvent& key);
    /// 按键抬起回调。
    virtual void KeyUp(const InputState::KeyEvent& key);

    /// 绘制全部可见子视图。
    void DrawChildren(RenderQueue& queue) const;
    /// 绘制基础阴影。
    void DrawShadow(RenderQueue& queue) const;
    /// 绘制基础背景。
    void DrawBackground(RenderQueue& queue) const;
    /// 绘制调试线框。
    void DrawDebugWireframe(RenderQueue& queue) const;
    /// 将百分比写法统一归一化到 0.0-1.0。
    [[nodiscard]] static float NormalizePercentage(float percentage);
    /// 将内容尺寸转换为带约束的边框尺寸。
    [[nodiscard]] Size ResolveConstrainedSize(const Size& available, const Size& contentSize) const;
    /// 返回按 zIndex 排序后的可见子视图。
    [[nodiscard]] const std::vector<std::shared_ptr<View>>& VisibleChildrenByZOrder(bool descending = false) const;

    Rect frame_{};
    std::vector<std::shared_ptr<View>> children_;
    View* parent_ = nullptr;
    std::string name_{};
    bool visible_ = true;
    bool needsLayout_ = true;
    bool debugWireframeEnabled_ = false;
    bool focusable_ = false;
    bool focused_ = false;
    int zIndex_ = 0;
    ColorRGBA backgroundColor_{0.0F, 0.0F, 0.0F, 0.0F};
    bool drawBackground_ = false;
    float cornerRadius_ = 0.0F;
    ShadowStyle shadow_{};
    FocusHighlightStyle focusHighlightStyle_{};
    EdgeInsets margin_{};
    EdgeInsets padding_{};
    float width_ = kAutoValue;
    float height_ = kAutoValue;
    float widthPercentage_ = kAutoValue;
    float heightPercentage_ = kAutoValue;
    float minWidth_ = 0.0F;
    float minHeight_ = 0.0F;
    float maxWidth_ = std::numeric_limits<float>::max();
    float maxHeight_ = std::numeric_limits<float>::max();
    float flexGrow_ = 0.0F;
    float flexShrink_ = 1.0F;

private:
    friend class Application;

    void InvalidateVisibleChildrenCache();
    void SetFocusedState(bool focused);
    void RememberFocusedDescendant(const std::shared_ptr<View>& focusedView);
    [[nodiscard]] bool ContainsDescendant(const std::shared_ptr<View>& view) const;
    [[nodiscard]] std::shared_ptr<View> ResolveAutomaticNavigationTarget(NavigationDirection direction) const;
    std::array<std::weak_ptr<View>, 4> navigationTargets_{};
    std::weak_ptr<View> defaultFocusView_{};
    std::weak_ptr<View> lastFocusedView_{};
    mutable bool visibleChildrenCacheDirty_ = true;
    mutable std::vector<std::shared_ptr<View>> visibleChildrenAscendingCache_{};
    mutable std::vector<std::shared_ptr<View>> visibleChildrenDescendingCache_{};
};

}
