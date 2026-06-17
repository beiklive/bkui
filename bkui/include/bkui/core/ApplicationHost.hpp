#pragma once

#include <bkui/core/Application.hpp>
#include <bkui/core/Event.hpp>
#include <bkui/platform/Platform.hpp>
#include <bkui/render/Device.hpp>
#include <bkui/render/RenderQueueRenderer.hpp>

#include <cstdint>
#include <memory>

namespace bk
{

/// 应用窗口与图形运行时配置。
struct ApplicationHostDesc
{
    ApplicationDesc application{};
    WindowDesc window{};
    Vector2 logicalSize{};
    Color clearColor{0.0F, 0.0F, 0.0F, 1.0F};
};

/// 托管 Application、Platform、Device 和 RenderQueueRenderer 的默认运行时。
class ApplicationHost
{
public:
    using FrameEvent = Event<void(ApplicationHost&, float, std::uint64_t)>;
    using ResizeEvent = Event<void(ApplicationHost&, Vector2)>;

    ApplicationHost();
    ~ApplicationHost();

    ApplicationHost(const ApplicationHost&) = delete;
    ApplicationHost& operator=(const ApplicationHost&) = delete;

    /// 初始化应用上下文、平台窗口与默认图形后端。
    [[nodiscard]] bool Initialize(const ApplicationHostDesc& desc = {}, int argc = 0, const char* const* argv = nullptr);
    [[nodiscard]] bool Initialize(const ApplicationDesc& desc, int argc = 0, const char* const* argv = nullptr);
    [[nodiscard]] bool Initialize(int argc, char** argv);
    [[nodiscard]] bool Initialize(int argc, const char* const* argv);

    /// 关闭图形运行时、平台窗口和应用上下文。
    void Shutdown();

    /// 运行完整窗口主循环，直到平台或应用请求退出。
    [[nodiscard]] std::uint64_t MainLoop(const MainLoopDesc& desc = {});

    /// 获取托管的应用对象。
    [[nodiscard]] Application& GetApplication();
    [[nodiscard]] const Application& GetApplication() const;

    /// 获取平台对象，窗口尚未初始化时返回空指针。
    [[nodiscard]] Platform* GetPlatform();
    [[nodiscard]] const Platform* GetPlatform() const;

    /// 获取图形设备对象，窗口尚未初始化时返回空指针。
    [[nodiscard]] Device* GetDevice();
    [[nodiscard]] const Device* GetDevice() const;

    /// 获取当前窗口尺寸。
    [[nodiscard]] Vector2 GetWindowSize() const;

    /// 设置 RenderQueue 输出时使用的逻辑尺寸。
    void SetLogicalSize(Vector2 size);

    /// 获取 RenderQueue 输出时使用的逻辑尺寸。
    [[nodiscard]] Vector2 GetLogicalSize() const;

    /// 设置主渲染通道清屏色。
    void SetClearColor(const Color& color);

    /// 获取主渲染通道清屏色。
    [[nodiscard]] const Color& GetClearColor() const;

    /// 获取每帧应用更新前事件。
    [[nodiscard]] FrameEvent& OnFrameBegin();

    /// 获取每帧应用更新后、提交渲染前事件。
    [[nodiscard]] FrameEvent& OnFrameEnd();

    /// 获取窗口尺寸变化事件。
    [[nodiscard]] ResizeEvent& OnResize();

private:
    [[nodiscard]] bool InitializeApplication(const ApplicationHostDesc& desc, int argc, const char* const* argv);
    [[nodiscard]] bool CreateWindowRuntime(const ApplicationHostDesc& desc);
    [[nodiscard]] bool PresentFrame();
    [[nodiscard]] ApplicationDesc ResolveApplicationDesc(const ApplicationHostDesc& desc) const;
    [[nodiscard]] Vector2 ResolveWindowSize(Vector2 size) const;
    [[nodiscard]] Vector2 ResolveLogicalSize() const;

    Application application_{};
    std::unique_ptr<Platform> platform_{};
    std::unique_ptr<Device> device_{};
    std::unique_ptr<RenderQueueRenderer> renderer_{};
    CommandBufferHandle commandBuffer_{};
    SwapchainHandle swapchain_{};
    Vector2 windowSize_{};
    Vector2 logicalSize_{};
    Color clearColor_{0.0F, 0.0F, 0.0F, 1.0F};
    bool initialized_ = false;
    FrameEvent onFrameBegin_{};
    FrameEvent onFrameEnd_{};
    ResizeEvent onResize_{};
};

}
