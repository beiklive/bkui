#include <bkui/bkui.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <string>
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

void FillRect(Image& image, int x, int y, int w, int h, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
{
    for (int yy = y; yy < y + h; ++yy)
    {
        for (int xx = x; xx < x + w; ++xx)
        {
            PutPixel(image, xx, yy, r, g, b, a);
        }
    }
}

void Blit(Image& dst, const Image& src, int x, int y, int w, int h)
{
    for (int yy = 0; yy < h; ++yy)
    {
        for (int xx = 0; xx < w; ++xx)
        {
            const int sx = xx * src.width / w;
            const int sy = yy * src.height / h;
            const std::size_t si = static_cast<std::size_t>((sy * src.width + sx) * 4);
            const std::uint8_t sr = src.pixels[si + 0];
            const std::uint8_t sg = src.pixels[si + 1];
            const std::uint8_t sb = src.pixels[si + 2];
            const std::uint8_t sa = src.pixels[si + 3];
            const std::size_t di = static_cast<std::size_t>(((y + yy) * dst.width + (x + xx)) * 4);
            const float alpha = sa / 255.0F;
            dst.pixels[di + 0] = static_cast<std::uint8_t>(sr * alpha + dst.pixels[di + 0] * (1.0F - alpha));
            dst.pixels[di + 1] = static_cast<std::uint8_t>(sg * alpha + dst.pixels[di + 1] * (1.0F - alpha));
            dst.pixels[di + 2] = static_cast<std::uint8_t>(sb * alpha + dst.pixels[di + 2] * (1.0F - alpha));
            dst.pixels[di + 3] = 255;
        }
    }
}

Image LoadPng(const std::string& path)
{
    bk::Buffer data;
    try
    {
        data = bk::FileSystem::Read(path);
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "Failed to read PNG resource %s: %s\n", path.c_str(), ex.what());
        return {};
    }

    if (data.empty())
    {
        std::fprintf(stderr, "PNG resource is empty: %s\n", path.c_str());
        return {};
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* decoded = stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &width, &height, &channels, 4);
    if (decoded == nullptr)
    {
        std::fprintf(stderr, "Failed to decode PNG resource %s: %s\n", path.c_str(), stbi_failure_reason());
        return {};
    }

    Image image;
    image.width = width;
    image.height = height;
    image.pixels.assign(decoded, decoded + static_cast<std::size_t>(width * height * 4));
    stbi_image_free(decoded);
    return image;
}

std::uint8_t HexByte(const std::string& text, std::size_t pos)
{
    return static_cast<std::uint8_t>(std::stoi(text.substr(pos, 2), nullptr, 16));
}

bool ReadHexColor(const std::string& text, std::size_t pos, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b)
{
    if (pos + 6 > text.size())
    {
        return false;
    }

    try
    {
        r = HexByte(text, pos);
        g = HexByte(text, pos + 2);
        b = HexByte(text, pos + 4);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

void DrawSvgPreview(Image& dst, const std::string& path, int x, int y, int size)
{
    bk::Buffer data;
    try
    {
        data = bk::FileSystem::Read(path);
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "Failed to read SVG resource %s: %s\n", path.c_str(), ex.what());
    }

    const std::string svg = data.empty()
        ? std::string{}
        : std::string(reinterpret_cast<const char*>(data.data()), data.size());
    FillRect(dst, x, y, size, size, 245, 247, 250, 255);

    int colorIndex = 0;
    std::size_t pos = 0;
    while ((pos = svg.find("fill=\"#", pos)) != std::string::npos && colorIndex < 12)
    {
        const std::size_t colorPos = pos + 7;
        std::uint8_t r = 0;
        std::uint8_t g = 0;
        std::uint8_t b = 0;
        if (!ReadHexColor(svg, colorPos, r, g, b))
        {
            pos = colorPos;
            continue;
        }
        const int inset = 14 + colorIndex * 7;
        const int w = std::max(18, size - inset * 2);
        const int h = std::max(18, size / 3 - colorIndex * 2);
        FillRect(dst, x + inset, y + inset + (colorIndex % 4) * 20, w, h, r, g, b, 230);
        pos = colorPos + 6;
        ++colorIndex;
    }
}

void DrawText(Image& dst, const std::string& fontPath, const std::u32string& text, int x, int y, float px, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    bk::Buffer font;
    try
    {
        font = bk::FileSystem::Read(fontPath);
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "Failed to read font resource %s: %s\n", fontPath.c_str(), ex.what());
        return;
    }

    if (font.empty())
    {
        std::fprintf(stderr, "Font resource is empty: %s\n", fontPath.c_str());
        return;
    }

    stbtt_fontinfo info{};
    if (stbtt_InitFont(&info, font.data(), stbtt_GetFontOffsetForIndex(font.data(), 0)) == 0)
    {
        std::fprintf(stderr, "Failed to initialize font resource: %s\n", fontPath.c_str());
        return;
    }

    const float scale = stbtt_ScaleForPixelHeight(&info, px);
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    int cursor = x;
    const int baseline = y + static_cast<int>(ascent * scale);

    for (char32_t cp : text)
    {
        int advance = 0;
        int bearing = 0;
        stbtt_GetCodepointHMetrics(&info, static_cast<int>(cp), &advance, &bearing);
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        stbtt_GetCodepointBitmapBox(&info, static_cast<int>(cp), scale, scale, &x0, &y0, &x1, &y1);
        const int gw = x1 - x0;
        const int gh = y1 - y0;
        std::vector<std::uint8_t> glyph(static_cast<std::size_t>(std::max(0, gw * gh)));
        if (gw > 0 && gh > 0)
        {
            stbtt_MakeCodepointBitmap(&info, glyph.data(), gw, gh, gw, scale, scale, static_cast<int>(cp));
            for (int yy = 0; yy < gh; ++yy)
            {
                for (int xx = 0; xx < gw; ++xx)
                {
                    const std::uint8_t a = glyph[static_cast<std::size_t>(yy * gw + xx)];
                    if (a > 0)
                    {
                        PutPixel(dst, cursor + x0 + xx, baseline + y0 + yy, r, g, b, a);
                    }
                }
            }
        }
        cursor += static_cast<int>(advance * scale);
    }
}

