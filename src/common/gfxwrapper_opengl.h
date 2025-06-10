// Copyright (c) 2017-2025 The Khronos Group Inc.
// Copyright (c) 2016, Oculus VR, LLC.
// Portions of macOS, iOS, functionality copyright (c) 2016, The Brenwill Workshop Ltd.
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

/* REUSE-IgnoreStart */
/* The following has copyright notices that duplicate the header above */

/*
================================================================================================

Description  : Convenient wrapper for the OpenGL API.
Orig. Author : J.M.P. van Waveren
Orig. Date   : 12/21/2014
Language     : C99
Copyright    : Copyright (c) 2016 Oculus VR, LLC. All Rights reserved.
             : Portions copyright (c) 2016 The Brenwill Workshop Ltd. All Rights reserved.

IMPLEMENTATION
==============

The code is written in an object-oriented style with a focus on minimizing state
and side effects. The majority of the functions manipulate self-contained objects
without modifying any global state (except for OpenGL state). The types
introduced in this file have no circular dependencies, and there are no forward
declarations.

Even though an object-oriented style is used, the code is written in straight C99 for
maximum portability and readability. To further improve portability and to simplify
compilation, all source code is in a single file without any dependencies on third-
party code or non-standard libraries. The code does not use an OpenGL loading library
like GLEE, GLEW, GL3W, or an OpenGL toolkit like GLUT, FreeGLUT, GLFW, etc. Instead,
the code provides direct access to window and context creation for driver extension work.

The code is written against version 4.3 of the Core Profile OpenGL Specification,
and version 3.1 of the OpenGL ES Specification.

Supported platforms are:

        - Microsoft Windows 7 or later
        - Ubuntu Linux 14.04 or later
        - Apple macOS 10.11 or later
        - Apple iOS 9.0 or later
        - Android 5.0 or later


GRAPHICS API WRAPPER
====================

The code wraps the OpenGL API with a convenient wrapper that takes care of a
lot of the OpenGL intricacies. This wrapper does not expose the full OpenGL API
but can be easily extended to support more features. Some of the current
limitations are:

- The wrapper is setup for forward rendering with a single render pass. This
  can be easily extended if more complex rendering algorithms are desired.

- A pipeline can only use 256 bytes worth of plain integer and floating-point
  uniforms, including vectors and matrices. If more uniforms are needed then
  it is advised to use a uniform buffer, which is the preferred approach for
  exposing large amounts of data anyway.

- Graphics programs currently consist of only a vertex and fragment shader.
  This can be easily extended if there is a need for geometry shaders etc.


KNOWN ISSUES
============

OS     : Apple Mac OS X 10.9.5
GPU    : Geforce GT 750M
DRIVER : NVIDIA 310.40.55b01
-----------------------------------------------
- glGetQueryObjectui64v( query, GL_QUERY_RESULT, &time ) always returns zero for a timer query.
- glFlush() after a glFenceSync() stalls the CPU for many milliseconds.
- Creating a context fails when the share context is current on another thread.

OS     : Android 6.0.1
GPU    : Adreno (TM) 530
DRIVER : OpenGL ES 3.1 V@145.0
-----------------------------------------------
- Enabling OVR_multiview hangs the GPU.


WORK ITEMS
==========

- Implement WGL, GLX and NSOpenGL equivalents of EGL_IMG_context_priority.
- Implement an extension that provides accurate display refresh timing (WGL_NV_delay_before_swap, D3DKMTGetScanLine).
- Implement an OpenGL extension that allows rendering directly to the front buffer.
- Implement an OpenGL extension that allows a compute shader to directly write to the front/back buffer images
(WGL_AMDX_drawable_view).
- Improve GPU task switching granularity.

================================================================================================
*/

#if !defined(KSGRAPHICSWRAPPER_OPENGL_H)
#define KSGRAPHICSWRAPPER_OPENGL_H

