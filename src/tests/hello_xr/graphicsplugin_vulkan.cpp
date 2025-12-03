// Copyright (c) 2017-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "common.h"
#include "geometry.h"
#include "graphicsplugin.h"
#include "graphics_plugin_impl_helpers.h"
#include "options.h"
#include <nonstd/span.hpp>
#include "check.h"

#ifdef XR_USE_GRAPHICS_API_VULKAN
#include <common/vulkan_debug_object_namer.hpp>
#include <common/xr_linear.h>
#include "vulkan_utils.h"

#ifdef USE_ONLINE_VULKAN_SHADERC
#include <shaderc/shaderc.hpp>
#endif

#if defined(VK_USE_PLATFORM_WIN32_KHR)
// Define USE_MIRROR_WINDOW to open a otherwise-unused window for e.g. RenderDoc
#define USE_MIRROR_WINDOW
#endif

// glslangValidator doesn't wrap its output in brackets if you don't have it define the whole array.
#if defined(USE_GLSLANGVALIDATOR)
#define SPV_PREFIX {
#define SPV_SUFFIX }
#else
#define SPV_PREFIX
#define SPV_SUFFIX
#endif

namespace {

using nonstd::span;

#ifdef USE_ONLINE_VULKAN_SHADERC
constexpr char VertexShaderGlsl[] =
    R"_(
    #version 430
    #extension GL_ARB_separate_shader_objects : enable

    layout (std140, push_constant) uniform buf
    {
        mat4 mvp;
    } ubuf;

    layout (location = 0) in vec3 Position;
    layout (location = 1) in vec3 Color;

    layout (location = 0) out vec4 oColor;
    out gl_PerVertex
    {
        vec4 gl_Position;
    };

    void main()
    {
        oColor.rgba  = Color.rgba;
        gl_Position = ubuf.mvp * Position;
    }
)_";

constexpr char FragmentShaderGlsl[] =
    R"_(
    #version 430
    #extension GL_ARB_separate_shader_objects : enable

    layout (location = 0) in vec4 oColor;

    layout (location = 0) out vec4 FragColor;

    void main()
    {
        FragColor = oColor;
    }
)_";
#endif  // USE_ONLINE_VULKAN_SHADERC

#if defined(USE_MIRROR_WINDOW)
// Swapchain
struct Swapchain {
    VkFormat format{VK_FORMAT_B8G8R8A8_SRGB};
    VkSurfaceKHR surface{VK_NULL_HANDLE};
    VkSwapchainKHR swapchain{VK_NULL_HANDLE};
    VkFence readyFence{VK_NULL_HANDLE};
    VkFence presentFence{VK_NULL_HANDLE};
    static const uint32_t maxImages = 4;
    uint32_t swapchainCount = 0;
    uint32_t renderImageIdx = 0;
    VkImage image[maxImages]{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

    Swapchain() {}
    ~Swapchain() { Release(); }

    void Create(VkInstance instance, VkPhysicalDevice physDevice, VkDevice device, uint32_t queueFamilyIndex);
    void Prepare(VkCommandBuffer buf);
    void Wait();
    void Acquire(VkSemaphore readySemaphore = VK_NULL_HANDLE);
    void Present(VkQueue queue, VkSemaphore drawComplete = VK_NULL_HANDLE);
    void Release() {
        if (m_vkDevice) {
            // Flush any pending Present() calls which are using the fence
            Wait();
            if (swapchain) vkDestroySwapchainKHR(m_vkDevice, swapchain, nullptr);
            if (readyFence) vkDestroyFence(m_vkDevice, readyFence, nullptr);
        }

        if (m_vkInstance && surface) vkDestroySurfaceKHR(m_vkInstance, surface, nullptr);

        readyFence = VK_NULL_HANDLE;
        presentFence = VK_NULL_HANDLE;
        swapchain = VK_NULL_HANDLE;
        surface = VK_NULL_HANDLE;
        for (uint32_t i = 0; i < swapchainCount; ++i) {
            image[i] = VK_NULL_HANDLE;
        }
        swapchainCount = 0;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
        if (hWnd) {
            DestroyWindow(hWnd);
            hWnd = nullptr;
            UnregisterClassW(L"hello_xr", hInst);
        }
        if (hUser32Dll != NULL) {
            ::FreeLibrary(hUser32Dll);
            hUser32Dll = NULL;
        }
#endif

        m_vkDevice = nullptr;
    }
    void Recreate() {
        Release();
        Create(m_vkInstance, m_vkPhysicalDevice, m_vkDevice, m_queueFamilyIndex);
    }

   private:
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    HINSTANCE hInst{NULL};
    HWND hWnd{NULL};
    HINSTANCE hUser32Dll{NULL};
#endif
    const VkExtent2D size{640, 480};
    VkInstance m_vkInstance{VK_NULL_HANDLE};
    VkPhysicalDevice m_vkPhysicalDevice{VK_NULL_HANDLE};
    VkDevice m_vkDevice{VK_NULL_HANDLE};
    uint32_t m_queueFamilyIndex = 0;
};

void Swapchain::Create(VkInstance instance, VkPhysicalDevice physDevice, VkDevice device, uint32_t queueFamilyIndex) {
    m_vkInstance = instance;
    m_vkPhysicalDevice = physDevice;
    m_vkDevice = device;
    m_queueFamilyIndex = queueFamilyIndex;

// Create a WSI surface for the window:
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    hInst = GetModuleHandle(NULL);

    WNDCLASSW wc{};
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = DefWindowProcW;
    wc.cbWndExtra = sizeof(this);
    wc.hInstance = hInst;
    wc.lpszClassName = L"hello_xr";
    RegisterClassW(&wc);

// adjust the window size and show at InitDevice time
#if defined(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)
    typedef DPI_AWARENESS_CONTEXT(WINAPI * PFN_SetThreadDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);
    hUser32Dll = ::LoadLibraryA("user32.dll");
    if (PFN_SetThreadDpiAwarenessContext SetThreadDpiAwarenessContextFn =
            reinterpret_cast<PFN_SetThreadDpiAwarenessContext>(::GetProcAddress(hUser32Dll, "SetThreadDpiAwarenessContext"))) {
        // Make sure we're 1:1 for HMD pixels
        SetThreadDpiAwarenessContextFn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }
#endif
    RECT rect{0, 0, (LONG)size.width, (LONG)size.height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
    hWnd = CreateWindowW(wc.lpszClassName, L"hello_xr (Vulkan)", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                         rect.right - rect.left, rect.bottom - rect.top, 0, 0, hInst, 0);
    assert(hWnd != NULL);

    SetWindowLongPtr(hWnd, 0, LONG_PTR(this));

    VkWin32SurfaceCreateInfoKHR surfCreateInfo{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    surfCreateInfo.flags = 0;
    surfCreateInfo.hinstance = hInst;
    surfCreateInfo.hwnd = hWnd;
    XRC_CHECK_THROW_VKCMD(vkCreateWin32SurfaceKHR(m_vkInstance, &surfCreateInfo, nullptr, &surface));
#else
#error CreateSurface not supported on this OS
#endif  // defined(VK_USE_PLATFORM_WIN32_KHR)

    VkSurfaceCapabilitiesKHR surfCaps;
    XRC_CHECK_THROW_VKCMD(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_vkPhysicalDevice, surface, &surfCaps));
    CHECK(surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    uint32_t surfFmtCount = 0;
    XRC_CHECK_THROW_VKCMD(vkGetPhysicalDeviceSurfaceFormatsKHR(m_vkPhysicalDevice, surface, &surfFmtCount, nullptr));
    std::vector<VkSurfaceFormatKHR> surfFmts(surfFmtCount);
    XRC_CHECK_THROW_VKCMD(vkGetPhysicalDeviceSurfaceFormatsKHR(m_vkPhysicalDevice, surface, &surfFmtCount, &surfFmts[0]));
    uint32_t foundFmt;
    for (foundFmt = 0; foundFmt < surfFmtCount; ++foundFmt) {
        if (surfFmts[foundFmt].format == format) break;
    }

    CHECK(foundFmt < surfFmtCount);

    uint32_t presentModeCount = 0;
    XRC_CHECK_THROW_VKCMD(vkGetPhysicalDeviceSurfacePresentModesKHR(m_vkPhysicalDevice, surface, &presentModeCount, nullptr));
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    XRC_CHECK_THROW_VKCMD(
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_vkPhysicalDevice, surface, &presentModeCount, &presentModes[0]));

    // Do not use VSYNC for the mirror window, but Nvidia doesn't support IMMEDIATE so fall back to MAILBOX
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    for (uint32_t i = 0; i < presentModeCount; ++i) {
        if ((presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) || (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)) {
            presentMode = presentModes[i];
            break;
        }
    }

    VkBool32 presentable = false;
    XRC_CHECK_THROW_VKCMD(vkGetPhysicalDeviceSurfaceSupportKHR(m_vkPhysicalDevice, m_queueFamilyIndex, surface, &presentable));
    CHECK(presentable);

    VkSwapchainCreateInfoKHR swapchainInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swapchainInfo.flags = 0;
    swapchainInfo.surface = surface;
    swapchainInfo.minImageCount = surfCaps.minImageCount;
    swapchainInfo.imageFormat = format;
    swapchainInfo.imageColorSpace = surfFmts[foundFmt].colorSpace;
    swapchainInfo.imageExtent = size;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.queueFamilyIndexCount = 0;
    swapchainInfo.pQueueFamilyIndices = nullptr;
    swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = presentMode;
    swapchainInfo.clipped = true;
    swapchainInfo.oldSwapchain = VK_NULL_HANDLE;
    XRC_CHECK_THROW_VKCMD(vkCreateSwapchainKHR(m_vkDevice, &swapchainInfo, nullptr, &swapchain));

    // Fence to throttle host on Acquire
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    XRC_CHECK_THROW_VKCMD(vkCreateFence(m_vkDevice, &fenceInfo, nullptr, &readyFence));

    swapchainCount = 0;
    XRC_CHECK_THROW_VKCMD(vkGetSwapchainImagesKHR(m_vkDevice, swapchain, &swapchainCount, nullptr));
    assert(swapchainCount < maxImages);
    XRC_CHECK_THROW_VKCMD(vkGetSwapchainImagesKHR(m_vkDevice, swapchain, &swapchainCount, image));
    if (swapchainCount > maxImages) {
        Log::Write(Log::Level::Info,
                   "Reducing swapchain length from " + std::to_string(swapchainCount) + " to " + std::to_string(maxImages));
        swapchainCount = maxImages;
    }

    Log::Write(Log::Level::Info, "Swapchain length " + std::to_string(swapchainCount));
}

void Swapchain::Prepare(VkCommandBuffer buf) {
    // Convert swapchain images to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    for (uint32_t i = 0; i < swapchainCount; ++i) {
        VkImageMemoryBarrier imgBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        imgBarrier.srcAccessMask = 0;  // XXX was VK_ACCESS_TRANSFER_READ_BIT wrong?
        imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        imgBarrier.image = image[i];
        imgBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &imgBarrier);
    }
}

