#include <bkui/renderer/RenderQueue.hpp>

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
    });
}

const std::vector<RenderCommand>& RenderQueue::Commands() const
{
    return commands_;
}

}
