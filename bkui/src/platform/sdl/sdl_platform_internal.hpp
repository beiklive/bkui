#pragma once

#include <bkui/platform/Platform.hpp>
#include <bkui/resource/FileSystem.hpp>

#include <SDL3/SDL.h>
#include <vector>

namespace bk::sdl
{

bool InitAudio();
void ShutdownAudio();

bool InitIme();
void ShutdownIme();

bool InitVideo(const WindowDesc& desc, SDL_Window*& window);
void ShutdownVideo(SDL_Window*& window);
Vector2 GetWindowSize(SDL_Window* window);

bool CreateOpenGLContext(SDL_Window* window, SDL_GLContext& context, const OpenGLContextDesc& desc);
void MakeOpenGLContextCurrent(SDL_Window* window, SDL_GLContext context);
void SwapOpenGLBuffers(SDL_Window* window);
void DestroyOpenGLContext(SDL_GLContext& context);
OpenGLProcAddress GetOpenGLProcAddress(const char* name);

void PollInput(bool& running, InputState& input);

std::vector<Buffer> LoadFonts();

}
