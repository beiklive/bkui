#pragma once

#include <bkui/render/backend/deko3d/Deko3DDevice.hpp>
#include <bkui/render/backend/opengl/OpenGLDevice.hpp>
#include <bkui/core/Types.hpp>
#include <bkui/core/FileSystem.hpp>
#include <bkui/platform/Font.hpp>
#include <bkui/platform/Ime.hpp>
#include <bkui/platform/Platform.hpp>
#include <bkui/render/Device.hpp>
#include <bkui/render/RenderCommand.hpp>
#include <bkui/render/RenderQueue.hpp>
#include <bkui/ui/Box.hpp>
#include <bkui/ui/Button.hpp>
#include <bkui/ui/Label.hpp>
#include <bkui/ui/View.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
