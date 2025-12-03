// Copyright (c) 2019-2025 The Khronos Group Inc.
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

#pragma once

#include <nonstd/span.hpp>

#include <algorithm>
#include <initializer_list>
#include <stdexcept>
#include <stdint.h>
#include <vector>

using nonstd::span;

/// Wraps a vector to keep track of collections of things referenced by a type-safe handle.
/// The handle consists of the index in the vector combined with a "generation number" which is
/// incremented every time the container is cleared.
///
/// Used with things like @ref MeshHandle, inside the graphics plugin implementations
template <typename T, typename HandleType>
class VectorWithGenerationCountedHandles {
   public:
    // TODO genericize
    // static_assert(sizeof(HandleType) == sizeof(uint64_t), "Only works with 64-bit handles right now");
    using GenerationType = uint32_t;
    template <typename... Args>
    HandleType emplace_back(Args&&... args) {
        auto index = m_data.size();
        m_data.emplace_back(std::forward<Args&&>(args)...);
        return HandleType{index | (static_cast<uint64_t>(m_generationNumber) << kGenerationShift)};
    }

    T& operator[](HandleType h) { return m_data[checkHandleInBoundsAndGetIndex(h)]; }

    const T& operator[](HandleType h) const { return m_data[checkHandleInBoundsAndGetIndex(h)]; }

    void assertContains(HandleType h) const { checkHandleInBoundsAndGetIndex(h); }

    void clear() {
        m_generationNumber++;
        m_data.clear();
    }

   private:
    uint32_t checkHandleAndGetIndex(HandleType h) const {
        if (h == HandleType{}) {
            throw std::logic_error("Internal error: Trying to use a null graphics handle!");
        }
        auto generation = static_cast<GenerationType>(h.get() >> kGenerationShift);
        if (generation != m_generationNumber) {
            throw std::logic_error(
                "Internal error: Trying to use a graphics handle left over from before a Shutdown() or ShutdownDevice() call!");
        }
        // TODO implicit mask is here by truncating!
        auto index = static_cast<uint32_t>(h.get());
        return index;
    }
    uint32_t checkHandleInBoundsAndGetIndex(HandleType h) const {
        auto index = checkHandleAndGetIndex(h);
        if (index >= m_data.size()) {
            throw std::logic_error("Internal error: Out-of-bounds handle");
        }
        return index;
    }
    static constexpr size_t kGenerationShift = 32;
    std::vector<T> m_data;
    GenerationType m_generationNumber{1};
};

/// Helper for selecting swapchain formats.
///
/// @param throwIfNotFound If true, an exception will be thrown if no suitable format is found.
/// @param runtimeSupportedTypes The types exposed by the runtime, in runtime-provided preference order
/// @param usableFormats The formats that would be usable.
///
/// Returns the first element of @p runtimeSupportedTypes found in @p usableFormats or
/// -1 if no intersection and @p throwIfNotFound is false.
template <typename FormatType>
int64_t SelectSwapchainFormat(bool throwIfNotFound, span<const int64_t> runtimeSupportedTypes,
                              const std::initializer_list<FormatType>& usableFormats) {
    auto it = std::find_first_of(
        runtimeSupportedTypes.begin(), runtimeSupportedTypes.end(), usableFormats.begin(), usableFormats.end(),
        [](int64_t supportedFormat, FormatType usableFormat) { return static_cast<int64_t>(usableFormat) == supportedFormat; });
    if (it == runtimeSupportedTypes.end()) {
        if (throwIfNotFound) {
            throw std::runtime_error("No suitable format was returned by the runtime");
        }
        return -1;
    }
    return *it;
};
