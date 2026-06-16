#pragma once

#include <bkui/core/Types.hpp>
#include <bkui/platform/Ime.hpp>

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bk
{

struct WindowDesc
{
    std::string title = "BeikUI";
    int width = 1280;
    int height = 720;
};

struct InputState
{
    bool quitRequested = false;
    bool confirmPressed = false;
    bool cancelPressed = false;
    bool mouseMoved = false;
    bool touchMoved = false;
    bool keyPressed = false;
    bool keyReleased = false;
    bool textEdited = false;
    bool textCommitted = false;
    bool windowResized = false;

    Vector2 windowSize{};
    Vector2 mousePosition{};
    Vector2 mouseDelta{};
    Vector2 mouseWheel{};

    bool mouseLeftDown = false;
    bool mouseMiddleDown = false;
    bool mouseRightDown = false;
    bool mouseLeftPressed = false;
    bool mouseMiddlePressed = false;
    bool mouseRightPressed = false;
    bool mouseLeftReleased = false;
    bool mouseMiddleReleased = false;
    bool mouseRightReleased = false;

    static constexpr std::size_t MaxTouchPoints = 8;

    struct TouchPoint
    {
        std::int64_t id = -1;
        Vector2 position{};
        bool down = false;
        bool pressed = false;
        bool released = false;
    };

    std::array<TouchPoint, MaxTouchPoints> touchPoints{};
    std::size_t touchCount = 0;

    static constexpr std::size_t MaxKeysDown = 16;
    static constexpr std::size_t MaxTextBytes = 128;
    static constexpr std::size_t MaxCompositionBytes = 128;
    static constexpr std::size_t MaxKeyNameLength = 32;

    struct KeyEvent
    {
        std::int32_t code = 0;
        char name[MaxKeyNameLength]{};
        bool down = false;
        bool repeat = false;
        bool shift = false;
        bool ctrl = false;
        bool alt = false;
        bool meta = false;
    };

    KeyEvent lastKeyEvent{};
    std::array<KeyEvent, MaxKeysDown> keysDown{};
    std::size_t keysDownCount = 0;
    char textInput[MaxTextBytes]{};
    char composition[MaxCompositionBytes]{};
    int compositionCursor = -1;
};

struct TextInputStatus
{
    bool active = false;
    bool editing = false;
    bool submitted = false;
    bool canceled = false;
    char text[InputState::MaxTextBytes]{};
    char composition[InputState::MaxCompositionBytes]{};
    int compositionCursor = -1;
};

struct OpenGLContextDesc
{
    int major = 3;
    int minor = 3;
    bool debug = false;
};

using OpenGLProcAddress = void (*)();

class Platform
{
public:
    virtual ~Platform() = default;

    virtual bool Init() = 0;
    virtual void PollEvents() = 0;
    virtual void Shutdown() = 0;
    virtual void* GetWindow() = 0;
    virtual InputState GetInput() const = 0;
    virtual Vector2 GetWindowSize() const = 0;
    virtual bool IsRunning() const = 0;
    virtual ImeManager* GetImeManager() = 0;
    virtual bool StartTextInput() = 0;
    virtual void StopTextInput() = 0;
    virtual bool IsTextInputActive() const = 0;
    virtual void SetTextInputArea(int x, int y, int width, int height, int cursor) = 0;
    virtual TextInputStatus GetTextInputStatus() const = 0;

    virtual bool CreateOpenGLContext(const OpenGLContextDesc& desc) = 0;
    virtual void MakeOpenGLContextCurrent() = 0;
    virtual void SwapOpenGLBuffers() = 0;
    virtual void DestroyOpenGLContext() = 0;
    virtual OpenGLProcAddress GetOpenGLProcAddress(const char* name) = 0;
};

std::unique_ptr<Platform> CreateDefaultPlatform(const WindowDesc& desc);

}
