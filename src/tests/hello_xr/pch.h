// Copyright (c) 2017-2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <exception>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include <time.h>
#include <string.h>

//
// Platform Headers
//
#ifdef XR_USE_PLATFORM_WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif  // !WIN32_LEAN_AND_MEAN

#ifndef NOMINMAX
#define NOMINMAX
#endif  // !NOMINMAX

#include <windows.h>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr
#endif

#ifdef XR_USE_PLATFORM_ANDROID
#include <android/log.h>
#include <android_native_app_glue.h>
#include <android/native_window.h>
#include <jni.h>
#include <sys/system_properties.h>
#endif

#ifdef XR_USE_PLATFORM_WAYLAND
#include "wayland-client.h"
#endif

#ifdef XR_USE_PLATFORM_XLIB
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#ifdef XR_USE_PLATFORM_XCB
#include <xcb/xcb.h>
#endif

//
// Graphics Headers
//
#ifdef XR_USE_GRAPHICS_API_D3D11
#include <d3d11_4.h>
#endif

#ifdef XR_USE_GRAPHICS_API_D3D12
#include <d3d12.h>
#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL
#if defined(XR_USE_PLATFORM_XLIB) || defined(XR_USE_PLATFORM_XCB)
#include <GL/glx.h>
#endif
#ifdef XR_USE_PLATFORM_XCB
#include <xcb/glx.h>
#endif
#ifdef XR_USE_PLATFORM_WIN32
#include <wingdi.h>  // For HGLRC
#include <GL/gl.h>
#endif
#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
#include <EGL/egl.h>
#endif

#ifdef XR_USE_PLATFORM_EGL
#include <EGL/egl.h>
#endif  // XR_USE_PLATFORM_EGL

#ifdef XR_USE_GRAPHICS_API_VULKAN
#ifdef XR_USE_PLATFORM_WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#ifdef XR_USE_PLATFORM_ANDROID
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#include <vulkan/vulkan.h>
#endif

//
// OpenXR Headers
//
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_reflection.h>

#define ADD_EXTRA_CUBES 1

#ifndef ENABLE_ADVANCED_FEATURES
#define ENABLE_ADVANCED_FEATURES 1
#endif

#if ENABLE_ADVANCED_FEATURES

#include <openxr/extx1_event_channel.h>
#include <openxr/fb_composition_layer_depth_test.h>
#include <openxr/fb_eye_tracking_social.h>
#include <openxr/fb_face_tracking2.h>

#include <openxr/fb_haptic_amplitude_envelope.h>
#include <openxr/fb_haptic_pcm.h>
#include <openxr/fb_scene.h>
#include <openxr/fb_scene_capture.h>
#include <openxr/fb_spatial_entity.h>
#include <openxr/fb_spatial_entity_container.h>
#include <openxr/fb_spatial_entity_query.h>
#include <openxr/fb_spatial_entity_sharing.h>
#include <openxr/fb_spatial_entity_storage.h>
#include <openxr/fb_spatial_entity_storage_batch.h>
#include <openxr/fb_spatial_entity_user.h>
#include <openxr/fb_touch_controller_pro.h>
#include <openxr/fb_touch_controller_proximity.h>
#include <openxr/metax1_hand_tracking_microgestures.h>
#include <openxr/metax1_simultaneous_hands_controllers_management.h>
#include <openxr/metax2_detached_controllers.h>
#include <openxr/metax2_environment_depth.h>
#include <openxr/meta_automatic_layer_filter.h>
#include <openxr/meta_body_tracking_calibration.h>
#include <openxr/meta_body_tracking_fidelity.h>
#include <openxr/meta_body_tracking_full_body.h>
#include <openxr/meta_environment_depth.h>
#include <openxr/meta_foveation_eye_tracked.h>
#include <openxr/meta_local_dimming.h>
#include <openxr/meta_recommended_layer_resolution.h>
#include <openxr/meta_spatial_entity_mesh.h>
#include <openxr/meta_touch_controller_plus.h>
#include <openxr/openxr_extension_helpers.h>
//#include <openxr/openxr_oculus_helpers.h>

#ifdef XR_USE_PLATFORM_ANDROID
#define PLATFORM_ANDROID 1
#define PLATFORM_PC 0
#elif XR_USE_PLATFORM_WIN32
#define PLATFORM_ANDROID 0
#define PLATFORM_PC 1
#endif

#define ENABLE_STREAMLINE (PLATFORM_PC && 0)

