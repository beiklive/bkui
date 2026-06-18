#pragma once

#include <bkui/render/backend/deko3d/Deko3DDevice.hpp>
#include <bkui/render/backend/opengl/OpenGLDevice.hpp>
#include <bkui/core/Application.hpp>
#include <bkui/core/ApplicationHost.hpp>
#include <bkui/core/Event.hpp>
#include <bkui/core/FileSystem.hpp>
#include <bkui/core/I18n.hpp>
#include <bkui/core/Logger.hpp>
#include <bkui/core/MetaData.hpp>
#include <bkui/core/Types.hpp>
#include <bkui/platform/Font.hpp>
#include <bkui/platform/Ime.hpp>
#include <bkui/platform/Platform.hpp>
#include <bkui/render/Device.hpp>
#include <bkui/render/RenderCommand.hpp>
#include <bkui/render/RenderQueue.hpp>
#include <bkui/render/RenderQueueRenderer.hpp>
#include <bkui/ui/Box.hpp>
#include <bkui/ui/Button.hpp>
#include <bkui/ui/CheckBox.hpp>
#include <bkui/ui/Label.hpp>
#include <bkui/ui/LogOverlayView.hpp>
#include <bkui/ui/ProgressBar.hpp>
#include <bkui/ui/ScrollGestureController.hpp>
#include <bkui/ui/ScrollView.hpp>
#include <bkui/ui/Slider.hpp>
#include <bkui/ui/View.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

// 启用i18n宏定义，允许使用"_i18n"字面量操作符进行国际化字符串处理
#define BK_MACRO_USE_I18N  using bk::operator""_i18n;