void Swapchain::Wait() {
    if (presentFence) {
        // Wait for the fence...
        XRC_CHECK_THROW_VKCMD(vkWaitForFences(m_vkDevice, 1, &presentFence, VK_TRUE, UINT64_MAX));
        // ...then reset the fence for future Acquire calls
        XRC_CHECK_THROW_VKCMD(vkResetFences(m_vkDevice, 1, &presentFence));
        presentFence = VK_NULL_HANDLE;
    }
}

void Swapchain::Acquire(VkSemaphore readySemaphore) {
    // If we're not using a semaphore to rate-limit the GPU, rate limit the host with a fence instead
    if (readySemaphore == VK_NULL_HANDLE) {
        Wait();
        presentFence = readyFence;
    }

    XRC_CHECK_THROW_VKCMD(vkAcquireNextImageKHR(m_vkDevice, swapchain, UINT64_MAX, readySemaphore, presentFence, &renderImageIdx));
}

void Swapchain::Present(VkQueue queue, VkSemaphore drawComplete) {
    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    if (drawComplete) {
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &drawComplete;
    }
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &renderImageIdx;
    auto res = vkQueuePresentKHR(queue, &presentInfo);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        Recreate();
        return;
    }
    XRC_CHECK_THROW_VKRESULT(res, "vkQueuePresentKHR");
}
#endif  // defined(USE_MIRROR_WINDOW)

struct VulkanArraySliceState {
    VulkanArraySliceState() = default;
    VulkanArraySliceState(const VulkanArraySliceState&) = delete;
    std::vector<RenderTarget> m_renderTarget;  // per swapchain index
    RenderPass m_rp{};
    Pipeline m_pipe{};
    Pipeline m_pipeCompute{};

    void init(const VulkanDebugObjectNamer& namer, VkDevice device, uint32_t capacity, const VkExtent2D size, VkFormat colorFormat,
              VkFormat depthFormat, VkSampleCountFlagBits sampleCount, const PipelineLayout& layout,
              const PipelineLayout& computeLayout, const ShaderProgram& sp, const ShaderProgram& spCompute,
              const VkVertexInputBindingDescription& bindDesc, span<const VkVertexInputAttributeDescription> attrDesc) {
        m_renderTarget.resize(capacity);
        m_rp.Create(namer, device, colorFormat, depthFormat, sampleCount);
        VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT};
        m_pipe.Create(device, size, layout, m_rp, sp, bindDesc, attrDesc, dynamicStates);

        // m_pipeCompute not created because hello_xr doesn't need compute shaders
        (void)computeLayout;
        (void)spCompute;
    }

    void Reset() {
        m_pipe.Reset();
        m_pipeCompute.Reset();
        m_rp.Reset();
        m_renderTarget.clear();
    }
};

