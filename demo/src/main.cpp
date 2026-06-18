#include <bkui/bkui.hpp>

#include "demo_support.cpp"

#if !defined(BKUI_PLATFORM_SWITCH)
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_video.h>
#else
#include <bkui/render/backend/deko3d/Deko3DDevice.hpp>
#endif

#if defined(DrawText)
#undef DrawText
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <memory>
#include <sstream>
#include <string>

namespace
{
constexpr float kDesignWidth = 1280.0F;
constexpr float kDesignHeight = 720.0F;
constexpr std::size_t kMaxPulseCount = 64;
constexpr float kPulseLifetimeSeconds = 1.0F;
constexpr int kGridColumns = 45;
constexpr int kGridRows = 25;
constexpr int kSwitchTextureWidth = 1280;
constexpr int kSwitchTextureHeight = 720;

struct Float3
{
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
};

struct Pulse
{
    float x = 0.5F;
    float y = 0.5F;
    float age = kPulseLifetimeSeconds;
    float strength = 0.0F;
};

float Fract(float value)
{
    return value - std::floor(value);
}

float Clamp01(float value)
{
    return std::clamp(value, 0.0F, 1.0F);
}

Float3 Scale(const Float3& color, float factor)
{
    return Float3{
        color.r * factor,
        color.g * factor,
        color.b * factor,
    };
}

Float3 Mix(const Float3& a, const Float3& b, float t)
{
    const float clamped = Clamp01(t);
    return Float3{
        a.r + (b.r - a.r) * clamped,
        a.g + (b.g - a.g) * clamped,
        a.b + (b.b - a.b) * clamped,
    };
}

Float3 ToneMap(const Float3& color)
{
    return Float3{
        color.r / (1.0F + color.r * 0.35F),
        color.g / (1.0F + color.g * 0.35F),
        color.b / (1.0F + color.b * 0.35F),
    };
}

float Hash12(float x, float y)
{
    float p3x = Fract(x * 0.1031F);
    float p3y = Fract(y * 0.1031F);
    float p3z = Fract(x * 0.1031F);
    const float sum = p3x * (p3y + 33.33F) + p3y * (p3z + 33.33F) + p3z * (p3x + 33.33F);
    p3x += sum;
    p3y += sum;
    p3z += sum;
    return Fract((p3x + p3y) * p3z);
}

Float3 Palette(float t)
{
    constexpr float tau = 6.28318F;
    return Float3{
        0.5F + 0.5F * std::cos(tau * (t + 0.00F)),
        0.5F + 0.5F * std::cos(tau * (t + 0.19F)),
        0.5F + 0.5F * std::cos(tau * (t + 0.41F)),
    };
}

Float3 RegionColor(float cellX, float cellY)
{
    const float regionX = std::floor(cellX / 4.0F);
    const float regionY = std::floor(cellY / 3.0F);
    const float regionSeed = Hash12(regionX + 1.37F, regionY + 4.91F);
    const float localSeed = Hash12(cellX * 0.37F + 8.12F, cellY * 0.37F + 2.45F);
    const float hue = Fract(regionSeed * 0.77F + localSeed * 0.33F);
    const Float3 tint = Palette(hue);
    return Mix(Float3{0.28F, 0.72F, 1.00F}, tint, 0.75F);
}

float ComputePulseGlow(float cellU, float cellV, float aspectX, const std::array<Pulse, kMaxPulseCount>& pulses)
{
    const float cellCenterX = (cellU - 0.5F) * aspectX;
    const float cellCenterY = cellV - 0.5F;

    float pulseGlow = 0.0F;
    for (const Pulse& pulse : pulses)
    {
        if (pulse.strength <= 0.0F)
        {
            continue;
        }

        const float progress = Clamp01(pulse.age / kPulseLifetimeSeconds);
        const float life = 1.0F - progress;
        const float pulseCenterX = (pulse.x - 0.5F) * aspectX;
        const float pulseCenterY = pulse.y - 0.5F;
        const float dx = cellCenterX - pulseCenterX;
        const float dy = cellCenterY - pulseCenterY;
        const float dist = std::sqrt(dx * dx + dy * dy);

        const float radius = 0.78F * progress;
        const float band = 0.018F + (0.050F - 0.018F) * progress;
        const float invBand = 1.0F / std::max(band, 0.0001F);
        const float ring = std::exp(-std::pow((dist - radius) * invBand, 2.0F) * 10.0F);
        const float haloBand = std::max(band * 1.9F, 0.0001F);
        const float halo = std::exp(-std::pow((dist - radius * 0.72F) / haloBand, 2.0F) * 3.0F) * 0.30F;
        const float core = std::exp(-dist * 20.0F) * std::exp(-progress * 12.0F) * 0.55F;
        const float fade = life * life;

        pulseGlow += (ring + halo + core) * fade * pulse.strength;
    }

    return std::min(pulseGlow, 1.8F);
}

std::string BoolText(bool value)
{
    return value ? "Y" : "N";
}

std::string BuildTouchDebugText(const bk::InputState& input, bk::Vector2 logicalSize)
{
    std::ostringstream stream;
    stream << "touchCount=" << input.touchCount
           << " moved=" << BoolText(input.touchMoved);

    if (logicalSize.x > 0.0F && logicalSize.y > 0.0F)
    {
        stream << "  logical=" << static_cast<int>(logicalSize.x)
               << "x" << static_cast<int>(logicalSize.y);
    }

    for (std::size_t index = 0; index < input.touchCount && index < input.touchPoints.size(); ++index)
    {
        const auto& touch = input.touchPoints[index];
        stream << "\n#"
               << index
               << " id=" << touch.id
               << " d=" << BoolText(touch.down)
               << " p=" << BoolText(touch.pressed)
               << " r=" << BoolText(touch.released)
               << " x=" << static_cast<int>(std::round(touch.position.x))
               << " y=" << static_cast<int>(std::round(touch.position.y));
    }

    if (input.touchCount == 0)
    {
        stream << "\nno active touches";
    }

    return stream.str();
}

void DrawMultilineText(
    demo::Image& image,
    const std::string& text,
    int x,
    int y,
    float pixelHeight,
    std::uint8_t r,
    std::uint8_t g,
    std::uint8_t b)
{
    std::size_t start = 0;
    int line = 0;
    while (start <= text.size())
    {
        const std::size_t end = text.find('\n', start);
        const std::string lineText = end == std::string::npos
            ? text.substr(start)
            : text.substr(start, end - start);
        auto drawTextFn = &demo::DrawText;
        drawTextFn(
            image,
            demo::GlobalFont(),
            demo::Utf8ToUtf32(lineText),
            x,
            y + static_cast<int>(std::round(line * (pixelHeight + 6.0F))),
            pixelHeight,
            r,
            g,
            b,
            nullptr);
        if (end == std::string::npos)
        {
            break;
        }
        start = end + 1;
        ++line;
    }
}

std::array<float, 24> BuildOverlayQuad(float x, float y, float width, float height, bk::Vector2 windowSize)
{
    const float left = (x / windowSize.x) * 2.0F - 1.0F;
    const float right = ((x + width) / windowSize.x) * 2.0F - 1.0F;
    const float top = 1.0F - (y / windowSize.y) * 2.0F;
    const float bottom = 1.0F - ((y + height) / windowSize.y) * 2.0F;

    return {{
        left,  top,    0.0F, 0.0F,
        left,  bottom, 0.0F, 1.0F,
        right, bottom, 1.0F, 1.0F,
        left,  top,    0.0F, 0.0F,
        right, bottom, 1.0F, 1.0F,
        right, top,    1.0F, 0.0F,
    }};
}

class PulseTrailState
{
public:
    void Update(const bk::InputState& input, bk::Vector2 logicalSize, float deltaSeconds)
    {
        UpdatePulses(deltaSeconds);
        UpdatePointerTrail(input, logicalSize);
    }

