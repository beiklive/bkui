#include <bkui/bkui.hpp>

#include "demo_support.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace demo
{
namespace
{
IntRect IntersectRect(const IntRect& a, const IntRect& b)
{
    const int left = std::max(a.x, b.x);
    const int top = std::max(a.y, b.y);
    const int right = std::min(a.x + a.width, b.x + b.width);
    const int bottom = std::min(a.y + a.height, b.y + b.height);
    if (right <= left || bottom <= top)
    {
        return {};
    }

    return IntRect{left, top, right - left, bottom - top};
}

bool PointInClip(const IntRect* clipRect, int x, int y)
{
    if (clipRect == nullptr)
    {
        return true;
    }

    return x >= clipRect->x &&
        y >= clipRect->y &&
        x < clipRect->x + clipRect->width &&
        y < clipRect->y + clipRect->height;
}

IntRect CommandBoundsToIntRect(const bk::RenderCommand& command)
{
    return IntRect{
        static_cast<int>(std::floor(command.bounds.x)),
        static_cast<int>(std::floor(command.bounds.y)),
        static_cast<int>(std::ceil(command.bounds.width)),
        static_cast<int>(std::ceil(command.bounds.height)),
    };
}

struct GlyphBitmap
{
    int width = 0;
    int height = 0;
    int x0 = 0;
    int y0 = 0;
    int advance = 0;
    std::vector<std::uint8_t> pixels;
};

struct GlyphCacheKey
{
    const FontResource::Face* face = nullptr;
    char32_t codepoint = 0;
    int pixelHeightX10 = 0;

    bool operator==(const GlyphCacheKey& other) const
    {
        return face == other.face &&
            codepoint == other.codepoint &&
            pixelHeightX10 == other.pixelHeightX10;
    }
};

struct GlyphCacheKeyHasher
{
    std::size_t operator()(const GlyphCacheKey& key) const noexcept
    {
        const std::size_t h1 = std::hash<const void*>{}(static_cast<const void*>(key.face));
        const std::size_t h2 = std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(key.codepoint));
        const std::size_t h3 = std::hash<int>{}(key.pixelHeightX10);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

const FontResource::Face* SelectFontFace(const FontResource& font, char32_t codepoint)
{
    for (const auto& face : font.faces)
    {
        if (stbtt_FindGlyphIndex(&face.info, static_cast<int>(codepoint)) != 0)
        {
            return &face;
        }
    }

    return font.PrimaryFace();
}

GlyphBitmap BuildGlyphBitmap(const FontResource::Face& face, char32_t codepoint, float pixelHeight)
{
    GlyphBitmap glyph;

    const float scale = stbtt_ScaleForPixelHeight(&face.info, pixelHeight);
    int advance = 0;
    int bearing = 0;
    stbtt_GetCodepointHMetrics(&face.info, static_cast<int>(codepoint), &advance, &bearing);
    glyph.advance = static_cast<int>(advance * scale);

    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    stbtt_GetCodepointBitmapBox(&face.info, static_cast<int>(codepoint), scale, scale, &x0, &y0, &x1, &y1);
    glyph.x0 = x0;
    glyph.y0 = y0;
    glyph.width = x1 - x0;
    glyph.height = y1 - y0;

    if (glyph.width > 0 && glyph.height > 0)
    {
        glyph.pixels.resize(static_cast<std::size_t>(glyph.width * glyph.height), 0);
        stbtt_MakeCodepointBitmap(
            &face.info,
            glyph.pixels.data(),
            glyph.width,
            glyph.height,
            glyph.width,
            scale,
            scale,
            static_cast<int>(codepoint));
    }

    return glyph;
}

const GlyphBitmap& GetGlyphBitmap(const FontResource::Face& face, char32_t codepoint, float pixelHeight)
{
    static std::unordered_map<GlyphCacheKey, GlyphBitmap, GlyphCacheKeyHasher> cache;

    const GlyphCacheKey key{
        &face,
        codepoint,
        static_cast<int>(std::round(pixelHeight * 10.0F)),
    };

    const auto it = cache.find(key);
    if (it != cache.end())
    {
        return it->second;
    }

    return cache.emplace(key, BuildGlyphBitmap(face, codepoint, pixelHeight)).first->second;
}
}

void PutPixel(Image& image, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    if (x < 0 || y < 0 || x >= image.width || y >= image.height)
    {
        return;
    }

    const std::size_t index = static_cast<std::size_t>((y * image.width + x) * 4);
    image.pixels[index + 0] = r;
    image.pixels[index + 1] = g;
    image.pixels[index + 2] = b;
    image.pixels[index + 3] = a;
}

void BlendPixel(Image& image, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    if (x < 0 || y < 0 || x >= image.width || y >= image.height)
    {
        return;
    }

    const std::size_t index = static_cast<std::size_t>((y * image.width + x) * 4);
    const float alpha = static_cast<float>(a) / 255.0F;
    image.pixels[index + 0] = static_cast<std::uint8_t>(r * alpha + image.pixels[index + 0] * (1.0F - alpha));
    image.pixels[index + 1] = static_cast<std::uint8_t>(g * alpha + image.pixels[index + 1] * (1.0F - alpha));
    image.pixels[index + 2] = static_cast<std::uint8_t>(b * alpha + image.pixels[index + 2] * (1.0F - alpha));
    image.pixels[index + 3] = std::max(image.pixels[index + 3], a);
}

void FillRect(Image& image, int x, int y, int width, int height, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    for (int yy = y; yy < y + height; ++yy)
    {
        for (int xx = x; xx < x + width; ++xx)
        {
            if (a >= 255)
            {
                PutPixel(image, xx, yy, r, g, b, a);
            }
            else
            {
                BlendPixel(image, xx, yy, r, g, b, a);
            }
        }
    }
}

void DrawLine(
    Image& image,
    int x0,
    int y0,
    int x1,
    int y1,
    std::uint8_t r,
    std::uint8_t g,
    std::uint8_t b,
    std::uint8_t a,
    float thickness = 1.0F,
    const IntRect* clipRect = nullptr)
{
    const int radius = std::max(0, static_cast<int>(std::round(std::max(1.0F, thickness) * 0.5F)) - 1);
    const int dx = std::abs(x1 - x0);
    const int dy = std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx - dy;

    for (;;)
    {
        for (int offsetY = -radius; offsetY <= radius; ++offsetY)
        {
            for (int offsetX = -radius; offsetX <= radius; ++offsetX)
            {
                const int pixelX = x0 + offsetX;
                const int pixelY = y0 + offsetY;
                if (!PointInClip(clipRect, pixelX, pixelY))
                {
                    continue;
                }

                if (a >= 255)
                {
                    PutPixel(image, pixelX, pixelY, r, g, b, a);
                }
                else
                {
                    BlendPixel(image, pixelX, pixelY, r, g, b, a);
                }
            }
        }

        if (x0 == x1 && y0 == y1)
        {
            break;
        }

        const int error2 = error * 2;
        if (error2 > -dy)
        {
            error -= dy;
            x0 += sx;
        }
        if (error2 < dx)
        {
            error += dx;
            y0 += sy;
        }
    }
}

Image MakeImage(int width, int height, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    Image image;
    image.width = width;
    image.height = height;
    image.pixels.resize(static_cast<std::size_t>(width * height * 4), 0);

    if (a == 0 && r == 0 && g == 0 && b == 0)
    {
        return image;
    }

    FillRect(image, 0, 0, width, height, r, g, b, a);
    return image;
}

void ClearImage(Image& image, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    FillRect(image, 0, 0, image.width, image.height, r, g, b, a);
}

IntRect NormalizeRect(const IntRect& rect)
{
    if (rect.width <= 0 || rect.height <= 0)
    {
        return {};
    }

    return rect;
}

IntRect ClampRectToImage(const Image& image, const IntRect& rect)
{
    IntRect normalized = NormalizeRect(rect);
    if (normalized.width <= 0 || normalized.height <= 0)
    {
        return {};
    }

    const int left = std::clamp(normalized.x, 0, image.width);
    const int top = std::clamp(normalized.y, 0, image.height);
    const int right = std::clamp(normalized.x + normalized.width, 0, image.width);
    const int bottom = std::clamp(normalized.y + normalized.height, 0, image.height);
    if (right <= left || bottom <= top)
    {
        return {};
    }

    return IntRect{left, top, right - left, bottom - top};
}

IntRect UnionRect(const IntRect& a, const IntRect& b)
{
    if (a.width <= 0 || a.height <= 0)
    {
        return b;
    }
    if (b.width <= 0 || b.height <= 0)
    {
        return a;
    }

    const int left = std::min(a.x, b.x);
    const int top = std::min(a.y, b.y);
    const int right = std::max(a.x + a.width, b.x + b.width);
    const int bottom = std::max(a.y + a.height, b.y + b.height);
    return IntRect{left, top, right - left, bottom - top};
}

void BlitRect(Image& dst, const Image& src, const IntRect& rect)
{
    const IntRect clipped = ClampRectToImage(dst, rect);
    if (clipped.width <= 0 || clipped.height <= 0 || src.width != dst.width || src.height != dst.height)
    {
        return;
    }

    const std::size_t rowBytes = static_cast<std::size_t>(clipped.width) * 4;
    for (int row = 0; row < clipped.height; ++row)
    {
        const std::size_t srcOffset = static_cast<std::size_t>((clipped.y + row) * src.width + clipped.x) * 4;
        const std::size_t dstOffset = static_cast<std::size_t>((clipped.y + row) * dst.width + clipped.x) * 4;
        std::memcpy(dst.pixels.data() + dstOffset, src.pixels.data() + srcOffset, rowBytes);
    }
}

Image ExtractRegion(const Image& image, const IntRect& rect)
{
    const IntRect clipped = ClampRectToImage(image, rect);
    if (clipped.width <= 0 || clipped.height <= 0)
    {
        return {};
    }

    Image region;
    region.width = clipped.width;
    region.height = clipped.height;
    region.pixels.resize(static_cast<std::size_t>(region.width * region.height * 4), 0);

    for (int row = 0; row < clipped.height; ++row)
    {
        const std::size_t srcOffset = static_cast<std::size_t>((clipped.y + row) * image.width + clipped.x) * 4;
        const std::size_t dstOffset = static_cast<std::size_t>(row * region.width) * 4;
        const std::size_t rowBytes = static_cast<std::size_t>(clipped.width) * 4;
        std::memcpy(region.pixels.data() + dstOffset, image.pixels.data() + srcOffset, rowBytes);
    }

    return region;
}

std::array<Vertex, 6> MakeFullscreenQuad()
{
    return {{
        {{-1.0F, 1.0F}, {1.0F, 1.0F, 1.0F}, {0.0F, 0.0F}},
        {{-1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}, {0.0F, 1.0F}},
        {{1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}, {1.0F, 1.0F}},
        {{-1.0F, 1.0F}, {1.0F, 1.0F, 1.0F}, {0.0F, 0.0F}},
        {{1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}, {1.0F, 1.0F}},
        {{1.0F, 1.0F}, {1.0F, 1.0F, 1.0F}, {1.0F, 0.0F}},
    }};
}

const FontResource& GlobalFont()
{
    static FontResource font = [] {
        FontResource result;
        try
        {
            std::vector<bk::Buffer> fontData = bk::LoadPlatformFonts();
            result.faces.reserve(fontData.size());

            for (auto& data : fontData)
            {
                if (data.empty())
                {
                    continue;
                }

                FontResource::Face face;
                face.data = std::move(data);
                if (stbtt_InitFont(&face.info, face.data.data(), stbtt_GetFontOffsetForIndex(face.data.data(), 0)) == 0)
                {
                    continue;
                }

                result.faces.push_back(std::move(face));
            }
        }
        catch (const std::exception& ex)
        {
            std::fprintf(stderr, "Failed to load platform fonts: %s\n", ex.what());
        }

        return result;
    }();

    return font;
}

std::u32string Utf8ToUtf32(const std::string& text)
{
    std::u32string result;
    result.reserve(text.size());

    std::size_t index = 0;
    while (index < text.size())
    {
        const unsigned char lead = static_cast<unsigned char>(text[index]);
        if (lead < 0x80)
        {
            result.push_back(static_cast<char32_t>(lead));
            ++index;
        }
        else if ((lead & 0xE0) == 0xC0 && index + 1 < text.size())
        {
            result.push_back(
                (static_cast<char32_t>(lead & 0x1F) << 6) |
                static_cast<char32_t>(static_cast<unsigned char>(text[index + 1]) & 0x3F));
            index += 2;
        }
        else if ((lead & 0xF0) == 0xE0 && index + 2 < text.size())
        {
            result.push_back(
                (static_cast<char32_t>(lead & 0x0F) << 12) |
                (static_cast<char32_t>(static_cast<unsigned char>(text[index + 1]) & 0x3F) << 6) |
                static_cast<char32_t>(static_cast<unsigned char>(text[index + 2]) & 0x3F));
            index += 3;
        }
        else if ((lead & 0xF8) == 0xF0 && index + 3 < text.size())
        {
            result.push_back(
                (static_cast<char32_t>(lead & 0x07) << 18) |
                (static_cast<char32_t>(static_cast<unsigned char>(text[index + 1]) & 0x3F) << 12) |
                (static_cast<char32_t>(static_cast<unsigned char>(text[index + 2]) & 0x3F) << 6) |
                static_cast<char32_t>(static_cast<unsigned char>(text[index + 3]) & 0x3F));
            index += 4;
        }
        else
        {
            result.push_back(U'?');
            ++index;
        }
    }

    return result;
}

std::uint8_t ToByte(float value)
{
    return static_cast<std::uint8_t>(std::clamp(value, 0.0F, 1.0F) * 255.0F + 0.5F);
}

std::string FormatFloat(float value, int precision)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

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
    const IntRect* clipRect)
{
    const FontResource::Face* primaryFace = font.PrimaryFace();
    if (primaryFace == nullptr)
    {
        return;
    }

    const float primaryScale = stbtt_ScaleForPixelHeight(&primaryFace->info, pixelHeight);
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&primaryFace->info, &ascent, &descent, &lineGap);
    const int baseline = y + static_cast<int>(ascent * primaryScale);
    int cursor = x;

    for (char32_t codepoint : text)
    {
        const FontResource::Face* face = SelectFontFace(font, codepoint);
        if (face == nullptr)
        {
            continue;
        }

        const GlyphBitmap& glyph = GetGlyphBitmap(*face, codepoint, pixelHeight);
        if (glyph.width > 0 && glyph.height > 0)
        {
            for (int yy = 0; yy < glyph.height; ++yy)
            {
                for (int xx = 0; xx < glyph.width; ++xx)
                {
                    const std::uint8_t alpha = glyph.pixels[static_cast<std::size_t>(yy * glyph.width + xx)];
                    if (alpha != 0)
                    {
                        const int pixelX = cursor + glyph.x0 + xx;
                        const int pixelY = baseline + glyph.y0 + yy;
                        if (PointInClip(clipRect, pixelX, pixelY))
                        {
                            BlendPixel(dst, pixelX, pixelY, r, g, b, alpha);
                        }
                    }
                }
            }
        }

        cursor += glyph.advance;
    }
}

