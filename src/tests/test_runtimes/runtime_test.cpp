// Copyright (c) 2017-2025 The Khronos Group Inc.
// Copyright (c) 2017 Valve Corporation
// Copyright (c) 2017 LunarG, Inc.
// Copyright (c) Meta Platforms, LLC and its affiliates. All rights reserved.
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
// Author(s): Mark Young <marky@lunarg.com>, John Kearney <johnkearney@meta.com>
//

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <math.h>
#include <vector>

#include "xr_dependencies.h"
#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>
#include <openxr/openxr_loader_negotiation.h>

#include "common/xr_linear.h"

#if defined(__GNUC__) && __GNUC__ >= 4
#define RUNTIME_EXPORT __attribute__((visibility("default")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590)
#define RUNTIME_EXPORT __attribute__((visibility("default")))
#else
#define RUNTIME_EXPORT
#endif

namespace {

constexpr XrSystemId VALID_SYSTEM_ID = 1;

struct XrSpace_T {
    enum class SpaceType {
        Unknown,
        ReferenceSpace,
        ActionSpace,
    };

    // parent
    XrSession session{XR_NULL_HANDLE};

    SpaceType spaceType{SpaceType::Unknown};

    // reference space
    XrReferenceSpaceType referenceSpaceType{XR_REFERENCE_SPACE_TYPE_MAX_ENUM};
    XrPosef poseInReferenceSpace{};

    ~XrSpace_T();
};

struct XrSession_T {
    // parent
    XrInstance instance{XR_NULL_HANDLE};

    // session state
    XrSessionState sessionState{XR_SESSION_STATE_UNKNOWN};
    XrTime beginTime = 0;
    XrTime endTime = 0;

    // spaces
    std::mutex spacesMutex;
    std::vector<std::unique_ptr<XrSpace_T>> spaces;

    // attached action sets
    bool actionsetsAttached = false;
    std::vector<XrActionSet> attachedActionSets;

    ~XrSession_T();
};

struct XrAction_T {
    // parent
    XrActionSet actionSet{XR_NULL_HANDLE};

    std::string actionName;
    std::string localizedActionName;
    XrActionType actionType{XR_ACTION_TYPE_MAX_ENUM};
    std::vector<XrPath> subactionPaths;
};

struct XrActionSet_T {
    // parent
    XrInstance instance{XR_NULL_HANDLE};

    std::string actionSetName;
    std::string localizedActionSetName;

    // attached
    bool hasBeenAttached = false;

    // actions
    std::mutex actionsMutex;
    std::vector<std::unique_ptr<XrAction_T>> actions;
};

struct XrInstance_T {
    // no parent

    XrVersion apiVersion{XR_MAKE_VERSION(0, 0, 0)};
    std::vector<std::string> enabledExtensions;

    // events
    // (note: order in class: events needs to be destructed after all other types)
    std::mutex instanceEventsMutex;
    std::deque<std::pair<uint64_t, XrEventDataBuffer>> instanceEvents;

    // sessions
    std::mutex sessionsMutex;
    std::vector<std::unique_ptr<XrSession_T>> sessions;

    // action sets
    std::mutex actionSetsMutex;
    std::vector<std::unique_ptr<XrActionSet_T>> actionSets;

    // paths
    std::mutex pathsMutex;
    std::map<XrPath, std::string> paths;
    XrPath lastPath = 1;
};

struct GlobalImpl {
    // no parent

    std::mutex instancesMutex;
    std::vector<std::unique_ptr<XrInstance_T>> instances;

