// Copyright (c) 2017-2025 The Khronos Group Inc.
// Copyright (c) 2017 Valve Corporation
// Copyright (c) 2017 LunarG, Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: Mark Young <marky@lunarg.com>
//

#include <cstring>
#include <iostream>

#include "xr_dependencies.h"
#include <openxr/openxr.h>
#include <openxr/openxr_loader_negotiation.h>

#if defined(__GNUC__) && __GNUC__ >= 4
#define RUNTIME_EXPORT __attribute__((visibility("default")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590)
#define RUNTIME_EXPORT __attribute__((visibility("default")))
#else
#define RUNTIME_EXPORT
#endif

static XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrCreateInstance(const XrInstanceCreateInfo * /* info */, XrInstance *instance) {
    *instance = (XrInstance)1;
    return XR_SUCCESS;
}

static XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrDestroyInstance(XrInstance /* instance */) { return XR_SUCCESS; }

static XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrEnumerateInstanceExtensionProperties(const char *layerName,
                                                                                        uint32_t propertyCapacityInput,
                                                                                        uint32_t *propertyCountOutput,
                                                                                        XrExtensionProperties *properties) {
    if (nullptr != layerName) {
        return XR_ERROR_API_LAYER_NOT_PRESENT;
    }
    // Return 2 fake extensions, just to test
    *propertyCountOutput = 2;
    if (0 != propertyCapacityInput) {
        strcpy(properties[0].extensionName, "XR_KHR_fake_ext1");
        properties[0].extensionVersion = 57;
        strcpy(properties[1].extensionName, "XR_KHR_fake_ext2");
        properties[1].extensionVersion = 3;
    }
    return XR_SUCCESS;
}

static XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrGetSystem(XrInstance instance, const XrSystemGetInfo * /* getInfo */,
                                                             XrSystemId *systemId) {
    if (instance == XR_NULL_HANDLE) {
        return XR_ERROR_HANDLE_INVALID;
    }
    *systemId = 1;
    return XR_SUCCESS;
}

static XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrGetSystemProperties(XrInstance instance, XrSystemId systemId,
                                                                       XrSystemProperties *properties) {
    if (instance == XR_NULL_HANDLE) {
        return XR_ERROR_HANDLE_INVALID;
    }
    if (systemId != 1) {
        return XR_ERROR_SYSTEM_INVALID;
    }
    properties->graphicsProperties.maxLayerCount = 1;
    properties->graphicsProperties.maxSwapchainImageHeight = 1;
    properties->graphicsProperties.maxSwapchainImageWidth = 1;
    properties->systemId = systemId;
    strcpy(properties->systemName, "Test system");
    properties->vendorId = 0x0;
    return XR_SUCCESS;
}

static XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrGetInstanceProcAddr(XrInstance instance, const char *name,
                                                                       PFN_xrVoidFunction *function) {
    if (0 == strcmp(name, "xrGetInstanceProcAddr")) {
        *function = reinterpret_cast<PFN_xrVoidFunction>(RuntimeTestXrGetInstanceProcAddr);
    } else if (0 == strcmp(name, "xrEnumerateInstanceExtensionProperties")) {
        *function = reinterpret_cast<PFN_xrVoidFunction>(RuntimeTestXrEnumerateInstanceExtensionProperties);
    } else if (0 == strcmp(name, "xrCreateInstance")) {
        *function = reinterpret_cast<PFN_xrVoidFunction>(RuntimeTestXrCreateInstance);
    };
    if (instance == XR_NULL_HANDLE) {
        return XR_ERROR_HANDLE_INVALID;
    }
    if (0 == strcmp(name, "xrDestroyInstance")) {
        *function = reinterpret_cast<PFN_xrVoidFunction>(RuntimeTestXrDestroyInstance);
    } else if (0 == strcmp(name, "xrGetSystem")) {
        *function = reinterpret_cast<PFN_xrVoidFunction>(RuntimeTestXrGetSystem);
    } else if (0 == strcmp(name, "xrGetSystemProperties")) {
        *function = reinterpret_cast<PFN_xrVoidFunction>(RuntimeTestXrGetSystemProperties);
    } else {
        *function = nullptr;
    }

    return *function ? XR_SUCCESS : XR_ERROR_FUNCTION_UNSUPPORTED;
}

