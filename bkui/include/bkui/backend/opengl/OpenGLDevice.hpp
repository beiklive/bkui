#pragma once

#include <bkui/platform/Platform.hpp>
#include <bkui/rhi/Device.hpp>

#include <memory>

namespace bk
{

std::unique_ptr<Device> CreateOpenGLDevice(Platform& platform);

}
