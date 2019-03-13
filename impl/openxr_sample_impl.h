// Copyright (c) 2017 The Khronos Group Inc.
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
// Author:
//

#ifndef SRC_IMPL_OPENXR_SAMPLE_IMPL_H_
#define SRC_IMPL_OPENXR_SAMPLE_IMPL_H_

#include "xr_dependencies.h"
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <string.h>
#include <string>
#include <vector>
#include "impl/si_display_timing.h"

#ifdef _WIN32
#define strcpy strcpy_s
#define strdup _strdup
#endif

struct SystemGraphics {
    SystemGraphics() {
        memset(this, 0, sizeof(*this));
        props.maxSwapchainImageHeight = 4096;
        props.maxSwapchainImageWidth = 4096;
        props.maxViewCount = 2;
    }
    XrSystemGraphicsProperties props;
};

struct SystemTracking {
    SystemTracking() {
        memset(this, 0, sizeof(*this));
        props.orientationTracking = XR_TRUE;
        props.positionTracking = XR_FALSE;
    }
    XrSystemTrackingProperties props;
};

// Maps to XrSystemId
struct System;

// Maps to XrInstance
struct Instance {
    Instance() { _unique_id = 0xF00BAD42; }

    bool IsValid() { return _unique_id == 0xF00BAD42; }
    XrPath StringToPath(const std::string& str);

    uint32_t _unique_id;  // 0xF00BAD42 - for debugging

    XrInstanceCreateInfo* create_info;

    std::vector<System> systems;
    std::vector<std::string> atom_table;
};

struct Swapchain;
typedef void(XRAPI_PTR* PFN_SetSwapchainProcs)(Swapchain* swch);

struct Session;
typedef void(XRAPI_PTR* PFN_SetSessionProcs)(Session* sess);

struct Space;

// Maps to XrSystemId, which is static data about a system.
struct System {
    XrInstance instance;
    XrFormFactor formFactor;

    SystemGraphics graphics;
    SystemTracking tracking;

    PFN_xrEnumerateViewConfigurations enumerate_view_configurations;
    PFN_xrGetViewConfigurationProperties get_view_configuration_properties;
    PFN_xrEnumerateViewConfigurationViews enumerate_view_configuration_views;
};

// Maps to XrSession
struct Session {
    Instance* instance;
    System* system;
    XrSessionCreateInfo* create_info;

    std::vector<XrReferenceSpaceType> spaces;
    // TODO: should contain a list of child spaces
    Space* space;

    float interpupillaryDistance;
    XrPath viewConfiguration;

    void* implementation_data;

    PFN_xrEnumerateSwapchainFormats enumerate_swapchain_formats;
    PFN_xrCreateSession create_session;
    PFN_xrDestroySession destroy_session;
    PFN_xrCreateSwapchain create_swapchain;
    PFN_xrDestroySwapchain destroy_swapchain;

    PFN_xrBeginSession begin_session;
    PFN_xrEndSession end_session;

    XrSessionBeginInfo* begin_info;
    DisplayTiming display_timing;

    bool begin_frame_allowed;

    PFN_xrWaitFrame wait_frame;
    PFN_xrBeginFrame begin_frame;
    PFN_xrEndFrame end_frame;

    PFN_SetSwapchainProcs set_swapchain_procs;
    // TODO: should contain a list of child swapchains
    // TODO: should contain a list of child action sets
};

// Maps to XrSpace
struct Space {
    XrSession session;
    XrReferenceSpaceCreateInfo* create_info;
};

// Maps to XrSwapchain
struct Swapchain {
    Session* session;
    XrSwapchainCreateInfo* create_info;

    void* implementation_data;

    PFN_xrEnumerateSwapchainImages get_swapchain_images;
    PFN_xrAcquireSwapchainImage acquire_swapchain_image;
    PFN_xrWaitSwapchainImage wait_swapchain_image;
    PFN_xrReleaseSwapchainImage release_swapchain_image;
};

void XRAPI_CALL XrGlSetSessionProcs(Session* sess);

void XRAPI_CALL XrGlSetSystemProcs(System* sys);

#endif  // SRC_IMPL_OPENXR_SAMPLE_IMPL_H_
