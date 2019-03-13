// Copyright (c) 2017 The Khronos Group Inc.
// Copyright (c) 2016 Oculus VR, LLC.
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
// Author: J.M.P. van Waveren
//

// Description :   Time in nanoseconds.
// Author      :   J.M.P. van Waveren
// Date        :   12/10/2016
// Language    :   C99
// Format      :   Indent 4 spaces - no tabs.

#include "impl/nanoseconds.h"

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#if !defined(OS_WINDOWS)
#define OS_WINDOWS
#endif
#elif defined(__ANDROID__)
#if !defined(OS_ANDROID)
#define OS_ANDROID
#endif
#elif defined(__hexagon__) || defined(__qdsp6__)
#if !defined(OS_HEXAGON)
#define OS_HEXAGON
#endif
#elif defined(__APPLE__)
#if !defined(OS_APPLE)
#define OS_APPLE
#endif
#include <Availability.h>
#if __IPHONE_OS_VERSION_MAX_ALLOWED && !defined(OS_APPLE_IOS)
#define OS_APPLE_IOS
#elif __MAC_OS_X_VERSION_MAX_ALLOWED && !defined(OS_APPLE_MACOS)
#define OS_APPLE_MACOS
#endif
#elif defined(__linux__)
#if !defined(OS_LINUX)
#define OS_LINUX
#endif
#else
#error "unknown platform"
#endif

#if defined(OS_WINDOWS)
#include <windows.h>
#elif defined(OS_LINUX)
#include <time.h>  // for timespec
#elif defined(OS_APPLE)
#include <mach/mach_time.h>
#elif defined(OS_ANDROID)
#include <time.h>
#elif defined(OS_HEXAGON)
#include <qurt_timer.h>
#endif

#include <stdint.h>

// Note: All the implementations below are subject to thread race conditions,
// because they check a value before initializing, and that check is subject
// to two threads executing that check at the same time. A resolution is to call
// this function once on startup to ensure the values get initialized in a
// thread-safe way.

XrTime GetTimeNanosecondsXr() {
#if defined(OS_WINDOWS)
    static XrTime ticks_per_second = 0;

    if (ticks_per_second == 0) {
        LARGE_INTEGER li;
        QueryPerformanceFrequency(&li);
        ticks_per_second = (XrTime)li.QuadPart;
    }

    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    XrTime counter = (XrTime)li.LowPart + (0xFFFFFFFFULL * li.HighPart);
    return (XrTime)(counter * (1000000000.0 / ticks_per_second));

#elif defined(OS_ANDROID) || defined(OS_LINUX)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (XrTime)((ts.tv_sec * 1000000000ULL) + ts.tv_nsec);

#elif defined(OS_APPLE)
    static mach_timebase_info_data_t timebase_info = {};
    if (timebase_info.numer == 0) mach_timebase_info(&timebase_info);

    return ((mach_absolute_time() * timebase_info.numer) / timebase_info.denom);

#elif defined(OS_HEXAGON)
    return QURT_TIMER_TIMETICK_TO_US(qurt_timer_get_ticks()) * 1000;

#else
    struct timeval tv;
    gettimeofday(&tv, 0);

    return (XrTime)((tv.tv_sec * 1000000000ULL) + (tv.tv_usec * 1000ULL));
#endif
}
