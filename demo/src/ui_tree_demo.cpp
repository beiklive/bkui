#include <bkui/bkui.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <exception>
#include <iomanip>
#include <memory>
#include <thread>
#include <unordered_map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
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

void PutPixel(Image& image, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
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

void BlendPixel(Image& image, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
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

void FillRect(Image& image, int x, int y, int width, int height, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    for (int yy = y; yy < y + height; ++yy)
    {
        for (int xx = x; xx < x + width; ++xx)
        {
            PutPixel(image, xx, yy, r, g, b, a);
        }
    }
}

Image MakeImage(int width, int height, std::uint8_t r = 0, std::uint8_t g = 0, std::uint8_t b = 0, std::uint8_t a = 0)
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

void ClearImage(Image& image, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
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

std::string FormatFloat(float value, int precision = 1)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
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

void DrawText(
    Image& dst,
    const FontResource& font,
    const std::u32string& text,
    int x,
    int y,
    float pixelHeight,
    std::uint8_t r,
    std::uint8_t g,
    std::uint8_t b)
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
                        BlendPixel(dst, cursor + glyph.x0 + xx, baseline + glyph.y0 + yy, r, g, b, alpha);
                    }
                }
            }
        }

        cursor += glyph.advance;
    }
}

void RenderQueueToImage(Image& image, const FontResource& font, const bk::RenderQueue& queue)
{
    for (const bk::RenderCommand& command : queue.Commands())
    {
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
            FillRect(image, x, y, width, height, r, g, b, a);
        }
        else if (command.type == bk::RenderCommandType::Text)
        {
            DrawText(image, font, Utf8ToUtf32(command.text), x, y, command.fontSize, r, g, b);
        }
    }
}

void RenderQueueToImage(Image& image, const FontResource& font, const bk::RenderQueue& queue, const IntRect& clipRect)
{
    const IntRect clipped = ClampRectToImage(image, clipRect);
    if (clipped.width <= 0 || clipped.height <= 0)
    {
        return;
    }

    for (const bk::RenderCommand& command : queue.Commands())
    {
        const IntRect commandRect{
            static_cast<int>(std::floor(command.bounds.x)),
            static_cast<int>(std::floor(command.bounds.y)),
            static_cast<int>(std::ceil(command.bounds.width)),
            static_cast<int>(std::ceil(command.bounds.height)),
        };
        const bool intersects =
            commandRect.x < clipped.x + clipped.width &&
            commandRect.x + commandRect.width > clipped.x &&
            commandRect.y < clipped.y + clipped.height &&
            commandRect.y + commandRect.height > clipped.y;
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
            FillRect(image, x, y, width, height, r, g, b, a);
        }
        else if (command.type == bk::RenderCommandType::Text)
        {
            DrawText(image, font, Utf8ToUtf32(command.text), x, y, command.fontSize, r, g, b);
        }
    }
}

class MovingBox final : public bk::Box
{
public:
    MovingBox(std::string title, const bk::Color& backgroundColor, bk::Vector2 size, bk::Vector2 velocity, const bk::Color& textColor)
        : bk::Box(bk::BoxDirection::Vertical)
        , title_(std::move(title))
        , size_(size)
        , velocity_(velocity)
        , textColor_(textColor)
    {
        SetName(title_);
        SetDrawBackground(true);
        SetBackgroundColor(backgroundColor);
    }

    void SetMotionBounds(const bk::Rect& bounds)
    {
        motionBounds_ = bounds;
        ClampIntoBounds();
    }

    [[nodiscard]] bk::Vector2 GetVelocity() const
    {
        return velocity_;
    }

    [[nodiscard]] bk::Vector2 GetPosition() const
    {
        return bk::Vector2{GetFrame().x, GetFrame().y};
    }

    [[nodiscard]] bk::Vector2 GetSize() const
    {
        return size_;
    }

    [[nodiscard]] const std::string& GetTitle() const
    {
        return title_;
    }

