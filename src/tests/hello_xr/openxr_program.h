// Copyright (c) 2017-2022, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#define SUPPORT_SCREENSHOTS 0

#define ENABLE_OPENXR_FB_REFRESH_RATE 0
#define DEFAULT_REFRESH_RATE 90.0f
#define DESIRED_REFRESH_RATE 90.0f

#define ENABLE_OPENXR_FB_RENDER_MODEL 0

#define ENABLE_OPENXR_FB_SHARPENING 1
#define ENABLE_OPENXR_FB_COMPOSITION_LAYER_SETTINGS ENABLE_OPENXR_FB_SHARPENING

#define ENABLE_OPENXR_FB_LOCAL_DIMMING 0

#define ENABLE_OPENXR_HAND_TRACKING 0
#define ENABLE_OPENXR_FB_EYE_TRACKING 0
#define ENABLE_OPENXR_FB_FACE_TRACKING 0
#define ENABLE_OPENXR_FB_BODY_TRACKING 0

#define USE_THUMBSTICKS_FOR_SMOOTH_LOCOMOTION 0
#define USE_WAIST_ORIENTATION_FOR_STICK_DIRECTION (ENABLE_OPENXR_FB_BODY_TRACKING && USE_THUMBSTICKS_FOR_SMOOTH_LOCOMOTION && 0)

#define USE_DUAL_LAYERS 0

#define LOG_EYE_TRACKING_DATA (ENABLE_OPENXR_FB_EYE_TRACKING && 0)
#define LOG_BODY_TRACKING_DATA (ENABLE_OPENXR_FB_BODY_TRACKING && 0)

struct IOpenXrProgram 
{
    virtual ~IOpenXrProgram() = default;

    // Create an Instance and other basic instance-level initialization.
    virtual void CreateInstance() = 0;

    // Select a System for the view configuration specified in the Options
    virtual void InitializeSystem() = 0;

    // Initialize the graphics device for the selected system.
    virtual void InitializeDevice() = 0;

    // Create a Session and other basic session-level initialization.
    virtual void InitializeSession() = 0;

    // Create a Swapchain which requires coordinating with the graphics plugin to select the format, getting the system graphics
    // properties, getting the view configuration and grabbing the resulting swapchain images.
    virtual void CreateSwapchains() = 0;

    // Process any events in the event queue.
    virtual void PollEvents(bool* exitRenderLoop, bool* requestRestart) = 0;

    // Manage session lifecycle to track if RenderFrame should be called.
    virtual bool IsSessionRunning() const = 0;

    // Manage session state to track if input should be processed.
    virtual bool IsSessionFocused() const = 0;

    // Sample input actions and generate haptic feedback.
    virtual void PollActions() = 0;

    // Create and submit a frame.
    virtual void RenderFrame() = 0;

    // Get preferred blend mode based on the view configuration specified in the Options
    virtual XrEnvironmentBlendMode GetPreferredBlendMode() const = 0;

#if SUPPORT_SCREENSHOTS
    virtual void TakeScreenShot() = 0;
#endif

#if ENABLE_OPENXR_FB_REFRESH_RATE
    // Only works on Meta HMDs (SteamVR also supports it).
    virtual const std::vector<float>& GetSupportedRefreshRates() = 0;
    virtual float GetCurrentRefreshRate() = 0;
    virtual float GetMaxRefreshRate() = 0;
    virtual bool IsRefreshRateSupported(const float refresh_rate) = 0;
    virtual void SetRefreshRate(const float refresh_rate) = 0; // 0.0 = system default
#endif

#if ENABLE_OPENXR_FB_SHARPENING
    virtual bool IsSharpeningEnabled() const = 0;
    virtual void SetSharpeningEnabled(const bool enabled) = 0;
#endif

#if ENABLE_OPENXR_FB_LOCAL_DIMMING
	virtual bool IsLocalDimmingEnabled() const = 0;
	virtual void SetLocalDimmingEnabled(const bool enabled) = 0;
#endif

#if ENABLE_OPENXR_FB_EYE_TRACKING
	virtual bool GetGazePose(const int eye, XrPosef& gaze_pose) = 0;
#endif
};

struct Swapchain {
    XrSwapchain handle;
    int32_t width;
    int32_t height;
};

std::shared_ptr<IOpenXrProgram> CreateOpenXrProgram(const std::shared_ptr<Options>& options,
                                                    const std::shared_ptr<IPlatformPlugin>& platformPlugin,
                                                    const std::shared_ptr<IGraphicsPlugin>& graphicsPlugin);
