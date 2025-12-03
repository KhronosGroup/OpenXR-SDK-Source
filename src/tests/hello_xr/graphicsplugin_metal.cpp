// Copyright (c) 2017-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "common.h"
#include "geometry.h"
#include "graphicsplugin.h"
#include "graphics_plugin_impl_helpers.h"
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
        m_swapchainImageDataMap.Clear();  // XXX CHECK IF THIS IS RIGHT
        m_cubeVerticesBuffer.reset();
        m_cubeIndicesBuffer.reset();
    }

    int64_t SelectColorSwapchainFormat(bool throwIfNotFound, span<const int64_t> imageFormatArray) const override {
        // List of supported color swapchain formats.
        return SelectSwapchainFormat(  //
            throwIfNotFound, imageFormatArray,
            {
                MTL::PixelFormatRGBA8Unorm_sRGB,
                MTL::PixelFormatBGRA8Unorm_sRGB,
                MTL::PixelFormatRGBA8Unorm,
                MTL::PixelFormatBGRA8Unorm,
            });
    }

    int64_t SelectDepthSwapchainFormat(bool throwIfNotFound, span<const int64_t> imageFormatArray) const override {
        // List of supported depth swapchain formats.
        return SelectSwapchainFormat(  //
            throwIfNotFound, imageFormatArray,
            {
                MTL::PixelFormatDepth32Float,
                MTL::PixelFormatDepth24Unorm_Stencil8,
                MTL::PixelFormatDepth16Unorm,
                MTL::PixelFormatDepth32Float_Stencil8,
            });
    }

    const XrBaseInStructure* GetGraphicsBinding() const override {
        return reinterpret_cast<const XrBaseInStructure*>(&m_graphicsBinding);
    }

    struct MetalFallbackDepthTexture {
       public:
        MetalFallbackDepthTexture() = default;

        void Reset() {
            m_texture.reset();
            m_xrImage.texture = nullptr;
        }
        bool Allocated() const { return m_texture.operator bool(); }

        void Allocate(MTL::Device* metalDevice, uint32_t width, uint32_t height, uint32_t arraySize, uint32_t sampleCount) {
            Reset();

            MTL::TextureDescriptor* desc =
                MTL::TextureDescriptor::texture2DDescriptor(GetDefaultDepthFormat(), width, height, false);
            if (sampleCount > 1) {
                if (arraySize > 1) {
                    desc->setTextureType(MTL::TextureType2DMultisampleArray);
                    desc->setArrayLength(arraySize);
                } else {
                    desc->setTextureType(MTL::TextureType2DMultisample);
                }
                desc->setSampleCount(sampleCount);
            } else {
                if (arraySize > 1) {
                    desc->setTextureType(MTL::TextureType2DArray);
                    desc->setArrayLength(arraySize);
                } else {
                    desc->setTextureType(MTL::TextureType2D);
                }
            }
            desc->setUsage(MTL::TextureUsageRenderTarget);
            desc->setStorageMode(MTL::StorageModePrivate);  // to be compatible with Intel-based Mac
            m_texture = NS::TransferPtr(metalDevice->newTexture(desc));
            XRC_CHECK_THROW(m_texture);
            m_xrImage.texture = m_texture.get();
        }

        const XrSwapchainImageMetalKHR& GetTexture() const { return m_xrImage; }

        static MTL::PixelFormat GetDefaultDepthFormat() { return MTL::PixelFormatDepth32Float; }

       private:
        NS::SharedPtr<MTL::Texture> m_texture{};
        XrSwapchainImageMetalKHR m_xrImage{XR_TYPE_SWAPCHAIN_IMAGE_METAL_KHR, NULL, nullptr};
    };

    struct SwapchainContext {
        NS::SharedPtr<MTL::Buffer> m_cubeMatricesBuffer;
    };

    class MetalSwapchainImageData : public SwapchainImageDataBase<XrSwapchainImageMetalKHR> {
       public:
        MetalSwapchainImageData(NS::SharedPtr<MTL::Device> device, uint32_t capacity, const XrSwapchainCreateInfo& createInfo,
                                XrSwapchain depthSwapchain, const XrSwapchainCreateInfo& depthCreateInfo)
            : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_METAL_KHR, capacity, createInfo, depthSwapchain, depthCreateInfo),
              m_device(device) {}
        MetalSwapchainImageData(NS::SharedPtr<MTL::Device> device, uint32_t capacity, const XrSwapchainCreateInfo& createInfo)
            : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_METAL_KHR, capacity, createInfo),
              m_device(device),
              m_internalDepthTextures(capacity) {}

        void Reset() override {
            m_pipelineStateObject.reset();
            m_internalDepthTextures.clear();
            m_device.reset();
            SwapchainImageDataBase::Reset();
        }

        const XrSwapchainImageMetalKHR& GetFallbackDepthSwapchainImage(uint32_t i) override {
            if (!m_internalDepthTextures[i].Allocated()) {
                m_internalDepthTextures[i].Allocate(m_device.get(), this->Width(), this->Height(), this->ArraySize(),
                                                    this->DepthSampleCount());
            }

            return m_internalDepthTextures[i].GetTexture();
        }

        NS::SharedPtr<MTL::RenderPipelineState> GetPipelineStateObject(NS::SharedPtr<MTL::Function> vertexFunction,
                                                                       NS::SharedPtr<MTL::Function> fragmentFunction) {
            if (!m_pipelineStateObject || vertexFunction != m_cachedVertexFunction ||
                fragmentFunction != m_cachedFragmentFunction) {
                auto pDesc = NS::TransferPtr(MTL::RenderPipelineDescriptor::alloc()->init());
                pDesc->setVertexFunction(vertexFunction.get());
                pDesc->setFragmentFunction(fragmentFunction.get());
                pDesc->colorAttachments()->object(0)->setPixelFormat((MTL::PixelFormat)GetCreateInfo().format);
                pDesc->setDepthAttachmentPixelFormat(GetDepthCreateInfo() ? (MTL::PixelFormat)GetDepthCreateInfo()->format
                                                                          : MetalFallbackDepthTexture::GetDefaultDepthFormat());

                NS::Error* pError = nullptr;
                m_pipelineStateObject = NS::TransferPtr(m_device->newRenderPipelineState(pDesc.get(), &pError));
                XRC_CHECK_THROW(m_pipelineStateObject);
                m_cachedVertexFunction = vertexFunction;
                m_cachedFragmentFunction = fragmentFunction;
            }
            return m_pipelineStateObject;
        }

        SwapchainContext swapchainContext;

       private:
        NS::SharedPtr<MTL::Device> m_device;
        std::vector<MetalFallbackDepthTexture> m_internalDepthTextures;
        NS::SharedPtr<MTL::Function> m_cachedVertexFunction;
        NS::SharedPtr<MTL::Function> m_cachedFragmentFunction;
        NS::SharedPtr<MTL::RenderPipelineState> m_pipelineStateObject;
    };

    ISwapchainImageData* AllocateSwapchainImageData(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo) override {
        auto typedResult = std::make_unique<MetalSwapchainImageData>(m_device, uint32_t(size), swapchainCreateInfo);

        // XXX TODO DOES SWAPCHAINCONTEXT need to be initialized?

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    ISwapchainImageData* AllocateSwapchainImageDataWithDepthSwapchain(
        size_t size, const XrSwapchainCreateInfo& colorSwapchainCreateInfo, XrSwapchain depthSwapchain,
        const XrSwapchainCreateInfo& depthSwapchainCreateInfo) override {
        auto typedResult = std::make_unique<MetalSwapchainImageData>(m_device, uint32_t(size), colorSwapchainCreateInfo,
                                                                     depthSwapchain, depthSwapchainCreateInfo);

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* swapchainImage,
                    int64_t swapchainFormat, const std::vector<Cube>& cubes) override {
        auto pAutoReleasePool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());

        SwapchainContext& swapchainContext =
            m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(swapchainImage).first->swapchainContext;

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

    SwapchainImageDataMap<MetalSwapchainImageData> m_swapchainImageDataMap;

    NS::SharedPtr<MTL::Texture> m_depthStencilTexture;

    std::array<float, 4> m_clearColor;
};

std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_Metal(const std::shared_ptr<Options>& options,
                                                            std::shared_ptr<IPlatformPlugin> platformPlugin) {
    return std::make_shared<MetalGraphicsPlugin>(options, platformPlugin);
}

#endif
