#include <bkui/ui/Box.hpp>

#include <algorithm>

namespace bk
{

Box::Box(BoxDirection direction)
    : direction_(direction)
{
}

void Box::SetSpacing(float spacing)
{
    spacing_ = spacing;
}

float Box::GetSpacing() const
{
    return spacing_;
}

Size Box::Measure(const Size& available) const
{
    Size result{};
    bool first = true;

    for (const auto& child : children_)
    {
        if (!child)
        {
            continue;
        }

        const Size childSize = child->Measure(available);
        if (direction_ == BoxDirection::Horizontal)
        {
            result.width += childSize.width;
            result.height = std::max(result.height, childSize.height);
            if (!first)
            {
                result.width += spacing_;
            }
        }
        else
        {
            result.width = std::max(result.width, childSize.width);
            result.height += childSize.height;
            if (!first)
            {
                result.height += spacing_;
            }
        }
        first = false;
    }

    return result;
}

void Box::Layout()
{
    float cursor = direction_ == BoxDirection::Horizontal ? frame_.x : frame_.y;

    for (const auto& child : children_)
    {
        if (!child)
        {
            continue;
        }

        const Size childSize = child->Measure(Size{frame_.width, frame_.height});
        if (direction_ == BoxDirection::Horizontal)
        {
            child->SetFrame(Rect{cursor, frame_.y, childSize.width, frame_.height});
            cursor += childSize.width + spacing_;
        }
        else
        {
            child->SetFrame(Rect{frame_.x, cursor, frame_.width, childSize.height});
            cursor += childSize.height + spacing_;
        }

        child->Layout();
    }
}

HBox::HBox()
    : Box(BoxDirection::Horizontal)
{
}

VBox::VBox()
    : Box(BoxDirection::Vertical)
{
}

}
