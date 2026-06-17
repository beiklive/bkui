#include <bkui/core/Application.hpp>
#include <bkui/core/FileSystem.hpp>
#include <bkui/core/I18n.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <limits>
#include <utility>

namespace bk
{

Application* Application::activeApplication_ = nullptr;

namespace
{
constexpr float kNavigationEpsilon = 0.001F;
constexpr float kFocusHighlightLerpSpeed = 14.0F;
constexpr float kFocusHighlightOpacitySpeed = 10.0F;
constexpr float kFocusHighlightSegmentLength = 8.0F;
constexpr float kFocusHighlightTravelSpeed = 0.95F;
constexpr float kFocusHighlightPulseSpeed = 5.7F;
constexpr float kFocusHighlightGlowSpanPixels = 150.0F;
constexpr float kFocusHighlightShadowLayers = 3.0F;

std::vector<std::shared_ptr<View>> VisibleChildrenSorted(const std::shared_ptr<View>& root)
{
    std::vector<std::shared_ptr<View>> ordered;
    if (!root)
    {
        return ordered;
    }

    ordered.reserve(root->Children().size());
    for (const auto& child : root->Children())
    {
        if (child && child->IsVisible())
        {
            ordered.push_back(child);
        }
    }

    std::stable_sort(
        ordered.begin(),
        ordered.end(),
        [](const std::shared_ptr<View>& lhs, const std::shared_ptr<View>& rhs) {
            return lhs->GetZIndex() < rhs->GetZIndex();
        });
    return ordered;
}

bool NearlyEqual(float lhs, float rhs)
{
    return std::abs(lhs - rhs) <= kNavigationEpsilon;
}

float Approach(float current, float target, float deltaSeconds, float speed)
{
    if (deltaSeconds <= 0.0F)
    {
        return target;
    }

    const float factor = 1.0F - std::exp(-speed * deltaSeconds);
    return current + (target - current) * factor;
}

Color HsvToRgb(float hue, float saturation, float value, float alpha)
{
    const float wrappedHue = hue - std::floor(hue);
    const float scaledHue = wrappedHue * 6.0F;
    const float chroma = value * saturation;
    const float x = chroma * (1.0F - std::fabs(std::fmod(scaledHue, 2.0F) - 1.0F));
    const float match = value - chroma;

    float red = 0.0F;
    float green = 0.0F;
    float blue = 0.0F;

    if (scaledHue < 1.0F)
    {
        red = chroma;
        green = x;
    }
    else if (scaledHue < 2.0F)
    {
        red = x;
        green = chroma;
    }
    else if (scaledHue < 3.0F)
    {
        green = chroma;
        blue = x;
    }
    else if (scaledHue < 4.0F)
    {
        green = x;
        blue = chroma;
    }
    else if (scaledHue < 5.0F)
    {
        red = x;
        blue = chroma;
    }
    else
    {
        red = chroma;
        blue = x;
    }

    return Color{
        red + match,
        green + match,
        blue + match,
        alpha,
    };
}

Color LerpColor(const Color& from, const Color& to, float t)
{
    const float clamped = std::clamp(t, 0.0F, 1.0F);
    return Color{
        from.r + (to.r - from.r) * clamped,
        from.g + (to.g - from.g) * clamped,
        from.b + (to.b - from.b) * clamped,
        from.a + (to.a - from.a) * clamped,
    };
}

float WrapDistance(float a, float b, float length)
{
    const float direct = std::fabs(a - b);
    return std::min(direct, std::max(0.0F, length - direct));
}

Vector2 RectCenter(const Rect& rect)
{
    return Vector2{
        rect.x + rect.width * 0.5F,
        rect.y + rect.height * 0.5F,
    };
}

bool IsInSubtree(const std::shared_ptr<View>& root, const std::shared_ptr<View>& view)
{
    if (!root || !view)
    {
        return false;
    }

    if (root == view)
    {
        return true;
    }

    const View* current = view->GetParent();
    while (current != nullptr)
    {
        if (current == root.get())
        {
            return true;
        }
        current = current->GetParent();
    }

    return false;
}

std::shared_ptr<View> TopLevelAncestor(const std::shared_ptr<View>& view)
{
    if (!view)
    {
        return nullptr;
    }

    std::shared_ptr<View> current = view;
    while (current && current->GetParent() != nullptr)
    {
        current = std::const_pointer_cast<View>(current->GetParent()->shared_from_this());
    }

    return current;
}

std::shared_ptr<View> FindTopmostVisibleRoot(const std::vector<std::shared_ptr<View>>& views)
{
    std::shared_ptr<View> topmost;
    for (const auto& view : views)
    {
        if (!view || !view->IsVisible())
        {
            continue;
        }

        if (!topmost || view->GetZIndex() >= topmost->GetZIndex())
        {
            topmost = view;
        }
    }

    return topmost;
}

std::shared_ptr<View> FindBestNavigationTargetInScope(
    const std::shared_ptr<View>& root,
    const std::shared_ptr<View>& source,
    NavigationDirection direction)
{
    if (!root || !source)
    {
        return nullptr;
    }

    std::vector<std::shared_ptr<View>> stack{root};
    std::vector<std::shared_ptr<View>> focusables;
    while (!stack.empty())
    {
        std::shared_ptr<View> current = stack.back();
        stack.pop_back();
        if (!current || !current->IsVisible())
        {
            continue;
        }

        if (current != source && current->IsFocusable())
        {
            focusables.push_back(current);
        }

        std::vector<std::shared_ptr<View>> children = VisibleChildrenSorted(current);
        for (auto it = children.rbegin(); it != children.rend(); ++it)
        {
            stack.push_back(*it);
        }
    }

    const Vector2 sourceCenter = RectCenter(source->GetFrame());
    std::shared_ptr<View> bestCandidate;
    float bestPrimaryDistance = std::numeric_limits<float>::max();
    float bestSecondaryDistance = std::numeric_limits<float>::max();
    int bestZIndex = std::numeric_limits<int>::min();

    for (const auto& candidate : focusables)
    {
        const Vector2 targetCenter = RectCenter(candidate->GetFrame());
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

        const bool betterPrimary = primaryDistance < bestPrimaryDistance - kNavigationEpsilon;
        const bool samePrimary = NearlyEqual(primaryDistance, bestPrimaryDistance);
        const bool betterSecondary = secondaryDistance < bestSecondaryDistance - kNavigationEpsilon;
        const bool sameSecondary = NearlyEqual(secondaryDistance, bestSecondaryDistance);
        const bool betterZ = candidate->GetZIndex() > bestZIndex;

        if (betterPrimary ||
            (samePrimary && betterSecondary) ||
            (samePrimary && sameSecondary && betterZ))
        {
            bestCandidate = candidate;
            bestPrimaryDistance = primaryDistance;
            bestSecondaryDistance = secondaryDistance;
            bestZIndex = candidate->GetZIndex();
        }
    }

    return bestCandidate;
}

std::shared_ptr<View> ResolvePreferredFocusCandidate(const std::shared_ptr<View>& root)
{
    if (!root || !root->IsVisible())
    {
        return nullptr;
    }

    if (std::shared_ptr<View> preferred = root->GetDefaultFocusView())
    {
        if (preferred->IsVisible())
        {
            if (preferred->IsFocusable())
            {
                return preferred;
            }

            if (std::shared_ptr<View> nested = ResolvePreferredFocusCandidate(preferred))
            {
                return nested;
            }
        }
    }

    if (root->IsFocusable())
    {
        return root;
    }

    for (const auto& child : VisibleChildrenSorted(root))
    {
        if (std::shared_ptr<View> nested = ResolvePreferredFocusCandidate(child))
        {
            return nested;
        }
    }

    return nullptr;
}
}

Application::Application()
{
    activeApplication_ = this;
}

Application::~Application()
{
    Shutdown();
    if (activeApplication_ == this)
    {
        activeApplication_ = nullptr;
    }
}

Application* Application::Active()
{
    return activeApplication_;
}

bool Application::Initialize(const ApplicationDesc& desc, int argc, const char* const* argv)
{
    if (initialized_)
    {
        return false;
    }

    descriptor_ = desc;
    logicalSize_ = desc.logicalSize;
    windowSize_ = Vector2{
        desc.window.width > 0 ? static_cast<float>(desc.window.width) : 1280.0F,
        desc.window.height > 0 ? static_cast<float>(desc.window.height) : 720.0F,
    };
    autoResizeRootViews_ = desc.autoResizeRootViews;
    CaptureArguments(argc, argv);
    metaData_.Clear();
    StoreDescriptorMetaData();
    frameIndex_ = 0;
    renderQueue_.Clear();

    // 初始化文件挂载系统
    {
        bk::FileSystem::Init(argc > 0 ? argv[0] : nullptr);
        bk::FileSystem::Mount("resources");
    }
    // 初始化国际化系统，自动获取系统语言并加载对应翻译文件
    {
        bk::I18n::Instance().Init("i18n");
    }

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
        ApplyRootViewFrames();
        InvalidateViewOrderCache();
    }
}

