#include <bkui/bkui.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#if defined(BKUI_PLATFORM_SWITCH)
#include <switch.h>
#endif

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

struct Point
{
    float x = 0.0F;
    float y = 0.0F;
};

struct Rgba
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

struct SvgShape
{
    std::vector<std::vector<Point>> contours;
    Rgba fill;
};

struct FontResource
{
    bk::Buffer data;
    stbtt_fontinfo info{};
    bool valid = false;
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
    const float alpha = a / 255.0F;
    image.pixels[index + 0] = static_cast<std::uint8_t>(r * alpha + image.pixels[index + 0] * (1.0F - alpha));
    image.pixels[index + 1] = static_cast<std::uint8_t>(g * alpha + image.pixels[index + 1] * (1.0F - alpha));
    image.pixels[index + 2] = static_cast<std::uint8_t>(b * alpha + image.pixels[index + 2] * (1.0F - alpha));
    image.pixels[index + 3] = std::max(image.pixels[index + 3], a);
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
    return static_cast<std::uint8_t>(std::strtoul(text.substr(pos, 2).c_str(), nullptr, 16));
}

bool ReadHexColor(const std::string& text, std::size_t pos, Rgba& color)
{
    if (pos + 6 > text.size())
    {
        return false;
    }

    color.r = HexByte(text, pos);
    color.g = HexByte(text, pos + 2);
    color.b = HexByte(text, pos + 4);
    color.a = 255;
    return true;
}

bool IsNumberStart(char c)
{
    return std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' || c == '.';
}

void SkipSvgSeparators(const std::string& text, std::size_t& pos)
{
    while (pos < text.size() && (std::isspace(static_cast<unsigned char>(text[pos])) || text[pos] == ','))
    {
        ++pos;
    }
}

bool ReadSvgNumber(const std::string& text, std::size_t& pos, float& value)
{
    SkipSvgSeparators(text, pos);
    if (pos >= text.size() || !IsNumberStart(text[pos]))
    {
        return false;
    }

    const char* begin = text.c_str() + pos;
    char* end = nullptr;
    value = std::strtof(begin, &end);
    if (end == begin)
    {
        return false;
    }
    pos = static_cast<std::size_t>(end - text.c_str());
    return true;
}

bool ReadSvgFlag(const std::string& text, std::size_t& pos, int& value)
{
    float number = 0.0F;
    if (!ReadSvgNumber(text, pos, number))
    {
        return false;
    }
    value = std::fabs(number) >= 0.5F ? 1 : 0;
    return true;
}

std::string ReadAttribute(const std::string& text, std::size_t elementPos, const char* name)
{
    const std::string key = std::string{name} + "=";
    const std::size_t elementEnd = text.find('>', elementPos);
    const std::size_t pos = text.find(key, elementPos);
    if (pos == std::string::npos || (elementEnd != std::string::npos && pos > elementEnd))
    {
        return {};
    }

    const std::size_t quotePos = pos + key.size();
    if (quotePos >= text.size() || (text[quotePos] != '"' && text[quotePos] != '\''))
    {
        return {};
    }

    const char quote = text[quotePos];
    const std::size_t valueStart = quotePos + 1;
    const std::size_t valueEnd = text.find(quote, valueStart);
    return valueEnd == std::string::npos ? std::string{} : text.substr(valueStart, valueEnd - valueStart);
}

Point CubicPoint(Point p0, Point p1, Point p2, Point p3, float t)
{
    const float inv = 1.0F - t;
    return Point{
        p0.x * inv * inv * inv + p1.x * 3.0F * inv * inv * t + p2.x * 3.0F * inv * t * t + p3.x * t * t * t,
        p0.y * inv * inv * inv + p1.y * 3.0F * inv * inv * t + p2.y * 3.0F * inv * t * t + p3.y * t * t * t,
    };
}

Point QuadraticPoint(Point p0, Point p1, Point p2, float t)
{
    const float inv = 1.0F - t;
    return Point{
        p0.x * inv * inv + p1.x * 2.0F * inv * t + p2.x * t * t,
        p0.y * inv * inv + p1.y * 2.0F * inv * t + p2.y * t * t,
    };
}