void RenderQueueToImage(Image& image, const FontResource& font, const bk::RenderQueue& queue)
{
    const IntRect imageClip{0, 0, image.width, image.height};
    std::vector<IntRect> clipStack{imageClip};

    for (const bk::RenderCommand& command : queue.Commands())
    {
        if (command.type == bk::RenderCommandType::PushClip)
        {
            clipStack.push_back(IntersectRect(clipStack.back(), ClampRectToImage(image, CommandBoundsToIntRect(command))));
            continue;
        }
        if (command.type == bk::RenderCommandType::PopClip)
        {
            if (clipStack.size() > 1)
            {
                clipStack.pop_back();
            }
            continue;
        }

        const IntRect currentClip = clipStack.back();
        if (currentClip.width <= 0 || currentClip.height <= 0)
        {
            continue;
        }

        const int x = static_cast<int>(std::round(command.bounds.x));
        const int y = static_cast<int>(std::round(command.bounds.y));
        const int width = static_cast<int>(std::round(command.bounds.width));
        const int height = static_cast<int>(std::round(command.bounds.height));
        const std::uint8_t r = ToByte(command.color.r);
        const std::uint8_t g = ToByte(command.color.g);
        const std::uint8_t b = ToByte(command.color.b);
        const std::uint8_t a = ToByte(command.color.a);

        if (command.type == bk::RenderCommandType::Rect)
        {
            const IntRect clippedRect = IntersectRect(currentClip, IntRect{x, y, width, height});
            if (clippedRect.width > 0 && clippedRect.height > 0)
            {
                FillRect(image, clippedRect.x, clippedRect.y, clippedRect.width, clippedRect.height, r, g, b, a);
            }
        }
        else if (command.type == bk::RenderCommandType::Text)
        {
            DrawText(image, font, Utf8ToUtf32(command.text), x, y, command.fontSize, r, g, b, &currentClip);
        }
        else if (command.type == bk::RenderCommandType::Line)
        {
            DrawLine(
                image,
                static_cast<int>(std::round(command.lineStart.x)),
                static_cast<int>(std::round(command.lineStart.y)),
                static_cast<int>(std::round(command.lineEnd.x)),
                static_cast<int>(std::round(command.lineEnd.y)),
                r,
                g,
                b,
                a,
                command.lineThickness,
                &currentClip);
        }
    }
}

