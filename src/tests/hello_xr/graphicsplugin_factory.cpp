// Copyright (c) 2017-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include <utility>

#include "pch.h"
#include "common.h"
#include "platformdata.h"
#include "graphicsplugin.h"

namespace {
using GraphicsPluginFactory = std::function<std::shared_ptr<IGraphicsPlugin>()>;

std::map<std::string, GraphicsPluginFactory, IgnoreCaseStringLess> graphicsPluginMap = {
#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
    {"OpenGLES", CreateGraphicsPlugin_OpenGLES},
#endif
#ifdef XR_USE_GRAPHICS_API_OPENGL
    {"OpenGL", CreateGraphicsPlugin_OpenGL},
#endif
#ifdef XR_USE_GRAPHICS_API_VULKAN
    {"Vulkan", CreateGraphicsPlugin_VulkanLegacy}, {"Vulkan2", CreateGraphicsPlugin_Vulkan},
#endif
#ifdef XR_USE_GRAPHICS_API_D3D11
    {"D3D11", CreateGraphicsPlugin_D3D11},
#endif
#ifdef XR_USE_GRAPHICS_API_D3D12
    {"D3D12", CreateGraphicsPlugin_D3D12},
#endif
#ifdef XR_USE_GRAPHICS_API_METAL
    {"Metal", CreateGraphicsPlugin_Metal},
#endif
};
}  // namespace

std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin(const std::string graphicsPluginName) {
    if (graphicsPluginName.empty()) {
        throw std::invalid_argument("No graphics API specified");
    }

    const auto apiIt = graphicsPluginMap.find(graphicsPluginName);
    if (apiIt == graphicsPluginMap.end()) {
        throw std::invalid_argument(Fmt("Unsupported graphics API '%s'", graphicsPluginName.c_str()));
    }

    return apiIt->second();
}
