#include "sdl_platform_internal.hpp"

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_touch.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace bk::sdl
{
namespace
{
struct TrackedTouchState
{
    std::int64_t id = -1;
    Vector2 position{};
    bool down = false;
};

using TouchStateArray = std::array<TrackedTouchState, InputState::MaxTouchPoints>;

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

    const std::size_t len = std::min(dstSize - 1, std::strlen(src));
    std::memcpy(dst, src, len);
    dst[len] = '\0';
}

void PushKeyDown(InputState& input, const SDL_KeyboardEvent& event)
{
    auto sameKey = [&](const InputState::KeyEvent& key) {
        return key.code == static_cast<std::int32_t>(event.scancode);
    };

    auto it = std::find_if(input.keysDown.begin(), input.keysDown.begin() + input.keysDownCount, sameKey);
    if (it == input.keysDown.begin() + input.keysDownCount && input.keysDownCount < input.keysDown.size())
    {
        it = input.keysDown.begin() + static_cast<std::ptrdiff_t>(input.keysDownCount++);
    }

    if (it == input.keysDown.begin() + input.keysDownCount)
    {
        return;
    }

    it->code = static_cast<std::int32_t>(event.scancode);
    CopyCString(it->name, sizeof(it->name), SDL_GetScancodeName(event.scancode));
    it->down = true;
    it->repeat = event.repeat;
    it->shift = (event.mod & SDL_KMOD_SHIFT) != 0;
    it->ctrl = (event.mod & SDL_KMOD_CTRL) != 0;
    it->alt = (event.mod & SDL_KMOD_ALT) != 0;
    it->meta = (event.mod & SDL_KMOD_GUI) != 0;
}

void RemoveKeyDown(InputState& input, SDL_Scancode scancode)
{
    for (std::size_t i = 0; i < input.keysDownCount; ++i)
    {
        if (input.keysDown[i].code == static_cast<std::int32_t>(scancode))
        {
            for (std::size_t j = i + 1; j < input.keysDownCount; ++j)
            {
                input.keysDown[j - 1] = input.keysDown[j];
            }
            input.keysDown[input.keysDownCount - 1] = {};
            --input.keysDownCount;
            break;
        }
    }
}

int FindTouchById(const TouchStateArray& touches, std::size_t count, std::int64_t id)
{
    for (std::size_t index = 0; index < count && index < touches.size(); ++index)
    {
        if (touches[index].down && touches[index].id == id)
        {
            return static_cast<int>(index);
        }
    }

    return -1;
}

void FillTouchPoints(SDL_Window* window, InputState& input)
{
    static TouchStateArray previousTouches{};
    static std::size_t previousTouchCount = 0;

    (void)window;
    TouchStateArray currentTouches{};
    std::size_t currentTouchCount = 0;
    bool touchChanged = false;

    int deviceCount = 0;
    SDL_TouchID* devices = SDL_GetTouchDevices(&deviceCount);
    if (devices == nullptr || deviceCount <= 0)
    {
        for (std::size_t index = 0; index < previousTouchCount && input.touchCount < input.touchPoints.size(); ++index)
        {
            const auto& previous = previousTouches[index];
            if (!previous.down)
            {
                continue;
            }

            auto& touch = input.touchPoints[input.touchCount++];
            touch.id = previous.id;
            touch.position = previous.position;
            touch.released = true;
        }
        input.touchMoved = input.touchMoved || input.touchCount > 0;
        previousTouches = {};
        previousTouchCount = 0;
        return;
    }

    const Vector2 windowSize = input.windowSize;
    for (int deviceIndex = 0; deviceIndex < deviceCount && currentTouchCount < currentTouches.size(); ++deviceIndex)
    {
        int fingerCount = 0;
        SDL_Finger** fingers = SDL_GetTouchFingers(devices[deviceIndex], &fingerCount);
        if (fingers == nullptr)
        {
            continue;
        }

        for (int fingerIndex = 0; fingerIndex < fingerCount && currentTouchCount < currentTouches.size(); ++fingerIndex)
        {
            SDL_Finger* finger = fingers[fingerIndex];
            if (finger == nullptr)
            {
                continue;
            }

            auto& touch = currentTouches[currentTouchCount++];
            touch.id = static_cast<std::int64_t>(finger->id);
            touch.position = Vector2{
                finger->x * windowSize.x,
                finger->y * windowSize.y,
            };
            touch.down = true;
        }

        SDL_free(fingers);
    }

    SDL_free(devices);

    for (std::size_t index = 0; index < currentTouchCount && input.touchCount < input.touchPoints.size(); ++index)
    {
        const auto& current = currentTouches[index];
        const int previousIndex = FindTouchById(previousTouches, previousTouchCount, current.id);
        auto& touch = input.touchPoints[input.touchCount++];
        touch.id = current.id;
        touch.position = current.position;
        touch.down = true;
        touch.pressed = previousIndex < 0;

        if (touch.pressed)
        {
            touchChanged = true;
        }
        else
        {
            const Vector2 previousPosition = previousTouches[static_cast<std::size_t>(previousIndex)].position;
            if (previousPosition.x != current.position.x || previousPosition.y != current.position.y)
            {
                touchChanged = true;
            }
        }
    }

    for (std::size_t previousIndex = 0; previousIndex < previousTouchCount && input.touchCount < input.touchPoints.size(); ++previousIndex)
    {
        const auto& previous = previousTouches[previousIndex];
        if (!previous.down || FindTouchById(currentTouches, currentTouchCount, previous.id) >= 0)
        {
            continue;
        }

        auto& touch = input.touchPoints[input.touchCount++];
        touch.id = previous.id;
        touch.position = previous.position;
        touch.released = true;
        touchChanged = true;
    }

    input.touchMoved = input.touchMoved || touchChanged;
    previousTouches = currentTouches;
    previousTouchCount = currentTouchCount;
}
}

