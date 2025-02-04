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
#include <map>

#include "xr_dependencies.h"
#include <openxr/openxr.h>
#include <openxr/openxr_loader_negotiation.h>

#if defined(__GNUC__) && __GNUC__ >= 4
#define LAYER_EXPORT __attribute__((visibility("default")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590)
#define LAYER_EXPORT __attribute__((visibility("default")))
#else
#define LAYER_EXPORT __declspec(dllexport)
#endif

std::map<XrInstance, PFN_xrGetInstanceProcAddr> g_next_gipa_map;

static XRAPI_ATTR XrResult XRAPI_CALL LayerTestXrCreateInstance(const XrInstanceCreateInfo * /* info */,
                                                                XrInstance * /* instance */) {
    // In a layer, LayerTestXrCreateApiLayerInstance is called instead of this function. This should not be called.
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

static XRAPI_ATTR XrResult XRAPI_CALL LayerTestXrDestroyInstance(XrInstance instance) {
    // Call down to the next xrDestroyInstance.
    PFN_xrVoidFunction nextDestroyInstance{nullptr};
    XrResult res = g_next_gipa_map[instance](instance, "xrDestroyInstance", &nextDestroyInstance);
    if (XR_SUCCEEDED(res)) {
        res = reinterpret_cast<PFN_xrDestroyInstance>(nextDestroyInstance)(instance);
    }

    if (XR_SUCCEEDED(res)) {
        g_next_gipa_map.erase(instance);
    }

    return res;
}

static XRAPI_ATTR XrResult XRAPI_CALL LayerTestXrGetInstanceProcAddr(XrInstance instance, const char *name,
                                                                     PFN_xrVoidFunction *function) {
    if (0 == strcmp(name, "xrGetInstanceProcAddr")) {
        *function = reinterpret_cast<PFN_xrVoidFunction>(LayerTestXrGetInstanceProcAddr);
    } else if (0 == strcmp(name, "xrCreateInstance")) {
        *function = reinterpret_cast<PFN_xrVoidFunction>(LayerTestXrCreateInstance);
    } else if (0 == strcmp(name, "xrDestroyInstance")) {
        *function = reinterpret_cast<PFN_xrVoidFunction>(LayerTestXrDestroyInstance);
    } else {
        *function = nullptr;
    }

    if (*function != nullptr) {
        return XR_SUCCESS;
    }

    // If the function is not intercepted in this layer, call down to the next layer.
    auto it = g_next_gipa_map.find(instance);
    if (it == std::end(g_next_gipa_map)) {
        return XR_ERROR_HANDLE_INVALID;
    }

    return it->second(instance, name, function);
}

static XRAPI_ATTR XrResult XRAPI_CALL LayerTestXrCreateApiLayerInstance(const XrInstanceCreateInfo *info,
                                                                        const XrApiLayerCreateInfo *apiLayerInfo,
                                                                        XrInstance *instance) {
    // Call down to the next layer's xrCreateApiLayerInstance.
    // Clone the XrApiLayerCreateInfo, but move to the next XrApiLayerNextInfo in the chain. nextInfo will be null
    // if the loader's terminator function is going to be called (between the layer and the runtime) but this is OK
    // because the loader's terminator function won't use it.
    XrApiLayerCreateInfo newApiLayerInfo = *apiLayerInfo;
    newApiLayerInfo.nextInfo = apiLayerInfo->nextInfo->next;

    const XrResult res = apiLayerInfo->nextInfo->nextCreateApiLayerInstance(info, &newApiLayerInfo, instance);
    if (XR_FAILED(res)) {
        return res;  // The next layer's xrCreateApiLayerInstance failed.
    }

    g_next_gipa_map[*instance] = apiLayerInfo->nextInfo->nextGetInstanceProcAddr;

    return XR_SUCCESS;
}

extern "C" {

// Function used to negotiate an interface betewen the loader and a layer.  Each library exposing one or
// more layers needs to expose at least this function.
LAYER_EXPORT XRAPI_ATTR XrResult XRAPI_CALL xrNegotiateLoaderApiLayerInterface(const XrNegotiateLoaderInfo *loaderInfo,
                                                                               const char *layerName,
                                                                               XrNegotiateApiLayerRequest *layerRequest) {
    if (nullptr == loaderInfo || nullptr == layerRequest || loaderInfo->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
        loaderInfo->structVersion != XR_LOADER_INFO_STRUCT_VERSION || loaderInfo->structSize != sizeof(XrNegotiateLoaderInfo) ||
        layerRequest->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_REQUEST ||
        layerRequest->structVersion != XR_API_LAYER_INFO_STRUCT_VERSION ||
        layerRequest->structSize != sizeof(XrNegotiateApiLayerRequest) ||
        loaderInfo->minInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->maxInterfaceVersion < XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->minApiVersion < XR_MAKE_VERSION(0, 1, 0) || loaderInfo->minApiVersion >= XR_MAKE_VERSION(1, 1, 0)) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    (void)layerName;
    layerRequest->layerInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
    layerRequest->layerApiVersion = XR_MAKE_VERSION(0, 1, 0);
    layerRequest->getInstanceProcAddr = LayerTestXrGetInstanceProcAddr;
    layerRequest->createApiLayerInstance = LayerTestXrCreateApiLayerInstance;

    return XR_SUCCESS;
}

// Always fail
LAYER_EXPORT XRAPI_ATTR XrResult XRAPI_CALL TestLayerAlwaysFailNegotiateLoaderApiLayerInterface(
    const XrNegotiateLoaderInfo * /* loaderInfo */, const char * /* layerName */, XrNegotiateApiLayerRequest * /* layerRequest */) {
    return XR_ERROR_INITIALIZATION_FAILED;
}

// Pass, but return NULL for the layer's xrGetInstanceProcAddr
LAYER_EXPORT XRAPI_ATTR XrResult XRAPI_CALL TestLayerNullGipaNegotiateLoaderApiLayerInterface(
    const XrNegotiateLoaderInfo *loaderInfo, const char *layerName, XrNegotiateApiLayerRequest *layerRequest) {
    if (nullptr == loaderInfo || nullptr == layerRequest || loaderInfo->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
        loaderInfo->structVersion != XR_LOADER_INFO_STRUCT_VERSION || loaderInfo->structSize != sizeof(XrNegotiateLoaderInfo) ||
        layerRequest->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_REQUEST ||
        layerRequest->structVersion != XR_API_LAYER_INFO_STRUCT_VERSION ||
        layerRequest->structSize != sizeof(XrNegotiateApiLayerRequest) ||
        loaderInfo->minInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->maxInterfaceVersion < XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->minApiVersion < XR_MAKE_VERSION(0, 1, 0) || loaderInfo->minApiVersion >= XR_MAKE_VERSION(1, 1, 0)) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }
    (void)layerName;
    layerRequest->layerInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
    layerRequest->layerApiVersion = XR_MAKE_VERSION(0, 1, 0);
    layerRequest->getInstanceProcAddr = nullptr;

    return XR_SUCCESS;
}

// Pass, but return invalid interface version
LAYER_EXPORT XRAPI_ATTR XrResult XRAPI_CALL TestLayerInvalidInterfaceNegotiateLoaderApiLayerInterface(
    const XrNegotiateLoaderInfo *loaderInfo, const char *layerName, XrNegotiateApiLayerRequest *layerRequest) {
    if (nullptr == loaderInfo || nullptr == layerRequest || loaderInfo->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
        loaderInfo->structVersion != XR_LOADER_INFO_STRUCT_VERSION || loaderInfo->structSize != sizeof(XrNegotiateLoaderInfo) ||
        layerRequest->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_REQUEST ||
        layerRequest->structVersion != XR_API_LAYER_INFO_STRUCT_VERSION ||
        layerRequest->structSize != sizeof(XrNegotiateApiLayerRequest) ||
        loaderInfo->minInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->maxInterfaceVersion < XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->minApiVersion < XR_MAKE_VERSION(0, 1, 0) || loaderInfo->minApiVersion >= XR_MAKE_VERSION(1, 1, 0)) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }
    (void)layerName;
    layerRequest->layerInterfaceVersion = 0;
    layerRequest->layerApiVersion = XR_MAKE_VERSION(0, 1, 0);
    layerRequest->getInstanceProcAddr = LayerTestXrGetInstanceProcAddr;

    return XR_SUCCESS;
}