float VectorAngle(float ux, float uy, float vx, float vy)
{
    const float len = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
    if (len <= 0.0F)
    {
        return 0.0F;
    }
    const float sign = (ux * vy - uy * vx) < 0.0F ? -1.0F : 1.0F;
    return sign * std::acos(std::clamp((ux * vx + uy * vy) / len, -1.0F, 1.0F));
}

void AddArcPoints(std::vector<Point>& contour, Point from, float rx, float ry, float rotationDeg, int largeArc, int sweep, Point to)
{
    static constexpr float kPi = 3.14159265358979323846F;
    rx = std::fabs(rx);
    ry = std::fabs(ry);
    if (rx <= 0.0F || ry <= 0.0F)
    {
        contour.push_back(to);
        return;
    }

    const float phi = rotationDeg * kPi / 180.0F;
    const float cosPhi = std::cos(phi);
    const float sinPhi = std::sin(phi);
    const float dx = (from.x - to.x) * 0.5F;
    const float dy = (from.y - to.y) * 0.5F;
    const float x1p = cosPhi * dx + sinPhi * dy;
    const float y1p = -sinPhi * dx + cosPhi * dy;
    const float lambda = x1p * x1p / (rx * rx) + y1p * y1p / (ry * ry);
    if (lambda > 1.0F)
    {
        const float scale = std::sqrt(lambda);
        rx *= scale;
        ry *= scale;
    }

    const float rx2 = rx * rx;
    const float ry2 = ry * ry;
    const float x1p2 = x1p * x1p;
    const float y1p2 = y1p * y1p;
    const float denom = rx2 * y1p2 + ry2 * x1p2;
    if (denom <= 0.0F)
    {
        contour.push_back(to);
        return;
    }

    const float sign = largeArc == sweep ? -1.0F : 1.0F;
    const float factor = sign * std::sqrt(std::max(0.0F, (rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2) / denom));
    const float cxp = factor * rx * y1p / ry;
    const float cyp = factor * -ry * x1p / rx;
    const float cx = cosPhi * cxp - sinPhi * cyp + (from.x + to.x) * 0.5F;
    const float cy = sinPhi * cxp + cosPhi * cyp + (from.y + to.y) * 0.5F;
    const float theta1 = VectorAngle(1.0F, 0.0F, (x1p - cxp) / rx, (y1p - cyp) / ry);
    float delta = VectorAngle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry);
    if (sweep == 0 && delta > 0.0F)
    {
        delta -= 2.0F * kPi;
    }
    else if (sweep != 0 && delta < 0.0F)
    {
        delta += 2.0F * kPi;
    }

    const int segments = std::max(6, static_cast<int>(std::ceil(std::fabs(delta) / (kPi / 18.0F))));
    for (int i = 1; i <= segments; ++i)
    {
        const float theta = theta1 + delta * (static_cast<float>(i) / segments);
        contour.push_back(Point{
            cx + rx * std::cos(theta) * cosPhi - ry * std::sin(theta) * sinPhi,
            cy + rx * std::cos(theta) * sinPhi + ry * std::sin(theta) * cosPhi,
        });
    }
}

