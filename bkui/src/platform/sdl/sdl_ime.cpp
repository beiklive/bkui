#include "sdl_platform_internal.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace bk::sdl
{
namespace
{
class SDLImeManager final : public ImeManager
{
public:
    explicit SDLImeManager(SDL_Window* window)
        : window_(window)
    {
    }

    bool OpenForText(
        std::function<void(std::string)> callback,
        std::string headerText,
        std::string subText,
        int maxStringLength,
        std::string initialText,
        int kbdDisableBitmask) override
    {
        (void)headerText;
        (void)subText;
        (void)kbdDisableBitmask;
        if (window_ == nullptr)
        {
            return false;
        }

        callbackText_ = std::move(callback);
        submitted_ = false;
        canceled_ = false;
        text_ = initialText;
        if (maxStringLength > 0 && static_cast<int>(text_.size()) > maxStringLength)
        {
            text_.resize(static_cast<std::size_t>(maxStringLength));
        }
        maxLength_ = std::max(1, maxStringLength);
        if (!SDL_StartTextInput(window_))
        {
            return false;
        }

        active_ = true;
        editing_ = false;
        SyncStatus();
        return true;
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
        (void)leftButton;
        (void)rightButton;
        return OpenForText(
            [callback = std::move(callback)](std::string text) {
                if (!text.empty() && callback)
                {
                    callback(std::strtol(text.c_str(), nullptr, 10));
                }
            },
            std::move(headerText),
            std::move(subText),
            maxStringLength,
            std::move(initialText),
            kbdDisableBitmask);
    }

    void Stop(bool canceled)
    {
        if (window_ != nullptr)
        {
            SDL_StopTextInput(window_);
        }

        active_ = false;
        editing_ = false;
        if (canceled)
        {
            canceled_ = true;
        }
        SyncStatus();
    }

    bool Active() const
    {
        return active_;
    }

    void HandleTextInput(const char* text)
    {
        if (!active_ || text == nullptr || *text == '\0')
        {
            return;
        }

        editing_ = false;
        const std::size_t allowed = static_cast<std::size_t>(std::max(0, maxLength_));
        if (text_.size() < allowed)
        {
            const std::size_t remaining = allowed - text_.size();
            text_ += std::string{text}.substr(0, remaining);
        }
        composition_.clear();
        compositionCursor_ = -1;
        SyncStatus();
    }

    void HandleTextEditing(const char* text, int cursor)
    {
        if (!active_)
        {
            return;
        }

        composition_ = text == nullptr ? std::string{} : std::string{text};
        compositionCursor_ = cursor;
        editing_ = !composition_.empty();
        SyncStatus();
    }

    bool HandleKeyDown(const SDL_KeyboardEvent& key)
    {
        if (!active_)
        {
            return false;
        }

        if (key.scancode == SDL_SCANCODE_ESCAPE)
        {
            Stop(true);
            return true;
        }

        if (key.scancode == SDL_SCANCODE_RETURN || key.scancode == SDL_SCANCODE_KP_ENTER)
        {
            Submit();
            return true;
        }

        if (key.scancode == SDL_SCANCODE_BACKSPACE && !text_.empty())
        {
            text_.pop_back();
            SyncStatus();
            return true;
        }

        return false;
    }

    void Submit()
    {
        if (!active_)
        {
            return;
        }

        submitted_ = true;
        SyncStatus();

        if (callbackText_)
        {
            callbackText_(text_);
        }

        Stop(false);
    }

    void SetInputArea(int x, int y, int width, int height, int cursor)
    {
        if (window_ == nullptr)
        {
            return;
        }

        SDL_Rect rect{x, y, width, height};
        SDL_SetTextInputArea(window_, &rect, cursor);
    }

    TextInputStatus Status() const
    {
        return status_;
    }

    void ResetTransientFlags()
    {
        submitted_ = false;
        canceled_ = false;
        SyncStatus();
    }

private:
    void SyncStatus()
    {
        status_ = {};
        status_.active = active_;
        status_.editing = editing_;
        status_.submitted = submitted_;
        status_.canceled = canceled_;
        status_.compositionCursor = compositionCursor_;
        const std::size_t textLen = std::min(text_.size(), sizeof(status_.text) - 1);
        std::memcpy(status_.text, text_.data(), textLen);
        status_.text[textLen] = '\0';
        const std::size_t compositionLen = std::min(composition_.size(), sizeof(status_.composition) - 1);
        std::memcpy(status_.composition, composition_.data(), compositionLen);
        status_.composition[compositionLen] = '\0';
    }

    SDL_Window* window_ = nullptr;
    std::function<void(std::string)> callbackText_;
    std::string text_;
    std::string composition_;
    TextInputStatus status_{};
    int maxLength_ = 32;
    int compositionCursor_ = -1;
    bool active_ = false;
    bool editing_ = false;
    bool submitted_ = false;
    bool canceled_ = false;
};

SDLImeManager* gImeManager = nullptr;
TextInputStatus gTextInputStatus{};
}

bool InitIme(SDL_Window* window, ImeManager*& imeManager)
{
    if (gImeManager != nullptr)
    {
        imeManager = gImeManager;
        return true;
    }

    gImeManager = new SDLImeManager(window);
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
    }
    return gTextInputStatus;
}

void ResetTextInputStatus()
{
    if (gImeManager == nullptr)
    {
        gTextInputStatus = {};
        return;
    }

    gImeManager->ResetTransientFlags();
    gTextInputStatus = gImeManager->Status();
}

bool IsImeActive()
{
    return gImeManager != nullptr && gImeManager->Active();
}

bool HandleImeKeyDown(const SDL_KeyboardEvent& event)
{
    if (gImeManager == nullptr)
    {
        return false;
    }

    return gImeManager->HandleKeyDown(event);
}

void HandleImeTextInput(const char* text)
{
    if (gImeManager != nullptr)
    {
        gImeManager->HandleTextInput(text);
    }
}

void HandleImeTextEditing(const char* text, int cursor)
{
    if (gImeManager != nullptr)
    {
        gImeManager->HandleTextEditing(text, cursor);
    }
}

void SetImeInputArea(SDL_Window* window, int x, int y, int width, int height, int cursor)
{
    if (gImeManager != nullptr)
    {
        gImeManager->SetInputArea(x, y, width, height, cursor);
    }
    else if (window != nullptr)
    {
        SDL_Rect rect{x, y, width, height};
        SDL_SetTextInputArea(window, &rect, cursor);
    }
}

}