    const std::array<Pulse, kMaxPulseCount>& Pulses() const
    {
        return pulses_;
    }

private:
    struct TouchTracker
    {
        std::int64_t id = -1;
        bool active = false;
        bk::Vector2 lastPointer{};
    };

    void UpdatePulses(float deltaSeconds)
    {
        for (Pulse& pulse : pulses_)
        {
            if (pulse.strength <= 0.0F)
            {
                continue;
            }

            pulse.age += deltaSeconds;
            if (pulse.age >= kPulseLifetimeSeconds)
            {
                pulse.age = kPulseLifetimeSeconds;
                pulse.strength = 0.0F;
            }
        }
    }

    void UpdatePointerTrail(const bk::InputState& input, bk::Vector2 logicalSize)
    {
        if (logicalSize.x <= 0.0F || logicalSize.y <= 0.0F)
        {
            mouseTracked_ = false;
            ResetTouchTrackers();
            return;
        }

        UpdateTouchTrails(input, logicalSize);
        UpdateMouseTrail(input, logicalSize);
    }

    void UpdateTouchTrails(const bk::InputState& input, bk::Vector2 logicalSize)
    {
        std::array<bool, bk::InputState::MaxTouchPoints> seen{};

        for (std::size_t index = 0; index < input.touchCount && index < input.touchPoints.size(); ++index)
        {
            const auto& touch = input.touchPoints[index];
            if (!touch.down && !touch.pressed)
            {
                continue;
            }

            const int trackerIndex = GetOrCreateTouchTracker(touch.id);
            if (trackerIndex < 0)
            {
                continue;
            }

            seen[static_cast<std::size_t>(trackerIndex)] = true;
            const bk::Vector2 current{
                Clamp01(touch.position.x / logicalSize.x),
                Clamp01(touch.position.y / logicalSize.y),
            };
            TouchTracker& tracker = touchTrackers_[static_cast<std::size_t>(trackerIndex)];

            if (!tracker.active)
            {
                EmitPulse(current.x, current.y, 1.0F);
                tracker.active = true;
                tracker.id = touch.id;
                tracker.lastPointer = current;
                continue;
            }

            if (DistanceSquared(tracker.lastPointer, current) > kMinPointerStepSquared)
            {
                EmitTrailSegment(tracker.lastPointer, current, 1.0F);
                tracker.lastPointer = current;
            }
        }

        for (std::size_t index = 0; index < touchTrackers_.size(); ++index)
        {
            if (!seen[index])
            {
                touchTrackers_[index] = {};
            }
        }
    }

    void UpdateMouseTrail(const bk::InputState& input, bk::Vector2 logicalSize)
    {
        const bool mouseActive = input.mouseMoved || input.mouseLeftDown || input.mouseLeftPressed;
        if (!mouseActive)
        {
            mouseTracked_ = false;
            return;
        }

        const bk::Vector2 current{
            Clamp01(input.mousePosition.x / logicalSize.x),
            Clamp01(input.mousePosition.y / logicalSize.y),
        };
        const float strength = input.mouseLeftDown ? 1.0F : 0.78F;

        if (!mouseTracked_)
        {
            EmitPulse(current.x, current.y, strength);
            mouseTracked_ = true;
            lastMousePointer_ = current;
            return;
        }

        if (DistanceSquared(lastMousePointer_, current) > kMinPointerStepSquared)
        {
            EmitTrailSegment(lastMousePointer_, current, strength);
            lastMousePointer_ = current;
        }
    }