#include <glad/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#define OS_WINDOWS
#elif defined(__ANDROID__)
#define OS_ANDROID
#elif defined(__APPLE__)
#define OS_APPLE
#include <Availability.h>
#if defined(__IPHONE_OS_VERSION_MAX_ALLOWED) && __IPHONE_OS_VERSION_MAX_ALLOWED
#define OS_APPLE_IOS
#elif defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED
#define OS_APPLE_MACOS
#endif
#elif defined(__linux__)
#define OS_LINUX
#else
#error "unknown platform"
#endif

/*
================================
Platform headers / declarations
================================
*/

#if defined(OS_WINDOWS)

#define XR_USE_PLATFORM_WIN32 1

#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#if defined(_MSC_VER)
#pragma warning(disable : 4204)  // nonstandard extension used : non-constant aggregate initializer
#pragma warning(disable : 4221)  // nonstandard extension used: 'layers': cannot be initialized using address of automatic variable
                                 // 'layerProjection'
#pragma warning(disable : 4255)  // '<name>' : no function prototype given: converting '()' to '(void)'
#pragma warning(disable : 4668)  // '__cplusplus' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif'
#pragma warning(disable : 4710)  // 'int printf(const char *const ,...)': function not inlined
#pragma warning(disable : 4711)  // function '<name>' selected for automatic inline expansion
#pragma warning(disable : 4738)  // storing 32-bit float result in memory, possible loss of performance
#pragma warning(disable : 4820)  // '<name>' : 'X' bytes padding added after data member '<member>'
#pragma warning(disable : 4505)  // unreferenced local function has been removed
#endif

#if _MSC_VER >= 1900
#pragma warning(disable : 4464)  // relative include path contains '..'
#pragma warning(disable : 4774)  // 'printf' : format string expected in argument 1 is not a string literal
#endif

#define OPENGL_VERSION_MAJOR 4
#define OPENGL_VERSION_MINOR 3
#define GLSL_VERSION "430"
#define SPIRV_VERSION "99"
#define USE_SYNC_OBJECT 0  // 0 = GLsync, 1 = EGLSyncKHR, 2 = storage buffer

#include <windows.h>
#include <glad/wgl.h>

#define GRAPHICS_API_OPENGL 1
#define OUTPUT_PATH ""

#elif defined(OS_LINUX)

#define OPENGL_VERSION_MAJOR 4
#define OPENGL_VERSION_MINOR 5
#define GLSL_VERSION "430"
#define SPIRV_VERSION "99"
#define USE_SYNC_OBJECT 0  // 0 = GLsync, 1 = EGLSyncKHR, 2 = storage buffer

#if !defined(_XOPEN_SOURCE)
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif
#endif

#include <time.h>      // for timespec
#include <sys/time.h>  // for gettimeofday()
#if !defined(__USE_UNIX98)
#define __USE_UNIX98 1  // for pthread_mutexattr_settype
#endif
#include <pthread.h>  // for pthread_create() etc.
#include <malloc.h>   // for memalign

#if defined(OS_LINUX_XLIB)
#define XR_USE_PLATFORM_XLIB 1

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/xf86vmode.h>  // for fullscreen video mode
#include <X11/extensions/Xrandr.h>     // for resolution changes
#include <glad/glx.h>

#ifdef Success
#undef Success
#endif  // Success

#ifdef Always
#undef Always
#endif  // Always

#ifdef None
#undef None
#endif  // None

#elif defined(OS_LINUX_XCB) || defined(OS_LINUX_XCB_GLX)
#define XR_USE_PLATFORM_XCB 1

#include <X11/keysym.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>
#include <xcb/randr.h>
#include <xcb/glx.h>
#include <xcb/dri2.h>
#include <glad/glx.h>

#ifdef Success
#undef Success
#endif  // Success

#ifdef Always
#undef Always
#endif  // Always

#ifdef None
#undef None
#endif  // None

#elif defined(OS_LINUX_WAYLAND)
#define XR_USE_PLATFORM_WAYLAND 1

