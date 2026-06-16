#include "switch_platform_internal.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace bk::sw
{
namespace
{
void CopyCString(char* dst, std::size_t dstSize, const char* src)
{
    if (dst == nullptr || dstSize == 0)
    {
        return;
    }

    if (src == nullptr)
    {
        dst[0] = '\0';
        return;
    }

    const std::size_t length = std::min(dstSize - 1, std::strlen(src));
    std::memcpy(dst, src, length);
    dst[length] = '\0';
}

const char* SwitchButtonName(u64 button)
{
    switch (button)
    {
    case HidNpadButton_A: return "A";
    case HidNpadButton_B: return "B";
    case HidNpadButton_X: return "X";
    case HidNpadButton_Y: return "Y";
    case HidNpadButton_L: return "L";
    case HidNpadButton_R: return "R";
    case HidNpadButton_ZL: return "ZL";
    case HidNpadButton_ZR: return "ZR";
    case HidNpadButton_Plus: return "Plus";
    case HidNpadButton_Minus: return "Minus";
    case HidNpadButton_Up: return "Up";
    case HidNpadButton_Down: return "Down";
    case HidNpadButton_Left: return "Left";
    case HidNpadButton_Right: return "Right";
    case HidNpadButton_StickL: return "LStick";
    case HidNpadButton_StickR: return "RStick";
    default: return "Unknown";
    }
}

void PushDownKey(InputState& input, std::int32_t code, const char* name)
{
    if (input.keysDownCount >= input.keysDown.size())
    {
        return;
    }

    auto& key = input.keysDown[input.keysDownCount++];
    key.code = code;
    key.down = true;
    CopyCString(key.name, sizeof(key.name), name);
}

void FillKeyboardState(InputState& input)
{
    HidKeyboardState keyboardState{};
    if (!hidGetKeyboardStates(&keyboardState, 1))
    {
        return;
    }

    input.lastKeyEvent.shift = (keyboardState.modifiers & HidKeyboardModifier_Shift) != 0;
    input.lastKeyEvent.ctrl = (keyboardState.modifiers & HidKeyboardModifier_Control) != 0;
    input.lastKeyEvent.alt = (keyboardState.modifiers & HidKeyboardModifier_LeftAlt) != 0;
    input.lastKeyEvent.meta = (keyboardState.modifiers & HidKeyboardModifier_Gui) != 0;

    for (int code = 0; code < 256 && input.keysDownCount < input.keysDown.size(); ++code)
    {
        const bool down = (keyboardState.keys[code / 64] & (1ULL << (code % 64))) != 0;
        if (!down)
        {
            continue;
        }

        PushDownKey(input, code, "Keyboard");
    }
}

void FillTouchState(InputState& input)
{
    HidTouchScreenState touchState{};
    if (!hidGetTouchScreenStates(&touchState, 1))
    {
        return;
    }

    input.touchCount = std::min<std::size_t>(touchState.count, input.touchPoints.size());
    for (std::size_t i = 0; i < input.touchCount; ++i)
    {
        const auto& source = touchState.touches[i];
        auto& touch = input.touchPoints[i];
        touch.id = source.finger_id;
        touch.position = Vector2{
            static_cast<float>(source.x),
            static_cast<float>(source.y),
        };
        touch.down = true;
    }
}

void FillMouseState(InputState& input)
{
    HidMouseState mouseState{};
    if (!hidGetMouseStates(&mouseState, 1))
    {
        return;
    }

    if ((mouseState.attributes & HidMouseAttribute_IsConnected) == 0)
    {
        return;
    }

    input.mousePosition = Vector2{
        static_cast<float>(mouseState.x),
        static_cast<float>(mouseState.y),
    };
    input.mouseDelta = Vector2{
        static_cast<float>(mouseState.delta_x),
        static_cast<float>(mouseState.delta_y),
    };
    input.mouseWheel = Vector2{
        static_cast<float>(mouseState.wheel_delta_x),
        static_cast<float>(mouseState.wheel_delta_y),
    };
    input.mouseLeftDown = (mouseState.buttons & HidMouseButton_Left) != 0;
    input.mouseMiddleDown = (mouseState.buttons & HidMouseButton_Middle) != 0;
    input.mouseRightDown = (mouseState.buttons & HidMouseButton_Right) != 0;
    input.mouseMoved = mouseState.delta_x != 0 || mouseState.delta_y != 0;
}
}

bool InitInput(PadState& pad)
{
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    hidInitializeTouchScreen();
    hidInitializeMouse();
    hidInitializeKeyboard();
    return true;
}

void PollInput(PadState& pad, bool& running, InputState& input)
{
    input = {};
    input.windowSize = GetWindowSize();
    ResetTextInputStatus();

    if (!appletMainLoop())
    {
        running = false;
        input.quitRequested = true;
        return;
    }

    padUpdate(&pad);
    const u64 down = padGetButtonsDown(&pad);
    const u64 held = padGetButtons(&pad);
    const u64 up = padGetButtonsUp(&pad);

    if ((down & HidNpadButton_Plus) != 0)
    {
        running = false;
        input.quitRequested = true;
    }

    input.confirmPressed = (down & HidNpadButton_A) != 0;
    input.cancelPressed = (down & HidNpadButton_B) != 0;
    input.keyPressed = down != 0;
    input.keyReleased = up != 0;

    if (down != 0 || up != 0)
    {
        static constexpr std::array<u64, 16> buttons = {{
            HidNpadButton_A,
            HidNpadButton_B,
            HidNpadButton_X,
            HidNpadButton_Y,
            HidNpadButton_L,
            HidNpadButton_R,
            HidNpadButton_ZL,
            HidNpadButton_ZR,
            HidNpadButton_Plus,
            HidNpadButton_Minus,
            HidNpadButton_Up,
            HidNpadButton_Down,
            HidNpadButton_Left,
            HidNpadButton_Right,
            HidNpadButton_StickL,
            HidNpadButton_StickR,
        }};

        for (u64 button : buttons)
        {
            if ((down & button) != 0 || (up & button) != 0)
            {
                input.lastKeyEvent.code = static_cast<std::int32_t>(button);
                input.lastKeyEvent.down = (down & button) != 0;
                CopyCString(input.lastKeyEvent.name, sizeof(input.lastKeyEvent.name), SwitchButtonName(button));
                break;
            }
        }
    }

    static constexpr std::array<u64, 8> heldButtons = {{
        HidNpadButton_A,
        HidNpadButton_B,
        HidNpadButton_X,
        HidNpadButton_Y,
        HidNpadButton_Up,
        HidNpadButton_Down,
        HidNpadButton_Left,
        HidNpadButton_Right,
    }};
    for (u64 button : heldButtons)
    {
        if ((held & button) != 0)
        {
            PushDownKey(input, static_cast<std::int32_t>(button), SwitchButtonName(button));
        }
    }

    FillKeyboardState(input);
    FillMouseState(input);
    FillTouchState(input);
    input.touchMoved = input.touchCount > 0;
}

void ShutdownInput()
{
}

}
