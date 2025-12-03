// Copyright (c) 2019-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#if defined(XR_USE_GRAPHICS_API_D3D12)

#include "common/xr_dependencies.h"  // IWYU pragma: keep

#include "check.h"

#include <d3d12.h>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr

#include <cassert>

Microsoft::WRL::ComPtr<ID3D12Resource> D3D12CreateResource(ID3D12Device* d3d12Device, uint32_t width, uint32_t height,
                                                           uint16_t depth, D3D12_RESOURCE_DIMENSION dimension, DXGI_FORMAT format,
                                                           D3D12_TEXTURE_LAYOUT layout, D3D12_HEAP_TYPE heapType);

Microsoft::WRL::ComPtr<ID3D12Resource> D3D12CreateBuffer(ID3D12Device* d3d12Device, uint32_t size, D3D12_HEAP_TYPE heapType);

Microsoft::WRL::ComPtr<ID3D12Resource> D3D12CreateImage(ID3D12Device* d3d12Device, uint32_t width, uint32_t height,
                                                        uint16_t arraySize, uint16_t mipLevels, DXGI_FORMAT format,
                                                        D3D12_HEAP_TYPE heapType);

template <typename T>
void D3D12BasicUpload(_In_ ID3D12Resource* buffer, _In_reads_(count) const T* data, size_t count) {
    void* pData;
    XRC_CHECK_THROW_HRCMD(buffer->Map(0, nullptr, &pData));

    size_t writeBytes = count * sizeof(T);
    memcpy(pData, data, writeBytes);

    D3D12_RANGE writtenRange{0, writeBytes};
    buffer->Unmap(0, &writtenRange);
}

struct D3D12ResourceWithSRVDesc {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
};

/// Wrap a normal GPU buffer (for an array or single structure)
/// and a corresponding upload buffer to be able to update the contents of the buffer asynchronously.
template <typename T>
class D3D12BufferWithUpload {
   public:
    D3D12BufferWithUpload() = default;

    /// Construct and allocate buffers suitable for an array of @p maxCapacity elements (defaulting to a single element)
    explicit D3D12BufferWithUpload(_In_ ID3D12Device* device, size_t maxCapacity = 1)
        : resource(D3D12CreateBuffer(device, (uint32_t)RequiredBytesFor(maxCapacity), D3D12_HEAP_TYPE_DEFAULT)),
          uploadBuffer(D3D12CreateBuffer(device, (uint32_t)RequiredBytesFor(maxCapacity), D3D12_HEAP_TYPE_UPLOAD)) {}

    /// Allocate (or discard and re-allocate) buffers suitable for an array of @p maxCapacity elements (defaulting to a single
    /// element)
    void Allocate(_In_ ID3D12Device* device, size_t maxCapacity = 1) { *this = D3D12BufferWithUpload(device, maxCapacity); }

    /// Would an array of @p count elements of this type fit in the resource?
    bool Fits(size_t count) const {
        if (!resource) {
            throw std::logic_error("Resources not allocated before calling Fits()");
        }
        D3D12_RESOURCE_DESC bufferDesc = resource->GetDesc();
        return bufferDesc.Width >= RequiredBytesFor(count);
    }

    /// Copy the supplied array @p data with @p count elements to an upload buffer,
    /// then add a copy to the "real" buffer to the supplied command list
    void AsyncUpload(ID3D12GraphicsCommandList* copyCommandList, _In_reads_(count) const T* data, size_t count = 1) const {
        if (!resource) {
            throw std::logic_error("Resources not allocated before calling AsyncUpload()");
        }
        assert(Fits(count));  // caller is responsible for making a larger buffer if count changes

        D3D12BasicUpload<T>(uploadBuffer.Get(), data, count);
        copyCommandList->CopyBufferRegion(resource.Get(), 0, uploadBuffer.Get(), 0, RequiredBytesFor(count));
    }

    /// Get the resource on the GPU, without affecting the reference count.
    ID3D12Resource* GetResource() const noexcept { return resource.Get(); }

   private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;

    static size_t RequiredBytesFor(size_t count) { return count * sizeof(T); }
};

#endif
