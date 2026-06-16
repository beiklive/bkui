#pragma once

#include <cstdint>
#include <vector>

namespace bk
{

/// 二维浮点坐标或尺寸。
struct Vector2
{
    float x = 0.0F;
    float y = 0.0F;
};

/// 归一化 RGBA 颜色，取值范围通常为 0.0 到 1.0。
struct Color
{
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 1.0F;
};

/// 通用二进制缓冲区，常用于文件数据或资源内容。
using Buffer = std::vector<std::uint8_t>;

}
