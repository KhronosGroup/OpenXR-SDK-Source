// Copyright (c) 2017-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "common.h"
#include "geometry.h"
#include "graphicsplugin.h"
#include "options.h"

#if defined(XR_USE_GRAPHICS_API_D3D11)

#include <common/xr_linear.h>
#include <DirectXColors.h>
#include <D3Dcompiler.h>

#include "d3d_common.h"

using namespace Microsoft::WRL;
using namespace DirectX;

namespace {
void InitializeD3D11DeviceForAdapter(IDXGIAdapter1* adapter, const std::vector<D3D_FEATURE_LEVEL>& featureLevels,
                                     ID3D11Device** device, ID3D11DeviceContext** deviceContext) {
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if !defined(NDEBUG)
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // Create the Direct3D 11 API device object and a corresponding context.
    D3D_DRIVER_TYPE driverType = ((adapter == nullptr) ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_UNKNOWN);

TryAgain:
    HRESULT hr = D3D11CreateDevice(adapter, driverType, 0, creationFlags, featureLevels.data(), (UINT)featureLevels.size(),
                                   D3D11_SDK_VERSION, device, nullptr, deviceContext);
    if (FAILED(hr)) {
        // If initialization failed, it may be because device debugging isn't supported, so retry without that.
        if ((creationFlags & D3D11_CREATE_DEVICE_DEBUG) && (hr == DXGI_ERROR_SDK_COMPONENT_MISSING)) {
            creationFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
            goto TryAgain;
        }

        // If the initialization still fails, fall back to the WARP device.
        // For more information on WARP, see: http://go.microsoft.com/fwlink/?LinkId=286690
        if (driverType != D3D_DRIVER_TYPE_WARP) {
            driverType = D3D_DRIVER_TYPE_WARP;
            goto TryAgain;
        }
    }
}

struct D3D11GraphicsPlugin : public IGraphicsPlugin {
    D3D11GraphicsPlugin(const std::shared_ptr<Options>& options, std::shared_ptr<IPlatformPlugin>)
        : m_clearColor(options->GetBackgroundClearColor()) {}

    std::vector<std::string> GetInstanceExtensions() const override { return {XR_KHR_D3D11_ENABLE_EXTENSION_NAME}; }