#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <wayland-egl.h>
#include <glad/egl.h>
#include <linux/input.h>
#include <poll.h>
#include <unistd.h>
#include "xdg-shell-unstable-v6.h"

#endif

#define GRAPHICS_API_OPENGL 1
#define OUTPUT_PATH ""

#elif defined(OS_APPLE_MACOS)

// Apple is still at OpenGL 4.1
#define OPENGL_VERSION_MAJOR 4
#define OPENGL_VERSION_MINOR 1
#define GLSL_VERSION "410"
#define SPIRV_VERSION "99"
#define USE_SYNC_OBJECT 0  // 0 = GLsync, 1 = EGLSyncKHR, 2 = storage buffer
#define XR_USE_PLATFORM_MACOS 1

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <pthread.h>
#include <ApplicationServices/ApplicationServices.h>
#if defined(__OBJC__)
#import <Cocoa/Cocoa.h>
#endif

#include <glad/gl.h>

#undef MAX
#undef MIN

#define GRAPHICS_API_OPENGL 1
#define OUTPUT_PATH ""

// Undocumented CGS and CGL
typedef void *CGSConnectionID;
typedef int CGSWindowID;
typedef int CGSSurfaceID;

CGLError CGLSetSurface(CGLContextObj ctx, CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid);
CGLError CGLGetSurface(CGLContextObj ctx, CGSConnectionID *cid, CGSWindowID *wid, CGSSurfaceID *sid);
CGLError CGLUpdateContext(CGLContextObj ctx);

#elif defined(OS_APPLE_IOS)

// Assume iOS 7+ which is GLES 3.0
#define OPENGL_VERSION_MAJOR 3
#define OPENGL_VERSION_MINOR 0
#define GLSL_VERSION "300 es"
#define SPIRV_VERSION "99"
#define USE_SYNC_OBJECT 0  // 0 = GLsync, 1 = EGLSyncKHR, 2 = storage buffer
#define XR_USE_PLATFORM_IOS 1

#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#include <glad/gl.h>
#include <sys/sysctl.h>

#define GRAPHICS_API_OPENGL_ES 1

#elif defined(OS_ANDROID)

#define OPENGL_VERSION_MAJOR 3
#define OPENGL_VERSION_MINOR 2
#define GLSL_VERSION "320 es"
#define SPIRV_VERSION "99"
#define USE_SYNC_OBJECT 1  // 0 = GLsync, 1 = EGLSyncKHR, 2 = storage buffer

#include <time.h>
#include <unistd.h>
#include <dirent.h>  // for opendir/closedir
#include <pthread.h>
#include <malloc.h>                     // for memalign
#include <dlfcn.h>                      // for dlopen
#include <sys/prctl.h>                  // for prctl( PR_SET_NAME )
#include <sys/stat.h>                   // for gettid
#include <sys/syscall.h>                // for syscall
#include <android/log.h>                // for __android_log_print
#include <android/input.h>              // for AKEYCODE_ etc.
#include <android/window.h>             // for AWINDOW_FLAG_KEEP_SCREEN_ON
#include <android/native_window_jni.h>  // for native window JNI
#include <glad/egl.h>

#define GRAPHICS_API_OPENGL_ES 1
#define OUTPUT_PATH "/sdcard/"

typedef struct {
    JavaVM *vm;        // Java Virtual Machine
    JNIEnv *env;       // Thread specific environment
    jobject activity;  // Java activity object
} Java_t;

#endif

/*
================================
Common headers
================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <string.h>  // for memset
#include <errno.h>   // for EBUSY, ETIMEDOUT etc.
#include <ctype.h>   // for isspace, isdigit

/*
================================
Common defines
================================
*/

#define UNUSED_PARM(x) \
    { (void)(x); }
