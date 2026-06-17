#include <bkui/core/ApplicationHost.hpp>

#include <bkui/core/Logger.hpp>
#include <bkui/render/backend/deko3d/Deko3DDevice.hpp>
#include <bkui/render/backend/opengl/OpenGLDevice.hpp>

#include <chrono>
#include <cstdio>
#include <thread>

namespace bk
{

ApplicationHost::ApplicationHost() = default;

ApplicationHost::~ApplicationHost()
{
    Shutdown();
}

bool ApplicationHost::Initialize(const ApplicationHostDesc& desc, int argc, const char* const* argv)
{
    if (initialized_)
    {
        return false;
    }

    if (!InitializeApplication(desc, argc, argv))
    {
        return false;
    }

    if (!CreateWindowRuntime(desc))
    {
        Shutdown();
        return false;
    }

    initialized_ = true;
    return true;
}

void ApplicationHost::Shutdown()
{
    if (renderer_ != nullptr)
    {
        renderer_->Shutdown();
        renderer_.reset();
    }

    if (device_ != nullptr)
    {
        if (IsValid(commandBuffer_))
        {
            device_->DestroyCommandBuffer(commandBuffer_);
            commandBuffer_ = {};
        }
        device_->Shutdown();
        device_.reset();
    }

    if (platform_ != nullptr)
    {
        platform_->Shutdown();
        platform_.reset();
    }

    application_.Shutdown();
    initialized_ = false;
    windowSize_ = {};
    logicalSize_ = {};
    clearColor_ = Color{0.0F, 0.0F, 0.0F, 1.0F};
    swapchain_ = {};
}

std::uint64_t ApplicationHost::MainLoop(const MainLoopDesc& desc)
{
    if (!initialized_ || platform_ == nullptr || device_ == nullptr || renderer_ == nullptr)
    {
        return 0;
    }

    if (!desc.useFixedDelta)
    {
        std::uint64_t executed = 0;
        auto previous = std::chrono::steady_clock::now();
        while (platform_->IsRunning() && application_.IsRunning())
        {
            platform_->PollEvents();
            const InputState input = platform_->GetInput();
            if (input.quitRequested)
            {
                application_.RequestQuit();
            }
            if (!application_.IsRunning())
            {
                break;
            }

            const auto now = std::chrono::steady_clock::now();
            const float deltaSeconds = std::chrono::duration<float>(now - previous).count();
            previous = now;

            if (input.windowResized)
            {
                windowSize_ = ResolveWindowSize(platform_->GetWindowSize());
                device_->Resize(windowSize_);
                onResize_.Emit(*this, windowSize_);
            }

            application_.SetInputState(input);
            onFrameBegin_.Emit(*this, deltaSeconds, application_.GetFrameIndex());
            if (!application_.RunFrame(deltaSeconds, desc.clearRenderQueueEachFrame))
            {
                break;
            }
            onFrameEnd_.Emit(*this, deltaSeconds, application_.GetFrameIndex() - 1);

            if (!PresentFrame())
            {
                break;
            }

            ++executed;
            if (desc.maxFrames != 0 && executed >= desc.maxFrames)
            {
                break;
            }
        }
        return executed;
    }

    const auto frameDuration = std::chrono::duration<float>(desc.fixedDeltaSeconds);
    auto nextFrameTime = std::chrono::steady_clock::now();
    std::uint64_t executed = 0;

    while (platform_->IsRunning() && application_.IsRunning())
    {
        if (desc.synchronizeToFixedDelta)
        {
            std::this_thread::sleep_until(nextFrameTime);
            nextFrameTime += std::chrono::duration_cast<std::chrono::steady_clock::duration>(frameDuration);
        }

        platform_->PollEvents();
        const InputState input = platform_->GetInput();
        if (input.quitRequested)
        {
            application_.RequestQuit();
        }
        if (!application_.IsRunning())
        {
            break;
        }

        if (input.windowResized)
        {
            windowSize_ = ResolveWindowSize(platform_->GetWindowSize());
            device_->Resize(windowSize_);
            onResize_.Emit(*this, windowSize_);
        }

        application_.SetInputState(input);
        onFrameBegin_.Emit(*this, desc.fixedDeltaSeconds, application_.GetFrameIndex());
        if (!application_.RunFrame(desc.fixedDeltaSeconds, desc.clearRenderQueueEachFrame))
        {
            break;
        }
        onFrameEnd_.Emit(*this, desc.fixedDeltaSeconds, application_.GetFrameIndex() - 1);

        if (!PresentFrame())
        {
            break;
        }

        ++executed;
        if (desc.maxFrames != 0 && executed >= desc.maxFrames)
        {
            break;
        }
    }

    return executed;
}

Application& ApplicationHost::GetApplication()
{
    return application_;
}

const Application& ApplicationHost::GetApplication() const
{
    return application_;
}

Platform* ApplicationHost::GetPlatform()
{
    return platform_.get();
}

const Platform* ApplicationHost::GetPlatform() const
{
    return platform_.get();
}

Device* ApplicationHost::GetDevice()
{
    return device_.get();
}

const Device* ApplicationHost::GetDevice() const
{
    return device_.get();
}

Vector2 ApplicationHost::GetWindowSize() const
{
    return windowSize_;
}

void ApplicationHost::SetLogicalSize(Vector2 size)
{
    logicalSize_ = size;
}

Vector2 ApplicationHost::GetLogicalSize() const
{
    return ResolveLogicalSize();
}

void ApplicationHost::SetClearColor(const Color& color)
{
    clearColor_ = color;
}

const Color& ApplicationHost::GetClearColor() const
{
    return clearColor_;
}

ApplicationHost::FrameEvent& ApplicationHost::OnFrameBegin()
{
    return onFrameBegin_;
}

ApplicationHost::FrameEvent& ApplicationHost::OnFrameEnd()
{
    return onFrameEnd_;
}

ApplicationHost::ResizeEvent& ApplicationHost::OnResize()
{
    return onResize_;
}

bool ApplicationHost::InitializeApplication(const ApplicationHostDesc& desc, int argc, const char* const* argv)
{
    if (!application_.Initialize(desc.application, argc, argv))
    {
        Logger::instance().Error("Unable to initialize bkui application.");
        return false;
    }

    logicalSize_ = desc.logicalSize;
    clearColor_ = desc.clearColor;
    return true;
}

bool ApplicationHost::CreateWindowRuntime(const ApplicationHostDesc& desc)
{
    platform_ = CreateDefaultPlatform(desc.window);
    if (platform_ == nullptr || !platform_->Init())
    {
        Logger::instance().Error("Unable to initialize platform.");
        return false;
    }

    device_ =
#if defined(BKUI_PLATFORM_SWITCH)
        CreateDeko3DDevice(*platform_);
#else
        CreateOpenGLDevice(*platform_);
#endif
    if (device_ == nullptr || !device_->Init())
    {
        Logger::instance().Error("Unable to initialize device.");
        return false;
    }

    windowSize_ = ResolveWindowSize(platform_->GetWindowSize());
    swapchain_ = device_->GetMainSwapchain();
    commandBuffer_ = device_->CreateCommandBuffer();
    if (!IsValid(commandBuffer_))
    {
        Logger::instance().Error("Unable to create command buffer.");
        return false;
    }

    renderer_ = std::make_unique<RenderQueueRenderer>(*device_);
    if (!renderer_->Initialize())
    {
        Logger::instance().Error("Unable to initialize render queue renderer.");
        return false;
    }

    if (windowSize_.x > 0.0F && windowSize_.y > 0.0F)
    {
        device_->Resize(windowSize_);
        onResize_.Emit(*this, windowSize_);
    }

    return true;
}

bool ApplicationHost::PresentFrame()
{
    if (platform_ == nullptr || device_ == nullptr || renderer_ == nullptr)
    {
        return false;
    }

    device_->BeginFrame(swapchain_, RenderPassDesc{clearColor_});
    device_->BeginCommandBuffer(commandBuffer_);
    if (!renderer_->Render(commandBuffer_, application_.GetRenderQueue(), ResolveLogicalSize()))
    {
        Logger::instance().Error("Failed to record UI draw commands.");
        return false;
    }
    device_->EndCommandBuffer(commandBuffer_);
    device_->Submit(commandBuffer_);
    device_->EndFrame(swapchain_);
    return true;
}

Vector2 ApplicationHost::ResolveWindowSize(Vector2 size) const
{
    return Vector2{
        size.x > 0.0F ? size.x : 1280.0F,
        size.y > 0.0F ? size.y : 720.0F,
    };
}

Vector2 ApplicationHost::ResolveLogicalSize() const
{
    if (logicalSize_.x > 0.0F && logicalSize_.y > 0.0F)
    {
        return logicalSize_;
    }
    return windowSize_;
}

}
