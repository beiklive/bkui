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

/// RGB 颜色（无 alpha，默认 1.0），取值 0.0 ~ 1.0 或 0 ~ 255。
struct ColorRGB
{
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;

    constexpr ColorRGB() = default;
    constexpr ColorRGB(float rf, float gf, float bf) : r(rf), g(gf), b(bf) {}
    constexpr ColorRGB(int ri, int gi, int bi)
        : r(ri / 255.0F), g(gi / 255.0F), b(bi / 255.0F) {}
};

/// RGBA 颜色（含 alpha），取值 0.0 ~ 1.0 或 0 ~ 255。
struct ColorRGBA
{
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 1.0F;

    constexpr ColorRGBA() = default;
    constexpr ColorRGBA(float rf, float gf, float bf, float af) : r(rf), g(gf), b(bf), a(af) {}
    constexpr ColorRGBA(int ri, int gi, int bi, int ai)
        : r(ri / 255.0F), g(gi / 255.0F), b(bi / 255.0F), a(ai / 255.0F) {}
    constexpr ColorRGBA(ColorRGB c) : r(c.r), g(c.g), b(c.b), a(1.0F) {}
};

/// 通用二进制缓冲区，常用于文件数据或资源内容。
using Buffer = std::vector<std::uint8_t>;

}
