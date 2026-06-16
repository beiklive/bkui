#pragma once

#include <bkui/core/Types.hpp>

#include <vector>

namespace bk
{

/// 读取当前平台可用的系统字体数据，用于构建字体回退链。
std::vector<Buffer> LoadPlatformFonts();

}
