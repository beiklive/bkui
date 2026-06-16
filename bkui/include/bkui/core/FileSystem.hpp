#pragma once

#include <bkui/core/Types.hpp>

#include <string>

namespace bk
{

/// 统一资源文件系统接口。
class FileSystem
{
public:
    /// 初始化文件系统，可选传入程序路径用于定位资源根目录。
    static bool Init(const char* argv0 = nullptr);
    /// 关闭文件系统并释放相关状态。
    static void Shutdown();
    /// 挂载一个资源目录或资源包路径。
    static bool Mount(const std::string& path);
    /// 读取指定虚拟路径下的完整文件内容。
    static Buffer Read(const std::string& path);
    /// 将虚拟路径解析为实际可访问的物理路径。
    static std::string ResolvePath(const std::string& path);
};

}