void RenderQueueToImage(Image& image, const FontResource& font, const bk::RenderQueue& queue, const IntRect& clipRect)
{
    const IntRect initialClip = ClampRectToImage(image, clipRect);
    if (initialClip.width <= 0 || initialClip.height <= 0)
    {
        return;
    }

    std::vector<IntRect> clipStack{initialClip};

    for (const bk::RenderCommand& command : queue.Commands())
    {
        if (command.type == bk::RenderCommandType::PushClip)
        {
            clipStack.push_back(IntersectRect(clipStack.back(), ClampRectToImage(image, CommandBoundsToIntRect(command))));
            continue;
        }
        if (command.type == bk::RenderCommandType::PopClip)
        {
            if (clipStack.size() > 1)
            {
                clipStack.pop_back();
            }
            continue;
        }

        const IntRect currentClip = clipStack.back();
        if (currentClip.width <= 0 || currentClip.height <= 0)
        {
            continue;
        }

        const IntRect commandRect = CommandBoundsToIntRect(command);
        const bool intersects =
            command.type == bk::RenderCommandType::Line ||
            (commandRect.x < currentClip.x + currentClip.width &&
            commandRect.x + commandRect.width > currentClip.x &&
            commandRect.y < currentClip.y + currentClip.height &&
            commandRect.y + commandRect.height > currentClip.y);
        if (!intersects)
        {
            continue;
        }

        const int x = static_cast<int>(std::round(command.bounds.x));
        const int y = static_cast<int>(std::round(command.bounds.y));
        const int width = static_cast<int>(std::round(command.bounds.width));
        const int height = static_cast<int>(std::round(command.bounds.height));
        const std::uint8_t r = ToByte(command.color.r);
        const std::uint8_t g = ToByte(command.color.g);
        const std::uint8_t b = ToByte(command.color.b);
        const std::uint8_t a = ToByte(command.color.a);

        if (command.type == bk::RenderCommandType::Rect)
        {
            const IntRect clippedRect = IntersectRect(currentClip, IntRect{x, y, width, height});
            if (clippedRect.width > 0 && clippedRect.height > 0)
            {
                FillRect(image, clippedRect.x, clippedRect.y, clippedRect.width, clippedRect.height, r, g, b, a);
            }
        }
        else if (command.type == bk::RenderCommandType::Text)
        {
            DrawText(image, font, Utf8ToUtf32(command.text), x, y, command.fontSize, r, g, b, &currentClip);
        }
        else if (command.type == bk::RenderCommandType::Line)
        {
            DrawLine(
                image,
                static_cast<int>(std::round(command.lineStart.x)),
                static_cast<int>(std::round(command.lineStart.y)),
                static_cast<int>(std::round(command.lineEnd.x)),
                static_cast<int>(std::round(command.lineEnd.y)),
                r,
                g,
                b,
                a,
                command.lineThickness,
                &currentClip);
        }
    }
}

bk::Vector2 EnsureWindowSize(const bk::Vector2& size)
{
    return bk::Vector2{
        size.x > 0.0F ? size.x : 1280.0F,
        size.y > 0.0F ? size.y : 720.0F,
    };
}

}
