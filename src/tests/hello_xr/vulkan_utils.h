// Copyright (c) 2019-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>

#ifdef XR_USE_GRAPHICS_API_VULKAN

#include "common/xr_linear.h"
#include "common/vulkan_debug_object_namer.hpp"

#include "common/xr_dependencies.h"  // IWYU pragma: keep
#include <openxr/openxr_platform.h>
#include <nonstd/span.hpp>

#include <string>
#include <vector>
#include <cstdint>
#include "check.h"

// #define USE_ONLINE_VULKAN_SHADERC
#ifdef USE_ONLINE_VULKAN_SHADERC
#include <shaderc/shaderc.hpp>
#endif

#if defined(VK_USE_PLATFORM_WIN32_KHR)
// Define USE_MIRROR_WINDOW to open an otherwise-unused window for e.g. RenderDoc
// #define USE_MIRROR_WINDOW
#endif

// Define USE_CHECKPOINTS to use the nvidia checkpoint extension
// #define USE_CHECKPOINTS

// glslangValidator doesn't wrap its output in brackets if you don't have it define the whole array.
#if defined(USE_GLSLANGVALIDATOR)
#define SPV_PREFIX {
#define SPV_SUFFIX }
#else
#define SPV_PREFIX
#define SPV_SUFFIX
#endif

using nonstd::span;

inline std::string vkResultString(VkResult res) {
    switch (res) {
        case VK_SUCCESS:
            return "SUCCESS";
        case VK_NOT_READY:
            return "NOT_READY";
        case VK_TIMEOUT:
            return "TIMEOUT";
        case VK_EVENT_SET:
            return "EVENT_SET";
        case VK_EVENT_RESET:
            return "EVENT_RESET";
        case VK_INCOMPLETE:
            return "INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST:
            return "ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR:
            return "SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            return "ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT:
            return "ERROR_VALIDATION_FAILED_EXT";
        case VK_ERROR_INVALID_SHADER_NV:
            return "ERROR_INVALID_SHADER_NV";
        default:
            return std::to_string(res);
    }
}

#define LIST_PIPE_STAGES(_)           \
    _(TOP_OF_PIPE)                    \
    _(DRAW_INDIRECT)                  \
    _(VERTEX_INPUT)                   \
    _(VERTEX_SHADER)                  \
    _(TESSELLATION_CONTROL_SHADER)    \
    _(TESSELLATION_EVALUATION_SHADER) \
    _(GEOMETRY_SHADER)                \
    _(FRAGMENT_SHADER)                \
    _(EARLY_FRAGMENT_TESTS)           \
    _(LATE_FRAGMENT_TESTS)            \
    _(COLOR_ATTACHMENT_OUTPUT)        \
    _(COMPUTE_SHADER)                 \
    _(TRANSFER)                       \
    _(BOTTOM_OF_PIPE)                 \
    _(HOST)                           \
    _(ALL_GRAPHICS)                   \
    _(ALL_COMMANDS)

inline std::string GetPipelineStages(VkPipelineStageFlags stages) {
    std::string desc;
#define MK_PIPE_STAGE_CHECK(n) \
    if (stages & VK_PIPELINE_STAGE_##n##_BIT) desc += " " #n;
    LIST_PIPE_STAGES(MK_PIPE_STAGE_CHECK)
#undef MK_PIPE_STAGE_CHECK
    return desc;
}

[[noreturn]] inline void ThrowVkResult(VkResult res, const char* originator = nullptr, const char* sourceLocation = nullptr) {
    Throw("VkResult failure " + vkResultString(res), originator, sourceLocation);
}

#if defined(USE_CHECKPOINTS)
#define _CHECKPOINT(line) Checkpoint(__FUNCTION__ ":" #line)
#define CHECKPOINT() _CHECKPOINT(__LINE__)
#define SHOW_CHECKPOINTS() ShowCheckpoints()
static void ShowCheckpoints();
#else
#define CHECKPOINT() \
    do               \
        ;            \
    while (0)
#define SHOW_CHECKPOINTS() \
    do                     \
        ;                  \
    while (0)
#endif

inline VkResult CheckThrowVkResult(VkResult res, const char* originator = nullptr, const char* sourceLocation = nullptr) {
    if ((res) < VK_SUCCESS) {
        SHOW_CHECKPOINTS();
        ThrowVkResult(res, originator, sourceLocation);
    }

    return res;
}

// XXX These really shouldn't have trailing ';'s
#define XRC_THROW_VK(res, cmd) ThrowVkResult(res, #cmd, XRC_FILE_AND_LINE);
#define XRC_CHECK_THROW_VKCMD(cmd) CheckThrowVkResult(cmd, #cmd, XRC_FILE_AND_LINE);
#define XRC_CHECK_THROW_VKRESULT(res, cmdStr) CheckThrowVkResult(res, cmdStr, XRC_FILE_AND_LINE);

struct MemoryAllocator {
    void Init(VkPhysicalDevice physicalDevice, VkDevice device) {
        m_vkDevice = device;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &m_memProps);
    }

    void Reset() {
        m_memProps = {};
        m_vkDevice = VK_NULL_HANDLE;
    }

    static const VkFlags defaultFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    void Allocate(VkMemoryRequirements const& memReqs, VkDeviceMemory* mem, VkFlags flags = defaultFlags,
                  const void* pNext = nullptr) const {
        // Search memtypes to find first index with those properties
        for (uint32_t i = 0; i < m_memProps.memoryTypeCount; ++i) {
            if ((memReqs.memoryTypeBits & (1 << i)) != 0u) {
                // Type is available, does it match user properties?
                if ((m_memProps.memoryTypes[i].propertyFlags & flags) == flags) {
                    VkMemoryAllocateInfo memAlloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, pNext};
                    memAlloc.allocationSize = memReqs.size;
                    memAlloc.memoryTypeIndex = i;
                    XRC_CHECK_THROW_VKCMD(vkAllocateMemory(m_vkDevice, &memAlloc, nullptr, mem))
                    return;
                }
            }
        }
        XRC_THROW("Memory format not supported")
    }

   private:
    VkDevice m_vkDevice{VK_NULL_HANDLE};
    VkPhysicalDeviceMemoryProperties m_memProps{};
};

