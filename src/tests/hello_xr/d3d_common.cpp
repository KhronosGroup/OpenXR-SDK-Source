// Copyright (c) 2017-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "common.h"

#if defined(XR_USE_GRAPHICS_API_D3D11) || defined(XR_USE_GRAPHICS_API_D3D12)

#include <common/xr_linear.h>
#include <DirectXColors.h>
#include <D3Dcompiler.h>

#include "d3d_common.h"

using namespace Microsoft::WRL;
using namespace DirectX;

XMMATRIX XM_CALLCONV LoadXrPose(const XrPosef& pose) {
    return XMMatrixAffineTransformation(DirectX::g_XMOne, DirectX::g_XMZero,
                                        XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(&pose.orientation)),
                                        XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&pose.position)));
}

XMMATRIX XM_CALLCONV LoadXrMatrix(const XrMatrix4x4f& matrix) {
    // XrMatrix4x4f has same memory layout as DirectX Math (Row-major,post-multiplied = column-major,pre-multiplied)
    return XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&matrix));
}

ComPtr<ID3DBlob> CompileShader(const char* hlsl, const char* entrypoint, const char* shaderTarget) {
    ComPtr<ID3DBlob> compiled;
    ComPtr<ID3DBlob> errMsgs;
    DWORD flags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;

#if !defined(NDEBUG)
    flags |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    HRESULT hr = D3DCompile(hlsl, strlen(hlsl), nullptr, nullptr, nullptr, entrypoint, shaderTarget, flags, 0,
                            compiled.GetAddressOf(), errMsgs.GetAddressOf());
    if (FAILED(hr)) {
        std::string errMsg((const char*)errMsgs->GetBufferPointer(), errMsgs->GetBufferSize());
        Log::Write(Log::Level::Error, Fmt("D3DCompile failed %X: %s", hr, errMsg.c_str()));
        THROW_HR(hr, "D3DCompile");
    }

    return compiled;
}

ComPtr<IDXGIAdapter1> GetAdapter(LUID adapterId) {
    // Create the DXGI factory.
    ComPtr<IDXGIFactory1> dxgiFactory;
    CHECK_HRCMD(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(dxgiFactory.ReleaseAndGetAddressOf())));

    for (UINT adapterIndex = 0;; adapterIndex++) {
        // EnumAdapters1 will fail with DXGI_ERROR_NOT_FOUND when there are no more adapters to enumerate.
        ComPtr<IDXGIAdapter1> dxgiAdapter;
        CHECK_HRCMD(dxgiFactory->EnumAdapters1(adapterIndex, dxgiAdapter.ReleaseAndGetAddressOf()));

        DXGI_ADAPTER_DESC1 adapterDesc;
        CHECK_HRCMD(dxgiAdapter->GetDesc1(&adapterDesc));
        if (memcmp(&adapterDesc.AdapterLuid, &adapterId, sizeof(adapterId)) == 0) {
            Log::Write(Log::Level::Verbose, Fmt("Using graphics adapter %ws", adapterDesc.Description));
            return dxgiAdapter;
        }
    }
}

#endif
