#include <bkui/render/RenderQueue.hpp>

#include <algorithm>
#include <utility>

namespace bk
{

void RenderQueue::Clear()
{
    commands_.clear();
}

void RenderQueue::PushRect(const Rect& bounds, const Color& color)
{
    commands_.push_back(RenderCommand{
        RenderCommandType::Rect,
        bounds,
        color,
        {},
        0.0F,
        {},
        {},
        1.5F,
        0.0F,
    });
}

void RenderQueue::PushRoundedRect(const Rect& bounds, const Color& color, float cornerRadius)
{
    commands_.push_back(RenderCommand{
        RenderCommandType::RoundedRect,
        bounds,
        color,
        {},
        0.0F,
        {},
        {},
        1.5F,
        std::max(0.0F, cornerRadius),
    });
}

void RenderQueue::PushText(const Rect& bounds, std::string text, float fontSize, const Color& color)
{
    commands_.push_back(RenderCommand{
        RenderCommandType::Text,
        bounds,
        color,
        std::move(text),
        fontSize,
        {},
        {},
        1.5F,
        0.0F,
    });
}

void RenderQueue::PushLine(const Vector2& start, const Vector2& end, const Color& color, float thickness)
{
    const float left = std::min(start.x, end.x);
    const float top = std::min(start.y, end.y);
    const float right = std::max(start.x, end.x);
    const float bottom = std::max(start.y, end.y);

    commands_.push_back(RenderCommand{
        RenderCommandType::Line,
        Rect{left, top, right - left, bottom - top},
        color,
        {},
        0.0F,
        start,
        end,
        std::max(0.1F, thickness),
        0.0F,
    });
}

void RenderQueue::PushClipRect(const Rect& bounds)
{
    commands_.push_back(RenderCommand{
        RenderCommandType::PushClip,
        bounds,
        {},
        {},
        0.0F,
        {},
        {},
        1.5F,
        0.0F,
    });
}

void RenderQueue::PopClipRect()
{
    commands_.push_back(RenderCommand{
        RenderCommandType::PopClip,
        {},
        {},
        {},
        0.0F,
        {},
        {},
        1.5F,
        0.0F,
    });
}

const std::vector<RenderCommand>& RenderQueue::Commands() const
{
    return commands_;
}

}
