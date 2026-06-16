#pragma once

#include <bkui/core/Types.hpp>

#include <string>

namespace bk
{

/// 渲染命令类型。
enum class RenderCommandType
{
    Rect,
    Text,
};

/// 浮点矩形区域。
struct Rect
{
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
};

/// 提交给渲染阶段的简单绘制命令。
struct RenderCommand
{
    RenderCommandType type = RenderCommandType::Rect;
    Rect bounds{};
    Color color{};
    std::string text;
    float fontSize = 16.0F;
};

}