    std::chrono::time_point<std::chrono::steady_clock> initialTime;
};

GlobalImpl g;

const std::vector<const char*> cSimpleControllerIPData_1_0{
    "/user/hand/left/input/select/click", "/user/hand/left/input/menu/click", "/user/hand/left/input/grip/pose",
    "/user/hand/left/input/aim/pose",     "/user/hand/left/output/haptic",    "/user/hand/right/input/select/click",
    "/user/hand/right/input/menu/click",  "/user/hand/right/input/grip/pose", "/user/hand/right/input/aim/pose",
    "/user/hand/right/output/haptic",
};

const std::vector<const char*> cSimpleControllerIPData_1_1{
    "/user/hand/left/input/select/click",       "/user/hand/left/input/menu/click",  "/user/hand/left/input/grip/pose",
    "/user/hand/left/input/grip_surface/pose",  "/user/hand/left/input/aim/pose",    "/user/hand/left/output/haptic",
    "/user/hand/right/input/select/click",      "/user/hand/right/input/menu/click", "/user/hand/right/input/grip/pose",
    "/user/hand/right/input/grip_surface/pose", "/user/hand/right/input/aim/pose",   "/user/hand/right/output/haptic",
};

const std::vector<const char*> cGoogleDaydreamControllerIPData_1_0{
    "/user/hand/left/input/select/click",    "/user/hand/left/input/trackpad/x",     "/user/hand/left/input/trackpad/y",
    "/user/hand/left/input/trackpad/click",  "/user/hand/left/input/trackpad/touch", "/user/hand/left/input/trackpad",
    "/user/hand/left/input/grip/pose",       "/user/hand/left/input/aim/pose",       "/user/hand/right/input/select/click",
    "/user/hand/right/input/trackpad/x",     "/user/hand/right/input/trackpad/y",    "/user/hand/right/input/trackpad/click",
    "/user/hand/right/input/trackpad/touch", "/user/hand/right/input/trackpad",      "/user/hand/right/input/grip/pose",
    "/user/hand/right/input/aim/pose",
};

const std::vector<const char*> cGoogleDaydreamControllerIPData_1_1{
    "/user/hand/left/input/select/click",    "/user/hand/left/input/trackpad/x",         "/user/hand/left/input/trackpad/y",
    "/user/hand/left/input/trackpad/click",  "/user/hand/left/input/trackpad/touch",     "/user/hand/left/input/trackpad",
    "/user/hand/left/input/grip/pose",       "/user/hand/left/input/grip_surface/pose",  "/user/hand/left/input/aim/pose",
    "/user/hand/right/input/select/click",   "/user/hand/right/input/trackpad/x",        "/user/hand/right/input/trackpad/y",
    "/user/hand/right/input/trackpad/click", "/user/hand/right/input/trackpad/touch",    "/user/hand/right/input/trackpad",
    "/user/hand/right/input/grip/pose",      "/user/hand/right/input/grip_surface/pose", "/user/hand/right/input/aim/pose",
};

const std::vector<const char*> cViveControllerIPData_1_0{
    "/user/hand/left/input/system/click",    "/user/hand/left/input/menu/click",     "/user/hand/left/input/squeeze/click",
    "/user/hand/left/input/trigger/value",   "/user/hand/left/input/trigger/click",  "/user/hand/left/input/trackpad/x",
    "/user/hand/left/input/trackpad/y",      "/user/hand/left/input/trackpad/click", "/user/hand/left/input/trackpad/touch",
    "/user/hand/left/input/trackpad",        "/user/hand/left/input/grip/pose",      "/user/hand/left/input/aim/pose",
    "/user/hand/left/output/haptic",         "/user/hand/right/input/system/click",  "/user/hand/right/input/menu/click",
    "/user/hand/right/input/squeeze/click",  "/user/hand/right/input/trigger/value", "/user/hand/right/input/trigger/click",
    "/user/hand/right/input/trackpad/x",     "/user/hand/right/input/trackpad/y",    "/user/hand/right/input/trackpad/click",
    "/user/hand/right/input/trackpad/touch", "/user/hand/right/input/trackpad",      "/user/hand/right/input/grip/pose",
    "/user/hand/right/input/aim/pose",       "/user/hand/right/output/haptic",
};

const std::vector<const char*> cViveControllerIPData_1_1{
    "/user/hand/left/input/system/click",    "/user/hand/left/input/menu/click",
    "/user/hand/left/input/squeeze/click",   "/user/hand/left/input/trigger/value",
    "/user/hand/left/input/trigger/click",   "/user/hand/left/input/trackpad/x",
    "/user/hand/left/input/trackpad/y",      "/user/hand/left/input/trackpad/click",
    "/user/hand/left/input/trackpad/touch",  "/user/hand/left/input/trackpad",
    "/user/hand/left/input/grip/pose",       "/user/hand/left/input/grip_surface/pose",
    "/user/hand/left/input/aim/pose",        "/user/hand/left/output/haptic",
    "/user/hand/right/input/system/click",   "/user/hand/right/input/menu/click",
    "/user/hand/right/input/squeeze/click",  "/user/hand/right/input/trigger/value",
    "/user/hand/right/input/trigger/click",  "/user/hand/right/input/trackpad/x",
    "/user/hand/right/input/trackpad/y",     "/user/hand/right/input/trackpad/click",
    "/user/hand/right/input/trackpad/touch", "/user/hand/right/input/trackpad",
    "/user/hand/right/input/grip/pose",      "/user/hand/right/input/grip_surface/pose",
    "/user/hand/right/input/aim/pose",       "/user/hand/right/output/haptic",
};

const std::vector<const char*> cViveProIPData{
    "/user/head/input/system/click",
    "/user/head/input/volume_up/click",
    "/user/head/input/volume_down/click",
    "/user/head/input/mute_mic/click",
};

const std::vector<const char*> cWMRControllerIPData_1_0{
    "/user/hand/left/input/menu/click",    "/user/hand/left/input/squeeze/click",   "/user/hand/left/input/trigger/value",
    "/user/hand/left/input/thumbstick/x",  "/user/hand/left/input/thumbstick/y",    "/user/hand/left/input/thumbstick/click",
    "/user/hand/left/input/thumbstick",    "/user/hand/left/input/trackpad/x",      "/user/hand/left/input/trackpad/y",
    "/user/hand/left/input/trackpad",      "/user/hand/left/input/trackpad/click",  "/user/hand/left/input/trackpad/touch",
    "/user/hand/left/input/grip/pose",     "/user/hand/left/input/aim/pose",        "/user/hand/left/output/haptic",
    "/user/hand/right/input/menu/click",   "/user/hand/right/input/squeeze/click",  "/user/hand/right/input/trigger/value",
    "/user/hand/right/input/thumbstick/x", "/user/hand/right/input/thumbstick/y",   "/user/hand/right/input/thumbstick/click",
    "/user/hand/right/input/thumbstick",   "/user/hand/right/input/trackpad/x",     "/user/hand/right/input/trackpad/y",
    "/user/hand/right/input/trackpad",     "/user/hand/right/input/trackpad/click", "/user/hand/right/input/trackpad/touch",
    "/user/hand/right/input/grip/pose",    "/user/hand/right/input/aim/pose",       "/user/hand/right/output/haptic",
};

const std::vector<const char*> cWMRControllerIPData_1_1{
    "/user/hand/left/input/menu/click",      "/user/hand/left/input/squeeze/click",
    "/user/hand/left/input/trigger/value",   "/user/hand/left/input/thumbstick/x",
    "/user/hand/left/input/thumbstick/y",    "/user/hand/left/input/thumbstick/click",
    "/user/hand/left/input/thumbstick",      "/user/hand/left/input/trackpad/x",
    "/user/hand/left/input/trackpad/y",      "/user/hand/left/input/trackpad",
    "/user/hand/left/input/trackpad/click",  "/user/hand/left/input/trackpad/touch",
    "/user/hand/left/input/grip/pose",       "/user/hand/left/input/grip_surface/pose",
    "/user/hand/left/input/aim/pose",        "/user/hand/left/output/haptic",
    "/user/hand/right/input/menu/click",     "/user/hand/right/input/squeeze/click",
    "/user/hand/right/input/trigger/value",  "/user/hand/right/input/thumbstick/x",
    "/user/hand/right/input/thumbstick/y",   "/user/hand/right/input/thumbstick/click",
    "/user/hand/right/input/thumbstick",     "/user/hand/right/input/trackpad/x",
    "/user/hand/right/input/trackpad/y",     "/user/hand/right/input/trackpad",
    "/user/hand/right/input/trackpad/click", "/user/hand/right/input/trackpad/touch",
    "/user/hand/right/input/grip/pose",      "/user/hand/right/input/grip_surface/pose",
    "/user/hand/right/input/aim/pose",       "/user/hand/right/output/haptic",
};

const std::vector<const char*> cGamepadIPData{
    "/user/gamepad/input/menu/click",
    "/user/gamepad/input/view/click",
    "/user/gamepad/input/a/click",
    "/user/gamepad/input/b/click",
    "/user/gamepad/input/x/click",
    "/user/gamepad/input/y/click",
    "/user/gamepad/input/dpad_down/click",
    "/user/gamepad/input/dpad_right/click",
    "/user/gamepad/input/dpad_up/click",
    "/user/gamepad/input/dpad_left/click",
    "/user/gamepad/input/shoulder_left/click",
    "/user/gamepad/input/shoulder_right/click",
    "/user/gamepad/input/thumbstick_left/click",
    "/user/gamepad/input/thumbstick_right/click",
    "/user/gamepad/input/trigger_left/value",
    "/user/gamepad/input/trigger_right/value",
    "/user/gamepad/input/thumbstick_left/x",
    "/user/gamepad/input/thumbstick_left/y",
    "/user/gamepad/input/thumbstick_left",
    "/user/gamepad/input/thumbstick_right/x",
    "/user/gamepad/input/thumbstick_right/y",
    "/user/gamepad/input/thumbstick_right",
    "/user/gamepad/output/haptic_left",
    "/user/gamepad/output/haptic_right",
    "/user/gamepad/output/haptic_left_trigger",
    "/user/gamepad/output/haptic_right_trigger",
};

const std::vector<const char*> cOculusGoIPData_1_0{
    "/user/hand/left/input/system/click",   "/user/hand/left/input/trigger/click",   "/user/hand/left/input/back/click",
    "/user/hand/left/input/trackpad/x",     "/user/hand/left/input/trackpad/y",      "/user/hand/left/input/trackpad",
    "/user/hand/left/input/trackpad/click", "/user/hand/left/input/trackpad/touch",  "/user/hand/left/input/grip/pose",
    "/user/hand/left/input/aim/pose",       "/user/hand/right/input/system/click",   "/user/hand/right/input/trigger/click",
    "/user/hand/right/input/back/click",    "/user/hand/right/input/trackpad/x",     "/user/hand/right/input/trackpad/y",
    "/user/hand/right/input/trackpad",      "/user/hand/right/input/trackpad/click", "/user/hand/right/input/trackpad/touch",
    "/user/hand/right/input/grip/pose",     "/user/hand/right/input/aim/pose",
};

const std::vector<const char*> cOculusGoIPData_1_1{
    "/user/hand/left/input/system/click",      "/user/hand/left/input/trigger/click",  "/user/hand/left/input/back/click",
    "/user/hand/left/input/trackpad/x",        "/user/hand/left/input/trackpad/y",     "/user/hand/left/input/trackpad",
    "/user/hand/left/input/trackpad/click",    "/user/hand/left/input/trackpad/touch", "/user/hand/left/input/grip/pose",
    "/user/hand/left/input/grip_surface/pose", "/user/hand/left/input/aim/pose",       "/user/hand/right/input/system/click",
    "/user/hand/right/input/trigger/click",    "/user/hand/right/input/back/click",    "/user/hand/right/input/trackpad/x",
    "/user/hand/right/input/trackpad/y",       "/user/hand/right/input/trackpad",      "/user/hand/right/input/trackpad/click",
    "/user/hand/right/input/trackpad/touch",   "/user/hand/right/input/grip/pose",     "/user/hand/right/input/grip_surface/pose",
    "/user/hand/right/input/aim/pose",
};

const std::vector<const char*> cOculusTouchIPData_1_0{
    "/user/hand/left/input/x/click",           "/user/hand/left/input/x/touch",           "/user/hand/left/input/y/click",
    "/user/hand/left/input/y/touch",           "/user/hand/left/input/menu/click",        "/user/hand/left/input/squeeze/value",
    "/user/hand/left/input/trigger/value",     "/user/hand/left/input/trigger/touch",     "/user/hand/left/input/thumbstick/x",
    "/user/hand/left/input/thumbstick/y",      "/user/hand/left/input/thumbstick/click",  "/user/hand/left/input/thumbstick/touch",
    "/user/hand/left/input/thumbstick",        "/user/hand/left/input/thumbrest/touch",   "/user/hand/left/input/grip/pose",
    "/user/hand/left/input/aim/pose",          "/user/hand/left/output/haptic",           "/user/hand/right/input/a/click",
    "/user/hand/right/input/a/touch",          "/user/hand/right/input/b/click",          "/user/hand/right/input/b/touch",
    "/user/hand/right/input/system/click",     "/user/hand/right/input/squeeze/value",    "/user/hand/right/input/trigger/value",
    "/user/hand/right/input/trigger/touch",    "/user/hand/right/input/thumbstick/x",     "/user/hand/right/input/thumbstick/y",
    "/user/hand/right/input/thumbstick/click", "/user/hand/right/input/thumbstick/touch", "/user/hand/right/input/thumbstick",
    "/user/hand/right/input/thumbrest/touch",  "/user/hand/right/input/grip/pose",        "/user/hand/right/input/aim/pose",
    "/user/hand/right/output/haptic",
};

const std::vector<const char*> cOculusTouchIPData_1_0_khr_maint1{
    "/user/hand/left/input/x/click",
    "/user/hand/left/input/x/touch",
    "/user/hand/left/input/y/click",
    "/user/hand/left/input/y/touch",
    "/user/hand/left/input/menu/click",
    "/user/hand/left/input/squeeze/value",
    "/user/hand/left/input/trigger/value",
    "/user/hand/left/input/trigger/touch",
    // https://gitlab.khronos.org/openxr/openxr/-/issues/2599
    // "/user/hand/left/input/trigger/proximity",
    "/user/hand/left/input/thumbstick/x",
    "/user/hand/left/input/thumbstick/y",
    "/user/hand/left/input/thumbstick/click",
    "/user/hand/left/input/thumbstick/touch",
    "/user/hand/left/input/thumbstick",
    "/user/hand/left/input/thumbrest/touch",
    // https://gitlab.khronos.org/openxr/openxr/-/issues/2599
    // "/user/hand/left/input/thumb_resting_surfaces/proximity",
    "/user/hand/left/input/grip/pose",
    "/user/hand/left/input/grip_surface/pose",
    "/user/hand/left/input/aim/pose",
    "/user/hand/left/output/haptic",
    "/user/hand/right/input/a/click",
    "/user/hand/right/input/a/touch",
    "/user/hand/right/input/b/click",
    "/user/hand/right/input/b/touch",
    "/user/hand/right/input/system/click",
    "/user/hand/right/input/squeeze/value",
    "/user/hand/right/input/trigger/value",
    "/user/hand/right/input/trigger/touch",
    // https://gitlab.khronos.org/openxr/openxr/-/issues/2599
    // "/user/hand/right/input/trigger/proximity",
    "/user/hand/right/input/thumbstick/x",
    "/user/hand/right/input/thumbstick/y",
    "/user/hand/right/input/thumbstick/click",
    "/user/hand/right/input/thumbstick/touch",
    "/user/hand/right/input/thumbstick",
    "/user/hand/right/input/thumbrest/touch",
    // https://gitlab.khronos.org/openxr/openxr/-/issues/2599
    // "/user/hand/right/input/thumb_resting_surfaces/proximity",
    "/user/hand/right/input/grip/pose",
    "/user/hand/right/input/grip_surface/pose",
    "/user/hand/right/input/aim/pose",
    "/user/hand/right/output/haptic",
};

const std::vector<const char*> cOculusTouchIPData_1_1{
    "/user/hand/left/input/x/click",
    "/user/hand/left/input/x/touch",
    "/user/hand/left/input/y/click",
    "/user/hand/left/input/y/touch",
    "/user/hand/left/input/menu/click",
    "/user/hand/left/input/squeeze/value",
    "/user/hand/left/input/trigger/value",
    "/user/hand/left/input/trigger/touch",
    "/user/hand/left/input/trigger/proximity",
    "/user/hand/left/input/thumbstick/x",
    "/user/hand/left/input/thumbstick/y",
    "/user/hand/left/input/thumbstick/click",
    "/user/hand/left/input/thumbstick/touch",
    "/user/hand/left/input/thumbstick",
    "/user/hand/left/input/thumbrest/touch",
    "/user/hand/left/input/thumb_resting_surfaces/proximity",
    "/user/hand/left/input/grip/pose",
    "/user/hand/left/input/grip_surface/pose",
    "/user/hand/left/input/aim/pose",
    "/user/hand/left/output/haptic",
    "/user/hand/right/input/a/click",
    "/user/hand/right/input/a/touch",
    "/user/hand/right/input/b/click",
    "/user/hand/right/input/b/touch",
    "/user/hand/right/input/system/click",
    "/user/hand/right/input/squeeze/value",
    "/user/hand/right/input/trigger/value",
    "/user/hand/right/input/trigger/touch",
    "/user/hand/right/input/trigger/proximity",
    "/user/hand/right/input/thumbstick/x",
    "/user/hand/right/input/thumbstick/y",
    "/user/hand/right/input/thumbstick/click",
    "/user/hand/right/input/thumbstick/touch",
    "/user/hand/right/input/thumbstick",
    "/user/hand/right/input/thumbrest/touch",
    "/user/hand/right/input/thumb_resting_surfaces/proximity",
    "/user/hand/right/input/grip/pose",
    "/user/hand/right/input/grip_surface/pose",
    "/user/hand/right/input/aim/pose",
    "/user/hand/right/output/haptic",
};

const std::vector<const char*> cMetaTouchPlusIPData_1_1{
    "/user/hand/left/input/x/click",
    "/user/hand/left/input/x/touch",
    "/user/hand/left/input/y/click",
    "/user/hand/left/input/y/touch",
    "/user/hand/left/input/menu/click",
    "/user/hand/left/input/squeeze/value",
    "/user/hand/left/input/trigger/value",
    "/user/hand/left/input/trigger/force",
    "/user/hand/left/input/trigger/touch",
    "/user/hand/left/input/trigger/proximity",
    "/user/hand/left/input/trigger_slide/value",
    "/user/hand/left/input/trigger_curl/value",
    "/user/hand/left/input/thumbstick/x",
    "/user/hand/left/input/thumbstick/y",
    "/user/hand/left/input/thumbstick/click",
    "/user/hand/left/input/thumbstick/touch",
    "/user/hand/left/input/thumbstick",
    "/user/hand/left/input/thumbrest/touch",
    "/user/hand/left/input/thumb_resting_surfaces/proximity",
    "/user/hand/left/input/grip/pose",
    "/user/hand/left/input/grip_surface/pose",
    "/user/hand/left/input/aim/pose",
    "/user/hand/left/output/haptic",
    "/user/hand/right/input/a/click",
    "/user/hand/right/input/a/touch",
    "/user/hand/right/input/b/click",
    "/user/hand/right/input/b/touch",
    "/user/hand/right/input/system/click",
    "/user/hand/right/input/squeeze/value",
    "/user/hand/right/input/trigger/force",
    "/user/hand/right/input/trigger/value",
    "/user/hand/right/input/trigger/touch",
    "/user/hand/right/input/trigger/proximity",
    "/user/hand/right/input/trigger_slide/value",
    "/user/hand/right/input/trigger_curl/value",
    "/user/hand/right/input/thumbstick/x",
    "/user/hand/right/input/thumbstick/y",
    "/user/hand/right/input/thumbstick/click",
    "/user/hand/right/input/thumbstick/touch",
    "/user/hand/right/input/thumbstick",
    "/user/hand/right/input/thumbrest/touch",
    "/user/hand/right/input/thumb_resting_surfaces/proximity",
    "/user/hand/right/input/grip/pose",
    "/user/hand/right/input/grip_surface/pose",
    "/user/hand/right/input/aim/pose",
    "/user/hand/right/output/haptic",
};

const std::vector<const char*> cMetaTouchProIPData_1_1{
    "/user/hand/left/input/x/click",
    "/user/hand/left/input/x/touch",
    "/user/hand/left/input/y/click",
    "/user/hand/left/input/y/touch",
    "/user/hand/left/input/menu/click",
    "/user/hand/left/input/squeeze/value",
    "/user/hand/left/input/stylus/force",
    "/user/hand/left/input/trigger/value",
    "/user/hand/left/input/trigger/touch",
    "/user/hand/left/input/trigger/proximity",
    "/user/hand/left/input/trigger_curl/value",
    "/user/hand/left/input/trigger_slide/value",
    "/user/hand/left/input/thumbstick/x",
    "/user/hand/left/input/thumbstick/y",
    "/user/hand/left/input/thumbstick/click",
    "/user/hand/left/input/thumbstick/touch",
    "/user/hand/left/input/thumbstick",
    "/user/hand/left/input/thumbrest/force",
    "/user/hand/left/input/thumbrest/touch",
    "/user/hand/left/input/thumb_resting_surfaces/proximity",
    "/user/hand/left/input/grip/pose",
    "/user/hand/left/input/grip_surface/pose",
    "/user/hand/left/input/aim/pose",
    "/user/hand/left/output/haptic",
    "/user/hand/left/output/haptic_thumb",
    "/user/hand/left/output/haptic_trigger",
    "/user/hand/right/input/a/click",
    "/user/hand/right/input/a/touch",
    "/user/hand/right/input/b/click",
    "/user/hand/right/input/b/touch",
    "/user/hand/right/input/system/click",
    "/user/hand/right/input/squeeze/value",
    "/user/hand/right/input/stylus/force",
    "/user/hand/right/input/trigger/value",
    "/user/hand/right/input/trigger/touch",
    "/user/hand/right/input/trigger/proximity",
    "/user/hand/right/input/trigger_curl/value",
    "/user/hand/right/input/trigger_slide/value",
    "/user/hand/right/input/thumbstick/x",
    "/user/hand/right/input/thumbstick/y",
    "/user/hand/right/input/thumbstick/click",
    "/user/hand/right/input/thumbstick/touch",
    "/user/hand/right/input/thumbstick",
    "/user/hand/right/input/thumbrest/force",
    "/user/hand/right/input/thumbrest/touch",
    "/user/hand/right/input/thumb_resting_surfaces/proximity",
    "/user/hand/right/input/grip/pose",
    "/user/hand/right/input/grip_surface/pose",
    "/user/hand/right/input/aim/pose",
    "/user/hand/right/output/haptic",
    "/user/hand/right/output/haptic_thumb",
    "/user/hand/right/output/haptic_trigger",
};

const std::vector<const char*> cHpMixedRealityControllerIPData_1_1{
    "/user/hand/left/input/x/click",       "/user/hand/left/input/y/click",           "/user/hand/left/input/menu/click",
    "/user/hand/left/input/squeeze/value", "/user/hand/left/input/trigger/value",     "/user/hand/left/input/thumbstick/x",
    "/user/hand/left/input/thumbstick/y",  "/user/hand/left/input/thumbstick/click",  "/user/hand/left/input/thumbstick",
    "/user/hand/left/input/grip/pose",     "/user/hand/left/input/grip_surface/pose", "/user/hand/left/input/aim/pose",
    "/user/hand/left/output/haptic",       "/user/hand/right/input/a/click",          "/user/hand/right/input/b/click",
    "/user/hand/right/input/menu/click",   "/user/hand/right/input/squeeze/value",    "/user/hand/right/input/trigger/value",
    "/user/hand/right/input/thumbstick/x", "/user/hand/right/input/thumbstick/y",     "/user/hand/right/input/thumbstick/click",
    "/user/hand/right/input/thumbstick",   "/user/hand/right/input/grip/pose",        "/user/hand/right/input/grip_surface/pose",
    "/user/hand/right/input/aim/pose",     "/user/hand/right/output/haptic",
};

const std::vector<const char*> cValveIndexIPData_1_0{
    "/user/hand/left/input/system/click",    "/user/hand/left/input/system/touch",      "/user/hand/left/input/a/click",
    "/user/hand/left/input/a/touch",         "/user/hand/left/input/b/click",           "/user/hand/left/input/b/touch",
    "/user/hand/left/input/squeeze/value",   "/user/hand/left/input/squeeze/force",     "/user/hand/left/input/trigger/click",
    "/user/hand/left/input/trigger/value",   "/user/hand/left/input/trigger/touch",     "/user/hand/left/input/thumbstick/x",
    "/user/hand/left/input/thumbstick/y",    "/user/hand/left/input/thumbstick/click",  "/user/hand/left/input/thumbstick/touch",
    "/user/hand/left/input/thumbstick",      "/user/hand/left/input/trackpad/x",        "/user/hand/left/input/trackpad/y",
    "/user/hand/left/input/trackpad/force",  "/user/hand/left/input/trackpad/touch",    "/user/hand/left/input/trackpad",
    "/user/hand/left/input/grip/pose",       "/user/hand/left/input/aim/pose",          "/user/hand/left/output/haptic",
    "/user/hand/right/input/system/click",   "/user/hand/right/input/system/touch",     "/user/hand/right/input/a/click",
    "/user/hand/right/input/a/touch",        "/user/hand/right/input/b/click",          "/user/hand/right/input/b/touch",
    "/user/hand/right/input/squeeze/value",  "/user/hand/right/input/squeeze/force",    "/user/hand/right/input/trigger/click",
    "/user/hand/right/input/trigger/value",  "/user/hand/right/input/trigger/touch",    "/user/hand/right/input/thumbstick/x",
    "/user/hand/right/input/thumbstick/y",   "/user/hand/right/input/thumbstick/click", "/user/hand/right/input/thumbstick/touch",
    "/user/hand/right/input/thumbstick",     "/user/hand/right/input/trackpad/x",       "/user/hand/right/input/trackpad/y",
    "/user/hand/right/input/trackpad/force", "/user/hand/right/input/trackpad/touch",   "/user/hand/right/input/trackpad",
    "/user/hand/right/input/grip/pose",      "/user/hand/right/input/aim/pose",         "/user/hand/right/output/haptic",
};

const std::vector<const char*> cValveIndexIPData_1_1{
    "/user/hand/left/input/system/click",
    "/user/hand/left/input/system/touch",
    "/user/hand/left/input/a/click",
    "/user/hand/left/input/a/touch",
    "/user/hand/left/input/b/click",
    "/user/hand/left/input/b/touch",
    "/user/hand/left/input/squeeze/value",
    "/user/hand/left/input/squeeze/force",
    "/user/hand/left/input/trigger/click",
    "/user/hand/left/input/trigger/value",
    "/user/hand/left/input/trigger/touch",
    "/user/hand/left/input/thumbstick/x",
    "/user/hand/left/input/thumbstick/y",
    "/user/hand/left/input/thumbstick/click",
    "/user/hand/left/input/thumbstick/touch",
    "/user/hand/left/input/thumbstick",
    "/user/hand/left/input/trackpad/x",
    "/user/hand/left/input/trackpad/y",
    "/user/hand/left/input/trackpad/force",
    "/user/hand/left/input/trackpad/touch",
    "/user/hand/left/input/trackpad",
    "/user/hand/left/input/grip/pose",
    "/user/hand/left/input/grip_surface/pose",
    "/user/hand/left/input/aim/pose",
    "/user/hand/left/output/haptic",
    "/user/hand/right/input/system/click",
    "/user/hand/right/input/system/touch",
    "/user/hand/right/input/a/click",
    "/user/hand/right/input/a/touch",
    "/user/hand/right/input/b/click",
    "/user/hand/right/input/b/touch",
    "/user/hand/right/input/squeeze/value",
    "/user/hand/right/input/squeeze/force",
    "/user/hand/right/input/trigger/click",
    "/user/hand/right/input/trigger/value",
    "/user/hand/right/input/trigger/touch",
    "/user/hand/right/input/thumbstick/x",
    "/user/hand/right/input/thumbstick/y",
    "/user/hand/right/input/thumbstick/click",
    "/user/hand/right/input/thumbstick/touch",
    "/user/hand/right/input/thumbstick",
    "/user/hand/right/input/trackpad/x",
    "/user/hand/right/input/trackpad/y",
    "/user/hand/right/input/trackpad/force",
    "/user/hand/right/input/trackpad/touch",
    "/user/hand/right/input/trackpad",
    "/user/hand/right/input/grip/pose",
    "/user/hand/right/input/aim/pose",
    "/user/hand/right/input/grip_surface/pose",
    "/user/hand/right/output/haptic",
};

const std::vector<const char*> cBytedancePicoNeo3IPData_1_1{
    "/user/hand/left/input/x/click",           "/user/hand/left/input/x/touch",
    "/user/hand/left/input/y/click",           "/user/hand/left/input/y/touch",
    "/user/hand/left/input/system/click",      "/user/hand/left/input/menu/click",
    "/user/hand/left/input/squeeze/click",     "/user/hand/left/input/squeeze/value",
    "/user/hand/left/input/trigger/click",     "/user/hand/left/input/trigger/touch",
    "/user/hand/left/input/trigger/value",     "/user/hand/left/input/thumbstick/x",
    "/user/hand/left/input/thumbstick/y",      "/user/hand/left/input/thumbstick/click",
    "/user/hand/left/input/thumbstick/touch",  "/user/hand/left/input/thumbstick",
    "/user/hand/left/input/grip/pose",         "/user/hand/left/input/grip_surface/pose",
    "/user/hand/left/input/aim/pose",          "/user/hand/left/output/haptic",
    "/user/hand/right/input/a/click",          "/user/hand/right/input/a/touch",
    "/user/hand/right/input/b/click",          "/user/hand/right/input/b/touch",
    "/user/hand/right/input/system/click",     "/user/hand/right/input/menu/click",
    "/user/hand/right/input/squeeze/click",    "/user/hand/right/input/squeeze/value",
    "/user/hand/right/input/trigger/click",    "/user/hand/right/input/trigger/touch",
    "/user/hand/right/input/trigger/value",    "/user/hand/right/input/thumbstick/x",
    "/user/hand/right/input/thumbstick/y",     "/user/hand/right/input/thumbstick/click",
    "/user/hand/right/input/thumbstick/touch", "/user/hand/right/input/thumbstick",
    "/user/hand/right/input/grip/pose",        "/user/hand/right/input/grip_surface/pose",
    "/user/hand/right/input/aim/pose",         "/user/hand/right/output/haptic",
};

const std::vector<const char*> cBytedancePicoG3IPData_1_1{
    "/user/hand/left/input/trigger/click",      "/user/hand/left/input/trigger/value",  "/user/hand/left/input/menu/click",
    "/user/hand/left/input/thumbstick/x",       "/user/hand/left/input/thumbstick/y",   "/user/hand/left/input/thumbstick/click",
    "/user/hand/left/input/thumbstick",         "/user/hand/left/input/grip/pose",      "/user/hand/left/input/grip_surface/pose",
    "/user/hand/left/input/aim/pose",           "/user/hand/right/input/trigger/click", "/user/hand/right/input/trigger/value",
    "/user/hand/right/input/menu/click",        "/user/hand/right/input/thumbstick/x",  "/user/hand/right/input/thumbstick/y",
    "/user/hand/right/input/thumbstick/click",  "/user/hand/right/input/thumbstick",    "/user/hand/right/input/grip/pose",
    "/user/hand/right/input/grip_surface/pose", "/user/hand/right/input/aim/pose",
};

const std::vector<const char*> cBytedancePico4IPData_1_1{
    "/user/hand/left/input/x/click",
    "/user/hand/left/input/x/touch",
    "/user/hand/left/input/y/click",
    "/user/hand/left/input/y/touch",
    "/user/hand/left/input/menu/click",
    "/user/hand/left/input/system/click",
    "/user/hand/left/input/squeeze/click",
    "/user/hand/left/input/squeeze/value",
    "/user/hand/left/input/trigger/click",
    "/user/hand/left/input/trigger/touch",
    "/user/hand/left/input/trigger/value",
    "/user/hand/left/input/thumbstick/x",
    "/user/hand/left/input/thumbstick/y",
    "/user/hand/left/input/thumbstick/click",
    "/user/hand/left/input/thumbstick/touch",
    "/user/hand/left/input/thumbstick",
    "/user/hand/left/input/grip/pose",
    "/user/hand/left/input/grip_surface/pose",
    "/user/hand/left/input/aim/pose",
    "/user/hand/left/output/haptic",
    "/user/hand/right/input/a/click",
    "/user/hand/right/input/a/touch",
    "/user/hand/right/input/b/click",
    "/user/hand/right/input/b/touch",
    "/user/hand/right/input/system/click",
    "/user/hand/right/input/squeeze/click",
    "/user/hand/right/input/squeeze/value",
    "/user/hand/right/input/trigger/click",
    "/user/hand/right/input/trigger/touch",
    "/user/hand/right/input/trigger/value",
    "/user/hand/right/input/thumbstick/x",
    "/user/hand/right/input/thumbstick/y",
    "/user/hand/right/input/thumbstick/click",
    "/user/hand/right/input/thumbstick/touch",
    "/user/hand/right/input/thumbstick",
    "/user/hand/right/input/grip/pose",
    "/user/hand/right/input/grip_surface/pose",
    "/user/hand/right/input/aim/pose",
    "/user/hand/right/output/haptic",
};

const std::vector<const char*> cMlMl2IPData_1_1{
    "/user/hand/left/input/menu/click",      "/user/hand/left/input/home/click",      "/user/hand/left/input/trigger/click",
    "/user/hand/left/input/trigger/value",   "/user/hand/left/input/trackpad/x",      "/user/hand/left/input/trackpad/y",
    "/user/hand/left/input/trackpad/click",  "/user/hand/left/input/trackpad/force",  "/user/hand/left/input/trackpad/touch",
    "/user/hand/left/input/trackpad",        "/user/hand/left/input/grip/pose",       "/user/hand/left/input/grip_surface/pose",
    "/user/hand/left/input/aim/pose",        "/user/hand/left/input/shoulder/click",  "/user/hand/left/output/haptic",
    "/user/hand/right/input/menu/click",     "/user/hand/right/input/home/click",     "/user/hand/right/input/trigger/click",
    "/user/hand/right/input/trigger/value",  "/user/hand/right/input/trackpad/x",     "/user/hand/right/input/trackpad/y",
    "/user/hand/right/input/trackpad/click", "/user/hand/right/input/trackpad/force", "/user/hand/right/input/trackpad/touch",
    "/user/hand/right/input/trackpad",       "/user/hand/right/input/grip/pose",      "/user/hand/right/input/grip_surface/pose",
    "/user/hand/right/input/aim/pose",       "/user/hand/right/input/shoulder/click", "/user/hand/right/output/haptic",
};

const std::vector<const char*> cSamsungOdysseyIPData_1_1{
    "/user/hand/left/input/menu/click",      "/user/hand/left/input/squeeze/click",      "/user/hand/left/input/trigger/value",
    "/user/hand/left/input/thumbstick/x",    "/user/hand/left/input/thumbstick/y",       "/user/hand/left/input/thumbstick/click",
    "/user/hand/left/input/thumbstick",      "/user/hand/left/input/trackpad/x",         "/user/hand/left/input/trackpad/y",
    "/user/hand/left/input/trackpad/click",  "/user/hand/left/input/trackpad/touch",     "/user/hand/left/input/trackpad",
    "/user/hand/left/input/grip/pose",       "/user/hand/left/input/grip_surface/pose",  "/user/hand/left/input/aim/pose",
    "/user/hand/left/output/haptic",

    "/user/hand/right/input/menu/click",     "/user/hand/right/input/squeeze/click",     "/user/hand/right/input/trigger/value",
    "/user/hand/right/input/thumbstick/x",   "/user/hand/right/input/thumbstick/y",      "/user/hand/right/input/thumbstick/click",
    "/user/hand/right/input/thumbstick",     "/user/hand/right/input/trackpad/x",        "/user/hand/right/input/trackpad/y",
    "/user/hand/right/input/trackpad/click", "/user/hand/right/input/trackpad/touch",    "/user/hand/right/input/trackpad",
    "/user/hand/right/input/grip/pose",      "/user/hand/right/input/grip_surface/pose", "/user/hand/right/input/aim/pose",
    "/user/hand/right/output/haptic",
};

const std::vector<const char*> cHtcViveCosmosIPData_1_1{
    "/user/hand/left/input/x/click",          "/user/hand/left/input/y/click",           "/user/hand/left/input/menu/click",
    "/user/hand/left/input/shoulder/click",   "/user/hand/left/input/squeeze/click",     "/user/hand/left/input/trigger/click",
    "/user/hand/left/input/trigger/value",    "/user/hand/left/input/thumbstick/x",      "/user/hand/left/input/thumbstick/y",
    "/user/hand/left/input/thumbstick/click", "/user/hand/left/input/thumbstick/touch",  "/user/hand/left/input/thumbstick",
    "/user/hand/left/input/grip/pose",        "/user/hand/left/input/grip_surface/pose", "/user/hand/left/input/aim/pose",
    "/user/hand/left/output/haptic",          "/user/hand/right/input/a/click",          "/user/hand/right/input/b/click",
    "/user/hand/right/input/system/click",    "/user/hand/right/input/shoulder/click",   "/user/hand/right/input/squeeze/click",
    "/user/hand/right/input/trigger/click",   "/user/hand/right/input/trigger/value",    "/user/hand/right/input/thumbstick/x",
    "/user/hand/right/input/thumbstick/y",    "/user/hand/right/input/thumbstick/click", "/user/hand/right/input/thumbstick/touch",
    "/user/hand/right/input/thumbstick",      "/user/hand/right/input/grip/pose",        "/user/hand/right/input/grip_surface/pose",
    "/user/hand/right/input/aim/pose",        "/user/hand/right/output/haptic",
};

const std::vector<const char*> cHtcViveFocus3IPData_1_1{
    "/user/hand/left/input/x/click",           "/user/hand/left/input/y/click",
    "/user/hand/left/input/menu/click",        "/user/hand/left/input/squeeze/click",
    "/user/hand/left/input/squeeze/touch",     "/user/hand/left/input/squeeze/value",
    "/user/hand/left/input/trigger/click",     "/user/hand/left/input/trigger/touch",
    "/user/hand/left/input/trigger/value",     "/user/hand/left/input/thumbstick/x",
    "/user/hand/left/input/thumbstick/y",      "/user/hand/left/input/thumbstick/click",
    "/user/hand/left/input/thumbstick/touch",  "/user/hand/left/input/thumbstick",
    "/user/hand/left/input/thumbrest/touch",   "/user/hand/left/input/grip/pose",
    "/user/hand/left/input/grip_surface/pose", "/user/hand/left/input/aim/pose",
    "/user/hand/left/output/haptic",           "/user/hand/right/input/a/click",
    "/user/hand/right/input/b/click",          "/user/hand/right/input/system/click",
    "/user/hand/right/input/squeeze/click",    "/user/hand/right/input/squeeze/touch",
    "/user/hand/right/input/squeeze/value",    "/user/hand/right/input/trigger/click",
    "/user/hand/right/input/trigger/touch",    "/user/hand/right/input/trigger/value",
    "/user/hand/right/input/thumbstick/x",     "/user/hand/right/input/thumbstick/y",
    "/user/hand/right/input/thumbstick/click", "/user/hand/right/input/thumbstick/touch",
    "/user/hand/right/input/thumbstick",       "/user/hand/right/input/thumbrest/touch",
    "/user/hand/right/input/grip/pose",        "/user/hand/right/input/grip_surface/pose",
    "/user/hand/right/input/aim/pose",         "/user/hand/right/output/haptic",
};

struct InteractionProfileMetadata {
    std::string InteractionProfilePathString;
    std::vector<std::string> TopLevelPaths;
    std::vector<const char*> InputSourcePaths;
};

const std::vector<InteractionProfileMetadata> cInteractionProfileDefinitions_1_0 = {
    {"/interaction_profiles/khr/simple_controller", {"/user/hand/left", "/user/hand/right"}, cSimpleControllerIPData_1_0},
    {"/interaction_profiles/google/daydream_controller",
     {"/user/hand/left", "/user/hand/right"},
     cGoogleDaydreamControllerIPData_1_0},
    {"/interaction_profiles/htc/vive_controller", {"/user/hand/left", "/user/hand/right"}, cViveControllerIPData_1_0},
    {"/interaction_profiles/htc/vive_pro", {"/user/head"}, cViveProIPData},
    {"/interaction_profiles/microsoft/motion_controller", {"/user/hand/left", "/user/hand/right"}, cWMRControllerIPData_1_0},
    {"/interaction_profiles/microsoft/xbox_controller", {"/user/gamepad"}, cGamepadIPData},
    {"/interaction_profiles/oculus/go_controller", {"/user/hand/left", "/user/hand/right"}, cOculusGoIPData_1_0},
    {"/interaction_profiles/oculus/touch_controller", {"/user/hand/left", "/user/hand/right"}, cOculusTouchIPData_1_0},
    {"/interaction_profiles/valve/index_controller", {"/user/hand/left", "/user/hand/right"}, cValveIndexIPData_1_0}};

const std::vector<InteractionProfileMetadata> cInteractionProfileDefinitions_1_0_khr_maintenance1 = {
    {"/interaction_profiles/khr/simple_controller", {"/user/hand/left", "/user/hand/right"}, cSimpleControllerIPData_1_1},
    {"/interaction_profiles/google/daydream_controller",
     {"/user/hand/left", "/user/hand/right"},
     cGoogleDaydreamControllerIPData_1_1},
    {"/interaction_profiles/htc/vive_controller", {"/user/hand/left", "/user/hand/right"}, cViveControllerIPData_1_1},
    {"/interaction_profiles/htc/vive_pro", {"/user/head"}, cViveProIPData},
    {"/interaction_profiles/microsoft/motion_controller", {"/user/hand/left", "/user/hand/right"}, cWMRControllerIPData_1_1},
    {"/interaction_profiles/microsoft/xbox_controller", {"/user/gamepad"}, cGamepadIPData},
    {"/interaction_profiles/oculus/go_controller", {"/user/hand/left", "/user/hand/right"}, cOculusGoIPData_1_1},
    {"/interaction_profiles/oculus/touch_controller", {"/user/hand/left", "/user/hand/right"}, cOculusTouchIPData_1_0_khr_maint1},
    {"/interaction_profiles/valve/index_controller", {"/user/hand/left", "/user/hand/right"}, cValveIndexIPData_1_1}};

const std::vector<InteractionProfileMetadata> cInteractionProfileDefinitions_1_1{
    {"/interaction_profiles/khr/simple_controller", {"/user/hand/left", "/user/hand/right"}, cSimpleControllerIPData_1_1},
    {"/interaction_profiles/google/daydream_controller",
     {"/user/hand/left", "/user/hand/right"},
     cGoogleDaydreamControllerIPData_1_1},
    {"/interaction_profiles/htc/vive_controller", {"/user/hand/left", "/user/hand/right"}, cViveControllerIPData_1_1},
    {"/interaction_profiles/htc/vive_pro", {"/user/head"}, cViveProIPData},
    {"/interaction_profiles/microsoft/motion_controller", {"/user/hand/left", "/user/hand/right"}, cWMRControllerIPData_1_1},
    {"/interaction_profiles/microsoft/xbox_controller", {"/user/gamepad"}, cGamepadIPData},
    {"/interaction_profiles/oculus/go_controller", {"/user/hand/left", "/user/hand/right"}, cOculusGoIPData_1_1},
    {"/interaction_profiles/oculus/touch_controller", {"/user/hand/left", "/user/hand/right"}, cOculusTouchIPData_1_1},
    {"/interaction_profiles/valve/index_controller", {"/user/hand/left", "/user/hand/right"}, cValveIndexIPData_1_1},

    // new profiles in 1.1

    {"/interaction_profiles/bytedance/pico_neo3_controller", {"/user/hand/left", "/user/hand/right"}, cBytedancePicoNeo3IPData_1_1},
    {"/interaction_profiles/bytedance/pico4_controller", {"/user/hand/left", "/user/hand/right"}, cBytedancePico4IPData_1_1},
    {"/interaction_profiles/bytedance/pico_g3_controller", {"/user/hand/left", "/user/hand/right"}, cBytedancePicoG3IPData_1_1},
    {"/interaction_profiles/hp/mixed_reality_controller",
     {"/user/hand/left", "/user/hand/right"},
     cHpMixedRealityControllerIPData_1_1},
    {"/interaction_profiles/htc/vive_cosmos_controller", {"/user/hand/left", "/user/hand/right"}, cHtcViveCosmosIPData_1_1},
    {"/interaction_profiles/htc/vive_focus3_controller", {"/user/hand/left", "/user/hand/right"}, cHtcViveFocus3IPData_1_1},

    {"/interaction_profiles/meta/touch_controller_rift_cv1", {"/user/hand/left", "/user/hand/right"}, cOculusTouchIPData_1_1},
    {"/interaction_profiles/meta/touch_controller_quest_1_rift_s", {"/user/hand/left", "/user/hand/right"}, cOculusTouchIPData_1_1},
    {"/interaction_profiles/meta/touch_controller_quest_2", {"/user/hand/left", "/user/hand/right"}, cOculusTouchIPData_1_1},
    {"/interaction_profiles/meta/touch_plus_controller", {"/user/hand/left", "/user/hand/right"}, cMetaTouchPlusIPData_1_1},
    {"/interaction_profiles/meta/touch_pro_controller", {"/user/hand/left", "/user/hand/right"}, cMetaTouchProIPData_1_1},

    {"/interaction_profiles/ml/ml2_controller", {"/user/hand/left", "/user/hand/right"}, cMlMl2IPData_1_1},
    {"/interaction_profiles/samsung/odyssey_controller", {"/user/hand/left", "/user/hand/right"}, cSamsungOdysseyIPData_1_1},
};

#if XR_PTR_SIZE == 8
template <typename H, typename T>
H promoteToHandle(T* p) {
    return reinterpret_cast<H>(p);
}
template <typename H, typename T>
T* demoteFromHandle(H h) {
    return reinterpret_cast<T*>(h);
}
template <typename H>
H intToHandle(uint64_t u) {
    return reinterpret_cast<H>(u);
}
template <typename H>
uint64_t intFromHandle(H h) {
    return reinterpret_cast<uint64_t>(h);
}
#else
template <typename H, typename T>
H promoteToHandle(T* p) {
    return static_cast<H>(reinterpret_cast<uintptr_t>(p));
}
template <typename H, typename T>
T* demoteFromHandle(H h) {
    return reinterpret_cast<T*>(h);
}
template <typename H>
H intToHandle(uint64_t u) {
    return u;
}
template <typename H>
uint64_t intFromHandle(H h) {
    return h;
}
#endif

template <typename T, typename U>
XrResult ElementCapacityWrite(uint32_t elementCapacityInput, uint32_t* elementCountOutput, T* elementsOutput,
                              const U* elementsSource, size_t elementsSize) {
    if (elementCountOutput == nullptr) {
        return XR_ERROR_VALIDATION_FAILURE;
    }

    *elementCountOutput = static_cast<uint32_t>(elementsSize);

    // request only
    if (elementCapacityInput == 0) {
        return XR_SUCCESS;
    }

    if (elementCapacityInput < elementsSize) {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }

    for (uint32_t i = 0; i < elementsSize; ++i) {
        elementsOutput[i] = elementsSource[i];
    }

    return XR_SUCCESS;
}

template <typename T, typename U>
XrResult XrElementCapacityWrite(uint32_t elementCapacityInput, uint32_t* elementCountOutput, T* elementsOutput,
                                const U* elementsSource, size_t elementsSize) {
    if (elementCountOutput == nullptr) {
        return XR_ERROR_VALIDATION_FAILURE;
    }

    *elementCountOutput = static_cast<uint32_t>(elementsSize);

    // request only
    if (elementCapacityInput == 0) {
        return XR_SUCCESS;
    }

    if (elementCapacityInput < elementsSize) {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }

    // TODO.CTS - https://gitlab.khronos.org/openxr/openxr/-/issues/2485#all-functions-with-two-call
    // https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html#fundamentals-valid-usage-for-structure-pointer-chains
    // Upon return from any function, all type and next fields in the chain must be unmodified.
    for (uint32_t i = 0; i < elementsSize; ++i) {
        XrStructureType originalOutputType = elementsOutput[i].type;
        void* originalOutputNext = elementsOutput[i].next;
        elementsOutput[i] = elementsSource[i];
        elementsOutput[i].type = originalOutputType;
        elementsOutput[i].next = originalOutputNext;
    }

    return XR_SUCCESS;
}

template <typename T, XrStructureType structType>
const T* findInNextChain(const void* next) {
    while (next != nullptr) {
        const auto* p = reinterpret_cast<const XrBaseInStructure*>(next);
        if (p->type == structType) {
            return reinterpret_cast<const T*>(p);
        } else {
            next = p->next;
        }
    }
    return nullptr;
}

template <typename T, XrStructureType structType>
T* findInNextChain(void* next) {
    return const_cast<T*>(findInNextChain<T, structType>(const_cast<const void*>(next)));
}

XrTime currentXrTime() {
    const auto current = std::chrono::steady_clock::now();
    return std::chrono::nanoseconds(current - g.initialTime).count();
}

bool validateQuat(const XrQuaternionf& quat) {
    // tolerance for unit quats is 1%
    float norm = sqrtf(quat.x * quat.x + quat.y * quat.y + quat.z * quat.z + quat.w * quat.w);
    return fabsf(norm - 1.f) <= 0.1f;
}

template <typename T>
void addSessionEventToQueueLocked(std::deque<std::pair<uint64_t, XrEventDataBuffer>>& instanceQueue, XrSession session,
                                  T* eventToAdd) {
    static_assert(sizeof(T) <= sizeof(XrEventDataBuffer), "event bigger than buffer");
    XrEventDataBuffer buffer;
    memcpy(&buffer, eventToAdd, sizeof(T));
    instanceQueue.emplace_back(intFromHandle(session), buffer);
}

// https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html#_xrpollevent
// The runtime must discard queued events which contain destroyed or otherwise invalid handles.
template <typename T>
void purgeEvents(XrInstance instance, T eventParent) {
    XrInstance_T* instancePtr = demoteFromHandle<XrInstance, XrInstance_T>(instance);
    std::unique_lock<std::mutex> lock(instancePtr->instanceEventsMutex);

    for (auto it = instancePtr->instanceEvents.begin(); it != instancePtr->instanceEvents.end();) {
        if (intToHandle<T>(it->first) == eventParent) {
            it = instancePtr->instanceEvents.erase(it);
        } else {
            ++it;
        }
    }
}

XrSession_T::~XrSession_T() { purgeEvents<XrSession>(instance, promoteToHandle<XrSession, XrSession_T>(this)); }

XrSpace_T::~XrSpace_T() {
    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);
    purgeEvents<XrSpace>(sessionPtr->instance, promoteToHandle<XrSpace, XrSpace_T>(this));
}