    int GetOrCreateTouchTracker(std::int64_t id)
    {
        for (std::size_t index = 0; index < touchTrackers_.size(); ++index)
        {
            if (touchTrackers_[index].active && touchTrackers_[index].id == id)
            {
                return static_cast<int>(index);
            }
        }

        for (std::size_t index = 0; index < touchTrackers_.size(); ++index)
        {
            if (!touchTrackers_[index].active)
            {
                touchTrackers_[index].id = id;
                return static_cast<int>(index);
            }
        }

        return -1;
    }

    void ResetTouchTrackers()
    {
        for (TouchTracker& tracker : touchTrackers_)
        {
            tracker = {};
        }
    }

    static float DistanceSquared(bk::Vector2 a, bk::Vector2 b)
    {
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        return dx * dx + dy * dy;
    }

    void EmitTrailSegment(bk::Vector2 from, bk::Vector2 to, float strength)
    {
        const float dx = to.x - from.x;
        const float dy = to.y - from.y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        const float spacing = 1.0F / 72.0F;
        const int steps = std::max(1, static_cast<int>(std::ceil(distance / spacing)));

        for (int index = 1; index <= steps; ++index)
        {
            const float t = static_cast<float>(index) / static_cast<float>(steps);
            EmitPulse(from.x + dx * t, from.y + dy * t, strength);
        }
    }

    void EmitPulse(float x, float y, float strength)
    {
        pulses_[pulseHead_] = Pulse{
            Clamp01(x),
            Clamp01(y),
            0.0F,
            strength,
        };
        pulseHead_ = (pulseHead_ + 1) % pulses_.size();
    }

    std::array<Pulse, kMaxPulseCount> pulses_{};
    std::array<TouchTracker, bk::InputState::MaxTouchPoints> touchTrackers_{};
    std::size_t pulseHead_ = 0;
    bool mouseTracked_ = false;
    bk::Vector2 lastMousePointer_{};
    static constexpr float kMinPointerStepSquared = 0.00001F;
};

#if !defined(BKUI_PLATFORM_SWITCH)
struct GLFunctions
{
    PFNGLCREATESHADERPROC createShader = nullptr;
    PFNGLSHADERSOURCEPROC shaderSource = nullptr;
    PFNGLCOMPILESHADERPROC compileShader = nullptr;
    PFNGLGETSHADERIVPROC getShaderiv = nullptr;
    PFNGLGETSHADERINFOLOGPROC getShaderInfoLog = nullptr;
    PFNGLDELETESHADERPROC deleteShader = nullptr;
    PFNGLCREATEPROGRAMPROC createProgram = nullptr;
    PFNGLATTACHSHADERPROC attachShader = nullptr;
    PFNGLLINKPROGRAMPROC linkProgram = nullptr;
    PFNGLGETPROGRAMIVPROC getProgramiv = nullptr;
    PFNGLGETPROGRAMINFOLOGPROC getProgramInfoLog = nullptr;
    PFNGLDELETEPROGRAMPROC deleteProgram = nullptr;
    PFNGLUSEPROGRAMPROC useProgram = nullptr;
    PFNGLGENVERTEXARRAYSPROC genVertexArrays = nullptr;
    PFNGLBINDVERTEXARRAYPROC bindVertexArray = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC deleteVertexArrays = nullptr;
    PFNGLGENBUFFERSPROC genBuffers = nullptr;
    PFNGLBINDBUFFERPROC bindBuffer = nullptr;
    PFNGLBUFFERDATAPROC bufferData = nullptr;
    PFNGLDELETEBUFFERSPROC deleteBuffers = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC enableVertexAttribArray = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC vertexAttribPointer = nullptr;
    PFNGLGETUNIFORMLOCATIONPROC getUniformLocation = nullptr;
    PFNGLUNIFORM1IPROC uniform1i = nullptr;
    PFNGLUNIFORM1FPROC uniform1f = nullptr;
    PFNGLUNIFORM2FPROC uniform2f = nullptr;
    PFNGLUNIFORM4FVPROC uniform4fv = nullptr;
};

template <typename T>
bool LoadProc(bk::Platform& platform, T& target, const char* name)
{
    target = reinterpret_cast<T>(platform.GetOpenGLProcAddress(name));
    return target != nullptr;
}

bool LoadGL(bk::Platform& platform, GLFunctions& gl)
{
    return
        LoadProc(platform, gl.createShader, "glCreateShader") &&
        LoadProc(platform, gl.shaderSource, "glShaderSource") &&
        LoadProc(platform, gl.compileShader, "glCompileShader") &&
        LoadProc(platform, gl.getShaderiv, "glGetShaderiv") &&
        LoadProc(platform, gl.getShaderInfoLog, "glGetShaderInfoLog") &&
        LoadProc(platform, gl.deleteShader, "glDeleteShader") &&
        LoadProc(platform, gl.createProgram, "glCreateProgram") &&
        LoadProc(platform, gl.attachShader, "glAttachShader") &&
        LoadProc(platform, gl.linkProgram, "glLinkProgram") &&
        LoadProc(platform, gl.getProgramiv, "glGetProgramiv") &&
        LoadProc(platform, gl.getProgramInfoLog, "glGetProgramInfoLog") &&
        LoadProc(platform, gl.deleteProgram, "glDeleteProgram") &&
        LoadProc(platform, gl.useProgram, "glUseProgram") &&
        LoadProc(platform, gl.genVertexArrays, "glGenVertexArrays") &&
        LoadProc(platform, gl.bindVertexArray, "glBindVertexArray") &&
        LoadProc(platform, gl.deleteVertexArrays, "glDeleteVertexArrays") &&
        LoadProc(platform, gl.genBuffers, "glGenBuffers") &&
        LoadProc(platform, gl.bindBuffer, "glBindBuffer") &&
        LoadProc(platform, gl.bufferData, "glBufferData") &&
        LoadProc(platform, gl.deleteBuffers, "glDeleteBuffers") &&
        LoadProc(platform, gl.enableVertexAttribArray, "glEnableVertexAttribArray") &&
        LoadProc(platform, gl.vertexAttribPointer, "glVertexAttribPointer") &&
        LoadProc(platform, gl.getUniformLocation, "glGetUniformLocation") &&
        LoadProc(platform, gl.uniform1i, "glUniform1i") &&
        LoadProc(platform, gl.uniform1f, "glUniform1f") &&
        LoadProc(platform, gl.uniform2f, "glUniform2f") &&
        LoadProc(platform, gl.uniform4fv, "glUniform4fv");
}

GLuint CompileShader(const GLFunctions& gl, GLenum shaderType, const char* source)
{
    const GLuint shader = gl.createShader(shaderType);
    if (shader == 0)
    {
        return 0;
    }

    const GLchar* sources[] = {source};
    gl.shaderSource(shader, 1, sources, nullptr);
    gl.compileShader(shader);

    GLint ok = GL_FALSE;
    gl.getShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE)
    {
        std::array<GLchar, 1024> log{};
        gl.getShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::fprintf(stderr, "Shader compile failed: %s\n", log.data());
        gl.deleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint LinkProgram(const GLFunctions& gl, GLuint vertexShader, GLuint fragmentShader)
{
    const GLuint program = gl.createProgram();
    if (program == 0)
    {
        return 0;
    }

    gl.attachShader(program, vertexShader);
    gl.attachShader(program, fragmentShader);
    gl.linkProgram(program);

    GLint ok = GL_FALSE;
    gl.getProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok == GL_FALSE)
    {
        std::array<GLchar, 1024> log{};
        gl.getProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::fprintf(stderr, "Program link failed: %s\n", log.data());
        gl.deleteProgram(program);
        return 0;
    }

    return program;
}

class OpenGLNeonGridDemo
{
public:
    explicit OpenGLNeonGridDemo(bk::Platform& platform)
        : platform_(platform)
    {
    }

