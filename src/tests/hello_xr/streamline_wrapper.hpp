/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _STREAMLINE_WRAPPER_H_
#define _STREAMLINE_WRAPPER_H_

#if ENABLE_STREAMLINE

#include <sl.h>
#include <sl_dlss.h>
//#include <sl_dlss_g.h>
#include <sl_reflex.h>
#include <sl_consts.h>
#include <sl_hooks.h>

#include <sl_helpers.h>
#include <sl_helpers_vk.h>
//#include <sl_nrd.h>
#include <sl_security.h>

#if SL_MANUAL_HOOKING

class StreamlineWrapper
{
public:
  static StreamlineWrapper& get()
  {
    static StreamlineWrapper wrapper;
    return wrapper;
  }

  bool isLoaded() const { return m_interposerModule != nullptr; }

  void initFunctions()
  {
    sl.Init = reinterpret_cast<PFun_slInit*>(GetProcAddress(m_interposerModule, "slInit"));
    sl.Shutdown = reinterpret_cast<PFun_slShutdown*>(GetProcAddress(m_interposerModule, "slShutdown"));
    sl.IsFeatureSupported = reinterpret_cast<PFun_slIsFeatureSupported*>(GetProcAddress(m_interposerModule, "slIsFeatureSupported"));
    sl.SetTag = reinterpret_cast<PFun_slSetTag*>(GetProcAddress(m_interposerModule, "slSetTag"));
    sl.SetConstants = reinterpret_cast<PFun_slSetConstants*>(GetProcAddress(m_interposerModule, "slSetConstants"));
    sl.GetFeatureRequirements = reinterpret_cast<PFun_slGetFeatureRequirements*>(GetProcAddress(m_interposerModule, "slGetFeatureRequirements"));
    sl.EvaluateFeature = reinterpret_cast<PFun_slEvaluateFeature*>(GetProcAddress(m_interposerModule, "slEvaluateFeature"));
    sl.GetFeatureFunction = reinterpret_cast<PFun_slGetFeatureFunction*>(GetProcAddress(m_interposerModule, "slGetFeatureFunction"));
    sl.GetNewFrameToken = reinterpret_cast<PFun_slGetNewFrameToken*>(GetProcAddress(m_interposerModule, "slGetNewFrameToken"));
    sl.SetVulkanInfo = reinterpret_cast<PFun_slSetVulkanInfo*>(GetProcAddress(m_interposerModule, "slSetVulkanInfo"));
  }

  void initVulkanHooks(VkDevice device)
  {
    const auto getDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(GetProcAddress(m_interposerModule, "vkGetDeviceProcAddr"));

    vk.CreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(getDeviceProcAddr(device, "vkCreateSwapchainKHR"));
    vk.DestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(getDeviceProcAddr(device, "vkDestroySwapchainKHR"));
    vk.GetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(getDeviceProcAddr(device, "vkGetSwapchainImagesKHR"));
    vk.AcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(getDeviceProcAddr(device, "vkAcquireNextImageKHR"));
    vk.QueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(getDeviceProcAddr(device, "vkQueuePresentKHR"));
  }

  struct FunctionTableSL
  {
    PFun_slInit* Init;
    PFun_slShutdown* Shutdown;
    PFun_slIsFeatureSupported* IsFeatureSupported;
    PFun_slSetTag* SetTag;
    PFun_slSetConstants* SetConstants;
    PFun_slGetFeatureRequirements* GetFeatureRequirements;
    PFun_slEvaluateFeature* EvaluateFeature;
    PFun_slGetFeatureFunction* GetFeatureFunction;
    PFun_slGetNewFrameToken* GetNewFrameToken;
    PFun_slSetVulkanInfo* SetVulkanInfo;
  } sl = {};

