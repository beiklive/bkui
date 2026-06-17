#pragma once

#include <bkui/core/Event.hpp>
#include <bkui/core/Logger.hpp>
#include <bkui/core/MetaData.hpp>
#include <bkui/core/Singleton.hpp>
#include <bkui/platform/Platform.hpp>
#include <bkui/render/RenderQueue.hpp>
#include <bkui/ui/View.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bk
{

/// 应用初始化描述信息。
struct ApplicationDesc
{
    std::string name = "BeikUI";
    std::string version = "0.1.0";
    std::string organization;
    std::string identifier;
    LoggerDesc logger{};
    WindowDesc window{};
    Vector2 logicalSize{};
    ColorRGBA clearColor{0.0F, 0.0F, 0.0F, 1.0F};
    bool autoResizeRootViews = true;
};

/// 应用主循环配置。
struct MainLoopDesc
{
    float fixedDeltaSeconds = 1.0F / 60.0F;
    std::uint64_t maxFrames = 0;
    bool useFixedDelta = true;
    bool synchronizeToFixedDelta = true;
    bool clearRenderQueueEachFrame = true;
};

/// 应用级上下文，负责管理基础状态、参数、元数据和生命周期事件。
class Application : public Singleton<Application>
{
public:
    using LifecycleEvent = Event<void(Application&)>;
    using DescriptorChangedEvent = Event<void(Application&, const ApplicationDesc&)>;
    using FrameEvent = Event<void(Application&, float, std::uint64_t)>;
    using ResizeEvent = Event<void(Application&, Vector2)>;

    Application();
    ~Application();

    /// 获取当前活动的应用实例，没有时返回空指针。
    [[nodiscard]] static Application* Active();

    /// 初始化应用上下文。
    bool Initialize(const ApplicationDesc& desc = {}, int argc = 0, const char* const* argv = nullptr);

    /// 关闭应用上下文并清理内部状态。
    void Shutdown();

    /// 请求退出应用。
    void RequestQuit();

    /// 清除退出请求并恢复运行状态。
    void CancelQuit();

    /// 查询应用是否已经初始化。
    [[nodiscard]] bool IsInitialized() const;

    /// 查询应用当前是否处于运行状态。
    [[nodiscard]] bool IsRunning() const;

    /// 查询应用当前是否已经收到退出请求。
    [[nodiscard]] bool QuitRequested() const;

    /// 向应用注册一个顶层页面视图。
    void AddView(std::shared_ptr<View> view);

    /// 移除指定顶层页面视图。
    void RemoveView(const std::shared_ptr<View>& view);

    /// 清空全部顶层页面视图。
    void ClearViews();

    /// 获取全部顶层页面视图。
    [[nodiscard]] const std::vector<std::shared_ptr<View>>& GetViews() const;

    /// 获取当前帧渲染命令队列。
    [[nodiscard]] RenderQueue& GetRenderQueue();

    /// 获取当前帧渲染命令队列。
    [[nodiscard]] const RenderQueue& GetRenderQueue() const;

    /// 获取当前已经执行完成的帧编号。
    [[nodiscard]] std::uint64_t GetFrameIndex() const;

    /// 设置当前帧输入状态，用于焦点导航和基础交互分发。
    void SetInputState(const InputState& input);

    /// 获取当前缓存的输入状态。
    [[nodiscard]] const InputState& GetInputState() const;

    /// 设置焦点视图。
    bool SetFocusedView(const std::shared_ptr<View>& view);

    /// 获取当前焦点视图。
    [[nodiscard]] std::shared_ptr<View> GetFocusedView() const;

    /// 清除当前焦点。
    void ClearFocus();

    /// 如果当前焦点等于指定视图则清除。
    void ClearFocus(const std::shared_ptr<View>& view);

    /// 按方向移动焦点。
    bool MoveFocus(NavigationDirection direction);

    /// 执行单帧更新与绘制。
    bool RunFrame(float deltaSeconds = 0.0F, bool clearRenderQueue = true);

    /// 运行主循环，直到收到退出请求或达到帧数上限。
    std::uint64_t RunMainLoop(const MainLoopDesc& desc = {});

    /// 更新应用描述信息。
    void SetDescriptor(const ApplicationDesc& desc);

    /// 获取当前应用描述信息。
    [[nodiscard]] const ApplicationDesc& GetDescriptor() const;

    /// 设置应用名称。
    void SetName(std::string name);

    /// 获取应用名称。
    [[nodiscard]] const std::string& GetName() const;

    /// 设置应用版本。
    void SetVersion(std::string version);

    /// 获取应用版本。
    [[nodiscard]] const std::string& GetVersion() const;

    /// 设置应用组织名。
    void SetOrganization(std::string organization);

    /// 获取应用组织名。
    [[nodiscard]] const std::string& GetOrganization() const;

    /// 设置应用标识符。
    void SetIdentifier(std::string identifier);

    /// 获取应用标识符。
    [[nodiscard]] const std::string& GetIdentifier() const;

    /// 获取可写元数据容器。
    [[nodiscard]] MetaData& GetMetaData();

    /// 获取只读元数据容器。
    [[nodiscard]] const MetaData& GetMetaData() const;

    /// 获取命令行参数列表。
    [[nodiscard]] const std::vector<std::string>& GetArguments() const;

    /// 获取命令行参数数量。
    [[nodiscard]] std::size_t GetArgumentCount() const;

    /// 获取指定索引的命令行参数，越界时返回空视图。
    [[nodiscard]] std::string_view GetArgument(std::size_t index) const;

    /// 获取可执行文件路径，通常等于第一个命令行参数。
    [[nodiscard]] std::string_view GetExecutablePath() const;

    /// 获取初始化事件。
    [[nodiscard]] LifecycleEvent& OnInitialize();

    /// 获取退出请求事件。
    [[nodiscard]] LifecycleEvent& OnQuitRequested();

    /// 获取关闭事件。
    [[nodiscard]] LifecycleEvent& OnShutdown();

    /// 获取应用描述变更事件。
    [[nodiscard]] DescriptorChangedEvent& OnDescriptorChanged();

    /// 获取帧开始事件。
    [[nodiscard]] FrameEvent& OnFrameBegin();

    /// 获取帧结束事件。
    [[nodiscard]] FrameEvent& OnFrameEnd();

    /// 设置当前窗口尺寸，并按需自动调整顶层 View。
    void SetWindowSize(Vector2 size);

    /// 获取当前窗口尺寸。
    [[nodiscard]] Vector2 GetWindowSize() const;

    /// 设置 RenderQueue 输出时使用的逻辑尺寸。
    void SetLogicalSize(Vector2 size);

    /// 获取 RenderQueue 输出时使用的逻辑尺寸。
    [[nodiscard]] Vector2 GetLogicalSize() const;

    /// 设置是否在窗口/逻辑尺寸变化时自动铺满顶层 View。
    void SetAutoResizeRootViews(bool enabled);

    /// 查询是否自动铺满顶层 View。
    [[nodiscard]] bool IsAutoResizeRootViewsEnabled() const;

    /// 获取窗口尺寸变化事件。大多数页面无需订阅，Application 会自动处理根视图尺寸。
    [[nodiscard]] ResizeEvent& OnResize();

    /// 设置聚焦框圆角半径，0 表示直角。已弃用：请在 View 上设置聚焦框样式。
    void SetFocusHighlightCornerRadius(float radius);

    /// 获取默认聚焦框圆角半径。已弃用：请在 View 上获取聚焦框样式。
    [[nodiscard]] float GetFocusHighlightCornerRadius() const;

    /// 设置是否启用聚焦框移动动画。
    void SetFocusHighlightMotionEnabled(bool enabled);

    /// 查询聚焦框移动动画是否启用。
    [[nodiscard]] bool IsFocusHighlightMotionEnabled() const;

    /// 设置默认聚焦框高亮主色 1。已弃用：请在 View 上设置聚焦框样式。
    void SetFocusHighlightColor1(const ColorRGBA& color);

    /// 获取默认聚焦框高亮主色 1。已弃用：请在 View 上获取聚焦框样式。
    [[nodiscard]] const ColorRGBA& GetFocusHighlightColor1() const;

    /// 设置默认聚焦框高亮主色 2。已弃用：请在 View 上设置聚焦框样式。
    void SetFocusHighlightColor2(const ColorRGBA& color);

    /// 获取默认聚焦框高亮主色 2。已弃用：请在 View 上获取聚焦框样式。
    [[nodiscard]] const ColorRGBA& GetFocusHighlightColor2() const;

    /// 设置是否保留非活动顶层 View 的聚焦框。
    void SetPreserveInactiveFocusHighlights(bool enabled);

    /// 查询是否保留非活动顶层 View 的聚焦框。
    [[nodiscard]] bool IsPreservingInactiveFocusHighlights() const;

private:
    struct FocusHighlightState
    {
        std::weak_ptr<View> view{};
        std::weak_ptr<View> root{};
        Rect currentRect{};
        Rect targetRect{};
        float opacity = 0.0F;
        float targetOpacity = 0.0F;
        float pulseTime = 0.0F;
        FocusHighlightStyle style{};
        bool initialized = false;
    };

    void ProcessInput();
    void ApplyRootViewFrames();
    void UpdateFocusHighlight(float deltaSeconds);
    void UpdateFocusHighlightState(FocusHighlightState& state, const std::shared_ptr<View>& trackedView, float deltaSeconds, float targetOpacity);
    void DrawFocusHighlight(RenderQueue& queue) const;
    void DrawFocusHighlightsForRoot(RenderQueue& queue, const std::shared_ptr<View>& root) const;
    void DrawFocusHighlightState(RenderQueue& queue, const FocusHighlightState& state) const;
    void RemoveInactiveFocusHighlight(const std::shared_ptr<View>& root);
    void CaptureInactiveFocusHighlight(const std::shared_ptr<View>& root, const std::shared_ptr<View>& view);
    void InvalidateViewOrderCache();
    [[nodiscard]] const std::vector<std::shared_ptr<View>>& OrderedViews(bool descending) const;
    [[nodiscard]] std::shared_ptr<View> FindTopmostViewAt(const Vector2& point) const;
    [[nodiscard]] std::shared_ptr<View> FindFirstFocusableView() const;
    [[nodiscard]] static bool IsDirectionalKey(const InputState::KeyEvent& key, NavigationDirection direction);
    [[nodiscard]] static bool IsActivationKey(const InputState& input);

    void ResetState();
    void StoreDescriptorMetaData();
    void CaptureArguments(int argc, const char* const* argv);

    ApplicationDesc descriptor_{};
    std::vector<std::string> arguments_{};
    std::vector<std::shared_ptr<View>> views_{};
    mutable bool viewOrderCacheDirty_ = true;
    mutable std::vector<std::shared_ptr<View>> orderedViewsAscendingCache_{};
    mutable std::vector<std::shared_ptr<View>> orderedViewsDescendingCache_{};
    RenderQueue renderQueue_{};
    MetaData metaData_{};
    InputState inputState_{};
    std::weak_ptr<View> focusedView_{};
    std::weak_ptr<View> pressedView_{};
    FocusHighlightState focusHighlight_{};
    std::vector<FocusHighlightState> inactiveFocusHighlights_{};
    FocusHighlightStyle defaultFocusHighlightStyle_{};
    bool focusHighlightMotionEnabled_ = true;
    bool preserveInactiveFocusHighlights_ = false;
    Vector2 windowSize_{1280.0F, 720.0F};
    Vector2 logicalSize_{};
    bool autoResizeRootViews_ = true;
    std::uint64_t frameIndex_ = 0;
    bool initialized_ = false;
    bool running_ = false;
    bool quitRequested_ = false;
    static Application* activeApplication_;
    LifecycleEvent onInitialize_{};
    LifecycleEvent onQuitRequested_{};
    LifecycleEvent onShutdown_{};
    DescriptorChangedEvent onDescriptorChanged_{};
    FrameEvent onFrameBegin_{};
    FrameEvent onFrameEnd_{};
    ResizeEvent onResize_{};
};

}
