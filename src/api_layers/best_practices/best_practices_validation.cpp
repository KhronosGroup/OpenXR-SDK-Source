// Copyright (c) 2017-2025 The Khronos Group Inc.
// Copyright (c) 2025 Collabora, Ltd.
// Copyright (c) 2017-2019 Valve Corporation
// Copyright (c) 2017-2019 LunarG, Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "layer_utils.h"

#include "hex_and_handles.h"
#include "platform_utils.hpp"
#include "xr_generated_dispatch_table.h"

#include <openxr/openxr.h>
#include <openxr/openxr_loader_negotiation.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include <deque>

#ifdef __ANDROID__
#include "android/log.h"
#endif

#if defined(__GNUC__) && __GNUC__ >= 4
#define LAYER_EXPORT __attribute__((visibility("default")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590)
#define LAYER_EXPORT __attribute__((visibility("default")))
#elif defined(_WIN32)
#define LAYER_EXPORT __declspec(dllexport)
#else
#define LAYER_EXPORT
#endif

// For routing platform_utils.hpp messages.
void LogPlatformUtilsError(const std::string &message) {
    (void)message;  // maybe unused
#if !defined(NDEBUG)
    std::cerr << message << std::endl;
#endif

#if defined(XR_OS_WINDOWS)
    OutputDebugStringA((message + "\n").c_str());
#elif defined(XR_OS_ANDROID)
    __android_log_write(ANDROID_LOG_ERROR, "OpenXR-BestPractices", message.c_str());
#endif
}

struct FrameState {
    bool waitFrameCalled;
    XrResult waitFrameResult;
    bool beginFrameCalled;
    XrResult beginFrameResult;
    bool endFrameCalled;
    XrResult endFrameResult;
    bool syncActionsSucceeded;
    XrTime predictedDisplayTime;
    XrTime predictedDisplayPeriod;
    uint32_t frameIndex;
    XrView views[2];
};

static LockedDispatchTable g_nextDispatch{};
static std::deque<FrameState> g_framesInFlight;
static std::mutex g_frameMutex;
static uint32_t g_frameIndex;
static XrTime g_lastEndFramePDT;

std::unordered_map<std::string, int> BPLogger::errorCounts;

PFN_xrVoidFunction BestPracticesLayerInnerGetInstanceProcAddr(const char *name);

XRAPI_ATTR XrResult XRAPI_CALL BestPracticesLayerXrWaitFrame(XrSession session, const XrFrameWaitInfo *frameWaitInfo,
                                                             XrFrameState *frameState) {
    XrResult result = XR_SUCCESS;
    {
        std::unique_lock<std::mutex> frameLock(g_frameMutex);

        // Remove any invalid frames that were created just because xrWaitFrame failed and the application tried again immediately.
        for (auto it = g_framesInFlight.begin(); it != g_framesInFlight.end();) {
            if (it->waitFrameCalled && it->waitFrameResult != XR_SUCCESS && !it->beginFrameCalled) {
                it = g_framesInFlight.erase(it);
            } else {
                ++it;
            }
        }

        if (g_framesInFlight.size() > 0) {
            FrameState &frontFrame = g_framesInFlight.front();
            // If the frame in front failed xrBeginFrame, we can assume it's on it's way to be an invalid frame. Reset
            // beginFrameCalled so it can be easily cleaned up in the next xrBeginFrame call.
            if (frontFrame.beginFrameCalled && frontFrame.beginFrameResult != XR_SUCCESS) {
                frontFrame.beginFrameCalled = false;
            } else if (!frontFrame.beginFrameCalled) {
                BPLogger::LogMessage(
                    "xrWaitFrame was called twice in a row, the last xrWaitFrame was not followed by a xrBeginFrame.");
            }
        }

        // Create new frame state
        FrameState newFrameState = {};
        newFrameState.waitFrameCalled = false;
        newFrameState.beginFrameCalled = false;
        newFrameState.endFrameCalled = false;
        newFrameState.syncActionsSucceeded = false;
        newFrameState.waitFrameResult = XR_SUCCESS;
        newFrameState.beginFrameResult = XR_SUCCESS;
        newFrameState.endFrameResult = XR_SUCCESS;

        newFrameState.frameIndex = ++g_frameIndex;

        g_framesInFlight.push_back(newFrameState);
    }

    result = g_nextDispatch.Get<PFN_xrWaitFrame>(&XrGeneratedDispatchTable::WaitFrame)(session, frameWaitInfo, frameState);
    {
        std::unique_lock<std::mutex> frameLock(g_frameMutex);

        FrameState &currentFrameState = g_framesInFlight.back();
        currentFrameState.waitFrameCalled = true;
        currentFrameState.waitFrameResult = result;
        currentFrameState.predictedDisplayTime = frameState->predictedDisplayTime;
        currentFrameState.predictedDisplayPeriod = frameState->predictedDisplayPeriod;
    }

    return result;
}