/// Vulkan data used per swapchain. One per XrSwapchain handle.
class VulkanSwapchainImageData : public SwapchainImageDataBase<XrSwapchainImageVulkanKHR> {
    void init(uint32_t capacity, VkFormat colorFormat, const PipelineLayout& layout, const PipelineLayout& computeLayout,
              const ShaderProgram& sp, const ShaderProgram& spCompute, const VkVertexInputBindingDescription& bindDesc,
              span<const VkVertexInputAttributeDescription> attrDesc) {
        m_depthBuffer.resize(capacity);
        for (auto& slice : m_slices) {
            slice.init(m_namer, m_vkDevice, capacity, m_size, colorFormat, m_depthFormat, m_sampleCount, layout, computeLayout, sp,
                       spCompute, bindDesc, attrDesc);
        }
    }

   public:
    VulkanSwapchainImageData(const VulkanDebugObjectNamer& namer, uint32_t capacity,
                             const XrSwapchainCreateInfo& swapchainCreateInfo, VkDevice device, MemoryAllocator* memAllocator,
                             const PipelineLayout& layout, const PipelineLayout& computeLayout, const ShaderProgram& sp,
                             const ShaderProgram& spCompute, const VkVertexInputBindingDescription& bindDesc,
                             span<const VkVertexInputAttributeDescription> attrDesc)
        : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR, capacity, swapchainCreateInfo),
          m_namer(namer),
          m_vkDevice(device),
          m_memAllocator(memAllocator),
          m_size{swapchainCreateInfo.width, swapchainCreateInfo.height},
          m_sampleCount{(VkSampleCountFlagBits)swapchainCreateInfo.sampleCount},
          m_slices(swapchainCreateInfo.arraySize) {
        init(capacity, (VkFormat)swapchainCreateInfo.format, layout, computeLayout, sp, spCompute, bindDesc, attrDesc);
    }

    VulkanSwapchainImageData(const VulkanDebugObjectNamer& namer, uint32_t capacity,
                             const XrSwapchainCreateInfo& swapchainCreateInfo, XrSwapchain depthSwapchain,
                             const XrSwapchainCreateInfo& depthSwapchainCreateInfo, VkDevice device, MemoryAllocator* memAllocator,
                             const PipelineLayout& layout, const PipelineLayout& computeLayout, const ShaderProgram& sp,
                             const ShaderProgram& spCompute, const VkVertexInputBindingDescription& bindDesc,
                             span<const VkVertexInputAttributeDescription> attrDesc)
        : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR, capacity, swapchainCreateInfo, depthSwapchain,
                                 depthSwapchainCreateInfo),
          m_namer(namer),
          m_vkDevice(device),
          m_memAllocator(memAllocator),
          m_size{swapchainCreateInfo.width, swapchainCreateInfo.height},
          m_sampleCount{(VkSampleCountFlagBits)swapchainCreateInfo.sampleCount},
          m_depthFormat((VkFormat)depthSwapchainCreateInfo.format),
          m_slices(swapchainCreateInfo.arraySize) {
        init(capacity, (VkFormat)swapchainCreateInfo.format, layout, computeLayout, sp, spCompute, bindDesc, attrDesc);
    }

    ~VulkanSwapchainImageData() override {
        // Calling a virtual function from a destructor doesn't work the way you'd expect, so we do this here.
        VulkanSwapchainImageData::Reset();
    }

    void BindRenderTarget(uint32_t index, uint32_t arraySlice, const VkRect2D& renderArea,
                          VkImageAspectFlags secondAttachmentAspect, VkRenderPassBeginInfo* renderPassBeginInfo) {
        RenderTarget& rt = m_slices[arraySlice].m_renderTarget[index];
        RenderPass& rp = m_slices[arraySlice].m_rp;
        if (rt.fb == VK_NULL_HANDLE) {
            rt.Create(m_namer, m_vkDevice, GetTypedImage(index).image, GetDepthImageForColorIndex(index).image,
                      secondAttachmentAspect, arraySlice, m_size, rp);
        }
        renderPassBeginInfo->renderPass = rp.pass;
        renderPassBeginInfo->framebuffer = rt.fb;
        renderPassBeginInfo->renderArea = renderArea;
    }

    void BindPipeline(VkCommandBuffer buf, uint32_t arraySlice, enum ShaderProgramType programType = SHADER_PROGRAM_TYPE_GRAPHICS) {
        switch (programType) {
            case SHADER_PROGRAM_TYPE_GRAPHICS:
                vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_slices[arraySlice].m_pipe.pipe);
                break;
            case SHADER_PROGRAM_TYPE_COMPUTE:
                vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_COMPUTE, m_slices[arraySlice].m_pipeCompute.pipe);
                break;
            default:
                Throw("unknown programType");
        }
    }

    void TransitionLayout(uint32_t imageIndex, CmdBuffer* cmdBuffer, VkImageLayout newLayout) {
        m_depthBuffer[imageIndex].TransitionLayout(cmdBuffer, newLayout);
    }

    void Reset() override {
        for (auto& slice : m_slices) {
            slice.Reset();
        }
        m_depthBuffer.clear();
        SwapchainImageDataBase::Reset();
    }

    int64_t GetDepthFormat() const { return m_depthFormat; }

    const std::vector<VulkanArraySliceState>& GetSlices() const { return m_slices; }

   protected:
    const XrSwapchainImageVulkanKHR& GetFallbackDepthSwapchainImage(uint32_t i) override {
        if (!m_depthBuffer[i].Allocated()) {
            m_depthBuffer[i].Allocate(m_namer, m_vkDevice, m_memAllocator, m_depthFormat, this->Width(), this->Height(),
                                      this->ArraySize(), this->SampleCount());
        }

        return m_depthBuffer[i].GetTexture();
    }

   private:
    VulkanDebugObjectNamer m_namer;
    VkDevice m_vkDevice{VK_NULL_HANDLE};
    MemoryAllocator* m_memAllocator{nullptr};
    VkExtent2D m_size{};
    VkSampleCountFlagBits m_sampleCount;
    std::vector<DepthBuffer> m_depthBuffer;  // per swapchain index
    VkFormat m_depthFormat{VK_FORMAT_D32_SFLOAT};

    std::vector<VulkanArraySliceState> m_slices;
};

struct VulkanGraphicsPlugin : public IGraphicsPlugin {
    VulkanGraphicsPlugin(const std::shared_ptr<Options>& options, std::shared_ptr<IPlatformPlugin> /*unused*/)
        : m_clearColor(options->GetBackgroundClearColor()) {
        m_graphicsBinding.type = GetGraphicsBindingType();
    };

    std::vector<std::string> GetInstanceExtensions() const override { return {XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME}; }

    // Note: The output must not outlive the input - this modifies the input and returns a collection of views into that modified
    // input!
    std::vector<const char*> ParseExtensionString(char* names) {
        std::vector<const char*> list;
        while (*names != 0) {
            list.push_back(names);
            while (*(++names) != 0) {
                if (*names == ' ') {
                    *names++ = '\0';
                    break;
                }
            }
        }
        return list;
    }