    void InitializeDevice(XrInstance instance, XrSystemId systemId) override {
        PFN_xrGetD3D11GraphicsRequirementsKHR pfnGetD3D11GraphicsRequirementsKHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetD3D11GraphicsRequirementsKHR",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetD3D11GraphicsRequirementsKHR)));

        // Create the D3D11 device for the adapter associated with the system.
        XrGraphicsRequirementsD3D11KHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
        CHECK_XRCMD(pfnGetD3D11GraphicsRequirementsKHR(instance, systemId, &graphicsRequirements));
        const ComPtr<IDXGIAdapter1> adapter = GetAdapter(graphicsRequirements.adapterLuid);

        // Create a list of feature levels which are both supported by the OpenXR runtime and this application.
        std::vector<D3D_FEATURE_LEVEL> featureLevels = {D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1,
                                                        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
        featureLevels.erase(std::remove_if(featureLevels.begin(), featureLevels.end(),
                                           [&](D3D_FEATURE_LEVEL fl) { return fl < graphicsRequirements.minFeatureLevel; }),
                            featureLevels.end());
        CHECK_MSG(featureLevels.size() != 0, "Unsupported minimum feature level!");

        InitializeD3D11DeviceForAdapter(adapter.Get(), featureLevels, m_device.ReleaseAndGetAddressOf(),
                                        m_deviceContext.ReleaseAndGetAddressOf());

        InitializeResources();

        m_graphicsBinding.device = m_device.Get();
    }

    void InitializeResources() {
        const ComPtr<ID3DBlob> vertexShaderBytes = CompileShader(ShaderHlsl, "MainVS", "vs_5_0");
        CHECK_HRCMD(m_device->CreateVertexShader(vertexShaderBytes->GetBufferPointer(), vertexShaderBytes->GetBufferSize(), nullptr,
                                                 m_vertexShader.ReleaseAndGetAddressOf()));

        const ComPtr<ID3DBlob> pixelShaderBytes = CompileShader(ShaderHlsl, "MainPS", "ps_5_0");
        CHECK_HRCMD(m_device->CreatePixelShader(pixelShaderBytes->GetBufferPointer(), pixelShaderBytes->GetBufferSize(), nullptr,
                                                m_pixelShader.ReleaseAndGetAddressOf()));

        const D3D11_INPUT_ELEMENT_DESC vertexDesc[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        CHECK_HRCMD(m_device->CreateInputLayout(vertexDesc, (UINT)ArraySize(vertexDesc), vertexShaderBytes->GetBufferPointer(),
                                                vertexShaderBytes->GetBufferSize(), &m_inputLayout));

        const CD3D11_BUFFER_DESC modelConstantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
        CHECK_HRCMD(m_device->CreateBuffer(&modelConstantBufferDesc, nullptr, m_modelCBuffer.ReleaseAndGetAddressOf()));

        const CD3D11_BUFFER_DESC viewProjectionConstantBufferDesc(sizeof(ViewProjectionConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
        CHECK_HRCMD(
            m_device->CreateBuffer(&viewProjectionConstantBufferDesc, nullptr, m_viewProjectionCBuffer.ReleaseAndGetAddressOf()));

        const D3D11_SUBRESOURCE_DATA vertexBufferData{Geometry::c_cubeVertices};
        const CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(Geometry::c_cubeVertices), D3D11_BIND_VERTEX_BUFFER);
        CHECK_HRCMD(m_device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, m_cubeVertexBuffer.ReleaseAndGetAddressOf()));

        const D3D11_SUBRESOURCE_DATA indexBufferData{Geometry::c_cubeIndices};
        const CD3D11_BUFFER_DESC indexBufferDesc(sizeof(Geometry::c_cubeIndices), D3D11_BIND_INDEX_BUFFER);
        CHECK_HRCMD(m_device->CreateBuffer(&indexBufferDesc, &indexBufferData, m_cubeIndexBuffer.ReleaseAndGetAddressOf()));
    }

    int64_t SelectColorSwapchainFormat(const std::vector<int64_t>& runtimeFormats) const override {
        // List of supported color swapchain formats.
        constexpr DXGI_FORMAT SupportedColorSwapchainFormats[] = {
            DXGI_FORMAT_R8G8B8A8_UNORM,
            DXGI_FORMAT_B8G8R8A8_UNORM,
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
            DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
        };

        auto swapchainFormatIt =
            std::find_first_of(runtimeFormats.begin(), runtimeFormats.end(), std::begin(SupportedColorSwapchainFormats),
                               std::end(SupportedColorSwapchainFormats));
        if (swapchainFormatIt == runtimeFormats.end()) {
            THROW("No runtime swapchain format supported for color swapchain");
        }

        return *swapchainFormatIt;
    }

    const XrBaseInStructure* GetGraphicsBinding() const override {
        return reinterpret_cast<const XrBaseInStructure*>(&m_graphicsBinding);
    }

    std::vector<XrSwapchainImageBaseHeader*> AllocateSwapchainImageStructs(
        uint32_t capacity, const XrSwapchainCreateInfo& /*swapchainCreateInfo*/) override {
        // Allocate and initialize the buffer of image structs (must be sequential in memory for xrEnumerateSwapchainImages).
        // Return back an array of pointers to each swapchain image struct so the consumer doesn't need to know the type/size.
        std::vector<XrSwapchainImageD3D11KHR> swapchainImageBuffer(capacity, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
        std::vector<XrSwapchainImageBaseHeader*> swapchainImageBase;
        for (XrSwapchainImageD3D11KHR& image : swapchainImageBuffer) {
            swapchainImageBase.push_back(reinterpret_cast<XrSwapchainImageBaseHeader*>(&image));
        }

        // Keep the buffer alive by moving it into the list of buffers.
        m_swapchainImageBuffers.push_back(std::move(swapchainImageBuffer));

        return swapchainImageBase;
    }

    ComPtr<ID3D11DepthStencilView> GetDepthStencilView(ID3D11Texture2D* colorTexture) {
        // If a depth-stencil view has already been created for this back-buffer, use it.
        auto depthBufferIt = m_colorToDepthMap.find(colorTexture);
        if (depthBufferIt != m_colorToDepthMap.end()) {
            return depthBufferIt->second;
        }

        // This back-buffer has no corresponding depth-stencil texture, so create one with matching dimensions.
        D3D11_TEXTURE2D_DESC colorDesc;
        colorTexture->GetDesc(&colorDesc);

        D3D11_TEXTURE2D_DESC depthDesc{};
        depthDesc.Width = colorDesc.Width;
        depthDesc.Height = colorDesc.Height;
        depthDesc.ArraySize = colorDesc.ArraySize;
        depthDesc.MipLevels = 1;
        depthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        depthDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
        depthDesc.SampleDesc.Count = 1;
        ComPtr<ID3D11Texture2D> depthTexture;
        CHECK_HRCMD(m_device->CreateTexture2D(&depthDesc, nullptr, depthTexture.ReleaseAndGetAddressOf()));

        // Create and cache the depth stencil view.
        ComPtr<ID3D11DepthStencilView> depthStencilView;
        CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D, DXGI_FORMAT_D32_FLOAT);
        CHECK_HRCMD(m_device->CreateDepthStencilView(depthTexture.Get(), &depthStencilViewDesc, depthStencilView.GetAddressOf()));
        depthBufferIt = m_colorToDepthMap.insert(std::make_pair(colorTexture, depthStencilView)).first;

        return depthStencilView;
    }

    void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* swapchainImage,
                    int64_t swapchainFormat, const std::vector<Cube>& cubes) override {
        CHECK(layerView.subImage.imageArrayIndex == 0);  // Texture arrays not supported.

        ID3D11Texture2D* const colorTexture = reinterpret_cast<const XrSwapchainImageD3D11KHR*>(swapchainImage)->texture;

        CD3D11_VIEWPORT viewport((float)layerView.subImage.imageRect.offset.x, (float)layerView.subImage.imageRect.offset.y,
                                 (float)layerView.subImage.imageRect.extent.width,
                                 (float)layerView.subImage.imageRect.extent.height);
        m_deviceContext->RSSetViewports(1, &viewport);

        // Create RenderTargetView with original swapchain format (swapchain is typeless).
        ComPtr<ID3D11RenderTargetView> renderTargetView;
        const CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc(D3D11_RTV_DIMENSION_TEXTURE2D, (DXGI_FORMAT)swapchainFormat);
        CHECK_HRCMD(
            m_device->CreateRenderTargetView(colorTexture, &renderTargetViewDesc, renderTargetView.ReleaseAndGetAddressOf()));

        const ComPtr<ID3D11DepthStencilView> depthStencilView = GetDepthStencilView(colorTexture);

        // Clear swapchain and depth buffer. NOTE: This will clear the entire render target view, not just the specified view.
        m_deviceContext->ClearRenderTargetView(renderTargetView.Get(), static_cast<const FLOAT*>(m_clearColor.data()));
        m_deviceContext->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        ID3D11RenderTargetView* renderTargets[] = {renderTargetView.Get()};
        m_deviceContext->OMSetRenderTargets((UINT)ArraySize(renderTargets), renderTargets, depthStencilView.Get());

        const XMMATRIX spaceToView = XMMatrixInverse(nullptr, LoadXrPose(layerView.pose));
        XrMatrix4x4f projectionMatrix;
        XrMatrix4x4f_CreateProjectionFov(&projectionMatrix, GRAPHICS_D3D, layerView.fov, 0.05f, 100.0f);

        // Set shaders and constant buffers.
        ViewProjectionConstantBuffer viewProjection;
        XMStoreFloat4x4(&viewProjection.ViewProjection, XMMatrixTranspose(spaceToView * LoadXrMatrix(projectionMatrix)));
        m_deviceContext->UpdateSubresource(m_viewProjectionCBuffer.Get(), 0, nullptr, &viewProjection, 0, 0);

        ID3D11Buffer* const constantBuffers[] = {m_modelCBuffer.Get(), m_viewProjectionCBuffer.Get()};
        m_deviceContext->VSSetConstantBuffers(0, (UINT)ArraySize(constantBuffers), constantBuffers);
        m_deviceContext->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        m_deviceContext->PSSetShader(m_pixelShader.Get(), nullptr, 0);

        // Set cube primitive data.
        const UINT strides[] = {sizeof(Geometry::Vertex)};
        const UINT offsets[] = {0};
        ID3D11Buffer* vertexBuffers[] = {m_cubeVertexBuffer.Get()};
        m_deviceContext->IASetVertexBuffers(0, (UINT)ArraySize(vertexBuffers), vertexBuffers, strides, offsets);
        m_deviceContext->IASetIndexBuffer(m_cubeIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_deviceContext->IASetInputLayout(m_inputLayout.Get());

        // Render each cube
        for (const Cube& cube : cubes) {
            // Compute and update the model transform.
            ModelConstantBuffer model;
            XMStoreFloat4x4(&model.Model,
                            XMMatrixTranspose(XMMatrixScaling(cube.Scale.x, cube.Scale.y, cube.Scale.z) * LoadXrPose(cube.Pose)));
            m_deviceContext->UpdateSubresource(m_modelCBuffer.Get(), 0, nullptr, &model, 0, 0);

            // Draw the cube.
            m_deviceContext->DrawIndexed((UINT)ArraySize(Geometry::c_cubeIndices), 0, 0);
        }
    }

    uint32_t GetSupportedSwapchainSampleCount(const XrViewConfigurationView&) override { return 1; }

    void UpdateOptions(const std::shared_ptr<Options>& options) override { m_clearColor = options->GetBackgroundClearColor(); }

   private:
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_deviceContext;
    XrGraphicsBindingD3D11KHR m_graphicsBinding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    std::list<std::vector<XrSwapchainImageD3D11KHR>> m_swapchainImageBuffers;
    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11InputLayout> m_inputLayout;
    ComPtr<ID3D11Buffer> m_modelCBuffer;
    ComPtr<ID3D11Buffer> m_viewProjectionCBuffer;
    ComPtr<ID3D11Buffer> m_cubeVertexBuffer;
    ComPtr<ID3D11Buffer> m_cubeIndexBuffer;

    // Map color buffer to associated depth buffer. This map is populated on demand.
    std::map<ID3D11Texture2D*, ComPtr<ID3D11DepthStencilView>> m_colorToDepthMap;
    std::array<float, 4> m_clearColor;
};
}  // namespace

std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_D3D11(const std::shared_ptr<Options>& options,
                                                            std::shared_ptr<IPlatformPlugin> platformPlugin) {
    return std::make_shared<D3D11GraphicsPlugin>(options, platformPlugin);
}

#endif