std::vector<std::vector<Point>> ParseSvgPath(const std::string& d)
{
    std::vector<std::vector<Point>> contours;
    std::vector<Point> contour;
    Point current{};
    Point start{};
    Point lastCubic{};
    Point lastQuad{};
    char command = 0;
    char previous = 0;
    std::size_t pos = 0;

    auto flush = [&]() {
        if (contour.size() >= 2)
        {
            contours.push_back(contour);
        }
        contour.clear();
    };

    while (pos < d.size())
    {
        SkipSvgSeparators(d, pos);
        if (pos >= d.size())
        {
            break;
        }
        if (std::isalpha(static_cast<unsigned char>(d[pos])))
        {
            command = d[pos++];
        }
        else if (command == 0)
        {
            break;
        }

        const bool relative = std::islower(static_cast<unsigned char>(command)) != 0;
        const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(command)));
        if (upper == 'Z')
        {
            if (!contour.empty())
            {
                contour.push_back(start);
                flush();
            }
            current = start;
            previous = command;
            continue;
        }

        bool firstPair = true;
        while (pos < d.size())
        {
            SkipSvgSeparators(d, pos);
            if (pos >= d.size() || std::isalpha(static_cast<unsigned char>(d[pos])))
            {
                break;
            }

            if (upper == 'M' || upper == 'L')
            {
                float vx = 0.0F;
                float vy = 0.0F;
                if (!ReadSvgNumber(d, pos, vx) || !ReadSvgNumber(d, pos, vy))
                {
                    break;
                }
                Point p{relative ? current.x + vx : vx, relative ? current.y + vy : vy};
                if (upper == 'M' && firstPair)
                {
                    flush();
                    start = p;
                }
                contour.push_back(p);
                current = p;
                firstPair = false;
            }
            else if (upper == 'H' || upper == 'V')
            {
                float v = 0.0F;
                if (!ReadSvgNumber(d, pos, v))
                {
                    break;
                }
                Point p = current;
                if (upper == 'H')
                {
                    p.x = relative ? current.x + v : v;
                }
                else
                {
                    p.y = relative ? current.y + v : v;
                }
                contour.push_back(p);
                current = p;
            }
            else if (upper == 'C')
            {
                float x1 = 0.0F;
                float y1 = 0.0F;
                float x2 = 0.0F;
                float y2 = 0.0F;
                float x = 0.0F;
                float y = 0.0F;
                if (!ReadSvgNumber(d, pos, x1) || !ReadSvgNumber(d, pos, y1) ||
                    !ReadSvgNumber(d, pos, x2) || !ReadSvgNumber(d, pos, y2) ||
                    !ReadSvgNumber(d, pos, x) || !ReadSvgNumber(d, pos, y))
                {
                    break;
                }
                Point p1{relative ? current.x + x1 : x1, relative ? current.y + y1 : y1};
                Point p2{relative ? current.x + x2 : x2, relative ? current.y + y2 : y2};
                Point p{relative ? current.x + x : x, relative ? current.y + y : y};
                for (int i = 1; i <= 20; ++i)
                {
                    contour.push_back(CubicPoint(current, p1, p2, p, static_cast<float>(i) / 20.0F));
                }
                current = p;
                lastCubic = p2;
            }
            else if (upper == 'S')
            {
                float x2 = 0.0F;
                float y2 = 0.0F;
                float x = 0.0F;
                float y = 0.0F;
                if (!ReadSvgNumber(d, pos, x2) || !ReadSvgNumber(d, pos, y2) ||
                    !ReadSvgNumber(d, pos, x) || !ReadSvgNumber(d, pos, y))
                {
                    break;
                }
                const char prevUpper = static_cast<char>(std::toupper(static_cast<unsigned char>(previous)));
                Point p1 = (prevUpper == 'C' || prevUpper == 'S') ? Point{current.x * 2.0F - lastCubic.x, current.y * 2.0F - lastCubic.y} : current;
                Point p2{relative ? current.x + x2 : x2, relative ? current.y + y2 : y2};
                Point p{relative ? current.x + x : x, relative ? current.y + y : y};
                for (int i = 1; i <= 20; ++i)
                {
                    contour.push_back(CubicPoint(current, p1, p2, p, static_cast<float>(i) / 20.0F));
                }
                current = p;
                lastCubic = p2;
            }
            else if (upper == 'Q')
            {
                float x1 = 0.0F;
                float y1 = 0.0F;
                float x = 0.0F;
                float y = 0.0F;
                if (!ReadSvgNumber(d, pos, x1) || !ReadSvgNumber(d, pos, y1) ||
                    !ReadSvgNumber(d, pos, x) || !ReadSvgNumber(d, pos, y))
                {
                    break;
                }
                Point p1{relative ? current.x + x1 : x1, relative ? current.y + y1 : y1};
                Point p{relative ? current.x + x : x, relative ? current.y + y : y};
                for (int i = 1; i <= 16; ++i)
                {
                    contour.push_back(QuadraticPoint(current, p1, p, static_cast<float>(i) / 16.0F));
                }
                current = p;
                lastQuad = p1;
            }
            else if (upper == 'T')
            {
                float x = 0.0F;
                float y = 0.0F;
                if (!ReadSvgNumber(d, pos, x) || !ReadSvgNumber(d, pos, y))
                {
                    break;
                }
                const char prevUpper = static_cast<char>(std::toupper(static_cast<unsigned char>(previous)));
                Point p1 = (prevUpper == 'Q' || prevUpper == 'T') ? Point{current.x * 2.0F - lastQuad.x, current.y * 2.0F - lastQuad.y} : current;
                Point p{relative ? current.x + x : x, relative ? current.y + y : y};
                for (int i = 1; i <= 16; ++i)
                {
                    contour.push_back(QuadraticPoint(current, p1, p, static_cast<float>(i) / 16.0F));
                }
                current = p;
                lastQuad = p1;
            }
            else if (upper == 'A')
            {
                float rx = 0.0F;
                float ry = 0.0F;
                float rotation = 0.0F;
                int largeArc = 0;
                int sweep = 0;
                float x = 0.0F;
                float y = 0.0F;
                if (!ReadSvgNumber(d, pos, rx) || !ReadSvgNumber(d, pos, ry) ||
                    !ReadSvgNumber(d, pos, rotation) || !ReadSvgFlag(d, pos, largeArc) ||
                    !ReadSvgFlag(d, pos, sweep) || !ReadSvgNumber(d, pos, x) ||
                    !ReadSvgNumber(d, pos, y))
                {
                    break;
                }
                Point p{relative ? current.x + x : x, relative ? current.y + y : y};
                AddArcPoints(contour, current, rx, ry, rotation, largeArc, sweep, p);
                current = p;
            }
            else
            {
                break;
            }

            previous = command;
        }
    }

    flush();
    return contours;
}