// Pass, but return invalid API version
LAYER_EXPORT XRAPI_ATTR XrResult XRAPI_CALL TestLayerInvalidApiNegotiateLoaderApiLayerInterface(
    const XrNegotiateLoaderInfo *loaderInfo, const char *layerName, XrNegotiateApiLayerRequest *layerRequest) {
    if (nullptr == loaderInfo || nullptr == layerRequest || loaderInfo->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
        loaderInfo->structVersion != XR_LOADER_INFO_STRUCT_VERSION || loaderInfo->structSize != sizeof(XrNegotiateLoaderInfo) ||
        layerRequest->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_REQUEST ||
        layerRequest->structVersion != XR_API_LAYER_INFO_STRUCT_VERSION ||
        layerRequest->structSize != sizeof(XrNegotiateApiLayerRequest) ||
        loaderInfo->minInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->maxInterfaceVersion < XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->minApiVersion < XR_MAKE_VERSION(0, 1, 0) || loaderInfo->minApiVersion >= XR_MAKE_VERSION(1, 1, 0)) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }
    (void)layerName;
    layerRequest->layerInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
    layerRequest->layerApiVersion = 0;
    layerRequest->getInstanceProcAddr = LayerTestXrGetInstanceProcAddr;

    return XR_SUCCESS;
}

}  // extern "C"
