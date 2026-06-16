#pragma once

#include <cstdint>
#include <vector>

namespace bk
{

struct Vector2
{
    float x = 0.0F;
    float y = 0.0F;
};

struct Color
{
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 1.0F;
};

using Buffer = std::vector<std::uint8_t>;

}
