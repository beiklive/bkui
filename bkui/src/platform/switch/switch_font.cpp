#include "switch_platform_internal.hpp"

namespace bk::sw
{

namespace
{

void TryAppendResourceFont(std::vector<Buffer>& fonts, const char* path)
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

    if (R_SUCCEEDED(plInitialize(PlServiceType_User)))
    {
        const PlSharedFontType candidates[] = {
            PlSharedFontType_Standard,
            PlSharedFontType_ChineseSimplified,
            PlSharedFontType_ExtChineseSimplified,
            PlSharedFontType_ChineseTraditional,
            PlSharedFontType_KO,
            PlSharedFontType_NintendoExt,
        };

        for (PlSharedFontType type : candidates)
        {
            PlFontData sharedFont{};
            if (R_SUCCEEDED(plGetSharedFontByType(&sharedFont, type)) &&
                sharedFont.address != nullptr && sharedFont.size > 0)
            {
                auto* bytes = static_cast<const std::uint8_t*>(sharedFont.address);
                fonts.emplace_back(bytes, bytes + sharedFont.size);
            }
        }

        plExit();
    }

    if (fonts.empty())
    {
        TryAppendResourceFont(fonts, "font/switch_font.ttf");
    }

    TryAppendResourceFont(fonts, "material/MaterialIcons-Regular.ttf");
    return fonts;
}

}

namespace bk
{

std::vector<Buffer> LoadPlatformFonts()
{
    return sw::LoadFonts();
}

}
