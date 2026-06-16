#pragma once

#include <bkui/platform/Platform.hpp>
#include <bkui/render/Device.hpp>

#include <memory>

namespace bk
{

/// 创建一个基于 OpenGL 的图形设备实例。
std::unique_ptr<Device> CreateOpenGLDevice(Platform& platform);

}
