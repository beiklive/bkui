#include <bkui/ui/Box.hpp>

#include <algorithm>
#include <vector>

namespace bk
{

Box::Box(BoxDirection direction)
    : direction_(direction)
{
}

void Box::SetSpacing(float spacing)
{
    spacing_ = spacing;
    InvalidateLayout();
}

float Box::GetSpacing() const
{
    return spacing_;
}

void Box::SetBackgroundColor(const Color& color)
{
    backgroundColor_ = color;
}

const Color& Box::GetBackgroundColor() const
{
    return backgroundColor_;
}

void Box::SetDrawBackground(bool enabled)
{
    drawBackground_ = enabled;
}

bool Box::IsBackgroundEnabled() const
{
    return drawBackground_;
}

Size Box::Measure(const Size& available) const
{
    Size result{};
    bool first = true;

    for (const auto& child : children_)
    {
        if (!child || !child->IsVisible())
        {
            continue;
        }

        const Size childSize = child->MeasureLayout(available);
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
    const Rect contentFrame = GetContentFrame();

    std::vector<std::shared_ptr<View>> visibleChildren;
    std::vector<Size> measuredSizes;
    visibleChildren.reserve(children_.size());
    measuredSizes.reserve(children_.size());

    float totalPrimary = 0.0F;
    float totalFlexGrow = 0.0F;
    bool first = true;
    for (const auto& child : children_)
    {
        if (!child || !child->IsVisible())
        {
            continue;
        }

        const Size measured = child->MeasureLayout(Size{contentFrame.width, contentFrame.height});
        visibleChildren.push_back(child);
        measuredSizes.push_back(measured);

        totalPrimary += direction_ == BoxDirection::Horizontal ? measured.width : measured.height;
        if (!first)
        {
            totalPrimary += spacing_;
        }
        totalFlexGrow += child->GetFlexGrow();
        first = false;
    }

    const float availablePrimary = direction_ == BoxDirection::Horizontal ? contentFrame.width : contentFrame.height;
    const float extraPrimary = std::max(0.0F, availablePrimary - totalPrimary);
    float cursor = direction_ == BoxDirection::Horizontal ? contentFrame.x : contentFrame.y;

    for (std::size_t index = 0; index < visibleChildren.size(); ++index)
    {
        const auto& child = visibleChildren[index];
        Size childSize = measuredSizes[index];

        float primaryGrow = 0.0F;
        if (extraPrimary > 0.0F && totalFlexGrow > 0.0F)
        {
            primaryGrow = extraPrimary * (child->GetFlexGrow() / totalFlexGrow);
        }

        if (direction_ == BoxDirection::Horizontal)
        {
            childSize.width += primaryGrow;
            child->SetFrame(child->ResolveFrameInSlot(Rect{
                cursor,
                contentFrame.y,
                childSize.width,
                contentFrame.height,
            }));
            cursor += childSize.width + spacing_;
        }
        else
        {
            childSize.height += primaryGrow;
            child->SetFrame(child->ResolveFrameInSlot(Rect{
                contentFrame.x,
                cursor,
                contentFrame.width,
                childSize.height,
            }));
            cursor += childSize.height + spacing_;
        }

        child->Layout();
    }

    needsLayout_ = false;
}

void Box::Draw(RenderQueue& queue) const
{
    if (drawBackground_ && frame_.width > 0.0F && frame_.height > 0.0F)
    {
        queue.PushRect(frame_, backgroundColor_);
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