#define ARRAY_SIZE(a) (sizeof((a)) / sizeof((a)[0]))
#define OFFSETOF_MEMBER(type, member) (size_t) & ((type *)0)->member
#define SIZEOF_MEMBER(type, member) sizeof(((type *)0)->member)
#define BIT(x) (1 << (x))
#ifndef MAX
#define MAX(x, y) ((x > y) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x, y) ((x < y) ? (x) : (y))
#endif
#define CLAMP(x, min, max) (((x) < (min)) ? (min) : (((x) > (max)) ? (max) : (x)))
#define STRINGIFY_EXPANDED(a) #a
#define STRINGIFY(a) STRINGIFY_EXPANDED(a)

#define APPLICATION_NAME "OpenGL SI"
#define WINDOW_TITLE "OpenGL SI"

#define PROGRAM(name) name##GLSL

#define GLSL_EXTENSIONS "#extension GL_EXT_shader_io_blocks : enable\n"
#define GL_FINISH_SYNC 1

#if defined(OS_ANDROID)
#define ES_HIGHP "highp"  // GLSL "310 es" requires a precision qualifier on a image2D
#else
#define ES_HIGHP ""  // GLSL "430" disallows a precision qualifier on a image2D
#endif

/*
================================================================================================================================

Driver Instance.

ksDriverInstance

bool ksDriverInstance_Create( ksDriverInstance * instance );
void ksDriverInstance_Destroy( ksDriverInstance * instance );

================================================================================================================================
*/

typedef struct {
    int placeholder;
} ksDriverInstance;

bool ksDriverInstance_Create(ksDriverInstance *instance);
void ksDriverInstance_Destroy(ksDriverInstance *instance);

/*
================================================================================================================================

GPU device.

ksGpuQueueProperty
ksGpuQueuePriority
ksGpuQueueInfo
ksGpuDevice

bool ksGpuDevice_Create( ksGpuDevice * device, ksDriverInstance * instance, const ksGpuQueueInfo * queueInfo );
void ksGpuDevice_Destroy( ksGpuDevice * device );

================================================================================================================================
*/

typedef enum {
    KS_GPU_QUEUE_PROPERTY_GRAPHICS = BIT(0),
    KS_GPU_QUEUE_PROPERTY_COMPUTE = BIT(1),
    KS_GPU_QUEUE_PROPERTY_TRANSFER = BIT(2)
} ksGpuQueueProperty;

typedef enum { KS_GPU_QUEUE_PRIORITY_LOW, KS_GPU_QUEUE_PRIORITY_MEDIUM, KS_GPU_QUEUE_PRIORITY_HIGH } ksGpuQueuePriority;

#define MAX_QUEUES 16

typedef struct {
    int queueCount;                                  // number of queues
    ksGpuQueueProperty queueProperties;              // desired queue family properties
    ksGpuQueuePriority queuePriorities[MAX_QUEUES];  // individual queue priorities
} ksGpuQueueInfo;

typedef struct {
    ksDriverInstance *instance;
    ksGpuQueueInfo queueInfo;
} ksGpuDevice;

bool ksGpuDevice_Create(ksGpuDevice *device, ksDriverInstance *instance, const ksGpuQueueInfo *queueInfo);
void ksGpuDevice_Destroy(ksGpuDevice *device);

/*
================================================================================================================================

GPU context.

A context encapsulates a queue that is used to submit command buffers.
A context can only be used by a single thread.
For optimal performance a context should only be created at load time, not at runtime.

ksGpuContext
ksGpuSurfaceColorFormat
ksGpuSurfaceDepthFormat
ksGpuSampleCount

bool ksGpuContext_CreateShared( ksGpuContext * context, const ksGpuContext * other, const int queueIndex );
void ksGpuContext_Destroy( ksGpuContext * context );
void ksGpuContext_SetCurrent( ksGpuContext * context );
void ksGpuContext_UnsetCurrent( ksGpuContext * context );
bool ksGpuContext_CheckCurrent( ksGpuContext * context );

bool ksGpuContext_CreateForSurface( ksGpuContext * context, const ksGpuDevice * device, const int queueIndex,
                                                                                const ksGpuSurfaceColorFormat colorFormat,
                                                                                const ksGpuSurfaceDepthFormat depthFormat,
                                                                                const ksGpuSampleCount sampleCount,
                                                                                ... );

================================================================================================================================
*/