    [[nodiscard]] IntRect GetPixelBounds() const
    {
        const bk::Rect& frame = GetFrame();
        return IntRect{
            static_cast<int>(std::floor(frame.x)) - 2,
            static_cast<int>(std::floor(frame.y)) - 2,
            static_cast<int>(std::ceil(frame.width)) + 4,
            static_cast<int>(std::ceil(frame.height)) + 4,
        };
    }

    bk::Size Measure(const bk::Size& available) const override
    {
        return bk::Size{
            std::min(available.width, size_.x),
            std::min(available.height, size_.y),
        };
    }

protected:
    void Update(float deltaSeconds) override
    {
        bk::Rect frame = GetFrame();
        frame.x += velocity_.x * deltaSeconds;
        frame.y += velocity_.y * deltaSeconds;

        bool hitX = false;
        bool hitY = false;

        if (frame.x < motionBounds_.x)
        {
            frame.x = motionBounds_.x;
            velocity_.x = std::fabs(velocity_.x);
            hitX = true;
        }
        else if (frame.x + frame.width > motionBounds_.x + motionBounds_.width)
        {
            frame.x = motionBounds_.x + motionBounds_.width - frame.width;
            velocity_.x = -std::fabs(velocity_.x);
            hitX = true;
        }

        if (frame.y < motionBounds_.y)
        {
            frame.y = motionBounds_.y;
            velocity_.y = std::fabs(velocity_.y);
            hitY = true;
        }
        else if (frame.y + frame.height > motionBounds_.y + motionBounds_.height)
        {
            frame.y = motionBounds_.y + motionBounds_.height - frame.height;
            velocity_.y = -std::fabs(velocity_.y);
            hitY = true;
        }

        if (hitX || hitY)
        {
            std::ostringstream stream;
            stream << title_ << " bounced at (" << FormatFloat(frame.x) << ", " << FormatFloat(frame.y) << ")";
            bk::Logger::instance().Debug(stream.str());
        }

        SetFrame(frame);
    }

    void Draw(bk::RenderQueue& queue) const override
    {
        bk::Box::Draw(queue);

        const bk::Rect& frame = GetFrame();
        queue.PushRect(
            bk::Rect{frame.x + 6.0F, frame.y + 6.0F, std::max(0.0F, frame.width - 12.0F), 4.0F},
            bk::Color{1.0F, 1.0F, 1.0F, 0.22F});
        queue.PushText(
            bk::Rect{frame.x + 16.0F, frame.y + 18.0F, std::max(0.0F, frame.width - 32.0F), std::max(0.0F, frame.height - 36.0F)},
            title_,
            22.0F,
            textColor_);
    }

private:
    void ClampIntoBounds()
    {
        bk::Rect frame = GetFrame();
        frame.width = size_.x;
        frame.height = size_.y;
        frame.x = std::clamp(frame.x, motionBounds_.x, motionBounds_.x + std::max(0.0F, motionBounds_.width - frame.width));
        frame.y = std::clamp(frame.y, motionBounds_.y, motionBounds_.y + std::max(0.0F, motionBounds_.height - frame.height));
        SetFrame(frame);
    }

    std::string title_;
    bk::Vector2 size_{};
    bk::Vector2 velocity_{};
    bk::Color textColor_{1.0F, 1.0F, 1.0F, 1.0F};
    bk::Rect motionBounds_{};
};

class StageView final : public bk::View
{
public:
    StageView()
    {
        SetName("StageView");

        firstBox_ = std::make_shared<MovingBox>(
            "Box A",
            bk::Color{0.19F, 0.52F, 0.96F, 1.0F},
            bk::Vector2{180.0F, 120.0F},
            bk::Vector2{240.0F, 180.0F},
            bk::Color{1.0F, 1.0F, 1.0F, 1.0F});
        secondBox_ = std::make_shared<MovingBox>(
            "Box B",
            bk::Color{0.98F, 0.52F, 0.24F, 1.0F},
            bk::Vector2{220.0F, 140.0F},
            bk::Vector2{-210.0F, 150.0F},
            bk::Color{0.08F, 0.12F, 0.18F, 1.0F});

        AddChild(firstBox_);
        AddChild(secondBox_);
    }