/// CmdBuffer - manage VkCommandBuffer state
struct CmdBuffer {
    enum class CmdBufferState { Undefined, Initialized, Recording, Executable, Executing };

    CmdBufferState state{CmdBufferState::Undefined};
    VkCommandPool pool{VK_NULL_HANDLE};
    VkCommandBuffer buf{VK_NULL_HANDLE};
    VkFence execFence{VK_NULL_HANDLE};

    CmdBuffer() = default;

    CmdBuffer(const CmdBuffer&) = delete;
    CmdBuffer& operator=(const CmdBuffer&) = delete;
    CmdBuffer(CmdBuffer&&) = delete;
    CmdBuffer& operator=(CmdBuffer&&) = delete;

    void Reset() {
        SetState(CmdBufferState::Undefined);
        if (m_vkDevice != nullptr) {
            if (buf != VK_NULL_HANDLE) {
                vkFreeCommandBuffers(m_vkDevice, pool, 1, &buf);
            }
            if (pool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(m_vkDevice, pool, nullptr);
            }
            if (execFence != VK_NULL_HANDLE) {
                vkDestroyFence(m_vkDevice, execFence, nullptr);
            }
        }
        buf = VK_NULL_HANDLE;
        pool = VK_NULL_HANDLE;
        execFence = VK_NULL_HANDLE;
        m_vkDevice = nullptr;
    }

    ~CmdBuffer() { Reset(); }

    bool Init(const VulkanDebugObjectNamer& namer, VkDevice device, uint32_t queueFamilyIndex) {
        XRC_CHECK_THROW((state == CmdBufferState::Undefined) || (state == CmdBufferState::Initialized))

        m_vkDevice = device;

        // Create a command pool to allocate our command buffer from
        VkCommandPoolCreateInfo cmdPoolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
        XRC_CHECK_THROW_VKCMD(vkCreateCommandPool(m_vkDevice, &cmdPoolInfo, nullptr, &pool));
        XRC_CHECK_THROW_VKCMD(namer.SetName(VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)pool, "hello_xr command pool"));

        // Create the command buffer from the command pool
        VkCommandBufferAllocateInfo cmd{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cmd.commandPool = pool;
        cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd.commandBufferCount = 1;
        XRC_CHECK_THROW_VKCMD(vkAllocateCommandBuffers(m_vkDevice, &cmd, &buf));
        XRC_CHECK_THROW_VKCMD(namer.SetName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)buf, "hello_xr command buffer"));

        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        XRC_CHECK_THROW_VKCMD(vkCreateFence(m_vkDevice, &fenceInfo, nullptr, &execFence));
        XRC_CHECK_THROW_VKCMD(namer.SetName(VK_OBJECT_TYPE_FENCE, (uint64_t)execFence, "hello_xr fence"));

        SetState(CmdBufferState::Initialized);
        return true;
    }

    bool Begin() {
        XRC_CHECK_THROW(state == CmdBufferState::Initialized);
        VkCommandBufferBeginInfo cmdBeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        XRC_CHECK_THROW_VKCMD(vkBeginCommandBuffer(buf, &cmdBeginInfo));
        SetState(CmdBufferState::Recording);
        return true;
    }

    bool End() {
        XRC_CHECK_THROW(state == CmdBufferState::Recording);
        XRC_CHECK_THROW_VKCMD(vkEndCommandBuffer(buf));
        SetState(CmdBufferState::Executable);
        return true;
    }

    bool Exec(VkQueue queue) {
        XRC_CHECK_THROW(state == CmdBufferState::Executable);

        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &buf;
        XRC_CHECK_THROW_VKCMD(vkQueueSubmit(queue, 1, &submitInfo, execFence));

        SetState(CmdBufferState::Executing);
        return true;
    }

    bool Wait() {
        // Waiting on a not-in-flight command buffer is a no-op
        if (state == CmdBufferState::Initialized) {
            return true;
        }

        XRC_CHECK_THROW(state == CmdBufferState::Executing);

        const uint32_t timeoutNs = 1 * 1000 * 1000 * 1000;
        for (int i = 0; i < 5; ++i) {
            auto res = vkWaitForFences(m_vkDevice, 1, &execFence, VK_TRUE, timeoutNs);
            if (res == VK_SUCCESS) {
                // Buffer can be executed multiple times...
                SetState(CmdBufferState::Executable);
                return true;
            }
            // Log::Write(Log::Level::Info, "Waiting for CmdBuffer fence timed out, retrying...");
        }

        return false;
    }

    bool Clear() {
        if (state != CmdBufferState::Initialized) {
            XRC_CHECK_THROW(state == CmdBufferState::Executable);

            XRC_CHECK_THROW_VKCMD(vkResetFences(m_vkDevice, 1, &execFence));
            XRC_CHECK_THROW_VKCMD(vkResetCommandBuffer(buf, 0));

            SetState(CmdBufferState::Initialized);
        }

        return true;
    }

   private:
    VkDevice m_vkDevice{VK_NULL_HANDLE};

    void SetState(CmdBufferState newState) { state = newState; }
};

enum ShaderProgramType {
    SHADER_PROGRAM_TYPE_GRAPHICS,
    SHADER_PROGRAM_TYPE_COMPUTE,
};

/// ShaderProgram to hold a pair of vertex & fragment shaders
struct ShaderProgram {
    std::vector<VkPipelineShaderStageCreateInfo> shaderInfo;
    ShaderProgramType m_programType;