bool PointInContour(const std::vector<Point>& contour, float x, float y)
{
    bool inside = false;
    for (std::size_t i = 0, j = contour.size() - 1; i < contour.size(); j = i++)
    {
        const Point& a = contour[i];
        const Point& b = contour[j];
        if (((a.y > y) != (b.y > y)) &&
            (x < (b.x - a.x) * (y - a.y) / (b.y - a.y + std::numeric_limits<float>::epsilon()) + a.x))
        {
            inside = !inside;
        }
    }
    return inside;
}

bool PointInShape(const SvgShape& shape, float x, float y)
{
    bool inside = false;
    for (const auto& contour : shape.contours)
    {
        if (contour.size() >= 3 && PointInContour(contour, x, y))
        {
            inside = !inside;
        }
    }
    return inside;
}

std::vector<SvgShape> ParseSvgShapes(const std::string& svg)
{
    std::vector<SvgShape> shapes;
    std::size_t pos = 0;
    while ((pos = svg.find("<path", pos)) != std::string::npos)
    {
        const std::string d = ReadAttribute(svg, pos, "d");
        const std::string fill = ReadAttribute(svg, pos, "fill");
        Rgba color{};
        if (!d.empty() && fill.size() == 7 && fill[0] == '#' && ReadHexColor(fill, 1, color))
        {
            SvgShape shape;
            shape.contours = ParseSvgPath(d);
            shape.fill = color;
            if (!shape.contours.empty())
            {
                shapes.push_back(std::move(shape));
            }
        }
        ++pos;
    }
    return shapes;
}

Image RasterizeSvg(const std::string& path, int size)
{
    bk::Buffer data;
    try
    {
        data = bk::FileSystem::Read(path);
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "Failed to read SVG resource %s: %s\n", path.c_str(), ex.what());
        return {};
    }

    if (data.empty())
    {
        return {};
    }

    const std::string svg(reinterpret_cast<const char*>(data.data()), data.size());
    const std::vector<SvgShape> shapes = ParseSvgShapes(svg);
    if (shapes.empty())
    {
        std::fprintf(stderr, "Failed to parse SVG paths: %s\n", path.c_str());
        return {};
    }

    Image image;
    image.width = size;
    image.height = size;
    image.pixels.resize(static_cast<std::size_t>(size * size * 4), 0);

    const float padding = 8.0F;
    const float scale = (static_cast<float>(size) - padding * 2.0F) / 1024.0F;
    for (const SvgShape& shape : shapes)
    {
        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                int covered = 0;
                for (int sy = 0; sy < 2; ++sy)
                {
                    for (int sx = 0; sx < 2; ++sx)
                    {
                        const float px = (static_cast<float>(x) + 0.25F + sx * 0.5F - padding) / scale;
                        const float py = (static_cast<float>(y) + 0.25F + sy * 0.5F - padding) / scale;
                        if (PointInShape(shape, px, py))
                        {
                            ++covered;
                        }
                    }
                }
                if (covered > 0)
                {
                    const int alpha = std::min(255, covered * 64);
                    BlendPixel(image, x, y, shape.fill.r, shape.fill.g, shape.fill.b, static_cast<std::uint8_t>(alpha));
                }
            }
        }
    }
    return image;
}

