#pragma once

#include <bkui/platform/Platform.hpp>
#include <bkui/core/FileSystem.hpp>

#include <switch.h>
#include <vector>

namespace bk::sw
{

bool InitAudio();
void ShutdownAudio();

bool InitIme(ImeManager*& imeManager);
void ShutdownIme();
TextInputStatus GetTextInputStatus();
void ResetTextInputStatus();

bool InitVideo();
void ShutdownVideo();
Vector2 GetWindowSize();

bool InitInput(PadState& pad);
void PollInput(PadState& pad, bool& running, InputState& input);
void ShutdownInput();

std::vector<Buffer> LoadFonts();

}