  struct FunctionTableVK
  {
    PFN_vkCreateSwapchainKHR CreateSwapchainKHR;
    PFN_vkDestroySwapchainKHR DestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR AcquireNextImageKHR;
    PFN_vkQueuePresentKHR QueuePresentKHR;
  } vk = {};

private:
  StreamlineWrapper()
  {
    // Get absolute path to interposer DLL
    std::wstring interposerModulePath(MAX_PATH, L'\0');
    interposerModulePath.resize(GetModuleFileNameW(nullptr, (LPWSTR)interposerModulePath.data(), static_cast<DWORD>(interposerModulePath.size())));
    interposerModulePath.erase(interposerModulePath.rfind('\\'));
    interposerModulePath.append(L"\\sl.interposer.dll");

#if 0
    // Optionally verify that the interposer DLL is signed by NVIDIA
    if (!sl::security::verifyEmbeddedSignature(interposerModulePath.c_str()))
      return;
#endif

    m_interposerModule = LoadLibraryW(interposerModulePath.c_str());

    if (isLoaded())
      initFunctions();
  }
  ~StreamlineWrapper()
  {
    if (isLoaded())
      FreeLibrary(m_interposerModule);
  }

  StreamlineWrapper(const StreamlineWrapper&) = delete;
  StreamlineWrapper& operator=(const StreamlineWrapper&) = delete;

  HMODULE m_interposerModule;
};

// Dynamically load Streamline functions on first call
inline sl::Result slInit(const sl::Preferences& pref, uint64_t sdkVersion)
{
  if (!StreamlineWrapper::get().isLoaded())
    return sl::Result::eErrorNotInitialized;

  return StreamlineWrapper::get().sl.Init(pref, sdkVersion);
}
inline sl::Result slShutdown()
{
  return StreamlineWrapper::get().sl.Shutdown();
}
inline sl::Result slIsFeatureSupported(sl::Feature feature, const sl::AdapterInfo& adapterInfo)
{
  return StreamlineWrapper::get().sl.IsFeatureSupported(feature, adapterInfo);
}
inline sl::Result slSetTag(const sl::ViewportHandle& viewport, const sl::ResourceTag* tags, uint32_t numTags, sl::CommandBuffer* cmdBuffer)
{
  return StreamlineWrapper::get().sl.SetTag(viewport, tags, numTags, cmdBuffer);
}
inline sl::Result slSetConstants(const sl::Constants& values, const sl::FrameToken& frame, const sl::ViewportHandle& viewport)
{
  return StreamlineWrapper::get().sl.SetConstants(values, frame, viewport);
}
inline sl::Result slGetFeatureRequirements(sl::Feature feature, sl::FeatureRequirements& requirements)
{
  return StreamlineWrapper::get().sl.GetFeatureRequirements(feature, requirements);
}
inline sl::Result slEvaluateFeature(sl::Feature feature, const sl::FrameToken& frame, const sl::BaseStructure** inputs, uint32_t numInputs, sl::CommandBuffer* cmdBuffer)
{
  return StreamlineWrapper::get().sl.EvaluateFeature(feature, frame, inputs, numInputs, cmdBuffer);
}
inline sl::Result slGetFeatureFunction(sl::Feature feature, const char* functionName, void*& function)
{
  return StreamlineWrapper::get().sl.GetFeatureFunction(feature, functionName, function);
}
inline sl::Result slGetNewFrameToken(sl::FrameToken*& token, const uint32_t* frameIndex)
{
  return StreamlineWrapper::get().sl.GetNewFrameToken(token, const_cast<uint32_t*>(frameIndex));
}
inline sl::Result slSetVulkanInfo(const sl::VulkanInfo& info)
{
  return StreamlineWrapper::get().sl.SetVulkanInfo(info);
}

// Override linked Vulkan functions, so that they are redirected to Streamline
// In a real application one should instead just use the function pointers directly, but for simplicity this sample links against vulkan-1.lib, so need to override those imports instead
extern VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain)
{
  return StreamlineWrapper::get().vk.CreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
}
extern void VKAPI_CALL vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator)
{
  StreamlineWrapper::get().vk.DestroySwapchainKHR(device, swapchain, pAllocator);
}
extern VkResult VKAPI_CALL vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages)
{
  return StreamlineWrapper::get().vk.GetSwapchainImagesKHR(device, swapchain, pSwapchainImageCount, pSwapchainImages);
}
extern VkResult VKAPI_CALL vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex)
{
  return StreamlineWrapper::get().vk.AcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
}
extern VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
{
  return StreamlineWrapper::get().vk.QueuePresentKHR(queue, pPresentInfo);
}

#else
#pragma comment(lib, "sl.interposer.lib")
#endif

#endif

#endif