    bool Initialize()
    {
        if (!platform_.CreateOpenGLContext(bk::OpenGLContextDesc{}))
        {
            return false;
        }

        platform_.MakeOpenGLContextCurrent();
        if (!LoadGL(platform_, gl_))
        {
            std::fprintf(stderr, "Failed to load OpenGL procedures for neon grid demo.\n");
            return false;
        }

        EnterFullscreenIfPossible();

        const char* vertexSource = R"(
            #version 330 core
            layout(location = 0) in vec2 inPosition;
            layout(location = 1) in vec2 inUv;
            out vec2 vUv;
            void main()
            {
                vUv = inUv;
                gl_Position = vec4(inPosition, 0.0, 1.0);
            }
        )";

        const char* fragmentSource = R"(
            #version 330 core
            in vec2 vUv;
            out vec4 outColor;

            uniform float uTime;
            uniform vec2 uResolution;
            uniform int uPulseCount;
            uniform vec4 uPulses[64];

            float hash12(vec2 p)
            {
                vec3 p3 = fract(vec3(p.xyx) * 0.1031);
                p3 += dot(p3, p3.yzx + 33.33);
                return fract((p3.x + p3.y) * p3.z);
            }

            vec3 palette(float t)
            {
                return 0.5 + 0.5 * cos(6.28318 * (t + vec3(0.00, 0.19, 0.41)));
            }

            vec3 regionColor(vec2 cell)
            {
                vec2 region = floor(cell / vec2(4.0, 3.0));
                float regionSeed = hash12(region + vec2(1.37, 4.91));
                float localSeed = hash12(cell * 0.37 + vec2(8.12, 2.45));
                float hue = fract(regionSeed * 0.77 + localSeed * 0.33);
                vec3 tint = palette(hue);
                return mix(vec3(0.28, 0.72, 1.00), tint, 0.75);
            }

            float gridLineMask(vec2 gridUv)
            {
                vec2 local = fract(gridUv);
                vec2 edge = min(local, 1.0 - local);
                vec2 fw = fwidth(gridUv);
                float gx = 1.0 - smoothstep(0.0, fw.x * 1.15, edge.x);
                float gy = 1.0 - smoothstep(0.0, fw.y * 1.15, edge.y);
                return max(gx, gy);
            }

            void main()
            {
                vec2 uv = vUv;
                vec2 gridCount = vec2(45.0, 25.0);
                vec2 gridUv = uv * gridCount;
                vec2 cell = floor(gridUv);
                vec2 cellCenterUv = (cell + 0.5) / gridCount;

                vec2 aspect = vec2(uResolution.x / max(uResolution.y, 1.0), 1.0);
                vec2 cellCenter = (cellCenterUv - 0.5) * aspect;
                vec3 tint = regionColor(cell);

                float pulseGlow = 0.0;
                for (int i = 0; i < 64; ++i)
                {
                    if (i >= uPulseCount)
                    {
                        break;
                    }

                    vec4 pulse = uPulses[i];
                    float progress = clamp(pulse.z, 0.0, 1.0);
                    float life = 1.0 - progress;
                    vec2 pulseCenter = (pulse.xy - 0.5) * aspect;
                    float dist = length(cellCenter - pulseCenter);

                    float radius = mix(0.0, 0.78, progress);
                    float band = mix(0.018, 0.050, progress);
                    float ring = exp(-pow((dist - radius) / max(band, 0.0001), 2.0) * 10.0);
                    float halo = exp(-pow((dist - radius * 0.72) / max(band * 1.9, 0.0001), 2.0) * 3.0) * 0.30;
                    float core = exp(-dist * 20.0) * exp(-progress * 12.0) * 0.55;
                    float fade = life * life;

                    pulseGlow += (ring + halo + core) * fade * pulse.w;
                }

                pulseGlow = min(pulseGlow, 1.8);

                float gridMask = gridLineMask(gridUv);
                float cellNoise = 0.96 + 0.04 * sin(dot(cell, vec2(0.8, 1.3)) + uTime * 3.5);

                vec3 color = vec3(0.0);
                color += tint * pulseGlow * 1.45 * cellNoise;
                color += vec3(1.0) * gridMask * (0.82 + min(pulseGlow, 1.0) * 0.40);

                color = color / (1.0 + color * 0.35);
                outColor = vec4(color, 1.0);
            }
        )";

