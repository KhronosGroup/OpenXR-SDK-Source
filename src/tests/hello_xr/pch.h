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
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/permission_manager.h>
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

// OVR SDK headers (found in $ENV{OCULUS_OPENXR_MOBILE_SDK}/OpenXR/Include)
#include <openxr/fb_body_tracking.h>
#include <openxr/fb_composition_layer_depth_test.h>
#include <openxr/fb_eye_tracking_social.h>
#include <openxr/fb_face_tracking.h>
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
#include <openxr/meta_foveation_eye_tracked.h>
#include <openxr/meta_local_dimming.h>
#include <openxr/openxr_extension_helpers.h>
#include <openxr/openxr_oculus.h>

#define USE_THUMBSTICKS_FOR_SMOOTH_LOCOMOTION 1
#include "utils.h"
