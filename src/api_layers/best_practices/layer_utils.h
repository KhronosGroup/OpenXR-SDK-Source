// Copyright (c) 2025 The Khronos Group Inc.
// Copyright (c) 2025 Collabora, Ltd.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <xr_generated_dispatch_table.h>

#include <memory>
#include <mutex>
#include <iostream>
#include <string>
#include <unordered_map>

#ifdef __ANDROID__
#include "android/log.h"
#endif

class LockedDispatchTable {
   public:
    LockedDispatchTable() = default;
    LockedDispatchTable(const LockedDispatchTable&) = delete;
    LockedDispatchTable(LockedDispatchTable&&) = delete;

    void Reset(std::unique_ptr<XrGeneratedDispatchTable>&& newTable = {});

    template <typename PFN>
    PFN Get(PFN(XrGeneratedDispatchTable::*p)) {
        std::unique_lock<std::mutex> lock{m_mutex};
        return m_dispatch.get()->*p;
    }

    bool isValid() { return m_dispatch.get() != nullptr; }

   private:
    std::unique_ptr<XrGeneratedDispatchTable> m_dispatch{};
    std::mutex m_mutex;
};

class BPLogger {
   public:
    static void LogMessage(const std::string& message) {
        bool printError = false;
        if (errorCounts.find(message) != errorCounts.end()) {
            if (errorCounts[message] < 10) {
                printError = true;
            } else if (errorCounts[message] % 1000 == 0) {
#if defined(XR_OS_WINDOWS)
                OutputDebugStringA(
                    (message + " (Limiting further occurrences [" + std::to_string(errorCounts[message]) + "])\n").c_str());
#elif defined(XR_OS_ANDROID)
                __android_log_write(
                    ANDROID_LOG_ERROR, "OpenXR-BestPractices",
                    (message + " (Limiting further occurrences [" + std::to_string(errorCounts[message]) + "])").c_str());
#endif
            }
            errorCounts[message]++;
        } else {
            errorCounts[message] = 1;
            printError = true;
        }

        if (printError) {
#if defined(XR_OS_WINDOWS)
            OutputDebugStringA((message + "\n").c_str());
#elif defined(XR_OS_ANDROID)
            __android_log_write(ANDROID_LOG_ERROR, "OpenXR-BestPractices", message.c_str());
#endif
        }
    }

   private:
    static std::unordered_map<std::string, int> errorCounts;
};
