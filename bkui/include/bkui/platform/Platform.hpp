#pragma once

#include <bkui/core/Types.hpp>

#include <memory>
#include <string>

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

    virtual bool CreateOpenGLContext(const OpenGLContextDesc& desc) = 0;
    virtual void MakeOpenGLContextCurrent() = 0;
    virtual void SwapOpenGLBuffers() = 0;
    virtual void DestroyOpenGLContext() = 0;
    virtual OpenGLProcAddress GetOpenGLProcAddress(const char* name) = 0;
};

std::unique_ptr<Platform> CreateDefaultPlatform(const WindowDesc& desc);

}