    ShaderProgram(ShaderProgramType programType) : m_programType(programType) {
        switch (programType) {
            case SHADER_PROGRAM_TYPE_GRAPHICS: {
                shaderInfo.push_back({VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO});
                shaderInfo.push_back({VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO});
                break;
            }
            case SHADER_PROGRAM_TYPE_COMPUTE: {
                shaderInfo.push_back({VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO});
                break;
            }
            default:
                XRC_THROW("Unknown shader program type");
        }
    }

    void Reset() {
        if (m_vkDevice != nullptr) {
            for (auto& si : shaderInfo) {
                if (si.module != VK_NULL_HANDLE) {
                    vkDestroyShaderModule(m_vkDevice, si.module, nullptr);
                }
                si.module = VK_NULL_HANDLE;
            }
        }
        shaderInfo = {
            {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}, {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}}};
        m_vkDevice = nullptr;
    }

    ~ShaderProgram() { Reset(); }

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&&) = delete;
    ShaderProgram& operator=(ShaderProgram&&) = delete;

    void LoadVertexShader(const span<const uint32_t> code) { Load(0, code); }

    void LoadFragmentShader(const span<const uint32_t> code) { Load(1, code); }

    void LoadComputeShader(const span<const uint32_t> code) { Load(0, code); }

    void Init(VkDevice device) { m_vkDevice = device; }

   private:
    VkDevice m_vkDevice{VK_NULL_HANDLE};

    void Load(uint32_t index, const span<const uint32_t> code) {
        VkShaderModuleCreateInfo modInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};

        auto& si = shaderInfo[index];
        si.pName = "main";
        std::string name;

        switch (m_programType) {
            case SHADER_PROGRAM_TYPE_GRAPHICS: {
                switch (index) {
                    case 0:
                        si.stage = VK_SHADER_STAGE_VERTEX_BIT;
                        name = "vertex";
                        break;
                    case 1:
                        si.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                        name = "fragment";
                        break;
                    default:
                        XRC_THROW("Unknown code index " + std::to_string(index));
                }
            } break;

            case SHADER_PROGRAM_TYPE_COMPUTE: {
                switch (index) {
                    case 0:
                        si.stage = VK_SHADER_STAGE_COMPUTE_BIT;
                        name = "compute";
                        break;
                    default:
                        XRC_THROW("Unknown code index " + std::to_string(index));
                }
            } break;
        }

        modInfo.codeSize = code.size() * sizeof(code[0]);
        modInfo.pCode = code.data();
        XRC_CHECK_THROW_MSG((modInfo.codeSize > 0) && modInfo.pCode, "Invalid shader " + name);

        XRC_CHECK_THROW_VKCMD(vkCreateShaderModule(m_vkDevice, &modInfo, nullptr, &si.module));

        // Log::Write(Log::Level::Info, Fmt("Loaded %s shader", name.c_str()));
    }
};

/// A wrapper for VkBuffer and VkDeviceMemory.
/// DOES NOT clean up itself on destruction because it does not carry a VkDevice pointer - the containing/derived class's destructor
/// must call Reset(device)
struct BufferAndMemory {
    VkBuffer buf{VK_NULL_HANDLE};
    VkDeviceMemory mem{VK_NULL_HANDLE};

    /// Destroy the buffer and free the memory, if applicable.
    void Reset(VkDevice device) {
        if (device != nullptr) {
            if (buf != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, buf, nullptr);
            }
            if (mem != VK_NULL_HANDLE) {
                vkFreeMemory(device, mem, nullptr);
            }
        }
        buf = VK_NULL_HANDLE;
        mem = VK_NULL_HANDLE;
    }

    /// Swap the internals with another object.
    /// Used by subclasses to provide move construction/assignment.
    void Swap(BufferAndMemory& other) {
        using std::swap;
        swap(buf, other.buf);
        swap(mem, other.mem);
    }

    /// Create the buffer handle (using the specified VkBufferCreateInfo),
    /// allocate the memory, and bind the buffer to the memory.
    void Create(VkDevice device, const MemoryAllocator& memAllocator, const VkBufferCreateInfo& bufInfo) {
        XRC_CHECK_THROW_VKCMD(vkCreateBuffer(device, &bufInfo, nullptr, &this->buf));
        VkMemoryRequirements memReq = {};
        vkGetBufferMemoryRequirements(device, buf, &memReq);
        memAllocator.Allocate(memReq, &mem);
        XRC_CHECK_THROW_VKCMD(vkBindBufferMemory(device, this->buf, this->mem, 0));
    }

    /// Create the buffer handle (using the specified array count, usage, and element type `T`),
    /// allocate the memory, and bind the buffer to the memory.
    /// Function template to allow easy, generic access: caller must make sure the type used here matches the type used elsewhere!
    template <typename T>
    void Create(VkDevice device, const MemoryAllocator& memAllocator, uint32_t count, VkBufferUsageFlags usage) {
        VkBufferCreateInfo bufInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufInfo.usage = usage;
        bufInfo.size = sizeof(T) * count;
        Create(device, memAllocator, bufInfo);
    }

    /// Update the elements of the buffer using vkMapMemory.
    ///
    /// Function template to allow easy, generic access: caller must make sure the type used here matches the type used elsewhere!
    ///
    /// @param device The VkDevice associated with this buffer
    /// @param data Your data span (contiguous - pointer and size)
    /// @param offsetElements A zero-based **element** offset from the beginning of the memory object.
    template <typename T>
    void Update(VkDevice device, span<const T> data, uint32_t offsetElements = 0) {
        const size_t elements = data.size();
        T* map = nullptr;
        XRC_CHECK_THROW_VKCMD(vkMapMemory(device, this->mem, sizeof(T) * offsetElements, sizeof(T) * elements, 0, (void**)&map));
        for (size_t i = 0; i < elements; ++i) {
            map[i] = data[i];
        }
        vkUnmapMemory(device, this->mem);
    }
};