XRAPI_ATTR XrResult XRAPI_CALL BestPracticesLayerXrBeginFrame(XrSession session, const XrFrameBeginInfo *frameBeginInfo) {
    XrResult result = XR_SUCCESS;
    {
        std::unique_lock<std::mutex> frameLock(g_frameMutex);

        if (g_framesInFlight.empty()) {
            BPLogger::LogMessage("There are no frames in queue. XrWaitFrame has not been called");
        }

        FrameState &currentFrameState = g_framesInFlight.front();

        if (currentFrameState.beginFrameCalled) {
            if (currentFrameState.beginFrameResult >= XR_SUCCESS) {
                // Failure case where xrEndFrame from the last frame was not successful, but everything else was.
                if (!currentFrameState.endFrameCalled || currentFrameState.endFrameResult != XR_SUCCESS) {
                    BPLogger::LogMessage(
                        "xrEndFrame was not successful for the previous frame. This xrBeginFrame is for a new frame.");
                    g_framesInFlight.pop_front();
                    if (g_framesInFlight.size() > 0) currentFrameState = g_framesInFlight.front();
                }
            } else {
                // Application is retrying xrBeginFrame and frame state may still be valid if this call succeeds.
                BPLogger::LogMessage(
                    "Application is retrying xrBeginFrame after a previous failure. Consider calling xrWaitFrame to start a new "
                    "frame instead");
            }
        } else {
            // beginFrameCalled being false but having a failure result means this frame was reset in xrWaitFrame for being invalid,
            // remove it here.
            if (currentFrameState.beginFrameResult != XR_SUCCESS) {
                g_framesInFlight.pop_front();
                if (g_framesInFlight.size() > 0) currentFrameState = g_framesInFlight.front();
            }
        }

        if (!currentFrameState.waitFrameCalled || currentFrameState.waitFrameResult != XR_SUCCESS) {
            BPLogger::LogMessage("XrWaitFrame was not called or failed for frame " + std::to_string(currentFrameState.frameIndex));
        }

        result = g_nextDispatch.Get<PFN_xrBeginFrame>(&XrGeneratedDispatchTable::BeginFrame)(session, frameBeginInfo);

        currentFrameState.beginFrameResult = result;
        currentFrameState.beginFrameCalled = true;
    }

    return result;
}

