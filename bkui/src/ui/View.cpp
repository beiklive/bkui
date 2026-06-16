#include <bkui/ui/View.hpp>

#include <algorithm>
#include <utility>

namespace bk
{

void View::SetName(std::string name)
{
    name_ = std::move(name);
}

const std::string& View::GetName() const
{
    return name_;
}

void View::SetFrame(const Rect& frame)
{
    frame_ = frame;
    InvalidateLayout();
}

const Rect& View::GetFrame() const
{
    return frame_;
}

void View::SetVisible(bool visible)
{
    if (visible_ == visible)
    {
        return;
    }

    visible_ = visible;
    InvalidateLayout();
}

bool View::IsVisible() const
{
    return visible_;
}

void View::AddChild(std::shared_ptr<View> child)
{
    if (child)
    {
        child->parent_ = this;
        children_.push_back(std::move(child));
        InvalidateLayout();
    }
}

void View::RemoveChild(const std::shared_ptr<View>& child)
{
    const auto it = std::remove(children_.begin(), children_.end(), child);
    if (it != children_.end())
    {
        for (auto clearIt = it; clearIt != children_.end(); ++clearIt)
        {
            if (*clearIt)
            {
                (*clearIt)->parent_ = nullptr;
            }
        }
        children_.erase(it, children_.end());
        InvalidateLayout();
    }
}

void View::ClearChildren()
{
    if (!children_.empty())
    {
        for (auto& child : children_)
        {
            if (child)
            {
                child->parent_ = nullptr;
            }
        }
        children_.clear();
        InvalidateLayout();
    }
}

const std::vector<std::shared_ptr<View>>& View::Children() const
{
    return children_;
}

const View* View::GetParent() const
{
    return parent_;
}

Size View::Measure(const Size& available) const
{
    return available;
}

void View::Frame(float deltaSeconds)
{
    if (!visible_)
    {
        return;
    }

    if (needsLayout_)
    {
        Layout();
    }

    Update(deltaSeconds);

    for (const auto& child : children_)
    {
        if (child && child->IsVisible())
        {
            child->Frame(deltaSeconds);
        }
    }
}

void View::Layout()
{
    needsLayout_ = false;
    for (const auto& child : children_)
    {
        if (child && child->IsVisible())
        {
            child->Layout();
        }
    }
}

void View::Draw(RenderQueue& queue) const
{
    if (!visible_)
    {
        return;
    }

    DrawSelf(queue);
    DrawChildren(queue);
}

void View::Render(RenderQueue& queue) const
{
    Draw(queue);
}

void View::InvalidateLayout()
{
    if (needsLayout_)
    {
        if (parent_ != nullptr)
        {
            parent_->InvalidateLayout();
        }
        return;
    }

    needsLayout_ = true;
    if (parent_ != nullptr)
    {
        parent_->InvalidateLayout();
    }
}

bool View::NeedsLayout() const
{
    return needsLayout_;
}

void View::Update(float)
{
}

void View::DrawSelf(RenderQueue&) const
{
}

void View::DrawChildren(RenderQueue& queue) const
{
    for (const auto& child : children_)
    {
        if (child && child->IsVisible())
        {
            child->Draw(queue);
        }
    }
}

}
