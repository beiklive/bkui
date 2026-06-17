#include <bkui/core/I18n.hpp>
#include <bkui/core/FileSystem.hpp>
#include <bkui/core/Logger.hpp>

#include <json.hpp>

#if !defined(BKUI_PLATFORM_SWITCH)
#include <SDL3/SDL_locale.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <format>

namespace bk
{

std::string I18nString::ToString(int v)
{
    return std::to_string(v);
}

std::string I18nString::ToString(float v)
{
    return std::format("{:.2f}", v);
}

std::string I18nString::ToString(double v)
{
    return std::format("{:.2f}", v);
}

std::string I18nString::str() const
{
    return I18n::Instance().Lookup(*this);
}

std::u32string I18nString::u32() const
{
    const std::string utf8 = I18n::Instance().Lookup(*this);
    std::u32string result;
    result.reserve(utf8.size());
    std::size_t i = 0;
    while (i < utf8.size())
    {
        const unsigned char c = static_cast<unsigned char>(utf8[i]);
        if (c < 0x80)
        {
            result.push_back(static_cast<char32_t>(c));
            ++i;
        }
        else if ((c & 0xE0) == 0xC0 && i + 1 < utf8.size())
        {
            const char32_t cp =
                (static_cast<char32_t>(c & 0x1F) << 6) |
                static_cast<char32_t>(static_cast<unsigned char>(utf8[i + 1]) & 0x3F);
            result.push_back(cp);
            i += 2;
        }
        else if ((c & 0xF0) == 0xE0 && i + 2 < utf8.size())
        {
            const char32_t cp =
                (static_cast<char32_t>(c & 0x0F) << 12) |
                (static_cast<char32_t>(static_cast<unsigned char>(utf8[i + 1]) & 0x3F) << 6) |
                static_cast<char32_t>(static_cast<unsigned char>(utf8[i + 2]) & 0x3F);
            result.push_back(cp);
            i += 3;
        }
        else if ((c & 0xF8) == 0xF0 && i + 3 < utf8.size())
        {
            const char32_t cp =
                (static_cast<char32_t>(c & 0x07) << 18) |
                (static_cast<char32_t>(static_cast<unsigned char>(utf8[i + 1]) & 0x3F) << 12) |
                (static_cast<char32_t>(static_cast<unsigned char>(utf8[i + 2]) & 0x3F) << 6) |
                static_cast<char32_t>(static_cast<unsigned char>(utf8[i + 3]) & 0x3F);
            result.push_back(cp);
            i += 4;
        }
        else
        {
            result.push_back(U'?');
            ++i;
        }
    }
    return result;
}

std::string I18n::Lookup(const I18nString& s) const
{
    auto it = translations_.find(s.key_);
    std::string tmpl = (it != translations_.end()) ? it->second : std::string(s.key_);

    if (s.args_.empty())
    {
        return tmpl;
    }

    std::string result;
    result.reserve(tmpl.size() + s.args_.size() * 16);
    std::size_t argPos = 0;

    for (std::size_t i = 0; i < tmpl.size(); ++i)
    {
        if (tmpl[i] == '{' && i + 1 < tmpl.size() && tmpl[i + 1] == '}')
        {
            if (argPos < s.args_.size())
            {
                result += s.args_[argPos++];
            }
            ++i;
        }
        else
        {
            result += tmpl[i];
        }
    }
    return result;
}

bool I18n::LoadFile(std::string_view langCode)
{
    const std::string path = resourceDir_ + "/" + std::string(langCode) + ".json";
    try
    {
        Buffer data = FileSystem::Read(path);
        if (!data.empty())
        {
            const std::string content(reinterpret_cast<const char*>(data.data()), data.size());
            nlohmann::json json = nlohmann::json::parse(content);
            if (json.is_object())
            {
                translations_.clear();
                for (auto it = json.begin(); it != json.end(); ++it)
                {
                    if (it.value().is_string())
                    {
                        translations_[it.key()] = it.value().get<std::string>();
                    }
                }
                return true;
            }
        }
    }
    catch (const std::exception& ex)
    {
        bklog.warn("I18n: Failed to load " + path + ": " + ex.what());
    }
    return false;
}

std::string I18n::DetectSystemLanguage()
{
#if !defined(BKUI_PLATFORM_SWITCH)
    int count = 0;
    SDL_Locale** locales = SDL_GetPreferredLocales(&count);
    if (locales && count > 0 && locales[0]->language)
    {
        std::string lang = locales[0]->language;
        if (locales[0]->country)
        {
            lang += "-" + std::string(locales[0]->country);
        }
        SDL_free(locales);
        return lang;
    }
#endif
    return "en";
}

void I18n::Init(std::string_view resourceDir)
{
    resourceDir_ = resourceDir;
    std::string lang = DetectSystemLanguage();

    // Try exact match (e.g. "zh-Hans-CN") → strip region ("zh-Hans") → strip script ("zh") → fallback "en"
    if (LoadFile(lang))
    {
        currentLang_ = lang;
        bklog.info("I18n: Loaded language " + lang);
        return;
    }

    std::string base = lang;
    auto dashPos = base.find('-');
    while (dashPos != std::string::npos)
    {
        base = base.substr(0, dashPos);
        if (LoadFile(base))
        {
            currentLang_ = base;
            bklog.info("I18n: Loaded language " + base);
            return;
        }
        dashPos = base.find('-');
    }

    // Try "zh-CN" style for Chinese variants
    if (lang.size() >= 2 && lang[0] == 'z' && lang[1] == 'h')
    {
        if (LoadFile("zh-CN"))
        {
            currentLang_ = "zh-CN";
            bklog.info("I18n: Loaded language zh-CN (fallback)");
            return;
        }
    }

    if (LoadFile("en"))
    {
        currentLang_ = "en";
        bklog.info("I18n: Loaded language en (default)");
        return;
    }

    currentLang_ = "en";
    translations_.clear();
    bklog.warn("I18n: No translation files found in " + std::string(resourceDir) + "/, using raw keys.");
}

void I18n::SetLanguage(std::string_view langCode)
{
    std::string lang(langCode);
    if (currentLang_ == lang)
    {
        return;
    }

    if (LoadFile(lang))
    {
        currentLang_ = lang;
        bklog.info("I18n: Switched language to " + lang);
    }
    else
    {
        bklog.warn("I18n: Failed to switch language to " + lang);
    }
}

I18n& I18n::Instance()
{
    static I18n instance;
    return instance;
}

}