XRAPI_ATTR XrResult XRAPI_CALL BestPracticesLayerXrEndFrame(XrSession session, const XrFrameEndInfo *frameEndInfo) {
    XrResult result = XR_SUCCESS;
    if (frameEndInfo->environmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND) {
        bool sourceAlphaSet = true;
        for (uint32_t i = 0; i < frameEndInfo->layerCount; i++) {
            const XrCompositionLayerBaseHeader *layer = frameEndInfo->layers[i];
            if ((layer->layerFlags & XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT) == 0) {
                sourceAlphaSet = false;
            }
        }
        if (!sourceAlphaSet) {
            BPLogger::LogMessage(
                "Environment Blend Mode was set to XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND but no layer has "
                "XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT set. Either add "
                "XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT or do not use XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND.");
        }
    }

    {
        std::unique_lock<std::mutex> frameLock(g_frameMutex);
        FrameState &currentFrameState = g_framesInFlight.front();

        if (!currentFrameState.waitFrameCalled || currentFrameState.waitFrameResult != XR_SUCCESS) {
            BPLogger::LogMessage("xrWaitFrame was not called or failed before calling XrEndFrame for frame " +
                                 std::to_string(currentFrameState.frameIndex));
        }

        if (!currentFrameState.beginFrameCalled || currentFrameState.beginFrameResult != XR_SUCCESS) {
            BPLogger::LogMessage("xrBeginFrame was not called or failed before calling XrEndFrame for frame " +
                                 std::to_string(currentFrameState.frameIndex));
        }

        if (currentFrameState.predictedDisplayTime != 0 && frameEndInfo->displayTime != currentFrameState.predictedDisplayTime) {
            BPLogger::LogMessage("xrEndFrame was called with a different displayTime than what was obtained from xrWaitFrame.");
        }

        for (uint32_t i = 0; i < frameEndInfo->layerCount; i++) {
            const XrCompositionLayerBaseHeader *layer = frameEndInfo->layers[i];
            if (layer->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
                const XrCompositionLayerProjection *projLayer = reinterpret_cast<const XrCompositionLayerProjection *>(layer);
                for (uint32_t view = 0; view < projLayer->viewCount; view++) {
                    if (projLayer->views[view].fov.angleLeft != currentFrameState.views[view].fov.angleLeft ||
                        projLayer->views[view].fov.angleRight != currentFrameState.views[view].fov.angleRight ||
                        projLayer->views[view].fov.angleDown != currentFrameState.views[view].fov.angleDown ||
                        projLayer->views[view].fov.angleUp != currentFrameState.views[view].fov.angleUp) {
                        BPLogger::LogMessage(
                            "xrEndFrame Projection Layer has a different FOV from what was acquired in xrLocateViews");
                    }

                    if (projLayer->views[view].fov.angleLeft == 0.0f && projLayer->views[view].fov.angleRight == 0.0f &&
                        projLayer->views[view].fov.angleDown == 0.0f && projLayer->views[view].fov.angleUp == 0.0f) {
                        BPLogger::LogMessage("xrEndFrame Projection Layer needs to have a non-zero FOV");
                    }

                    if (projLayer->views[view].pose.position.x != currentFrameState.views[view].pose.position.x ||
                        projLayer->views[view].pose.position.y != currentFrameState.views[view].pose.position.y ||
                        projLayer->views[view].pose.position.z != currentFrameState.views[view].pose.position.z) {
                        BPLogger::LogMessage(
                            "xrEndFrame Projection Layer has a different positional pose from what was acquired in xrLocateViews");
                    }

                    if (projLayer->views[view].pose.orientation.x != currentFrameState.views[view].pose.orientation.x ||
                        projLayer->views[view].pose.orientation.y != currentFrameState.views[view].pose.orientation.y ||
                        projLayer->views[view].pose.orientation.z != currentFrameState.views[view].pose.orientation.z ||
                        projLayer->views[view].pose.orientation.w != currentFrameState.views[view].pose.orientation.w) {
                        BPLogger::LogMessage(
                            "xrEndFrame Projection Layer has a different rotational pose from what was acquired in xrLocateViews");
                    }
                }
            }
        }

        result = g_nextDispatch.Get<PFN_xrEndFrame>(&XrGeneratedDispatchTable::EndFrame)(session, frameEndInfo);

        // If we get to this point, xrWaitFrame, xrBeginFrame, and xrEndFrame were all successful so we can retire this frame data.
        if (result >= XR_SUCCESS) {
            g_framesInFlight.pop_front();
        } else {
            currentFrameState.endFrameResult = result;
            g_lastEndFramePDT = frameEndInfo->displayTime;
        }

        // This is Scenario B in the xrEndFrame failure flow, if the app is retrying we warn that the frame should be discarded
        // instead.
        if (frameEndInfo->displayTime == g_lastEndFramePDT) {
            BPLogger::LogMessage("xrEndFrame was retried with the same display time, consider discarding the frame instead.");
        }
    }

    return result;
}

