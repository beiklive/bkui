#include <bkui/bkui.hpp>



#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <sstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using bk::operator""_i18n;

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

void FillCircle(Image& image, int cx, int cy, int radius, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
{
    const int radiusSquared = radius * radius;
    for (int yy = -radius; yy <= radius; ++yy)
    {
        for (int xx = -radius; xx <= radius; ++xx)
        {
            if (xx * xx + yy * yy <= radiusSquared)
            {
                PutPixel(image, cx + xx, cy + yy, r, g, b, a);
            }
        }
    }
}

Image MakeImage(int width, int height, std::uint8_t r = 0, std::uint8_t g = 0, std::uint8_t b = 0, std::uint8_t a = 0)
{
    Image image;
    image.width = width;
    image.height = height;
    image.pixels.resize(static_cast<std::size_t>(width * height * 4), 0);
    if (r == 0 && g == 0 && b == 0 && a == 0)
    {
        return image;
    }

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            PutPixel(image, x, y, r, g, b, a);
        }
    }

    return image;
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

std::array<Vertex, 6> MakeQuad(float left, float top, float right, float bottom)
{
    return {{
        {{left,  top},    {1.0F, 1.0F, 1.0F}, {0.0F, 0.0F}},
        {{left,  bottom}, {1.0F, 1.0F, 1.0F}, {0.0F, 1.0F}},
        {{right, bottom}, {1.0F, 1.0F, 1.0F}, {1.0F, 1.0F}},
        {{left,  top},    {1.0F, 1.0F, 1.0F}, {0.0F, 0.0F}},
        {{right, bottom}, {1.0F, 1.0F, 1.0F}, {1.0F, 1.0F}},
        {{right, top},    {1.0F, 1.0F, 1.0F}, {1.0F, 0.0F}},
    }};
}

std::array<Vertex, 6> MakePixelQuad(int x, int y, int w, int h, int screenWidth, int screenHeight)
{
    const float left = static_cast<float>(x) / static_cast<float>(screenWidth) * 2.0F - 1.0F;
    const float right = static_cast<float>(x + w) / static_cast<float>(screenWidth) * 2.0F - 1.0F;
    const float top = 1.0F - static_cast<float>(y) / static_cast<float>(screenHeight) * 2.0F;
    const float bottom = 1.0F - static_cast<float>(y + h) / static_cast<float>(screenHeight) * 2.0F;
    return MakeQuad(left, top, right, bottom);
}

std::array<Vertex, 6> MakeRotatedPixelQuad(int cx, int cy, int w, int h, float angle, int screenWidth, int screenHeight)
{
    const float halfW = static_cast<float>(w) * 0.5F;
    const float halfH = static_cast<float>(h) * 0.5F;
    const float cosA = std::cos(angle);
    const float sinA = std::sin(angle);

    auto toNdc = [&](float px, float py) -> std::array<float, 2>
    {
        return {{
            px / static_cast<float>(screenWidth) * 2.0F - 1.0F,
            1.0F - py / static_cast<float>(screenHeight) * 2.0F,
        }};
    };

    auto rotate = [&](float localX, float localY) -> std::array<float, 2>
    {
        return toNdc(
            static_cast<float>(cx) + localX * cosA - localY * sinA,
            static_cast<float>(cy) + localX * sinA + localY * cosA);
    };

    const auto p0 = rotate(-halfW, -halfH);
    const auto p1 = rotate(-halfW, halfH);
    const auto p2 = rotate(halfW, halfH);
    const auto p3 = rotate(halfW, -halfH);

    return {{
        {{p0[0], p0[1]}, {1.0F, 1.0F, 1.0F}, {0.0F, 0.0F}},
        {{p1[0], p1[1]}, {1.0F, 1.0F, 1.0F}, {0.0F, 1.0F}},
        {{p2[0], p2[1]}, {1.0F, 1.0F, 1.0F}, {1.0F, 1.0F}},
        {{p0[0], p0[1]}, {1.0F, 1.0F, 1.0F}, {0.0F, 0.0F}},
        {{p2[0], p2[1]}, {1.0F, 1.0F, 1.0F}, {1.0F, 1.0F}},
        {{p3[0], p3[1]}, {1.0F, 1.0F, 1.0F}, {1.0F, 0.0F}},
    }};
}

