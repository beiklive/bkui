#include "sdl_platform_internal.hpp"

namespace bk::sdl
{

bool InitVideo(const WindowDesc& desc, SDL_Window*& window)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    window = SDL_CreateWindow(
        desc.title.c_str(),
        desc.width,
        desc.height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    if (window == nullptr)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    return true;
}

void ShutdownVideo(SDL_Window*& window)
{
    if (window != nullptr)
    {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    SDL_Quit();
}

Vector2 GetWindowSize(SDL_Window* window)
{
    int width = 0;
    int height = 0;
    if (window != nullptr)
    {
        SDL_GetWindowSizeInPixels(window, &width, &height);
    }
    return Vector2{static_cast<float>(width), static_cast<float>(height)};
}

bool CreateOpenGLContext(SDL_Window* window, SDL_GLContext& context, const OpenGLContextDesc& desc)
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

    context = SDL_GL_CreateContext(window);
    if (context == nullptr)
    {
        SDL_Log("SDL_GL_CreateContext failed: %s", SDL_GetError());
        return false;
    }

    MakeOpenGLContextCurrent(window, context);
    SDL_GL_SetSwapInterval(1);
    return true;
}

void MakeOpenGLContextCurrent(SDL_Window* window, SDL_GLContext context)
{
    if (window != nullptr && context != nullptr)
    {
        SDL_GL_MakeCurrent(window, context);
    }
}

void SwapOpenGLBuffers(SDL_Window* window)
{
    if (window != nullptr)
    {
        SDL_GL_SwapWindow(window);
    }
}

void DestroyOpenGLContext(SDL_GLContext& context)
{
    if (context != nullptr)
    {
        SDL_GL_DestroyContext(context);
        context = nullptr;
    }
}

OpenGLProcAddress GetOpenGLProcAddress(const char* name)
{
    return SDL_GL_GetProcAddress(name);
}

}
