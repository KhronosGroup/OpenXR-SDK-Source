// Copyright (c) 2017-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "common.h"
#include "platformplugin.h"

#if defined(XR_OS_APPLE) || defined(XR_OS_LINUX)

namespace {
struct PosixPlatformPlugin : public IPlatformPlugin {
    PosixPlatformPlugin(const std::shared_ptr<Options>& /*unused*/) {}

    std::vector<std::string> GetInstanceExtensions() const override { return {}; }

    XrBaseInStructure* GetInstanceCreateExtension() const override { return nullptr; }

    void UpdateOptions(const std::shared_ptr<struct Options>& /*unused*/) override {}
};
}  // namespace

std::shared_ptr<IPlatformPlugin> CreatePlatformPlugin_Posix(const std::shared_ptr<Options>& options) {
    return std::make_shared<PosixPlatformPlugin>(options);
}

#endif  // defined(XR_OS_APPLE) || defined(XR_OS_LINUX)