    [[nodiscard]] const MovingBox& GetFirstBox() const
    {
        return *firstBox_;
    }

    [[nodiscard]] const MovingBox& GetSecondBox() const
    {
        return *secondBox_;
    }

    void DrawStatic(bk::RenderQueue& queue) const
    {
        DrawStaticSelf(queue);
    }

    void Layout() override
    {
        const float inset = 18.0F;
        const bk::Rect motionBounds{
            frame_.x + inset,
            frame_.y + 54.0F,
            std::max(0.0F, frame_.width - inset * 2.0F),
            std::max(0.0F, frame_.height - 72.0F),
        };

        firstBox_->SetMotionBounds(motionBounds);
        secondBox_->SetMotionBounds(motionBounds);

        if (!initialized_)
        {
            const bk::Vector2 firstSize = firstBox_->GetSize();
            const bk::Vector2 secondSize = secondBox_->GetSize();
            firstBox_->SetFrame(bk::Rect{motionBounds.x + 24.0F, motionBounds.y + 24.0F, firstSize.x, firstSize.y});
            secondBox_->SetFrame(
                bk::Rect{
                    motionBounds.x + motionBounds.width - secondSize.x - 42.0F,
                    motionBounds.y + motionBounds.height - secondSize.y - 28.0F,
                    secondSize.x,
                    secondSize.y});
            initialized_ = true;
        }

        needsLayout_ = false;
    }

protected:
    void Draw(bk::RenderQueue& queue) const override
    {
        DrawStaticSelf(queue);
    }

private:
    void DrawStaticSelf(bk::RenderQueue& queue) const
    {
        queue.PushRect(frame_, bk::Color{0.08F, 0.11F, 0.16F, 1.0F});
        queue.PushRect(
            bk::Rect{frame_.x + 1.0F, frame_.y + 1.0F, std::max(0.0F, frame_.width - 2.0F), std::max(0.0F, frame_.height - 2.0F)},
            bk::Color{0.11F, 0.15F, 0.22F, 1.0F});
        queue.PushRect(
            bk::Rect{frame_.x, frame_.y, frame_.width, 44.0F},
            bk::Color{0.15F, 0.20F, 0.29F, 1.0F});
        queue.PushText(
            bk::Rect{frame_.x + 20.0F, frame_.y + 12.0F, frame_.width - 40.0F, 24.0F},
            "Stage / Moving Boxes",
            24.0F,
            bk::Color{0.96F, 0.97F, 0.99F, 1.0F});
    }
    std::shared_ptr<MovingBox> firstBox_;
    std::shared_ptr<MovingBox> secondBox_;
    bool initialized_ = false;
};

class DemoPage final : public bk::View
{
public:
    static constexpr int kStatusPanelHeight = 180;

