#include <bkui/render/RenderQueueRenderer.hpp>

#include <bkui/core/Logger.hpp>
#include <bkui/platform/Font.hpp>

#include <stb_truetype.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bk
{
namespace
{

#if defined(BKUI_PLATFORM_SWITCH)
constexpr std::uint32_t kAtlasWidth = 1024;
constexpr std::uint32_t kAtlasHeight = 1024;
#else
constexpr std::uint32_t kAtlasWidth = 2048;
constexpr std::uint32_t kAtlasHeight = 2048;
#endif
constexpr std::size_t kInitialVertexCapacity = 1024;
constexpr float kRoundedRectSegmentLength = 8.0F;

struct RectF
{
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
};

struct GpuVertex
{
    float position[2];
    float color[4];
    float uv[2];
};

struct FontFace
{
    Buffer data;
    stbtt_fontinfo info{};
};

struct GlyphKey
{
    std::uint32_t faceIndex = 0;
    char32_t codepoint = 0;
    int pixelHeightX10 = 0;

    bool operator==(const GlyphKey& other) const
    {
        return faceIndex == other.faceIndex &&
            codepoint == other.codepoint &&
            pixelHeightX10 == other.pixelHeightX10;
    }
};

struct GlyphKeyHasher
{
    std::size_t operator()(const GlyphKey& key) const noexcept
    {
        const std::size_t h1 = std::hash<std::uint32_t>{}(key.faceIndex);
        const std::size_t h2 = std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(key.codepoint));
        const std::size_t h3 = std::hash<int>{}(key.pixelHeightX10);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
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

struct GlyphEntry
{
    int width = 0;
    int height = 0;
    int x0 = 0;
    int y0 = 0;
    int advance = 0;
    float u0 = 0.0F;
    float v0 = 0.0F;
    float u1 = 0.0F;
    float v1 = 0.0F;
};

struct DrawBatch
{
    TextureHandle texture;
    std::uint32_t firstVertex = 0;
    std::uint32_t vertexCount = 0;
};

RectF IntersectRect(const RectF& a, const RectF& b)
{
    const float left = std::max(a.x, b.x);
    const float top = std::max(a.y, b.y);
    const float right = std::min(a.x + a.width, b.x + b.width);
    const float bottom = std::min(a.y + a.height, b.y + b.height);
    if (right <= left || bottom <= top)
    {
        return {};
    }

    return RectF{left, top, right - left, bottom - top};
}

bool IsValidRect(const RectF& rect)
{
    return rect.width > 0.0F && rect.height > 0.0F;
}

std::vector<Vector2> BuildRoundedRectPath(const RectF& bounds, float radius)
{
    std::vector<Vector2> points;
    if (bounds.width <= 0.0F || bounds.height <= 0.0F)
    {
        return points;
    }

    const float clampedRadius = std::max(0.0F, std::min({radius, bounds.width * 0.5F, bounds.height * 0.5F}));
    if (clampedRadius <= 0.0F)
    {
        points.push_back(Vector2{bounds.x, bounds.y});
        points.push_back(Vector2{bounds.x + bounds.width, bounds.y});
        points.push_back(Vector2{bounds.x + bounds.width, bounds.y + bounds.height});
        points.push_back(Vector2{bounds.x, bounds.y + bounds.height});
        return points;
    }

    const float arcLength = 0.5F * 3.1415926535F * clampedRadius;
    const int arcSteps = std::max(4, static_cast<int>(std::ceil(arcLength / kRoundedRectSegmentLength)));
    points.push_back(Vector2{bounds.x + clampedRadius, bounds.y});
    points.push_back(Vector2{bounds.x + bounds.width - clampedRadius, bounds.y});

    const auto appendArc = [&](float centerX, float centerY, float startAngle, float endAngle) {
        for (int step = 1; step <= arcSteps; ++step)
        {
            const float t = static_cast<float>(step) / static_cast<float>(arcSteps);
            const float angle = startAngle + (endAngle - startAngle) * t;
            points.push_back(Vector2{
                centerX + std::cos(angle) * clampedRadius,
                centerY + std::sin(angle) * clampedRadius,
            });
        }
    };

    appendArc(
        bounds.x + bounds.width - clampedRadius,
        bounds.y + clampedRadius,
        -0.5F * 3.1415926535F,
        0.0F);
    points.push_back(Vector2{bounds.x + bounds.width, bounds.y + bounds.height - clampedRadius});
    appendArc(
        bounds.x + bounds.width - clampedRadius,
        bounds.y + bounds.height - clampedRadius,
        0.0F,
        0.5F * 3.1415926535F);
    points.push_back(Vector2{bounds.x + clampedRadius, bounds.y + bounds.height});
    appendArc(
        bounds.x + clampedRadius,
        bounds.y + bounds.height - clampedRadius,
        0.5F * 3.1415926535F,
        3.1415926535F);
    points.push_back(Vector2{bounds.x, bounds.y + clampedRadius});
    appendArc(
        bounds.x + clampedRadius,
        bounds.y + clampedRadius,
        3.1415926535F,
        1.5F * 3.1415926535F);
    return points;
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
            continue;
        }

        if ((lead & 0xE0) == 0xC0 && index + 1 < text.size())
        {
            result.push_back(
                (static_cast<char32_t>(lead & 0x1F) << 6) |
                static_cast<char32_t>(static_cast<unsigned char>(text[index + 1]) & 0x3F));
            index += 2;
            continue;
        }

        if ((lead & 0xF0) == 0xE0 && index + 2 < text.size())
        {
            result.push_back(
                (static_cast<char32_t>(lead & 0x0F) << 12) |
                (static_cast<char32_t>(static_cast<unsigned char>(text[index + 1]) & 0x3F) << 6) |
                static_cast<char32_t>(static_cast<unsigned char>(text[index + 2]) & 0x3F));
            index += 3;
            continue;
        }

        if ((lead & 0xF8) == 0xF0 && index + 3 < text.size())
        {
            result.push_back(
                (static_cast<char32_t>(lead & 0x07) << 18) |
                (static_cast<char32_t>(static_cast<unsigned char>(text[index + 1]) & 0x3F) << 12) |
                (static_cast<char32_t>(static_cast<unsigned char>(text[index + 2]) & 0x3F) << 6) |
                static_cast<char32_t>(static_cast<unsigned char>(text[index + 3]) & 0x3F));
            index += 4;
            continue;
        }

        result.push_back(U'?');
        ++index;
    }

    return result;
}

int ClipCode(const RectF& clip, float x, float y)
{
    int code = 0;
    if (x < clip.x)
    {
        code |= 1;
    }
    else if (x > clip.x + clip.width)
    {
        code |= 2;
    }

    if (y < clip.y)
    {
        code |= 4;
    }
    else if (y > clip.y + clip.height)
    {
        code |= 8;
    }

    return code;
}

bool ClipLineToRect(const RectF& clip, Vector2& start, Vector2& end)
{
    float x0 = start.x;
    float y0 = start.y;
    float x1 = end.x;
    float y1 = end.y;

    int code0 = ClipCode(clip, x0, y0);
    int code1 = ClipCode(clip, x1, y1);

    while (true)
    {
        if ((code0 | code1) == 0)
        {
            start = Vector2{x0, y0};
            end = Vector2{x1, y1};
            return true;
        }

        if ((code0 & code1) != 0)
        {
            return false;
        }

        const int outsideCode = code0 != 0 ? code0 : code1;
        float x = 0.0F;
        float y = 0.0F;

        if ((outsideCode & 8) != 0)
        {
            if (std::abs(y1 - y0) <= 0.0001F)
            {
                return false;
            }
            x = x0 + (x1 - x0) * ((clip.y + clip.height) - y0) / (y1 - y0);
            y = clip.y + clip.height;
        }
        else if ((outsideCode & 4) != 0)
        {
            if (std::abs(y1 - y0) <= 0.0001F)
            {
                return false;
            }
            x = x0 + (x1 - x0) * (clip.y - y0) / (y1 - y0);
            y = clip.y;
        }
        else if ((outsideCode & 2) != 0)
        {
            if (std::abs(x1 - x0) <= 0.0001F)
            {
                return false;
            }
            y = y0 + (y1 - y0) * ((clip.x + clip.width) - x0) / (x1 - x0);
            x = clip.x + clip.width;
        }
        else
        {
            if (std::abs(x1 - x0) <= 0.0001F)
            {
                return false;
            }
            y = y0 + (y1 - y0) * (clip.x - x0) / (x1 - x0);
            x = clip.x;
        }

        if (outsideCode == code0)
        {
            x0 = x;
            y0 = y;
            code0 = ClipCode(clip, x0, y0);
        }
        else
        {
            x1 = x;
            y1 = y;
            code1 = ClipCode(clip, x1, y1);
        }
    }
}

GlyphBitmap BuildGlyphBitmap(const FontFace& face, char32_t codepoint, float pixelHeight)
{
    GlyphBitmap glyph;
    const float scale = stbtt_ScaleForPixelHeight(&face.info, pixelHeight);

    int advance = 0;
    int leftBearing = 0;
    stbtt_GetCodepointHMetrics(&face.info, static_cast<int>(codepoint), &advance, &leftBearing);
    glyph.advance = static_cast<int>(std::round(static_cast<float>(advance) * scale));

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

}

class RenderQueueRenderer::Impl
{
public:
    explicit Impl(Device& device)
        : device(device)
    {
    }

    bool Initialize()
    {
        if (initialized)
        {
            return true;
        }

        LoadFonts();
        if (!CreateTextures() || !CreatePipeline())
        {
            Shutdown();
            return false;
        }

        initialized = true;
        return true;
    }

    void Shutdown()
    {
        if (IsValid(vertexBuffer))
        {
            device.DestroyBuffer(vertexBuffer);
            vertexBuffer = {};
        }

        if (IsValid(pipeline))
        {
            device.DestroyPipeline(pipeline);
            pipeline = {};
        }

        if (IsValid(fragmentShader))
        {
            device.DestroyShader(fragmentShader);
            fragmentShader = {};
        }

        if (IsValid(vertexShader))
        {
            device.DestroyShader(vertexShader);
            vertexShader = {};
        }

        if (IsValid(fontAtlasTexture))
        {
            device.DestroyTexture(fontAtlasTexture);
            fontAtlasTexture = {};
        }

        if (IsValid(whiteTexture))
        {
            device.DestroyTexture(whiteTexture);
            whiteTexture = {};
        }

        initialized = false;
        vertexCapacity = 0;
        vertices.clear();
        batches.clear();
        atlasPixels.clear();
        glyphCache.clear();
        atlasDirty = false;
        atlasPenX = 1;
        atlasPenY = 1;
        atlasRowHeight = 0;
        atlasOverflowLogged = false;
    }

    bool Render(CommandBufferHandle commandBuffer, const RenderQueue& queue, Vector2 logicalSize)
    {
        if (!initialized || logicalSize.x <= 0.0F || logicalSize.y <= 0.0F)
        {
            return false;
        }

        currentLogicalSize = logicalSize;
        vertices.clear();
        batches.clear();

        const RectF fullClip{0.0F, 0.0F, logicalSize.x, logicalSize.y};
        std::vector<RectF> clipStack;
        clipStack.push_back(fullClip);

        for (const RenderCommand& command : queue.Commands())
        {
            if (command.type == RenderCommandType::PushClip)
            {
                clipStack.push_back(IntersectRect(clipStack.back(), RectF{
                    command.bounds.x,
                    command.bounds.y,
                    command.bounds.width,
                    command.bounds.height,
                }));
                continue;
            }

            if (command.type == RenderCommandType::PopClip)
            {
                if (clipStack.size() > 1)
                {
                    clipStack.pop_back();
                }
                continue;
            }

            const RectF clip = clipStack.back();
            if (!IsValidRect(clip))
            {
                continue;
            }

            switch (command.type)
            {
            case RenderCommandType::Rect:
                EmitRect(command.bounds, command.color, clip);
                break;
            case RenderCommandType::RoundedRect:
                EmitRoundedRect(command.bounds, command.color, command.cornerRadius, clip);
                break;
            case RenderCommandType::Text:
                EmitText(command, clip);
                break;
            case RenderCommandType::Line:
                EmitLine(command, clip);
                break;
            case RenderCommandType::PushClip:
            case RenderCommandType::PopClip:
                break;
            }
        }

        if (atlasDirty)
        {
            if (!device.UpdateTexture(fontAtlasTexture, TextureDesc{
                kAtlasWidth,
                kAtlasHeight,
                atlasPixels.data(),
            }))
            {
                Logger::instance().Error("Failed to update font atlas texture.");
                return false;
            }
            atlasDirty = false;
        }

        if (vertices.empty())
        {
            return true;
        }

        if (!UploadVertices())
        {
            return false;
        }

        device.BindPipeline(commandBuffer, pipeline);
        device.BindVertexBuffer(commandBuffer, vertexBuffer);
        for (const DrawBatch& batch : batches)
        {
            device.BindTexture(commandBuffer, batch.texture);
            device.Draw(commandBuffer, batch.vertexCount, batch.firstVertex);
        }

        return true;
    }

private:
    void LoadFonts()
    {
        faces.clear();
        std::vector<Buffer> loadedFonts = LoadPlatformFonts();
        faces.reserve(loadedFonts.size());

        for (auto& data : loadedFonts)
        {
            if (data.empty())
            {
                continue;
            }

            FontFace face;
            face.data = std::move(data);
            const int offset = stbtt_GetFontOffsetForIndex(face.data.data(), 0);
            if (offset < 0 || stbtt_InitFont(&face.info, face.data.data(), offset) == 0)
            {
                continue;
            }

            faces.push_back(std::move(face));
        }

        if (faces.empty())
        {
            Logger::instance().Warn("RenderQueueRenderer did not load any platform fonts; text commands will be skipped.");
        }
    }

    bool CreateTextures()
    {
        const std::array<std::uint8_t, 4> whitePixel = {255, 255, 255, 255};
        whiteTexture = device.CreateTexture(TextureDesc{1, 1, whitePixel.data()});
        if (!IsValid(whiteTexture))
        {
            Logger::instance().Error("Failed to create UI white texture.");
            return false;
        }

        atlasPixels.assign(static_cast<std::size_t>(kAtlasWidth) * kAtlasHeight * 4, 0);
        fontAtlasTexture = device.CreateTexture(TextureDesc{
            kAtlasWidth,
            kAtlasHeight,
            atlasPixels.data(),
        });
        if (!IsValid(fontAtlasTexture))
        {
            Logger::instance().Error("Failed to create UI font atlas texture.");
            return false;
        }

        return true;
    }

    bool CreatePipeline()
    {
        const char* vertexSource =
#if defined(BKUI_PLATFORM_SWITCH)
            "shaders/deko_ui_vsh.dksh";
#else
            R"(
            #version 330 core
            layout(location = 0) in vec2 inPosition;
            layout(location = 1) in vec4 inColor;
            layout(location = 2) in vec2 inTexCoord;
            out vec4 vertexColor;
            out vec2 texCoord;
            void main()
            {
                vertexColor = inColor;
                texCoord = inTexCoord;
                gl_Position = vec4(inPosition, 0.0, 1.0);
            }
        )";
#endif

        const char* fragmentSource =
#if defined(BKUI_PLATFORM_SWITCH)
            "shaders/deko_ui_fsh.dksh";
#else
            R"(
            #version 330 core
            in vec4 vertexColor;
            in vec2 texCoord;
            uniform sampler2D uTexture;
            out vec4 outColor;
            void main()
            {
                outColor = texture(uTexture, texCoord) * vertexColor;
            }
        )";
#endif

        vertexShader = device.CreateShader(ShaderDesc{ShaderStage::Vertex, vertexSource});
        fragmentShader = device.CreateShader(ShaderDesc{ShaderStage::Fragment, fragmentSource});
        if (!IsValid(vertexShader) || !IsValid(fragmentShader))
        {
            Logger::instance().Error("Failed to create UI shaders.");
            return false;
        }

        constexpr std::array<VertexAttributeDesc, 3> attributes = {{
            {0, VertexFormat::Float2, offsetof(GpuVertex, position)},
            {1, VertexFormat::Float4, offsetof(GpuVertex, color)},
            {2, VertexFormat::Float2, offsetof(GpuVertex, uv)},
        }};

        pipeline = device.CreatePipeline(PipelineDesc{
            vertexShader,
            fragmentShader,
            VertexLayoutDesc{sizeof(GpuVertex), attributes.data(), attributes.size()},
            PrimitiveTopology::Triangles,
        });
        if (!IsValid(pipeline))
        {
            Logger::instance().Error("Failed to create UI pipeline.");
            return false;
        }

        return true;
    }

    const FontFace* PrimaryFace() const
    {
        return faces.empty() ? nullptr : &faces.front();
    }

    std::uint32_t SelectFaceIndex(char32_t codepoint) const
    {
        for (std::uint32_t index = 0; index < faces.size(); ++index)
        {
            if (stbtt_FindGlyphIndex(&faces[index].info, static_cast<int>(codepoint)) != 0)
            {
                return index;
            }
        }

        return faces.empty() ? std::numeric_limits<std::uint32_t>::max() : 0;
    }

    const GlyphEntry* EnsureGlyph(std::uint32_t faceIndex, char32_t codepoint, float pixelHeight)
    {
        if (faceIndex >= faces.size())
        {
            return nullptr;
        }

        const GlyphKey key{
            faceIndex,
            codepoint,
            static_cast<int>(std::round(pixelHeight * 10.0F)),
        };

        const auto existing = glyphCache.find(key);
        if (existing != glyphCache.end())
        {
            return &existing->second;
        }

        const GlyphBitmap bitmap = BuildGlyphBitmap(faces[faceIndex], codepoint, pixelHeight);
        GlyphEntry entry;
        entry.width = bitmap.width;
        entry.height = bitmap.height;
        entry.x0 = bitmap.x0;
        entry.y0 = bitmap.y0;
        entry.advance = bitmap.advance;

        if (bitmap.width > 0 && bitmap.height > 0)
        {
            constexpr int padding = 1;
            const int glyphWidth = bitmap.width + padding * 2;
            const int glyphHeight = bitmap.height + padding * 2;

            if (atlasPenX + glyphWidth > static_cast<int>(kAtlasWidth))
            {
                atlasPenX = 1;
                atlasPenY += atlasRowHeight + 1;
                atlasRowHeight = 0;
            }

            if (atlasPenY + glyphHeight > static_cast<int>(kAtlasHeight))
            {
                if (!atlasOverflowLogged)
                {
                    Logger::instance().Warn("Font atlas is full; some glyphs may be missing.");
                    atlasOverflowLogged = true;
                }

                return nullptr;
            }

            const int atlasX = atlasPenX + padding;
            const int atlasY = atlasPenY + padding;
            atlasPenX += glyphWidth + 1;
            atlasRowHeight = std::max(atlasRowHeight, glyphHeight);

            for (int yy = 0; yy < bitmap.height; ++yy)
            {
                for (int xx = 0; xx < bitmap.width; ++xx)
                {
                    const std::size_t dstIndex = static_cast<std::size_t>(
                        ((atlasY + yy) * static_cast<int>(kAtlasWidth) + atlasX + xx) * 4);
                    const std::uint8_t alpha = bitmap.pixels[static_cast<std::size_t>(yy * bitmap.width + xx)];
                    atlasPixels[dstIndex + 0] = 255;
                    atlasPixels[dstIndex + 1] = 255;
                    atlasPixels[dstIndex + 2] = 255;
                    atlasPixels[dstIndex + 3] = alpha;
                }
            }

            entry.u0 = static_cast<float>(atlasX) / static_cast<float>(kAtlasWidth);
            entry.v0 = static_cast<float>(atlasY) / static_cast<float>(kAtlasHeight);
            entry.u1 = static_cast<float>(atlasX + bitmap.width) / static_cast<float>(kAtlasWidth);
            entry.v1 = static_cast<float>(atlasY + bitmap.height) / static_cast<float>(kAtlasHeight);
            atlasDirty = true;
        }

        return &glyphCache.emplace(key, entry).first->second;
    }

    bool UploadVertices()
    {
        const std::size_t bytes = vertices.size() * sizeof(GpuVertex);
        if (!IsValid(vertexBuffer) || bytes > vertexCapacity)
        {
            if (IsValid(vertexBuffer))
            {
                device.DestroyBuffer(vertexBuffer);
                vertexBuffer = {};
            }

            vertexCapacity = std::max(bytes, vertexCapacity == 0 ? kInitialVertexCapacity * sizeof(GpuVertex) : vertexCapacity * 2);
            vertexBuffer = device.CreateBuffer(BufferDesc{
                BufferUsage::Vertex,
                bytes,
                vertices.data(),
            });
            if (!IsValid(vertexBuffer))
            {
                Logger::instance().Error("Failed to allocate UI vertex buffer.");
                return false;
            }

            vertexCapacity = bytes;
            return true;
        }

        if (!device.UpdateBuffer(vertexBuffer, BufferDesc{
            BufferUsage::Vertex,
            bytes,
            vertices.data(),
        }))
        {
            Logger::instance().Error("Failed to upload UI vertices.");
            return false;
        }

        return true;
    }

    void AppendBatch(TextureHandle texture, std::uint32_t vertexCount)
    {
        if (!IsValid(texture) || vertexCount == 0)
        {
            return;
        }

        const std::uint32_t firstVertex = static_cast<std::uint32_t>(vertices.size() - vertexCount);
        if (!batches.empty() && batches.back().texture.id == texture.id &&
            batches.back().firstVertex + batches.back().vertexCount == firstVertex)
        {
            batches.back().vertexCount += vertexCount;
            return;
        }

        batches.push_back(DrawBatch{
            texture,
            firstVertex,
            vertexCount,
        });
    }

    Vector2 ToClipSpace(float x, float y, Vector2 logicalSize) const
    {
        return Vector2{
            (x / logicalSize.x) * 2.0F - 1.0F,
            1.0F - (y / logicalSize.y) * 2.0F,
        };
    }

    void PushQuad(const RectF& rect, const Color& color, TextureHandle texture, Vector2 logicalSize, float u0, float v0, float u1, float v1)
    {
        const Vector2 topLeft = ToClipSpace(rect.x, rect.y, logicalSize);
        const Vector2 bottomLeft = ToClipSpace(rect.x, rect.y + rect.height, logicalSize);
        const Vector2 bottomRight = ToClipSpace(rect.x + rect.width, rect.y + rect.height, logicalSize);
        const Vector2 topRight = ToClipSpace(rect.x + rect.width, rect.y, logicalSize);

        const auto pushVertex = [&](const Vector2& position, float u, float v) {
            vertices.push_back(GpuVertex{
                {position.x, position.y},
                {color.r, color.g, color.b, color.a},
                {u, v},
            });
        };

        pushVertex(topLeft, u0, v0);
        pushVertex(bottomLeft, u0, v1);
        pushVertex(bottomRight, u1, v1);
        pushVertex(topLeft, u0, v0);
        pushVertex(bottomRight, u1, v1);
        pushVertex(topRight, u1, v0);
        AppendBatch(texture, 6);
    }

    void PushTriangle(const Vector2& p0, const Vector2& p1, const Vector2& p2, const Color& color, TextureHandle texture)
    {
        const auto pushVertex = [&](const Vector2& position) {
            const Vector2 clipPos = ToClipSpace(position.x, position.y, currentLogicalSize);
            vertices.push_back(GpuVertex{
                {clipPos.x, clipPos.y},
                {color.r, color.g, color.b, color.a},
                {0.0F, 0.0F},
            });
        };

        pushVertex(p0);
        pushVertex(p1);
        pushVertex(p2);
        AppendBatch(texture, 3);
    }

    void ClipPolygonAgainstLeft(std::vector<Vector2>& points, float edge) const
    {
        std::vector<Vector2> output;
        output.reserve(points.size() + 2);
        if (points.empty())
        {
            points.swap(output);
            return;
        }

        Vector2 previous = points.back();
        bool previousInside = previous.x >= edge;
        for (const Vector2& current : points)
        {
            const bool currentInside = current.x >= edge;
            if (currentInside != previousInside && std::abs(current.x - previous.x) > 0.0001F)
            {
                const float t = (edge - previous.x) / (current.x - previous.x);
                output.push_back(Vector2{
                    edge,
                    previous.y + (current.y - previous.y) * t,
                });
            }
            if (currentInside)
            {
                output.push_back(current);
            }
            previous = current;
            previousInside = currentInside;
        }
        points.swap(output);
    }

    void ClipPolygonAgainstRight(std::vector<Vector2>& points, float edge) const
    {
        std::vector<Vector2> output;
        output.reserve(points.size() + 2);
        if (points.empty())
        {
            points.swap(output);
            return;
        }

        Vector2 previous = points.back();
        bool previousInside = previous.x <= edge;
        for (const Vector2& current : points)
        {
            const bool currentInside = current.x <= edge;
            if (currentInside != previousInside && std::abs(current.x - previous.x) > 0.0001F)
            {
                const float t = (edge - previous.x) / (current.x - previous.x);
                output.push_back(Vector2{
                    edge,
                    previous.y + (current.y - previous.y) * t,
                });
            }
            if (currentInside)
            {
                output.push_back(current);
            }
            previous = current;
            previousInside = currentInside;
        }
        points.swap(output);
    }

    void ClipPolygonAgainstTop(std::vector<Vector2>& points, float edge) const
    {
        std::vector<Vector2> output;
        output.reserve(points.size() + 2);
        if (points.empty())
        {
            points.swap(output);
            return;
        }

        Vector2 previous = points.back();
        bool previousInside = previous.y >= edge;
        for (const Vector2& current : points)
        {
            const bool currentInside = current.y >= edge;
            if (currentInside != previousInside && std::abs(current.y - previous.y) > 0.0001F)
            {
                const float t = (edge - previous.y) / (current.y - previous.y);
                output.push_back(Vector2{
                    previous.x + (current.x - previous.x) * t,
                    edge,
                });
            }
            if (currentInside)
            {
                output.push_back(current);
            }
            previous = current;
            previousInside = currentInside;
        }
        points.swap(output);
    }

    void ClipPolygonAgainstBottom(std::vector<Vector2>& points, float edge) const
    {
        std::vector<Vector2> output;
        output.reserve(points.size() + 2);
        if (points.empty())
        {
            points.swap(output);
            return;
        }

        Vector2 previous = points.back();
        bool previousInside = previous.y <= edge;
        for (const Vector2& current : points)
        {
            const bool currentInside = current.y <= edge;
            if (currentInside != previousInside && std::abs(current.y - previous.y) > 0.0001F)
            {
                const float t = (edge - previous.y) / (current.y - previous.y);
                output.push_back(Vector2{
                    previous.x + (current.x - previous.x) * t,
                    edge,
                });
            }
            if (currentInside)
            {
                output.push_back(current);
            }
            previous = current;
            previousInside = currentInside;
        }
        points.swap(output);
    }

    void PushClippedTriangle(const Vector2& p0, const Vector2& p1, const Vector2& p2, const Color& color, const RectF& clip)
    {
        std::vector<Vector2> polygon = {p0, p1, p2};
        ClipPolygonAgainstLeft(polygon, clip.x);
        ClipPolygonAgainstRight(polygon, clip.x + clip.width);
        ClipPolygonAgainstTop(polygon, clip.y);
        ClipPolygonAgainstBottom(polygon, clip.y + clip.height);
        if (polygon.size() < 3)
        {
            return;
        }

        for (std::size_t index = 1; index + 1 < polygon.size(); ++index)
        {
            PushTriangle(polygon[0], polygon[index], polygon[index + 1], color, whiteTexture);
        }
    }

    void EmitRect(const Rect& rect, const Color& color, const RectF& clip)
    {
        const RectF clipped = IntersectRect(clip, RectF{rect.x, rect.y, rect.width, rect.height});
        if (!IsValidRect(clipped))
        {
            return;
        }

        PushQuad(clipped, color, whiteTexture, currentLogicalSize, 0.0F, 0.0F, 1.0F, 1.0F);
    }

    void EmitRoundedRect(const Rect& rect, const Color& color, float cornerRadius, const RectF& clip)
    {
        const RectF bounds{rect.x, rect.y, rect.width, rect.height};
        const RectF clippedBounds = IntersectRect(bounds, clip);
        if (!IsValidRect(clippedBounds))
        {
            return;
        }

        const float clampedRadius = std::max(0.0F, std::min({cornerRadius, bounds.width * 0.5F, bounds.height * 0.5F}));
        if (clampedRadius <= 0.0F)
        {
            EmitRect(rect, color, clip);
            return;
        }

        const std::vector<Vector2> path = BuildRoundedRectPath(bounds, clampedRadius);
        if (path.size() < 3)
        {
            EmitRect(rect, color, clip);
            return;
        }

        Vector2 center{};
        for (const Vector2& point : path)
        {
            center.x += point.x;
            center.y += point.y;
        }
        center.x /= static_cast<float>(path.size());
        center.y /= static_cast<float>(path.size());

        for (std::size_t index = 0; index < path.size(); ++index)
        {
            const std::size_t nextIndex = (index + 1) % path.size();
            PushClippedTriangle(center, path[index], path[nextIndex], color, clip);
        }
    }

    void EmitText(const RenderCommand& command, const RectF& clip)
    {
        const FontFace* primaryFace = PrimaryFace();
        if (primaryFace == nullptr)
        {
            return;
        }

        const std::u32string text = Utf8ToUtf32(command.text);
        const float primaryScale = stbtt_ScaleForPixelHeight(&primaryFace->info, command.fontSize);
        int ascent = 0;
        int descent = 0;
        int lineGap = 0;
        stbtt_GetFontVMetrics(&primaryFace->info, &ascent, &descent, &lineGap);
        const float baseline = command.bounds.y + static_cast<float>(ascent) * primaryScale;
        float cursor = command.bounds.x;

        for (char32_t codepoint : text)
        {
            const std::uint32_t faceIndex = SelectFaceIndex(codepoint);
            const GlyphEntry* glyph = EnsureGlyph(faceIndex, codepoint, command.fontSize);
            if (glyph == nullptr)
            {
                continue;
            }

            if (glyph->width > 0 && glyph->height > 0)
            {
                RectF glyphRect{
                    cursor + static_cast<float>(glyph->x0),
                    baseline + static_cast<float>(glyph->y0),
                    static_cast<float>(glyph->width),
                    static_cast<float>(glyph->height),
                };
                const RectF clipped = IntersectRect(glyphRect, clip);
                if (IsValidRect(clipped))
                {
                    const float du = glyph->u1 - glyph->u0;
                    const float dv = glyph->v1 - glyph->v0;
                    const float localLeft = (clipped.x - glyphRect.x) / glyphRect.width;
                    const float localTop = (clipped.y - glyphRect.y) / glyphRect.height;
                    const float localRight = (clipped.x + clipped.width - glyphRect.x) / glyphRect.width;
                    const float localBottom = (clipped.y + clipped.height - glyphRect.y) / glyphRect.height;

                    PushQuad(
                        clipped,
                        command.color,
                        fontAtlasTexture,
                        currentLogicalSize,
                        glyph->u0 + du * localLeft,
                        glyph->v0 + dv * localTop,
                        glyph->u0 + du * localRight,
                        glyph->v0 + dv * localBottom);
                }
            }

            cursor += static_cast<float>(glyph->advance);
        }
    }

    void EmitLine(const RenderCommand& command, const RectF& clip)
    {
        Vector2 start = command.lineStart;
        Vector2 end = command.lineEnd;
        if (!ClipLineToRect(clip, start, end))
        {
            return;
        }

        const float dx = end.x - start.x;
        const float dy = end.y - start.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length <= 0.0001F)
        {
            EmitRect(Rect{start.x, start.y, 1.0F, 1.0F}, command.color, clip);
            return;
        }

        const float thickness = std::max(0.1F, command.lineThickness);
        const float nx = -dy / length * (thickness * 0.5F);
        const float ny = dx / length * (thickness * 0.5F);

        const Vector2 p0{start.x + nx, start.y + ny};
        const Vector2 p1{start.x - nx, start.y - ny};
        const Vector2 p2{end.x - nx, end.y - ny};
        const Vector2 p3{end.x + nx, end.y + ny};

        const auto pushVertex = [&](const Vector2& position) {
            const Vector2 clipPos = ToClipSpace(position.x, position.y, currentLogicalSize);
            vertices.push_back(GpuVertex{
                {clipPos.x, clipPos.y},
                {command.color.r, command.color.g, command.color.b, command.color.a},
                {0.0F, 0.0F},
            });
        };

        // Keep the same clip-space winding as PushQuad so back-face culling on
        // stricter backends (notably deko3d) does not discard UI line quads.
        pushVertex(p0);
        pushVertex(p2);
        pushVertex(p1);
        pushVertex(p0);
        pushVertex(p3);
        pushVertex(p2);
        AppendBatch(whiteTexture, 6);
    }

    Device& device;
    bool initialized = false;

    ShaderHandle vertexShader{};
    ShaderHandle fragmentShader{};
    PipelineHandle pipeline{};
    TextureHandle whiteTexture{};
    TextureHandle fontAtlasTexture{};
    BufferHandle vertexBuffer{};

    std::size_t vertexCapacity = 0;
    std::vector<GpuVertex> vertices;
    std::vector<DrawBatch> batches;
    std::vector<FontFace> faces;
    std::unordered_map<GlyphKey, GlyphEntry, GlyphKeyHasher> glyphCache;
    std::vector<std::uint8_t> atlasPixels;
    bool atlasDirty = false;
    int atlasPenX = 1;
    int atlasPenY = 1;
    int atlasRowHeight = 0;
    bool atlasOverflowLogged = false;
    Vector2 currentLogicalSize{1280.0F, 720.0F};
};

RenderQueueRenderer::RenderQueueRenderer(Device& device)
    : device_(device)
    , impl_(new Impl(device))
{
}

RenderQueueRenderer::~RenderQueueRenderer()
{
    if (impl_ != nullptr)
    {
        impl_->Shutdown();
        delete impl_;
        impl_ = nullptr;
    }
}

bool RenderQueueRenderer::Initialize()
{
    return impl_ != nullptr && impl_->Initialize();
}

void RenderQueueRenderer::Shutdown()
{
    if (impl_ != nullptr)
    {
        impl_->Shutdown();
    }
}

bool RenderQueueRenderer::Render(CommandBufferHandle commandBuffer, const RenderQueue& queue, Vector2 logicalSize)
{
    if (impl_ == nullptr)
    {
        return false;
    }

    return impl_->Render(commandBuffer, queue, logicalSize);
}

}
