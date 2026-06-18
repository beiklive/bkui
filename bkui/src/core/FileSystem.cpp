#include <bkui/core/FileSystem.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

#if defined(BKUI_USE_PHYSFS)
#include <physfs.h>
#endif

namespace bk
{
namespace
{
bool g_initialized = false;

std::string NormalizePathString(const std::filesystem::path& path)
{
    return path.generic_string();
}
}

bool FileSystem::Init(const char* argv0)
{
#if defined(BKUI_USE_PHYSFS)
    if (!g_initialized)
    {
        g_initialized = PHYSFS_init(argv0) != 0;
    }
    return g_initialized;
#else
    (void)argv0;
    g_initialized = true;
    return true;
#endif
}

void FileSystem::Shutdown()
{
#if defined(BKUI_USE_PHYSFS)
    if (g_initialized)
    {
        PHYSFS_deinit();
    }
#endif
    g_initialized = false;
}

std::string FileSystem::DefaultResourceRoot()
{
#if defined(BKUI_PLATFORM_SWITCH)
    return "romfs:/";
#elif defined(BKUI_PLATFORM_MACOS)
    const std::filesystem::path bundleResources = std::filesystem::path("..") / "Resources" / "resources";
    if (std::filesystem::exists(bundleResources))
    {
        return NormalizePathString(bundleResources);
    }
    return "resources";
#else
    return "resources";
#endif
}

bool FileSystem::MountDefaultResources()
{
    return Mount(DefaultResourceRoot());
}

bool FileSystem::Mount(const std::string& path)
{
#if defined(BKUI_USE_PHYSFS)
    return g_initialized && PHYSFS_mount(path.c_str(), nullptr, 1) != 0;
#else
    (void)path;
    return g_initialized;
#endif
}

Buffer FileSystem::Read(const std::string& path)
{
#if defined(BKUI_USE_PHYSFS)
    if (g_initialized)
    {
        PHYSFS_File* file = PHYSFS_openRead(path.c_str());
        if (file != nullptr)
        {
            const PHYSFS_sint64 length = PHYSFS_fileLength(file);
            Buffer buffer;
            if (length > 0)
            {
                buffer.resize(static_cast<std::size_t>(length));
                const PHYSFS_sint64 read = PHYSFS_readBytes(file, buffer.data(), length);
                if (read != length)
                {
                    PHYSFS_close(file);
                    throw std::runtime_error("Failed to read full resource: " + path);
                }
            }
            PHYSFS_close(file);
            return buffer;
        }
    }
#endif

    std::ifstream stream(ResolvePath(path), std::ios::binary);
    if (!stream)
    {
        throw std::runtime_error("Resource not found: " + path);
    }

    return Buffer(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::string FileSystem::ResolvePath(const std::string& path)
{
#if defined(BKUI_PLATFORM_SWITCH)
    return "romfs:/" + path;
#elif defined(BKUI_PLATFORM_MACOS)
    const std::filesystem::path bundleResources = std::filesystem::path("..") / "Resources" / "resources";
    if (std::filesystem::exists(bundleResources))
    {
        return NormalizePathString(bundleResources / path);
    }
    return "resources/" + path;
#else
    return "resources/" + path;
#endif
}

}