void switchSessionState(XrSession session, XrSessionState newSessionState) {
    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);

    {
        XrInstance_T* instancePtr = demoteFromHandle<XrInstance, XrInstance_T>(sessionPtr->instance);

        std::unique_lock<std::mutex> lock(instancePtr->instanceEventsMutex);

        sessionPtr->sessionState = newSessionState;

        XrEventDataSessionStateChanged sessionState{XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED};
        sessionState.session = session;
        sessionState.state = newSessionState;
        sessionState.time = currentXrTime();

        addSessionEventToQueueLocked(instancePtr->instanceEvents, session, &sessionState);
    }
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrCreateInstance(const XrInstanceCreateInfo* createInfo, XrInstance* instance) {
    if (createInfo->applicationInfo.apiVersion < XR_MAKE_VERSION(1, 0, 0) ||
        createInfo->applicationInfo.apiVersion >= XR_MAKE_VERSION(1, 2, 0)) {
        return XR_ERROR_API_VERSION_UNSUPPORTED;
    }

    std::vector<std::string> enabledExtensions(createInfo->enabledExtensionCount);

    for (uint32_t i = 0; i < createInfo->enabledExtensionCount; ++i) {
        const char* requestedExtension = createInfo->enabledExtensionNames[i];
        enabledExtensions[i] = requestedExtension;

        // The OpenXR Loader validates that the extensions requested by the application
        // are available
        // TODO.CTS: https://gitlab.khronos.org/openxr/openxr/-/issues/2485#core-functions
    }

    auto instancePtr = std::make_unique<XrInstance_T>();
    instancePtr->apiVersion = createInfo->applicationInfo.apiVersion;
    instancePtr->enabledExtensions = enabledExtensions;

    *instance = promoteToHandle<XrInstance, XrInstance_T>(instancePtr.get());

    {
        std::unique_lock<std::mutex> lock(g.instancesMutex);
        g.instances.push_back(std::move(instancePtr));
    }

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrDestroyInstance(XrInstance instance) {
    {
        std::unique_lock<std::mutex> lock(g.instancesMutex);
        const auto& it = std::find_if(g.instances.begin(), g.instances.end(), [instance](const auto& i) {
            return demoteFromHandle<XrInstance, XrInstance_T>(instance) == i.get();
        });
        g.instances.erase(it);
    }

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrEnumerateApiLayerProperties(uint32_t propertyCapacityInput,
                                                                        uint32_t* propertyCountOutput,
                                                                        XrApiLayerProperties* properties) {
    // Missing CTS test for the runtime implementation of enumerateApiLayerProperties;
    // current validation is in the loader.
    // TODO.CTS: https://gitlab.khronos.org/openxr/openxr/-/issues/2485#core-functions

    std::array<XrApiLayerProperties, 0> runtimeLayers{};
    return XrElementCapacityWrite(propertyCapacityInput, propertyCountOutput, properties, runtimeLayers.data(),
                                  runtimeLayers.size());
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrEnumerateInstanceExtensionProperties(const char* layerName,
                                                                                 uint32_t propertyCapacityInput,
                                                                                 uint32_t* propertyCountOutput,
                                                                                 XrExtensionProperties* properties) {
    if (layerName != nullptr) {
        return XR_ERROR_API_LAYER_NOT_PRESENT;
    }

    static constexpr std::array<XrExtensionProperties, 3> runtimeExtensions = {{
        XrExtensionProperties{
            /*.type =*/XR_TYPE_EXTENSION_PROPERTIES,
            /*.next =*/nullptr,
            /*.extensionName =*/XR_KHR_MAINTENANCE1_EXTENSION_NAME,
            /*.extensionVersion =*/XR_KHR_maintenance1_SPEC_VERSION,
        },
        XrExtensionProperties{
            /*.type =*/XR_TYPE_EXTENSION_PROPERTIES,
            /*.next =*/nullptr,
            /*.extensionName =*/XR_MND_HEADLESS_EXTENSION_NAME,
            /*.extensionVersion =*/XR_MND_headless_SPEC_VERSION,
        },
        XrExtensionProperties{
            /*.type =*/XR_TYPE_EXTENSION_PROPERTIES,
            /*.next =*/nullptr,
            /*.extensionName =*/"XR_KHR_fake_ext2",
            /*.extensionVersion =*/3,
        },
    }};

    return XrElementCapacityWrite(propertyCapacityInput, propertyCountOutput, properties, runtimeExtensions.data(),
                                  runtimeExtensions.size());
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrGetSystem(XrInstance /*instance*/, const XrSystemGetInfo* getInfo,
                                                      XrSystemId* systemId) {
    if (getInfo->formFactor != XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY) {
        return XR_ERROR_FORM_FACTOR_UNSUPPORTED;
    }
    *systemId = VALID_SYSTEM_ID;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrGetSystemProperties(XrInstance instance, XrSystemId systemId,
                                                                XrSystemProperties* properties) {
    if (instance == XR_NULL_HANDLE) {
        return XR_ERROR_HANDLE_INVALID;
    }
    if (systemId != 1) {
        return XR_ERROR_SYSTEM_INVALID;
    }

    properties->graphicsProperties.maxLayerCount = 16;
    properties->graphicsProperties.maxSwapchainImageHeight = 1;
    properties->graphicsProperties.maxSwapchainImageWidth = 1;
    properties->systemId = systemId;
    strcpy(properties->systemName, "Test system");
    properties->vendorId = 0x0;
    return XR_SUCCESS;
}

XrResult viewConfigurationError(XrInstance instance, XrViewConfigurationType viewConfigurationType) {
    switch (viewConfigurationType) {
        case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
            return XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
        case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET: {
            XrInstance_T* instancePtr = demoteFromHandle<XrInstance, XrInstance_T>(instance);
            if (instancePtr->apiVersion >= XR_MAKE_VERSION(1, 1, 0)) {
                return XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
            } else {
                return XR_ERROR_VALIDATION_FAILURE;
            }
        }
        case XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT:
        case XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM:
        default:
            return XR_ERROR_VALIDATION_FAILURE;
    }
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrGetInstanceProperties(XrInstance /*instance*/,
                                                                  XrInstanceProperties* instanceProperties) {
    instanceProperties->runtimeVersion = XR_MAKE_VERSION(0, 0, 0);
    memset(instanceProperties->runtimeName, 0, sizeof(instanceProperties->runtimeName));
    strcpy(instanceProperties->runtimeName, "Test Runtime");
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrPollEvent(XrInstance instance, XrEventDataBuffer* eventData) {
    XrInstance_T* instancePtr = demoteFromHandle<XrInstance, XrInstance_T>(instance);

    std::unique_lock<std::mutex> lock(instancePtr->instanceEventsMutex);
    if (instancePtr->instanceEvents.empty()) {
        return XR_EVENT_UNAVAILABLE;
    }

    auto eventDataPair = instancePtr->instanceEvents.front();
    *eventData = eventDataPair.second;
    instancePtr->instanceEvents.pop_front();
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrEnumerateEnvironmentBlendModes(XrInstance instance, XrSystemId systemId,
                                                                           XrViewConfigurationType viewConfigurationType,
                                                                           uint32_t environmentBlendModeCapacityInput,
                                                                           uint32_t* environmentBlendModeCountOutput,
                                                                           XrEnvironmentBlendMode* environmentBlendModes) {
    if (systemId != VALID_SYSTEM_ID) {
        return XR_ERROR_SYSTEM_INVALID;
    }

    if (viewConfigurationType != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
        return viewConfigurationError(instance, viewConfigurationType);
    }

    std::array<XrEnvironmentBlendMode, 1> modes{{XR_ENVIRONMENT_BLEND_MODE_OPAQUE}};
    return ElementCapacityWrite(environmentBlendModeCapacityInput, environmentBlendModeCountOutput, environmentBlendModes,
                                modes.data(), modes.size());
}

XRAPI_ATTR XrResult XRAPI_CALL
RuntimeTestXrResultToString(XrInstance /*instance*/, XrResult value,
                            char buffer[XR_MAX_RESULT_STRING_SIZE])  // NOLINT(modernize-avoid-c-arrays)
{
#define XR_ENUM_CASE_STR(name, val)                                                      \
    case val:                                                                            \
        static_assert(sizeof(#name) / sizeof(#name[0]) > sizeof("XR_"), "invalid data"); \
        strncpy(buffer, #name, XR_MAX_RESULT_STRING_SIZE);                               \
        buffer[XR_MAX_RESULT_STRING_SIZE - 1] = '\0';                                    \
        break;

// Potential intentional truncation
#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif  // defined(__GNUC__) && __GNUC__ >= 8
    switch (value) {
        XR_LIST_ENUM_XrResult(XR_ENUM_CASE_STR)  // nbsp
            default : {
            const char* desc = XR_SUCCEEDED(value) ? "XR_UNKNOWN_SUCCESS_" : "XR_UNKNOWN_FAILURE_";
            snprintf(buffer, XR_MAX_RESULT_STRING_SIZE, "%s%d", desc, (int)value);
        }
    }
#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic pop
#endif  // defined(__GNUC__) && __GNUC__ >= 8

#undef XR_ENUM_CASE_STR

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL
RuntimeTestXrStructureTypeToString(XrInstance /*instance*/, XrStructureType value,
                                   char buffer[XR_MAX_STRUCTURE_NAME_SIZE])  // NOLINT(modernize-avoid-c-arrays)
{
#define XR_ENUM_CASE_STR(name, val)                                                           \
    case val:                                                                                 \
        static_assert(sizeof(#name) / sizeof(#name[0]) > sizeof("XR_TYPE_"), "invalid data"); \
        strncpy(buffer, #name, XR_MAX_STRUCTURE_NAME_SIZE);                                   \
        buffer[XR_MAX_STRUCTURE_NAME_SIZE - 1] = '\0';                                        \
        break;

// Intentional truncation: see XR_KHR_extended_struct_name_lengths
#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif  // defined(__GNUC__) && __GNUC__ >= 8
    switch (value) {
        XR_LIST_ENUM_XrStructureType(XR_ENUM_CASE_STR)  // nbsp
            default : {
            snprintf(buffer, XR_MAX_RESULT_STRING_SIZE, "XR_UNKNOWN_STRUCTURE_TYPE_%d", (int)value);
            break;
        }
    }
#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic pop
#endif  // defined(__GNUC__) && __GNUC__ >= 8

#undef XR_ENUM_CASE_STR
    return XR_SUCCESS;
}

//
// Session
//

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo,
                                                          XrSession* session) {
    if (createInfo->systemId != VALID_SYSTEM_ID) {
        return XR_ERROR_SYSTEM_INVALID;
    }

    auto sessionPtr = std::make_unique<XrSession_T>();
    sessionPtr->instance = instance;

    *session = promoteToHandle<XrSession, XrSession_T>(sessionPtr.get());

    {
        XrInstance_T* instancePtr = demoteFromHandle<XrInstance, XrInstance_T>(instance);
        std::unique_lock<std::mutex> lock(instancePtr->sessionsMutex);
        instancePtr->sessions.push_back(std::move(sessionPtr));
    }

    switchSessionState(*session, XR_SESSION_STATE_IDLE);
    switchSessionState(*session, XR_SESSION_STATE_READY);

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrDestroySession(XrSession session) {
    {
        XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);
        XrInstance_T* instancePtr = demoteFromHandle<XrInstance, XrInstance_T>(sessionPtr->instance);
        std::unique_lock<std::mutex> lock(instancePtr->sessionsMutex);
        const auto& it = std::find_if(instancePtr->sessions.begin(), instancePtr->sessions.end(),
                                      [sessionPtr](const auto& i) { return sessionPtr == i.get(); });
        instancePtr->sessions.erase(it);
    }

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo) {
    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);

    if (beginInfo->primaryViewConfigurationType != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
        return viewConfigurationError(sessionPtr->instance, beginInfo->primaryViewConfigurationType);
    }

    if (sessionPtr->beginTime != 0) {
        return XR_ERROR_SESSION_RUNNING;
    }

    sessionPtr->beginTime = currentXrTime();

    switchSessionState(session, XR_SESSION_STATE_SYNCHRONIZED);

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrEndSession(XrSession session) {
    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);
    if (sessionPtr->sessionState != XR_SESSION_STATE_STOPPING) {
        return XR_ERROR_SESSION_NOT_STOPPING;
    }

    sessionPtr->beginTime = 0;

    switchSessionState(session, XR_SESSION_STATE_IDLE);
    switchSessionState(session, XR_SESSION_STATE_EXITING);

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrRequestExitSession(XrSession session) {
    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);
    if (sessionPtr->beginTime == 0) {
        return XR_ERROR_SESSION_NOT_RUNNING;
    }

    // Behavior for request exit session twice not defined, but we return not running.
    if (sessionPtr->endTime != 0) {
        return XR_ERROR_SESSION_NOT_RUNNING;
    }
    sessionPtr->endTime = currentXrTime();

    if (sessionPtr->sessionState == XR_SESSION_STATE_FOCUSED) {
        switchSessionState(session, XR_SESSION_STATE_VISIBLE);
        switchSessionState(session, XR_SESSION_STATE_SYNCHRONIZED);
    }
    switchSessionState(session, XR_SESSION_STATE_STOPPING);

    return XR_SUCCESS;
}

//
// Spaces
//

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrEnumerateReferenceSpaces(XrSession session, uint32_t spaceCapacityInput,
                                                                     uint32_t* spaceCountOutput, XrReferenceSpaceType* spaces) {
    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);
    XrInstance_T* instancePtr = demoteFromHandle<XrInstance, XrInstance_T>(sessionPtr->instance);

    if (instancePtr->apiVersion < XR_MAKE_VERSION(1, 1, 0)) {
        static constexpr std::array<XrReferenceSpaceType, 3> baseSpaces{{
            XR_REFERENCE_SPACE_TYPE_VIEW,
            XR_REFERENCE_SPACE_TYPE_LOCAL,
            XR_REFERENCE_SPACE_TYPE_STAGE,
        }};

        return ElementCapacityWrite(spaceCapacityInput, spaceCountOutput, spaces, baseSpaces.data(), baseSpaces.size());
    } else {
        // OpenXR 1.1 introduced LOCAL_FLOOR
        static constexpr std::array<XrReferenceSpaceType, 4> baseSpaces{{
            XR_REFERENCE_SPACE_TYPE_VIEW,
            XR_REFERENCE_SPACE_TYPE_LOCAL,
            XR_REFERENCE_SPACE_TYPE_STAGE,
            XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR,
        }};

        return ElementCapacityWrite(spaceCapacityInput, spaceCountOutput, spaces, baseSpaces.data(), baseSpaces.size());
    }
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrCreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo* createInfo,
                                                                 XrSpace* space) {
    if (!validateQuat(createInfo->poseInReferenceSpace.orientation)) {
        return XR_ERROR_POSE_INVALID;
    }

    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);
    XrInstance_T* instancePtr = demoteFromHandle<XrInstance, XrInstance_T>(sessionPtr->instance);

    if (instancePtr->apiVersion < XR_MAKE_VERSION(1, 1, 0)) {
        std::array<XrReferenceSpaceType, 3> knownReferenceSpaces{
            {XR_REFERENCE_SPACE_TYPE_VIEW, XR_REFERENCE_SPACE_TYPE_LOCAL, XR_REFERENCE_SPACE_TYPE_STAGE}};

        if (std::find(knownReferenceSpaces.begin(), knownReferenceSpaces.end(), createInfo->referenceSpaceType) ==
            knownReferenceSpaces.end()) {
            return XR_ERROR_REFERENCE_SPACE_UNSUPPORTED;
        }
    } else {
        // OpenXR 1.1 introduced local floor
        std::array<XrReferenceSpaceType, 4> knownReferenceSpaces{{XR_REFERENCE_SPACE_TYPE_VIEW, XR_REFERENCE_SPACE_TYPE_LOCAL,
                                                                  XR_REFERENCE_SPACE_TYPE_STAGE,
                                                                  XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR}};

        if (std::find(knownReferenceSpaces.begin(), knownReferenceSpaces.end(), createInfo->referenceSpaceType) ==
            knownReferenceSpaces.end()) {
            return XR_ERROR_REFERENCE_SPACE_UNSUPPORTED;
        }
    }

    auto spacePtr = std::make_unique<XrSpace_T>();
    *space = promoteToHandle<XrSpace, XrSpace_T>(spacePtr.get());
    spacePtr->session = session;
    spacePtr->spaceType = XrSpace_T::SpaceType::ReferenceSpace;
    spacePtr->referenceSpaceType = createInfo->referenceSpaceType;
    spacePtr->poseInReferenceSpace = createInfo->poseInReferenceSpace;

    {
        std::unique_lock<std::mutex> lock(sessionPtr->spacesMutex);
        sessionPtr->spaces.push_back(std::move(spacePtr));
    }

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrGetReferenceSpaceBoundsRect(XrSession session, XrReferenceSpaceType referenceSpaceType,
                                                                        XrExtent2Df* bounds) {
    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);
    XrInstance_T* instancePtr = demoteFromHandle<XrInstance, XrInstance_T>(sessionPtr->instance);
    if (instancePtr->apiVersion > XR_MAKE_VERSION(1, 1, 0)) {
        //  OpenXR 1.1 and later support LOCAL_FLOOR; Bounds of LOCAL FLOOR
        //  are the same as LOCAL.
        if (referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR) {
            referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        }
    }

    switch (referenceSpaceType) {
        case XR_REFERENCE_SPACE_TYPE_VIEW:
        case XR_REFERENCE_SPACE_TYPE_LOCAL:
        case XR_REFERENCE_SPACE_TYPE_STAGE:
            // stage might have bounds with a real runtime, but ours does not.
            bounds->width = 0;
            bounds->height = 0;
            return XR_SPACE_BOUNDS_UNAVAILABLE;
        case XR_REFERENCE_SPACE_TYPE_MAX_ENUM:
            return XR_ERROR_VALIDATION_FAILURE;
        default:
            return XR_ERROR_REFERENCE_SPACE_UNSUPPORTED;
    }
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location) {
    if (time <= 0) {
        return XR_ERROR_TIME_INVALID;
    }

    auto* spaceVelocity = findInNextChain<XrSpaceVelocity, XR_TYPE_SPACE_VELOCITY>(location->next);

    static constexpr XrSpaceLocationFlags validAndTracked =
        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT |
        XR_SPACE_LOCATION_POSITION_TRACKED_BIT;

    static constexpr XrSpaceLocationFlags validOnly =
        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;

    static constexpr XrSpaceVelocityFlags velocityTracked =
        XR_SPACE_VELOCITY_LINEAR_VALID_BIT | XR_SPACE_VELOCITY_ANGULAR_VALID_BIT;

    XrSpace_T* spacePtr = demoteFromHandle<XrSpace, XrSpace_T>(space);
    XrSpace_T* baseSpacePtr = demoteFromHandle<XrSpace, XrSpace_T>(baseSpace);

    if (spacePtr->spaceType == XrSpace_T::SpaceType::ActionSpace &&
        baseSpacePtr->spaceType == XrSpace_T::SpaceType::ReferenceSpace) {
        location->locationFlags = 0;
        return XR_SUCCESS;
    } else if (spacePtr->spaceType == XrSpace_T::SpaceType::ActionSpace &&
               baseSpacePtr->spaceType == XrSpace_T::SpaceType::ActionSpace) {
        location->locationFlags = 0;
        return XR_SUCCESS;
    } else if (spacePtr->spaceType != XrSpace_T::SpaceType::ReferenceSpace ||
               baseSpacePtr->spaceType != XrSpace_T::SpaceType::ReferenceSpace) {
        location->locationFlags = 0;
        return XR_SUCCESS;
    } else {
        // both reference spaces
        if (spacePtr->referenceSpaceType == baseSpacePtr->referenceSpaceType) {
            // same referenceSpaceType
            location->locationFlags = validAndTracked;
        } else if ((spacePtr->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL &&
                    baseSpacePtr->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR) ||
                   (spacePtr->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR &&
                    baseSpacePtr->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL)) {
            // same referenceSpaceType
            location->locationFlags = validAndTracked;
        } else {
            // different referenceSpaceType
            location->locationFlags = validOnly;
        }

        XrPosef xfTargetOffsetFromTargetSpace = spacePtr->poseInReferenceSpace;
        XrPosef xfBaseOffsetFromBaseSpace = baseSpacePtr->poseInReferenceSpace;

        XrPosef xfBaseSpaceFromBaseOffset;
        XrPosef_Invert(&xfBaseSpaceFromBaseOffset, &xfBaseOffsetFromBaseSpace);

        XrPosef result;
        XrPosef_Multiply(&result, &xfBaseSpaceFromBaseOffset, &xfTargetOffsetFromTargetSpace);

        location->pose = result;

        if (spaceVelocity != nullptr) {
            spaceVelocity->velocityFlags = velocityTracked;
            spaceVelocity->linearVelocity = {0, 0, 0};
            spaceVelocity->angularVelocity = {0, 0, 0};
        }

        return XR_SUCCESS;
    }
}

XRAPI_ATTR XrResult XRAPI_CALL TestRuntimeXrLocateSpaces(XrSession /*session*/, const XrSpacesLocateInfo* locateInfo,
                                                         XrSpaceLocations* spaceLocations) {
    if (locateInfo->time <= 0) {
        return XR_ERROR_TIME_INVALID;
    }
    if (locateInfo->spaceCount != spaceLocations->locationCount) {
        return XR_ERROR_VALIDATION_FAILURE;
    }

    auto* spaceVelocities = findInNextChain<XrSpaceVelocities, XR_TYPE_SPACE_VELOCITIES>(spaceLocations->next);
    if (spaceVelocities != nullptr) {
        if (locateInfo->spaceCount != spaceVelocities->velocityCount) {
            return XR_ERROR_VALIDATION_FAILURE;
        }
    }

    for (uint32_t i = 0; i < locateInfo->spaceCount; ++i) {
        XrSpaceVelocity vel{XR_TYPE_SPACE_VELOCITY};
        XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION, &vel};
        XrResult res = RuntimeTestXrLocateSpace(locateInfo->spaces[i], locateInfo->baseSpace, locateInfo->time, &loc);
        if (res != XR_SUCCESS) {
            return res;
        }
        spaceLocations->locations[i].locationFlags = loc.locationFlags;
        spaceLocations->locations[i].pose = loc.pose;
        if (spaceVelocities != nullptr) {
            spaceVelocities->velocities[i].velocityFlags = vel.velocityFlags;
            spaceVelocities->velocities[i].linearVelocity = vel.linearVelocity;
            spaceVelocities->velocities[i].angularVelocity = vel.angularVelocity;
        }
    }

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrDestroySpace(XrSpace space) {
    {
        XrSpace_T* spacePtr = demoteFromHandle<XrSpace, XrSpace_T>(space);
        XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(spacePtr->session);
        std::unique_lock<std::mutex> lock(sessionPtr->spacesMutex);
        const auto& it = std::find_if(sessionPtr->spaces.begin(), sessionPtr->spaces.end(),
                                      [space](const auto& i) { return demoteFromHandle<XrSpace, XrSpace_T>(space) == i.get(); });
        sessionPtr->spaces.erase(it);
    }

    return XR_SUCCESS;
}

//
// Action System
//

bool validatePathChar(const char c) {
    if (c >= 'a' && c <= 'z') {
        return true;
    }
    if (c >= '0' && c <= '9') {
        return true;
    }
    if (c == '-' || c == '_' || c == '.') {
        return true;
    }
    return false;
}

bool validatePath(const char* path) {
    // A well-formed path name string must conform to the following rules:
    //   - Path name strings must be constructed entirely from characters on the following list.
    //       - Lower case ASCII letters : a - z
    //       - Numeric digits : 0 - 9
    //       - Dash : -
    //       - Underscore : _
    //       - Period : .
    //       - Forward Slash : /
    //   - Path name strings must start with a single forward slash character.
    //   - Path name strings must not contain two or more adjacent forward slash characters.
    //   - Path name strings must not contain two forward slash characters that are separated by only period characters.
    //   - Path name strings must not contain only period characters following the final forward slash character in the string.
    //   - The maximum string length for a path name string, including the terminating \0 character, is defined by
    //   XR_MAX_PATH_LENGTH.

    // start with forward slash
    if (path[0] != '/') {
        return false;
    }

    size_t len = 0;
    for (const char* c = path; *c != '\0'; ++c) {
        len++;

        if (*c == '/') {
            const char* n = c + 1;
            while (*n == '.') {
                n++;
            }
            if (*n == '/' || *n == '\0') {
                return false;
            }
            continue;
        }

        if (validatePathChar(*c)) {
            continue;
        }

        return false;
    }

    // max length test
    if (len >= XR_MAX_PATH_LENGTH) {
        return false;
    }

    // valid path
    return true;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrStringToPath(XrInstance instance, const char* pathString, XrPath* path) {
    XrInstance_T* instancePtr = demoteFromHandle<XrInstance, XrInstance_T>(instance);

    {
        std::unique_lock<std::mutex> lock(instancePtr->pathsMutex);
        const auto& it = std::find_if(instancePtr->paths.begin(), instancePtr->paths.end(),
                                      [pathString](const auto& i) { return strcmp(i.second.c_str(), pathString) == 0; });
        if (it != instancePtr->paths.end()) {
            *path = it->first;
            return XR_SUCCESS;
        }

        if (!validatePath(pathString)) {
            return XR_ERROR_PATH_FORMAT_INVALID;
        }

        XrPath currentPath = instancePtr->lastPath;
        instancePtr->lastPath++;
        instancePtr->paths[currentPath] = pathString;
        *path = currentPath;
        return XR_SUCCESS;
    }
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrPathToString(XrInstance instance, XrPath path, uint32_t bufferCapacityInput,
                                                         uint32_t* bufferCountOutput, char* buffer) {
    XrInstance_T* instancePtr = demoteFromHandle<XrInstance, XrInstance_T>(instance);

    {
        std::unique_lock<std::mutex> lock(instancePtr->pathsMutex);
        const auto& it =
            std::find_if(instancePtr->paths.begin(), instancePtr->paths.end(), [path](const auto& i) { return i.first == path; });

        if (it != instancePtr->paths.end()) {
            std::string str = it->second;
            return ElementCapacityWrite(bufferCapacityInput, bufferCountOutput, buffer, str.data(), str.size() + 1);
        }

        return XR_ERROR_PATH_INVALID;
    }
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrCreateActionSet(XrInstance instance, const XrActionSetCreateInfo* createInfo,
                                                            XrActionSet* actionSet) {
    if (createInfo->actionSetName[0] == '\0') {
        return XR_ERROR_NAME_INVALID;
    }
    if (createInfo->localizedActionSetName[0] == '\0') {
        return XR_ERROR_LOCALIZED_NAME_INVALID;
    }

    for (const char* c = createInfo->actionSetName; *c != '\0'; ++c) {
        if (!validatePathChar(*c)) {
            return XR_ERROR_PATH_FORMAT_INVALID;
        }
    }

    XrInstance_T* instancePtr = demoteFromHandle<XrInstance, XrInstance_T>(instance);

    {
        std::unique_lock<std::mutex> lock(instancePtr->actionSetsMutex);
        for (const auto& existingActionSet : instancePtr->actionSets) {
            if (existingActionSet->actionSetName == createInfo->actionSetName) {
                return XR_ERROR_NAME_DUPLICATED;
            }
            if (existingActionSet->localizedActionSetName == createInfo->localizedActionSetName) {
                return XR_ERROR_LOCALIZED_NAME_DUPLICATED;
            }
        }
    }

    auto actionSetPtr = std::make_unique<XrActionSet_T>();
    actionSetPtr->instance = instance;
    actionSetPtr->actionSetName = createInfo->actionSetName;
    actionSetPtr->localizedActionSetName = createInfo->localizedActionSetName;

    *actionSet = promoteToHandle<XrActionSet, XrActionSet_T>(actionSetPtr.get());

    {
        std::unique_lock<std::mutex> lock(instancePtr->actionSetsMutex);
        instancePtr->actionSets.push_back(std::move(actionSetPtr));
    }

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrDestroyActionSet(XrActionSet actionSet) {
    {
        XrActionSet_T* actionSetPtr = demoteFromHandle<XrActionSet, XrActionSet_T>(actionSet);
        XrInstance_T* instancePtr = demoteFromHandle<XrInstance, XrInstance_T>(actionSetPtr->instance);
        std::unique_lock<std::mutex> lock(instancePtr->actionSetsMutex);
        const auto& it = std::find_if(instancePtr->actionSets.begin(), instancePtr->actionSets.end(),
                                      [actionSetPtr](const auto& i) { return actionSetPtr == i.get(); });
        instancePtr->actionSets.erase(it);
    }

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrCreateAction(XrActionSet actionSet, const XrActionCreateInfo* createInfo,
                                                         XrAction* action) {
    if (createInfo->actionName[0] == '\0') {
        return XR_ERROR_NAME_INVALID;
    }
    if (createInfo->localizedActionName[0] == '\0') {
        return XR_ERROR_LOCALIZED_NAME_INVALID;
    }

    for (const char* c = createInfo->actionName; *c != '\0'; ++c) {
        if (!validatePathChar(*c)) {
            return XR_ERROR_PATH_FORMAT_INVALID;
        }
    }

    XrActionSet_T* actionSetPtr = demoteFromHandle<XrActionSet, XrActionSet_T>(actionSet);

    if (actionSetPtr->hasBeenAttached) {
        return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
    }

    {
        std::unique_lock<std::mutex> lock(actionSetPtr->actionsMutex);
        for (const auto& existingActions : actionSetPtr->actions) {
            if (existingActions->actionName == createInfo->actionName) {
                return XR_ERROR_NAME_DUPLICATED;
            }
            if (existingActions->localizedActionName == createInfo->localizedActionName) {
                return XR_ERROR_LOCALIZED_NAME_DUPLICATED;
            }
        }
    }

    auto actionPtr = std::make_unique<XrAction_T>();
    actionPtr->actionSet = actionSet;
    actionPtr->actionName = createInfo->actionName;
    actionPtr->localizedActionName = createInfo->localizedActionName;
    actionPtr->actionType = createInfo->actionType;

    for (uint32_t i = 0; i < createInfo->countSubactionPaths; ++i) {
        // CTS requires duplicate path testing
        // This seems like something valuable to check but is not strictly required by the spec.
        if (std::find(actionPtr->subactionPaths.begin(), actionPtr->subactionPaths.end(), createInfo->subactionPaths[i]) !=
            actionPtr->subactionPaths.end()) {
            return XR_ERROR_PATH_UNSUPPORTED;
        }

        XrInstance_T* instancePtr = demoteFromHandle<XrInstance, XrInstance_T>(actionSetPtr->instance);

        std::vector<InteractionProfileMetadata> definitions;
        if (instancePtr->apiVersion < XR_MAKE_VERSION(1, 1, 0)) {
            definitions = cInteractionProfileDefinitions_1_0;
            if (std::find(instancePtr->enabledExtensions.begin(), instancePtr->enabledExtensions.end(),
                          XR_KHR_MAINTENANCE1_EXTENSION_NAME) != instancePtr->enabledExtensions.end()) {
                definitions = cInteractionProfileDefinitions_1_0_khr_maintenance1;
            }
        } else {
            // This is not strictly correct - but OpenXR 1.1 is a superset of maintenance1
            // so we are ok if OpenXR 1.1 and maintenance1 are both enabled.
            // https://gitlab.khronos.org/openxr/openxr/-/issues/2599
            definitions = cInteractionProfileDefinitions_1_1;
        }

        // CTS requires valid top level path from known IPs
        // This seems like something valuable to check but is not strictly required by the spec.
        bool validTLP = false;
        for (const auto& def : definitions) {
            for (const auto& tlp : def.TopLevelPaths) {
                XrPath path{XR_NULL_PATH};
                XrResult res = RuntimeTestXrStringToPath(actionSetPtr->instance, tlp.c_str(), &path);
                if (res != XR_SUCCESS) {
                    return res;
                }
                if (createInfo->subactionPaths[i] == path) {
                    validTLP = true;
                }
            }
        }
        if (!validTLP) {
            return XR_ERROR_PATH_UNSUPPORTED;
        }

        actionPtr->subactionPaths.push_back(createInfo->subactionPaths[i]);
    }

    *action = promoteToHandle<XrAction, XrAction_T>(actionPtr.get());

    {
        std::unique_lock<std::mutex> lock(actionSetPtr->actionsMutex);
        actionSetPtr->actions.push_back(std::move(actionPtr));
    }

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrDestroyAction(XrAction action) {
    {
        XrAction_T* actionPtr = demoteFromHandle<XrAction, XrAction_T>(action);
        XrActionSet_T* actionSetPtr = demoteFromHandle<XrActionSet, XrActionSet_T>(actionPtr->actionSet);
        std::unique_lock<std::mutex> lock(actionSetPtr->actionsMutex);
        const auto& it = std::find_if(actionSetPtr->actions.begin(), actionSetPtr->actions.end(),
                                      [actionPtr](const auto& i) { return actionPtr == i.get(); });
        actionSetPtr->actions.erase(it);
    }

    return XR_SUCCESS;
}

std::string pathToStdString(XrInstance instance, XrPath path) {
    std::array<char, XR_MAX_PATH_LENGTH> buffer{};
    auto count = static_cast<uint32_t>(buffer.size());
    // if we get a failure here we will return an empty string
    (void)RuntimeTestXrPathToString(instance, path, count, &count, buffer.data());
    return {buffer.data()};
}

XRAPI_ATTR XrResult XRAPI_CALL
RuntimeTestXrSuggestInteractionProfileBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings) {
    if (suggestedBindings->countSuggestedBindings == 0) {
        return XR_ERROR_VALIDATION_FAILURE;
    }

    std::string pathStr = pathToStdString(instance, suggestedBindings->interactionProfile);

    XrInstance_T* instancePtr = demoteFromHandle<XrInstance, XrInstance_T>(instance);

    std::vector<InteractionProfileMetadata> definitions;
    if (instancePtr->apiVersion < XR_MAKE_VERSION(1, 1, 0)) {
        definitions = cInteractionProfileDefinitions_1_0;
        if (std::find(instancePtr->enabledExtensions.begin(), instancePtr->enabledExtensions.end(),
                      XR_KHR_MAINTENANCE1_EXTENSION_NAME) != instancePtr->enabledExtensions.end()) {
            definitions = cInteractionProfileDefinitions_1_0_khr_maintenance1;
        }
    } else {
        definitions = cInteractionProfileDefinitions_1_1;
    }

    const auto& it = std::find_if(definitions.begin(), definitions.end(), [pathStr](const auto& i) {
        return strcmp(i.InteractionProfilePathString.c_str(), pathStr.c_str()) == 0;
    });

    if (it == definitions.end()) {
        return XR_ERROR_PATH_UNSUPPORTED;
    }

    for (uint32_t i = 0; i < suggestedBindings->countSuggestedBindings; ++i) {
        // is this supported for this IP?
        {
            XrPath bindingPath = suggestedBindings->suggestedBindings[i].binding;
            auto bindingStr = pathToStdString(instance, bindingPath);

            bool supportedForIP = false;
            for (const auto& supportedPath : it->InputSourcePaths) {
                constexpr std::array<const char*, 3> supportedSuffixes = {{
                    "/click",
                    "/value",
                    "/pose",
                }};

                for (const char* suffix : supportedSuffixes) {
                    size_t suffixLen = strlen(suffix);
                    size_t supportedPathLen = strlen(supportedPath);

                    // if (supportedPath.ends_with(suffix)) {...}
                    if (strcmp(supportedPath + supportedPathLen - suffixLen, suffix) == 0) {
                        // if (std::string_view(supportedPath, supportedPathLen - suffixLen) == bindingStr) {...}
                        if (strncmp(supportedPath, bindingStr.c_str(), supportedPathLen - suffixLen) == 0 &&
                            bindingStr.c_str()[supportedPathLen - suffixLen] == '\0') {
                            supportedForIP = true;
                            break;
                        }
                    }
                }

                if (strcmp(supportedPath, bindingStr.c_str()) == 0) {
                    supportedForIP = true;
                    break;
                }
            }

            if (!supportedForIP) {
                return XR_ERROR_PATH_UNSUPPORTED;
            }
        }

        // is actionset attached for any of the actions?
        {
            XrAction bindingAction = suggestedBindings->suggestedBindings[i].action;
            XrAction_T* actionPtr = demoteFromHandle<XrAction, XrAction_T>(bindingAction);

            XrActionSet_T* actionSetPtr = demoteFromHandle<XrActionSet, XrActionSet_T>(actionPtr->actionSet);
            if (actionSetPtr->hasBeenAttached) {
                return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
            }
        }
    }

    // Note: we do not actually use the suggestions; we just do the minimum to validate as
    // required by the spec.

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrAttachSessionActionSets(XrSession session,
                                                                    const XrSessionActionSetsAttachInfo* attachInfo) {
    if (attachInfo->countActionSets == 0) {
        return XR_ERROR_VALIDATION_FAILURE;
    }

    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);

    if (sessionPtr->actionsetsAttached) {
        return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
    }

    sessionPtr->actionsetsAttached = true;

    for (uint32_t i = 0; i < attachInfo->countActionSets; ++i) {
        sessionPtr->attachedActionSets.push_back(attachInfo->actionSets[i]);

        XrActionSet_T* actionSetPtr = demoteFromHandle<XrActionSet, XrActionSet_T>(attachInfo->actionSets[i]);
        actionSetPtr->hasBeenAttached = true;
    }

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrGetCurrentInteractionProfile(XrSession session, XrPath /*topLevelUserPath*/,
                                                                         XrInteractionProfileState* interactionProfile) {
    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);

    if (!sessionPtr->actionsetsAttached) {
        return XR_ERROR_ACTIONSET_NOT_ATTACHED;
    }

    interactionProfile->interactionProfile = XR_NULL_PATH;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrGetActionStateBoolean(XrSession session, const XrActionStateGetInfo* getInfo,
                                                                  XrActionStateBoolean* state) {
    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);

    if (!sessionPtr->actionsetsAttached) {
        return XR_ERROR_ACTIONSET_NOT_ATTACHED;
    }

    XrAction_T* actionPtr = demoteFromHandle<XrAction, XrAction_T>(getInfo->action);
    if (actionPtr->actionType != XR_ACTION_TYPE_BOOLEAN_INPUT) {
        return XR_ERROR_ACTION_TYPE_MISMATCH;
    }
    if (getInfo->subactionPath != XR_NULL_PATH) {
        if (std::find(actionPtr->subactionPaths.begin(), actionPtr->subactionPaths.end(), getInfo->subactionPath) ==
            actionPtr->subactionPaths.end()) {
            return XR_ERROR_PATH_UNSUPPORTED;
        }
    }

    state->isActive = XR_FALSE;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrGetActionStateFloat(XrSession session, const XrActionStateGetInfo* getInfo,
                                                                XrActionStateFloat* state) {
    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);

    if (!sessionPtr->actionsetsAttached) {
        return XR_ERROR_ACTIONSET_NOT_ATTACHED;
    }

    XrAction_T* actionPtr = demoteFromHandle<XrAction, XrAction_T>(getInfo->action);
    if (actionPtr->actionType != XR_ACTION_TYPE_FLOAT_INPUT) {
        return XR_ERROR_ACTION_TYPE_MISMATCH;
    }
    if (getInfo->subactionPath != XR_NULL_PATH) {
        if (std::find(actionPtr->subactionPaths.begin(), actionPtr->subactionPaths.end(), getInfo->subactionPath) ==
            actionPtr->subactionPaths.end()) {
            return XR_ERROR_PATH_UNSUPPORTED;
        }
    }

    state->isActive = XR_FALSE;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrGetActionStateVector2f(XrSession session, const XrActionStateGetInfo* getInfo,
                                                                   XrActionStateVector2f* state) {
    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);

    if (!sessionPtr->actionsetsAttached) {
        return XR_ERROR_ACTIONSET_NOT_ATTACHED;
    }

    XrAction_T* actionPtr = demoteFromHandle<XrAction, XrAction_T>(getInfo->action);
    if (actionPtr->actionType != XR_ACTION_TYPE_VECTOR2F_INPUT) {
        return XR_ERROR_ACTION_TYPE_MISMATCH;
    }
    if (getInfo->subactionPath != XR_NULL_PATH) {
        if (std::find(actionPtr->subactionPaths.begin(), actionPtr->subactionPaths.end(), getInfo->subactionPath) ==
            actionPtr->subactionPaths.end()) {
            return XR_ERROR_PATH_UNSUPPORTED;
        }
    }

    state->isActive = XR_FALSE;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrGetActionStatePose(XrSession session, const XrActionStateGetInfo* getInfo,
                                                               XrActionStatePose* state) {
    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);

    if (!sessionPtr->actionsetsAttached) {
        return XR_ERROR_ACTIONSET_NOT_ATTACHED;
    }

    XrAction_T* actionPtr = demoteFromHandle<XrAction, XrAction_T>(getInfo->action);
    if (actionPtr->actionType != XR_ACTION_TYPE_POSE_INPUT) {
        return XR_ERROR_ACTION_TYPE_MISMATCH;
    }
    if (getInfo->subactionPath != XR_NULL_PATH) {
        if (std::find(actionPtr->subactionPaths.begin(), actionPtr->subactionPaths.end(), getInfo->subactionPath) ==
            actionPtr->subactionPaths.end()) {
            return XR_ERROR_PATH_UNSUPPORTED;
        }
    }

    state->isActive = XR_FALSE;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrSyncActions(XrSession session, const XrActionsSyncInfo* /*syncInfo*/) {
    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);

    if (!sessionPtr->actionsetsAttached) {
        return XR_ERROR_ACTIONSET_NOT_ATTACHED;
    }

    if (sessionPtr->sessionState != XR_SESSION_STATE_FOCUSED) {
        return XR_SESSION_NOT_FOCUSED;
    }

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL
RuntimeTestXrEnumerateBoundSourcesForAction(XrSession session, const XrBoundSourcesForActionEnumerateInfo* /*enumerateInfo*/,
                                            uint32_t /*sourceCapacityInput*/, uint32_t* sourceCountOutput, XrPath* /*sources*/) {
    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);

    if (!sessionPtr->actionsetsAttached) {
        return XR_ERROR_ACTIONSET_NOT_ATTACHED;
    }

    *sourceCountOutput = 0;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrGetInputSourceLocalizedName(XrSession /*session*/,
                                                                        const XrInputSourceLocalizedNameGetInfo* /*getInfo*/,
                                                                        uint32_t /*bufferCapacityInput*/,
                                                                        uint32_t* /*bufferCountOutput*/, char* /*buffer*/) {
    return XR_ERROR_FEATURE_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrApplyHapticFeedback(XrSession session, const XrHapticActionInfo* hapticActionInfo,
                                                                const XrHapticBaseHeader* /*hapticFeedback*/) {
    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);

    if (!sessionPtr->actionsetsAttached) {
        return XR_ERROR_ACTIONSET_NOT_ATTACHED;
    }

    if (sessionPtr->sessionState != XR_SESSION_STATE_FOCUSED) {
        return XR_SESSION_NOT_FOCUSED;
    }

    XrAction_T* actionPtr = demoteFromHandle<XrAction, XrAction_T>(hapticActionInfo->action);
    if (actionPtr->actionType != XR_ACTION_TYPE_VIBRATION_OUTPUT) {
        return XR_ERROR_ACTION_TYPE_MISMATCH;
    }
    if (hapticActionInfo->subactionPath != XR_NULL_PATH) {
        if (std::find(actionPtr->subactionPaths.begin(), actionPtr->subactionPaths.end(), hapticActionInfo->subactionPath) ==
            actionPtr->subactionPaths.end()) {
            return XR_ERROR_PATH_UNSUPPORTED;
        }
    }

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrStopHapticFeedback(XrSession session, const XrHapticActionInfo* hapticActionInfo) {
    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);

    if (!sessionPtr->actionsetsAttached) {
        return XR_ERROR_ACTIONSET_NOT_ATTACHED;
    }

    if (sessionPtr->sessionState != XR_SESSION_STATE_FOCUSED) {
        return XR_SESSION_NOT_FOCUSED;
    }

    XrAction_T* actionPtr = demoteFromHandle<XrAction, XrAction_T>(hapticActionInfo->action);
    if (actionPtr->actionType != XR_ACTION_TYPE_VIBRATION_OUTPUT) {
        return XR_ERROR_ACTION_TYPE_MISMATCH;
    }
    if (hapticActionInfo->subactionPath != XR_NULL_PATH) {
        if (std::find(actionPtr->subactionPaths.begin(), actionPtr->subactionPaths.end(), hapticActionInfo->subactionPath) ==
            actionPtr->subactionPaths.end()) {
            return XR_ERROR_PATH_UNSUPPORTED;
        }
    }

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrCreateActionSpace(XrSession session, const XrActionSpaceCreateInfo* /*createInfo*/,
                                                              XrSpace* space) {
    // TODO CTS: https://gitlab.khronos.org/openxr/openxr/-/issues/2485#core-functions
    // if (!validateQuat(createInfo->poseInReferenceSpace.orientation)) {
    //  return XR_ERROR_POSE_INVALID;
    //}

    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);

    auto spacePtr = std::make_unique<XrSpace_T>();
    *space = promoteToHandle<XrSpace, XrSpace_T>(spacePtr.get());
    spacePtr->session = session;
    spacePtr->spaceType = XrSpace_T::SpaceType::ActionSpace;

    {
        std::unique_lock<std::mutex> lock(sessionPtr->spacesMutex);
        sessionPtr->spaces.push_back(std::move(spacePtr));
    }

    return XR_SUCCESS;
}

//
// Unimplemented graphics bindings functions
//
XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrWaitFrame(XrSession session, const XrFrameWaitInfo* /*frameWaitInfo*/,
                                                      XrFrameState* frameState) {
    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);
    if (sessionPtr->beginTime == 0) {
        return XR_ERROR_SESSION_NOT_RUNNING;
    }

    frameState->shouldRender = XR_FALSE;
    frameState->predictedDisplayTime = currentXrTime();
    frameState->predictedDisplayPeriod = (1.f / 60.f) * 1e9;  // 60 Hz
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrBeginFrame(XrSession /*session*/, const XrFrameBeginInfo* /*frameBeginInfo*/) {
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrEndFrame(XrSession session, const XrFrameEndInfo* /*frameEndInfo*/) {
    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);
    if (sessionPtr->beginTime != 0 && sessionPtr->endTime == 0 && sessionPtr->sessionState != XR_SESSION_STATE_FOCUSED) {
        switchSessionState(session, XR_SESSION_STATE_VISIBLE);
        switchSessionState(session, XR_SESSION_STATE_FOCUSED);
    }

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrLocateViews(XrSession session, const XrViewLocateInfo* viewLocateInfo,
                                                        XrViewState* /*viewState*/, uint32_t viewCapacityInput,
                                                        uint32_t* viewCountOutput, XrView* views) {
    XrSession_T* sessionPtr = demoteFromHandle<XrSession, XrSession_T>(session);

    if (viewLocateInfo->viewConfigurationType != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
        return viewConfigurationError(sessionPtr->instance, viewLocateInfo->viewConfigurationType);
    }
    if (viewLocateInfo->displayTime <= 0) {
        return XR_ERROR_TIME_INVALID;
    }

    std::array<XrView, 2> locatedViews{{XrView{XR_TYPE_VIEW}, XrView{XR_TYPE_VIEW}}};
    for (auto& locatedView : locatedViews) {
        locatedView.pose.orientation.w = 1.f;
        locatedView.fov.angleLeft = -1.f;
        locatedView.fov.angleRight = 1.f;
        locatedView.fov.angleUp = 1.f;
        locatedView.fov.angleDown = -1.f;
    };

    return XrElementCapacityWrite(viewCapacityInput, viewCountOutput, views, locatedViews.data(), locatedViews.size());
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrEnumerateViewConfigurations(XrInstance /*instance*/, XrSystemId /*systemId*/,
                                                                        uint32_t viewConfigurationTypeCapacityInput,
                                                                        uint32_t* viewConfigurationTypeCountOutput,
                                                                        XrViewConfigurationType* viewConfigurationTypes) {
    // Specification does not require systemId validation

    std::array<XrViewConfigurationType, 1> viewConfigs{{XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO}};
    return ElementCapacityWrite(viewConfigurationTypeCapacityInput, viewConfigurationTypeCountOutput, viewConfigurationTypes,
                                viewConfigs.data(), viewConfigs.size());
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrEnumerateViewConfigurationViews(XrInstance instance, XrSystemId /*systemId*/,
                                                                            XrViewConfigurationType viewConfigurationType,
                                                                            uint32_t viewCapacityInput, uint32_t* viewCountOutput,
                                                                            XrViewConfigurationView* views) {
    // Specification does not require systemId validation

    // viewConfigurationType not validated by CTS right now.
    // TODO CTS: https://gitlab.khronos.org/openxr/openxr/-/issues/2485#core-functions
    if (viewConfigurationType != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
        return viewConfigurationError(instance, viewConfigurationType);
    }

    XrViewConfigurationView dummyVcv{XR_TYPE_VIEW_CONFIGURATION_VIEW};
    dummyVcv.recommendedImageRectWidth = 1024;
    dummyVcv.maxImageRectWidth = 1024;
    dummyVcv.recommendedImageRectHeight = 1024;
    dummyVcv.maxImageRectHeight = 1024;
    dummyVcv.recommendedSwapchainSampleCount = 1;
    dummyVcv.maxSwapchainSampleCount = 4;

    const std::array<XrViewConfigurationView, 2> knownViews{{dummyVcv, dummyVcv}};
    return XrElementCapacityWrite(viewCapacityInput, viewCountOutput, views, knownViews.data(), knownViews.size());
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrGetViewConfigurationProperties(XrInstance instance, XrSystemId /*systemId*/,
                                                                           XrViewConfigurationType viewConfigurationType,
                                                                           XrViewConfigurationProperties* configurationProperties) {
    if (viewConfigurationType != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
        return viewConfigurationError(instance, viewConfigurationType);
    }
    configurationProperties->viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    configurationProperties->fovMutable = XR_FALSE;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrEnumerateSwapchainFormats(XrSession /*session*/, uint32_t formatCapacityInput,
                                                                      uint32_t* formatCountOutput, int64_t* formats) {
    std::array<int64_t, 0> knownFormats{};
    return ElementCapacityWrite(formatCapacityInput, formatCountOutput, formats, knownFormats.data(), knownFormats.size());
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrCreateSwapchain(XrSession /*session*/, const XrSwapchainCreateInfo* /*createInfo*/,
                                                            XrSwapchain* /*swapchain*/) {
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrDestroySwapchain(XrSwapchain /*swapchain*/) { return XR_ERROR_FUNCTION_UNSUPPORTED; }

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrEnumerateSwapchainImages(XrSwapchain /*swapchain*/, uint32_t /*imageCapacityInput*/,
                                                                     uint32_t* /*imageCountOutput*/,
                                                                     XrSwapchainImageBaseHeader* /*images*/) {
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrAcquireSwapchainImage(XrSwapchain /*swapchain*/,
                                                                  const XrSwapchainImageAcquireInfo* /*acquireInfo*/,
                                                                  uint32_t* /*index*/) {
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrWaitSwapchainImage(XrSwapchain /*swapchain*/,
                                                               const XrSwapchainImageWaitInfo* /*waitInfo*/) {
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrReleaseSwapchainImage(XrSwapchain /*swapchain*/,
                                                                  const XrSwapchainImageReleaseInfo* /*releaseInfo*/) {
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

//
// xrGetInstanceProcAddr
//
XRAPI_ATTR XrResult XRAPI_CALL RuntimeTestXrGetInstanceProcAddr(XrInstance instance, const char* name,
                                                                PFN_xrVoidFunction* function) {
    if (0 == strcmp(name, "xrGetInstanceProcAddr")) {
        *function = reinterpret_cast<PFN_xrVoidFunction>(RuntimeTestXrGetInstanceProcAddr);
        return XR_SUCCESS;
    } else if (0 == strcmp(name, "xrEnumerateInstanceExtensionProperties")) {
        *function = reinterpret_cast<PFN_xrVoidFunction>(RuntimeTestXrEnumerateInstanceExtensionProperties);
        return XR_SUCCESS;
    } else if (0 == strcmp(name, "xrEnumerateApiLayerProperties")) {
        *function = reinterpret_cast<PFN_xrVoidFunction>(RuntimeTestXrEnumerateApiLayerProperties);
        return XR_SUCCESS;
    } else if (0 == strcmp(name, "xrCreateInstance")) {
        *function = reinterpret_cast<PFN_xrVoidFunction>(RuntimeTestXrCreateInstance);
        return XR_SUCCESS;
    };

    if (instance == XR_NULL_HANDLE) {
        return XR_ERROR_HANDLE_INVALID;
    }

#define FUNCTIONINFO(functionName, _)                                                  \
    if (strcmp(name, "xr" #functionName) == 0) {                                       \
        *function = reinterpret_cast<PFN_xrVoidFunction>(RuntimeTestXr##functionName); \
        return XR_SUCCESS;                                                             \
    }
    XR_LIST_FUNCTIONS_XR_VERSION_1_0(FUNCTIONINFO)
#undef FUNCTIONINFO
#define FUNCTIONINFO(functionName, _)                                                      \
    XrInstance_T* instancePtr = demoteFromHandle<XrInstance, XrInstance_T>(instance);      \
    if (instancePtr->apiVersion >= XR_MAKE_VERSION(1, 1, 0)) {                             \
        if (strcmp(name, "xr" #functionName) == 0) {                                       \
            *function = reinterpret_cast<PFN_xrVoidFunction>(TestRuntimeXr##functionName); \
            return XR_SUCCESS;                                                             \
        }                                                                                  \
    }
    XR_LIST_FUNCTIONS_XR_VERSION_1_1(FUNCTIONINFO)
#undef FUNCTIONINFO

    *function = nullptr;
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

}  // namespace

extern "C" {

// forward decl
RUNTIME_EXPORT XRAPI_ATTR XrResult XRAPI_CALL xrNegotiateLoaderRuntimeInterface(const XrNegotiateLoaderInfo* loaderInfo,
                                                                                XrNegotiateRuntimeRequest* runtimeRequest);
RUNTIME_EXPORT XRAPI_ATTR XrResult XRAPI_CALL TestRuntimeAlwaysFailNegotiateLoaderRuntimeInterface(
    const XrNegotiateLoaderInfo* loaderInfo, XrNegotiateRuntimeRequest* runtimeRequest);
RUNTIME_EXPORT XRAPI_ATTR XrResult XRAPI_CALL TestRuntimeNullGipaNegotiateLoaderRuntimeInterface(
    const XrNegotiateLoaderInfo* loaderInfo, XrNegotiateRuntimeRequest* runtimeRequest);

RUNTIME_EXPORT XRAPI_ATTR XrResult XRAPI_CALL TestRuntimeInvalidInterfaceNegotiateLoaderRuntimeInterface(
    const XrNegotiateLoaderInfo* loaderInfo, XrNegotiateRuntimeRequest* runtimeRequest);
RUNTIME_EXPORT XRAPI_ATTR XrResult XRAPI_CALL TestRuntimeInvalidApiNegotiateLoaderRuntimeInterface(
    const XrNegotiateLoaderInfo* loaderInfo, XrNegotiateRuntimeRequest* runtimeRequest);

// Function used to negotiate an interface betewen the loader and a runtime.
RUNTIME_EXPORT XRAPI_ATTR XrResult XRAPI_CALL xrNegotiateLoaderRuntimeInterface(const XrNegotiateLoaderInfo* loaderInfo,
                                                                                XrNegotiateRuntimeRequest* runtimeRequest) {
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
    const XrNegotiateLoaderInfo* /* loaderInfo */, XrNegotiateRuntimeRequest* /* runtimeRequest */) {
    return XR_ERROR_INITIALIZATION_FAILED;
}

// Pass, but return NULL for the runtime's xrGetInstanceProcAddr
RUNTIME_EXPORT XRAPI_ATTR XrResult XRAPI_CALL TestRuntimeNullGipaNegotiateLoaderRuntimeInterface(
    const XrNegotiateLoaderInfo* loaderInfo, XrNegotiateRuntimeRequest* runtimeRequest) {
    auto result = xrNegotiateLoaderRuntimeInterface(loaderInfo, runtimeRequest);
    if (result == XR_SUCCESS) {
        runtimeRequest->getInstanceProcAddr = nullptr;
    }

    return result;
}

// Pass, but return invalid interface version
RUNTIME_EXPORT XRAPI_ATTR XrResult XRAPI_CALL TestRuntimeInvalidInterfaceNegotiateLoaderRuntimeInterface(
    const XrNegotiateLoaderInfo* loaderInfo, XrNegotiateRuntimeRequest* runtimeRequest) {
    auto result = xrNegotiateLoaderRuntimeInterface(loaderInfo, runtimeRequest);
    if (result == XR_SUCCESS) {
        runtimeRequest->runtimeInterfaceVersion = 0;
    }

    return result;
}

// Pass, but return invalid API version
RUNTIME_EXPORT XRAPI_ATTR XrResult XRAPI_CALL TestRuntimeInvalidApiNegotiateLoaderRuntimeInterface(
    const XrNegotiateLoaderInfo* loaderInfo, XrNegotiateRuntimeRequest* runtimeRequest) {
    auto result = xrNegotiateLoaderRuntimeInterface(loaderInfo, runtimeRequest);
    if (result == XR_SUCCESS) {
        runtimeRequest->runtimeApiVersion = 0;
    }

    return result;
}

}  // extern "C"
