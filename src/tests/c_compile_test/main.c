// Copyright (c) 2017-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#ifdef XR_USE_PLATFORM_WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif  // !WIN32_LEAN_AND_MEAN

#if defined(_MSC_VER)
#pragma warning(disable : 4201)
#pragma warning(disable : 4204)
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif  // !NOMINMAX

#include <windows.h>
#include <unknwn.h>
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

#ifdef XR_USE_GRAPHICS_API_VULKAN
#ifdef XR_USE_PLATFORM_WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#ifdef XR_USE_PLATFORM_ANDROID
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#include <vulkan/vulkan.h>
#endif

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

#ifdef XR_USE_TIMESPEC
#include <time.h>
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_reflection.h>
#include <openxr/openxr_platform_defines.h>
#include <xr_linear.h>

#if !defined(ANDROID)
// this is just a test that our SDK headers compile with the C compiler
int main(int argc, const char** argv) {
    (void)argc;
    (void)argv;

    return 0;
}
#else   // defined(ANDROID)
static void app_handle_cmd(struct android_app* app, int32_t cmd) {
    (void)app;
    (void)cmd;
}

void android_main(struct android_app* app) {
    JNIEnv* Env;

    JavaVM* java_vm = app->activity->vm;

    (*java_vm)->AttachCurrentThread(java_vm, &Env, NULL);

    app->userData = NULL;
    app->onAppCmd = app_handle_cmd;

    // empty implementation

    ANativeActivity_finish(app->activity);
    (*java_vm)->DetachCurrentThread(java_vm);
}
#endif  // !defined(ANDROID)