    const char* GetValidationLayerName() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        std::vector<const char*> validationLayerNames;
        validationLayerNames.push_back("VK_LAYER_KHRONOS_validation");
        validationLayerNames.push_back("VK_LAYER_LUNARG_standard_validation");

        // Enable only one validation layer from the list above. Prefer KHRONOS.
        for (auto& validationLayerName : validationLayerNames) {
            for (const auto& layerProperties : availableLayers) {
                if (0 == strcmp(validationLayerName, layerProperties.layerName)) {
                    return validationLayerName;
                }
            }
        }

        return nullptr;
    }

    void InitializeDevice(XrInstance instance, XrSystemId systemId) override {
        // Create the Vulkan device for the adapter associated with the system.
        // Extension function must be loaded by name
        XrGraphicsRequirementsVulkan2KHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
        CHECK_XRCMD(GetVulkanGraphicsRequirements2KHR(instance, systemId, &graphicsRequirements));

        VkResult err;

        std::vector<const char*> layers;
#if !defined(NDEBUG)
        const char* const validationLayerName = GetValidationLayerName();
        if (validationLayerName) {
            layers.push_back(validationLayerName);
        } else {
            Log::Write(Log::Level::Warning, "No validation layers found in the system, skipping");
        }
#endif

        std::vector<const char*> extensions;
        {
            uint32_t extensionCount = 0;
            XRC_CHECK_THROW_VKCMD(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));

            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            XRC_CHECK_THROW_VKCMD(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data()));
            const auto b = availableExtensions.begin();
            const auto e = availableExtensions.end();

            auto isExtSupported = [&](const char* extName) -> bool {
                auto it = std::find_if(b, e, [&](const VkExtensionProperties& properties) {
                    return (0 == strcmp(extName, properties.extensionName));
                });
                return (it != e);
            };

            // Debug utils is optional and not always available
            if (isExtSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }
            // TODO add back VK_EXT_debug_report code for compatibility with older systems? (Android)
        }
#if defined(USE_MIRROR_WINDOW)
        extensions.push_back("VK_KHR_surface");
#if defined(VK_USE_PLATFORM_WIN32_KHR)
        extensions.push_back("VK_KHR_win32_surface");
#else
#error CreateSurface not supported on this OS
#endif  // defined(VK_USE_PLATFORM_WIN32_KHR)
#endif  // defined(USE_MIRROR_WINDOW)

        VkDebugUtilsMessengerCreateInfoEXT debugInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
#if !defined(NDEBUG)
        debugInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
#endif
        debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugInfo.pfnUserCallback = debugMessageThunk;
        debugInfo.pUserData = this;

        VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        appInfo.pApplicationName = "hello_xr";
        appInfo.applicationVersion = 1;
        appInfo.pEngineName = "hello_xr";
        appInfo.engineVersion = 1;
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo instInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        instInfo.pNext = &debugInfo;
        instInfo.pApplicationInfo = &appInfo;
        instInfo.enabledLayerCount = (uint32_t)layers.size();
        instInfo.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();
        instInfo.enabledExtensionCount = (uint32_t)extensions.size();
        instInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();

        XrVulkanInstanceCreateInfoKHR createInfo{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
        createInfo.systemId = systemId;
        createInfo.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
        createInfo.vulkanCreateInfo = &instInfo;
        createInfo.vulkanAllocator = nullptr;
        CHECK_XRCMD(CreateVulkanInstanceKHR(instance, &createInfo, &m_vkInstance, &err));
        XRC_CHECK_THROW_VKCMD(err);

        vkCreateDebugUtilsMessengerEXT =
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_vkInstance, "vkCreateDebugUtilsMessengerEXT");

        if (vkCreateDebugUtilsMessengerEXT != nullptr) {
            XRC_CHECK_THROW_VKCMD(vkCreateDebugUtilsMessengerEXT(m_vkInstance, &debugInfo, nullptr, &m_vkDebugUtilsMessenger));
        }

        XrVulkanGraphicsDeviceGetInfoKHR deviceGetInfo{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
        deviceGetInfo.systemId = systemId;
        deviceGetInfo.vulkanInstance = m_vkInstance;
        CHECK_XRCMD(GetVulkanGraphicsDevice2KHR(instance, &deviceGetInfo, &m_vkPhysicalDevice));

        VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        float queuePriorities = 0;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriorities;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysicalDevice, &queueFamilyCount, &queueFamilyProps[0]);

        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            // Only need graphics (not presentation) for draw queue
            if ((queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u) {
                m_queueFamilyIndex = queueInfo.queueFamilyIndex = i;
                break;
            }
        }

        std::vector<const char*> deviceExtensions;

        VkPhysicalDeviceFeatures features{};
        // features.samplerAnisotropy = VK_TRUE;

#if defined(USE_MIRROR_WINDOW)
        deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#endif

        VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledLayerCount = 0;
        deviceInfo.ppEnabledLayerNames = nullptr;
        deviceInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
        deviceInfo.ppEnabledExtensionNames = deviceExtensions.empty() ? nullptr : deviceExtensions.data();
        deviceInfo.pEnabledFeatures = &features;

        XrVulkanDeviceCreateInfoKHR deviceCreateInfo{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
        deviceCreateInfo.systemId = systemId;
        deviceCreateInfo.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
        deviceCreateInfo.vulkanCreateInfo = &deviceInfo;
        deviceCreateInfo.vulkanPhysicalDevice = m_vkPhysicalDevice;
        deviceCreateInfo.vulkanAllocator = nullptr;
        CHECK_XRCMD(CreateVulkanDeviceKHR(instance, &deviceCreateInfo, &m_vkDevice, &err));
        XRC_CHECK_THROW_VKCMD(err);

        m_namer.Init(m_vkInstance, m_vkDevice);

        vkGetDeviceQueue(m_vkDevice, queueInfo.queueFamilyIndex, 0, &m_vkQueue);

        m_memAllocator.Init(m_vkPhysicalDevice, m_vkDevice);

        InitializeResources();

        m_graphicsBinding.instance = m_vkInstance;
        m_graphicsBinding.physicalDevice = m_vkPhysicalDevice;
        m_graphicsBinding.device = m_vkDevice;
        m_graphicsBinding.queueFamilyIndex = queueInfo.queueFamilyIndex;
        m_graphicsBinding.queueIndex = 0;
    }

#ifdef USE_ONLINE_VULKAN_SHADERC
    // Compile a shader to a SPIR-V binary.
    std::vector<uint32_t> CompileGlslShader(const std::string& name, shaderc_shader_kind kind, const std::string& source) {
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        options.SetOptimizationLevel(shaderc_optimization_level_size);

        shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, kind, name.c_str(), options);

        if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
            Log::Write(Log::Level::Error, Fmt("Shader %s compilation failed: %s", name.c_str(), module.GetErrorMessage().c_str()));
            return std::vector<uint32_t>();
        }

        return {module.cbegin(), module.cend()};
    }