/// Type-generic base class for what d3d12 calls a "structured buffer" - an array of arbitrary things.
/// Unlike @ref UntypedBuffer this *does* carry the VkDevice
struct StructuredBufferBase : BufferAndMemory {
    /// Default constructible
    StructuredBufferBase() noexcept = default;

    /// Delete the buffer, free the memory, and reset the count to 0.
    void Reset() {
        BufferAndMemory::Reset(m_vkDevice);
        m_vkDevice = nullptr;
        m_memAllocator = nullptr;
        m_count = 0;
    }

    /// Destructor - frees resources
    ~StructuredBufferBase() { Reset(); }

    StructuredBufferBase(StructuredBufferBase&&) noexcept = delete;
    StructuredBufferBase& operator=(StructuredBufferBase&& other) noexcept = delete;
    StructuredBufferBase(const StructuredBufferBase&) = delete;
    StructuredBufferBase& operator=(const StructuredBufferBase&) = delete;

    /// Initialize with a device and a memory allocator
    void Init(VkDevice device, const MemoryAllocator& memAllocator) {
        m_vkDevice = device;
        m_memAllocator = &memAllocator;
    }

   protected:
    /// Swap the internals with another object.
    /// Used by subclasses to provide move construction/assignment.
    void Swap(StructuredBufferBase& other) {
        using std::swap;
        BufferAndMemory::Swap(other);
        swap(m_vkDevice, other.m_vkDevice);
        swap(m_count, other.m_count);
        swap(m_memAllocator, other.m_memAllocator);
    }
    VkDevice m_vkDevice{VK_NULL_HANDLE};
    uint32_t m_count{};
    const MemoryAllocator* m_memAllocator{nullptr};
};

/// Class template for a "Structured Buffer" - an array of some arbitrary type.
///
/// Most of the functionality is in StructuredBufferBase, only the few things that are type-specific are on this most-derived class.
template <typename T>
struct StructuredBuffer : StructuredBufferBase {
    /// Default constructible
    StructuredBuffer() noexcept = default;

    /// Move-constructor
    StructuredBuffer(StructuredBuffer<T>&& other) noexcept : StructuredBuffer() { Swap(other); }

    /// Move-assignment
    StructuredBuffer& operator=(StructuredBuffer<T>&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        Reset();
        Swap(other);
        return *this;
    }

    StructuredBuffer(const StructuredBuffer&) = delete;
    StructuredBuffer& operator=(const StructuredBuffer&) = delete;

    /// Create the buffer handle (using the specified array count and usage), allocate the memory,
    /// and bind the buffer to the memory.
    bool Create(uint32_t count, VkBufferUsageFlags usage) {
        BufferAndMemory::Create<T>(m_vkDevice, *m_memAllocator, count, usage);

        m_count = count;

        return true;
    }

    /// Update the elements of the buffer using vkMapMemory
    ///
    /// @param device The VkDevice associated with this buffer
    /// @param data Your data (contiguous)
    /// @param offsetElements A zero-based **element** offset from the beginning of the memory object.
    void Update(span<const T> data, uint32_t offsetElements = 0) { BufferAndMemory::Update(m_vkDevice, data, offsetElements); }

    /// Create a VkDescriptorBufferInfo covering the entire buffer
    VkDescriptorBufferInfo MakeDescriptor() { return {buf, 0, sizeof(T) * m_count}; }
};

// VertexBuffer base class
struct VertexBufferBase {
    BufferAndMemory idx;
    BufferAndMemory vtx;
    VkVertexInputBindingDescription bindDesc{};
    std::vector<VkVertexInputAttributeDescription> attrDesc{};
    struct {
        uint32_t idx;
        uint32_t vtx;
    } count = {0, 0};

    VertexBufferBase() = default;

    void Reset() {
        idx.Reset(m_vkDevice);
        vtx.Reset(m_vkDevice);
        bindDesc = {};
        attrDesc.clear();
        count = {0, 0};
        m_vkDevice = nullptr;
    }

    ~VertexBufferBase() { Reset(); }

    VertexBufferBase(const VertexBufferBase&) = delete;
    VertexBufferBase& operator=(const VertexBufferBase&) = delete;
    VertexBufferBase(VertexBufferBase&&) = delete;
    VertexBufferBase& operator=(VertexBufferBase&&) = delete;
    void Init(VkDevice device, const MemoryAllocator* memAllocator, std::vector<VkVertexInputAttributeDescription>&& attr) {
        m_vkDevice = device;
        m_memAllocator = memAllocator;
        attrDesc = std::move(attr);
    }

    void Init(VkDevice device, const MemoryAllocator* memAllocator, const std::vector<VkVertexInputAttributeDescription>& attr) {
        m_vkDevice = device;
        m_memAllocator = memAllocator;
        attrDesc = attr;
    }

   protected:
    VkDevice m_vkDevice{VK_NULL_HANDLE};