extern "C" {

// Function used to negotiate an interface betewen the loader and a runtime.
RUNTIME_EXPORT XRAPI_ATTR XrResult XRAPI_CALL xrNegotiateLoaderRuntimeInterface(const XrNegotiateLoaderInfo *loaderInfo,
                                                                                XrNegotiateRuntimeRequest *runtimeRequest) {
    if (nullptr == loaderInfo || nullptr == runtimeRequest || loaderInfo->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
        loaderInfo->structVersion != XR_LOADER_INFO_STRUCT_VERSION || loaderInfo->structSize != sizeof(XrNegotiateLoaderInfo) ||
        runtimeRequest->structType != XR_LOADER_INTERFACE_STRUCT_RUNTIME_REQUEST ||
        runtimeRequest->structVersion != XR_RUNTIME_INFO_STRUCT_VERSION ||
        runtimeRequest->structSize != sizeof(XrNegotiateRuntimeRequest) ||
        loaderInfo->minInterfaceVersion > XR_CURRENT_LOADER_RUNTIME_VERSION ||
        loaderInfo->maxInterfaceVersion < XR_CURRENT_LOADER_RUNTIME_VERSION ||
        loaderInfo->minApiVersion < XR_MAKE_VERSION(0, 1, 0) || loaderInfo->minApiVersion >= XR_MAKE_VERSION(1, 1, 0)) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    runtimeRequest->runtimeInterfaceVersion = XR_CURRENT_LOADER_RUNTIME_VERSION;
    runtimeRequest->runtimeApiVersion = XR_CURRENT_API_VERSION;
    runtimeRequest->getInstanceProcAddr = RuntimeTestXrGetInstanceProcAddr;

    return XR_SUCCESS;
}

// Always fail
RUNTIME_EXPORT XRAPI_ATTR XrResult XRAPI_CALL TestRuntimeAlwaysFailNegotiateLoaderRuntimeInterface(
    const XrNegotiateLoaderInfo * /* loaderInfo */, XrNegotiateRuntimeRequest * /* runtimeRequest */) {
    return XR_ERROR_INITIALIZATION_FAILED;
}

// Pass, but return NULL for the runtime's xrGetInstanceProcAddr
RUNTIME_EXPORT XRAPI_ATTR XrResult XRAPI_CALL TestRuntimeNullGipaNegotiateLoaderRuntimeInterface(
    const XrNegotiateLoaderInfo *loaderInfo, XrNegotiateRuntimeRequest *runtimeRequest) {
    auto result = xrNegotiateLoaderRuntimeInterface(loaderInfo, runtimeRequest);
    if (result == XR_SUCCESS) {
        runtimeRequest->getInstanceProcAddr = nullptr;
    }

    return result;
}

// Pass, but return invalid interface version
RUNTIME_EXPORT XRAPI_ATTR XrResult XRAPI_CALL TestRuntimeInvalidInterfaceNegotiateLoaderRuntimeInterface(
    const XrNegotiateLoaderInfo *loaderInfo, XrNegotiateRuntimeRequest *runtimeRequest) {
    auto result = xrNegotiateLoaderRuntimeInterface(loaderInfo, runtimeRequest);
    if (result == XR_SUCCESS) {
        runtimeRequest->runtimeInterfaceVersion = 0;
    }

    return result;
}

// Pass, but return invalid API version
RUNTIME_EXPORT XRAPI_ATTR XrResult XRAPI_CALL TestRuntimeInvalidApiNegotiateLoaderRuntimeInterface(
    const XrNegotiateLoaderInfo *loaderInfo, XrNegotiateRuntimeRequest *runtimeRequest) {
    auto result = xrNegotiateLoaderRuntimeInterface(loaderInfo, runtimeRequest);
    if (result == XR_SUCCESS) {
        runtimeRequest->runtimeApiVersion = 0;
    }

    return result;
}

}  // extern "C"