XRAPI_ATTR XrResult XRAPI_CALL BestPracticesLayerXrSyncActions(XrSession session, const XrActionsSyncInfo *syncInfo) {
    XrResult result = XR_SUCCESS;
    {
        bool syncCalledBeforeWait = false;
        std::unique_lock<std::mutex> frameLock(g_frameMutex);

        if (g_framesInFlight.empty()) {
            // If there are no frames in flight, xrSyncActions was called outside the frame loop which is probably before
            // xrWaitFrame.
            syncCalledBeforeWait = true;
        } else {
            FrameState &currentFrameState = g_framesInFlight.front();
            // In a pipelined system, if we begin frame N but get a call to XrSyncActions after the fact, it's probably for frame
            // N+1 which hasn't even had xrWaitFrame called which is too early.
            if (currentFrameState.waitFrameCalled && currentFrameState.waitFrameResult >= XR_SUCCESS &&
                currentFrameState.beginFrameCalled && currentFrameState.beginFrameResult >= XR_SUCCESS) {
                syncCalledBeforeWait = true;
            }

            if (currentFrameState.syncActionsSucceeded) {
                BPLogger::LogMessage(
                    "xrSyncActions was called multiple times in the frame. It's best practice to avoid unnecessary extra calls.");
            }
        }

        if (syncCalledBeforeWait) {
            BPLogger::LogMessage(
                "xrSyncActions was called between xrBeginFrame and xrEndFrame. If this is before next frame's xrWaitFrame, "
                "consider doing it after.");
        }
    }

    result = g_nextDispatch.Get<PFN_xrSyncActions>(&XrGeneratedDispatchTable::SyncActions)(session, syncInfo);

    if (result == XR_SUCCESS && !g_framesInFlight.empty()) {
        std::unique_lock<std::mutex> frameLock(g_frameMutex);
        FrameState &currentFrameState = g_framesInFlight.front();
        currentFrameState.syncActionsSucceeded = true;
    }
    return result;
}

XRAPI_ATTR XrResult XRAPI_CALL BestPracticesLayerXrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time,
                                                               XrSpaceLocation *location) {
    XrResult result = XR_SUCCESS;
    {
        bool locateCalledBeforeWait = false;
        std::unique_lock<std::mutex> frameLock(g_frameMutex);

        if (g_framesInFlight.empty()) {
            // If there are no frames in flight, xrSyncActions was called outside the frame loop which is probably before
            // xrWaitFrame.
            locateCalledBeforeWait = true;
        } else {
            FrameState &currentFrameState = g_framesInFlight.front();
            if (!currentFrameState.waitFrameCalled) {
                locateCalledBeforeWait = true;
            }
        }

        if (locateCalledBeforeWait) {
            BPLogger::LogMessage(
                "xrLocateSpace was called before xrWaitFrame. It's best practice to call xrLocateSpace after xrWaitFrame to have "
                "more accurate tracking data.");
        }
    }

    result = g_nextDispatch.Get<PFN_xrLocateSpace>(&XrGeneratedDispatchTable::LocateSpace)(space, baseSpace, time, location);

    return result;
}

XRAPI_ATTR XrResult XRAPI_CALL BestPracticesLayerXrLocateViews(XrSession session, const XrViewLocateInfo *viewLocateInfo,
                                                               XrViewState *viewState, uint32_t viewCapacityInput,
                                                               uint32_t *viewCountOutput, XrView *views) {
    XrResult result = XR_SUCCESS;
    if (!g_framesInFlight.empty()) {
        std::unique_lock<std::mutex> frameLock(g_frameMutex);
        FrameState &currentFrameState = g_framesInFlight.front();
        if (currentFrameState.predictedDisplayTime != 0 && viewLocateInfo->displayTime != currentFrameState.predictedDisplayTime) {
            BPLogger::LogMessage("xrLocateViews was called with a different displayTime than what was obtained from xrWaitFrame.");
        }
    }

    result = g_nextDispatch.Get<PFN_xrLocateViews>(&XrGeneratedDispatchTable::LocateViews)(
        session, viewLocateInfo, viewState, viewCapacityInput, viewCountOutput, views);

    if (!g_framesInFlight.empty()) {
        if (viewCapacityInput != 0) {
            std::unique_lock<std::mutex> frameLock(g_frameMutex);
            FrameState &currentFrameState = g_framesInFlight.front();
            for (uint32_t i = 0; i < viewCapacityInput; i++) {
                currentFrameState.views[i] = views[i];
            }
        }
    } else {
        BPLogger::LogMessage(
            "xrLocateViews was called before xrWaitFrame with no frames in flight. Consider making the call between xrBeginFrame "
            "and xrEndFrame.");
    }
    return result;
}

