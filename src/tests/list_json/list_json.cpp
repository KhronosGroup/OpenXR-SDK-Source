// Copyright (c) 2017-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif  // defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)

#include "xr_dependencies.h"

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_reflection.h>

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <sstream>
#include <vector>

#include <ctype.h>  // tolower
#include <stdio.h>
#include <string.h>

#if defined(XR_USE_PLATFORM_ANDROID)
#include <android_native_app_glue.h>

#define LOG_TAG "list_json"
#include "android_logging.h"
#define LOGE(...) ALOGE(__VA_ARGS__)
#define LOGI(...) ALOGI(__VA_ARGS__)
#else  // !defined(XR_USE_PLATFORM_ANDROID)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#define LOGI(...) printf(__VA_ARGS__)
#endif  // defined(XR_USE_PLATFORM_ANDROID)

#if defined(XR_USE_PLATFORM_ANDROID)
static struct android_app* g_app = nullptr;
#endif

// The equivalent of C++17 std::size. A helper to get the dimension for an array.
template <typename T, std::size_t Size>
constexpr std::size_t ArraySize(const T (&unused)[Size]) noexcept {
    (void)unused;
    return Size;
}

static std::string uint32_to_hex(uint32_t val) {
    char buf[32]{0};
    snprintf(buf, sizeof(buf), "0x%08" PRIx32, val);
    return std::string{buf};
}

static std::string vendorNameFromRuntimeName(const char* runtimeName) {
    static constexpr std::pair<const char*, const char*> vendorLookup[] = {
        {"Qualcomm", "QUALCOMM"},
        {"PICO", "BYTEDANCE"},
        {"HTC", "HTC America, Inc."},
        {"Windows Mixed Reality", "Microsoft Corporation"},
        {"Oculus", "Meta Platforms"},
        {"SteamVR", "Valve Corporation"},
        {"Varjo", "Varjo Technologies Oy"},
        // Monado needs to appear last in this last as other vendors may be using
        // Monado runtime for their headsets.
        {"Monado", "Monado Community + Collabora, Ltd."},
    };

    for (auto it : vendorLookup) {
        if (strstr(runtimeName, it.first) != nullptr) {
            return it.second;
        }
    }

    return "Unknown vendor (" + std::string(runtimeName) + ")";
}

static std::string viewConfigurationTypeName(XrViewConfigurationType config) {
#define AS_LIST(name, val) {name, #name},
    static constexpr std::pair<XrViewConfigurationType, const char*> KnownViewTypes[] = {
        XR_LIST_ENUM_XrViewConfigurationType(AS_LIST)};
#undef AS_LIST

    auto it = std::find_if(KnownViewTypes, KnownViewTypes + ArraySize(KnownViewTypes),
                           [&](const auto& vt) { return vt.first == config; });
    if (it != KnownViewTypes + ArraySize(KnownViewTypes)) {
        return it->second;
    } else {
        return "Unknown XrViewConfigurationType (" + uint32_to_hex((uint32_t)config) + ")";
    }
}

static std::string envBlendModeTypeName(XrEnvironmentBlendMode blendMode) {
    // JSON schema for our files requires blend modes to be short strings rather than
    // the spec strings.

    constexpr bool useShortBlendModes = true;

    if (useShortBlendModes) {
        switch (blendMode) {
            case XR_ENVIRONMENT_BLEND_MODE_OPAQUE:
                return "OPAQUE";
            case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE:
                return "ADDITIVE";
            case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND:
                return "ALPHA_BLEND";
            default:
                return "UNKNOWN";
        }
    } else {
#define AS_LIST(name, val) {name, #name},
        static constexpr std::pair<XrEnvironmentBlendMode, const char*> KnownBlendModes[] = {
            XR_LIST_ENUM_XrEnvironmentBlendMode(AS_LIST)};
#undef AS_LIST

        auto it = std::find_if(KnownBlendModes, KnownBlendModes + ArraySize(KnownBlendModes),
                               [&](const auto& vt) { return vt.first == blendMode; });
        if (it != KnownBlendModes + ArraySize(KnownBlendModes)) {
            return it->second;
        } else {
            return "Unknown XrEnvironmentBlendMode (" + uint32_to_hex((uint32_t)blendMode) + ")";
        }
    }
}

static std::string stringFromXrVersion(XrVersion ver) {
    char buf[128]{0};
    snprintf(buf, sizeof(buf), "%d.%d.%d", XR_VERSION_MAJOR(ver), XR_VERSION_MINOR(ver), XR_VERSION_PATCH(ver));
    return std::string{buf};
}