#endif

    void InitializeResources() {
#ifdef USE_ONLINE_VULKAN_SHADERC
        auto vertexSPIRV = CompileGlslShader("vertex", shaderc_glsl_default_vertex_shader, VertexShaderGlsl);
        auto fragmentSPIRV = CompileGlslShader("fragment", shaderc_glsl_default_fragment_shader, FragmentShaderGlsl);
#else
        std::vector<uint32_t> vertexSPIRV = SPV_PREFIX
#include "vert.spv"
            SPV_SUFFIX;
        std::vector<uint32_t> fragmentSPIRV = SPV_PREFIX
#include "frag.spv"
            SPV_SUFFIX;
#endif
        if (vertexSPIRV.empty()) THROW("Failed to compile vertex shader");
        if (fragmentSPIRV.empty()) THROW("Failed to compile fragment shader");

        m_shaderProgram.Init(m_vkDevice);
        m_shaderProgram.LoadVertexShader(vertexSPIRV);
        m_shaderProgram.LoadFragmentShader(fragmentSPIRV);

        // Semaphore to block on draw complete
        VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        XRC_CHECK_THROW_VKCMD(vkCreateSemaphore(m_vkDevice, &semInfo, nullptr, &m_vkDrawDone));
        XRC_CHECK_THROW_VKCMD(m_namer.SetName(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)m_vkDrawDone, "hello_xr draw done semaphore"));

        if (!m_cmdBuffer.Init(m_namer, m_vkDevice, m_queueFamilyIndex)) THROW("Failed to create command buffer");

        m_pipelineLayout.Create(m_vkDevice);

        // hello_xr: doesn't need compute shader support
#if 0
        XRC_CHECK_THROW_VKCMD(
            m_namer.SetName(VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)m_pipelineLayout.layout, "hello_xr graphics pipeline layout"));

        m_computePipelineLayout.Create(m_vkDevice, SHADER_PROGRAM_TYPE_COMPUTE);
        XRC_CHECK_THROW_VKCMD(m_namer.SetName(VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)m_computePipelineLayout.layout,
                                    "hello_xr compute pipeline layout"));
        XRC_CHECK_THROW_VKCMD(m_namer.SetName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)m_computePipelineLayout.descriptorSetLayout,
                                    "hello_xr compute descriptor set layout"));

        m_computeDescriptorPool.adopt(CreateDescriptorPool(m_vkDevice, 1, 1), m_vkDevice);

        VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorPool = m_computeDescriptorPool.get();
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_computePipelineLayout.descriptorSetLayout;
        XRC_CHECK_THROW_VKCMD(vkAllocateDescriptorSets(m_vkDevice, &allocInfo, &m_ComputeDescriptorSet));
#endif

        static_assert(sizeof(Geometry::Vertex) == 24, "Unexpected Vertex size");
        m_drawBuffer.Init(m_vkDevice, &m_memAllocator,
                          {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Geometry::Vertex, Position)},
                           {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Geometry::Vertex, Color)}});
        uint32_t numCubeIdicies = sizeof(Geometry::c_cubeIndices) / sizeof(Geometry::c_cubeIndices[0]);
        uint32_t numCubeVerticies = sizeof(Geometry::c_cubeVertices) / sizeof(Geometry::c_cubeVertices[0]);
        m_drawBuffer.Create(numCubeIdicies, numCubeVerticies);

        m_drawBuffer.UpdateIndices(span<const uint16_t>(Geometry::c_cubeIndices, numCubeIdicies), 0);
        m_drawBuffer.UpdateVertices(span<const Geometry::Vertex>(Geometry::c_cubeVertices, numCubeVerticies), 0);
#if defined(USE_MIRROR_WINDOW)
        m_swapchain.Create(m_vkInstance, m_vkPhysicalDevice, m_vkDevice, m_graphicsBinding.queueFamilyIndex);

        m_cmdBuffer.Reset();
        if (!m_cmdBuffer.Init(m_namer, m_vkDevice, m_queueFamilyIndex)) THROW("Failed to create command buffer");

        m_cmdBuffer.Begin();
        m_swapchain.Prepare(m_cmdBuffer.buf);
        m_cmdBuffer.End();
        m_cmdBuffer.Exec(m_vkQueue);
        m_cmdBuffer.Wait();