    /// Create and bind the index and vertex buffers.
    /// @pre Call Init()
    template <typename VertexType, typename IndexType>
    bool Create(uint32_t idxCount, uint32_t vtxCount) {
        idx.Create<IndexType>(m_vkDevice, *m_memAllocator, idxCount, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        vtx.Create<VertexType>(m_vkDevice, *m_memAllocator, vtxCount, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

        count = {idxCount, vtxCount};

        return true;
    }

    /// Swap the internals with another object.
    /// Used by VertexBuffer<T> to provide move construction/assignment.
    void Swap(VertexBufferBase& other) {
        using std::swap;
        swap(idx, other.idx);
        swap(vtx, other.vtx);
        swap(bindDesc, other.bindDesc);
        swap(attrDesc, other.attrDesc);
        swap(count, other.count);
        swap(m_vkDevice, other.m_vkDevice);
        swap(m_memAllocator, other.m_memAllocator);
    }

   private:
    const MemoryAllocator* m_memAllocator{nullptr};
};

// VertexBuffer template to wrap the indices and vertices
template <typename VertexType, typename IndexType = uint16_t>
struct VertexBuffer : public VertexBufferBase {
    static constexpr VkVertexInputBindingDescription c_bindingDesc = {0, sizeof(VertexType), VK_VERTEX_INPUT_RATE_VERTEX};

    /// Default constructible
    VertexBuffer() noexcept = default;

    /// Move-constructor
    VertexBuffer(VertexBuffer<VertexType, IndexType>&& other) noexcept : VertexBuffer() { Swap(other); }

    /// Move-assignment
    VertexBuffer& operator=(VertexBuffer<VertexType, IndexType>&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        Reset();
        Swap(other);
        return *this;
    }

    // no copy construct
    VertexBuffer(const VertexBuffer<VertexType, IndexType>&) = delete;
    // no copy assign
    VertexBuffer& operator=(const VertexBuffer<VertexType, IndexType>&) = delete;

    /// Create and bind the index and vertex buffers.
    /// @pre Call Init()
    bool Create(uint32_t idxCount, uint32_t vtxCount) {
        bindDesc = c_bindingDesc;
        return VertexBufferBase::Create<VertexType, IndexType>(idxCount, vtxCount);
    }

    /// Update the elements of the index buffer using vkMapMemory
    ///
    /// @param device The VkDevice associated with this buffer
    /// @param data Your index data (contiguous)
    /// @param offsetElements A zero-based **element** offset from the beginning of the memory object.
    void UpdateIndices(span<const IndexType> data, uint32_t offsetElements = 0) {
        idx.Update<IndexType>(m_vkDevice, data, offsetElements);
    }

    void UpdateVertices(span<const VertexType> data, uint32_t offsetElements = 0) {
        vtx.Update<VertexType>(m_vkDevice, data, offsetElements);
    }
};

template <typename VertexType, typename IndexType>
constexpr VkVertexInputBindingDescription VertexBuffer<VertexType, IndexType>::c_bindingDesc;

// RenderPass wrapper
struct RenderPass {
    VkFormat colorFmt{};
    VkFormat depthFmt{};
    VkSampleCountFlagBits sampleCount{};
    VkRenderPass pass{VK_NULL_HANDLE};

    RenderPass() = default;

    bool Create(const VulkanDebugObjectNamer& namer, VkDevice device, VkFormat aColorFmt, VkFormat aDepthFmt,
                VkSampleCountFlagBits aSampleCount) {
        m_vkDevice = device;
        colorFmt = aColorFmt;
        depthFmt = aDepthFmt;
        sampleCount = aSampleCount;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        std::array<VkAttachmentDescription, 2> at = {};

        VkRenderPassCreateInfo rpInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpInfo.attachmentCount = 0;
        rpInfo.pAttachments = at.data();
        rpInfo.subpassCount = 1;
        rpInfo.pSubpasses = &subpass;

        if (colorFmt != VK_FORMAT_UNDEFINED) {
            colorRef.attachment = rpInfo.attachmentCount++;

            at[colorRef.attachment].format = colorFmt;
            at[colorRef.attachment].samples = sampleCount;
            at[colorRef.attachment].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            at[colorRef.attachment].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            at[colorRef.attachment].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            at[colorRef.attachment].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            at[colorRef.attachment].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            at[colorRef.attachment].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorRef;
        }

        if (depthFmt != VK_FORMAT_UNDEFINED) {
            depthRef.attachment = rpInfo.attachmentCount++;

            at[depthRef.attachment].format = depthFmt;
            at[depthRef.attachment].samples = sampleCount;
            at[depthRef.attachment].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            at[depthRef.attachment].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            at[depthRef.attachment].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            at[depthRef.attachment].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            at[depthRef.attachment].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            at[depthRef.attachment].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            subpass.pDepthStencilAttachment = &depthRef;
        }

        XRC_CHECK_THROW_VKCMD(vkCreateRenderPass(m_vkDevice, &rpInfo, nullptr, &pass));
        XRC_CHECK_THROW_VKCMD(namer.SetName(VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)pass, "hello_xr render pass"));

        return true;
    }

    void Reset() {
        if (m_vkDevice != nullptr) {
            if (pass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(m_vkDevice, pass, nullptr);
            }
        }
        pass = VK_NULL_HANDLE;
        m_vkDevice = nullptr;
    }

    ~RenderPass() { Reset(); }

    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;
    RenderPass(RenderPass&&) = delete;
    RenderPass& operator=(RenderPass&&) = delete;

   private:
    VkDevice m_vkDevice{VK_NULL_HANDLE};
};

// VkImage + framebuffer wrapper
struct RenderTarget {
    VkImage colorImage{VK_NULL_HANDLE};
    VkImage depthImage{VK_NULL_HANDLE};
    VkImageView colorView{VK_NULL_HANDLE};
    VkImageView depthView{VK_NULL_HANDLE};
    VkFramebuffer fb{VK_NULL_HANDLE};

    RenderTarget() = default;

    ~RenderTarget() {
        if (m_vkDevice != VK_NULL_HANDLE) {
            if (fb != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(m_vkDevice, fb, nullptr);
            }
            if (colorView != VK_NULL_HANDLE) {
                vkDestroyImageView(m_vkDevice, colorView, nullptr);
            }
            if (depthView != VK_NULL_HANDLE) {
                vkDestroyImageView(m_vkDevice, depthView, nullptr);
            }
        }

        // Note we don't own color/depthImage, it will get destroyed when xrDestroySwapchain is called
        colorImage = VK_NULL_HANDLE;
        depthImage = VK_NULL_HANDLE;
        colorView = VK_NULL_HANDLE;
        depthView = VK_NULL_HANDLE;
        fb = VK_NULL_HANDLE;
        m_vkDevice = VK_NULL_HANDLE;
    }

