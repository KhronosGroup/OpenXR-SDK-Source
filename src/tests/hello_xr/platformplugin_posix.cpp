// Copyright (c) 2017-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "common.h"
#include "platformplugin.h"

#if defined(XR_OS_APPLE) || defined(XR_OS_LINUX)

namespace {
struct PosixPlatformPlugin : public IPlatformPlugin {
    PosixPlatformPlugin() {}

    std::vector<std::string> GetInstanceExtensions() const override { return {}; }

    XrBaseInStructure* GetInstanceCreateExtension() const override { return nullptr; }
};
}  // namespace

std::shared_ptr<IPlatformPlugin> CreatePlatformPlugin_Posix() { return std::make_shared<PosixPlatformPlugin>(); }

#endif  // defined(XR_OS_APPLE) || defined(XR_OS_LINUX)
