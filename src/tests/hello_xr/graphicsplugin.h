// Copyright (c) 2017-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "swapchain_image_data.h"
#include <nonstd/span.hpp>
using nonstd::span;

struct Cube {
    XrPosef Pose;
    XrVector3f Scale;
};

// Wraps a graphics API so the main openxr program can be graphics API-independent.
struct IGraphicsPlugin {
    virtual ~IGraphicsPlugin() = default;

    // OpenXR extensions required by this graphics API.
    virtual std::vector<std::string> GetInstanceExtensions() const = 0;

    // Create an instance of this graphics api for the provided instance and systemId.
    virtual void InitializeDevice(XrInstance instance, XrSystemId systemId) = 0;

    // Select the preferred swapchain format from the list of available formats.
    virtual int64_t SelectColorSwapchainFormat(bool throwIfNotFound, span<const int64_t> imageFormatArray) const = 0;
    virtual int64_t SelectDepthSwapchainFormat(bool throwIfNotFound, span<const int64_t> imageFormatArray) const = 0;

    // Get the graphics binding header for session creation.
    virtual const XrBaseInStructure* GetGraphicsBinding() const = 0;

    /// Allocates an object owning (among other things) an array of XrSwapchainImage* in a portable way and
    /// returns an **observing** pointer to an interface providing generic access to the associated pointers.
    /// (The object remains owned by the graphics plugin, and will be destroyed on @ref ShutdownDevice())
    /// This is all for the purpose of being able to call the xrEnumerateSwapchainImages function
    /// in a platform-independent way. The user of this must not use the images beyond @ref ShutdownDevice()
    ///
    /// Example usage:
    ///
    /// ```c++
    /// ISwapchainImageData * p = graphicsPlugin->AllocateSwapchainImageData(3, swapchainCreateInfo);
    /// xrEnumerateSwapchainImages(swapchain, 3, &count, p->GetColorImageArray());
    /// ```
    virtual ISwapchainImageData* AllocateSwapchainImageData(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo) = 0;

    /// Allocates an object owning (among other things) an array of XrSwapchainImage* in a portable way and
    /// returns an **observing** pointer to an interface providing generic access to the associated pointers.
    ///
    /// Signals that we will use a depth swapchain allocated by the runtime, instead of a fallback depth
    /// allocated by the plugin.
    virtual ISwapchainImageData* AllocateSwapchainImageDataWithDepthSwapchain(
        size_t size, const XrSwapchainCreateInfo& colorSwapchainCreateInfo, XrSwapchain depthSwapchain,
        const XrSwapchainCreateInfo& depthSwapchainCreateInfo) = 0;

    // Render to a swapchain image for a projection view.
    virtual void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* swapchainImage,
                            int64_t swapchainFormat, const std::vector<Cube>& cubes) = 0;

    // Get recommended number of sub-data element samples in view (recommendedSwapchainSampleCount)
    // if supported by the graphics plugin. A supported value otherwise.
    virtual uint32_t GetSupportedSwapchainSampleCount(const XrViewConfigurationView& view) {
        return view.recommendedSwapchainSampleCount;
    }

    // Perform required steps after updating Options
    virtual void UpdateOptions(const std::shared_ptr<struct Options>& options) = 0;
};

// Graphics API factories are forward declared here.
#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_OpenGLES(const std::shared_ptr<struct Options>& options,
                                                               std::shared_ptr<struct IPlatformPlugin> platformPlugin);
#endif
#ifdef XR_USE_GRAPHICS_API_OPENGL
std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_OpenGL(const std::shared_ptr<struct Options>& options,
                                                             std::shared_ptr<struct IPlatformPlugin> platformPlugin);
#endif
#ifdef XR_USE_GRAPHICS_API_VULKAN
std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_VulkanLegacy(const std::shared_ptr<struct Options>& options,
                                                                   std::shared_ptr<struct IPlatformPlugin> platformPlugin);

std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_Vulkan(const std::shared_ptr<struct Options>& options,
                                                             std::shared_ptr<struct IPlatformPlugin> platformPlugin);
#endif
#ifdef XR_USE_GRAPHICS_API_D3D11
std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_D3D11(const std::shared_ptr<struct Options>& options,
                                                            std::shared_ptr<struct IPlatformPlugin> platformPlugin);
#endif
#ifdef XR_USE_GRAPHICS_API_D3D12
std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_D3D12(const std::shared_ptr<struct Options>& options,
                                                            std::shared_ptr<struct IPlatformPlugin> platformPlugin);
#endif
#ifdef XR_USE_GRAPHICS_API_METAL
std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_Metal(const std::shared_ptr<struct Options>& options,
                                                            std::shared_ptr<struct IPlatformPlugin> platformPlugin);
#endif

// Create a graphics plugin for the graphics API specified in the options.
std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin(const std::shared_ptr<struct Options>& options,
                                                      std::shared_ptr<struct IPlatformPlugin> platformPlugin);
