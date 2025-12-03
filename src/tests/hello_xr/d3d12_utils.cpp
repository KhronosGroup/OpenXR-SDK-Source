// Copyright (c) 2019-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#if defined(XR_USE_GRAPHICS_API_D3D12)

#include "d3d12_utils.h"
#include <cstdint>

template <std::uint32_t alignment>
constexpr std::uint32_t AlignTo(std::uint32_t n) {
    static_assert((alignment & (alignment - 1)) == 0, "The alignment must be power-of-two");
    // Add one less than the alignment: this will give us the largest possible value that we might need to round to.
    // Then, mask off the bits that are lower order than the alignment.
    return (n + alignment - 1) & ~(alignment - 1);
}

#include "check.h"
#define XRC_CHECK_THROW_HRCMD CHECK_HRCMD

#include <d3d12.h>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr

using Microsoft::WRL::ComPtr;

static ComPtr<ID3D12Resource> D3D12CreateResource(ID3D12Device* d3d12Device, uint32_t width, uint32_t height, uint16_t depth,
                                                  uint16_t mipLevels, D3D12_RESOURCE_DIMENSION dimension, DXGI_FORMAT format,
                                                  D3D12_TEXTURE_LAYOUT layout, D3D12_HEAP_TYPE heapType) {
    D3D12_RESOURCE_STATES d3d12ResourceState;
    if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
        d3d12ResourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
        width = AlignTo<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(width);
    } else {
        d3d12ResourceState = D3D12_RESOURCE_STATE_COMMON;
    }

    D3D12_HEAP_PROPERTIES heapProp{};
    heapProp.Type = heapType;
    heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC buffDesc{};
    buffDesc.Dimension = dimension;
    buffDesc.Alignment = 0;
    buffDesc.Width = width;
    buffDesc.Height = height;
    buffDesc.DepthOrArraySize = depth;
    buffDesc.MipLevels = mipLevels;
    buffDesc.Format = format;
    buffDesc.SampleDesc.Count = 1;
    buffDesc.SampleDesc.Quality = 0;
    buffDesc.Layout = layout;
    buffDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    ComPtr<ID3D12Resource> buffer;
    XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &buffDesc, d3d12ResourceState,
                                                               nullptr, __uuidof(ID3D12Resource),
                                                               reinterpret_cast<void**>(buffer.ReleaseAndGetAddressOf())));
    return buffer;
}

ComPtr<ID3D12Resource> D3D12CreateBuffer(ID3D12Device* d3d12Device, uint32_t size, D3D12_HEAP_TYPE heapType) {
    return D3D12CreateResource(d3d12Device, size, 1, 1, 1, D3D12_RESOURCE_DIMENSION_BUFFER, DXGI_FORMAT_UNKNOWN,
                               D3D12_TEXTURE_LAYOUT_ROW_MAJOR, heapType);
}

ComPtr<ID3D12Resource> D3D12CreateImage(ID3D12Device* d3d12Device, uint32_t width, uint32_t height, uint16_t arraySize,
                                        uint16_t mipLevels, DXGI_FORMAT format, D3D12_HEAP_TYPE heapType) {
    return D3D12CreateResource(d3d12Device, width, height, arraySize, mipLevels, D3D12_RESOURCE_DIMENSION_TEXTURE2D, format,
                               D3D12_TEXTURE_LAYOUT_UNKNOWN, heapType);
}

#endif
