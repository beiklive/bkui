#include <bkui/ui/View.hpp>

namespace bk
{

void View::SetFrame(const Rect& frame)
{
    frame_ = frame;
}

const Rect& View::GetFrame() const
{
    return frame_;
}

void View::AddChild(std::shared_ptr<View> child)
{
    if (child)
    {
        children_.push_back(std::move(child));
    }
}

const std::vector<std::shared_ptr<View>>& View::Children() const
{
    return children_;
}

Size View::Measure(const Size& available) const
{
    return available;
}

void View::Layout()
{
    for (const auto& child : children_)
    {
        if (child)
        {
            child->Layout();
        }
    }
}

void View::Render(RenderQueue& queue) const
{
    for (const auto& child : children_)
    {
        if (child)
        {
            child->Render(queue);
        }
    }
}

}
