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

/// 窗口创建参数。
struct WindowDesc
{
    std::string title = "BeikUI";
    int width = 1280;
    int height = 720;
};

/// 当前帧采集到的输入状态快照。
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

/// 当前文本输入会话的状态。
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

/// OpenGL 上下文创建参数。
struct OpenGLContextDesc
{
    int major = 3;
    int minor = 3;
    bool debug = false;
};

using OpenGLProcAddress = void (*)();

/// 平台抽象接口，负责窗口、输入、IME 和图形上下文能力。
class Platform
{
public:
    virtual ~Platform() = default;

    /// 初始化平台层资源。
    virtual bool Init() = 0;
    /// 拉取并处理平台事件。
    virtual void PollEvents() = 0;
    /// 释放平台层资源。
    virtual void Shutdown() = 0;
    /// 获取底层原生窗口句柄。
    virtual void* GetWindow() = 0;
    /// 获取当前输入状态。
    virtual InputState GetInput() const = 0;
    /// 获取当前窗口大小。
    virtual Vector2 GetWindowSize() const = 0;
    /// 返回主循环是否仍应继续运行。
    virtual bool IsRunning() const = 0;
    /// 获取平台 IME 管理器，没有则返回空指针。
    virtual ImeManager* GetImeManager() = 0;
    /// 开始接收文本输入。
    virtual bool StartTextInput() = 0;
    /// 停止文本输入。
    virtual void StopTextInput() = 0;
    /// 查询文本输入是否处于激活状态。
    virtual bool IsTextInputActive() const = 0;
    /// 设置文本输入候选框关联区域与光标位置。
    virtual void SetTextInputArea(int x, int y, int width, int height, int cursor) = 0;
    /// 获取文本输入的当前状态。
    virtual TextInputStatus GetTextInputStatus() const = 0;

    /// 创建 OpenGL 上下文。
    virtual bool CreateOpenGLContext(const OpenGLContextDesc& desc) = 0;
    /// 将 OpenGL 上下文切换为当前线程可用。
    virtual void MakeOpenGLContextCurrent() = 0;
    /// 交换 OpenGL 前后缓冲。
    virtual void SwapOpenGLBuffers() = 0;
    /// 销毁 OpenGL 上下文。
    virtual void DestroyOpenGLContext() = 0;
    /// 查询 OpenGL 函数地址。
    virtual OpenGLProcAddress GetOpenGLProcAddress(const char* name) = 0;
};

/// 根据当前平台创建默认的平台实现。
std::unique_ptr<Platform> CreateDefaultPlatform(const WindowDesc& desc);

}
