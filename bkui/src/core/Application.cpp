#include <bkui/core/Application.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <utility>

namespace bk
{

Application::Application() = default;

Application::~Application()
{
    Shutdown();
}

bool Application::Initialize(const ApplicationDesc& desc, int argc, const char* const* argv)
{
    if (initialized_)
    {
        return false;
    }

    descriptor_ = desc;
    CaptureArguments(argc, argv);
    metaData_.Clear();
    StoreDescriptorMetaData();
    frameIndex_ = 0;
    renderQueue_.Clear();

    if (!Logger::instance().Initialize(descriptor_.logger))
    {
        return false;
    }

    initialized_ = true;
    running_ = true;
    quitRequested_ = false;
    Logger::instance().Info("Application initialized.");
    onInitialize_.Emit(*this);
    return true;
}

void Application::Shutdown()
{
    if (!initialized_)
    {
        return;
    }

    running_ = false;
    Logger::instance().Info("Application shutting down.");
    onShutdown_.Emit(*this);
    ResetState();
    Logger::instance().Shutdown();
}

void Application::RequestQuit()
{
    if (!initialized_ || quitRequested_)
    {
        return;
    }

    quitRequested_ = true;
    running_ = false;
    onQuitRequested_.Emit(*this);
}

void Application::CancelQuit()
{
    if (!initialized_)
    {
        return;
    }

    quitRequested_ = false;
    running_ = true;
}

bool Application::IsInitialized() const
{
    return initialized_;
}

bool Application::IsRunning() const
{
    return initialized_ && running_ && !quitRequested_;
}

bool Application::QuitRequested() const
{
    return quitRequested_;
}

void Application::AddView(std::shared_ptr<View> view)
{
    if (view)
    {
        views_.push_back(std::move(view));
    }
}

void Application::RemoveView(const std::shared_ptr<View>& view)
{
    const auto it = std::remove(views_.begin(), views_.end(), view);
    if (it != views_.end())
    {
        views_.erase(it, views_.end());
    }
}

void Application::ClearViews()
{
    views_.clear();
}

const std::vector<std::shared_ptr<View>>& Application::GetViews() const
{
    return views_;
}

RenderQueue& Application::GetRenderQueue()
{
    return renderQueue_;
}

const RenderQueue& Application::GetRenderQueue() const
{
    return renderQueue_;
}

std::uint64_t Application::GetFrameIndex() const
{
    return frameIndex_;
}

bool Application::RunFrame(float deltaSeconds, bool clearRenderQueue)
{
    if (!IsRunning())
    {
        return false;
    }

    if (clearRenderQueue)
    {
        renderQueue_.Clear();
    }

    onFrameBegin_.Emit(*this, deltaSeconds, frameIndex_);

    for (const auto& view : views_)
    {
        if (!view || !view->IsVisible())
        {
            continue;
        }

        view->Frame(deltaSeconds);
        view->Draw(renderQueue_);
    }

    metaData_.Set("application.frame_index", frameIndex_);
    metaData_.Set("application.delta_seconds", deltaSeconds);
    onFrameEnd_.Emit(*this, deltaSeconds, frameIndex_);
    ++frameIndex_;
    return true;
}

std::uint64_t Application::RunMainLoop(const MainLoopDesc& desc)
{
    if (!IsRunning())
    {
        return 0;
    }

    auto previousTime = std::chrono::steady_clock::now();
    std::uint64_t executedFrames = 0;

    while (IsRunning())
    {
        float deltaSeconds = desc.fixedDeltaSeconds;
        if (!desc.useFixedDelta)
        {
            const auto currentTime = std::chrono::steady_clock::now();
            deltaSeconds = std::chrono::duration<float>(currentTime - previousTime).count();
            previousTime = currentTime;
        }

        if (!RunFrame(deltaSeconds, desc.clearRenderQueueEachFrame))
        {
            break;
        }

        ++executedFrames;
        if (desc.maxFrames != 0 && executedFrames >= desc.maxFrames)
        {
            break;
        }
    }

    return executedFrames;
}

void Application::SetDescriptor(const ApplicationDesc& desc)
{
    descriptor_ = desc;
    StoreDescriptorMetaData();
    onDescriptorChanged_.Emit(*this, descriptor_);
}

const ApplicationDesc& Application::GetDescriptor() const
{
    return descriptor_;
}

void Application::SetName(std::string name)
{
    descriptor_.name = std::move(name);
    StoreDescriptorMetaData();
    onDescriptorChanged_.Emit(*this, descriptor_);
}

const std::string& Application::GetName() const
{
    return descriptor_.name;
}

void Application::SetVersion(std::string version)
{
    descriptor_.version = std::move(version);
    StoreDescriptorMetaData();
    onDescriptorChanged_.Emit(*this, descriptor_);
}

const std::string& Application::GetVersion() const
{
    return descriptor_.version;
}

void Application::SetOrganization(std::string organization)
{
    descriptor_.organization = std::move(organization);
    StoreDescriptorMetaData();
    onDescriptorChanged_.Emit(*this, descriptor_);
}

const std::string& Application::GetOrganization() const
{
    return descriptor_.organization;
}

void Application::SetIdentifier(std::string identifier)
{
    descriptor_.identifier = std::move(identifier);
    StoreDescriptorMetaData();
    onDescriptorChanged_.Emit(*this, descriptor_);
}

const std::string& Application::GetIdentifier() const
{
    return descriptor_.identifier;
}

MetaData& Application::GetMetaData()
{
    return metaData_;
}

const MetaData& Application::GetMetaData() const
{
    return metaData_;
}

const std::vector<std::string>& Application::GetArguments() const
{
    return arguments_;
}

std::size_t Application::GetArgumentCount() const
{
    return arguments_.size();
}

std::string_view Application::GetArgument(std::size_t index) const
{
    return index < arguments_.size() ? std::string_view(arguments_[index]) : std::string_view{};
}

std::string_view Application::GetExecutablePath() const
{
    return arguments_.empty() ? std::string_view{} : std::string_view(arguments_.front());
}

Application::LifecycleEvent& Application::OnInitialize()
{
    return onInitialize_;
}

Application::LifecycleEvent& Application::OnQuitRequested()
{
    return onQuitRequested_;
}

Application::LifecycleEvent& Application::OnShutdown()
{
    return onShutdown_;
}

Application::DescriptorChangedEvent& Application::OnDescriptorChanged()
{
    return onDescriptorChanged_;
}

Application::FrameEvent& Application::OnFrameBegin()
{
    return onFrameBegin_;
}

Application::FrameEvent& Application::OnFrameEnd()
{
    return onFrameEnd_;
}

void Application::ResetState()
{
    descriptor_ = ApplicationDesc{};
    arguments_.clear();
    views_.clear();
    renderQueue_.Clear();
    metaData_.Clear();
    frameIndex_ = 0;
    initialized_ = false;
    running_ = false;
    quitRequested_ = false;
}

void Application::StoreDescriptorMetaData()
{
    metaData_.Set("application.name", descriptor_.name);
    metaData_.Set("application.version", descriptor_.version);
    metaData_.Set("application.organization", descriptor_.organization);
    metaData_.Set("application.identifier", descriptor_.identifier);
    metaData_.Set("application.argument_count", static_cast<std::uint32_t>(arguments_.size()));
    metaData_.Set("application.executable", std::string(GetExecutablePath()));
}

void Application::CaptureArguments(int argc, const char* const* argv)
{
    arguments_.clear();
    if (argc <= 0 || argv == nullptr)
    {
        return;
    }

    arguments_.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i)
    {
        arguments_.emplace_back(argv[i] != nullptr ? argv[i] : "");
    }
}

}
