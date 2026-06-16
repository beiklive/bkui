#pragma once

#include <bkui/platform/Platform.hpp>
#include <bkui/core/FileSystem.hpp>

#include <SDL3/SDL.h>
#include <vector>

namespace bk::sdl
{

bool InitAudio();
void ShutdownAudio();

bool InitIme(SDL_Window* window, ImeManager*& imeManager);
void ShutdownIme();
TextInputStatus GetTextInputStatus();
void ResetTextInputStatus();
bool IsImeActive();
bool HandleImeKeyDown(const SDL_KeyboardEvent& event);
void HandleImeTextInput(const char* text);
void HandleImeTextEditing(const char* text, int cursor);
void SetImeInputArea(SDL_Window* window, int x, int y, int width, int height, int cursor);

bool InitVideo(const WindowDesc& desc, SDL_Window*& window);
void ShutdownVideo(SDL_Window*& window);
Vector2 GetWindowSize(SDL_Window* window);

bool CreateOpenGLContext(SDL_Window* window, SDL_GLContext& context, const OpenGLContextDesc& desc);
void MakeOpenGLContextCurrent(SDL_Window* window, SDL_GLContext context);
void SwapOpenGLBuffers(SDL_Window* window);
void DestroyOpenGLContext(SDL_GLContext& context);
OpenGLProcAddress GetOpenGLProcAddress(const char* name);

void PollInput(SDL_Window* window, bool& running, InputState& input);

std::vector<Buffer> LoadFonts();

}