void PollInput(SDL_Window* window, bool& running, InputState& input)
{
    input = {};
    input.windowSize = GetWindowSize(window);
    ResetTextInputStatus();

    float mouseX = 0.0F;
    float mouseY = 0.0F;
    const SDL_MouseButtonFlags mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);
    input.mousePosition = Vector2{mouseX, mouseY};
    input.mouseLeftDown = (mouseButtons & SDL_BUTTON_LMASK) != 0;
    input.mouseMiddleDown = (mouseButtons & SDL_BUTTON_MMASK) != 0;
    input.mouseRightDown = (mouseButtons & SDL_BUTTON_RMASK) != 0;

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            running = false;
            input.quitRequested = true;
            break;
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            input.windowResized = true;
            input.windowSize = Vector2{
                static_cast<float>(event.window.data1),
                static_cast<float>(event.window.data2),
            };
            break;
        case SDL_EVENT_KEY_DOWN:
        {
            input.keyPressed = true;
            input.lastKeyEvent.code = static_cast<std::int32_t>(event.key.scancode);
            CopyCString(input.lastKeyEvent.name, sizeof(input.lastKeyEvent.name), SDL_GetScancodeName(event.key.scancode));
            input.lastKeyEvent.down = true;
            input.lastKeyEvent.repeat = event.key.repeat;
            input.lastKeyEvent.shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
            input.lastKeyEvent.ctrl = (event.key.mod & SDL_KMOD_CTRL) != 0;
            input.lastKeyEvent.alt = (event.key.mod & SDL_KMOD_ALT) != 0;
            input.lastKeyEvent.meta = (event.key.mod & SDL_KMOD_GUI) != 0;
            PushKeyDown(input, event.key);

            if (event.key.scancode == SDL_SCANCODE_ESCAPE)
            {
                if (IsImeActive())
                {
                    HandleImeKeyDown(event.key);
                }
                else
                {
                    running = false;
                    input.cancelPressed = true;
                    input.quitRequested = true;
                }
            }
            else if (event.key.scancode == SDL_SCANCODE_RETURN ||
                     event.key.scancode == SDL_SCANCODE_KP_ENTER ||
                     event.key.scancode == SDL_SCANCODE_SPACE)
            {
                input.confirmPressed = true;
                if (IsImeActive())
                {
                    HandleImeKeyDown(event.key);
                }
            }
            else if (event.key.scancode == SDL_SCANCODE_BACKSPACE)
            {
                if (IsImeActive())
                {
                    HandleImeKeyDown(event.key);
                }
            }
            break;
        }
        case SDL_EVENT_KEY_UP:
            input.keyReleased = true;
            input.lastKeyEvent.code = static_cast<std::int32_t>(event.key.scancode);
            CopyCString(input.lastKeyEvent.name, sizeof(input.lastKeyEvent.name), SDL_GetScancodeName(event.key.scancode));
            input.lastKeyEvent.down = false;
            input.lastKeyEvent.repeat = false;
            input.lastKeyEvent.shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
            input.lastKeyEvent.ctrl = (event.key.mod & SDL_KMOD_CTRL) != 0;
            input.lastKeyEvent.alt = (event.key.mod & SDL_KMOD_ALT) != 0;
            input.lastKeyEvent.meta = (event.key.mod & SDL_KMOD_GUI) != 0;
            RemoveKeyDown(input, event.key.scancode);
            break;
        case SDL_EVENT_TEXT_INPUT:
            input.textCommitted = true;
            CopyCString(input.textInput, sizeof(input.textInput), event.text.text);
            HandleImeTextInput(event.text.text);
            break;
        case SDL_EVENT_TEXT_EDITING:
            input.textEdited = true;
            input.compositionCursor = event.edit.start;
            CopyCString(input.composition, sizeof(input.composition), event.edit.text);
            HandleImeTextEditing(event.edit.text, event.edit.start);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            input.mouseMoved = true;
            input.mousePosition = Vector2{event.motion.x, event.motion.y};
            input.mouseDelta = Vector2{event.motion.xrel, event.motion.yrel};
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                input.mouseLeftDown = true;
                input.mouseLeftPressed = true;
            }
            else if (event.button.button == SDL_BUTTON_MIDDLE)
            {
                input.mouseMiddleDown = true;
                input.mouseMiddlePressed = true;
            }
            else if (event.button.button == SDL_BUTTON_RIGHT)
            {
                input.mouseRightDown = true;
                input.mouseRightPressed = true;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                input.mouseLeftDown = false;
                input.mouseLeftReleased = true;
            }
            else if (event.button.button == SDL_BUTTON_MIDDLE)
            {
                input.mouseMiddleDown = false;
                input.mouseMiddleReleased = true;
            }
            else if (event.button.button == SDL_BUTTON_RIGHT)
            {
                input.mouseRightDown = false;
                input.mouseRightReleased = true;
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            input.mouseWheel = Vector2{event.wheel.x, event.wheel.y};
            break;
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED:
            input.touchMoved = true;
            break;
        default:
            break;
        }
    }

    FillTouchPoints(window, input);
}

}