#define ENABLE_OPENXR_FB_REFRESH_RATE 1
#define DEFAULT_REFRESH_RATE 90.0f
#define DESIRED_REFRESH_RATE 90.0f

#define ENABLE_OPENXR_FB_RENDER_MODEL 0

#define ENABLE_OPENXR_FB_SHARPENING (PLATFORM_ANDROID && 1) // Only works on standalone Android builds, not PC / Link
#define TOGGLE_SHARPENING_AT_RUNTIME_USING_RIGHT_GRIP (ENABLE_OPENXR_FB_SHARPENING && 0) // for debugging / comparison
#define ENABLE_OPENXR_FB_COMPOSITION_LAYER_SETTINGS ENABLE_OPENXR_FB_SHARPENING

// This is pointless, as Quest Pro supports LD as a platform-wide system setting. Just use that.
#define ENABLE_OPENXR_FB_LOCAL_DIMMING (PLATFORM_ANDROID && 0)

// Eye tracking only enabled on PC for now (needs permissions on Android, requires java calls. TODO)
#define ENABLE_OPENXR_FB_EYE_TRACKING_SOCIAL (PLATFORM_PC && 1) 
#define ENABLE_EXT_EYE_TRACKING (PLATFORM_PC && 1)
#define ENABLE_EYE_TRACKING (ENABLE_OPENXR_FB_EYE_TRACKING_SOCIAL || ENABLE_EXT_EYE_TRACKING)

#define DRAW_LOCAL_EYE_LASERS (ENABLE_EYE_TRACKING && 1)
#define DRAW_WORLD_EYE_LASERS (ENABLE_EYE_TRACKING && USE_THUMBSTICKS_FOR_SMOOTH_LOCOMOTION && 1)

#define ENABLE_OPENXR_META_FOVEATION_EYE_TRACKED (PLATFORM_PC && 0)

// Face tracking (not implemented yet)
#define ENABLE_OPENXR_FB_FACE_TRACKING 0

#define ENABLE_OPENXR_HAND_TRACKING 0
#define ENABLE_OPENXR_FB_BODY_TRACKING 1 // Hand tracking is redundant if you have body tracking, which includes all the same finger joints
#define ENABLE_OPENXR_FB_SIMULTEANEOUS_HANDS_AND_CONTROLLERS ((ENABLE_OPENXR_FB_BODY_TRACKING || ENABLE_OPENXR_HAND_TRACKING) && 1)

#define ENABLE_QUAD_LAYER 0

#define USE_WAIST_ORIENTATION_FOR_STICK_DIRECTION (ENABLE_OPENXR_FB_BODY_TRACKING && USE_THUMBSTICKS_FOR_SMOOTH_LOCOMOTION && 1)
#define DRAW_LOCAL_BODY_JOINTS (ENABLE_OPENXR_FB_BODY_TRACKING && 1)
#define DRAW_WORLD_BODY_JOINTS (ENABLE_OPENXR_FB_BODY_TRACKING && USE_THUMBSTICKS_FOR_SMOOTH_LOCOMOTION && 1)

#define USE_DUAL_LAYERS 0

#define LOG_EYE_TRACKING_DATA (ENABLE_EYE_TRACKING && 0)
#define LOG_BODY_TRACKING_DATA (ENABLE_OPENXR_FB_BODY_TRACKING && 0)

#define USE_SDL_JOYSTICKS 0

#define USE_THUMBSTICKS_FOR_SMOOTH_LOCOMOTION 1
#define PREFER_SNAP_TURNING 1
#define SNAP_TURN_DEGREES 30.0f

#define ADD_GRIP_POSE 1
#define DRAW_GRIP_POSE (ADD_GRIP_POSE && 0)

#define ADD_AIM_POSE 1
#define DRAW_AIM_POSE (ADD_AIM_POSE && 1)

#define BODY_CUBE_SIZE 0.02f
#define ADD_GROUND 1

#define SUPPORT_SCREENSHOTS 0
#define TAKE_SCREENSHOT_WITH_LEFT_GRAB (SUPPORT_SCREENSHOTS && 0)
#define ENABLE_LOCAL_DIMMING_WITH_RIGHT_GRAB (ENABLE_OPENXR_FB_LOCAL_DIMMING && 0)
#define LOG_IPD 0
#define LOG_FOV 0
#define LOG_MATRICES 0

#include "utils.h"
#endif
