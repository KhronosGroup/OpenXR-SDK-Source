// Copyright (c) 2017-2019 Collabora, Ltd.
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
// Author: Jakob Bornecrantz <jakob.bornecrantz@collabora.com>
//

// We do not need any graphics.

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif  // defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)

#include "xr_dependencies.h"
#include <openxr/openxr.h>

#include <cstdlib>
#include <cstdio>
#include <string.h>
#include <inttypes.h>
#include <vector>

#ifndef PRIx32
#define PRIx32 "x"
#endif
#ifndef PRIx64
#define PRIx64 "x"
#endif

// Struct that does book keeping of what a
// OpenXR application need to keep track of.
struct Program {
   public:
    XrInstance instance;

   public:
    Program() : instance(XR_NULL_HANDLE) {}
    ~Program() {
        if (instance != XR_NULL_HANDLE) {
            xrDestroyInstance(instance);
            instance = XR_NULL_HANDLE;
        }
    }

    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;
    Program(Program&&) = delete;
    Program& operator=(Program&&) = delete;
};

// This below function is written in as close to "C style" as
// possible, so users of C (and other languages) are not too
// bogged down with C++ism. The cleanup code is hidden in
// the struct so the example is lighter but still correct.
int main() {
    Program program = {};

    // Start with creating a instance.
    XrInstanceCreateInfo instanceCreateInfo = {XR_TYPE_INSTANCE_CREATE_INFO};
    strncpy(instanceCreateInfo.applicationInfo.applicationName, "List", XR_MAX_APPLICATION_NAME_SIZE);
    instanceCreateInfo.applicationInfo.applicationVersion = 1;
    strncpy(instanceCreateInfo.applicationInfo.engineName, "List Engine", XR_MAX_ENGINE_NAME_SIZE);
    instanceCreateInfo.applicationInfo.engineVersion = 1;
    // Current version is 1.1.x, but this app only requires 1.0.x
    instanceCreateInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0;

    if (xrCreateInstance(&instanceCreateInfo, &program.instance) != XR_SUCCESS) {
        fprintf(stderr, "Failed to create XR instance.\n");
        return 1;
    }

    // Get the default system for the HMD form factor.
    XrSystemGetInfo systemGetInfo = {XR_TYPE_SYSTEM_GET_INFO};
    systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    XrSystemId systemId;
    if (xrGetSystem(program.instance, &systemGetInfo, &systemId) != XR_SUCCESS) {
        fprintf(stderr, "Failed to get system for HMD form factor.\n");
        return 1;
    }

    // Print information about the system.
    XrSystemProperties systemProperties = {XR_TYPE_SYSTEM_PROPERTIES};
    xrGetSystemProperties(program.instance, systemId, &systemProperties);

    printf("Evaluating system\n");
    printf("\t           name: '%s'\n", systemProperties.systemName);
    printf("\t       vendorId: 0x%" PRIx32 "\n", systemProperties.vendorId);
    printf("\t       systemId: 0x%" PRIx64 "\n", systemProperties.systemId);
    printf("\t     systemName: %s\n", systemProperties.systemName);

    uint32_t size;
    xrEnumerateInstanceExtensionProperties(nullptr, 0, &size, nullptr);
    std::vector<XrExtensionProperties> extensions(size, {XR_TYPE_EXTENSION_PROPERTIES});
    xrEnumerateInstanceExtensionProperties(nullptr, size, &size, extensions.data());

    printf("List instance extensions\n");
    for (XrExtensionProperties extension : extensions) {
        printf("\t%s %d\n", extension.extensionName, extension.extensionVersion);
    }

    xrEnumerateApiLayerProperties(0, &size, nullptr);
    std::vector<XrApiLayerProperties> apiLayerProperties(size, {XR_TYPE_API_LAYER_PROPERTIES});
    xrEnumerateApiLayerProperties(size, &size, apiLayerProperties.data());

    printf("List API layer properties\n");
    for (XrApiLayerProperties apiLayerProperty : apiLayerProperties) {
        printf("\t%s %u.%u.%u %u %s\n", apiLayerProperty.layerName, XR_VERSION_MAJOR(apiLayerProperty.specVersion),
               XR_VERSION_MINOR(apiLayerProperty.specVersion), XR_VERSION_PATCH(apiLayerProperty.specVersion),
               apiLayerProperty.layerVersion, apiLayerProperty.description);
    }

    // The program struct will do cleanup for us.
    return 0;
}