    RenderTarget(RenderTarget&& other) noexcept : RenderTarget() {
        using std::swap;
        swap(colorImage, other.colorImage);
        swap(depthImage, other.depthImage);
        swap(colorView, other.colorView);
        swap(depthView, other.depthView);
        swap(fb, other.fb);
        swap(m_vkDevice, other.m_vkDevice);
    }
    RenderTarget& operator=(RenderTarget&& other) noexcept {
        if (&other == this) {
            return *this;
        }
        // Clean up ourselves.
        this->~RenderTarget();
        using std::swap;
        swap(colorImage, other.colorImage);
        swap(depthImage, other.depthImage);
        swap(colorView, other.colorView);
        swap(depthView, other.depthView);
        swap(fb, other.fb);
        swap(m_vkDevice, other.m_vkDevice);
        return *this;
    }
    void Create(const VulkanDebugObjectNamer& namer, VkDevice device, VkImage aColorImage, VkImage aDepthOrStencilImage,
                VkImageAspectFlags depthOrStencilImageAspect, uint32_t baseArrayLayer, VkExtent2D size, RenderPass& renderPass) {
        m_vkDevice = device;

        colorImage = aColorImage;
        depthImage = aDepthOrStencilImage;

        std::array<VkImageView, 2> attachments{};
        uint32_t attachmentCount = 0;

        // Create color image view
        if (colorImage != VK_NULL_HANDLE) {
            VkImageViewCreateInfo colorViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            colorViewInfo.image = colorImage;
            colorViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            colorViewInfo.format = renderPass.colorFmt;
            colorViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
            colorViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
            colorViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
            colorViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
            colorViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            colorViewInfo.subresourceRange.baseMipLevel = 0;
            colorViewInfo.subresourceRange.levelCount = 1;
            colorViewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
            colorViewInfo.subresourceRange.layerCount = 1;
            XRC_CHECK_THROW_VKCMD(vkCreateImageView(m_vkDevice, &colorViewInfo, nullptr, &colorView));
            XRC_CHECK_THROW_VKCMD(namer.SetName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)colorView, "hello_xr color image view"));
            attachments[attachmentCount++] = colorView;
        }

        // Create depth image view
        if (depthImage != VK_NULL_HANDLE) {
            VkImageViewCreateInfo depthViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            depthViewInfo.image = depthImage;
            depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            depthViewInfo.format = renderPass.depthFmt;
            depthViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
            depthViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
            depthViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
            depthViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
            depthViewInfo.subresourceRange.aspectMask = depthOrStencilImageAspect;
            depthViewInfo.subresourceRange.baseMipLevel = 0;
            depthViewInfo.subresourceRange.levelCount = 1;
            depthViewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
            depthViewInfo.subresourceRange.layerCount = 1;
            XRC_CHECK_THROW_VKCMD(vkCreateImageView(m_vkDevice, &depthViewInfo, nullptr, &depthView));

            const bool isDepth = depthOrStencilImageAspect & VK_IMAGE_ASPECT_DEPTH_BIT;
            const bool isStencil = depthOrStencilImageAspect & VK_IMAGE_ASPECT_STENCIL_BIT;
            if (isDepth && isStencil) {
                XRC_CHECK_THROW_VKCMD(
                    namer.SetName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)depthView, "hello_xr depth/stencil image view"));
            } else if (isDepth) {
                XRC_CHECK_THROW_VKCMD(namer.SetName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)depthView, "hello_xr depth image view"));
            } else if (isStencil) {
                XRC_CHECK_THROW_VKCMD(namer.SetName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)depthView, "hello_xr stencil image view"));
            }

            attachments[attachmentCount++] = depthView;
        }

        VkFramebufferCreateInfo fbInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fbInfo.renderPass = renderPass.pass;
        fbInfo.attachmentCount = attachmentCount;
        fbInfo.pAttachments = attachments.data();
        fbInfo.width = size.width;
        fbInfo.height = size.height;
        fbInfo.layers = 1;
        XRC_CHECK_THROW_VKCMD(vkCreateFramebuffer(m_vkDevice, &fbInfo, nullptr, &fb));
        XRC_CHECK_THROW_VKCMD(namer.SetName(VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)fb, "hello_xr framebuffer"));
    }

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

   private:
    VkDevice m_vkDevice{VK_NULL_HANDLE};
};

struct VulkanUniformBuffer {
    XrMatrix4x4f mvp;
    XrColor4f tintColor;
};

// Simple vertex MVP xform, tint color & color fragment shader layout
struct PipelineLayout {
    VkPipelineLayout layout{VK_NULL_HANDLE};