typedef enum {
    KS_GPU_SURFACE_COLOR_FORMAT_R5G6B5,
    KS_GPU_SURFACE_COLOR_FORMAT_B5G6R5,
    KS_GPU_SURFACE_COLOR_FORMAT_R8G8B8A8,
    KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8,
    KS_GPU_SURFACE_COLOR_FORMAT_MAX
} ksGpuSurfaceColorFormat;

typedef enum {
    KS_GPU_SURFACE_DEPTH_FORMAT_NONE,
    KS_GPU_SURFACE_DEPTH_FORMAT_D16,
    KS_GPU_SURFACE_DEPTH_FORMAT_D24,
    KS_GPU_SURFACE_DEPTH_FORMAT_MAX
} ksGpuSurfaceDepthFormat;

typedef enum {
    KS_GPU_SAMPLE_COUNT_1 = 1,
    KS_GPU_SAMPLE_COUNT_2 = 2,
    KS_GPU_SAMPLE_COUNT_4 = 4,
    KS_GPU_SAMPLE_COUNT_8 = 8,
    KS_GPU_SAMPLE_COUNT_16 = 16,
    KS_GPU_SAMPLE_COUNT_32 = 32,
    KS_GPU_SAMPLE_COUNT_64 = 64,
} ksGpuSampleCount;

typedef struct ksGpuLimits {
    size_t maxPushConstantsSize;
    int maxSamples;
} ksGpuLimits;

typedef struct {
    const ksGpuDevice *device;
#if defined(OS_WINDOWS)
    HDC hDC;
    HGLRC hGLRC;
#elif defined(OS_LINUX_XLIB) || defined(OS_LINUX_XCB_GLX)
    Display *xDisplay;
    uint32_t visualid;
    GLXFBConfig glxFBConfig;
    GLXDrawable glxDrawable;
    GLXContext glxContext;
#elif defined(OS_LINUX_XCB)
    xcb_connection_t *connection;
    uint32_t screen_number;
    xcb_glx_fbconfig_t fbconfigid;
    xcb_visualid_t visualid;
    xcb_glx_drawable_t glxDrawable;
    xcb_glx_context_t glxContext;
    xcb_glx_context_tag_t glxContextTag;
#elif defined(OS_LINUX_WAYLAND)
    EGLNativeWindowType native_window;
    EGLDisplay display;
    EGLContext context;
    EGLConfig config;
    EGLSurface mainSurface;
#elif defined(OS_APPLE_MACOS)
#if defined(__OBJC__)
    NSOpenGLContext *nsContext;
#else
    void *nsContext;
#endif  // defined(__OBJC__)
    CGLContextObj cglContext;
#elif defined(OS_ANDROID)
    EGLDisplay display;
    EGLConfig config;
    EGLSurface tinySurface;
    EGLSurface mainSurface;
    EGLContext context;
#endif
} ksGpuContext;

typedef struct {
    unsigned char redBits;
    unsigned char greenBits;
    unsigned char blueBits;
    unsigned char alphaBits;
    unsigned char colorBits;
    unsigned char depthBits;
} ksGpuSurfaceBits;

bool ksGpuContext_CreateShared(ksGpuContext *context, const ksGpuContext *other, int queueIndex);
void ksGpuContext_Destroy(ksGpuContext *context);
void ksGpuContext_SetCurrent(ksGpuContext *context);
void ksGpuContext_UnsetCurrent(ksGpuContext *context);
bool ksGpuContext_CheckCurrent(ksGpuContext *context);