void Application::RemoveView(const std::shared_ptr<View>& view)
{
    if (view)
    {
        RemoveInactiveFocusHighlight(view);
        if (std::shared_ptr<View> focused = focusedView_.lock())
        {
            if (focused == view || IsInSubtree(view, focused))
            {
                ClearFocus();
            }
        }
    }

    const auto it = std::remove(views_.begin(), views_.end(), view);
    if (it != views_.end())
    {
        views_.erase(it, views_.end());
        ApplyRootViewFrames();
        InvalidateViewOrderCache();
    }
}

void Application::ClearViews()
{
    views_.clear();
    inactiveFocusHighlights_.clear();
    InvalidateViewOrderCache();
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

void Application::SetInputState(const InputState& input)
{
    inputState_ = input;
    if (input.windowResized && input.windowSize.x > 0.0F && input.windowSize.y > 0.0F)
    {
        SetWindowSize(input.windowSize);
    }
}

const InputState& Application::GetInputState() const
{
    return inputState_;
}

bool Application::SetFocusedView(const std::shared_ptr<View>& view)
{
    if (view && (!view->IsVisible() || !view->IsFocusable()))
    {
        return false;
    }

    std::shared_ptr<View> current = focusedView_.lock();
    if (current == view)
    {
        return current != nullptr;
    }

    const std::shared_ptr<View> currentRoot = TopLevelAncestor(current);
    const std::shared_ptr<View> nextRoot = TopLevelAncestor(view);

    if (current)
    {
        if (preserveInactiveFocusHighlights_ && view && currentRoot && currentRoot != nextRoot)
        {
            CaptureInactiveFocusHighlight(currentRoot, current);
        }
        else if (currentRoot)
        {
            RemoveInactiveFocusHighlight(currentRoot);
        }
        current->SetFocusedState(false);
    }

    focusedView_.reset();
    if (view)
    {
        if (nextRoot)
        {
            RemoveInactiveFocusHighlight(nextRoot);
        }
        view->SetFocusedState(true);
        focusedView_ = view;
        focusHighlight_.view = view;
        focusHighlight_.root = nextRoot;

        std::shared_ptr<View> cursor = view;
        while (cursor)
        {
            cursor->RememberFocusedDescendant(view);

            const View* parent = cursor->GetParent();
            cursor = parent != nullptr ? std::const_pointer_cast<View>(parent->shared_from_this()) : nullptr;
        }
    }

    return view != nullptr;
}

std::shared_ptr<View> Application::GetFocusedView() const
{
    return focusedView_.lock();
}

void Application::ClearFocus()
{
    SetFocusedView(nullptr);
}

void Application::ClearFocus(const std::shared_ptr<View>& view)
{
    if (focusedView_.lock() == view)
    {
        SetFocusedView(nullptr);
    }
}

bool Application::MoveFocus(NavigationDirection direction)
{
    const std::shared_ptr<View> activeRoot = FindTopmostVisibleRoot(views_);
    std::shared_ptr<View> focused = focusedView_.lock();

    if (focused && activeRoot && !IsInSubtree(activeRoot, focused))
    {
        focused.reset();
    }

    if (focused)
    {
        if (std::shared_ptr<View> target = focused->FindNavigationTarget(direction))
        {
            if (!activeRoot || IsInSubtree(activeRoot, target))
            {
                return SetFocusedView(target);
            }
        }

        if (std::shared_ptr<View> scopedTarget = FindBestNavigationTargetInScope(
                activeRoot ? activeRoot : TopLevelAncestor(focused),
                focused,
                direction))
        {
            return SetFocusedView(scopedTarget);
        }

        return false;
    }

    if (activeRoot)
    {
        if (std::shared_ptr<View> preferred = ResolvePreferredFocusCandidate(activeRoot))
        {
            return SetFocusedView(preferred);
        }
    }

    return SetFocusedView(FindFirstFocusableView());
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
    ProcessInput();
    UpdateFocusHighlight(deltaSeconds);

    for (const auto& view : OrderedViews(false))
    {
        if (!view || !view->IsVisible())
        {
            continue;
        }

        view->Frame(deltaSeconds);
        view->Paint(renderQueue_);
        DrawFocusHighlightsForRoot(renderQueue_, view);
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
    logicalSize_ = desc.logicalSize;
    autoResizeRootViews_ = desc.autoResizeRootViews;
    StoreDescriptorMetaData();
    ApplyRootViewFrames();
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

void Application::SetFocusHighlightCornerRadius(float radius)
{
    defaultFocusHighlightStyle_.cornerRadius = std::max(0.0F, radius);
}

float Application::GetFocusHighlightCornerRadius() const
{
    return defaultFocusHighlightStyle_.cornerRadius;
}

void Application::SetFocusHighlightMotionEnabled(bool enabled)
{
    focusHighlightMotionEnabled_ = enabled;
}

bool Application::IsFocusHighlightMotionEnabled() const
{
    return focusHighlightMotionEnabled_;
}

void Application::SetPreserveInactiveFocusHighlights(bool enabled)
{
    preserveInactiveFocusHighlights_ = enabled;
    if (!preserveInactiveFocusHighlights_)
    {
        inactiveFocusHighlights_.clear();
    }
}

bool Application::IsPreservingInactiveFocusHighlights() const
{
    return preserveInactiveFocusHighlights_;
}

void Application::SetFocusHighlightColor1(const Color& color)
{
    defaultFocusHighlightStyle_.color1 = color;
}

const Color& Application::GetFocusHighlightColor1() const
{
    return defaultFocusHighlightStyle_.color1;
}

void Application::SetFocusHighlightColor2(const Color& color)
{
    defaultFocusHighlightStyle_.color2 = color;
}

const Color& Application::GetFocusHighlightColor2() const
{
    return defaultFocusHighlightStyle_.color2;
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

void Application::SetWindowSize(Vector2 size)
{
    const Vector2 resolved{
        size.x > 0.0F ? size.x : 1280.0F,
        size.y > 0.0F ? size.y : 720.0F,
    };
    if (NearlyEqual(windowSize_.x, resolved.x) && NearlyEqual(windowSize_.y, resolved.y))
    {
        return;
    }

    windowSize_ = resolved;
    ApplyRootViewFrames();
    onResize_.Emit(*this, windowSize_);
}

Vector2 Application::GetWindowSize() const
{
    return windowSize_;
}

void Application::SetLogicalSize(Vector2 size)
{
    logicalSize_ = size;
    ApplyRootViewFrames();
}

Vector2 Application::GetLogicalSize() const
{
    if (logicalSize_.x > 0.0F && logicalSize_.y > 0.0F)
    {
        return logicalSize_;
    }
    return windowSize_;
}

void Application::SetAutoResizeRootViews(bool enabled)
{
    autoResizeRootViews_ = enabled;
    ApplyRootViewFrames();
}

bool Application::IsAutoResizeRootViewsEnabled() const
{
    return autoResizeRootViews_;
}

Application::ResizeEvent& Application::OnResize()
{
    return onResize_;
}

void Application::ProcessInput()
{
    const bool touchDown = inputState_.touchCount > 0 && inputState_.touchPoints[0].down;
    const bool touchPressed = inputState_.touchCount > 0 && inputState_.touchPoints[0].pressed;
    const bool touchReleased = inputState_.touchCount > 0 && inputState_.touchPoints[0].released;
    const Vector2 pointerPosition = touchDown || touchPressed || touchReleased
        ? inputState_.touchPoints[0].position
        : inputState_.mousePosition;

    if (inputState_.mouseMoved || inputState_.touchMoved)
    {
        if (std::shared_ptr<View> hovered = FindTopmostViewAt(pointerPosition))
        {
            hovered->DispatchPointerMove(pointerPosition);
        }
    }

    if (inputState_.mouseLeftPressed || touchPressed)
    {
        std::shared_ptr<View> target = FindTopmostViewAt(pointerPosition);
        pressedView_ = target;
        if (target)
        {
            if (target->IsFocusable())
            {
                SetFocusedView(target);
            }
            target->DispatchPointerDown(pointerPosition);
        }
    }

    if (inputState_.keyPressed)
    {
        if (IsDirectionalKey(inputState_.lastKeyEvent, NavigationDirection::Up))
        {
            MoveFocus(NavigationDirection::Up);
        }
        else if (IsDirectionalKey(inputState_.lastKeyEvent, NavigationDirection::Down))
        {
            MoveFocus(NavigationDirection::Down);
        }
        else if (IsDirectionalKey(inputState_.lastKeyEvent, NavigationDirection::Left))
        {
            MoveFocus(NavigationDirection::Left);
        }
        else if (IsDirectionalKey(inputState_.lastKeyEvent, NavigationDirection::Right))
        {
            MoveFocus(NavigationDirection::Right);
        }
        else if (std::strcmp(inputState_.lastKeyEvent.name, "Tab") == 0)
        {
            MoveFocus(inputState_.lastKeyEvent.shift ? NavigationDirection::Left : NavigationDirection::Right);
        }

        if (std::shared_ptr<View> focused = focusedView_.lock())
        {
            focused->DispatchKeyDown(inputState_.lastKeyEvent);
        }
    }

    if (inputState_.keyReleased)
    {
        if (std::shared_ptr<View> focused = focusedView_.lock())
        {
            focused->DispatchKeyUp(inputState_.lastKeyEvent);
        }
    }

    if (inputState_.mouseLeftReleased || touchReleased)
    {
        std::shared_ptr<View> releasedTarget = FindTopmostViewAt(pointerPosition);
        if (std::shared_ptr<View> pressedTarget = pressedView_.lock())
        {
            pressedTarget->DispatchPointerUp(pointerPosition);
            if (pressedTarget == releasedTarget || pressedTarget->ContainsPoint(pointerPosition))
            {
                pressedTarget->DispatchClick(pointerPosition);
            }
        }
        pressedView_.reset();
    }

    if (IsActivationKey(inputState_))
    {
        if (std::shared_ptr<View> focused = focusedView_.lock())
        {
            const Vector2 center{
                focused->GetFrame().x + focused->GetFrame().width * 0.5F,
                focused->GetFrame().y + focused->GetFrame().height * 0.5F,
            };
            focused->DispatchClick(center);
        }
    }
}

void Application::ApplyRootViewFrames()
{
    if (!autoResizeRootViews_)
    {
        return;
    }

    const Vector2 size = GetLogicalSize();
    if (size.x <= 0.0F || size.y <= 0.0F)
    {
        return;
    }

    for (const auto& view : views_)
    {
        if (view)
        {
            view->SetFrame(Rect{0.0F, 0.0F, size.x, size.y});
        }
    }
}

void Application::UpdateFocusHighlightState(
    FocusHighlightState& state,
    const std::shared_ptr<View>& trackedView,
    float deltaSeconds,
    float targetOpacity)
{
    state.pulseTime += std::max(0.0F, deltaSeconds);

    if (trackedView && trackedView->IsVisible())
    {
        state.style = trackedView->GetFocusHighlightStyle();
        const Rect frame = trackedView->GetFrame();
        const float inset = std::max(0.0F, state.style.inset);
        state.targetRect = Rect{
            frame.x - inset,
            frame.y - inset,
            frame.width + inset * 2.0F,
            frame.height + inset * 2.0F,
        };
        state.targetOpacity = state.style.enabled ? targetOpacity : 0.0F;

        if (!state.initialized)
        {
            state.currentRect = state.targetRect;
            state.opacity = state.targetOpacity;
            state.initialized = true;
            return;
        }
    }
    else
    {
        state.targetOpacity = 0.0F;
    }

    if (!focusHighlightMotionEnabled_)
    {
        state.currentRect = state.targetRect;
        state.opacity = Approach(
            state.opacity,
            state.targetOpacity,
            deltaSeconds,
            kFocusHighlightOpacitySpeed);
        return;
    }

    state.currentRect.x = Approach(
        state.currentRect.x,
        state.targetRect.x,
        deltaSeconds,
        kFocusHighlightLerpSpeed);
    state.currentRect.y = Approach(
        state.currentRect.y,
        state.targetRect.y,
        deltaSeconds,
        kFocusHighlightLerpSpeed);
    state.currentRect.width = Approach(
        state.currentRect.width,
        state.targetRect.width,
        deltaSeconds,
        kFocusHighlightLerpSpeed);
    state.currentRect.height = Approach(
        state.currentRect.height,
        state.targetRect.height,
        deltaSeconds,
        kFocusHighlightLerpSpeed);
    state.opacity = Approach(
        state.opacity,
        state.targetOpacity,
        deltaSeconds,
        kFocusHighlightOpacitySpeed);
}

void Application::UpdateFocusHighlight(float deltaSeconds)
{
    UpdateFocusHighlightState(focusHighlight_, focusedView_.lock(), deltaSeconds, 1.0F);

    if (!preserveInactiveFocusHighlights_)
    {
        inactiveFocusHighlights_.clear();
        return;
    }

    const std::shared_ptr<View> activeRoot = TopLevelAncestor(focusedView_.lock());
    for (std::size_t index = 0; index < inactiveFocusHighlights_.size();)
    {
        FocusHighlightState& state = inactiveFocusHighlights_[index];
        const std::shared_ptr<View> root = state.root.lock();
        const std::shared_ptr<View> trackedView = state.view.lock();
        if (!root || !trackedView || !root->IsVisible() || !trackedView->IsVisible() ||
            (activeRoot && root == activeRoot) || !IsInSubtree(root, trackedView))
        {
            inactiveFocusHighlights_.erase(inactiveFocusHighlights_.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
        }

        UpdateFocusHighlightState(state, trackedView, deltaSeconds, 1.0F);
        ++index;
    }
}

void Application::DrawFocusHighlightState(RenderQueue& queue, const FocusHighlightState& state) const
{
    if (!state.initialized || !state.style.enabled || state.opacity <= 0.01F)
    {
        return;
    }

    const Rect& rect = state.currentRect;
    if (rect.width <= 0.0F || rect.height <= 0.0F)
    {
        return;
    }

    const float pulse = 0.5F + 0.5F * std::sin(state.pulseTime * 5.5F);
    const float glowAlpha = state.opacity * (0.14F + pulse * 0.10F);
    const float borderAlpha = state.opacity * (0.82F + pulse * 0.10F);

    const std::shared_ptr<View> trackedView = state.view.lock();
    const float preferredCornerRadius = state.style.cornerRadius >= 0.0F
        ? state.style.cornerRadius
        : (trackedView ? trackedView->GetCornerRadius() + std::max(0.0F, state.style.inset) : 0.0F);
    const float cornerRadius = std::max(
        0.0F,
        std::min({
            preferredCornerRadius,
            rect.width * 0.5F,
            rect.height * 0.5F,
        }));

    const auto buildRoundedRectPath = [&](const Rect& bounds, float radius) {
        std::vector<Vector2> points;
        if (bounds.width <= 0.0F || bounds.height <= 0.0F)
        {
            return points;
        }

        const float clampedRadius = std::max(0.0F, std::min({radius, bounds.width * 0.5F, bounds.height * 0.5F}));
        if (clampedRadius <= 0.0F)
        {
            points.push_back(Vector2{bounds.x, bounds.y});
            points.push_back(Vector2{bounds.x + bounds.width, bounds.y});
            points.push_back(Vector2{bounds.x + bounds.width, bounds.y + bounds.height});
            points.push_back(Vector2{bounds.x, bounds.y + bounds.height});
            return points;
        }

        const float arcLength = 0.5F * 3.1415926535F * clampedRadius;
        const int arcSteps = std::max(4, static_cast<int>(std::ceil(arcLength / kFocusHighlightSegmentLength)));
        points.push_back(Vector2{bounds.x + clampedRadius, bounds.y});
        points.push_back(Vector2{bounds.x + bounds.width - clampedRadius, bounds.y});

        const auto appendArc = [&](float centerX, float centerY, float startAngle, float endAngle) {
            for (int step = 1; step <= arcSteps; ++step)
            {
                const float t = static_cast<float>(step) / static_cast<float>(arcSteps);
                const float angle = startAngle + (endAngle - startAngle) * t;
                points.push_back(Vector2{
                    centerX + std::cos(angle) * clampedRadius,
                    centerY + std::sin(angle) * clampedRadius,
                });
            }
        };

        appendArc(
            bounds.x + bounds.width - clampedRadius,
            bounds.y + clampedRadius,
            -0.5F * 3.1415926535F,
            0.0F);
        points.push_back(Vector2{bounds.x + bounds.width, bounds.y + bounds.height - clampedRadius});
        appendArc(
            bounds.x + bounds.width - clampedRadius,
            bounds.y + bounds.height - clampedRadius,
            0.0F,
            0.5F * 3.1415926535F);
        points.push_back(Vector2{bounds.x + clampedRadius, bounds.y + bounds.height});
        appendArc(
            bounds.x + clampedRadius,
            bounds.y + bounds.height - clampedRadius,
            0.5F * 3.1415926535F,
            3.1415926535F);
        points.push_back(Vector2{bounds.x, bounds.y + clampedRadius});
        appendArc(
            bounds.x + clampedRadius,
            bounds.y + clampedRadius,
            3.1415926535F,
            1.5F * 3.1415926535F);
        return points;
    };

    const auto computePathLength = [&](const std::vector<Vector2>& path) {
        float total = 0.0F;
        if (path.size() < 2)
        {
            return total;
        }

        for (std::size_t index = 0; index < path.size(); ++index)
        {
            const std::size_t nextIndex = (index + 1) % path.size();
            const float dx = path[nextIndex].x - path[index].x;
            const float dy = path[nextIndex].y - path[index].y;
            total += std::sqrt(dx * dx + dy * dy);
        }
        return total;
    };

    const auto drawShadowPath = [&](const std::vector<Vector2>& path, float thickness, float alphaScale) {
        if (path.size() < 2 || thickness <= 0.0F)
        {
            return;
        }

        const Color shadowColor{0.0F, 0.0F, 0.0F, alphaScale};
        for (std::size_t index = 0; index < path.size(); ++index)
        {
            const std::size_t nextIndex = (index + 1) % path.size();
            queue.PushLine(path[index], path[nextIndex], shadowColor, thickness);
        }
    };

    const auto drawAnimatedPath = [&](const std::vector<Vector2>& path, float thickness, float alphaScale) {
        if (path.size() < 2 || thickness <= 0.0F)
        {
            return;
        }

        const float totalLength = std::max(1.0F, computePathLength(path));
        const float gradientX = (std::cos(state.pulseTime / kFocusHighlightTravelSpeed / 3.0F) + 1.0F) * 0.5F;
        const float gradientY = (std::sin(state.pulseTime / kFocusHighlightTravelSpeed / 3.0F) + 1.0F) * 0.5F;
        const float colorT = (std::sin(state.pulseTime * kFocusHighlightPulseSpeed) + 1.0F) * 0.5F;

        const Color highlight1 = state.style.color1;
        const Color highlight2 = state.style.color2;
        const Color pulseColor = LerpColor(highlight2, highlight1, colorT);
        const Color glowColor = LerpColor(highlight1, highlight2, 0.35F);

        const float glowPosition1 = gradientX * totalLength;
        const float glowPosition2 = (1.0F - gradientY) * totalLength;
        const float glowSpan = std::max(48.0F, kFocusHighlightGlowSpanPixels);

        float traversed = 0.0F;
        for (std::size_t index = 0; index < path.size(); ++index)
        {
            const std::size_t nextIndex = (index + 1) % path.size();
            const Vector2 start = path[index];
            const Vector2 end = path[nextIndex];
            const float dx = end.x - start.x;
            const float dy = end.y - start.y;
            const float segmentLength = std::sqrt(dx * dx + dy * dy);
            if (segmentLength <= 0.0001F)
            {
                continue;
            }

            const float midDistance = traversed + segmentLength * 0.5F;
            const float distance1 = WrapDistance(midDistance, glowPosition1, totalLength);
            const float distance2 = WrapDistance(midDistance, glowPosition2, totalLength);
            const float glowWeight1 = std::clamp(1.0F - distance1 / glowSpan, 0.0F, 1.0F);
            const float glowWeight2 = std::clamp(1.0F - distance2 / glowSpan, 0.0F, 1.0F);
            const float glowWeight = std::max(glowWeight1, glowWeight2);
            Color segmentColor = LerpColor(
                pulseColor,
                glowColor,
                0.25F + glowWeight * 0.75F);
            segmentColor.a = borderAlpha * alphaScale * (0.90F + glowWeight * 0.25F);

            queue.PushLine(start, end, segmentColor, thickness);
            traversed += segmentLength;
        }
    };

    for (int layer = 0; layer < static_cast<int>(kFocusHighlightShadowLayers); ++layer)
    {
        const float expansion = 2.0F + static_cast<float>(layer) * 2.5F;
        const float offsetY = 1.0F + static_cast<float>(layer) * 1.5F;
        const Rect shadowRect{
            rect.x - expansion,
            rect.y - expansion + offsetY,
            rect.width + expansion * 2.0F,
            rect.height + expansion * 2.0F,
        };
        const std::vector<Vector2> shadowPath = buildRoundedRectPath(shadowRect, cornerRadius + expansion);
        drawShadowPath(
            shadowPath,
            std::max(0.0F, state.style.glowThickness) + static_cast<float>(layer),
            state.opacity * (0.12F - static_cast<float>(layer) * 0.025F));
    }

    const std::vector<Vector2> borderPath = buildRoundedRectPath(rect, cornerRadius);
    const float glowThickness = std::max(0.0F, state.style.glowThickness);
    const Rect glowRect{
        rect.x - glowThickness * 0.5F,
        rect.y - glowThickness * 0.5F,
        rect.width + glowThickness,
        rect.height + glowThickness,
    };
    const std::vector<Vector2> glowPath = buildRoundedRectPath(glowRect, cornerRadius + glowThickness * 0.5F);

    drawAnimatedPath(glowPath, glowThickness, glowAlpha / std::max(0.001F, borderAlpha));
    drawAnimatedPath(borderPath, std::max(0.0F, state.style.thickness), 1.0F);
}

void Application::DrawFocusHighlight(RenderQueue& queue) const
{
    for (const FocusHighlightState& state : inactiveFocusHighlights_)
    {
        DrawFocusHighlightState(queue, state);
    }
    DrawFocusHighlightState(queue, focusHighlight_);
}

void Application::DrawFocusHighlightsForRoot(RenderQueue& queue, const std::shared_ptr<View>& root) const
{
    if (!root)
    {
        return;
    }

    if (preserveInactiveFocusHighlights_)
    {
        for (const FocusHighlightState& state : inactiveFocusHighlights_)
        {
            if (state.root.lock() == root)
            {
                DrawFocusHighlightState(queue, state);
            }
        }
    }

    if (focusHighlight_.root.lock() == root)
    {
        DrawFocusHighlightState(queue, focusHighlight_);
    }
}

void Application::RemoveInactiveFocusHighlight(const std::shared_ptr<View>& root)
{
    if (!root)
    {
        return;
    }

    inactiveFocusHighlights_.erase(
        std::remove_if(
            inactiveFocusHighlights_.begin(),
            inactiveFocusHighlights_.end(),
            [&](const FocusHighlightState& state) {
                return state.root.lock() == root;
            }),
        inactiveFocusHighlights_.end());
}

void Application::CaptureInactiveFocusHighlight(const std::shared_ptr<View>& root, const std::shared_ptr<View>& view)
{
    if (!root || !view)
    {
        return;
    }

    for (FocusHighlightState& state : inactiveFocusHighlights_)
    {
        if (state.root.lock() == root)
        {
            state.view = view;
            state.root = root;
            state.currentRect = focusHighlight_.currentRect;
            state.targetRect = focusHighlight_.targetRect;
            state.opacity = std::max(focusHighlight_.opacity, 1.0F);
            state.targetOpacity = 1.0F;
            state.pulseTime = focusHighlight_.pulseTime;
            state.initialized = true;
            return;
        }
    }

    FocusHighlightState state = focusHighlight_;
    state.view = view;
    state.root = root;
    state.opacity = std::max(focusHighlight_.opacity, 1.0F);
    state.targetOpacity = 1.0F;
    state.initialized = true;
    inactiveFocusHighlights_.push_back(std::move(state));
}

std::shared_ptr<View> Application::FindTopmostViewAt(const Vector2& point) const
{
    for (const auto& view : OrderedViews(true))
    {
        if (!view || !view->IsVisible())
        {
            continue;
        }

        if (std::shared_ptr<View> hit = view->HitTest(point))
        {
            return hit;
        }
    }

    return nullptr;
}

std::shared_ptr<View> Application::FindFirstFocusableView() const
{
    auto visit = [&](const std::shared_ptr<View>& root, const auto& self) -> std::shared_ptr<View> {
        if (!root || !root->IsVisible())
        {
            return nullptr;
        }

        for (const auto& child : root->VisibleChildrenByZOrder())
        {
            if (std::shared_ptr<View> nested = self(child, self))
            {
                return nested;
            }
        }

        if (root->IsFocusable())
        {
            return root;
        }

        return nullptr;
    };

    const std::vector<std::shared_ptr<View>>& orderedViews = OrderedViews(false);

    const std::shared_ptr<View> activeRoot = FindTopmostVisibleRoot(orderedViews);
    if (activeRoot)
    {
        if (std::shared_ptr<View> preferred = ResolvePreferredFocusCandidate(activeRoot))
        {
            return preferred;
        }

        if (std::shared_ptr<View> focusable = visit(activeRoot, visit))
        {
            return focusable;
        }
    }

    for (const auto& view : orderedViews)
    {
        if (std::shared_ptr<View> preferred = ResolvePreferredFocusCandidate(view))
        {
            return preferred;
        }

        if (std::shared_ptr<View> focusable = visit(view, visit))
        {
            return focusable;
        }
    }
    return nullptr;
}

void Application::InvalidateViewOrderCache()
{
    viewOrderCacheDirty_ = true;
}

const std::vector<std::shared_ptr<View>>& Application::OrderedViews(bool descending) const
{
    if (viewOrderCacheDirty_)
    {
        orderedViewsAscendingCache_ = views_;
        std::stable_sort(
            orderedViewsAscendingCache_.begin(),
            orderedViewsAscendingCache_.end(),
            [](const std::shared_ptr<View>& lhs, const std::shared_ptr<View>& rhs) {
                if (!lhs || !rhs)
                {
                    return static_cast<bool>(lhs);
                }
                return lhs->GetZIndex() < rhs->GetZIndex();
            });
        orderedViewsDescendingCache_ = orderedViewsAscendingCache_;
        std::reverse(orderedViewsDescendingCache_.begin(), orderedViewsDescendingCache_.end());
        viewOrderCacheDirty_ = false;
    }

    return descending ? orderedViewsDescendingCache_ : orderedViewsAscendingCache_;
}

bool Application::IsDirectionalKey(const InputState::KeyEvent& key, NavigationDirection direction)
{
    switch (direction)
    {
    case NavigationDirection::Up:
        return std::strcmp(key.name, "Up") == 0;
    case NavigationDirection::Down:
        return std::strcmp(key.name, "Down") == 0;
    case NavigationDirection::Left:
        return std::strcmp(key.name, "Left") == 0;
    case NavigationDirection::Right:
        return std::strcmp(key.name, "Right") == 0;
    }

    return false;
}

bool Application::IsActivationKey(const InputState& input)
{
    if (input.confirmPressed)
    {
        return true;
    }

    if (!input.keyPressed)
    {
        return false;
    }

    return std::strcmp(input.lastKeyEvent.name, "Return") == 0 ||
        std::strcmp(input.lastKeyEvent.name, "Enter") == 0 ||
        std::strcmp(input.lastKeyEvent.name, "Space") == 0 ||
        std::strcmp(input.lastKeyEvent.name, "A") == 0;
}

void Application::ResetState()
{
    descriptor_ = ApplicationDesc{};
    arguments_.clear();
    views_.clear();
    orderedViewsAscendingCache_.clear();
    orderedViewsDescendingCache_.clear();
    viewOrderCacheDirty_ = true;
    renderQueue_.Clear();
    metaData_.Clear();
    inputState_ = {};
    focusedView_.reset();
    pressedView_.reset();
    focusHighlight_ = FocusHighlightState{};
    inactiveFocusHighlights_.clear();
    defaultFocusHighlightStyle_ = FocusHighlightStyle{};
    windowSize_ = Vector2{1280.0F, 720.0F};
    logicalSize_ = {};
    autoResizeRootViews_ = true;
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
    metaData_.Set("application.window_width", static_cast<std::uint32_t>(std::max(0.0F, windowSize_.x)));
    metaData_.Set("application.window_height", static_cast<std::uint32_t>(std::max(0.0F, windowSize_.y)));
    metaData_.Set("application.logical_width", static_cast<std::uint32_t>(std::max(0.0F, GetLogicalSize().x)));
    metaData_.Set("application.logical_height", static_cast<std::uint32_t>(std::max(0.0F, GetLogicalSize().y)));
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
