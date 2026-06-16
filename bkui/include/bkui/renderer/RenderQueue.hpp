#pragma once

#include <bkui/renderer/RenderCommand.hpp>

#include <vector>

namespace bk
{

class RenderQueue
{
public:
    void Clear();
    void PushRect(const Rect& bounds, const Color& color);
    void PushText(const Rect& bounds, std::string text, float fontSize, const Color& color);

    [[nodiscard]] const std::vector<RenderCommand>& Commands() const;

private:
    std::vector<RenderCommand> commands_;
};

}