    DemoPage()
    {
        SetName("DemoPage");

        infoPanel_ = std::make_shared<bk::VBox>();
        infoPanel_->SetName("InfoPanel");
        infoPanel_->SetSpacing(12.0F);
        infoPanel_->SetDrawBackground(true);
        infoPanel_->SetBackgroundColor(bk::Color{0.96F, 0.97F, 0.99F, 1.0F});

        titleLabel_ = std::make_shared<bk::Label>("BeikUI Main Loop Demo");
        titleLabel_->SetFontSize(30.0F);
        titleLabel_->SetTextColor(bk::Color{0.09F, 0.12F, 0.18F, 1.0F});

        subtitleLabel_ = std::make_shared<bk::Label>("Application / Logger / Frame / Draw / Page Tree");
        subtitleLabel_->SetFontSize(18.0F);
        subtitleLabel_->SetTextColor(bk::Color{0.36F, 0.42F, 0.49F, 1.0F});

        frameLabel_ = std::make_shared<bk::Label>("Frame: 0");
        frameLabel_->SetFontSize(20.0F);
        frameLabel_->SetTextColor(bk::Color{0.12F, 0.18F, 0.26F, 1.0F});

        inputLabel_ = std::make_shared<bk::Label>("Input: idle");
        inputLabel_->SetFontSize(18.0F);
        inputLabel_->SetTextColor(bk::Color{0.21F, 0.26F, 0.32F, 1.0F});

        firstBoxLabel_ = std::make_shared<bk::Label>("Box A");
        firstBoxLabel_->SetFontSize(18.0F);
        firstBoxLabel_->SetTextColor(bk::Color{0.19F, 0.32F, 0.55F, 1.0F});

        secondBoxLabel_ = std::make_shared<bk::Label>("Box B");
        secondBoxLabel_->SetFontSize(18.0F);
        secondBoxLabel_->SetTextColor(bk::Color{0.55F, 0.28F, 0.12F, 1.0F});

        pipelineButton_ = std::make_shared<bk::Button>("Application::RunFrame()");
        pipelineButton_->SetFontSize(18.0F);
        pipelineButton_->SetBackgroundColor(bk::Color{0.18F, 0.45F, 0.90F, 1.0F});

        loggerButton_ = std::make_shared<bk::Button>("Logger -> console + file");
        loggerButton_->SetFontSize(18.0F);
        loggerButton_->SetBackgroundColor(bk::Color{0.13F, 0.63F, 0.55F, 1.0F});

        infoPanel_->AddChild(titleLabel_);
        infoPanel_->AddChild(subtitleLabel_);
        infoPanel_->AddChild(frameLabel_);
        infoPanel_->AddChild(inputLabel_);
        infoPanel_->AddChild(firstBoxLabel_);
        infoPanel_->AddChild(secondBoxLabel_);
        infoPanel_->AddChild(pipelineButton_);
        infoPanel_->AddChild(loggerButton_);

        stageView_ = std::make_shared<StageView>();

        AddChild(infoPanel_);
        AddChild(stageView_);
    }

    void SyncStatus(const bk::Application& app, const bk::InputState& input, const std::string& backendName)
    {
        frameLabel_->SetText(
            "Frame: " + std::to_string(app.GetFrameIndex()) +
            "  Queue: " + std::to_string(app.GetRenderQueue().Commands().size()));

        const std::string activeInput =
            input.touchCount > 0 ? "touch" :
            (input.mouseLeftDown ? "mouse-left" :
            (input.keyPressed || input.keyReleased ? input.lastKeyEvent.name : "idle"));
        inputLabel_->SetText(
            "Backend: " + backendName +
            "  Input: " + activeInput +
            "  Pointer: (" + FormatFloat(input.mousePosition.x) + ", " + FormatFloat(input.mousePosition.y) + ")");

        const MovingBox& firstBox = stageView_->GetFirstBox();
        const MovingBox& secondBox = stageView_->GetSecondBox();
        firstBoxLabel_->SetText(
            firstBox.GetTitle() +
            "  pos=(" + FormatFloat(firstBox.GetPosition().x) + ", " + FormatFloat(firstBox.GetPosition().y) + ")" +
            "  vel=(" + FormatFloat(firstBox.GetVelocity().x, 0) + ", " + FormatFloat(firstBox.GetVelocity().y, 0) + ")");
        secondBoxLabel_->SetText(
            secondBox.GetTitle() +
            "  pos=(" + FormatFloat(secondBox.GetPosition().x) + ", " + FormatFloat(secondBox.GetPosition().y) + ")" +
            "  vel=(" + FormatFloat(secondBox.GetVelocity().x, 0) + ", " + FormatFloat(secondBox.GetVelocity().y, 0) + ")");
    }

    [[nodiscard]] IntRect GetStatusPanelRect() const
    {
        const bk::Rect& frame = infoPanel_->GetFrame();
        return IntRect{
            static_cast<int>(std::floor(frame.x)) - 4,
            static_cast<int>(std::floor(frame.y)) - 4,
            static_cast<int>(std::ceil(frame.width)) + 8,
            static_cast<int>(std::ceil(frame.height)) + 8,
        };
    }