static int main_body() {
    XrInstanceCreateInfo instanceCreateInfo = {XR_TYPE_INSTANCE_CREATE_INFO};
    strncpy(instanceCreateInfo.applicationInfo.applicationName, "OpenXR-Inventory List", XR_MAX_APPLICATION_NAME_SIZE);
    // Current version is 1.1.x, but this app only requires 1.0.x
    instanceCreateInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0;

#ifdef XR_USE_PLATFORM_ANDROID
    XrInstanceCreateInfoAndroidKHR iciAndroid = {XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
    iciAndroid.applicationActivity = g_app->activity->clazz;
    iciAndroid.applicationVM = g_app->activity->vm;

    const char* platformExts[] = {XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME};
    instanceCreateInfo.enabledExtensionNames = platformExts;
    instanceCreateInfo.enabledExtensionCount = 1;
    instanceCreateInfo.next = &iciAndroid;
#endif

    XrInstance instance = XR_NULL_HANDLE;

    if (xrCreateInstance(&instanceCreateInfo, &instance) != XR_SUCCESS) {
        LOGE("Failed to create XR instance.\n");
        return 1;
    }

    auto instanceDestroyer = [](XrInstance* instance) {
        if (instance != XR_NULL_HANDLE) {
            xrDestroyInstance(*instance);
        }
    };

    std::unique_ptr<XrInstance, decltype(instanceDestroyer)> wrapperInstance({&instance, instanceDestroyer});

    XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
    if (xrGetInstanceProperties(instance, &instanceProperties) != XR_SUCCESS) {
        LOGE("Failed to get instance properties.\n");
        return 1;
    }

    // TODO: might be a bit neater to use jsoncpp here.

    LOGI("{\n");
    std::string versionStr = stringFromXrVersion(instanceProperties.runtimeVersion);
    LOGI("    \"notes\": \"Generated using list_json: '%s' (%s)\",\n", instanceProperties.runtimeName, versionStr.c_str());
    LOGI("    \"$schema\": \"../schema.json\",\n");
    LOGI("    \"name\": \"TODO\",\n");
    LOGI("    \"conformance_submission\": 0,\n");

#if defined(WIN32)
    LOGI("    \"platform\": \"Windows (Desktop)\",\n");
#elif defined(__ANDROID__)
    LOGI("    \"platform\": \"Android (All-in-one)\",\n");
#endif

    std::string vendorName = vendorNameFromRuntimeName(instanceProperties.runtimeName);
    LOGI("    \"vendor\": \"%s\",\n", vendorName.c_str());

    uint32_t size;
    xrEnumerateInstanceExtensionProperties(nullptr, 0, &size, nullptr);
    std::vector<XrExtensionProperties> extensions(size, {XR_TYPE_EXTENSION_PROPERTIES});
    xrEnumerateInstanceExtensionProperties(nullptr, size, &size, extensions.data());

    // remove XR_EXT_debug_utils
    // The loader implements XR_EXT_debug_utils so it will always look like
    // the runtime implements the extension...
    auto const& debugUtilsIt = std::find_if(extensions.begin(), extensions.end(),
                                            [](auto const& ext) { return strcmp(ext.extensionName, "XR_EXT_debug_utils") == 0; });
    if (debugUtilsIt != extensions.end()) {
        extensions.erase(debugUtilsIt);
    }

    // case insensitive sort first
    // then we will print KHR, EXT and others in order after
    std::sort(extensions.begin(), extensions.end(), [](auto const& a, auto const& b) {
        return std::lexicographical_compare(a.extensionName, a.extensionName + strlen(a.extensionName), b.extensionName,
                                            b.extensionName + strlen(b.extensionName),
                                            [](char a, char b) { return tolower(a) < tolower(b); });
    });

    // skip extensions which are not listed in openxr_reflection.h
    auto isPublicExtension = [](const XrExtensionProperties& extension) -> bool {
#define MAKE_EXTENSION_NAME_LIST(NAME, NUM) #NAME,
        static const std::vector<const char*> publicExtensions = {XR_LIST_EXTENSIONS(MAKE_EXTENSION_NAME_LIST)};
#undef MAKE_EXTENSION_NAME_LIST
        return std::find_if(publicExtensions.begin(), publicExtensions.end(), [&](const char* extensionName) {
                   return strcmp(extensionName, extension.extensionName) == 0;
               }) != publicExtensions.end();
    };

    std::vector<XrExtensionProperties> runtimePublicExtensions;
    std::vector<XrExtensionProperties> runtimeNonPublicExtensions;
    std::copy_if(extensions.begin(), extensions.end(), std::back_inserter(runtimePublicExtensions), isPublicExtension);
    std::copy_if(extensions.begin(), extensions.end(), std::back_inserter(runtimeNonPublicExtensions),
                 [isPublicExtension](const XrExtensionProperties& extension) { return !isPublicExtension(extension); });

    auto hasPrefix = [](XrExtensionProperties const& props, const char* prefix) -> bool {
        return strncmp(prefix, props.extensionName, strlen(prefix)) == 0;
    };

    LOGI("    \"extensions\": [\n");
    for (XrExtensionProperties extension : runtimePublicExtensions) {
        if (hasPrefix(extension, "XR_KHR")) {
            LOGI("        \"%s\",\n", extension.extensionName);
        }
    }
    for (XrExtensionProperties extension : runtimePublicExtensions) {
        if (hasPrefix(extension, "XR_EXT")) {
            LOGI("        \"%s\",\n", extension.extensionName);
        }
    }
    for (XrExtensionProperties extension : runtimePublicExtensions) {
        if (!hasPrefix(extension, "XR_KHR") && !hasPrefix(extension, "XR_EXT")) {
            LOGI("        \"%s\",\n", extension.extensionName);
        }
    }
    LOGI("    ],\n");

    constexpr bool writeNonPublicExtensions = false;
    if (writeNonPublicExtensions) {
        if (!runtimeNonPublicExtensions.empty()) {
            LOGI("    \"private_extensions\": [\n");
            for (XrExtensionProperties extension : runtimeNonPublicExtensions) {
                LOGI("        \"%s\",\n", extension.extensionName);
            }
            LOGI("    ],\n");
        }
    }

    LOGI("    \"form_factors\": [\n");

#define AS_LIST(name, val) {name, #name},
    static constexpr std::pair<XrFormFactor, const char*> KnownFormFactors[] = {XR_LIST_ENUM_XrFormFactor(AS_LIST)};
#undef AS_LIST

    // Iterate over known form factors and list any form factors that are supported
    for (auto formFactor : KnownFormFactors) {
        // Get the default system for the HMD form factor.
        XrSystemGetInfo systemGetInfo = {XR_TYPE_SYSTEM_GET_INFO};
        systemGetInfo.formFactor = formFactor.first;

        XrSystemId systemId;
        if (xrGetSystem(instance, &systemGetInfo, &systemId) != XR_SUCCESS) {
            continue;
        }

        LOGI("        {\n");
        LOGI("            \"form_factor\": \"%s\",\n", formFactor.second);
        LOGI("            \"view_configurations\": [\n");

        uint32_t viewConfigurationCount = 0;
        xrEnumerateViewConfigurations(instance, systemId, 0, &viewConfigurationCount, nullptr);
        std::vector<XrViewConfigurationType> viewConfigurationTypes(viewConfigurationCount);
        xrEnumerateViewConfigurations(instance, systemId, (uint32_t)viewConfigurationTypes.size(), &viewConfigurationCount,
                                      viewConfigurationTypes.data());

        for (auto const& config : viewConfigurationTypes) {
            std::string configName = viewConfigurationTypeName(config);
            LOGI("                {\n");
            LOGI("                    \"view_configuration\": \"%s\",\n", configName.c_str());

            uint32_t blendModeCount = 0;
            xrEnumerateEnvironmentBlendModes(instance, systemId, config, 0, &blendModeCount, nullptr);
            std::vector<XrEnvironmentBlendMode> blendModes(blendModeCount);
            xrEnumerateEnvironmentBlendModes(instance, systemId, config, (uint32_t)blendModes.size(), &blendModeCount,
                                             blendModes.data());

            LOGI("                    \"environment_blend_modes\": [\n");
            for (auto const& blendMode : blendModes) {
                std::string blendModeName = envBlendModeTypeName(blendMode);
                LOGI("                        \"%s\",\n", blendModeName.c_str());
            }
            LOGI("                    ]\n");

            LOGI("                }\n");
        }

        LOGI("            ]\n");
        LOGI("        }\n");
    }

    LOGI("    ]\n");
    LOGI("}\n");

    return 0;
}

#if defined(XR_USE_PLATFORM_ANDROID)

static void app_handle_cmd(struct android_app*, int32_t) {}

void android_main(struct android_app* app) {
    JNIEnv* Env;
    app->activity->vm->AttachCurrentThread(&Env, nullptr);

    app->userData = nullptr;
    app->onAppCmd = app_handle_cmd;

    PFN_xrInitializeLoaderKHR initializeLoader = nullptr;
    if (XR_SUCCEEDED(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)(&initializeLoader)))) {
        XrLoaderInitInfoAndroidKHR loaderInitInfoAndroid{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        loaderInitInfoAndroid.applicationVM = app->activity->vm;
        loaderInitInfoAndroid.applicationContext = app->activity->clazz;
        initializeLoader(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&loaderInitInfoAndroid));
    }

    g_app = app;

    main_body();

    g_app = nullptr;

    ANativeActivity_finish(app->activity);
    app->activity->vm->DetachCurrentThread();
}

#else  // !defined(XR_USE_PLATFORM_ANDROID)

int main() { return main_body(); }

#endif  // defined(XR_USE_PLATFORM_ANDROID)