Image BuildShowcase()
{
    Image canvas;
    canvas.width = 1280;
    canvas.height = 720;
    canvas.pixels.resize(static_cast<std::size_t>(canvas.width * canvas.height * 4), 255);

    FillRect(canvas, 0, 0, canvas.width, canvas.height, 22, 28, 36, 255);
    FillRect(canvas, 48, 48, 928, 544, 242, 246, 250, 255);
    FillRect(canvas, 64, 64, 896, 76, 36, 77, 122, 255);



    DrawText(canvas, "fonts/switch_font.ttf", U"BeikUI 资源渲染演示", 92, 82, 34.0F, 255, 255, 255);
    DrawText(canvas, "fonts/switch_font.ttf", U"中文字体 / PNG 图片 / SVG 图片", 92, 158, 26.0F, 32, 42, 55);

    const Image png = LoadPng("images/beiklive.png");
    if (!png.pixels.empty())
    {
        Blit(canvas, png, 96, 218, 336, 220);
    }
    else
    {
        FillRect(canvas, 96, 218, 336, 220, 220, 226, 234, 255);
        DrawText(canvas, "fonts/font.ttf", U"PNG load failed", 164, 316, 22.0F, 120, 40, 40);
    }
    FillRect(canvas, 96, 448, 336, 38, 236, 240, 245, 255);
    DrawText(canvas, "fonts/font.ttf", U"PNG: images/beiklive.png", 112, 454, 20.0F, 20, 34, 48);

    DrawSvgPreview(canvas, "images/1.svg", 592, 210, 220);
    FillRect(canvas, 552, 448, 336, 38, 236, 240, 245, 255);
    DrawText(canvas, "fonts/font.ttf", U"SVG: images/1.svg", 590, 454, 20.0F, 20, 34, 48);

    DrawText(canvas, "fonts/font.ttf", U"Switch 使用 Deko3D，Windows 使用 OpenGL", 196, 526, 24.0F, 56, 64, 74);
    return canvas;
}
}

int main(int argc, char** argv)
{
    try
    {
        bk::FileSystem::Init(argc > 0 ? argv[0] : nullptr);
        bk::FileSystem::Mount("resources");

        auto platform = bk::CreateDefaultPlatform(bk::WindowDesc{
            "BeikUI Resource Demo",
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
        const bool graphicsReady = device->Init();
        if (!graphicsReady)
        {
            std::fprintf(stderr, "Graphics backend unavailable: %s\n", device->BackendName());
            return 1;
        }

        const Image showcase = BuildShowcase();
        const std::array<Vertex, 6> vertices = {{
            {{-0.86F,  0.82F}, {1.0F, 1.0F, 1.0F}, {0.0F, 0.0F}},
            {{-0.86F, -0.82F}, {1.0F, 1.0F, 1.0F}, {0.0F, 1.0F}},
            {{ 0.86F, -0.82F}, {1.0F, 1.0F, 1.0F}, {1.0F, 1.0F}},
            {{-0.86F,  0.82F}, {1.0F, 1.0F, 1.0F}, {0.0F, 0.0F}},
            {{ 0.86F, -0.82F}, {1.0F, 1.0F, 1.0F}, {1.0F, 1.0F}},
            {{ 0.86F,  0.82F}, {1.0F, 1.0F, 1.0F}, {1.0F, 0.0F}},
        }};

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

        bk::SwapchainHandle swapchain = device->GetMainSwapchain();
        bk::BufferHandle vertexBuffer = device->CreateBuffer(bk::BufferDesc{
            bk::BufferUsage::Vertex,
            sizeof(Vertex) * vertices.size(),
            vertices.data(),
        });
        bk::TextureHandle texture = device->CreateTexture(bk::TextureDesc{
            static_cast<std::uint32_t>(showcase.width),
            static_cast<std::uint32_t>(showcase.height),
            showcase.pixels.data(),
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

        if (!bk::IsValid(vertexBuffer) || !bk::IsValid(texture) || !bk::IsValid(vertexShader) ||
            !bk::IsValid(fragmentShader) || !bk::IsValid(pipeline) || !bk::IsValid(commandBuffer))
        {
            std::fprintf(stderr, "Failed to create resource demo RHI objects.\n");
            return 1;
        }

        while (platform->IsRunning())
        {
            platform->PollEvents();
            device->BeginFrame(swapchain, bk::RenderPassDesc{bk::Color{0.05F, 0.06F, 0.08F, 1.0F}});
            device->BeginCommandBuffer(commandBuffer);
            device->BindPipeline(commandBuffer, pipeline);
            device->BindVertexBuffer(commandBuffer, vertexBuffer);
            device->BindTexture(commandBuffer, texture);
            device->Draw(commandBuffer, static_cast<std::uint32_t>(vertices.size()));
            device->EndCommandBuffer(commandBuffer);
            device->Submit(commandBuffer);
            device->EndFrame(swapchain);
        }

        device->DestroyCommandBuffer(commandBuffer);
        device->DestroyPipeline(pipeline);
        device->DestroyShader(fragmentShader);
        device->DestroyShader(vertexShader);
        device->DestroyTexture(texture);
        device->DestroyBuffer(vertexBuffer);
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
