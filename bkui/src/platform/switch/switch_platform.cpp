#include "switch_platform_internal.hpp"

#include <memory>
#include <utility>

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
        if (!sw::InitVideo())
        {
            return false;
        }
        if (!sw::InitAudio() || !sw::InitIme(imeManager_) || !sw::InitInput(pad_))
        {
            Shutdown();
            return false;
        }

        running_ = true;
        return true;
    }

    void PollEvents() override
    {
        sw::PollInput(pad_, running_, input_);
    }

    void Shutdown() override
    {
        sw::ShutdownInput();
        sw::ShutdownIme();
        sw::ShutdownAudio();
        sw::ShutdownVideo();
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
        return sw::GetWindowSize();
    }

    bool IsRunning() const override
    {
        return running_;
    }

    ImeManager* GetImeManager() override
    {
        return imeManager_;
    }

    bool StartTextInput() override
    {
        return false;
    }

    void StopTextInput() override
    {
    }

    bool IsTextInputActive() const override
    {
        return false;
    }

    void SetTextInputArea(int x, int y, int width, int height, int cursor) override
    {
        (void)x;
        (void)y;
        (void)width;
        (void)height;
        (void)cursor;
    }

    TextInputStatus GetTextInputStatus() const override
    {
        return sw::GetTextInputStatus();
    }

    bool CreateOpenGLContext(const OpenGLContextDesc& desc) override
    {
        (void)desc;
        return false;
    }

    void MakeOpenGLContextCurrent() override
    {
    }

    void SwapOpenGLBuffers() override
    {
    }

    void DestroyOpenGLContext() override
    {
    }

    OpenGLProcAddress GetOpenGLProcAddress(const char* name) override
    {
        (void)name;
        return nullptr;
    }

private:
    WindowDesc desc_;
    ImeManager* imeManager_ = nullptr;
    InputState input_;
    bool running_ = false;
    PadState pad_{};
};
}

std::unique_ptr<Platform> CreateDefaultPlatform(const WindowDesc& desc)
{
    return std::make_unique<SwitchPlatform>(desc);
}

}
