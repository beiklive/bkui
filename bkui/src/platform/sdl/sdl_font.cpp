#include "sdl_platform_internal.hpp"

namespace bk::sdl
{

namespace
{

void TryAppendFont(std::vector<Buffer>& fonts, const char* path)
{
    try
    {
        Buffer data = FileSystem::Read(path);
        if (!data.empty())
        {
            fonts.push_back(std::move(data));
        }
    }
    catch (...)
    {
    }
}

}

std::vector<Buffer> LoadFonts()
{
    std::vector<Buffer> fonts;
    TryAppendFont(fonts, "font/switch_font.ttf");
    TryAppendFont(fonts, "material/MaterialIcons-Regular.ttf");
    return fonts;
}

}

namespace bk
{

std::vector<Buffer> LoadPlatformFonts()
{
    return sdl::LoadFonts();
}

}
