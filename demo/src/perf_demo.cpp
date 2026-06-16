#include <bkui/bkui.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iomanip>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
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

enum class RenderMode
{
    FullRebuild,
    DirtyRegion,
};

struct PerfStats
{
    double fps = 0.0;
    double avgCpuMs = 0.0;
    double avgPresentMs = 0.0;
    std::uint64_t frameCount = 0;
    std::size_t boxCount = 0;
    std::size_t commandCount = 0;
    RenderMode mode = RenderMode::FullRebuild;
};

struct UiButtons
{
    bk::Rect addButton{};
    bk::Rect removeButton{};
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

Image MakeImage(int width, int height, std::uint8_t r = 0, std::uint8_t g = 0, std::uint8_t b = 0, std::uint8_t a = 255)
{
    Image image;
    image.width = width;
    image.height = height;
    image.pixels.resize(static_cast<std::size_t>(width * height * 4), 0);
    FillRect(image, 0, 0, width, height, r, g, b, a);
    return image;
}

void ClearImage(Image& image, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
{
    FillRect(image, 0, 0, image.width, image.height, r, g, b, a);
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
    Image& image,
    const FontResource& font,
    const std::string& text,
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

    for (char32_t codepoint : Utf8ToUtf32(text))
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
                        BlendPixel(image, cursor + glyph.x0 + xx, baseline + glyph.y0 + yy, r, g, b, alpha);
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
        const std::uint8_t r = static_cast<std::uint8_t>(std::clamp(command.color.r, 0.0F, 1.0F) * 255.0F);
        const std::uint8_t g = static_cast<std::uint8_t>(std::clamp(command.color.g, 0.0F, 1.0F) * 255.0F);
        const std::uint8_t b = static_cast<std::uint8_t>(std::clamp(command.color.b, 0.0F, 1.0F) * 255.0F);
        const std::uint8_t a = static_cast<std::uint8_t>(std::clamp(command.color.a, 0.0F, 1.0F) * 255.0F);

        if (command.type == bk::RenderCommandType::Rect)
        {
            FillRect(image, x, y, width, height, r, g, b, a);
        }
        else if (command.type == bk::RenderCommandType::Text)
        {
            DrawText(image, font, command.text, x, y, command.fontSize, r, g, b);
        }
    }
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

bk::Vector2 EnsureWindowSize(const bk::Vector2& size)
{
    return bk::Vector2{
        size.x > 0.0F ? size.x : 1280.0F,
        size.y > 0.0F ? size.y : 720.0F,
    };
}

std::string ModeName(RenderMode mode)
{
    return mode == RenderMode::FullRebuild ? "Full Rebuild" : "Dirty Region";
}

bool PointInRect(float x, float y, const bk::Rect& rect)
{
    return x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height;
}

class PerfBox final : public bk::Box
{
public:
    PerfBox(std::string name, bk::Vector2 size, bk::Vector2 velocity, bk::Color color)
        : bk::Box(bk::BoxDirection::Vertical)
        , name_(std::move(name))
        , size_(size)
        , velocity_(velocity)
    {
        SetDrawBackground(true);
        SetBackgroundColor(color);
    }

    void SetBounds(const bk::Rect& bounds)
    {
        bounds_ = bounds;
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

        if (frame.x < bounds_.x)
        {
            frame.x = bounds_.x;
            velocity_.x = std::fabs(velocity_.x);
        }
        else if (frame.x + frame.width > bounds_.x + bounds_.width)
        {
            frame.x = bounds_.x + bounds_.width - frame.width;
            velocity_.x = -std::fabs(velocity_.x);
        }

        if (frame.y < bounds_.y)
        {
            frame.y = bounds_.y;
            velocity_.y = std::fabs(velocity_.y);
        }
        else if (frame.y + frame.height > bounds_.y + bounds_.height)
        {
            frame.y = bounds_.y + bounds_.height - frame.height;
            velocity_.y = -std::fabs(velocity_.y);
        }

        SetFrame(frame);
    }

    void Draw(bk::RenderQueue& queue) const override
    {
        bk::Box::Draw(queue);
        const bk::Rect& frame = GetFrame();
        queue.PushRect(
            bk::Rect{frame.x + 3.0F, frame.y + 3.0F, std::max(0.0F, frame.width - 6.0F), 3.0F},
            bk::Color{1.0F, 1.0F, 1.0F, 0.22F});
        queue.PushText(
            bk::Rect{frame.x + 8.0F, frame.y + 10.0F, std::max(0.0F, frame.width - 16.0F), std::max(0.0F, frame.height - 20.0F)},
            name_,
            12.0F,
            bk::Color{1.0F, 1.0F, 1.0F, 1.0F});
    }

private:
    std::string name_;
    bk::Vector2 size_{};
    bk::Vector2 velocity_{};
    bk::Rect bounds_{};
};

class PerfPage final : public bk::View
{
public:
    PerfPage()
    {
        SetName("PerfPage");
        RebuildBoxes(256);
    }

    void RebuildBoxes(std::size_t count)
    {
        ClearChildren();
        boxes_.clear();
        boxes_.reserve(count);

        std::mt19937 rng(1337 + static_cast<std::uint32_t>(count));
        std::uniform_real_distribution<float> sizeDist(18.0F, 52.0F);
        std::uniform_real_distribution<float> velocityDist(80.0F, 360.0F);
        std::uniform_real_distribution<float> colorDist(0.15F, 1.0F);

        for (std::size_t index = 0; index < count; ++index)
        {
            const float width = sizeDist(rng);
            const float height = sizeDist(rng);
            const float vx = velocityDist(rng) * ((index % 2) == 0 ? 1.0F : -1.0F);
            const float vy = velocityDist(rng) * ((index % 3) == 0 ? -1.0F : 1.0F);
            auto box = std::make_shared<PerfBox>(
                std::to_string(index),
                bk::Vector2{width, height},
                bk::Vector2{vx, vy},
                bk::Color{colorDist(rng), colorDist(rng), colorDist(rng), 1.0F});
            boxes_.push_back(box);
            AddChild(box);
        }

        initialized_ = false;
        InvalidateLayout();
    }

    void IncreaseBoxes(std::size_t delta)
    {
        RebuildBoxes(boxes_.size() + delta);
    }

    void DecreaseBoxes(std::size_t delta)
    {
        RebuildBoxes(boxes_.size() > delta ? boxes_.size() - delta : 1);
    }

    [[nodiscard]] std::size_t GetBoxCount() const
    {
        return boxes_.size();
    }

    void SetMode(RenderMode mode)
    {
        mode_ = mode;
    }

    void SetStats(const PerfStats& stats)
    {
        stats_ = stats;
    }

    void SetUiButtons(const UiButtons& buttons)
    {
        buttons_ = buttons;
    }

    void Layout() override
    {
        const float leftHudWidth = 360.0F;
        const float rightUiWidth = 190.0F;
        const bk::Rect motionBounds{
            frame_.x + leftHudWidth + 16.0F,
            frame_.y + 18.0F,
            std::max(0.0F, frame_.width - leftHudWidth - rightUiWidth - 36.0F),
            std::max(0.0F, frame_.height - 36.0F),
        };

        const int columns = std::max(1, static_cast<int>(std::sqrt(static_cast<float>(boxes_.size()))));
        const int rows = std::max(1, static_cast<int>((boxes_.size() + static_cast<std::size_t>(columns) - 1) / static_cast<std::size_t>(columns)));
        const float stepX = motionBounds.width / static_cast<float>(columns);
        const float stepY = motionBounds.height / static_cast<float>(rows);

        for (std::size_t index = 0; index < boxes_.size(); ++index)
        {
            const auto& box = boxes_[index];
            box->SetBounds(motionBounds);

            if (!initialized_)
            {
                const int column = static_cast<int>(index % static_cast<std::size_t>(columns));
                const int row = static_cast<int>(index / static_cast<std::size_t>(columns));
                const bk::Size size = box->Measure(bk::Size{motionBounds.width, motionBounds.height});
                box->SetFrame(bk::Rect{
                    motionBounds.x + column * stepX + 2.0F,
                    motionBounds.y + row * stepY + 2.0F,
                    size.width,
                    size.height,
                });
            }
        }

        initialized_ = true;
        needsLayout_ = false;
    }

protected:
    void Draw(bk::RenderQueue& queue) const override
    {
        queue.PushRect(frame_, bk::Color{0.04F, 0.05F, 0.08F, 1.0F});
        queue.PushRect(
            bk::Rect{frame_.x, frame_.y, frame_.width, 8.0F},
            bk::Color{0.16F, 0.56F, 0.98F, 1.0F});

        queue.PushRect(
            bk::Rect{18.0F, 18.0F, 332.0F, 170.0F},
            bk::Color{0.10F, 0.13F, 0.19F, 0.96F});
        queue.PushText(bk::Rect{32.0F, 30.0F, 300.0F, 28.0F}, "Performance HUD", 24.0F, bk::Color{0.96F, 0.97F, 0.99F, 1.0F});
        queue.PushText(bk::Rect{32.0F, 64.0F, 300.0F, 20.0F}, "Mode: " + ModeName(mode_), 18.0F, bk::Color{0.72F, 0.84F, 1.0F, 1.0F});
        queue.PushText(bk::Rect{32.0F, 88.0F, 300.0F, 20.0F}, "FPS: " + [&] { std::ostringstream s; s << std::fixed << std::setprecision(1) << stats_.fps; return s.str(); }(), 18.0F, bk::Color{0.84F, 0.95F, 0.84F, 1.0F});
        queue.PushText(bk::Rect{32.0F, 112.0F, 300.0F, 20.0F}, "CPU ms: " + [&] { std::ostringstream s; s << std::fixed << std::setprecision(2) << stats_.avgCpuMs; return s.str(); }(), 18.0F, bk::Color{0.97F, 0.88F, 0.72F, 1.0F});
        queue.PushText(bk::Rect{32.0F, 136.0F, 300.0F, 20.0F}, "Present ms: " + [&] { std::ostringstream s; s << std::fixed << std::setprecision(2) << stats_.avgPresentMs; return s.str(); }(), 18.0F, bk::Color{0.97F, 0.78F, 0.76F, 1.0F});
        queue.PushText(bk::Rect{32.0F, 160.0F, 300.0F, 20.0F}, "Boxes: " + std::to_string(stats_.boxCount) + "  Commands: " + std::to_string(stats_.commandCount), 18.0F, bk::Color{0.84F, 0.85F, 0.90F, 1.0F});

        queue.PushRect(
            bk::Rect{frame_.width - 174.0F, 22.0F, 150.0F, 56.0F},
            bk::Color{0.17F, 0.47F, 0.91F, 1.0F});
        queue.PushText(
            bk::Rect{frame_.width - 144.0F, 40.0F, 120.0F, 20.0F},
            "+ 10 Boxes",
            20.0F,
            bk::Color{1.0F, 1.0F, 1.0F, 1.0F});

        queue.PushRect(
            bk::Rect{frame_.width - 174.0F, 92.0F, 150.0F, 56.0F},
            bk::Color{0.89F, 0.40F, 0.24F, 1.0F});
        queue.PushText(
            bk::Rect{frame_.width - 147.0F, 110.0F, 120.0F, 20.0F},
            "- 10 Boxes",
            20.0F,
            bk::Color{1.0F, 1.0F, 1.0F, 1.0F});
    }

private:
    std::vector<std::shared_ptr<PerfBox>> boxes_;
    bool initialized_ = false;
    RenderMode mode_ = RenderMode::FullRebuild;
    PerfStats stats_{};
    UiButtons buttons_{};
};

}

int main(int argc, char** argv)
{
    try
    {
        bk::FileSystem::Init(argc > 0 ? argv[0] : nullptr);
        bk::FileSystem::Mount("resources");

        auto platform = bk::CreateDefaultPlatform(bk::WindowDesc{
            "BeikUI Perf Demo",
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
        appDesc.name = "BeikUI Perf Demo";
        appDesc.version = "0.2.0";
        appDesc.identifier = "bkui.demo.perf";
        appDesc.logger.level = bk::LogLevel::Info;
        appDesc.logger.enableConsole = true;
        appDesc.logger.enableColor = true;
        appDesc.logger.filePath = "bkui_perf_demo.log";
        if (!app.Initialize(appDesc, argc, const_cast<const char* const*>(argv)))
        {
            std::fprintf(stderr, "Failed to initialize application.\n");
            return 1;
        }

        auto page = std::make_shared<PerfPage>();
        bk::Vector2 windowSize = EnsureWindowSize(platform->GetWindowSize());
        page->SetFrame(bk::Rect{0.0F, 0.0F, windowSize.x, windowSize.y});
        app.AddView(page);

        RenderMode mode = RenderMode::FullRebuild;
        PerfStats stats{};
        stats.boxCount = page->GetBoxCount();
        stats.mode = mode;
        page->SetStats(stats);
        page->SetMode(mode);

        const std::array<Vertex, 6> quadVertices = MakeFullscreenQuad();
        const char* vertexShaderSource =
#if defined(BKUI_PLATFORM_SWITCH)
            "shaders/deko_triangle_vsh.dksh";
#else
            R"(
            #version 330 core
            layout(location = 0) in vec2 inPosition;
            layout(location = 1) in vec3 inColor;
            layout(location = 2) in vec2 inTexCoord;
            out vec3 vertexColor;
            out vec2 texCoord;
            void main()
            {
                vertexColor = inColor;
                texCoord = inTexCoord;
                gl_Position = vec4(inPosition, 0.0, 1.0);
            }
        )";
#endif
        const char* fragmentShaderSource =
#if defined(BKUI_PLATFORM_SWITCH)
            "shaders/deko_triangle_fsh.dksh";
#else
            R"(
            #version 330 core
            in vec3 vertexColor;
            in vec2 texCoord;
            uniform sampler2D uTexture;
            out vec4 outColor;
            void main()
            {
                outColor = texture(uTexture, texCoord) * vec4(vertexColor, 1.0);
            }
        )";
#endif

        bk::BufferHandle quadBuffer = device->CreateBuffer(bk::BufferDesc{
            bk::BufferUsage::Vertex,
            sizeof(Vertex) * quadVertices.size(),
            quadVertices.data(),
        });
        bk::ShaderHandle vertexShader = device->CreateShader(bk::ShaderDesc{bk::ShaderStage::Vertex, vertexShaderSource});
        bk::ShaderHandle fragmentShader = device->CreateShader(bk::ShaderDesc{bk::ShaderStage::Fragment, fragmentShaderSource});
        const std::array<bk::VertexAttributeDesc, 3> attributes = {{
            {0, bk::VertexFormat::Float2, offsetof(Vertex, position)},
            {1, bk::VertexFormat::Float3, offsetof(Vertex, color)},
            {2, bk::VertexFormat::Float2, offsetof(Vertex, uv)},
        }};
        bk::PipelineHandle pipeline = device->CreatePipeline(bk::PipelineDesc{
            vertexShader,
            fragmentShader,
            bk::VertexLayoutDesc{sizeof(Vertex), attributes.data(), attributes.size()},
            bk::PrimitiveTopology::Triangles,
        });
        bk::CommandBufferHandle commandBuffer = device->CreateCommandBuffer();

        const FontResource& font = GlobalFont();
        Image frameImage = MakeImage(static_cast<int>(windowSize.x), static_cast<int>(windowSize.y), 11, 16, 24, 255);
        bk::TextureHandle frameTexture = device->CreateTexture(bk::TextureDesc{
            static_cast<std::uint32_t>(frameImage.width),
            static_cast<std::uint32_t>(frameImage.height),
            frameImage.pixels.data(),
        });

        if (!bk::IsValid(quadBuffer) || !bk::IsValid(vertexShader) || !bk::IsValid(fragmentShader) ||
            !bk::IsValid(pipeline) || !bk::IsValid(commandBuffer) || !bk::IsValid(frameTexture))
        {
            bk::Logger::instance().Error("Failed to create GPU resources.");
            return 1;
        }

        using Clock = std::chrono::steady_clock;
        const float fixedDeltaSeconds = 1.0F / 60.0F;
        const auto frameDuration = std::chrono::duration<double>(fixedDeltaSeconds);
        auto nextFrameTime = Clock::now();
        auto statsWindowStart = Clock::now();
        std::uint64_t statsFrameCount = 0;
        double statsCpuMs = 0.0;
        double statsPresentMs = 0.0;
        bk::SwapchainHandle swapchain = device->GetMainSwapchain();

        bool togglePressed = false;
        bool addPressed = false;
        bool removePressed = false;

        while (platform->IsRunning() && app.IsRunning())
        {
            std::this_thread::sleep_until(nextFrameTime);
            nextFrameTime += std::chrono::duration_cast<Clock::duration>(frameDuration);

            const auto frameStart = Clock::now();
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
                frameImage = MakeImage(static_cast<int>(windowSize.x), static_cast<int>(windowSize.y), 11, 16, 24, 255);
                device->Resize(windowSize);
                device->DestroyTexture(frameTexture);
                frameTexture = device->CreateTexture(bk::TextureDesc{
                    static_cast<std::uint32_t>(frameImage.width),
                    static_cast<std::uint32_t>(frameImage.height),
                    frameImage.pixels.data(),
                });
                nextFrameTime = Clock::now();
            }
            else if (input.windowResized)
            {
                device->Resize(windowSize);
            }

            UiButtons buttons{
                bk::Rect{windowSize.x - 174.0F, 22.0F, 150.0F, 56.0F},
                bk::Rect{windowSize.x - 174.0F, 92.0F, 150.0F, 56.0F},
            };
            page->SetUiButtons(buttons);

            const bool toggleDown = std::find_if(
                input.keysDown.begin(),
                input.keysDown.begin() + input.keysDownCount,
                [](const bk::InputState::KeyEvent& key) {
                    return strcmp(key.name, "Tab") == 0;
                }) != input.keysDown.begin() + input.keysDownCount;
            if (toggleDown && !togglePressed)
            {
                mode = mode == RenderMode::FullRebuild ? RenderMode::DirtyRegion : RenderMode::FullRebuild;
                page->SetMode(mode);
                bk::Logger::instance().Info("Switched mode to " + ModeName(mode));
            }
            togglePressed = toggleDown;

            const bool touchPrimaryDown = input.touchCount > 0 && input.touchPoints[0].down;
            const bk::Vector2 pointerPosition = touchPrimaryDown ? input.touchPoints[0].position : input.mousePosition;
            const bool primaryDown = touchPrimaryDown || input.mouseLeftDown;

            const bool addDown = primaryDown &&
                PointInRect(pointerPosition.x, pointerPosition.y, buttons.addButton);
            if (addDown && !addPressed)
            {
                page->IncreaseBoxes(10);
            }
            addPressed = addDown;

            const bool removeDown = primaryDown &&
                PointInRect(pointerPosition.x, pointerPosition.y, buttons.removeButton);
            if (removeDown && !removePressed)
            {
                page->DecreaseBoxes(10);
            }
            removePressed = removeDown;

            stats.boxCount = page->GetBoxCount();
            stats.mode = mode;
            page->SetStats(stats);
            page->SetMode(mode);

            app.SetInputState(input);
            if (!app.RunFrame(fixedDeltaSeconds, true))
            {
                break;
            }

            ClearImage(frameImage, 11, 16, 24, 255);
            RenderQueueToImage(frameImage, font, app.GetRenderQueue());

            if (!device->UpdateTexture(frameTexture, bk::TextureDesc{
                static_cast<std::uint32_t>(frameImage.width),
                static_cast<std::uint32_t>(frameImage.height),
                frameImage.pixels.data(),
            }))
            {
                bk::Logger::instance().Error("Failed to update frame texture.");
                break;
            }

            const auto presentStart = Clock::now();
            device->BeginFrame(swapchain, bk::RenderPassDesc{bk::Color{0.03F, 0.04F, 0.06F, 1.0F}});
            device->BeginCommandBuffer(commandBuffer);
            device->BindPipeline(commandBuffer, pipeline);
            device->BindVertexBuffer(commandBuffer, quadBuffer);
            device->BindTexture(commandBuffer, frameTexture);
            device->Draw(commandBuffer, static_cast<std::uint32_t>(quadVertices.size()));
            device->EndCommandBuffer(commandBuffer);
            device->Submit(commandBuffer);
            device->EndFrame(swapchain);
            const auto frameEnd = Clock::now();

            statsCpuMs += std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
            statsPresentMs += std::chrono::duration<double, std::milli>(frameEnd - presentStart).count();
            ++statsFrameCount;

            const auto elapsed = std::chrono::duration<double>(frameEnd - statsWindowStart).count();
            if (elapsed >= 1.0)
            {
                stats.fps = static_cast<double>(statsFrameCount) / elapsed;
                stats.avgCpuMs = statsCpuMs / static_cast<double>(statsFrameCount);
                stats.avgPresentMs = statsPresentMs / static_cast<double>(statsFrameCount);
                stats.frameCount += statsFrameCount;
                stats.boxCount = page->GetBoxCount();
                stats.commandCount = app.GetRenderQueue().Commands().size();
                stats.mode = mode;
                page->SetStats(stats);

                std::ostringstream stream;
                stream << "perf mode=" << ModeName(mode)
                       << " fps=" << std::fixed << std::setprecision(1) << stats.fps
                       << " cpu_ms=" << std::setprecision(2) << stats.avgCpuMs
                       << " present_ms=" << stats.avgPresentMs
                       << " boxes=" << stats.boxCount
                       << " commands=" << stats.commandCount;
                bk::Logger::instance().Info(stream.str());

                statsWindowStart = frameEnd;
                statsFrameCount = 0;
                statsCpuMs = 0.0;
                statsPresentMs = 0.0;
            }
        }

        device->DestroyTexture(frameTexture);
        device->DestroyCommandBuffer(commandBuffer);
        device->DestroyPipeline(pipeline);
        device->DestroyShader(fragmentShader);
        device->DestroyShader(vertexShader);
        device->DestroyBuffer(quadBuffer);

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