XRAPI_ATTR XrResult XRAPI_CALL BestPracticesValidationLayerXrGetInstanceProcAddr(XrInstance instance, const char *name,
                                                                                 PFN_xrVoidFunction *function) {
    try {
        *function = BestPracticesLayerInnerGetInstanceProcAddr(name);

        if (*function != nullptr) {
            return XR_SUCCESS;
        }

        // We have not found it, so pass it down to the next layer/runtime
        if (!g_nextDispatch.isValid()) return XR_ERROR_HANDLE_INVALID;

        PFN_xrGetInstanceProcAddr next_gipa =
            g_nextDispatch.Get<PFN_xrGetInstanceProcAddr>(&XrGeneratedDispatchTable::GetInstanceProcAddr);

        return next_gipa(instance, name, function);
    } catch (...) {
        return XR_ERROR_VALIDATION_FAILURE;
    }
}

XRAPI_ATTR XrResult XRAPI_CALL BestPracticesXrCreateApiLayerInstance(const XrInstanceCreateInfo *info,
                                                                     const struct XrApiLayerCreateInfo *apiLayerInfo,
                                                                     XrInstance *instance) {
    try {
        XrApiLayerCreateInfo new_api_layer_info = {};

        // Validate the API layer info and next API layer info structures before we try to use them
        if (nullptr == apiLayerInfo || XR_LOADER_INTERFACE_STRUCT_API_LAYER_CREATE_INFO != apiLayerInfo->structType ||
            XR_API_LAYER_CREATE_INFO_STRUCT_VERSION > apiLayerInfo->structVersion ||
            sizeof(XrApiLayerCreateInfo) > apiLayerInfo->structSize || nullptr == apiLayerInfo->nextInfo ||
            XR_LOADER_INTERFACE_STRUCT_API_LAYER_NEXT_INFO != apiLayerInfo->nextInfo->structType ||
            XR_API_LAYER_NEXT_INFO_STRUCT_VERSION > apiLayerInfo->nextInfo->structVersion ||
            sizeof(XrApiLayerNextInfo) > apiLayerInfo->nextInfo->structSize ||
            0 != strcmp("XR_APILAYER_KHRONOS_best_practices_validation", apiLayerInfo->nextInfo->layerName) ||
            nullptr == apiLayerInfo->nextInfo->nextGetInstanceProcAddr ||
            nullptr == apiLayerInfo->nextInfo->nextCreateApiLayerInstance) {
            return XR_ERROR_INITIALIZATION_FAILED;
        }

        // Copy the contents of the layer info struct, but then move the next info up by
        // one slot so that the next layer gets information.
        memcpy(&new_api_layer_info, apiLayerInfo, sizeof(XrApiLayerCreateInfo));
        new_api_layer_info.nextInfo = apiLayerInfo->nextInfo->next;

        // Get the function pointers we need
        PFN_xrGetInstanceProcAddr next_get_instance_proc_addr = apiLayerInfo->nextInfo->nextGetInstanceProcAddr;
        PFN_xrCreateApiLayerInstance next_create_api_layer_instance = apiLayerInfo->nextInfo->nextCreateApiLayerInstance;

        // Create the instance using the layer create instance command for the next layer
        XrInstance returned_instance = *instance;
        XrResult next_result = next_create_api_layer_instance(info, &new_api_layer_info, &returned_instance);
        *instance = returned_instance;

        // Create the dispatch table to the next levels
        std::unique_ptr<XrGeneratedDispatchTable> next_dispatch = std::make_unique<XrGeneratedDispatchTable>();
        GeneratedXrPopulateDispatchTable(next_dispatch.get(), returned_instance, next_get_instance_proc_addr);

        g_nextDispatch.Reset(std::move(next_dispatch));

        g_frameIndex = 0;
        g_lastEndFramePDT = 0;

        return next_result;

    } catch (...) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }
}