    // only set for compute pipelines
    VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};

    PipelineLayout() = default;

    void Reset() {
        if (m_vkDevice != nullptr) {
            if (layout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(m_vkDevice, layout, nullptr);
            }
            if (descriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(m_vkDevice, descriptorSetLayout, nullptr);
            }
        }
        layout = VK_NULL_HANDLE;
        m_vkDevice = nullptr;
    }

    ~PipelineLayout() { Reset(); }

    void Create(VkDevice device, ShaderProgramType programType = SHADER_PROGRAM_TYPE_GRAPHICS) {
        m_vkDevice = device;
        switch (programType) {
            case SHADER_PROGRAM_TYPE_GRAPHICS: {
                // MVP matrix is a push_constant
                VkPushConstantRange pcr = {};
                pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
                pcr.offset = 0;
                pcr.size = sizeof(VulkanUniformBuffer);

                VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
                pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
                pipelineLayoutCreateInfo.pPushConstantRanges = &pcr;
                XRC_CHECK_THROW_VKCMD(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &layout));
            } break;
            case SHADER_PROGRAM_TYPE_COMPUTE: {
                VkDescriptorSetLayoutBinding descriptorSetBindings[2]{};
                descriptorSetBindings[0].binding = 0;
                descriptorSetBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                descriptorSetBindings[0].descriptorCount = 1;
                descriptorSetBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

                descriptorSetBindings[1].binding = 1;
                descriptorSetBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorSetBindings[1].descriptorCount = 1;
                descriptorSetBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

                VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
                descriptorSetLayoutInfo.bindingCount = (uint32_t)ArraySize(descriptorSetBindings);
                descriptorSetLayoutInfo.pBindings = descriptorSetBindings;

                vkCreateDescriptorSetLayout(device,  //
                                            &descriptorSetLayoutInfo, NULL, &descriptorSetLayout);

                VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
                pipelineLayoutCreateInfo.setLayoutCount = 1;
                pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
                XRC_CHECK_THROW_VKCMD(vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutCreateInfo, nullptr, &layout));
            } break;
            default:
                XRC_THROW("Unknown programType");
        }
    }

    PipelineLayout(const PipelineLayout&) = delete;
    PipelineLayout& operator=(const PipelineLayout&) = delete;
    PipelineLayout(PipelineLayout&&) = delete;
    PipelineLayout& operator=(PipelineLayout&&) = delete;

   private:
    VkDevice m_vkDevice{VK_NULL_HANDLE};
};

// Pipeline wrapper for rendering pipeline state
struct Pipeline {
    VkPipeline pipe{VK_NULL_HANDLE};

    Pipeline() = default;
    ~Pipeline() { Reset(); }

    void CreateGraphics(VkDevice device, VkExtent2D /*size*/, const PipelineLayout& layout, const RenderPass& rp,
                        const ShaderProgram& sp, const VkVertexInputBindingDescription& bindDesc,
                        span<const VkVertexInputAttributeDescription> attrDesc, span<VkDynamicState> dynamicStates) {
        m_vkDevice = device;

        VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vi.vertexBindingDescriptionCount = 1;
        vi.pVertexBindingDescriptions = &bindDesc;
        vi.vertexAttributeDescriptionCount = (uint32_t)attrDesc.size();
        vi.pVertexAttributeDescriptions = attrDesc.data();

        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        ia.primitiveRestartEnable = VK_FALSE;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.cullMode = VK_CULL_MODE_BACK_BIT;
        rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rs.depthClampEnable = VK_FALSE;
        rs.rasterizerDiscardEnable = VK_FALSE;
        rs.depthBiasEnable = VK_FALSE;
        rs.depthBiasConstantFactor = 0;
        rs.depthBiasClamp = 0;
        rs.depthBiasSlopeFactor = 0;
        rs.lineWidth = 1.0f;

        VkPipelineColorBlendAttachmentState attachState{};
        attachState.blendEnable = 0;
        attachState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        attachState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        attachState.colorBlendOp = VK_BLEND_OP_ADD;
        attachState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        attachState.alphaBlendOp = VK_BLEND_OP_ADD;
        attachState.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        cb.attachmentCount = 1;
        cb.pAttachments = &attachState;
        cb.logicOpEnable = VK_FALSE;
        cb.logicOp = VK_LOGIC_OP_NO_OP;
        cb.blendConstants[0] = 1.0f;
        cb.blendConstants[1] = 1.0f;
        cb.blendConstants[2] = 1.0f;
        cb.blendConstants[3] = 1.0f;

        // Use dynamic scissor and viewport
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vp.viewportCount = 1;
        vp.scissorCount = 1;

        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        ds.depthTestEnable = VK_TRUE;
        ds.depthWriteEnable = VK_TRUE;
        ds.depthCompareOp = VK_COMPARE_OP_LESS;
        ds.depthBoundsTestEnable = VK_FALSE;
        ds.stencilTestEnable = VK_FALSE;
        ds.front.failOp = VK_STENCIL_OP_KEEP;
        ds.front.passOp = VK_STENCIL_OP_KEEP;
        ds.front.depthFailOp = VK_STENCIL_OP_KEEP;
        ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
        ds.back = ds.front;
        ds.minDepthBounds = 0.0f;
        ds.maxDepthBounds = 1.0f;

        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples = rp.sampleCount;

        VkGraphicsPipelineCreateInfo pipeInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipeInfo.stageCount = (uint32_t)sp.shaderInfo.size();
        pipeInfo.pStages = sp.shaderInfo.data();
        pipeInfo.pVertexInputState = &vi;
        pipeInfo.pInputAssemblyState = &ia;
        pipeInfo.pTessellationState = nullptr;
        pipeInfo.pViewportState = &vp;
        pipeInfo.pRasterizationState = &rs;
        pipeInfo.pMultisampleState = &ms;
        pipeInfo.pDepthStencilState = &ds;
        pipeInfo.pColorBlendState = &cb;
        if (dynamicState.dynamicStateCount > 0) {
            pipeInfo.pDynamicState = &dynamicState;
        }
        pipeInfo.layout = layout.layout;
        pipeInfo.renderPass = rp.pass;
        pipeInfo.subpass = 0;

        Create(device, pipeInfo);
    }

    void CreateCompute(VkDevice device, VkExtent2D /*size*/, const PipelineLayout& layout, const RenderPass& /* rp */,
                       const ShaderProgram& sp, const VkVertexInputBindingDescription& /* bindDesc */,
                       span<const VkVertexInputAttributeDescription> /* attrDesc */, span<VkDynamicState> /* dynamicStates */) {
        VkComputePipelineCreateInfo pipeInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipeInfo.flags = 0;
        pipeInfo.stage = sp.shaderInfo[0], pipeInfo.layout = layout.layout,

        Create(device, pipeInfo);
    }