#endif
    }

    // Select the preferred swapchain format from the list of available formats.
    int64_t SelectColorSwapchainFormat(bool throwIfNotFound, span<const int64_t> imageFormatArray) const override {
        // List of supported color swapchain formats.
        return SelectSwapchainFormat(  //
            throwIfNotFound, imageFormatArray,
            {
                VK_FORMAT_R8G8B8A8_SRGB,
                VK_FORMAT_B8G8R8A8_SRGB,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_FORMAT_B8G8R8A8_UNORM,
            });
    }

    // Select the preferred swapchain format from the list of available formats.
    int64_t SelectDepthSwapchainFormat(bool throwIfNotFound, span<const int64_t> imageFormatArray) const override {
        // List of supported depth swapchain formats.
        return SelectSwapchainFormat(  //
            throwIfNotFound, imageFormatArray,
            {
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_FORMAT_D16_UNORM,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
            });
    }

    const XrBaseInStructure* GetGraphicsBinding() const override {
        return reinterpret_cast<const XrBaseInStructure*>(&m_graphicsBinding);
    }

    ISwapchainImageData* AllocateSwapchainImageData(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo) override {
        auto typedResult = std::make_unique<VulkanSwapchainImageData>(
            m_namer, uint32_t(size), swapchainCreateInfo, m_vkDevice, &m_memAllocator, m_pipelineLayout, m_computePipelineLayout,
            m_shaderProgram, m_computeShaderProgram, m_drawBuffer.bindDesc, m_drawBuffer.attrDesc);

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    inline ISwapchainImageData* AllocateSwapchainImageDataWithDepthSwapchain(
        size_t size, const XrSwapchainCreateInfo& colorSwapchainCreateInfo, XrSwapchain depthSwapchain,
        const XrSwapchainCreateInfo& depthSwapchainCreateInfo) override {
        auto typedResult = std::make_unique<VulkanSwapchainImageData>(
            m_namer, uint32_t(size), colorSwapchainCreateInfo, depthSwapchain, depthSwapchainCreateInfo, m_vkDevice,
            &m_memAllocator, m_pipelineLayout, m_computePipelineLayout, m_shaderProgram, m_computeShaderProgram,
            m_drawBuffer.bindDesc, m_drawBuffer.attrDesc);

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    void SetViewportAndScissor(const VkRect2D& rect) {
        VkViewport viewport{
            float(rect.offset.x), float(rect.offset.y), float(rect.extent.width), float(rect.extent.height), 0.0f, 1.0f};
        vkCmdSetViewport(m_cmdBuffer.buf, 0, 1, &viewport);
        vkCmdSetScissor(m_cmdBuffer.buf, 0, 1, &rect);
    }

    void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* swapchainImage,
                    int64_t /*swapchainFormat*/, const std::vector<Cube>& cubes) override {
        CHECK(layerView.subImage.imageArrayIndex == 0);  // Texture arrays not supported.

        VulkanSwapchainImageData* swapchainData;
        uint32_t imageIndex;

        std::tie(swapchainData, imageIndex) = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(swapchainImage);

        m_cmdBuffer.Clear();
        m_cmdBuffer.Begin();

        const XrRect2Di& r = layerView.subImage.imageRect;
        VkRect2D renderArea = {{r.offset.x, r.offset.y}, {uint32_t(r.extent.width), uint32_t(r.extent.height)}};
        SetViewportAndScissor(renderArea);

        // may be depth, stencil, or both
        // XXX support VK_IMAGE_ASPECT_STENCIL_BIT
        VkImageAspectFlags secondAttachmentAspect = VK_IMAGE_ASPECT_DEPTH_BIT;

        VkRenderPassBeginInfo renderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};

        // aka slice
        auto imageArrayIndex = layerView.subImage.imageArrayIndex;

        swapchainData->BindRenderTarget(imageIndex, imageArrayIndex, renderArea, secondAttachmentAspect, &renderPassBeginInfo);

        if (!swapchainData->DepthSwapchainEnabled()) {
            // Ensure self-made fallback depth is in the right layout
            swapchainData->TransitionLayout(imageIndex, &m_cmdBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }

        vkCmdBeginRenderPass(m_cmdBuffer.buf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        swapchainData->BindPipeline(m_cmdBuffer.buf, imageArrayIndex);

        // Bind and clear eye render target
        static std::array<VkClearValue, 2> clearValues;
        clearValues[0].color.float32[0] = m_clearColor[0];
        clearValues[0].color.float32[1] = m_clearColor[1];
        clearValues[0].color.float32[2] = m_clearColor[2];
        clearValues[0].color.float32[3] = m_clearColor[3];
        clearValues[1].depthStencil.depth = 1.0f;
        clearValues[1].depthStencil.stencil = 0;
        renderPassBeginInfo.clearValueCount = (uint32_t)clearValues.size();
        renderPassBeginInfo.pClearValues = clearValues.data();

        std::array<VkClearAttachment, 2> clearAttachments{{
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, clearValues[0]},
            {secondAttachmentAspect, 0, clearValues[1]},
        }};

        // imageArrayIndex already included in the VkImageView
        VkClearRect clearRect{renderArea, 0, 1};
        vkCmdClearAttachments(m_cmdBuffer.buf, 2, &clearAttachments[0], 1, &clearRect);

        // Bind index and vertex buffers
        vkCmdBindIndexBuffer(m_cmdBuffer.buf, m_drawBuffer.idx.buf, 0, VK_INDEX_TYPE_UINT16);

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(m_cmdBuffer.buf, 0, 1, &m_drawBuffer.vtx.buf, &offset);

        // Compute the view-projection transform.
        // Note all matrixes (including OpenXR's) are column-major, right-handed.
        const auto& pose = layerView.pose;
        XrMatrix4x4f proj;
        XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_VULKAN, layerView.fov, 0.05f, 100.0f);
        XrMatrix4x4f toView;
        XrMatrix4x4f_CreateFromRigidTransform(&toView, &pose);
        XrMatrix4x4f view;
        XrMatrix4x4f_InvertRigidBody(&view, &toView);
        XrMatrix4x4f vp;
        XrMatrix4x4f_Multiply(&vp, &proj, &view);

        // Render each cube
        for (const Cube& cube : cubes) {
            // Compute the model-view-projection transform and push it.
            XrMatrix4x4f model;
            XrMatrix4x4f_CreateTranslationRotationScale(&model, &cube.Pose.position, &cube.Pose.orientation, &cube.Scale);
            XrMatrix4x4f mvp;
            XrMatrix4x4f_Multiply(&mvp, &vp, &model);
            vkCmdPushConstants(m_cmdBuffer.buf, m_pipelineLayout.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mvp.m), &mvp.m[0]);

            // Draw the cube.
            vkCmdDrawIndexed(m_cmdBuffer.buf, m_drawBuffer.count.idx, 1, 0, 0, 0);
        }

        vkCmdEndRenderPass(m_cmdBuffer.buf);

        m_cmdBuffer.End();
        m_cmdBuffer.Exec(m_vkQueue);
        // XXX Should double-buffer the command buffers, for now just flush
        m_cmdBuffer.Wait();

#if defined(USE_MIRROR_WINDOW)
        // Cycle the window's swapchain on the last view rendered
        // XXX bit of a hack
        if (layerView.subImage.imageRect.offset.x != 0) {
            m_swapchain.Acquire();
            m_swapchain.Wait();
            m_swapchain.Present(m_vkQueue);
        }
#endif
    }

    uint32_t GetSupportedSwapchainSampleCount(const XrViewConfigurationView&) override { return VK_SAMPLE_COUNT_1_BIT; }

    void UpdateOptions(const std::shared_ptr<Options>& options) override { m_clearColor = options->GetBackgroundClearColor(); }

   protected:
    XrGraphicsBindingVulkan2KHR m_graphicsBinding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
    SwapchainImageDataMap<VulkanSwapchainImageData> m_swapchainImageDataMap;

    VkInstance m_vkInstance{VK_NULL_HANDLE};
    VkPhysicalDevice m_vkPhysicalDevice{VK_NULL_HANDLE};
    VkDevice m_vkDevice{VK_NULL_HANDLE};
    VulkanDebugObjectNamer m_namer{};
    uint32_t m_queueFamilyIndex = 0;
    VkQueue m_vkQueue{VK_NULL_HANDLE};
    VkSemaphore m_vkDrawDone{VK_NULL_HANDLE};

    MemoryAllocator m_memAllocator{};
    ShaderProgram m_shaderProgram{SHADER_PROGRAM_TYPE_GRAPHICS};
    ShaderProgram m_computeShaderProgram{SHADER_PROGRAM_TYPE_COMPUTE};
    CmdBuffer m_cmdBuffer{};
    PipelineLayout m_pipelineLayout{};
    VertexBuffer<Geometry::Vertex> m_drawBuffer{};
    std::array<float, 4> m_clearColor;

    PipelineLayout m_computePipelineLayout{};
    VkDescriptorSet m_ComputeDescriptorSet;

#if defined(USE_MIRROR_WINDOW)
    Swapchain m_swapchain{};
