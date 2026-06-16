#pragma once

#include <bkui/core/Types.hpp>
#include <bkui/platform/Font.hpp>
#include <bkui/render/RenderQueue.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace demo
{

struct Vertex
{
    float position[2];
    float color[3];
    float uv[2];
};

struct Image
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

struct IntRect
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct FontResource
{
    struct Face
    {
        bk::Buffer data;
        stbtt_fontinfo info{};
    };

    std::vector<Face> faces;

    [[nodiscard]] bool Valid() const
    {
        return !faces.empty();
    }

    [[nodiscard]] const Face* PrimaryFace() const
    {
        return faces.empty() ? nullptr : &faces.front();
    }
};

void PutPixel(Image& image, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);
void BlendPixel(Image& image, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);
void FillRect(Image& image, int x, int y, int width, int height, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);
Image MakeImage(int width, int height, std::uint8_t r = 0, std::uint8_t g = 0, std::uint8_t b = 0, std::uint8_t a = 0);
void ClearImage(Image& image, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);
IntRect NormalizeRect(const IntRect& rect);
IntRect ClampRectToImage(const Image& image, const IntRect& rect);
IntRect UnionRect(const IntRect& a, const IntRect& b);
void BlitRect(Image& dst, const Image& src, const IntRect& rect);
Image ExtractRegion(const Image& image, const IntRect& rect);
std::array<Vertex, 6> MakeFullscreenQuad();
const FontResource& GlobalFont();
std::u32string Utf8ToUtf32(const std::string& text);
std::uint8_t ToByte(float value);
std::string FormatFloat(float value, int precision = 1);
void DrawText(
    Image& dst,
    const FontResource& font,
    const std::u32string& text,
    int x,
    int y,
    float pixelHeight,
    std::uint8_t r,
    std::uint8_t g,
    std::uint8_t b,
    const IntRect* clipRect = nullptr);
void RenderQueueToImage(Image& image, const FontResource& font, const bk::RenderQueue& queue);
void RenderQueueToImage(Image& image, const FontResource& font, const bk::RenderQueue& queue, const IntRect& clipRect);
bk::Vector2 EnsureWindowSize(const bk::Vector2& size);

}
