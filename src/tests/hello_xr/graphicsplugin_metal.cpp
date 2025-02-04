// Copyright (c) 2017-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "common.h"
#include "geometry.h"
#include "graphicsplugin.h"
#include "options.h"

#if defined(XR_USE_GRAPHICS_API_METAL)

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <common/xr_linear.h>
#include <simd/simd.h>

struct MetalGraphicsPlugin : public IGraphicsPlugin {
    MetalGraphicsPlugin(const std::shared_ptr<Options>& /*options*/, std::shared_ptr<IPlatformPlugin>) {}

    ~MetalGraphicsPlugin() override {
        DestroyResources();
        m_commandQueue.reset();
        m_device.reset();
    }

    std::vector<std::string> GetInstanceExtensions() const override { return {XR_KHR_METAL_ENABLE_EXTENSION_NAME}; }

    void InitializeDevice(XrInstance instance, XrSystemId systemId) override {
        PFN_xrGetMetalGraphicsRequirementsKHR pfnGetMetalGraphicsRequirementsKHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetMetalGraphicsRequirementsKHR",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetMetalGraphicsRequirementsKHR)));

        XrGraphicsRequirementsMetalKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_METAL_KHR};
        CHECK_XRCMD(pfnGetMetalGraphicsRequirementsKHR(instance, systemId, &graphicsRequirements));

        m_device = NS::TransferPtr((MTL::Device*)graphicsRequirements.metalDevice);
        m_commandQueue = NS::TransferPtr(m_device->newCommandQueue());

        m_graphicsBinding.commandQueue = m_commandQueue.get();