    [[nodiscard]] IntRect GetMovingRegionRect() const
    {
        return UnionRect(stageView_->GetFirstBox().GetPixelBounds(), stageView_->GetSecondBox().GetPixelBounds());
    }

    void DrawStatic(bk::RenderQueue& queue) const
    {
        Draw(queue);
        queue.PushRect(infoPanel_->GetFrame(), bk::Color{0.96F, 0.97F, 0.99F, 1.0F});
        stageView_->DrawStatic(queue);
    }

    void Layout() override
    {
        const float margin = 28.0F;
        const float panelWidth = std::min(400.0F, std::max(320.0F, frame_.width * 0.30F));
        const float stageX = margin + panelWidth + 24.0F;
        const float stageWidth = std::max(280.0F, frame_.width - stageX - margin);

        infoPanel_->SetFrame(bk::Rect{
            frame_.x + margin,
            frame_.y + margin,
            panelWidth,
            static_cast<float>(kStatusPanelHeight),
        });
        stageView_->SetFrame(bk::Rect{
            frame_.x + stageX,
            frame_.y + margin,
            stageWidth,
            std::max(240.0F, frame_.height - margin * 2.0F),
        });

        infoPanel_->Layout();
        stageView_->Layout();
        needsLayout_ = false;
    }

protected:
    void Draw(bk::RenderQueue& queue) const override
    {
        queue.PushRect(frame_, bk::Color{0.05F, 0.07F, 0.10F, 1.0F});
        queue.PushRect(
            bk::Rect{frame_.x, frame_.y, frame_.width, 6.0F},
            bk::Color{0.15F, 0.53F, 0.97F, 1.0F});
    }

private:
    std::shared_ptr<bk::VBox> infoPanel_;
    std::shared_ptr<bk::Label> titleLabel_;
    std::shared_ptr<bk::Label> subtitleLabel_;
    std::shared_ptr<bk::Label> frameLabel_;
    std::shared_ptr<bk::Label> inputLabel_;
    std::shared_ptr<bk::Label> firstBoxLabel_;
    std::shared_ptr<bk::Label> secondBoxLabel_;
    std::shared_ptr<bk::Button> pipelineButton_;
    std::shared_ptr<bk::Button> loggerButton_;
    std::shared_ptr<StageView> stageView_;
};

bk::Vector2 EnsureWindowSize(const bk::Vector2& size)
{
    return bk::Vector2{
        size.x > 0.0F ? size.x : 1280.0F,
        size.y > 0.0F ? size.y : 720.0F,
    };
}

}

