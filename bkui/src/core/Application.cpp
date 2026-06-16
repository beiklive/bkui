#include <bkui/core/Application.hpp>

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

void Application::SetInputState(const InputState& input)
{
    inputState_ = input;
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

    if (current)
    {
        current->SetFocusedState(false);
    }

    focusedView_.reset();
    if (view)
    {
        view->SetFocusedState(true);
        focusedView_ = view;

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

    std::vector<std::shared_ptr<View>> orderedViews = views_;
    std::stable_sort(
        orderedViews.begin(),
        orderedViews.end(),
        [](const std::shared_ptr<View>& lhs, const std::shared_ptr<View>& rhs) {
            if (!lhs || !rhs)
            {
                return static_cast<bool>(lhs);
            }
            return lhs->GetZIndex() < rhs->GetZIndex();
        });

    for (const auto& view : orderedViews)
    {
        if (!view || !view->IsVisible())
        {
            continue;
        }

        view->Frame(deltaSeconds);
        view->Paint(renderQueue_);
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

std::shared_ptr<View> Application::FindTopmostViewAt(const Vector2& point) const
{
    std::vector<std::shared_ptr<View>> orderedViews = views_;
    std::stable_sort(
        orderedViews.begin(),
        orderedViews.end(),
        [](const std::shared_ptr<View>& lhs, const std::shared_ptr<View>& rhs) {
            if (!lhs || !rhs)
            {
                return static_cast<bool>(lhs);
            }
            return lhs->GetZIndex() > rhs->GetZIndex();
        });

    for (const auto& view : orderedViews)
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

    std::vector<std::shared_ptr<View>> orderedViews = views_;
    std::stable_sort(
        orderedViews.begin(),
        orderedViews.end(),
        [](const std::shared_ptr<View>& lhs, const std::shared_ptr<View>& rhs) {
            if (!lhs || !rhs)
            {
                return static_cast<bool>(lhs);
            }
            return lhs->GetZIndex() < rhs->GetZIndex();
        });

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
    renderQueue_.Clear();
    metaData_.Clear();
    inputState_ = {};
    focusedView_.reset();
    pressedView_.reset();
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
