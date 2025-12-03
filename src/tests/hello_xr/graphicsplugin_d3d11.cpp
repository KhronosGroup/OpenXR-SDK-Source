// Copyright (c) 2017-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "common.h"
#include "geometry.h"
#include "graphicsplugin.h"
#include "graphics_plugin_impl_helpers.h"
#include "options.h"

#if defined(XR_USE_GRAPHICS_API_D3D11)

#include <common/xr_linear.h>
#include <DirectXColors.h>
#include <D3Dcompiler.h>

#include "d3d_common.h"

using namespace Microsoft::WRL;
using namespace DirectX;

#define XRC_CHECK_THROW_HRCMD CHECK_HRCMD

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

    // Select the preferred swapchain format from the list of available formats.
    int64_t SelectColorSwapchainFormat(bool throwIfNotFound, span<const int64_t> imageFormatArray) const override {
        // List of supported color swapchain formats.
        return SelectSwapchainFormat(  //
            throwIfNotFound, imageFormatArray,
            {
                DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                DXGI_FORMAT_B8G8R8A8_UNORM,
            });
    }

    // Select the preferred swapchain format from the list of available formats.
    int64_t SelectDepthSwapchainFormat(bool throwIfNotFound, span<const int64_t> imageFormatArray) const override {
        // List of supported depth swapchain formats.
        return SelectSwapchainFormat(  //
            throwIfNotFound, imageFormatArray,
            {
                DXGI_FORMAT_D32_FLOAT,
                DXGI_FORMAT_D24_UNORM_S8_UINT,
                DXGI_FORMAT_D16_UNORM,
                DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
            });
    }

    const XrBaseInStructure* GetGraphicsBinding() const override {
        return reinterpret_cast<const XrBaseInStructure*>(&m_graphicsBinding);
    }

    struct D3D11FallbackDepthTexture {
       public:
        D3D11FallbackDepthTexture() = default;

        void Reset() {
            m_texture = nullptr;
            m_xrImage.texture = nullptr;
        }
        bool Allocated() const { return m_texture != nullptr; }

        void Allocate(ID3D11Device* d3d11Device, UINT width, UINT height, UINT arraySize) {
            Reset();
            D3D11_TEXTURE2D_DESC depthDesc{};
            depthDesc.Width = width;
            depthDesc.Height = height;
            depthDesc.ArraySize = arraySize;
            depthDesc.MipLevels = 1;
            depthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            depthDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
            depthDesc.SampleDesc.Count = 1;
            XRC_CHECK_THROW_HRCMD(d3d11Device->CreateTexture2D(&depthDesc, nullptr, m_texture.ReleaseAndGetAddressOf()));
            m_xrImage.texture = m_texture.Get();
        }

        const XrSwapchainImageD3D11KHR& GetTexture() const { return m_xrImage; }

       private:
        ComPtr<ID3D11Texture2D> m_texture{};
        XrSwapchainImageD3D11KHR m_xrImage{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR};
    };

    class D3D11SwapchainImageData : public SwapchainImageDataBase<XrSwapchainImageD3D11KHR> {
       public:
        D3D11SwapchainImageData(ComPtr<ID3D11Device> device, uint32_t capacity, const XrSwapchainCreateInfo& createInfo,
                                XrSwapchain depthSwapchain, const XrSwapchainCreateInfo& depthCreateInfo)
            : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR, capacity, createInfo, depthSwapchain, depthCreateInfo),
              m_device(std::move(device))

        {}
        D3D11SwapchainImageData(ComPtr<ID3D11Device> device, uint32_t capacity, const XrSwapchainCreateInfo& createInfo)
            : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR, capacity, createInfo),
              m_device(std::move(device)),
              m_internalDepthTextures(capacity) {}

        void Reset() override {
            m_internalDepthTextures.clear();
            m_device = nullptr;
            SwapchainImageDataBase::Reset();
        }

        const XrSwapchainImageD3D11KHR& GetFallbackDepthSwapchainImage(uint32_t i) override {
            if (!m_internalDepthTextures[i].Allocated()) {
                m_internalDepthTextures[i].Allocate(m_device.Get(), this->Width(), this->Height(), this->ArraySize());
            }

            return m_internalDepthTextures[i].GetTexture();
        }

       private:
        ComPtr<ID3D11Device> m_device;
        std::vector<D3D11FallbackDepthTexture> m_internalDepthTextures;
    };

    ISwapchainImageData* AllocateSwapchainImageData(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo) override {
        auto d3d11Device = m_device;
        auto typedResult = std::make_unique<D3D11SwapchainImageData>(d3d11Device, uint32_t(size), swapchainCreateInfo);

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    inline ISwapchainImageData* AllocateSwapchainImageDataWithDepthSwapchain(
        size_t size, const XrSwapchainCreateInfo& colorSwapchainCreateInfo, XrSwapchain depthSwapchain,
        const XrSwapchainCreateInfo& depthSwapchainCreateInfo) override {
        auto d3d11Device = m_device;
        auto typedResult = std::make_unique<D3D11SwapchainImageData>(d3d11Device, uint32_t(size), colorSwapchainCreateInfo,
                                                                     depthSwapchain, depthSwapchainCreateInfo);

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    ComPtr<ID3D11RenderTargetView> CreateRenderTargetView(D3D11SwapchainImageData& swapchainData, uint32_t imageIndex,
                                                          uint32_t imageArrayIndex) const {
        auto d3d11Device = m_device;

        // Create RenderTargetView with original swapchain format (swapchain is typeless).
        ComPtr<ID3D11RenderTargetView> renderTargetView;
        const CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc(
            (swapchainData.SampleCount() > 1) ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D11_RTV_DIMENSION_TEXTURE2DARRAY,
            (DXGI_FORMAT)swapchainData.GetCreateInfo().format, 0 /* mipSlice */, imageArrayIndex, 1 /* arraySize */);

        ID3D11Texture2D* const colorTexture = swapchainData.GetTypedImage(imageIndex).texture;

        XRC_CHECK_THROW_HRCMD(
            d3d11Device->CreateRenderTargetView(colorTexture, &renderTargetViewDesc, renderTargetView.ReleaseAndGetAddressOf()));
        return renderTargetView;
    }

    ComPtr<ID3D11DepthStencilView> CreateDepthStencilView(D3D11SwapchainImageData& swapchainData, uint32_t imageIndex,
                                                          uint32_t imageArrayIndex) const {
        auto d3d11Device = m_device;

        // Clear depth buffer.
        const ComPtr<ID3D11Texture2D> depthStencilTexture = swapchainData.GetDepthImageForColorIndex(imageIndex).texture;
        ComPtr<ID3D11DepthStencilView> depthStencilView;
        const XrSwapchainCreateInfo* depthCreateInfo = swapchainData.GetDepthCreateInfo();
        DXGI_FORMAT depthSwapchainFormatDX = (DXGI_FORMAT)depthCreateInfo->format;
        CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(
            (swapchainData.DepthSampleCount() > 1) ? D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY : D3D11_DSV_DIMENSION_TEXTURE2DARRAY,
            depthSwapchainFormatDX, 0 /* mipSlice */, imageArrayIndex, 1 /* arraySize */);
        XRC_CHECK_THROW_HRCMD(
            d3d11Device->CreateDepthStencilView(depthStencilTexture.Get(), &depthStencilViewDesc, depthStencilView.GetAddressOf()));
        return depthStencilView;
    }

    void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* swapchainImage,
                    int64_t /* swapchainFormat */, const std::vector<Cube>& cubes) override {
        CHECK(layerView.subImage.imageArrayIndex == 0);  // Texture arrays not supported.

        D3D11SwapchainImageData* swapchainData;
        uint32_t imageIndex;

        std::tie(swapchainData, imageIndex) = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(swapchainImage);

        CD3D11_VIEWPORT viewport((float)layerView.subImage.imageRect.offset.x, (float)layerView.subImage.imageRect.offset.y,
                                 (float)layerView.subImage.imageRect.extent.width,
                                 (float)layerView.subImage.imageRect.extent.height);
        m_deviceContext->RSSetViewports(1, &viewport);

        // Create RenderTargetView with original swapchain format (swapchain is typeless).
        ComPtr<ID3D11RenderTargetView> renderTargetView =
            CreateRenderTargetView(*swapchainData, imageIndex, layerView.subImage.imageArrayIndex);

        ComPtr<ID3D11DepthStencilView> depthStencilView =
            CreateDepthStencilView(*swapchainData, imageIndex, layerView.subImage.imageArrayIndex);

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
    SwapchainImageDataMap<D3D11SwapchainImageData> m_swapchainImageDataMap;
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