        const char* overlayFragmentSource = R"(
            #version 330 core
            in vec2 vUv;
            out vec4 outColor;

            uniform sampler2D uTexture;

            void main()
            {
                outColor = texture(uTexture, vUv);
            }
        )";

        const GLuint vertexShader = CompileShader(gl_, GL_VERTEX_SHADER, vertexSource);
        const GLuint fragmentShader = CompileShader(gl_, GL_FRAGMENT_SHADER, fragmentSource);
        const GLuint overlayFragmentShader = CompileShader(gl_, GL_FRAGMENT_SHADER, overlayFragmentSource);
        if (vertexShader == 0 || fragmentShader == 0 || overlayFragmentShader == 0)
        {
            if (vertexShader != 0)
            {
                gl_.deleteShader(vertexShader);
            }
            if (fragmentShader != 0)
            {
                gl_.deleteShader(fragmentShader);
            }
            if (overlayFragmentShader != 0)
            {
                gl_.deleteShader(overlayFragmentShader);
            }
            return false;
        }

        program_ = LinkProgram(gl_, vertexShader, fragmentShader);
        overlayProgram_ = LinkProgram(gl_, vertexShader, overlayFragmentShader);
        gl_.deleteShader(vertexShader);
        gl_.deleteShader(fragmentShader);
        gl_.deleteShader(overlayFragmentShader);
        if (program_ == 0 || overlayProgram_ == 0)
        {
            return false;
        }

        constexpr std::array<float, 24> quadVertices = {{
            -1.0F,  1.0F, 0.0F, 0.0F,
            -1.0F, -1.0F, 0.0F, 1.0F,
             1.0F, -1.0F, 1.0F, 1.0F,
            -1.0F,  1.0F, 0.0F, 0.0F,
             1.0F, -1.0F, 1.0F, 1.0F,
             1.0F,  1.0F, 1.0F, 0.0F,
        }};

        gl_.genVertexArrays(1, &vao_);
        gl_.bindVertexArray(vao_);
        gl_.genBuffers(1, &vbo_);
        gl_.bindBuffer(GL_ARRAY_BUFFER, vbo_);
        gl_.bufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices.data(), GL_STATIC_DRAW);
        gl_.enableVertexAttribArray(0);
        gl_.vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(0));
        gl_.enableVertexAttribArray(1);
        gl_.vertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(sizeof(float) * 2));
        gl_.bindBuffer(GL_ARRAY_BUFFER, 0);
        gl_.bindVertexArray(0);

        gl_.genVertexArrays(1, &overlayVao_);
        gl_.bindVertexArray(overlayVao_);
        gl_.genBuffers(1, &overlayVbo_);
        gl_.bindBuffer(GL_ARRAY_BUFFER, overlayVbo_);
        gl_.bufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, nullptr, GL_DYNAMIC_DRAW);
        gl_.enableVertexAttribArray(0);
        gl_.vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(0));
        gl_.enableVertexAttribArray(1);
        gl_.vertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(sizeof(float) * 2));
        gl_.bindBuffer(GL_ARRAY_BUFFER, 0);
        gl_.bindVertexArray(0);

        debugImage_ = demo::MakeImage(
            static_cast<int>(kDesignWidth),
            static_cast<int>(kDesignHeight),
            0,
            0,
            0,
            0);
        glGenTextures(1, &debugTexture_);
        glBindTexture(GL_TEXTURE_2D, debugTexture_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            debugImage_.width,
            debugImage_.height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            debugImage_.pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);

        pulseUniforms_.fill(0.0F);
        return true;
    }

    void Shutdown()
    {
        if (debugTexture_ != 0)
        {
            glDeleteTextures(1, &debugTexture_);
            debugTexture_ = 0;
        }
        if (overlayVbo_ != 0)
        {
            gl_.deleteBuffers(1, &overlayVbo_);
            overlayVbo_ = 0;
        }
        if (overlayVao_ != 0)
        {
            gl_.deleteVertexArrays(1, &overlayVao_);
            overlayVao_ = 0;
        }
        if (overlayProgram_ != 0)
        {
            gl_.deleteProgram(overlayProgram_);
            overlayProgram_ = 0;
        }
        if (vbo_ != 0)
        {
            gl_.deleteBuffers(1, &vbo_);
            vbo_ = 0;
        }
        if (vao_ != 0)
        {
            gl_.deleteVertexArrays(1, &vao_);
            vao_ = 0;
        }
        if (program_ != 0)
        {
            gl_.deleteProgram(program_);
            program_ = 0;
        }
        platform_.DestroyOpenGLContext();
    }

    void Step(
        const bk::InputState& input,
        bk::Vector2 logicalSize,
        float deltaSeconds,
        const bk::RenderQueue& overlayQueue)
    {
        elapsedSeconds_ += deltaSeconds;
        trail_.Update(input, logicalSize, deltaSeconds);
        UploadPulseUniforms();
        UpdateDebugOverlay(input, logicalSize, overlayQueue);
        DrawScene(logicalSize);
    }

