// Copyright (c) 2017-2020 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "platformplugin.h"

#define UNUSED_PARM(x) \
    { (void)(x); }

// Implementation in platformplugin_win32.cpp
std::shared_ptr<IPlatformPlugin> CreatePlatformPlugin_Win32(const std::shared_ptr<Options>& options);

std::shared_ptr<IPlatformPlugin> CreatePlatformPlugin_Wayland(const std::shared_ptr<Options>& /*unused*/) {
    // TODO: Implementation should go in its own cpp. Perhaps can share implementation with other
    // window managers?
    throw std::runtime_error("Wayland Platform Adapter Not Implemented");
}

std::shared_ptr<IPlatformPlugin> CreatePlatformPlugin_Xlib(const std::shared_ptr<Options>&);

std::shared_ptr<IPlatformPlugin> CreatePlatformPlugin_Xcb(const std::shared_ptr<Options>& /*unused*/) {
    // TODO: Implementation should go in its own cpp. Perhaps can share implementation with other
    // window managers?
    throw std::runtime_error("Xcb Platform Adapter Not Implemented");
}

std::shared_ptr<IPlatformPlugin> CreatePlatformPlugin_Android(const std::shared_ptr<Options>& /*unused*/,
                                                              const std::shared_ptr<PlatformData>& /*unused*/);

std::shared_ptr<IPlatformPlugin> CreatePlatformPlugin(const std::shared_ptr<Options>& options,
                                                      const std::shared_ptr<PlatformData>& data) {
#if !defined(XR_USE_PLATFORM_ANDROID)
    UNUSED_PARM(data);
#endif

#if defined(XR_USE_PLATFORM_WIN32)
    return CreatePlatformPlugin_Win32(options);
#elif defined(XR_USE_PLATFORM_ANDROID)
    return CreatePlatformPlugin_Android(options, data);
#elif defined(XR_USE_PLATFORM_XLIB)
    return CreatePlatformPlugin_Xlib(options);
#elif defined(XR_USE_PLATFORM_XCB)
    return CreatePlatformPlugin_Xcb(options);
#elif defined(XR_USE_PLATFORM_WAYLAND)
    return CreatePlatformPlugin_Wayland(options);
#else
#error Unsupported platform or no XR platform defined!
#endif
}
