#include <bkui/platform/Platform.hpp>

#include <SDL3/SDL.h>

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
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        {
            SDL_Log("SDL_Init failed: %s", SDL_GetError());
            return false;
        }

        window_ = SDL_CreateWindow(
            desc_.title.c_str(),
            desc_.width,
            desc_.height,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
        );

        if (window_ == nullptr)
        {
            SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
            return false;
        }

        running_ = true;
        return true;
    }

    void PollEvents() override
    {
        input_ = {};

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                running_ = false;
                input_.quitRequested = true;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN)
            {
                if (event.key.key == SDLK_ESCAPE)
                {
                    running_ = false;
                    input_.cancelPressed = true;
                    input_.quitRequested = true;
                }
                else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE)
                {
                    input_.confirmPressed = true;
                }
            }
        }
    }

    void Shutdown() override
    {
        DestroyOpenGLContext();

        if (window_ != nullptr)
        {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }

        SDL_Quit();
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
        int width = 0;
        int height = 0;
        if (window_ != nullptr)
        {
            SDL_GetWindowSizeInPixels(window_, &width, &height);
        }
        return Vector2{static_cast<float>(width), static_cast<float>(height)};
    }

    bool IsRunning() const override
    {
        return running_;
    }

    bool CreateOpenGLContext(const OpenGLContextDesc& desc) override
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, desc.major);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, desc.minor);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        if (desc.debug)
        {
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
        }

        glContext_ = SDL_GL_CreateContext(window_);
        if (glContext_ == nullptr)
        {
            SDL_Log("SDL_GL_CreateContext failed: %s", SDL_GetError());
            return false;
        }

        MakeOpenGLContextCurrent();
        SDL_GL_SetSwapInterval(1);
        return true;
    }

    void MakeOpenGLContextCurrent() override
    {
        if (window_ != nullptr && glContext_ != nullptr)
        {
            SDL_GL_MakeCurrent(window_, glContext_);
        }
    }

    void SwapOpenGLBuffers() override
    {
        if (window_ != nullptr)
        {
            SDL_GL_SwapWindow(window_);
        }
    }

    void DestroyOpenGLContext() override
    {
        if (glContext_ != nullptr)
        {
            SDL_GL_DestroyContext(glContext_);
            glContext_ = nullptr;
        }
    }

    OpenGLProcAddress GetOpenGLProcAddress(const char* name) override
    {
        return SDL_GL_GetProcAddress(name);
    }

private:
    WindowDesc desc_;
    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    InputState input_;
    bool running_ = false;
};
}

std::unique_ptr<Platform> CreateDefaultPlatform(const WindowDesc& desc)
{
    return std::make_unique<SDLPlatform>(desc);
}

}
