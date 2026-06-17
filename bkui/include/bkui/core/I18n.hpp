#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <cstddef>

namespace bk
{

class I18n;

class I18nString
{
public:
    std::string str() const;
    std::u32string u32() const;

    template<typename... Args>
    I18nString operator()(Args&&... args) const
    {
        I18nString result(key_);
        (result.args_.push_back(ToString(std::forward<Args>(args))), ...);
        return result;
    }

private:
    friend class I18n;
    friend I18nString operator""_i18n(const char* key, std::size_t);

    explicit I18nString(const char* key) : key_(key) {}

    static std::string ToString(const std::string& s) { return s; }
    static std::string ToString(std::string&& s) { return std::move(s); }
    static std::string ToString(const char* s) { return std::string(s); }
    static std::string ToString(std::string_view s) { return std::string(s); }
    static std::string ToString(int v);
    static std::string ToString(float v);
    static std::string ToString(double v);

    const char* key_;
    std::vector<std::string> args_;
};

inline I18nString operator""_i18n(const char* key, std::size_t)
{
    return I18nString(key);
}

class I18n
{
public:
    static I18n& Instance();

    void Init(std::string_view resourceDir = "i18n");
    void SetLanguage(std::string_view langCode);
    std::string_view CurrentLanguage() const { return currentLang_; }

private:
    friend class I18nString;
    std::string Lookup(const I18nString& s) const;

    std::string currentLang_ = "en";
    std::string resourceDir_ = "i18n";
    std::unordered_map<std::string, std::string> translations_;

    std::string DetectSystemLanguage();
    bool LoadFile(std::string_view langCode);
};

}
