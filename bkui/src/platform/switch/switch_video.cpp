#include "switch_platform_internal.hpp"

#define HANDHELD_WIDTH 1280.0f
#define HANDHELD_HEIGHT 720.0f

#define DOCKED_WIDTH 1920.f
#define DOCKED_HEIGHT 1080.f


namespace bk::sw
{

bool InitVideo()
{
    return R_SUCCEEDED(romfsInit());
}

void ShutdownVideo()
{
    romfsExit();
}

Vector2 GetWindowSize()
{
    int32_t width, height;
    Result rc = appletGetDefaultDisplayResolution(&width, &height);
    if (R_SUCCEEDED(rc))
    {
        return Vector2{static_cast<float>(width), static_cast<float>(height)};
    }
    // If it failed, use hardcoded resolutions as fallback
    else
    {
        switch (appletGetOperationMode())
        {
            case AppletOperationMode_Handheld:
                width = HANDHELD_WIDTH;
                height = HANDHELD_HEIGHT;
                break;
            case AppletOperationMode_Console:
                width = DOCKED_WIDTH;
                height = DOCKED_HEIGHT;
                break;
            default:
                return Vector2{1280.0F, 720.0F};
        }
    }
    return Vector2{static_cast<float>(width), static_cast<float>(height)};
}

}
