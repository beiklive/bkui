#pragma once

#include <bkui/core/Types.hpp>

#include <string>

namespace bk
{

class FileSystem
{
public:
    static bool Init(const char* argv0 = nullptr);
    static void Shutdown();
    static bool Mount(const std::string& path);
    static Buffer Read(const std::string& path);
    static std::string ResolvePath(const std::string& path);
};

}
