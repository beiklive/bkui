#pragma once

#include <functional>
#include <string>

namespace bk
{

enum KeyboardKeyDisableBitmask
{
    KEYBOARD_DISABLE_NONE = 0,
    KEYBOARD_DISABLE_SPACE = 1,
    KEYBOARD_DISABLE_AT = 1 << 1,
    KEYBOARD_DISABLE_PERCENT = 1 << 2,
    KEYBOARD_DISABLE_FORWSLASH = 1 << 3,
    KEYBOARD_DISABLE_BACKSLASH = 1 << 4,
    KEYBOARD_DISABLE_NUMBERS = 1 << 5,
    KEYBOARD_DISABLE_DOWNLOADCODE = 1 << 6,
    KEYBOARD_DISABLE_USERNAME = 1 << 7,
};

class ImeManager
{
public:
    virtual ~ImeManager() = default;

    virtual bool OpenForText(
        std::function<void(std::string)> callback,
        std::string headerText = "",
        std::string subText = "",
        int maxStringLength = 32,
        std::string initialText = "",
        int kbdDisableBitmask = KeyboardKeyDisableBitmask::KEYBOARD_DISABLE_NONE) = 0;

    virtual bool OpenForNumber(
        std::function<void(long)> callback,
        std::string headerText = "",
        std::string subText = "",
        int maxStringLength = 18,
        std::string initialText = "",
        std::string leftButton = "",
        std::string rightButton = "",
        int kbdDisableBitmask = KeyboardKeyDisableBitmask::KEYBOARD_DISABLE_NONE) = 0;
};

}