private:
    void EnterFullscreenIfPossible()
    {
        SDL_Window* window = SDL_GL_GetCurrentWindow();
        if (window != nullptr)
        {
            SDL_SetWindowFullscreen(window, true);
        }
    }

    void UploadPulseUniforms()
    {
        pulseUniforms_.fill(0.0F);
        pulseCount_ = 0;

        for (const Pulse& pulse : trail_.Pulses())
        {
            if (pulse.strength <= 0.0F)
            {
                continue;
            }

            const std::size_t base = pulseCount_ * 4;
            pulseUniforms_[base + 0] = pulse.x;
            pulseUniforms_[base + 1] = pulse.y;
            pulseUniforms_[base + 2] = Clamp01(pulse.age / kPulseLifetimeSeconds);
            pulseUniforms_[base + 3] = pulse.strength;
            ++pulseCount_;

            if (pulseCount_ >= kMaxPulseCount)
            {
                break;
            }
        }
    }

    void DrawScene(bk::Vector2 logicalSize)
    {
        const bk::Vector2 windowSize = platform_.GetWindowSize();
        glViewport(0, 0, static_cast<GLsizei>(windowSize.x), static_cast<GLsizei>(windowSize.y));
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);

        gl_.useProgram(program_);

        const GLint timeLocation = gl_.getUniformLocation(program_, "uTime");
        if (timeLocation >= 0)
        {
            gl_.uniform1f(timeLocation, elapsedSeconds_);
        }

        const GLint resolutionLocation = gl_.getUniformLocation(program_, "uResolution");
        if (resolutionLocation >= 0)
        {
            gl_.uniform2f(resolutionLocation, logicalSize.x, logicalSize.y);
        }

        const GLint pulseCountLocation = gl_.getUniformLocation(program_, "uPulseCount");
        if (pulseCountLocation >= 0)
        {
            gl_.uniform1i(pulseCountLocation, static_cast<GLint>(pulseCount_));
        }

        const GLint pulsesLocation = gl_.getUniformLocation(program_, "uPulses[0]");
        if (pulsesLocation >= 0)
        {
            gl_.uniform4fv(pulsesLocation, static_cast<GLsizei>(pulseCount_), pulseUniforms_.data());
        }

        gl_.bindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        gl_.bindVertexArray(0);
        gl_.useProgram(0);

        DrawDebugOverlay(windowSize);

        platform_.SwapOpenGLBuffers();
    }

    void UpdateDebugOverlay(
        const bk::InputState& input,
        bk::Vector2 logicalSize,
        const bk::RenderQueue& overlayQueue)
    {
        demo::ClearImage(debugImage_, 0, 0, 0, 0);
        demo::FillRect(debugImage_, 0, 0, 640, 220, 0, 0, 0, 180);
        const std::string text = BuildTouchDebugText(input, logicalSize);
        DrawMultilineText(debugImage_, text, 12, 12, 22.0F, 255, 255, 255);
        demo::RenderQueueToImage(debugImage_, demo::GlobalFont(), overlayQueue);

        glBindTexture(GL_TEXTURE_2D, debugTexture_);
        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            debugImage_.width,
            debugImage_.height,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            debugImage_.pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void DrawDebugOverlay(bk::Vector2 windowSize)
    {
        if (overlayProgram_ == 0 || overlayVao_ == 0 || overlayVbo_ == 0 || debugTexture_ == 0)
        {
            return;
        }

        const auto overlayVertices = BuildOverlayQuad(
            0.0F,
            0.0F,
            windowSize.x,
            windowSize.y,
            windowSize);

        gl_.useProgram(overlayProgram_);
        const GLint textureLocation = gl_.getUniformLocation(overlayProgram_, "uTexture");
        if (textureLocation >= 0)
        {
            gl_.uniform1i(textureLocation, 0);
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindTexture(GL_TEXTURE_2D, debugTexture_);
        gl_.bindVertexArray(overlayVao_);
        gl_.bindBuffer(GL_ARRAY_BUFFER, overlayVbo_);
        gl_.bufferData(GL_ARRAY_BUFFER, sizeof(overlayVertices), overlayVertices.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        gl_.bindBuffer(GL_ARRAY_BUFFER, 0);
        gl_.bindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_BLEND);
        gl_.useProgram(0);
    }

    bk::Platform& platform_;
    GLFunctions gl_{};
    PulseTrailState trail_{};
    GLuint program_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint overlayProgram_ = 0;
    GLuint overlayVao_ = 0;
    GLuint overlayVbo_ = 0;
    GLuint debugTexture_ = 0;
    demo::Image debugImage_{};
    std::array<float, kMaxPulseCount * 4> pulseUniforms_{};
    std::size_t pulseCount_ = 0;
    float elapsedSeconds_ = 0.0F;
};

#else
class SwitchNeonGridDemo
{
public:
    explicit SwitchNeonGridDemo(bk::Platform& platform)
        : platform_(platform)
    {
    }

    bool Initialize()
    {
        device_ = bk::CreateDeko3DDevice(platform_);
        if (!device_ || !device_->Init())
        {
            std::fprintf(stderr, "Failed to initialize deko3d device.\n");
            return false;
        }

        swapchain_ = device_->GetMainSwapchain();
        commandBuffer_ = device_->CreateCommandBuffer();
        if (!bk::IsValid(commandBuffer_))
        {
            std::fprintf(stderr, "Failed to create deko3d command buffer.\n");
            return false;
        }

        if (!CreateRenderResources())
        {
            return false;
        }

        Resize(platform_.GetWindowSize());
        return true;
    }

    void Shutdown()
    {
        if (device_)
        {
            if (bk::IsValid(commandBuffer_))
            {
                device_->DestroyCommandBuffer(commandBuffer_);
                commandBuffer_ = {};
            }
            if (bk::IsValid(pipeline_))
            {
                device_->DestroyPipeline(pipeline_);
                pipeline_ = {};
            }
            if (bk::IsValid(fragmentShader_))
            {
                device_->DestroyShader(fragmentShader_);
                fragmentShader_ = {};
            }
            if (bk::IsValid(vertexShader_))
            {
                device_->DestroyShader(vertexShader_);
                vertexShader_ = {};
            }
            if (bk::IsValid(texture_))
            {
                device_->DestroyTexture(texture_);
                texture_ = {};
            }
            if (bk::IsValid(vertexBuffer_))
            {
                device_->DestroyBuffer(vertexBuffer_);
                vertexBuffer_ = {};
            }
            device_->Shutdown();
            device_.reset();
        }
    }

    void Resize(bk::Vector2 size)
    {
        if (device_)
        {
            device_->Resize(size);
        }
    }

    void Step(
        const bk::InputState& input,
        bk::Vector2 logicalSize,
        float deltaSeconds,
        const bk::RenderQueue& overlayQueue)
    {
        elapsedSeconds_ += deltaSeconds;
        trail_.Update(input, logicalSize, deltaSeconds);
        Rasterize(logicalSize);
        DrawTouchDebugText(input, logicalSize);
        demo::RenderQueueToImage(image_, demo::GlobalFont(), overlayQueue);

        const bk::TextureDesc textureDesc{
            static_cast<std::uint32_t>(image_.width),
            static_cast<std::uint32_t>(image_.height),
            image_.pixels.data(),
        };
        device_->UpdateTexture(texture_, textureDesc);

        device_->BeginFrame(swapchain_, bk::RenderPassDesc{bk::ColorRGBA{0.0F, 0.0F, 0.0F, 1.0F}});
        device_->BeginCommandBuffer(commandBuffer_);
        device_->BindPipeline(commandBuffer_, pipeline_);
        device_->BindVertexBuffer(commandBuffer_, vertexBuffer_);
        device_->BindTexture(commandBuffer_, texture_);
        device_->Draw(commandBuffer_, 6);
        device_->EndCommandBuffer(commandBuffer_);
        device_->Submit(commandBuffer_);
        device_->EndFrame(swapchain_);
    }

private:
    bool CreateRenderResources()
    {
        const std::array<demo::Vertex, 6> quad = demo::MakeFullscreenQuad();
        const bk::BufferDesc bufferDesc{
            bk::BufferUsage::Vertex,
            sizeof(quad),
            quad.data(),
        };
        vertexBuffer_ = device_->CreateBuffer(bufferDesc);
        if (!bk::IsValid(vertexBuffer_))
        {
            std::fprintf(stderr, "Failed to create fullscreen vertex buffer.\n");
            return false;
        }

        image_ = demo::MakeImage(kSwitchTextureWidth, kSwitchTextureHeight, 0, 0, 0, 255);
        const bk::TextureDesc textureDesc{
            static_cast<std::uint32_t>(image_.width),
            static_cast<std::uint32_t>(image_.height),
            image_.pixels.data(),
        };
        texture_ = device_->CreateTexture(textureDesc);
        if (!bk::IsValid(texture_))
        {
            std::fprintf(stderr, "Failed to create fullscreen texture.\n");
            return false;
        }

        vertexShader_ = device_->CreateShader(bk::ShaderDesc{
            bk::ShaderStage::Vertex,
            "shaders/deko_triangle_vsh.dksh",
        });
        fragmentShader_ = device_->CreateShader(bk::ShaderDesc{
            bk::ShaderStage::Fragment,
            "shaders/deko_triangle_fsh.dksh",
        });
        if (!bk::IsValid(vertexShader_) || !bk::IsValid(fragmentShader_))
        {
            std::fprintf(stderr, "Failed to load deko3d demo shaders.\n");
            return false;
        }

        constexpr std::array<bk::VertexAttributeDesc, 3> attributes = {{
            {0, bk::VertexFormat::Float2, offsetof(demo::Vertex, position)},
            {1, bk::VertexFormat::Float3, offsetof(demo::Vertex, color)},
            {2, bk::VertexFormat::Float2, offsetof(demo::Vertex, uv)},
        }};
        const bk::VertexLayoutDesc layout{
            sizeof(demo::Vertex),
            attributes.data(),
            attributes.size(),
        };
        pipeline_ = device_->CreatePipeline(bk::PipelineDesc{
            vertexShader_,
            fragmentShader_,
            layout,
            bk::PrimitiveTopology::Triangles,
        });
        if (!bk::IsValid(pipeline_))
        {
            std::fprintf(stderr, "Failed to create deko3d pipeline.\n");
            return false;
        }

        return true;
    }

    void Rasterize(bk::Vector2 logicalSize)
    {
        demo::ClearImage(image_, 0, 0, 0, 255);

        const float aspectX = logicalSize.y > 0.0F ? logicalSize.x / logicalSize.y : (16.0F / 9.0F);
        constexpr int lineWidth = 1;

        for (int row = 0; row < kGridRows; ++row)
        {
            const int y0 = row * image_.height / kGridRows;
            const int y1 = (row + 1) * image_.height / kGridRows;
            const int innerY = y0 + lineWidth;
            const int innerHeight = std::max(0, y1 - innerY);

            for (int column = 0; column < kGridColumns; ++column)
            {
                const int x0 = column * image_.width / kGridColumns;
                const int x1 = (column + 1) * image_.width / kGridColumns;
                const int innerX = x0 + lineWidth;
                const int innerWidth = std::max(0, x1 - innerX);
                if (innerWidth <= 0 || innerHeight <= 0)
                {
                    continue;
                }

                const float centerU = (static_cast<float>(column) + 0.5F) / static_cast<float>(kGridColumns);
                const float centerV = (static_cast<float>(row) + 0.5F) / static_cast<float>(kGridRows);
                const Float3 tint = RegionColor(static_cast<float>(column), static_cast<float>(row));
                const float pulseGlow = ComputePulseGlow(centerU, centerV, aspectX, trail_.Pulses());
                const float cellNoise = 0.96F +
                    0.04F * std::sin(static_cast<float>(column) * 0.8F + static_cast<float>(row) * 1.3F + elapsedSeconds_ * 3.5F);
                const Float3 color = ToneMap(Scale(tint, pulseGlow * 1.45F * cellNoise));

                if (color.r <= 0.001F && color.g <= 0.001F && color.b <= 0.001F)
                {
                    continue;
                }

                demo::FillRect(
                    image_,
                    innerX,
                    innerY,
                    innerWidth,
                    innerHeight,
                    demo::ToByte(color.r),
                    demo::ToByte(color.g),
                    demo::ToByte(color.b),
                    255);
            }
        }

        for (int column = 0; column <= kGridColumns; ++column)
        {
            const int x = std::min(image_.width - 1, column * image_.width / kGridColumns);
            demo::FillRect(image_, x, 0, lineWidth, image_.height, 255, 255, 255, 255);
        }

        for (int row = 0; row <= kGridRows; ++row)
        {
            const int y = std::min(image_.height - 1, row * image_.height / kGridRows);
            demo::FillRect(image_, 0, y, image_.width, lineWidth, 255, 255, 255, 255);
        }
    }

    void DrawTouchDebugText(const bk::InputState& input, bk::Vector2 logicalSize)
    {
        demo::FillRect(image_, 0, 0, 640, 220, 0, 0, 0, 180);
        const std::string text = BuildTouchDebugText(input, logicalSize);
        DrawMultilineText(image_, text, 12, 12, 22.0F, 255, 255, 255);
    }

    bk::Platform& platform_;
    std::unique_ptr<bk::Device> device_{};
    bk::SwapchainHandle swapchain_{};
    bk::CommandBufferHandle commandBuffer_{};
    bk::BufferHandle vertexBuffer_{};
    bk::TextureHandle texture_{};
    bk::ShaderHandle vertexShader_{};
    bk::ShaderHandle fragmentShader_{};
    bk::PipelineHandle pipeline_{};
    demo::Image image_{};
    PulseTrailState trail_{};
    float elapsedSeconds_ = 0.0F;
};
#endif
}