// Function used to negotiate an interface betewen the loader and an API layer.  Each library exposing one or
// more API layers needs to expose at least this function.
extern "C" LAYER_EXPORT XRAPI_ATTR XrResult XRAPI_CALL xrNegotiateLoaderApiLayerInterface(
    const XrNegotiateLoaderInfo *loaderInfo, const char * /*apiLayerName*/, XrNegotiateApiLayerRequest *apiLayerRequest) {
    if (loaderInfo == nullptr || loaderInfo->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
        loaderInfo->structVersion != XR_LOADER_INFO_STRUCT_VERSION || loaderInfo->structSize != sizeof(XrNegotiateLoaderInfo)) {
        LogPlatformUtilsError("loaderInfo struct is not valid");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    if (loaderInfo->minInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
        loaderInfo->maxInterfaceVersion < XR_CURRENT_LOADER_API_LAYER_VERSION) {
        LogPlatformUtilsError("loader interface version is not in the range [minInterfaceVersion, maxInterfaceVersion]");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    if (loaderInfo->minApiVersion > XR_CURRENT_API_VERSION || loaderInfo->maxApiVersion < XR_CURRENT_API_VERSION) {
        LogPlatformUtilsError("loader api version is not in the range [minApiVersion, maxApiVersion]");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    if (apiLayerRequest == nullptr || apiLayerRequest->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_REQUEST ||
        apiLayerRequest->structVersion != XR_API_LAYER_INFO_STRUCT_VERSION ||
        apiLayerRequest->structSize != sizeof(XrNegotiateApiLayerRequest)) {
        LogPlatformUtilsError("apiLayerRequest is not valid");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    apiLayerRequest->layerInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
    apiLayerRequest->layerApiVersion = XR_CURRENT_API_VERSION;
    apiLayerRequest->getInstanceProcAddr = BestPracticesValidationLayerXrGetInstanceProcAddr;
    apiLayerRequest->createApiLayerInstance = BestPracticesXrCreateApiLayerInstance;

    return XR_SUCCESS;
}

PFN_xrVoidFunction BestPracticesLayerInnerGetInstanceProcAddr(const char *name) {
    std::string func_name = name;

    // ---- Core 1.0 commands that we intercept/override
    if (func_name == "xrGetInstanceProcAddr") {
        return reinterpret_cast<PFN_xrVoidFunction>(BestPracticesValidationLayerXrGetInstanceProcAddr);
    }
    if (func_name == "xrEndFrame") {
        return reinterpret_cast<PFN_xrVoidFunction>(BestPracticesLayerXrEndFrame);
    }
    if (func_name == "xrBeginFrame") {
        return reinterpret_cast<PFN_xrVoidFunction>(BestPracticesLayerXrBeginFrame);
    }
    if (func_name == "xrWaitFrame") {
        return reinterpret_cast<PFN_xrVoidFunction>(BestPracticesLayerXrWaitFrame);
    }
    if (func_name == "xrSyncActions") {
        return reinterpret_cast<PFN_xrVoidFunction>(BestPracticesLayerXrSyncActions);
    }
    if (func_name == "xrLocateSpace") {
        return reinterpret_cast<PFN_xrVoidFunction>(BestPracticesLayerXrLocateSpace);
    }
    if (func_name == "xrLocateViews") {
        return reinterpret_cast<PFN_xrVoidFunction>(BestPracticesLayerXrLocateViews);
    }
    return nullptr;
}