    void Create(VkDevice device, VkExtent2D size, const PipelineLayout& layout, const RenderPass& rp, const ShaderProgram& sp,
                const VkVertexInputBindingDescription& bindDesc, span<const VkVertexInputAttributeDescription> attrDesc,
                span<VkDynamicState> dynamicStates) {
        switch (sp.m_programType) {
            case SHADER_PROGRAM_TYPE_GRAPHICS:
                CreateGraphics(device, size, layout, rp, sp, bindDesc, attrDesc, dynamicStates);
                break;
            case SHADER_PROGRAM_TYPE_COMPUTE:
                CreateCompute(device, size, layout, rp, sp, bindDesc, attrDesc, dynamicStates);
                break;
            default:
                XRC_THROW("Unknown programType")
        }
    }

    void Create(VkDevice device, const VkGraphicsPipelineCreateInfo& info) {
        m_vkDevice = device;

        XRC_CHECK_THROW_VKCMD(vkCreateGraphicsPipelines(m_vkDevice, VK_NULL_HANDLE, 1, &info, nullptr, &pipe));
    }

    void Create(VkDevice device, const VkComputePipelineCreateInfo& info) {
        m_vkDevice = device;

        XRC_CHECK_THROW_VKCMD(vkCreateComputePipelines(m_vkDevice, VK_NULL_HANDLE, 1, &info, nullptr, &pipe));
    }

    void Reset() {
        if (m_vkDevice != nullptr) {
            if (pipe != VK_NULL_HANDLE) {
                vkDestroyPipeline(m_vkDevice, pipe, nullptr);
            }
        }
        pipe = VK_NULL_HANDLE;
        m_vkDevice = nullptr;
    }

   private:
    VkDevice m_vkDevice{VK_NULL_HANDLE};
};

struct DepthBuffer {
    VkDeviceMemory depthMemory{VK_NULL_HANDLE};
    VkImage depthImage{VK_NULL_HANDLE};

    DepthBuffer() = default;
    ~DepthBuffer() { Reset(); }

    void Reset() {
        if (m_vkDevice != nullptr) {
            if (depthImage != VK_NULL_HANDLE) {
                vkDestroyImage(m_vkDevice, depthImage, nullptr);
            }
            if (depthMemory != VK_NULL_HANDLE) {
                vkFreeMemory(m_vkDevice, depthMemory, nullptr);
            }
        }
        depthImage = VK_NULL_HANDLE;
        depthMemory = VK_NULL_HANDLE;
        m_vkDevice = nullptr;
        m_initialized = false;
    }
    void swap(DepthBuffer& other) noexcept {
        using std::swap;

        swap(depthImage, other.depthImage);
        swap(depthMemory, other.depthMemory);
        swap(m_vkDevice, other.m_vkDevice);
        swap(m_initialized, other.m_initialized);
        swap(m_xrImage, other.m_xrImage);
    }

    DepthBuffer(DepthBuffer&& other) noexcept : DepthBuffer() { swap(other); }
    DepthBuffer& operator=(DepthBuffer&& other) noexcept {
        if (&other == this) {
            return *this;
        }
        // clean up self
        this->~DepthBuffer();
        swap(other);

        return *this;
    }

    bool Allocated() { return m_initialized; }

    void Allocate(const VulkanDebugObjectNamer& namer, VkDevice device, MemoryAllocator* memAllocator, VkFormat depthFormat,
                  uint32_t width, uint32_t height, uint32_t arraySize, uint32_t sampleCount) {
        Reset();

        m_vkDevice = device;

        // Create a D32 depthbuffer
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = arraySize;
        imageInfo.format = depthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = (VkSampleCountFlagBits)sampleCount;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        XRC_CHECK_THROW_VKCMD(vkCreateImage(device, &imageInfo, nullptr, &depthImage));
        XRC_CHECK_THROW_VKCMD(namer.SetName(VK_OBJECT_TYPE_IMAGE, (uint64_t)depthImage, "hello_xr fallback depth image"));
        m_xrImage.image = depthImage;

        VkMemoryRequirements memRequirements{};
        vkGetImageMemoryRequirements(device, depthImage, &memRequirements);
        memAllocator->Allocate(memRequirements, &depthMemory, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        XRC_CHECK_THROW_VKCMD(
            namer.SetName(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)depthMemory, "hello_xr fallback depth image memory"));
        XRC_CHECK_THROW_VKCMD(vkBindImageMemory(device, depthImage, depthMemory, 0));

        m_initialized = true;
    }

    void TransitionLayout(CmdBuffer* cmdBuffer, VkImageLayout newLayout) {
        if (!m_initialized || (newLayout == m_vkLayout)) {
            return;
        }

        VkImageMemoryBarrier depthBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        depthBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        depthBarrier.oldLayout = m_vkLayout;
        depthBarrier.newLayout = newLayout;
        depthBarrier.image = depthImage;
        depthBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(cmdBuffer->buf, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr,
                             0, nullptr, 1, &depthBarrier);

        m_vkLayout = newLayout;
    }
    const XrSwapchainImageVulkanKHR& GetTexture() const { return m_xrImage; }

    DepthBuffer(const DepthBuffer&) = delete;
    DepthBuffer& operator=(const DepthBuffer&) = delete;

   private:
    bool m_initialized{false};
    VkDevice m_vkDevice{VK_NULL_HANDLE};
    VkImageLayout m_vkLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    XrSwapchainImageVulkanKHR m_xrImage{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR, nullptr, VK_NULL_HANDLE};
};

#endif  // XR_USE_GRAPHICS_API_VULKAN
