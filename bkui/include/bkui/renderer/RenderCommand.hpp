#pragma once

#include <bkui/core/Types.hpp>

#include <string>

namespace bk
{

enum class RenderCommandType
{
    Rect,
    Text,
};

struct Rect
{
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
};

struct RenderCommand
{
    RenderCommandType type = RenderCommandType::Rect;
    Rect bounds{};
    Color color{};
    std::string text;
    float fontSize = 16.0F;
};

}