int main(int argc, char** argv)
{
    try
    {
        bk::FileSystem::Init(argc > 0 ? argv[0] : nullptr);
        bk::FileSystem::Mount("resources");

        auto platform = bk::CreateDefaultPlatform(bk::WindowDesc{
            "BeikUI Main Loop Demo",
            1280,
            720,
        });

        if (!platform->Init())
        {
            std::fprintf(stderr, "Failed to initialize platform.\n");
            return 1;
        }

        auto device =
#if defined(BKUI_PLATFORM_SWITCH)
            bk::CreateDeko3DDevice(*platform);
#else
            bk::CreateOpenGLDevice(*platform);
#endif

        if (!device->Init())
        {
            std::fprintf(stderr, "Graphics backend unavailable: %s\n", device->BackendName());
            return 1;
        }

        bk::Application app;
        bk::ApplicationDesc appDesc;
        appDesc.name = "BeikUI Demo";
        appDesc.version = "0.2.0";
        appDesc.identifier = "bkui.demo.mainloop";
        appDesc.logger.level = bk::LogLevel::Debug;
        appDesc.logger.enableConsole = true;
        appDesc.logger.enableColor = true;
        appDesc.logger.filePath = "bkui_demo.log";

        if (!app.Initialize(appDesc, argc, const_cast<const char* const*>(argv)))
        {
            std::fprintf(stderr, "Failed to initialize application context.\n");
            return 1;
        }

        bk::Logger::instance().Info(std::string("Graphics backend: ") + device->BackendName());

        const auto frameHeartbeat = bk::ScopedEventConnection(
            app.OnFrameBegin().Connect(
                [](bk::Application&, float deltaSeconds, std::uint64_t frameIndex) {
                    if (frameIndex % 180 == 0)
                    {
                        std::ostringstream stream;
                        stream << "Frame heartbeat #" << frameIndex << " delta=" << FormatFloat(deltaSeconds, 3);
                        bk::Logger::instance().Info(stream.str());
                    }
                }));

        const auto quitLog = bk::ScopedEventConnection(
            app.OnQuitRequested().Connect(
                [](bk::Application&) {
                    bk::Logger::instance().Info("Quit requested.");
                }));

        auto page = std::make_shared<DemoPage>();
        bk::Vector2 windowSize = EnsureWindowSize(platform->GetWindowSize());
        page->SetFrame(bk::Rect{0.0F, 0.0F, windowSize.x, windowSize.y});
        app.AddView(page);

        bk::RenderQueueRenderer renderer(*device);
        if (!renderer.Initialize())
        {
            bk::Logger::instance().Error("Failed to initialize RenderQueueRenderer.");
            return 1;
        }

        bk::CommandBufferHandle commandBuffer = device->CreateCommandBuffer();

        if (!bk::IsValid(commandBuffer))
        {
            bk::Logger::instance().Error("Failed to create demo GPU resources.");
            return 1;
        }

        using Clock = std::chrono::steady_clock;

        const float fixedDeltaSeconds = 1.0F / 60.0F;
        const auto frameDuration = std::chrono::duration<double>(fixedDeltaSeconds);
        bk::SwapchainHandle swapchain = device->GetMainSwapchain();
        auto nextFrameTime = Clock::now();

        while (platform->IsRunning() && app.IsRunning())
        {
            std::this_thread::sleep_until(nextFrameTime);
            nextFrameTime += std::chrono::duration_cast<Clock::duration>(frameDuration);

            platform->PollEvents();
            const bk::InputState input = platform->GetInput();
            if (input.quitRequested || input.cancelPressed)
            {
                app.RequestQuit();
            }

            const bk::Vector2 latestWindowSize = EnsureWindowSize(platform->GetWindowSize());
            if (latestWindowSize.x != windowSize.x || latestWindowSize.y != windowSize.y)
            {
                windowSize = latestWindowSize;
                page->SetFrame(bk::Rect{0.0F, 0.0F, windowSize.x, windowSize.y});
                device->Resize(windowSize);

                std::ostringstream stream;
                stream << "Window resized to " << windowSize.x << "x" << windowSize.y;
                bk::Logger::instance().Info(stream.str());
                nextFrameTime = Clock::now();
            }
            else if (input.windowResized)
            {
                device->Resize(windowSize);
            }

            page->SyncStatus(app, input, device->BackendName());
            app.SetInputState(input);
            if (!app.RunFrame(fixedDeltaSeconds, true))
            {
                break;
            }

            device->BeginFrame(swapchain, bk::RenderPassDesc{bk::Color{0.04F, 0.05F, 0.08F, 1.0F}});
            device->BeginCommandBuffer(commandBuffer);
            if (!renderer.Render(commandBuffer, app.GetRenderQueue(), windowSize))
            {
                bk::Logger::instance().Error("Failed to record UI draw commands.");
                break;
            }
            device->EndCommandBuffer(commandBuffer);
            device->Submit(commandBuffer);
            device->EndFrame(swapchain);
        }

        device->DestroyCommandBuffer(commandBuffer);
        renderer.Shutdown();

        app.Shutdown();
        device->Shutdown();
        platform->Shutdown();
        bk::FileSystem::Shutdown();
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "Fatal error: %s\n", ex.what());
        return 1;
    }

    return 0;
}
