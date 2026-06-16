#include "sdl_platform_internal.hpp"

#include <memory>
#include <utility>

namespace bk
{
namespace
{
class SDLPlatform final : public Platform
{
public:
    explicit SDLPlatform(WindowDesc desc)
        : desc_(std::move(desc))
    {
    }

    bool Init() override
    {
        if (!sdl::InitVideo(desc_, window_))
        {
            return false;
        }
        if (!sdl::InitAudio() || !sdl::InitIme(window_, imeManager_))
        {
            Shutdown();
            return false;
        }

        running_ = true;
        return true;
    }

    void PollEvents() override
    {
        sdl::PollInput(window_, running_, input_);
    }

    void Shutdown() override
    {
        sdl::DestroyOpenGLContext(glContext_);
        sdl::ShutdownIme();
        sdl::ShutdownAudio();
        sdl::ShutdownVideo(window_);
        running_ = false;
    }

    void* GetWindow() override
    {
        return window_;
    }

    InputState GetInput() const override
    {
        return input_;
    }

    Vector2 GetWindowSize() const override
    {
        return sdl::GetWindowSize(window_);
    }

    bool IsRunning() const override
    {
        return running_;
    }

    ImeManager* GetImeManager() override
    {
        return imeManager_;
    }

    bool StartTextInput() override
    {
        if (window_ == nullptr)
        {
            return false;
        }
        return SDL_StartTextInput(window_);
    }

    void StopTextInput() override
    {
        if (window_ != nullptr)
        {
            SDL_StopTextInput(window_);
        }
    }

    bool IsTextInputActive() const override
    {
        return window_ != nullptr && SDL_TextInputActive(window_);
    }

    void SetTextInputArea(int x, int y, int width, int height, int cursor) override
    {
        sdl::SetImeInputArea(window_, x, y, width, height, cursor);
    }

    TextInputStatus GetTextInputStatus() const override
    {
        return sdl::GetTextInputStatus();
    }

    bool CreateOpenGLContext(const OpenGLContextDesc& desc) override
    {
        return sdl::CreateOpenGLContext(window_, glContext_, desc);
    }

    void MakeOpenGLContextCurrent() override
    {
        sdl::MakeOpenGLContextCurrent(window_, glContext_);
    }

    void SwapOpenGLBuffers() override
    {
        sdl::SwapOpenGLBuffers(window_);
    }

    void DestroyOpenGLContext() override
    {
        sdl::DestroyOpenGLContext(glContext_);
    }

    OpenGLProcAddress GetOpenGLProcAddress(const char* name) override
    {
        return sdl::GetOpenGLProcAddress(name);
    }

private:
    WindowDesc desc_;
    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    ImeManager* imeManager_ = nullptr;
    InputState input_;
    bool running_ = false;
};
}

std::unique_ptr<Platform> CreateDefaultPlatform(const WindowDesc& desc)
{
    return std::make_unique<SDLPlatform>(desc);
}

}
