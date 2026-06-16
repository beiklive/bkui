#pragma once

#include <functional>
#include <string>

namespace bk
{

/// 软键盘禁用字符的位掩码定义。
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

    /// 打开文本输入界面，并在用户确认后回调输入结果。
    virtual bool OpenForText(
        std::function<void(std::string)> callback,
        std::string headerText = "",
        std::string subText = "",
        int maxStringLength = 32,
        std::string initialText = "",
        int kbdDisableBitmask = KeyboardKeyDisableBitmask::KEYBOARD_DISABLE_NONE) = 0;

    /// 打开数字输入界面，并在用户确认后回调输入结果。
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
