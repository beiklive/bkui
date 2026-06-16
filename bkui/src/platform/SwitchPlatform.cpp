#include <bkui/platform/Platform.hpp>

#include <memory>
#include <utility>

#if defined(BKUI_PLATFORM_SWITCH)
#include <switch.h>
#endif

namespace bk
{
namespace
{
class SwitchPlatform final : public Platform
{
public:
    explicit SwitchPlatform(WindowDesc desc)
        : desc_(std::move(desc))
    {
    }

    bool Init() override
    {
#if defined(BKUI_PLATFORM_SWITCH)
        if (R_FAILED(romfsInit()))
        {
            return false;
        }
        padConfigureInput(1, HidNpadStyleSet_NpadStandard);
        padInitializeDefault(&pad_);
#endif
        running_ = true;
        return true;
    }

    void PollEvents() override
    {
        input_ = {};
#if defined(BKUI_PLATFORM_SWITCH)
        if (!appletMainLoop())
        {
            running_ = false;
            input_.quitRequested = true;
            return;
        }

        padUpdate(&pad_);
        const u64 down = padGetButtonsDown(&pad_);
        if ((down & HidNpadButton_Plus) != 0)
        {
            running_ = false;
            input_.quitRequested = true;
        }
        input_.confirmPressed = (down & HidNpadButton_A) != 0;
        input_.cancelPressed = (down & HidNpadButton_B) != 0;
#endif
    }

    void Shutdown() override
    {
#if defined(BKUI_PLATFORM_SWITCH)
        romfsExit();
#endif
        running_ = false;
    }

    void* GetWindow() override
    {
        return nullptr;
    }

    InputState GetInput() const override
    {
        return input_;
    }

    Vector2 GetWindowSize() const override
    {
        return Vector2{1280.0F, 720.0F};
    }

    bool IsRunning() const override
    {
        return running_;
    }

    bool CreateOpenGLContext(const OpenGLContextDesc& desc) override
    {
        (void)desc;
        return false;
    }

    void MakeOpenGLContextCurrent() override {}
    void SwapOpenGLBuffers() override {}
    void DestroyOpenGLContext() override {}

    OpenGLProcAddress GetOpenGLProcAddress(const char* name) override
    {
        (void)name;
        return nullptr;
    }

private:
    WindowDesc desc_;
    InputState input_;
    bool running_ = false;
#if defined(BKUI_PLATFORM_SWITCH)
    PadState pad_{};
#endif
};
}

std::unique_ptr<Platform> CreateDefaultPlatform(const WindowDesc& desc)
{
    return std::make_unique<SwitchPlatform>(desc);
}

}