int main(int argc, char** argv)
{
    try
    {
        bk::Application app;
        bk::ApplicationDesc appDesc;
        appDesc.window.title = "BeikUI Neon Grid Demo";
        appDesc.window.width = static_cast<int>(kDesignWidth);
        appDesc.window.height = static_cast<int>(kDesignHeight);
        appDesc.logicalSize = bk::Vector2{kDesignWidth, kDesignHeight};
        appDesc.clearColor = bk::ColorRGBA{0.0F, 0.0F, 0.0F, 1.0F};
        appDesc.name = "BeikUI Neon Grid Demo";
        appDesc.version = "0.1.0";
        appDesc.identifier = "bkui.demo.neon_grid";
        appDesc.logger.level = bk::LogLevel::Info;
        appDesc.logger.enableConsole = true;
        appDesc.logger.enableColor =
#if defined(BKUI_PLATFORM_SWITCH)
            false;
#else
            true;
#endif

        if (!app.Initialize(appDesc, argc, argv))
        {
            std::fprintf(stderr, "Failed to initialize application.\n");
            return 1;
        }
        bklog.info("Neon grid demo booting.");

        std::unique_ptr<bk::Platform> platform = bk::CreateDefaultPlatform(appDesc.window);
        if (!platform || !platform->Init())
        {
            std::fprintf(stderr, "Failed to initialize platform.\n");
            app.Shutdown();
            return 1;
        }
        bklog.info("Platform initialized.");

        app.SetWindowSize(platform->GetWindowSize());
        app.SetLogOverlayVisible(true);
#if defined(BKUI_PLATFORM_SWITCH)
        SwitchNeonGridDemo demo(*platform);
        if (!demo.Initialize())
        {
            std::fprintf(stderr, "Failed to initialize Switch neon grid demo.\n");
            platform->Shutdown();
            app.Shutdown();
            return 1;
        }
        bklog.info("Switch neon grid renderer initialized.");
#else
        OpenGLNeonGridDemo demo(*platform);
        if (!demo.Initialize())
        {
            std::fprintf(stderr, "Failed to initialize OpenGL neon grid demo.\n");
            platform->Shutdown();
            app.Shutdown();
            return 1;
        }
        bklog.info("OpenGL neon grid renderer initialized.");
#endif

        bklog.info("Neon grid demo entering main loop.");
        auto previousTime = std::chrono::steady_clock::now();
        while (platform->IsRunning() && app.IsRunning())
        {
            platform->PollEvents();
            const bk::InputState platformInput = platform->GetInput();
            if (platformInput.quitRequested)
            {
                app.RequestQuit();
            }

#if defined(BKUI_PLATFORM_SWITCH)
            if (platformInput.windowResized)
            {
                const bk::Vector2 windowSize = platform->GetWindowSize();
                app.SetWindowSize(windowSize);
                demo.Resize(windowSize);
            }
#endif

            app.SetInputState(platformInput);
            if (!app.IsRunning())
            {
                break;
            }

            const auto now = std::chrono::steady_clock::now();
            const float deltaSeconds = std::chrono::duration<float>(now - previousTime).count();
            previousTime = now;

            if (!app.RunFrame(deltaSeconds, true))
            {
                break;
            }

            demo.Step(app.GetInputState(), app.GetLogicalSize(), deltaSeconds, app.GetRenderQueue());
        }

        bklog.info("Neon grid demo main loop exited.");
        demo.Shutdown();
        bklog.info("Neon grid demo renderer shutdown complete.");
        platform->Shutdown();
        bklog.info("Platform shutdown complete.");
        app.Shutdown();
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "Fatal error: %s\n", ex.what());
        return 1;
    }

    return 0;
}
