// Copyright (c) 2017-2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

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

#if ENABLE_OPENXR_FB_EYE_TRACKING_SOCIAL
    virtual bool GetGazePoseSocial(const int eye, XrPosef& gaze_pose) = 0;
    virtual void SetSocialEyeTrackerEnabled(const bool enabled) = 0;
    virtual void UpdateSocialEyeTrackerGazes() = 0;
#endif

#if ENABLE_EXT_EYE_TRACKING
	virtual bool GetGazePoseEXT(XrPosef& gaze_pose) = 0;
    virtual void SetEXTEyeTrackerEnabled(const bool enabled) = 0;
    virtual void UpdateEXTEyeTrackerGaze(XrTime predicted_display_time) = 0;
#endif

#if ENABLE_EYE_TRACKING && 0
	virtual bool GetGazePose(const int eye, XrPosef& gaze_pose, const bool prefer_ext) = 0;
    virtual void UpdateEyeTrackers(XrTime predicted_display_time) = 0;
#endif

#if ENABLE_OPENXR_FB_SIMULTANEOUS_HANDS_AND_CONTROLLERS
    virtual bool AreSimultaneousHandsAndControllersSupported() const = 0;
    virtual bool AreSimultaneousHandsAndControllersEnabled() const = 0;
    virtual void SetSimultaneousHandsAndControllersEnabled(const bool enabled) = 0;
#endif

#if USE_SDL_JOYSTICKS
    virtual void InitSDLJoysticks() = 0;
    virtual void ShutdownSDLJoySticks() = 0;
    virtual void UpdateSDLJoysticks() = 0;
#endif
};

struct Swapchain {
    XrSwapchain handle;
    int32_t width;
    int32_t height;
};


#if ENABLE_QUAD_LAYER
struct QuadLayer {

	XrCompositionLayerQuad xr_quad_layer_{ XR_TYPE_COMPOSITION_LAYER_QUAD };
	XrCompositionLayerBaseHeader* header_ = {};

	uint32_t width_ = 0;
	uint32_t height_ = 0;
	int64_t format_ = -1;

	XrSwapchain quad_swapchain_{};
	std::vector<XrSwapchainImageBaseHeader*> quad_images_;

	bool initialized_ = false;

    bool init(const uint32_t width, const uint32_t height, const int64_t format, std::shared_ptr<IGraphicsPlugin> plugin, XrSession session, XrSpace space);
    void shutdown();
};
#endif


std::shared_ptr<IOpenXrProgram> CreateOpenXrProgram(const std::shared_ptr<Options>& options,
                                                    const std::shared_ptr<IPlatformPlugin>& platformPlugin,
                                                    const std::shared_ptr<IGraphicsPlugin>& graphicsPlugin);