#endif

    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT{nullptr};
    VkDebugUtilsMessengerEXT m_vkDebugUtilsMessenger{VK_NULL_HANDLE};

    static std::string vkObjectTypeToString(VkObjectType objectType) {
        std::string objName;

#define LIST_OBJECT_TYPES(_)          \
    _(UNKNOWN)                        \
    _(INSTANCE)                       \
    _(PHYSICAL_DEVICE)                \
    _(DEVICE)                         \
    _(QUEUE)                          \
    _(SEMAPHORE)                      \
    _(COMMAND_BUFFER)                 \
    _(FENCE)                          \
    _(DEVICE_MEMORY)                  \
    _(BUFFER)                         \
    _(IMAGE)                          \
    _(EVENT)                          \
    _(QUERY_POOL)                     \
    _(BUFFER_VIEW)                    \
    _(IMAGE_VIEW)                     \
    _(SHADER_MODULE)                  \
    _(PIPELINE_CACHE)                 \
    _(PIPELINE_LAYOUT)                \
    _(RENDER_PASS)                    \
    _(PIPELINE)                       \
    _(DESCRIPTOR_SET_LAYOUT)          \
    _(SAMPLER)                        \
    _(DESCRIPTOR_POOL)                \
    _(DESCRIPTOR_SET)                 \
    _(FRAMEBUFFER)                    \
    _(COMMAND_POOL)                   \
    _(SURFACE_KHR)                    \
    _(SWAPCHAIN_KHR)                  \
    _(DISPLAY_KHR)                    \
    _(DISPLAY_MODE_KHR)               \
    _(DESCRIPTOR_UPDATE_TEMPLATE_KHR) \
    _(DEBUG_UTILS_MESSENGER_EXT)

        switch (objectType) {
            default:
#define MK_OBJECT_TYPE_CASE(name) \
    case VK_OBJECT_TYPE_##name:   \
        objName = #name;          \
        break;
                LIST_OBJECT_TYPES(MK_OBJECT_TYPE_CASE)
        }

        return objName;
    }
    VkBool32 debugMessage(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                          const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData) {
        std::string flagNames;
        std::string objName;
        Log::Level level = Log::Level::Error;

        if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) != 0u) {
            flagNames += "DEBUG:";
            level = Log::Level::Verbose;
        }
        if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0u) {
            flagNames += "INFO:";
            level = Log::Level::Info;
        }
        if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0u) {
            flagNames += "WARN:";
            level = Log::Level::Warning;
        }
        if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0u) {
            flagNames += "ERROR:";
            level = Log::Level::Error;
        }
        if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0u) {
            flagNames += "PERF:";
            level = Log::Level::Warning;
        }

        uint64_t object = 0;
        // skip loader messages about device extensions
        if (pCallbackData->objectCount > 0) {
            auto objectType = pCallbackData->pObjects[0].objectType;
            if ((objectType == VK_OBJECT_TYPE_INSTANCE) && (strncmp(pCallbackData->pMessage, "Device Extension:", 17) == 0)) {
                return VK_FALSE;
            }
            objName = vkObjectTypeToString(objectType);
            object = pCallbackData->pObjects[0].objectHandle;
            if (pCallbackData->pObjects[0].pObjectName != nullptr) {
                objName += " " + std::string(pCallbackData->pObjects[0].pObjectName);
            }
        }

        Log::Write(level, Fmt("%s (%s 0x%llx) %s", flagNames.c_str(), objName.c_str(), object, pCallbackData->pMessage));

        return VK_FALSE;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageThunk(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                            VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                                            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                            void* pUserData) {
        return static_cast<VulkanGraphicsPlugin*>(pUserData)->debugMessage(messageSeverity, messageTypes, pCallbackData);
    }

    virtual XrStructureType GetGraphicsBindingType() const { return XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR; }
    virtual XrStructureType GetSwapchainImageType() const { return XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR; }

    virtual XrResult CreateVulkanInstanceKHR(XrInstance instance, const XrVulkanInstanceCreateInfoKHR* createInfo,
                                             VkInstance* vulkanInstance, VkResult* vulkanResult) {
        PFN_xrCreateVulkanInstanceKHR pfnCreateVulkanInstanceKHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrCreateVulkanInstanceKHR",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&pfnCreateVulkanInstanceKHR)));

        return pfnCreateVulkanInstanceKHR(instance, createInfo, vulkanInstance, vulkanResult);
    }

    virtual XrResult CreateVulkanDeviceKHR(XrInstance instance, const XrVulkanDeviceCreateInfoKHR* createInfo,
                                           VkDevice* vulkanDevice, VkResult* vulkanResult) {
        PFN_xrCreateVulkanDeviceKHR pfnCreateVulkanDeviceKHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrCreateVulkanDeviceKHR",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&pfnCreateVulkanDeviceKHR)));

        return pfnCreateVulkanDeviceKHR(instance, createInfo, vulkanDevice, vulkanResult);
    }

    virtual XrResult GetVulkanGraphicsDevice2KHR(XrInstance instance, const XrVulkanGraphicsDeviceGetInfoKHR* getInfo,
                                                 VkPhysicalDevice* vulkanPhysicalDevice) {
        PFN_xrGetVulkanGraphicsDevice2KHR pfnGetVulkanGraphicsDevice2KHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsDevice2KHR",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanGraphicsDevice2KHR)));

        return pfnGetVulkanGraphicsDevice2KHR(instance, getInfo, vulkanPhysicalDevice);
    }

    virtual XrResult GetVulkanGraphicsRequirements2KHR(XrInstance instance, XrSystemId systemId,
                                                       XrGraphicsRequirementsVulkan2KHR* graphicsRequirements) {
        PFN_xrGetVulkanGraphicsRequirements2KHR pfnGetVulkanGraphicsRequirements2KHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirements2KHR",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanGraphicsRequirements2KHR)));

        return pfnGetVulkanGraphicsRequirements2KHR(instance, systemId, graphicsRequirements);
    }
};

// A compatibility class that implements the KHR_vulkan_enable2 functionality on top of KHR_vulkan_enable
struct VulkanGraphicsPluginLegacy : public VulkanGraphicsPlugin {
    VulkanGraphicsPluginLegacy(const std::shared_ptr<Options>& options, std::shared_ptr<IPlatformPlugin> platformPlugin)
        : VulkanGraphicsPlugin(options, platformPlugin) {
        m_graphicsBinding.type = GetGraphicsBindingType();
    };

    std::vector<std::string> GetInstanceExtensions() const override { return {XR_KHR_VULKAN_ENABLE_EXTENSION_NAME}; }
    virtual XrStructureType GetGraphicsBindingType() const override { return XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR; }
    virtual XrStructureType GetSwapchainImageType() const override { return XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR; }

    static void LogVulkanExtensions(const std::string title, const std::vector<const char*>& extensions, unsigned int start = 0) {
        const std::string indentStr(1, ' ');

        Log::Write(Log::Level::Verbose, Fmt("%s: (%d)", title.c_str(), extensions.size() - start));
        for (auto ext : extensions) {
            if (start) {
                start--;
                continue;
            }
            Log::Write(Log::Level::Verbose, Fmt("%s  Name=%s", indentStr.c_str(), ext));
        }
    }

