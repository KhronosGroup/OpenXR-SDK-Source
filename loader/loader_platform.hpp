// Copyright (c) 2017 The Khronos Group Inc.
// Copyright (c) 2017 Valve Corporation
// Copyright (c) 2017 LunarG, Inc.
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

#pragma once

#include <cassert>

#if defined(__GNUC__) && __GNUC__ >= 4
#define LOADER_EXPORT __attribute__((visibility("default")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590)
#define LOADER_EXPORT __attribute__((visibility("default")))
#else
#define LOADER_EXPORT
#endif

// Environment variables
#if defined(XR_OS_LINUX) || defined(XR_OS_APPLE)

#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#define PATH_SEPARATOR ':'
#define DIRECTORY_SYMBOL '/'

// Dynamic Loading of libraries:
typedef void *LoaderPlatformLibraryHandle;
static inline LoaderPlatformLibraryHandle LoaderPlatformLibraryOpen(const std::string &path) {
    // When loading the library, we use RTLD_LAZY so that not all symbols have to be
    // resolved at this time (which improves performance). Note that if not all symbols
    // can be resolved, this could cause crashes later.
    // For experimenting/debugging: Define the LD_BIND_NOW environment variable to force all
    // symbols to be resolved here.
    return dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
}

static inline const char *LoaderPlatformLibraryOpenError(const std::string &path) {
    (void)path;
    return dlerror();
}

static inline void LoaderPlatformLibraryClose(LoaderPlatformLibraryHandle library) { dlclose(library); }

static inline void *LoaderPlatformLibraryGetProcAddr(LoaderPlatformLibraryHandle library, const std::string &name) {
    assert(library);
    assert(name.size() > 0);
    return dlsym(library, name.c_str());
}

static inline const char *LoaderPlatformLibraryGetProcAddrError(const std::string &name) {
    (void)name;
    return dlerror();
}

#elif defined(XR_OS_WINDOWS)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Winreg.h>

static inline bool PathFileExists(LPCTSTR szPath) {
    DWORD dwAttrib = GetFileAttributes(szPath);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

#define PATH_SEPARATOR ';'
#define DIRECTORY_SYMBOL '\\'

// Workaround for MS VS 2010/2013 missing snprintf and vsnprintf
#if defined(_MSC_VER) && _MSC_VER < 1900
#include <stdint.h>

static inline int32_t xr_vsnprintf(char *result_buffer, size_t buffer_size, const char *print_format, va_list varying_list) {
    int32_t copy_count = -1;
    if (buffer_size != 0) {
        copy_count = _vsnprintf_s(result_buffer, buffer_size, _TRUNCATE, print_format, varying_list);
    }
    if (copy_count == -1) {
        copy_count = _vscprintf(print_format, varying_list);
    }
    return copy_count;
}

static inline int32_t xr_snprintf(char *result_buffer, size_t buffer_size, const char *print_format, ...) {
    va_list varying_list;
    va_start(varying_list, print_format);
    int32_t copy_count = xr_vsnprintf(result_buffer, buffer_size, print_format, varying_list);
    va_end(varying_list);
    return copy_count;
}

#define snprintf xr_snprintf
#define vsnprintf xr_vsnprintf

#endif

// Dynamic Loading:
typedef HMODULE LoaderPlatformLibraryHandle;
static LoaderPlatformLibraryHandle LoaderPlatformLibraryOpen(const std::string &path) {
    // Try loading the library the original way first.
    LoaderPlatformLibraryHandle handle = LoadLibrary(path.c_str());
    if (handle == NULL && GetLastError() == ERROR_MOD_NOT_FOUND && PathFileExists(path.c_str())) {
        // If that failed, then try loading it with broader search folders.
        handle = LoadLibraryEx(path.c_str(), NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
    }
    return handle;
}

static char *LoaderPlatformLibraryOpenError(const std::string &path) {
    static char errorMsg[164];
    (void)snprintf(errorMsg, 163, "Failed to open dynamic library \"%s\" with error %d", path.c_str(), GetLastError());
    return errorMsg;
}

static void LoaderPlatformLibraryClose(LoaderPlatformLibraryHandle library) { FreeLibrary(library); }

static void *LoaderPlatformLibraryGetProcAddr(LoaderPlatformLibraryHandle library, const std::string &name) {
    assert(library);
    assert(name.size() > 0);
    return GetProcAddress(library, name.c_str());
}

static char *LoaderPlatformLibraryGetProcAddrAddrError(const std::string &name) {
    static char errorMsg[120];
    (void)snprintf(errorMsg, 119, "Failed to find function \"%s\" in dynamic library", name.c_str());
    return errorMsg;
}

#else  // Not Linux or Windows

#define PATH_SEPARATOR ':'
#define DIRECTORY_SYMBOL '/'

static inline LoaderPlatformLibraryHandle LoaderPlatformLibraryOpen(const std::string &path) {
// Stub func
#error("Unknown platform, undefined dynamic library routines resulting");
    (void)path;
}

static inline const char *LoaderPlatformLibraryOpenError(const std::string &path) {
    // Stub func
    (void)path;
}

static inline void LoaderPlatformLibraryClose(LoaderPlatformLibraryHandle library) {
    // Stub func
    (void)library;
}

static inline void *LoaderPlatformLibraryGetProcAddr(LoaderPlatformLibraryHandle library, const std::string &name) {
    // Stub func
    void(library);
    void(name);
}

static inline const char *LoaderPlatformLibraryGetProcAddrError(const std::string &name) {
    // Stub func
    (void)name;
}

#endif