/*
================================================================================================================================

GPU Window.

Window with associated GPU context for GPU accelerated rendering.
For optimal performance a window should only be created at load time, not at runtime.
Because on some platforms the OS/drivers use thread local storage, ksGpuWindow *must* be created
and destroyed on the same thread that will actually render to the window and swap buffers.

ksGpuWindow
ksGpuWindowEvent
ksGpuWindowInput
ksKeyboardKey
ksMouseButton

bool ksGpuWindow_Create( ksGpuWindow * window, ksDriverInstance * instance,
                                                const ksGpuQueueInfo * queueInfo, const int queueIndex,
                                                const ksGpuSurfaceColorFormat colorFormat, const ksGpuSurfaceDepthFormat
depthFormat,
                                                const ksGpuSampleCount sampleCount, const int width, const int height, const bool
fullscreen );
void ksGpuWindow_Destroy( ksGpuWindow * window );

================================================================================================================================
*/

typedef enum {
    KS_GPU_WINDOW_EVENT_NONE,
    KS_GPU_WINDOW_EVENT_ACTIVATED,
    KS_GPU_WINDOW_EVENT_DEACTIVATED,
    KS_GPU_WINDOW_EVENT_EXIT
} ksGpuWindowEvent;

typedef struct {
    bool keyInput[256];
    bool mouseInput[8];
    int mouseInputX[8];
    int mouseInputY[8];
} ksGpuWindowInput;

typedef struct {
    ksGpuDevice device;
    ksGpuContext context;
    ksGpuSurfaceColorFormat colorFormat;
    ksGpuSurfaceDepthFormat depthFormat;
    ksGpuSampleCount sampleCount;
    int windowWidth;
    int windowHeight;
    int windowSwapInterval;
    float windowRefreshRate;
    bool windowFullscreen;
    bool windowActive;
    bool windowExit;
    ksGpuWindowInput input;

#if defined(OS_WINDOWS)
    HINSTANCE hInstance;
    HDC hDC;
    HWND hWnd;
    bool windowActiveState;
#elif defined(OS_LINUX_XLIB)
    Display *xDisplay;
    int xScreen;
    Window xRoot;
    XVisualInfo *xVisual;
    Colormap xColormap;
    Window xWindow;
    int desktopWidth;
    int desktopHeight;
    float desktopRefreshRate;
#elif defined(OS_LINUX_XCB) || defined(OS_LINUX_XCB_GLX)
    Display *xDisplay;
    xcb_connection_t *connection;
    xcb_screen_t *screen;
    xcb_colormap_t colormap;
    xcb_window_t window;
    xcb_atom_t wm_delete_window_atom;
    xcb_key_symbols_t *key_symbols;
    xcb_glx_window_t glxWindow;
    int desktopWidth;
    int desktopHeight;
    float desktopRefreshRate;
#elif defined(OS_LINUX_WAYLAND)
    struct wl_display *display;

    struct wl_surface *surface;

    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct zxdg_shell_v6 *shell;
    struct zxdg_surface_v6 *shell_surface;

    struct wl_keyboard *keyboard;
    struct wl_pointer *pointer;
    struct wl_seat *seat;
#elif defined(OS_APPLE_MACOS)
    CGDirectDisplayID display;
    CGDisplayModeRef desktopDisplayMode;
#if defined(__OBJC__)
    NSWindow *nsWindow;
    NSView *nsView;
#else
    void *nsWindow;
    void *nsView;
#endif  // defined(__OBJC__)
#elif defined(OS_APPLE_IOS)
    UIWindow *uiWindow;
    UIView *uiView;
#elif defined(OS_ANDROID)
    EGLDisplay display;
    EGLint majorVersion;
    EGLint minorVersion;
    Java_t java;
#endif
} ksGpuWindow;

bool ksGpuWindow_Create(ksGpuWindow *window, ksDriverInstance *instance, const ksGpuQueueInfo *queueInfo, int queueIndex,
                        ksGpuSurfaceColorFormat colorFormat, ksGpuSurfaceDepthFormat depthFormat, ksGpuSampleCount sampleCount,
                        int width, int height, bool fullscreen);
void ksGpuWindow_Destroy(ksGpuWindow *window);

#ifdef __cplusplus
}
#endif

#endif  // !KSGRAPHICSWRAPPER_OPENGL_H