void DrawSvgPreview(Image& dst, const std::string& path, int x, int y, int size)
{
    FillRect(dst, x, y, size, size, 245, 247, 250, 255);
    const Image svg = RasterizeSvg(path, size);
    if (!svg.pixels.empty())
    {
        Blit(dst, svg, x, y, size, size);
    }
}

const FontResource& GlobalFont()
{
    static FontResource font = [] {
        FontResource result;
#if defined(BKUI_PLATFORM_SWITCH)
        if (R_SUCCEEDED(plInitialize(PlServiceType_User)))
        {
            const PlSharedFontType candidates[] = {
                PlSharedFontType_ExtChineseSimplified,
                PlSharedFontType_ChineseSimplified,
                PlSharedFontType_Standard,
            };

            for (PlSharedFontType type : candidates)
            {
                PlFontData sharedFont{};
                if (R_SUCCEEDED(plGetSharedFontByType(&sharedFont, type)) &&
                    sharedFont.address != nullptr && sharedFont.size > 0)
                {
                    auto* bytes = static_cast<const std::uint8_t*>(sharedFont.address);
                    result.data.assign(bytes, bytes + sharedFont.size);
                    break;
                }
            }
            plExit();
        }

        if (result.data.empty())
        {
            std::fprintf(stderr, "Failed to read Switch shared font; falling back to romfs font.\n");
        }
#endif

        if (result.data.empty())
        {
        try
        {
            result.data = bk::FileSystem::Read("font/switch_font.ttf");
        }
        catch (const std::exception& ex)
        {
            std::fprintf(stderr, "Failed to read font resource font/switch_font.ttf: %s\n", ex.what());
            return result;
        }
        }

        if (result.data.empty())
        {
            std::fprintf(stderr, "Font data is empty.\n");
            return result;
        }

        if (stbtt_InitFont(&result.info, result.data.data(), stbtt_GetFontOffsetForIndex(result.data.data(), 0)) == 0)
        {
            std::fprintf(stderr, "Failed to initialize font data.\n");
            return result;
        }

        result.valid = true;
        return result;
    }();
    return font;
}

void DrawText(Image& dst, const FontResource& font, const std::u32string& text, int x, int y, float px, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    if (!font.valid)
    {
        return;
    }

    const stbtt_fontinfo& info = font.info;
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
    const FontResource& font = GlobalFont();

    Image canvas;
    canvas.width = 1280;
    canvas.height = 720;
    canvas.pixels.resize(static_cast<std::size_t>(canvas.width * canvas.height * 4), 255);

    FillRect(canvas, 0, 0, canvas.width, canvas.height, 22, 28, 36, 255);
    FillRect(canvas, 48, 48, 928, 544, 242, 246, 250, 255);
    FillRect(canvas, 64, 64, 896, 76, 36, 77, 122, 255);



    DrawText(canvas, font, U"BeikUI 资源渲染演示", 92, 82, 34.0F, 255, 255, 255);
    DrawText(canvas, font, U"中文字体 / PNG 图片 / SVG 图片", 92, 158, 26.0F, 32, 42, 55);

    const Image png = LoadPng("images/beiklive.png");
    if (!png.pixels.empty())
    {
        Blit(canvas, png, 96, 218, 336, 220);
    }
    else
    {
        FillRect(canvas, 96, 218, 336, 220, 220, 226, 234, 255);
        DrawText(canvas, font, U"PNG load failed", 164, 316, 22.0F, 120, 40, 40);
    }
    FillRect(canvas, 96, 448, 336, 38, 236, 240, 245, 255);
    DrawText(canvas, font, U"PNG: images/beiklive.png", 112, 454, 20.0F, 20, 34, 48);

    DrawSvgPreview(canvas, "images/1.svg", 592, 210, 220);
    FillRect(canvas, 552, 448, 336, 38, 236, 240, 245, 255);
    DrawText(canvas, font, U"SVG: images/1.svg", 590, 454, 20.0F, 20, 34, 48);

    DrawText(canvas, font, U"Switch 使用 Deko3D，Windows 使用 OpenGL", 196, 526, 24.0F, 56, 64, 74);
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