void BlitCircle(Image& dst, const Image& src, int cx, int cy, int diameter)
{
    if (src.pixels.empty() || diameter <= 0)
    {
        return;
    }

    const int radius = diameter / 2;
    const int radiusSquared = radius * radius;
    const int left = cx - radius;
    const int top = cy - radius;

    for (int yy = 0; yy < diameter; ++yy)
    {
        for (int xx = 0; xx < diameter; ++xx)
        {
            const int dx = xx - radius;
            const int dy = yy - radius;
            if (dx * dx + dy * dy > radiusSquared)
            {
                continue;
            }

            const int sx = xx * src.width / diameter;
            const int sy = yy * src.height / diameter;
            const std::size_t si = static_cast<std::size_t>((sy * src.width + sx) * 4);
            BlendPixel(
                dst,
                left + xx,
                top + yy,
                src.pixels[si + 0],
                src.pixels[si + 1],
                src.pixels[si + 2],
                src.pixels[si + 3]);
        }
    }
}

void BlitTransformed(Image& dst, const Image& src, int x, int y, int w, int h, float angle, float scale)
{
    const float cx = x + w * 0.5F;
    const float cy = y + h * 0.5F;
    const float cos_a = std::cos(-angle);
    const float sin_a = std::sin(-angle);
    const float inv_scale = 1.0F / scale;
    const float src_half_w = src.width * 0.5F;
    const float src_half_h = src.height * 0.5F;

    for (int yy = y; yy < y + h; ++yy)
    {
        for (int xx = x; xx < x + w; ++xx)
        {
            const float rel_x = (xx - cx) * inv_scale;
            const float rel_y = (yy - cy) * inv_scale;
            const float src_x = rel_x * cos_a - rel_y * sin_a + src_half_w;
            const float src_y = rel_x * sin_a + rel_y * cos_a + src_half_h;

            if (src_x < 0.0F || src_y < 0.0F || src_x >= static_cast<float>(src.width - 1) || src_y >= static_cast<float>(src.height - 1))
            {
                continue;
            }

            const int x0 = static_cast<int>(std::floor(src_x));
            const int y0 = static_cast<int>(std::floor(src_y));
            const int x1 = std::min(x0 + 1, src.width - 1);
            const int y1 = std::min(y0 + 1, src.height - 1);
            const float tx = src_x - static_cast<float>(x0);
            const float ty = src_y - static_cast<float>(y0);

            auto sample = [&](int sx, int sy, int channel) -> float
            {
                const std::size_t index = static_cast<std::size_t>((sy * src.width + sx) * 4 + channel);
                return static_cast<float>(src.pixels[index]);
            };

            auto lerp = [](float a, float b, float t) -> float
            {
                return a + (b - a) * t;
            };

            const float sr0 = lerp(sample(x0, y0, 0), sample(x1, y0, 0), tx);
            const float sr1 = lerp(sample(x0, y1, 0), sample(x1, y1, 0), tx);
            const float sg0 = lerp(sample(x0, y0, 1), sample(x1, y0, 1), tx);
            const float sg1 = lerp(sample(x0, y1, 1), sample(x1, y1, 1), tx);
            const float sb0 = lerp(sample(x0, y0, 2), sample(x1, y0, 2), tx);
            const float sb1 = lerp(sample(x0, y1, 2), sample(x1, y1, 2), tx);
            const float sa0 = lerp(sample(x0, y0, 3), sample(x1, y0, 3), tx);
            const float sa1 = lerp(sample(x0, y1, 3), sample(x1, y1, 3), tx);

            BlendPixel(
                dst,
                xx,
                yy,
                static_cast<std::uint8_t>(lerp(sr0, sr1, ty)),
                static_cast<std::uint8_t>(lerp(sg0, sg1, ty)),
                static_cast<std::uint8_t>(lerp(sb0, sb1, ty)),
                static_cast<std::uint8_t>(lerp(sa0, sa1, ty)));
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
        float svgMinX = std::numeric_limits<float>::max();
        float svgMaxX = -std::numeric_limits<float>::max();
        float svgMinY = std::numeric_limits<float>::max();
        float svgMaxY = -std::numeric_limits<float>::max();
        for (const auto& contour : shape.contours)
        {
            for (const auto& p : contour)
            {
                if (p.x < svgMinX) svgMinX = p.x;
                if (p.x > svgMaxX) svgMaxX = p.x;
                if (p.y < svgMinY) svgMinY = p.y;
                if (p.y > svgMaxY) svgMaxY = p.y;
            }
        }

        const int pxMinX = std::max(0, static_cast<int>(std::floor(svgMinX * scale + padding - 1.0F)));
        const int pxMaxX = std::min(size - 1, static_cast<int>(std::ceil(svgMaxX * scale + padding + 1.0F)));
        const int pxMinY = std::max(0, static_cast<int>(std::floor(svgMinY * scale + padding - 1.0F)));
        const int pxMaxY = std::min(size - 1, static_cast<int>(std::ceil(svgMaxY * scale + padding + 1.0F)));

        for (int y = pxMinY; y <= pxMaxY; ++y)
        {
            for (int x = pxMinX; x <= pxMaxX; ++x)
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
            return result;
        }

        if (!result.Valid())
        {
            std::fprintf(stderr, "No valid platform font loaded.\n");
            return result;
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

void DrawText(Image& dst, const FontResource& font, const std::u32string& text, int x, int y, float px, std::uint8_t r, std::uint8_t g, std::uint8_t b);

std::u32string Utf8ToUtf32(const std::string& text)
{
    std::u32string result;
    result.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size())
    {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80)
        {
            result.push_back(static_cast<char32_t>(c));
            ++i;
        }
        else if ((c & 0xE0) == 0xC0 && i + 1 < text.size())
        {
            const char32_t cp =
                (static_cast<char32_t>(c & 0x1F) << 6) |
                static_cast<char32_t>(static_cast<unsigned char>(text[i + 1]) & 0x3F);
            result.push_back(cp);
            i += 2;
        }
        else if ((c & 0xF0) == 0xE0 && i + 2 < text.size())
        {
            const char32_t cp =
                (static_cast<char32_t>(c & 0x0F) << 12) |
                (static_cast<char32_t>(static_cast<unsigned char>(text[i + 1]) & 0x3F) << 6) |
                static_cast<char32_t>(static_cast<unsigned char>(text[i + 2]) & 0x3F);
            result.push_back(cp);
            i += 3;
        }
        else if ((c & 0xF8) == 0xF0 && i + 3 < text.size())
        {
            const char32_t cp =
                (static_cast<char32_t>(c & 0x07) << 18) |
                (static_cast<char32_t>(static_cast<unsigned char>(text[i + 1]) & 0x3F) << 12) |
                (static_cast<char32_t>(static_cast<unsigned char>(text[i + 2]) & 0x3F) << 6) |
                static_cast<char32_t>(static_cast<unsigned char>(text[i + 3]) & 0x3F);
            result.push_back(cp);
            i += 4;
        }
        else
        {
            result.push_back(U'?');
            ++i;
        }
    }
    return result;
}

std::string NarrowCString(const char* text)
{
    return text == nullptr ? std::string{} : std::string{text};
}

std::u32string ToUtf32(const std::string& text)
{
    return Utf8ToUtf32(text);
}

std::string FormatFloat(float value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << value;
    return stream.str();
}

std::string JoinKeys(const bk::InputState& input)
{
    if (input.keysDownCount == 0)
    {
        return "None";
    }

    std::string result;
    for (std::size_t i = 0; i < input.keysDownCount; ++i)
    {
        if (i > 0)
        {
            result += "  ";
        }
        result += input.keysDown[i].name;
    }
    return result;
}

Image BuildStatusPanel(const FontResource& font, const bk::InputState& input, const bk::TextInputStatus& imeStatus)
{
    Image panel = MakeImage(532, 228, 252, 253, 255, 235);
    FillRect(panel, 0, 0, panel.width, 2, 88, 101, 242, 255);
    FillRect(panel, 0, panel.height - 1, panel.width, 1, 221, 226, 234, 255);

    DrawText(panel, font, "status_panel/title"_i18n.u32(), 20, 18, 28.0F, 24, 35, 52);
    DrawText(panel, font, "status_panel/tip"_i18n.u32(), 20, 50, 18.0F, 99, 111, 126);

    const std::string pointerLine =
        "status_panel/mouse"_i18n(
            FormatFloat(input.mousePosition.x),
            FormatFloat(input.mousePosition.y),
            std::string(input.mouseLeftDown ? "  L" : "") +
                (input.mouseMiddleDown ? "  M" : "") +
                (input.mouseRightDown ? "  R" : ""));
    DrawText(panel, font, ToUtf32(pointerLine), 20, 86, 20.0F, 32, 43, 58);

    std::string touchLine = "Touch: ";
    if (input.touchCount == 0)
    {
        touchLine = "status_panel/touch_inactive"_i18n;
    }
    else
    {
        const auto& touch = input.touchPoints[0];
        touchLine = "status_panel/touch_active"_i18n(
                        std::to_string(touch.id),
                        FormatFloat(touch.position.x),
                        FormatFloat(touch.position.y))
                        ;
    }
    DrawText(panel, font, ToUtf32(touchLine), 20, 116, 20.0F, 32, 43, 58);

    const std::string lastKey = "status_panel/last_key"_i18n(
        input.lastKeyEvent.name[0] != '\0' ? input.lastKeyEvent.name : "None",
        std::string(input.lastKeyEvent.down ? " (down)" : (input.keyReleased ? " (up)" : "")));
    DrawText(panel, font, ToUtf32(lastKey), 20, 146, 20.0F, 32, 43, 58);

    const std::string heldKeys = "status_panel/held_keys"_i18n(JoinKeys(input));
    DrawText(panel, font, ToUtf32(heldKeys), 20, 176, 20.0F, 32, 43, 58);

    std::string imeLine = "IME: ";
    imeLine += imeStatus.active ? "active" : "idle";
    if (imeStatus.submitted)
    {
        imeLine += "  submitted=" + NarrowCString(imeStatus.text);
    }
    else if (imeStatus.canceled)
    {
        imeLine += "  canceled";
    }
    else if (imeStatus.editing || imeStatus.composition[0] != '\0')
    {
        imeLine += "  composing=" + NarrowCString(imeStatus.composition);
    }
    else if (imeStatus.text[0] != '\0')
    {
        imeLine += "  text=" + NarrowCString(imeStatus.text);
    }
    DrawText(panel, font, ToUtf32(imeLine), 20, 206, 18.0F, 71, 85, 104);

    return panel;
}

Image BuildDraggableSquareLayer(int width, int height, int squareX, int squareY, int squareSize, bool dragging)
{
    Image layer = MakeImage(width, height, 0, 0, 0, 0);

    const std::uint8_t borderR = dragging ? 255 : 70;
    const std::uint8_t borderG = dragging ? 196 : 130;
    const std::uint8_t borderB = dragging ? 90 : 255;
    const std::uint8_t fillR = dragging ? 255 : 100;
    const std::uint8_t fillG = dragging ? 233 : 179;
    const std::uint8_t fillB = dragging ? 173 : 255;

    FillRect(layer, squareX, squareY, squareSize, squareSize, borderR, borderG, borderB, 255);
    FillRect(layer, squareX + 4, squareY + 4, squareSize - 8, squareSize - 8, fillR, fillG, fillB, 255);
    return layer;
}

void DrawText(Image& dst, const FontResource& font, const std::u32string& text, int x, int y, float px, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    const FontResource::Face* primaryFace = font.PrimaryFace();
    if (primaryFace == nullptr)
    {
        return;
    }

    const float primaryScale = stbtt_ScaleForPixelHeight(&primaryFace->info, px);
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&primaryFace->info, &ascent, &descent, &lineGap);
    int cursor = x;
    const int baseline = y + static_cast<int>(ascent * primaryScale);

    for (char32_t cp : text)
    {
        const FontResource::Face* face = SelectFontFace(font, cp);
        if (face == nullptr)
        {
            continue;
        }

        const stbtt_fontinfo& info = face->info;
        const float scale = stbtt_ScaleForPixelHeight(&info, px);
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
                        BlendPixel(dst, cursor + x0 + xx, baseline + y0 + yy, r, g, b, a);
                    }
                }
            }
        }
        cursor += static_cast<int>(advance * scale);
    }
}
}
int main(int argc, char** argv)
{
    try
    {
        bk::FileSystem::Init(argc > 0 ? argv[0] : nullptr);
        bk::FileSystem::Mount("resources");
        bk::I18n::Instance().Init("i18n");

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

        const FontResource& font = GlobalFont();
        const Image avatarImage = LoadPng("images/beiklive.png");
        const Image svgImage = RasterizeSvg("images/1.svg", 320);

        const int leftPaneWidth = 608;
        const int avatarCenterX = 128;
        const int avatarCenterY = 112;
        const int avatarDiameter = 108;
        const int svgStageX = 84;
        const int svgStageY = 214;
        const int svgStageWidth = 432;
        const int svgStageHeight = 392;
        const int svgImageX = 140;
        const int svgImageY = 258;
        const int svgImageSize = 320;
        const int statusPanelX = 656;
        const int statusPanelY = 430;
        const int statusPanelWidth = 532;
        const int statusPanelHeight = 228;
        const int dragLayerX = 700;
        const int dragLayerY = 232;
        const int dragLayerWidth = 220;
        const int dragLayerHeight = 140;
        const int dragSquareSize = 72;
        const int screenWidth = 1280;
        const int screenHeight = 720;

        const Image staticBackground = [&]() {
            Image bg;
            bg.width = 1280;
            bg.height = 720;
            bg.pixels.resize(static_cast<std::size_t>(bg.width * bg.height * 4), 255);

            FillRect(bg, 0, 0, bg.width, bg.height, 15, 19, 26, 255);
            FillRect(bg, 0, 0, leftPaneWidth, bg.height, 24, 33, 46, 255);
            FillRect(bg, leftPaneWidth, 0, bg.width - leftPaneWidth, bg.height, 245, 247, 250, 255);
            FillRect(bg, leftPaneWidth - 1, 0, 2, bg.height, 59, 71, 89, 255);

            FillCircle(bg, avatarCenterX, avatarCenterY, avatarDiameter / 2 + 6, 100, 179, 255, 255);
            FillCircle(bg, avatarCenterX, avatarCenterY, avatarDiameter / 2, 232, 238, 246, 255);
            if (!avatarImage.pixels.empty())
            {
                BlitCircle(bg, avatarImage, avatarCenterX, avatarCenterY, avatarDiameter);
            }
            else
            {
                FillCircle(bg, avatarCenterX, avatarCenterY, avatarDiameter / 2, 64, 81, 104, 255);
                DrawText(bg, font, U"BK", avatarCenterX - 22, avatarCenterY - 18, 28.0F, 255, 255, 255);
            }

            DrawText(bg, font, "app/title"_i18n.u32(), 208, 72, 34.0F, 255, 255, 255);
            DrawText(bg, font, "app/subtitle"_i18n.u32(), 208, 116, 22.0F, 186, 200, 216);
            DrawText(bg, font, "app/avatar"_i18n.u32(), 208, 150, 20.0F, 120, 210, 255);

            FillRect(bg, svgStageX, svgStageY, svgStageWidth, svgStageHeight, 236, 241, 247, 255);
            FillRect(bg, svgStageX + 10, svgStageY + 10, svgStageWidth - 20, svgStageHeight - 20, 251, 253, 255, 255);
            DrawText(bg, font, "svg/showcase"_i18n.u32(), 96, 218, 24.0F, 37, 53, 74);
            DrawText(bg, font, "svg/desc"_i18n.u32(), 96, 248, 18.0F, 94, 110, 129);
            DrawText(bg, font, "svg/asset"_i18n.u32(), 96, 614, 20.0F, 173, 188, 205);
            DrawText(bg, font, "svg/platform"_i18n.u32(), 96, 646, 20.0F, 132, 149, 169);

            DrawText(bg, font, "font/title"_i18n.u32(), 656, 68, 34.0F, 24, 35, 52);
            DrawText(bg, font, "font/desc"_i18n.u32(), 656, 108, 21.0F, 87, 100, 116);

            DrawText(bg, font, "font/english"_i18n.u32(), 656, 164, 24.0F, 31, 41, 55);
            DrawText(bg, font, "font/zh_cn"_i18n.u32(), 656, 210, 28.0F, 31, 41, 55);
            DrawText(bg, font, "font/zh_tw"_i18n.u32(), 656, 258, 28.0F, 31, 41, 55);
            DrawText(bg, font, "font/ja"_i18n.u32(), 656, 306, 28.0F, 31, 41, 55);
            DrawText(bg, font, "font/ko"_i18n.u32(), 656, 354, 28.0F, 31, 41, 55);

            DrawText(bg, font, "icon/title"_i18n.u32(), 656, 408, 24.0F, 24, 35, 52);
            DrawText(bg, font, U"\uE88A  \uE87C  \uE8B8  \uE87D", 656, 442, 20.0F, 37, 99, 235);
            DrawText(bg, font, U"\uE88A  \uE87C  \uE8B8  \uE87D", 656, 480, 34.0F, 0, 150, 136);
            DrawText(bg, font, U"\uE88A  \uE87C  \uE8B8  \uE87D", 656, 536, 52.0F, 220, 68, 55);
            DrawText(bg, font, "icon/hint"_i18n.u32(), 656, 604, 22.0F, 107, 120, 138);

            FillRect(bg, dragLayerX - 12, dragLayerY - 12, dragLayerWidth + 24, dragLayerHeight + 24, 236, 241, 247, 255);
            FillRect(bg, dragLayerX - 2, dragLayerY - 2, dragLayerWidth + 4, dragLayerHeight + 4, 220, 226, 234, 255);
            DrawText(bg, font, "drag/title"_i18n.u32(), dragLayerX - 2, dragLayerY - 44, 24.0F, 24, 35, 52);
            DrawText(bg, font, "drag/desc"_i18n.u32(), dragLayerX - 2, dragLayerY - 16, 18.0F, 99, 111, 126);
            return bg;
        }();

        const Image svgLayer = [&]() {
            Image layer = MakeImage(svgImageSize, svgImageSize, 251, 253, 255, 255);
            if (!svgImage.pixels.empty())
            {
                const int inset = 18;
                Blit(layer, svgImage, inset, inset, svgImageSize - inset * 2, svgImageSize - inset * 2);
            }
            return layer;
        }();

        const std::array<Vertex, 6> backgroundVertices = MakeQuad(-1.0F, 1.0F, 1.0F, -1.0F);
        std::array<Vertex, 6> svgVertices = MakeRotatedPixelQuad(
            svgImageX + svgImageSize / 2,
            svgImageY + svgImageSize / 2,
            svgImageSize,
            svgImageSize,
            0.0F,
            screenWidth,
            screenHeight);
        const std::array<Vertex, 6> statusVertices = MakePixelQuad(
            statusPanelX,
            statusPanelY,
            statusPanelWidth,
            statusPanelHeight,
            screenWidth,
            screenHeight);
        const std::array<Vertex, 6> dragVertices = MakePixelQuad(
            dragLayerX,
            dragLayerY,
            dragLayerWidth,
            dragLayerHeight,
            screenWidth,
            screenHeight);

        const bk::InputState initialInput = platform->GetInput();
        const bk::TextInputStatus initialImeStatus = platform->GetTextInputStatus();
        Image statusLayer = BuildStatusPanel(font, initialInput, initialImeStatus);
        int dragSquareX = 72;
        int dragSquareY = 34;
        bool dragActive = false;
        float dragOffsetX = 0.0F;
        float dragOffsetY = 0.0F;
        Image dragLayer = BuildDraggableSquareLayer(dragLayerWidth, dragLayerHeight, dragSquareX, dragSquareY, dragSquareSize, dragActive);

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
        bk::BufferHandle backgroundVertexBuffer = device->CreateBuffer(bk::BufferDesc{
            bk::BufferUsage::Vertex,
            sizeof(Vertex) * backgroundVertices.size(),
            backgroundVertices.data(),
        });
        bk::BufferHandle svgVertexBuffer = device->CreateBuffer(bk::BufferDesc{
            bk::BufferUsage::Vertex,
            sizeof(Vertex) * svgVertices.size(),
            svgVertices.data(),
        });
        bk::TextureHandle backgroundTexture = device->CreateTexture(bk::TextureDesc{
            static_cast<std::uint32_t>(staticBackground.width),
            static_cast<std::uint32_t>(staticBackground.height),
            staticBackground.pixels.data(),
        });
        bk::TextureHandle svgTexture = device->CreateTexture(bk::TextureDesc{
            static_cast<std::uint32_t>(svgLayer.width),
            static_cast<std::uint32_t>(svgLayer.height),
            svgLayer.pixels.data(),
        });
        bk::BufferHandle statusVertexBuffer = device->CreateBuffer(bk::BufferDesc{
            bk::BufferUsage::Vertex,
            sizeof(Vertex) * statusVertices.size(),
            statusVertices.data(),
        });
        bk::TextureHandle statusTexture = device->CreateTexture(bk::TextureDesc{
            static_cast<std::uint32_t>(statusLayer.width),
            static_cast<std::uint32_t>(statusLayer.height),
            statusLayer.pixels.data(),
        });
        bk::BufferHandle dragVertexBuffer = device->CreateBuffer(bk::BufferDesc{
            bk::BufferUsage::Vertex,
            sizeof(Vertex) * dragVertices.size(),
            dragVertices.data(),
        });
        bk::TextureHandle dragTexture = device->CreateTexture(bk::TextureDesc{
            static_cast<std::uint32_t>(dragLayer.width),
            static_cast<std::uint32_t>(dragLayer.height),
            dragLayer.pixels.data(),
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

        if (!bk::IsValid(backgroundVertexBuffer) || !bk::IsValid(svgVertexBuffer) || !bk::IsValid(statusVertexBuffer) || !bk::IsValid(dragVertexBuffer) ||
            !bk::IsValid(backgroundTexture) || !bk::IsValid(svgTexture) || !bk::IsValid(statusTexture) || !bk::IsValid(dragTexture) ||
            !bk::IsValid(vertexShader) ||
            !bk::IsValid(fragmentShader) || !bk::IsValid(pipeline) ||
            !bk::IsValid(commandBuffer))
        {
            std::fprintf(stderr, "Failed to create resource demo RHI objects.\n");
            return 1;
        }

        auto startTime = std::chrono::steady_clock::now();

        while (platform->IsRunning())
        {
            platform->PollEvents();
            const bk::InputState input = platform->GetInput();
            bk::TextInputStatus imeStatus = platform->GetTextInputStatus();

            auto clampSquare = [&]() {
                dragSquareX = std::clamp(dragSquareX, 0, dragLayerWidth - dragSquareSize);
                dragSquareY = std::clamp(dragSquareY, 0, dragLayerHeight - dragSquareSize);
            };

            auto pointInSquare = [&](float px, float py) {
                const float localX = px - static_cast<float>(dragLayerX);
                const float localY = py - static_cast<float>(dragLayerY);
                return localX >= static_cast<float>(dragSquareX) &&
                    localY >= static_cast<float>(dragSquareY) &&
                    localX < static_cast<float>(dragSquareX + dragSquareSize) &&
                    localY < static_cast<float>(dragSquareY + dragSquareSize);
            };

            if (input.mouseLeftPressed && pointInSquare(input.mousePosition.x, input.mousePosition.y))
            {
                dragActive = true;
                dragOffsetX = input.mousePosition.x - static_cast<float>(dragLayerX + dragSquareX);
                dragOffsetY = input.mousePosition.y - static_cast<float>(dragLayerY + dragSquareY);
            }
            if (input.touchCount > 0 && !dragActive && pointInSquare(input.touchPoints[0].position.x, input.touchPoints[0].position.y))
            {
                dragActive = true;
                dragOffsetX = input.touchPoints[0].position.x - static_cast<float>(dragLayerX + dragSquareX);
                dragOffsetY = input.touchPoints[0].position.y - static_cast<float>(dragLayerY + dragSquareY);
            }
            if (dragActive && !input.mouseLeftDown && input.touchCount == 0)
            {
                dragActive = false;
            }
            if (dragActive)
            {
                if (input.touchCount > 0)
                {
                    dragSquareX = static_cast<int>(input.touchPoints[0].position.x - static_cast<float>(dragLayerX) - dragOffsetX);
                    dragSquareY = static_cast<int>(input.touchPoints[0].position.y - static_cast<float>(dragLayerY) - dragOffsetY);
                }
                else
                {
                    dragSquareX = static_cast<int>(input.mousePosition.x - static_cast<float>(dragLayerX) - dragOffsetX);
                    dragSquareY = static_cast<int>(input.mousePosition.y - static_cast<float>(dragLayerY) - dragOffsetY);
                }
                clampSquare();
            }

            if (input.keyPressed && std::strcmp(input.lastKeyEvent.name, "I") == 0 && platform->GetImeManager() != nullptr)
            {
                platform->GetImeManager()->OpenForText(
                    [](std::string) {},
                    "BeikUI IME",
                    "Type something here",
                    32,
                    imeStatus.text);
                imeStatus = platform->GetTextInputStatus();
            }

            const auto now = std::chrono::steady_clock::now();
            const float elapsed = std::chrono::duration<float>(now - startTime).count();
            const float angle = elapsed * 3.14159265358979323846F * 0.45F;
            const float scale = 0.82F + 0.24F * (0.5F + 0.5F * std::sin(elapsed * 3.14159265358979323846F * 0.90F));
            const int offsetX = static_cast<int>(std::sin(elapsed * 1.7F) * 18.0F);
            const int offsetY = static_cast<int>(std::cos(elapsed * 1.3F) * 14.0F);
            const int animatedSize = static_cast<int>(static_cast<float>(svgImageSize) * scale);
            const int centerX = svgImageX + svgImageSize / 2 + offsetX;
            const int centerY = svgImageY + svgImageSize / 2 + offsetY;
            svgVertices = MakeRotatedPixelQuad(centerX, centerY, animatedSize, animatedSize, angle, screenWidth, screenHeight);

            if (!device->UpdateBuffer(svgVertexBuffer, bk::BufferDesc{
                bk::BufferUsage::Vertex,
                sizeof(Vertex) * svgVertices.size(),
                svgVertices.data(),
            }))
            {
                std::fprintf(stderr, "Failed to update SVG vertex buffer.\n");
                break;
            }

            statusLayer = BuildStatusPanel(font, input, imeStatus);
            dragLayer = BuildDraggableSquareLayer(dragLayerWidth, dragLayerHeight, dragSquareX, dragSquareY, dragSquareSize, dragActive);
            if (!device->UpdateTexture(statusTexture, bk::TextureDesc{
                static_cast<std::uint32_t>(statusLayer.width),
                static_cast<std::uint32_t>(statusLayer.height),
                statusLayer.pixels.data(),
            }))
            {
                std::fprintf(stderr, "Failed to update status texture.\n");
                break;
            }
            if (!device->UpdateTexture(dragTexture, bk::TextureDesc{
                static_cast<std::uint32_t>(dragLayer.width),
                static_cast<std::uint32_t>(dragLayer.height),
                dragLayer.pixels.data(),
            }))
            {
                std::fprintf(stderr, "Failed to update drag texture.\n");
                break;
            }

            device->BeginFrame(swapchain, bk::RenderPassDesc{bk::ColorRGBA{0.05F, 0.06F, 0.08F, 1.0F}});
            device->BeginCommandBuffer(commandBuffer);
            device->BindPipeline(commandBuffer, pipeline);
            device->BindVertexBuffer(commandBuffer, backgroundVertexBuffer);
            device->BindTexture(commandBuffer, backgroundTexture);
            device->Draw(commandBuffer, static_cast<std::uint32_t>(backgroundVertices.size()));
            device->BindVertexBuffer(commandBuffer, svgVertexBuffer);
            device->BindTexture(commandBuffer, svgTexture);
            device->Draw(commandBuffer, static_cast<std::uint32_t>(svgVertices.size()));
            device->BindVertexBuffer(commandBuffer, statusVertexBuffer);
            device->BindTexture(commandBuffer, statusTexture);
            device->Draw(commandBuffer, static_cast<std::uint32_t>(statusVertices.size()));
            device->BindVertexBuffer(commandBuffer, dragVertexBuffer);
            device->BindTexture(commandBuffer, dragTexture);
            device->Draw(commandBuffer, static_cast<std::uint32_t>(dragVertices.size()));
            device->EndCommandBuffer(commandBuffer);
            device->Submit(commandBuffer);
            device->EndFrame(swapchain);
        }

        device->DestroyCommandBuffer(commandBuffer);
        device->DestroyPipeline(pipeline);
        device->DestroyShader(fragmentShader);
        device->DestroyShader(vertexShader);
        device->DestroyTexture(dragTexture);
        device->DestroyTexture(statusTexture);
        device->DestroyTexture(svgTexture);
        device->DestroyTexture(backgroundTexture);
        device->DestroyBuffer(dragVertexBuffer);
        device->DestroyBuffer(statusVertexBuffer);
        device->DestroyBuffer(svgVertexBuffer);
        device->DestroyBuffer(backgroundVertexBuffer);
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
