#include "switch_platform_internal.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace bk::sw
{
namespace
{
class SwitchImeManager final : public ImeManager
{
public:
    bool OpenForText(
        std::function<void(std::string)> callback,
        std::string headerText,
        std::string subText,
        int maxStringLength,
        std::string initialText,
        int kbdDisableBitmask) override
    {
        SwkbdConfig config = CreateBaseConfig(
            std::move(headerText),
            std::move(subText),
            maxStringLength,
            std::move(initialText));

        swkbdConfigSetType(&config, SwkbdType_All);
        swkbdConfigSetKeySetDisableBitmask(&config, TranslateDisableBitmask(kbdDisableBitmask));

        char buffer[0x100]{};
        const bool success = R_SUCCEEDED(swkbdShow(&config, buffer, sizeof(buffer)));
        swkbdClose(&config);

        status_ = {};
        status_.submitted = success;
        status_.canceled = !success;
        CopyStatusText(buffer);

        if (success && callback)
        {
            callback(buffer);
        }

        return success;
    }

    bool OpenForNumber(
        std::function<void(long)> callback,
        std::string headerText,
        std::string subText,
        int maxStringLength,
        std::string initialText,
        std::string leftButton,
        std::string rightButton,
        int kbdDisableBitmask) override
    {
        SwkbdConfig config = CreateBaseConfig(
            std::move(headerText),
            std::move(subText),
            maxStringLength,
            std::move(initialText));

        swkbdConfigSetType(&config, SwkbdType_NumPad);
        swkbdConfigSetLeftOptionalSymbolKey(&config, leftButton.c_str());
        swkbdConfigSetRightOptionalSymbolKey(&config, rightButton.c_str());
        swkbdConfigSetKeySetDisableBitmask(&config, TranslateDisableBitmask(kbdDisableBitmask));

        char buffer[0x100]{};
        const bool success = R_SUCCEEDED(swkbdShow(&config, buffer, sizeof(buffer))) && buffer[0] != '\0';
        swkbdClose(&config);

        status_ = {};
        status_.submitted = success;
        status_.canceled = !success;
        CopyStatusText(buffer);

        if (success && callback)
        {
            callback(std::strtol(buffer, nullptr, 10));
        }

        return success;
    }

    TextInputStatus Status() const
    {
        return status_;
    }

    void ResetTransientFlags()
    {
        status_.submitted = false;
        status_.canceled = false;
    }

private:
    static SwkbdConfig CreateBaseConfig(
        std::string headerText,
        std::string subText,
        int maxStringLength,
        std::string initialText)
    {
        SwkbdConfig config;
        swkbdCreate(&config, 0);
        swkbdConfigMakePresetDefault(&config);
        swkbdConfigSetHeaderText(&config, headerText.c_str());
        swkbdConfigSetSubText(&config, subText.c_str());
        swkbdConfigSetStringLenMax(&config, std::max(1, maxStringLength));
        swkbdConfigSetInitialText(&config, initialText.c_str());
        swkbdConfigSetBlurBackground(&config, true);
        return config;
    }

    static int TranslateDisableBitmask(int bitmask)
    {
        int result = 0;
        if ((bitmask & KEYBOARD_DISABLE_SPACE) != 0)
        {
            result |= SwkbdKeyDisableBitmask_Space;
        }
        if ((bitmask & KEYBOARD_DISABLE_AT) != 0)
        {
            result |= SwkbdKeyDisableBitmask_At;
        }
        if ((bitmask & KEYBOARD_DISABLE_PERCENT) != 0)
        {
            result |= SwkbdKeyDisableBitmask_Percent;
        }
        if ((bitmask & KEYBOARD_DISABLE_FORWSLASH) != 0)
        {
            result |= SwkbdKeyDisableBitmask_ForwardSlash;
        }
        if ((bitmask & KEYBOARD_DISABLE_BACKSLASH) != 0)
        {
            result |= SwkbdKeyDisableBitmask_Backslash;
        }
        if ((bitmask & KEYBOARD_DISABLE_NUMBERS) != 0)
        {
            result |= SwkbdKeyDisableBitmask_Numbers;
        }
        if ((bitmask & KEYBOARD_DISABLE_DOWNLOADCODE) != 0)
        {
            result |= SwkbdKeyDisableBitmask_DownloadCode;
        }
        if ((bitmask & KEYBOARD_DISABLE_USERNAME) != 0)
        {
            result |= SwkbdKeyDisableBitmask_UserName;
        }
        return result;
    }

    void CopyStatusText(const char* text)
    {
        if (text == nullptr)
        {
            status_.text[0] = '\0';
            return;
        }

        const std::size_t length = std::min(sizeof(status_.text) - 1, std::strlen(text));
        std::memcpy(status_.text, text, length);
        status_.text[length] = '\0';
    }

    TextInputStatus status_{};
};

SwitchImeManager* gImeManager = nullptr;
TextInputStatus gTextInputStatus{};
}

bool InitIme(ImeManager*& imeManager)
{
    if (gImeManager == nullptr)
    {
        gImeManager = new SwitchImeManager();
    }

    imeManager = gImeManager;
    gTextInputStatus = {};
    return true;
}

void ShutdownIme()
{
    delete gImeManager;
    gImeManager = nullptr;
    gTextInputStatus = {};
}

TextInputStatus GetTextInputStatus()
{
    if (gImeManager != nullptr)
    {
        gTextInputStatus = gImeManager->Status();
        gImeManager->ResetTransientFlags();
    }
    return gTextInputStatus;
}

void ResetTextInputStatus()
{
    if (gImeManager != nullptr)
    {
        gImeManager->ResetTransientFlags();
        gTextInputStatus = gImeManager->Status();
    }
    else
    {
        gTextInputStatus = {};
    }
}

}
