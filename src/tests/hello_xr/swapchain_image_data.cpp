// Copyright (c) 2019-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "swapchain_image_data.h"

#include <openxr/openxr.h>

#include <algorithm>
#include <cstdint>

#include "common/xr_dependencies.h"  // IWYU pragma: keep

#include "check.h"

// Compatibility defines to keep code similar to CTS code
#define XR_NULL_HANDLE_CPP XR_NULL_HANDLE

ISwapchainImageData::~ISwapchainImageData() = default;

DepthSwapchainHandling::DepthSwapchainHandling(XrSwapchain depthSwapchain) : m_depthSwapchain(depthSwapchain) {
    XRC_CHECK_THROW(depthSwapchain != XR_NULL_HANDLE_CPP);
}

void DepthSwapchainHandling::AcquireAndWaitDepthSwapchainImage(uint32_t colorImageIndex) {
    std::unique_lock<std::mutex> lock(m_mutex);
    uint32_t depthImageIndex;
    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    XRC_CHECK_THROW_XRCMD(xrAcquireSwapchainImage(m_depthSwapchain, &acquireInfo, &depthImageIndex));

    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = XR_INFINITE_DURATION;  // Call can block waiting for image to become available for writing.
    XRC_CHECK_THROW_XRCMD(xrWaitSwapchainImage(m_depthSwapchain, &waitInfo));
    m_colorToAcquiredDepthIndices.emplace_back(colorImageIndex, depthImageIndex);
}

bool DepthSwapchainHandling::GetWaitedDepthSwapchainImageIndexFor(uint32_t colorImageIndex, uint32_t& outDepthImageIndex) const {
    std::unique_lock<std::mutex> lock(m_mutex);
    const auto b = m_colorToAcquiredDepthIndices.begin();
    const auto e = m_colorToAcquiredDepthIndices.end();
    auto it = std::find_if(b, e, [colorImageIndex](std::pair<uint32_t, uint32_t> const& p) { return p.first == colorImageIndex; });
    if (it == e) {
        return false;
    }
    outDepthImageIndex = it->second;
    return true;
}

void DepthSwapchainHandling::ReleaseDepthSwapchainImage() {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_colorToAcquiredDepthIndices.empty()) {
        // over-releasing?
        return;
    }

    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    XRC_CHECK_THROW_XRCMD(xrReleaseSwapchainImage(m_depthSwapchain, &releaseInfo));
    m_colorToAcquiredDepthIndices.erase(m_colorToAcquiredDepthIndices.begin());
}
