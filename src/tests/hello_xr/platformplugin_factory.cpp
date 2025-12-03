// Copyright (c) 2017-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "platformplugin.h"

#define UNUSED_PARM(x) \
    { (void)(x); }

std::shared_ptr<IPlatformPlugin> CreatePlatformPlugin(const std::shared_ptr<Options>& options,
                                                      const std::shared_ptr<PlatformData>& data) {
#if !defined(XR_USE_PLATFORM_ANDROID)
    UNUSED_PARM(data);
#endif

#if defined(XR_USE_PLATFORM_WIN32)
    return CreatePlatformPlugin_Win32(options);
#elif defined(XR_USE_PLATFORM_ANDROID)
    return CreatePlatformPlugin_Android(options, data);
#elif defined(XR_OS_APPLE) || defined(XR_OS_LINUX)
    return CreatePlatformPlugin_Posix(options);
#else
#error Unsupported platform or no XR platform defined!
#endif
}
