#include "switch_platform_internal.hpp"

namespace bk::sw
{

bool InitInput(PadState& pad)
{
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    return true;
}

void PollInput(PadState& pad, bool& running, InputState& input)
{
    input = {};

    if (!appletMainLoop())
    {
        running = false;
        input.quitRequested = true;
        return;
    }

    padUpdate(&pad);
    const u64 down = padGetButtonsDown(&pad);
    if ((down & HidNpadButton_Plus) != 0)
    {
        running = false;
        input.quitRequested = true;
    }
    input.confirmPressed = (down & HidNpadButton_A) != 0;
    input.cancelPressed = (down & HidNpadButton_B) != 0;
}

void ShutdownInput()
{
}

}
