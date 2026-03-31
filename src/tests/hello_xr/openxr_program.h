// Copyright (c) 2017-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

struct IOpenXrProgram {
    virtual ~IOpenXrProgram() = default;

    // Create an Instance and other basic instance-level initialization.
    virtual void CreateInstance() = 0;

    // Select a System for the view configuration specified in the Options
    virtual void InitializeSystem(XrFormFactor formFactor, XrViewConfigurationType viewConfigType, bool ebmOverride,
                                  XrEnvironmentBlendMode ebm) = 0;

    // Initialize the graphics device for the selected system.
    virtual void InitializeDevice() = 0;

    // Create a Session and other basic session-level initialization.
    virtual void InitializeSession(std::string appSpace) = 0;

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

    // Query the appropriate clear color based on the environment (blend mode)
    virtual std::array<float, 4> GetBackgroundClearColor() const = 0;
};

struct Swapchain {
    XrSwapchain handle;
    int32_t width;
    int32_t height;
};

std::shared_ptr<IOpenXrProgram> CreateOpenXrProgram(const std::shared_ptr<IPlatformPlugin>& platformPlugin,
                                                    const std::shared_ptr<IGraphicsPlugin>& graphicsPlugin);