    virtual XrResult CreateVulkanInstanceKHR(XrInstance instance, const XrVulkanInstanceCreateInfoKHR* createInfo,
                                             VkInstance* vulkanInstance, VkResult* vulkanResult) override {
        PFN_xrGetVulkanInstanceExtensionsKHR pfnGetVulkanInstanceExtensionsKHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanInstanceExtensionsKHR",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanInstanceExtensionsKHR)));

        uint32_t extensionNamesSize = 0;
        CHECK_XRCMD(pfnGetVulkanInstanceExtensionsKHR(instance, createInfo->systemId, 0, &extensionNamesSize, nullptr));

        std::vector<char> extensionNames(extensionNamesSize);
        CHECK_XRCMD(pfnGetVulkanInstanceExtensionsKHR(instance, createInfo->systemId, extensionNamesSize, &extensionNamesSize,
                                                      &extensionNames[0]));
        {
            // Note: This cannot outlive the extensionNames above, since it's just a collection of views into that string!
            std::vector<const char*> extensions = ParseExtensionString(&extensionNames[0]);
            LogVulkanExtensions("Vulkan Instance Extensions, requested by runtime", extensions);

            // Merge the runtime's request with the applications requests
            for (uint32_t i = 0; i < createInfo->vulkanCreateInfo->enabledExtensionCount; ++i) {
                extensions.push_back(createInfo->vulkanCreateInfo->ppEnabledExtensionNames[i]);
            }
            LogVulkanExtensions("Vulkan Instance Extensions, requested by application", extensions,
                                (uint32_t)extensions.size() - createInfo->vulkanCreateInfo->enabledExtensionCount);

            VkInstanceCreateInfo instInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
            memcpy(&instInfo, createInfo->vulkanCreateInfo, sizeof(instInfo));
            instInfo.enabledExtensionCount = (uint32_t)extensions.size();
            instInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();

            auto pfnCreateInstance = (PFN_vkCreateInstance)createInfo->pfnGetInstanceProcAddr(nullptr, "vkCreateInstance");
            *vulkanResult = pfnCreateInstance(&instInfo, createInfo->vulkanAllocator, vulkanInstance);
        }

        return XR_SUCCESS;
    }

    virtual XrResult CreateVulkanDeviceKHR(XrInstance instance, const XrVulkanDeviceCreateInfoKHR* createInfo,
                                           VkDevice* vulkanDevice, VkResult* vulkanResult) override {
        PFN_xrGetVulkanDeviceExtensionsKHR pfnGetVulkanDeviceExtensionsKHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanDeviceExtensionsKHR",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanDeviceExtensionsKHR)));

        uint32_t deviceExtensionNamesSize = 0;
        CHECK_XRCMD(pfnGetVulkanDeviceExtensionsKHR(instance, createInfo->systemId, 0, &deviceExtensionNamesSize, nullptr));
        std::vector<char> deviceExtensionNames(deviceExtensionNamesSize);
        if (deviceExtensionNamesSize > 0) {
            CHECK_XRCMD(pfnGetVulkanDeviceExtensionsKHR(instance, createInfo->systemId, deviceExtensionNamesSize,
                                                        &deviceExtensionNamesSize, &deviceExtensionNames[0]));
        }
        {
            // Note: This cannot outlive the extensionNames above, since it's just a collection of views into that string!
            std::vector<const char*> extensions;

            if (deviceExtensionNamesSize > 0) {
                extensions = ParseExtensionString(&deviceExtensionNames[0]);
            }
            LogVulkanExtensions("Vulkan Device Extensions, requested by runtime", extensions);

            // Merge the runtime's request with the applications requests
            for (uint32_t i = 0; i < createInfo->vulkanCreateInfo->enabledExtensionCount; ++i) {
                extensions.push_back(createInfo->vulkanCreateInfo->ppEnabledExtensionNames[i]);
            }
            LogVulkanExtensions("Vulkan Device Extensions, requested by application", extensions,
                                (uint32_t)extensions.size() - createInfo->vulkanCreateInfo->enabledExtensionCount);

            VkPhysicalDeviceFeatures features{};
            memcpy(&features, createInfo->vulkanCreateInfo->pEnabledFeatures, sizeof(features));

#if !defined(XR_USE_PLATFORM_ANDROID)
            VkPhysicalDeviceFeatures availableFeatures{};
            vkGetPhysicalDeviceFeatures(m_vkPhysicalDevice, &availableFeatures);
            if (availableFeatures.shaderStorageImageMultisample == VK_TRUE) {
                // Setting this quiets down a validation error triggered by the Oculus runtime
                features.shaderStorageImageMultisample = VK_TRUE;
            }
#endif

            VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            memcpy(&deviceInfo, createInfo->vulkanCreateInfo, sizeof(deviceInfo));
            deviceInfo.pEnabledFeatures = &features;
            deviceInfo.enabledExtensionCount = (uint32_t)extensions.size();
            deviceInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();

            auto pfnCreateDevice = (PFN_vkCreateDevice)createInfo->pfnGetInstanceProcAddr(m_vkInstance, "vkCreateDevice");
            *vulkanResult = pfnCreateDevice(m_vkPhysicalDevice, &deviceInfo, createInfo->vulkanAllocator, vulkanDevice);
        }

        return XR_SUCCESS;
    }

    virtual XrResult GetVulkanGraphicsDevice2KHR(XrInstance instance, const XrVulkanGraphicsDeviceGetInfoKHR* getInfo,
                                                 VkPhysicalDevice* vulkanPhysicalDevice) override {
        PFN_xrGetVulkanGraphicsDeviceKHR pfnGetVulkanGraphicsDeviceKHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsDeviceKHR",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanGraphicsDeviceKHR)));

        if (getInfo->next != nullptr) {
            return XR_ERROR_FEATURE_UNSUPPORTED;
        }

        CHECK_XRCMD(pfnGetVulkanGraphicsDeviceKHR(instance, getInfo->systemId, getInfo->vulkanInstance, vulkanPhysicalDevice));

        return XR_SUCCESS;
    }

    virtual XrResult GetVulkanGraphicsRequirements2KHR(XrInstance instance, XrSystemId systemId,
                                                       XrGraphicsRequirementsVulkan2KHR* graphicsRequirements) override {
        PFN_xrGetVulkanGraphicsRequirementsKHR pfnGetVulkanGraphicsRequirementsKHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirementsKHR",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanGraphicsRequirementsKHR)));

        XrGraphicsRequirementsVulkanKHR legacyRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
        CHECK_XRCMD(pfnGetVulkanGraphicsRequirementsKHR(instance, systemId, &legacyRequirements));

        graphicsRequirements->maxApiVersionSupported = legacyRequirements.maxApiVersionSupported;
        graphicsRequirements->minApiVersionSupported = legacyRequirements.minApiVersionSupported;

        return XR_SUCCESS;
    }
};

}  // namespace

std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_Vulkan(const std::shared_ptr<Options>& options,
                                                             std::shared_ptr<IPlatformPlugin> platformPlugin) {
    return std::make_shared<VulkanGraphicsPlugin>(options, std::move(platformPlugin));
}

std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_VulkanLegacy(const std::shared_ptr<Options>& options,
                                                                   std::shared_ptr<IPlatformPlugin> platformPlugin) {
    return std::make_shared<VulkanGraphicsPluginLegacy>(options, std::move(platformPlugin));
}

#endif  // XR_USE_GRAPHICS_API_VULKAN