        InitializeResources();
    }

    void InitializeResources() {
        using NS::StringEncoding::UTF8StringEncoding;

        const char* shaderSrc = R"(
            #include <metal_stdlib>
            using namespace metal;

            struct VertexBuffer {
                float4 position;
                float4 color;
            };

            struct v2f
            {
                float4 position [[position]];
                half4 color;
            };

            v2f vertex vertexMain( uint vertexId [[vertex_id]],
                                   uint instanceId [[instance_id]],
                                   device const VertexBuffer* vertexBuffer [[buffer(0)]],
                                   device const float4x4* matricesBuffer [[buffer(1)]] )
            {
                v2f o;
                float4 pos = vertexBuffer[vertexId].position;
                o.position = matricesBuffer[instanceId] * pos;
                o.color = half4(vertexBuffer[vertexId].color);
                return o;
            }

            half4 fragment fragmentMain( v2f in [[stage_in]] )
            {
                return in.color;
            }
        )";

        NS::Error* pError = nullptr;
        m_library = NS::TransferPtr(m_device->newLibrary(NS::String::string(shaderSrc, UTF8StringEncoding), nullptr, &pError));
        if (!m_library) {
            Log::Write(Log::Level::Error, Fmt("%s", pError->localizedDescription()->utf8String()));
            assert(false);
            return;
        }

        m_vertexFunction = NS::TransferPtr(m_library->newFunction(NS::String::string("vertexMain", UTF8StringEncoding)));
        m_fragmentFunction = NS::TransferPtr(m_library->newFunction(NS::String::string("fragmentMain", UTF8StringEncoding)));

        auto depthDescriptor = NS::TransferPtr(MTL::DepthStencilDescriptor::alloc()->init());
        depthDescriptor->setDepthCompareFunction(MTL::CompareFunctionLessEqual);
        depthDescriptor->setDepthWriteEnabled(true);
        m_depthStencilState = NS::TransferPtr(m_device->newDepthStencilState(depthDescriptor.get()));

        BuildBuffers();
    }

    void DestroyResources() {
        DestroyBuffers();

        m_depthStencilState.reset();
        m_vertexFunction.reset();
        m_fragmentFunction.reset();
        m_library.reset();
    }

    void BuildBuffers() {
        struct VertexData {
            simd::float4 position;
            simd::float4 color;
        };

        std::vector<VertexData> vertices;

        for (size_t i = 0; i < sizeof(Geometry::c_cubeVertices) / sizeof(Geometry::c_cubeVertices[0]); ++i) {
            const auto& d = Geometry::c_cubeVertices[i];
            VertexData v{simd_make_float4(d.Position.x, d.Position.y, d.Position.z, 1.0f),
                         simd_make_float4(d.Color.x, d.Color.y, d.Color.z, 1.0f)};
            vertices.push_back(v);
        }

        m_cubeVerticesBuffer =
            NS::TransferPtr(m_device->newBuffer(sizeof(VertexData) * vertices.size(), MTL::ResourceStorageModeManaged));
        m_cubeIndicesBuffer =
            NS::TransferPtr(m_device->newBuffer(sizeof(Geometry::c_cubeIndices), MTL::ResourceStorageModeManaged));

        memcpy(m_cubeVerticesBuffer->contents(), vertices.data(), m_cubeVerticesBuffer->length());
        memcpy(m_cubeIndicesBuffer->contents(), Geometry::c_cubeIndices, m_cubeIndicesBuffer->length());
        m_cubeVerticesBuffer->didModifyRange(NS::Range::Make(0, m_cubeVerticesBuffer->length()));
        m_cubeIndicesBuffer->didModifyRange(NS::Range::Make(0, m_cubeIndicesBuffer->length()));
    }

    void DestroyBuffers() {
        m_swapchainContextMap.clear();
        m_cubeVerticesBuffer.reset();
        m_cubeIndicesBuffer.reset();
    }

    int64_t SelectColorSwapchainFormat(const std::vector<int64_t>& runtimeFormats) const override {
        // List of supported color swapchain formats.
        constexpr MTL::PixelFormat SupportedColorSwapchainFormats[] = {MTL::PixelFormatRGBA8Unorm_sRGB, MTL::PixelFormatRGBA8Unorm};

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
        std::vector<XrSwapchainImageMetalKHR> swapchainImageBuffer(capacity);
        std::vector<XrSwapchainImageBaseHeader*> swapchainImageBase;
        for (XrSwapchainImageMetalKHR& image : swapchainImageBuffer) {
            image.type = XR_TYPE_SWAPCHAIN_IMAGE_METAL_KHR;
            swapchainImageBase.push_back(reinterpret_cast<XrSwapchainImageBaseHeader*>(&image));
        }

        // Keep the buffer alive by moving it into the list of buffers.
        m_swapchainImageBuffers.push_back(std::move(swapchainImageBuffer));

        for (auto imageBaseHeader : swapchainImageBase) {
            m_swapchainContextMap.insert({imageBaseHeader, SwapchainContext()});
        }

        return swapchainImageBase;
    }

    void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* swapchainImage,
                    int64_t swapchainFormat, const std::vector<Cube>& cubes) override {
        auto pAutoReleasePool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());

        SwapchainContext& swapchainContext = m_swapchainContextMap[swapchainImage];

        auto mtlSwapchainFormat = (MTL::PixelFormat)swapchainFormat;
        if (mtlSwapchainFormat != m_colorAttachmentFormat) {
            auto pDesc = NS::TransferPtr(MTL::RenderPipelineDescriptor::alloc()->init());
            pDesc->setVertexFunction(m_vertexFunction.get());
            pDesc->setFragmentFunction(m_fragmentFunction.get());
            pDesc->colorAttachments()->object(0)->setPixelFormat(mtlSwapchainFormat);
            pDesc->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);

            NS::Error* pError = nullptr;
            m_pipelineStateObject = NS::TransferPtr(m_device->newRenderPipelineState(pDesc.get(), &pError));
            if (!m_pipelineStateObject) {
                Log::Write(Log::Level::Error, Fmt("%s", pError->localizedDescription()->utf8String()));
                assert(false);
                return;
            }
            m_colorAttachmentFormat = mtlSwapchainFormat;
        }

        CHECK(layerView.subImage.imageArrayIndex == 0);  // Texture arrays not supported.
        void* rawTexture = reinterpret_cast<const XrSwapchainImageMetalKHR*>(swapchainImage)->texture;
        NS::SharedPtr<MTL::Texture> colorTexture = NS::RetainPtr(reinterpret_cast<MTL::Texture*>(rawTexture));

        if (!m_depthStencilTexture) {
            auto depthTextureDescriptor = NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
            depthTextureDescriptor->setTextureType(colorTexture->textureType());
            depthTextureDescriptor->setPixelFormat(MTL::PixelFormatDepth32Float);
            depthTextureDescriptor->setWidth(colorTexture->width());
            depthTextureDescriptor->setHeight(colorTexture->height());
            depthTextureDescriptor->setUsage(MTL::TextureUsageRenderTarget);
            depthTextureDescriptor->setStorageMode(MTL::StorageModePrivate);
            m_depthStencilTexture = NS::TransferPtr(m_device->newTexture(depthTextureDescriptor.get()));
        }

        MTL::CommandBuffer* pCmd = m_commandQueue->commandBuffer();

        auto renderPassDesc = NS::TransferPtr(MTL::RenderPassDescriptor::alloc()->init());
        renderPassDesc->colorAttachments()->object(0)->setTexture(colorTexture.get());
        renderPassDesc->colorAttachments()->object(0)->setClearColor(
            MTL::ClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]));
        renderPassDesc->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
        renderPassDesc->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
        renderPassDesc->depthAttachment()->setTexture(m_depthStencilTexture.get());
        renderPassDesc->depthAttachment()->setLoadAction(MTL::LoadActionClear);
        renderPassDesc->depthAttachment()->setStoreAction(MTL::StoreActionStore);

        MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder(renderPassDesc.get());

        MTL::Viewport viewport{(double)layerView.subImage.imageRect.offset.x,
                               (double)layerView.subImage.imageRect.offset.y,
                               (double)layerView.subImage.imageRect.extent.width,
                               (double)layerView.subImage.imageRect.extent.height,
                               0.0,
                               1.0};
        pEnc->setViewport(viewport);
        pEnc->setDepthStencilState(m_depthStencilState.get());
        pEnc->setCullMode(MTL::CullModeBack);

        // Compute the view-projection transform.
        // Note all matrixes are column-major, right-handed.
        const auto& pose = layerView.pose;
        XrMatrix4x4f proj;
        XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_METAL, layerView.fov, 0.05f, 100.0f);
        XrMatrix4x4f toView;
        XrVector3f scale{1.f, 1.f, 1.f};
        XrMatrix4x4f_CreateTranslationRotationScale(&toView, &pose.position, &pose.orientation, &scale);
        XrMatrix4x4f view;
        XrMatrix4x4f_InvertRigidBody(&view, &toView);
        XrMatrix4x4f vp;
        XrMatrix4x4f_Multiply(&vp, &proj, &view);

        static_assert(sizeof(XrMatrix4x4f) == sizeof(simd::float4x4), "Unexpected matrix size");

        size_t matricesBufferLength = cubes.size() * sizeof(simd::float4x4);
        if (!swapchainContext.m_cubeMatricesBuffer || swapchainContext.m_cubeMatricesBuffer->length() != matricesBufferLength) {
            swapchainContext.m_cubeMatricesBuffer =
                NS::TransferPtr(m_device->newBuffer(matricesBufferLength, MTL::ResourceStorageModeManaged));
        }

        auto matricesBufferData = (simd::float4x4*)swapchainContext.m_cubeMatricesBuffer->contents();
        for (auto i = 0; i < cubes.size(); ++i) {
            auto& cube = cubes[i];
            // Compute and update the model transform.
            XrMatrix4x4f model;
            XrMatrix4x4f_CreateTranslationRotationScale(&model, &cube.Pose.position, &cube.Pose.orientation, &cube.Scale);
            XrMatrix4x4f mvp;
            XrMatrix4x4f_Multiply(&mvp, &vp, &model);
            memcpy(&matricesBufferData[i], &mvp, sizeof(simd::float4x4));
        }
        swapchainContext.m_cubeMatricesBuffer->didModifyRange(NS::Range::Make(0, swapchainContext.m_cubeMatricesBuffer->length()));

        pEnc->setRenderPipelineState(m_pipelineStateObject.get());
        pEnc->setVertexBuffer(m_cubeVerticesBuffer.get(), 0, 0);
        pEnc->setVertexBuffer(swapchainContext.m_cubeMatricesBuffer.get(), 0, 1);
        uint32_t numCubeIdicies = sizeof(Geometry::c_cubeIndices) / sizeof(Geometry::c_cubeIndices[0]);
        pEnc->drawIndexedPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, numCubeIdicies, MTL::IndexTypeUInt16,
                                    m_cubeIndicesBuffer.get(), 0, cubes.size());

        pEnc->endEncoding();
        pCmd->commit();
    }

    void UpdateOptions(const std::shared_ptr<Options>& options) override { m_clearColor = options->GetBackgroundClearColor(); }

   private:
    NS::SharedPtr<MTL::Device> m_device;
    NS::SharedPtr<MTL::CommandQueue> m_commandQueue;
    NS::SharedPtr<MTL::RenderPipelineState> m_pipelineStateObject;
    NS::SharedPtr<MTL::DepthStencilState> m_depthStencilState;
    MTL::PixelFormat m_colorAttachmentFormat{MTL::PixelFormatInvalid};

    NS::SharedPtr<MTL::Library> m_library;
    NS::SharedPtr<MTL::Function> m_vertexFunction;
    NS::SharedPtr<MTL::Function> m_fragmentFunction;

    NS::SharedPtr<MTL::Buffer> m_cubeVerticesBuffer;
    NS::SharedPtr<MTL::Buffer> m_cubeIndicesBuffer;

    XrGraphicsBindingMetalKHR m_graphicsBinding{XR_TYPE_GRAPHICS_BINDING_METAL_KHR};
    std::list<std::vector<XrSwapchainImageMetalKHR>> m_swapchainImageBuffers;

    struct SwapchainContext {
        NS::SharedPtr<MTL::Buffer> m_cubeMatricesBuffer;
    };
    std::map<const XrSwapchainImageBaseHeader*, SwapchainContext> m_swapchainContextMap;

    NS::SharedPtr<MTL::Texture> m_depthStencilTexture;

    std::array<float, 4> m_clearColor;
};

std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_Metal(const std::shared_ptr<Options>& options,
                                                            std::shared_ptr<IPlatformPlugin> platformPlugin) {
    return std::make_shared<MetalGraphicsPlugin>(options, platformPlugin);
}

#endif
