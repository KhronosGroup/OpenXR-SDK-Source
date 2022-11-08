// Copyright (c) 2017-2022, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "common.h"
#include "options.h"
#include "platformdata.h"
#include "platformplugin.h"
#include "graphicsplugin.h"
#include "openxr_program.h"
#include <common/xr_linear.h>
#include <array>
#include <cmath>
#include <set>

#define ADD_GRIP_POSE 1
#define DRAW_GRIP_POSE (ADD_GRIP_POSE && !ENABLE_OPENXR_FB_BODY_TRACKING && 1)

#define ADD_AIM_POSE 1
#define DRAW_AIM_POSE (ADD_AIM_POSE && !ENABLE_OPENXR_FB_BODY_TRACKING && 1)

#define ADD_EXTRA_CUBES 1
#define TAKE_SCREENSHOT_WITH_LEFT_GRAB (SUPPORT_SCREENSHOTS && 0)
#define ENABLE_LOCAL_DIMMING_WITH_RIGHT_GRAB (ENABLE_OPENXR_FB_LOCAL_DIMMING && 1)
#define LOG_MATRICES 0

#define BODY_CUBE_SIZE 0.02f

#ifndef XR_LOAD
#define XR_LOAD(instance, fn) CHECK_XRCMD(xrGetInstanceProcAddr(instance, #fn, reinterpret_cast<PFN_xrVoidFunction*>(&fn)))
#endif

#if ENABLE_OPENXR_FB_REFRESH_RATE
bool supports_refresh_rate_ = false;
#endif

#if ENABLE_OPENXR_FB_RENDER_MODEL
bool supports_render_model_ = false;
#endif

#if ENABLE_OPENXR_FB_COMPOSITION_LAYER_SETTINGS
bool supports_composition_layer_ = false;
#endif

#if ENABLE_OPENXR_FB_LOCAL_DIMMING
#include <openxr/meta_local_dimming.h>
bool supports_local_dimming_ = false;
XrLocalDimmingFrameEndInfoMETA local_dimming_settings_ = { (XrStructureType)1000216000, nullptr, XR_LOCAL_DIMMING_MODE_ON_META };
#endif

#if ENABLE_OPENXR_HAND_TRACKING
bool supports_hand_tracking_ = false;
#endif

#if ENABLE_OPENXR_FB_EYE_TRACKING_SOCIAL
#include <openxr/fb_eye_tracking_social.h>
bool supports_eye_tracking_social_ = false;
#endif

#if ENABLE_OPENXR_FB_FACE_TRACKING
#include <openxr/fb_face_tracking.h>
bool supports_face_tracking_ = false;
#endif

#if ENABLE_OPENXR_FB_BODY_TRACKING
//#define XR_FB_body_tracking_EXPERIMENTAL_VERSION 2
#include <openxr/fb_body_tracking.h>
bool supports_body_tracking_ = false;
#endif

#if USE_THUMBSTICKS_FOR_SMOOTH_LOCOMOTION
GLMPose player_pose;
const float movement_speed = 1.0f;
const float rotation_speed = 1.0f;

void move_player(const glm::vec2& left_thumbstick_values)
{
    // Move player in the direction they are facing currently
    glm::vec3 position_increment_local{ left_thumbstick_values.x, 0.0f, left_thumbstick_values.y };
    glm::vec3 position_increment_world = player_pose.rotation_ * position_increment_local;

    player_pose.translation_ += position_increment_world * movement_speed;
}

void rotate_player(const float right_thumbstick_x_value)
{
    // Rotate player about +Y (UP) axis
    const float rotation_degrees = right_thumbstick_x_value * rotation_speed;
    player_pose.euler_angles_degrees_.y = fmodf(player_pose.euler_angles_degrees_.y + rotation_degrees, TWO_PI);
}
#endif


namespace {

#if !defined(XR_USE_PLATFORM_WIN32)
#define strcpy_s(dest, source) strncpy((dest), (source), sizeof(dest))
#endif

namespace Side {
const int LEFT = 0;
const int RIGHT = 1;
const int COUNT = 2;
}  // namespace Side

inline std::string GetXrVersionString(XrVersion ver) {
    return Fmt("%d.%d.%d", XR_VERSION_MAJOR(ver), XR_VERSION_MINOR(ver), XR_VERSION_PATCH(ver));
}

namespace Math {
namespace Pose {
XrPosef Identity() {
    XrPosef t{};
    t.orientation.w = 1;
    return t;
}

XrPosef Translation(const XrVector3f& translation) {
    XrPosef t = Identity();
    t.position = translation;
    return t;
}

XrPosef RotateCCWAboutYAxis(float radians, XrVector3f translation) {
    XrPosef t = Identity();
    t.orientation.x = 0.f;
    t.orientation.y = std::sin(radians * 0.5f);
    t.orientation.z = 0.f;
    t.orientation.w = std::cos(radians * 0.5f);
    t.position = translation;
    return t;
}
}  // namespace Pose
}  // namespace Math

inline XrReferenceSpaceCreateInfo GetXrReferenceSpaceCreateInfo(const std::string& referenceSpaceTypeStr) {
    XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::Identity();
    if (EqualsIgnoreCase(referenceSpaceTypeStr, "View")) {
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "ViewFront")) {
        // Render head-locked 2m in front of device.
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::Translation({0.f, 0.f, -2.f}),
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "Local")) {
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "Stage")) {
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageLeft")) {
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(0.f, {-2.f, 0.f, -2.f});
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageRight")) {
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(0.f, {2.f, 0.f, -2.f});
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageLeftRotated")) {
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(3.14f / 3.f, {-2.f, 0.5f, -2.f});
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageRightRotated")) {
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(-3.14f / 3.f, {2.f, 0.5f, -2.f});
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else {
        throw std::invalid_argument(Fmt("Unknown reference space type '%s'", referenceSpaceTypeStr.c_str()));
    }
    return referenceSpaceCreateInfo;
}

struct OpenXrProgram : IOpenXrProgram 
{
    OpenXrProgram(const std::shared_ptr<Options>& options, const std::shared_ptr<IPlatformPlugin>& platformPlugin,
                  const std::shared_ptr<IGraphicsPlugin>& graphicsPlugin)
        : m_options(options),
          m_platformPlugin(platformPlugin),
          m_graphicsPlugin(graphicsPlugin),
          m_acceptableBlendModes{XR_ENVIRONMENT_BLEND_MODE_OPAQUE, XR_ENVIRONMENT_BLEND_MODE_ADDITIVE,
                                 XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND} {}

    ~OpenXrProgram() override 
    {
#if ENABLE_OPENXR_FB_BODY_TRACKING
		DestroyBodyTracker();
#endif

#if ENABLE_OPENXR_FB_EYE_TRACKING_SOCIAL
		DestroyEyeTracker();
#endif

        if (m_input.actionSet != XR_NULL_HANDLE) 
        {
            for (auto hand : {Side::LEFT, Side::RIGHT}) 
            {
                xrDestroySpace(m_input.handSpace[hand]);

#if ADD_AIM_POSE
                xrDestroySpace(m_input.aimSpace[hand]);
#endif
            }
            xrDestroyActionSet(m_input.actionSet);
        }

        for (Swapchain swapchain : m_swapchains) 
        {
            xrDestroySwapchain(swapchain.handle);
        }

        for (XrSpace visualizedSpace : m_visualizedSpaces) 
        {
            xrDestroySpace(visualizedSpace);
        }

        if (m_appSpace != XR_NULL_HANDLE) 
        {
            xrDestroySpace(m_appSpace);
        }

        if (m_session != XR_NULL_HANDLE) 
        {
            xrDestroySession(m_session);
        }

        if (m_instance != XR_NULL_HANDLE) 
        {
            xrDestroyInstance(m_instance);
        }
    }

    static void LogLayersAndExtensions() 
    {
        // Write out extension properties for a given layer.
        const auto logExtensions = [](const char* layerName, int indent = 0) 
        {
            uint32_t instanceExtensionCount;
            CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(layerName, 0, &instanceExtensionCount, nullptr));

            std::vector<XrExtensionProperties> extensions(instanceExtensionCount);

            for (XrExtensionProperties& extension : extensions) 
            {
                extension.type = XR_TYPE_EXTENSION_PROPERTIES;
            }

            CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(layerName, (uint32_t)extensions.size(), &instanceExtensionCount, extensions.data()));

            const std::string indentStr(indent, ' ');
            Log::Write(Log::Level::Verbose, Fmt("%sAvailable Extensions: (%d)", indentStr.c_str(), instanceExtensionCount));

            for (const XrExtensionProperties& extension : extensions) 
            {
                Log::Write(Log::Level::Verbose, Fmt("%s  Name=%s SpecVersion=%d", indentStr.c_str(), extension.extensionName,
                                                    extension.extensionVersion));

#if ENABLE_OPENXR_FB_REFRESH_RATE
				if (!strcmp(extension.extensionName, XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME))
				{
                    Log::Write(Log::Level::Info, "FB OPENXR XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME - DETECTED");
					supports_refresh_rate_ = true;
				}
#endif

#if ENABLE_OPENXR_FB_RENDER_MODEL
				if (!strcmp(extension.extensionName, XR_FB_RENDER_MODEL_EXTENSION_NAME))
				{
                    Log::Write(Log::Level::Info, "FB OPENXR XR_FB_RENDER_MODEL_EXTENSION_NAME - DETECTED");
					supports_render_model_ = true;
				}
#endif

#if ENABLE_OPENXR_FB_COMPOSITION_LAYER_SETTINGS
				if (!strcmp(extension.extensionName, XR_FB_COMPOSITION_LAYER_SETTINGS_EXTENSION_NAME))
				{
                    Log::Write(Log::Level::Info, "FB OPENXR XR_FB_COMPOSITION_LAYER_SETTINGS_EXTENSION_NAME - DETECTED");
					supports_composition_layer_ = true;
				}
#endif

#if ENABLE_OPENXR_FB_LOCAL_DIMMING
				if (!strcmp(extension.extensionName, XR_META_LOCAL_DIMMING_EXTENSION_NAME))
				{
                    Log::Write(Log::Level::Info, "FB OPENXR XR_META_LOCAL_DIMMING_EXTENSION_NAME - DETECTED");
					supports_local_dimming_ = true;
				}
#endif

#if ENABLE_OPENXR_HAND_TRACKING
				if (!strcmp(extension.extensionName, XR_EXT_HAND_TRACKING_EXTENSION_NAME))
				{
					Log::Write(Log::Level::Info, "FB OPENXR XR_EXT_HAND_TRACKING_EXTENSION_NAME - DETECTED");
                    supports_hand_tracking_ = true;
				}
#endif

#if ENABLE_OPENXR_FB_EYE_TRACKING_SOCIAL
				if (!strcmp(extension.extensionName, XR_FB_EYE_TRACKING_SOCIAL_EXTENSION_NAME))
				{
					Log::Write(Log::Level::Info, "FB OPENXR XR_FB_EYE_TRACKING_SOCIAL_EXTENSION_NAME - DETECTED");
					supports_eye_tracking_social_ = true;
				}
#endif

#if ENABLE_OPENXR_FB_FACE_TRACKING
				if (!strcmp(extension.extensionName, XR_FB_FACE_TRACKING_EXTENSION_NAME))
				{
					Log::Write(Log::Level::Info, "FB OPENXR XR_FB_FACE_TRACKING_EXTENSION_NAME - DETECTED");
                    supports_face_tracking_ = true;
				}
#endif

#if ENABLE_OPENXR_FB_BODY_TRACKING
				if (!strcmp(extension.extensionName, XR_FB_BODY_TRACKING_EXTENSION_NAME))
				{
					Log::Write(Log::Level::Info, "FB OPENXR XR_FB_BODY_TRACKING_EXTENSION_NAME - DETECTED");
                    supports_body_tracking_ = true;
				}
#endif
            }

            return;
        };

        // Log non-layer extensions (layerName==nullptr).
        logExtensions(nullptr);

        // Log layers and any of their extensions.
        {
            uint32_t layerCount = 0;
            CHECK_XRCMD(xrEnumerateApiLayerProperties(0, &layerCount, nullptr));

            std::vector<XrApiLayerProperties> layers(layerCount);
            for (XrApiLayerProperties& layer : layers) {
                layer.type = XR_TYPE_API_LAYER_PROPERTIES;
            }

            CHECK_XRCMD(xrEnumerateApiLayerProperties((uint32_t)layers.size(), &layerCount, layers.data()));

            Log::Write(Log::Level::Info, Fmt("Available Layers: (%d)", layerCount));
            for (const XrApiLayerProperties& layer : layers) {
                Log::Write(Log::Level::Verbose,
                           Fmt("  Name=%s SpecVersion=%s LayerVersion=%d Description=%s", layer.layerName,
                               GetXrVersionString(layer.specVersion).c_str(), layer.layerVersion, layer.description));
                logExtensions(layer.layerName, 4);
            }
        }
    }

    void LogInstanceInfo() {
        CHECK(m_instance != XR_NULL_HANDLE);

        XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
        CHECK_XRCMD(xrGetInstanceProperties(m_instance, &instanceProperties));

        Log::Write(Log::Level::Info, Fmt("Instance RuntimeName=%s RuntimeVersion=%s", instanceProperties.runtimeName,
                                         GetXrVersionString(instanceProperties.runtimeVersion).c_str()));
    }

    void CreateInstanceInternal() 
    {
        CHECK(m_instance == XR_NULL_HANDLE);

        // Create union of extensions required by platform and graphics plugins.
        std::vector<const char*> extensions;

        // Transform platform and graphics extension std::strings to C strings.
        const std::vector<std::string> platformExtensions = m_platformPlugin->GetInstanceExtensions();

        std::transform(platformExtensions.begin(), platformExtensions.end(), std::back_inserter(extensions),
                       [](const std::string& ext) { return ext.c_str(); });

        const std::vector<std::string> graphicsExtensions = m_graphicsPlugin->GetInstanceExtensions();

        std::transform(graphicsExtensions.begin(), graphicsExtensions.end(), std::back_inserter(extensions),
                       [](const std::string& ext) { return ext.c_str(); });

#if ENABLE_OPENXR_FB_REFRESH_RATE
        if (supports_refresh_rate_)
        {
            extensions.push_back(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);
        }
#endif

#if ENABLE_OPENXR_FB_COMPOSITION_LAYER_SETTINGS
        if (supports_composition_layer_)
        {
            extensions.push_back(XR_FB_COMPOSITION_LAYER_SETTINGS_EXTENSION_NAME);

			composition_layer_settings_.next = nullptr;
			composition_layer_settings_.layerFlags = 0;

#if ENABLE_OPENXR_FB_SHARPENING
            SetSharpeningEnabled(true);
#endif

        }
#endif

#if ENABLE_OPENXR_FB_LOCAL_DIMMING
        if (supports_local_dimming_)
        {
            extensions.push_back(XR_META_LOCAL_DIMMING_EXTENSION_NAME);

            SetLocalDimmingEnabled(true);
        }
#endif

#if ENABLE_OPENXR_HAND_TRACKING
		if (supports_hand_tracking_)
		{
			Log::Write(Log::Level::Info, "Hand Tracking is supported");
			extensions.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
		}
		else
		{
			Log::Write(Log::Level::Info, "Hand Tracking is NOT supported");
		}
#endif

#if ENABLE_OPENXR_FB_EYE_TRACKING_SOCIAL
		if (supports_eye_tracking_social_)
		{
			Log::Write(Log::Level::Info, "Eye Tracking is supported (Quest Pro)");
			extensions.push_back(XR_FB_EYE_TRACKING_SOCIAL_EXTENSION_NAME);
		}
		else
		{
			Log::Write(Log::Level::Info, "Eye Tracking is NOT supported");
		}
#endif

#if ENABLE_OPENXR_FB_FACE_TRACKING
		if (supports_face_tracking_)
		{
			Log::Write(Log::Level::Info, "Face Tracking is supported");
			extensions.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
		}
		else
		{
			Log::Write(Log::Level::Info, "Face Tracking is NOT supported");
		}
#endif

#if ENABLE_OPENXR_FB_BODY_TRACKING
		if (supports_body_tracking_)
		{
			Log::Write(Log::Level::Info, "Body Tracking is supported");
			extensions.push_back(XR_FB_BODY_TRACKING_EXTENSION_NAME);
		}
		else
		{
			Log::Write(Log::Level::Info, "Body Tracking is NOT supported");
		}
#endif

        XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
        createInfo.next = m_platformPlugin->GetInstanceCreateExtension();
        createInfo.enabledExtensionCount = (uint32_t)extensions.size();
        createInfo.enabledExtensionNames = extensions.data();

        strcpy(createInfo.applicationInfo.applicationName, "HelloXR");
        createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

        CHECK_XRCMD(xrCreateInstance(&createInfo, &m_instance));
    }

    void CreateInstance() override 
    {
        LogLayersAndExtensions();
        CreateInstanceInternal();
        LogInstanceInfo();
    }

    void LogViewConfigurations() 
    {
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_systemId != XR_NULL_SYSTEM_ID);

        uint32_t viewConfigTypeCount = 0;
        CHECK_XRCMD(xrEnumerateViewConfigurations(m_instance, m_systemId, 0, &viewConfigTypeCount, nullptr));
        
        std::vector<XrViewConfigurationType> viewConfigTypes(viewConfigTypeCount);
        CHECK_XRCMD(xrEnumerateViewConfigurations(m_instance, m_systemId, viewConfigTypeCount, &viewConfigTypeCount,
                                                  viewConfigTypes.data()));
        CHECK((uint32_t)viewConfigTypes.size() == viewConfigTypeCount);

        Log::Write(Log::Level::Info, Fmt("Available View Configuration Types: (%d)", viewConfigTypeCount));

        for (XrViewConfigurationType viewConfigType : viewConfigTypes) 
        {
            Log::Write(Log::Level::Verbose, Fmt("  View Configuration Type: %s %s", to_string(viewConfigType),
                                                viewConfigType == m_options->Parsed.ViewConfigType ? "(Selected)" : ""));

            XrViewConfigurationProperties viewConfigProperties{XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
            CHECK_XRCMD(xrGetViewConfigurationProperties(m_instance, m_systemId, viewConfigType, &viewConfigProperties));

            Log::Write(Log::Level::Verbose,
                       Fmt("  View configuration FovMutable=%s", viewConfigProperties.fovMutable == XR_TRUE ? "True" : "False"));

            uint32_t viewCount;
            CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, viewConfigType, 0, &viewCount, nullptr));

            if (viewCount > 0) 
            {
                std::vector<XrViewConfigurationView> views(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});

                CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, viewConfigType, viewCount, &viewCount, views.data()));

                for (uint32_t i = 0; i < views.size(); i++) 
                {
                    const XrViewConfigurationView& view = views[i];

                    Log::Write(Log::Level::Verbose, Fmt("    View [%d]: Recommended Width=%d Height=%d SampleCount=%d", i,
                                                        view.recommendedImageRectWidth, view.recommendedImageRectHeight,
                                                        view.recommendedSwapchainSampleCount));
                    Log::Write(Log::Level::Verbose,
                               Fmt("    View [%d]:     Maximum Width=%d Height=%d SampleCount=%d", i, view.maxImageRectWidth,
                                   view.maxImageRectHeight, view.maxSwapchainSampleCount));
                }
            } else {
                Log::Write(Log::Level::Error, Fmt("Empty view configuration type"));
            }

            LogEnvironmentBlendMode(viewConfigType);
        }
    }

    void LogEnvironmentBlendMode(XrViewConfigurationType type) {
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_systemId != 0);

        uint32_t count;
        CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(m_instance, m_systemId, type, 0, &count, nullptr));
        CHECK(count > 0);

        Log::Write(Log::Level::Info, Fmt("Available Environment Blend Mode count : (%d)", count));

        std::vector<XrEnvironmentBlendMode> blendModes(count);
        CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(m_instance, m_systemId, type, count, &count, blendModes.data()));

        bool blendModeFound = false;
        for (XrEnvironmentBlendMode mode : blendModes) {
            const bool blendModeMatch = (mode == m_options->Parsed.EnvironmentBlendMode);
            Log::Write(Log::Level::Info,
                       Fmt("Environment Blend Mode (%s) : %s", to_string(mode), blendModeMatch ? "(Selected)" : ""));
            blendModeFound |= blendModeMatch;
        }
        CHECK(blendModeFound);
    }

    XrEnvironmentBlendMode GetPreferredBlendMode() const override {
        uint32_t count;
        CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(m_instance, m_systemId, m_options->Parsed.ViewConfigType, 0, &count, nullptr));
        CHECK(count > 0);

        std::vector<XrEnvironmentBlendMode> blendModes(count);
        CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(m_instance, m_systemId, m_options->Parsed.ViewConfigType, count, &count,
                                                     blendModes.data()));
        for (const auto& blendMode : blendModes) {
            if (m_acceptableBlendModes.count(blendMode)) return blendMode;
        }
        THROW("No acceptable blend mode returned from the xrEnumerateEnvironmentBlendModes");
    }

    void InitializeSystem() override {
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_systemId == XR_NULL_SYSTEM_ID);

        XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
        systemInfo.formFactor = m_options->Parsed.FormFactor;
        CHECK_XRCMD(xrGetSystem(m_instance, &systemInfo, &m_systemId));

        Log::Write(Log::Level::Verbose,
                   Fmt("Using system %d for form factor %s", m_systemId, to_string(m_options->Parsed.FormFactor)));
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_systemId != XR_NULL_SYSTEM_ID);
    }

    void InitializeDevice() override {
        LogViewConfigurations();

        // The graphics API can initialize the graphics device now that the systemId and instance
        // handle are available.
        m_graphicsPlugin->InitializeDevice(m_instance, m_systemId);
    }

    void LogReferenceSpaces() {
        CHECK(m_session != XR_NULL_HANDLE);

        uint32_t spaceCount;
        CHECK_XRCMD(xrEnumerateReferenceSpaces(m_session, 0, &spaceCount, nullptr));
        std::vector<XrReferenceSpaceType> spaces(spaceCount);
        CHECK_XRCMD(xrEnumerateReferenceSpaces(m_session, spaceCount, &spaceCount, spaces.data()));

        Log::Write(Log::Level::Info, Fmt("Available reference spaces: %d", spaceCount));
        for (XrReferenceSpaceType space : spaces) {
            Log::Write(Log::Level::Verbose, Fmt("  Name: %s", to_string(space)));
        }
    }

    struct InputState {
        XrActionSet actionSet{XR_NULL_HANDLE};
        XrAction grabAction{XR_NULL_HANDLE};
        XrAction poseAction{XR_NULL_HANDLE};
        XrAction vibrateAction{XR_NULL_HANDLE};
        XrAction quitAction{XR_NULL_HANDLE};
        std::array<XrPath, Side::COUNT> handSubactionPath;
        std::array<XrSpace, Side::COUNT> handSpace;
        std::array<float, Side::COUNT> handScale = {{1.0f, 1.0f}};
        std::array<XrBool32, Side::COUNT> handActive;

#if ADD_AIM_POSE
        XrAction aimPoseAction{XR_NULL_HANDLE};
        std::array<XrPath, Side::COUNT> aimSubactionPath;
        std::array<XrSpace, Side::COUNT> aimSpace;
#endif

#if USE_THUMBSTICKS_FOR_SMOOTH_LOCOMOTION
		XrAction thumbstickXAction;
		XrAction thumbstickYAction;
#endif
    };

    void InitializeActions() {
        // Create an action set.
        {
            XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy_s(actionSetInfo.actionSetName, "gameplay");
            strcpy_s(actionSetInfo.localizedActionSetName, "Gameplay");
            actionSetInfo.priority = 0;
            CHECK_XRCMD(xrCreateActionSet(m_instance, &actionSetInfo, &m_input.actionSet));
        }

        // Get the XrPath for the left and right hands - we will use them as subaction paths.
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left", &m_input.handSubactionPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right", &m_input.handSubactionPath[Side::RIGHT]));

        // Create actions.
        {
            // Create an input action for grabbing objects with the left and right hands.
            XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
            actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
            strcpy_s(actionInfo.actionName, "grab_object");
            strcpy_s(actionInfo.localizedActionName, "Grab Object");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.grabAction));

            // Create an input action getting the left and right hand poses.
            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy_s(actionInfo.actionName, "hand_pose");
            strcpy_s(actionInfo.localizedActionName, "Hand Pose");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.poseAction));
                        
#if ADD_AIM_POSE
            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy_s(actionInfo.actionName, "aim_pose");
            strcpy_s(actionInfo.localizedActionName, "Aim Pose");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.aimPoseAction));
#endif

#if USE_THUMBSTICKS_FOR_SMOOTH_LOCOMOTION
            actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
			strcpy_s(actionInfo.actionName, "thumbstick_x");
			strcpy_s(actionInfo.localizedActionName, "Thumbstick X");
			CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.thumbstickXAction));

            actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
			strcpy_s(actionInfo.actionName, "thumbstick_y");
			strcpy_s(actionInfo.localizedActionName, "Thumbstick Y");
			CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.thumbstickYAction));
#endif

            // Create output actions for vibrating the left and right controller.
            actionInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
            strcpy_s(actionInfo.actionName, "vibrate_hand");
            strcpy_s(actionInfo.localizedActionName, "Vibrate Hand");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.vibrateAction));

            // Create input actions for quitting the session using the left and right controller.
            // Since it doesn't matter which hand did this, we do not specify subaction paths for it.
            // We will just suggest bindings for both hands, where possible.
            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "quit_session");
            strcpy_s(actionInfo.localizedActionName, "Quit Session");
            actionInfo.countSubactionPaths = 0;
            actionInfo.subactionPaths = nullptr;
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.quitAction));
        }

        std::array<XrPath, Side::COUNT> selectPath;
        std::array<XrPath, Side::COUNT> squeezeValuePath;
        std::array<XrPath, Side::COUNT> squeezeForcePath;
        std::array<XrPath, Side::COUNT> squeezeClickPath;
        std::array<XrPath, Side::COUNT> posePath;
#if ADD_AIM_POSE
        std::array<XrPath, Side::COUNT> aimPath;
#endif
#if USE_THUMBSTICKS_FOR_SMOOTH_LOCOMOTION
		std::array<XrPath, Side::COUNT> stickXPath;
        std::array<XrPath, Side::COUNT> stickYPath;
#endif
        std::array<XrPath, Side::COUNT> hapticPath;
        std::array<XrPath, Side::COUNT> menuClickPath;
        std::array<XrPath, Side::COUNT> bClickPath;
        std::array<XrPath, Side::COUNT> triggerValuePath;
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/select/click", &selectPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/select/click", &selectPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/squeeze/value", &squeezeValuePath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/squeeze/value", &squeezeValuePath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/squeeze/force", &squeezeForcePath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/squeeze/force", &squeezeForcePath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/squeeze/click", &squeezeClickPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/squeeze/click", &squeezeClickPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/grip/pose", &posePath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/grip/pose", &posePath[Side::RIGHT]));
#if ADD_AIM_POSE
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/aim/pose", &aimPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/aim/pose", &aimPath[Side::RIGHT]));
#endif
#if USE_THUMBSTICKS_FOR_SMOOTH_LOCOMOTION
		CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/thumbstick/x", &stickXPath[Side::LEFT]));
		CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/thumbstick/x", &stickXPath[Side::RIGHT]));

		CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/thumbstick/y", &stickYPath[Side::LEFT]));
		CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/thumbstick/y", &stickYPath[Side::RIGHT]));
#endif
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/output/haptic", &hapticPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/output/haptic", &hapticPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/menu/click", &menuClickPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/menu/click", &menuClickPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/b/click", &bClickPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/b/click", &bClickPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/trigger/value", &triggerValuePath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/trigger/value", &triggerValuePath[Side::RIGHT]));
        // Suggest bindings for KHR Simple.
        {
            XrPath khrSimpleInteractionProfilePath;
            CHECK_XRCMD(xrStringToPath(m_instance, "/interaction_profiles/khr/simple_controller", &khrSimpleInteractionProfilePath));

            std::vector<XrActionSuggestedBinding> bindings{{// Fall back to a click input for the grab action.
                                                            {m_input.grabAction, selectPath[Side::LEFT]},
                                                            {m_input.grabAction, selectPath[Side::RIGHT]},
                                                            {m_input.poseAction, posePath[Side::LEFT]},
                                                            {m_input.poseAction, posePath[Side::RIGHT]},
#if ADD_AIM_POSE
                                                            {m_input.aimPoseAction, aimPath[Side::LEFT]},
                                                            {m_input.aimPoseAction, aimPath[Side::RIGHT]},
#endif
#if USE_THUMBSTICKS_FOR_SMOOTH_LOCOMOTION
															{m_input.thumbstickXAction, stickXPath[Side::LEFT]},
															{m_input.thumbstickXAction, stickXPath[Side::RIGHT]},
															{m_input.thumbstickYAction, stickYPath[Side::LEFT]},
															{m_input.thumbstickYAction, stickYPath[Side::RIGHT]},

#endif
                                                            {m_input.quitAction, menuClickPath[Side::LEFT]},
                                                            {m_input.quitAction, menuClickPath[Side::RIGHT]},
                                                            {m_input.vibrateAction, hapticPath[Side::LEFT]},
                                                            {m_input.vibrateAction, hapticPath[Side::RIGHT]}}};

            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedBindings.interactionProfile = khrSimpleInteractionProfilePath;
            suggestedBindings.suggestedBindings = bindings.data();
            suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
            CHECK_XRCMD(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
        }
        // Suggest bindings for the Oculus Touch.
        {
            XrPath oculusTouchInteractionProfilePath;
            CHECK_XRCMD(xrStringToPath(m_instance, "/interaction_profiles/oculus/touch_controller", &oculusTouchInteractionProfilePath));

            std::vector<XrActionSuggestedBinding> bindings{{{m_input.grabAction, squeezeValuePath[Side::LEFT]},
                                                            {m_input.grabAction, squeezeValuePath[Side::RIGHT]},
                                                            {m_input.poseAction, posePath[Side::LEFT]},
                                                            {m_input.poseAction, posePath[Side::RIGHT]},
#if ADD_AIM_POSE
                                                            {m_input.aimPoseAction, aimPath[Side::LEFT]},
                                                            {m_input.aimPoseAction, aimPath[Side::RIGHT]},
#endif
#if USE_THUMBSTICKS_FOR_SMOOTH_LOCOMOTION
															{m_input.thumbstickXAction, stickXPath[Side::LEFT]},
															{m_input.thumbstickXAction, stickXPath[Side::RIGHT]},
															{m_input.thumbstickYAction, stickYPath[Side::LEFT]},
															{m_input.thumbstickYAction, stickYPath[Side::RIGHT]},

#endif
                                                            {m_input.quitAction, menuClickPath[Side::LEFT]},
                                                            {m_input.vibrateAction, hapticPath[Side::LEFT]},
                                                            {m_input.vibrateAction, hapticPath[Side::RIGHT]}}};

            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedBindings.interactionProfile = oculusTouchInteractionProfilePath;
            suggestedBindings.suggestedBindings = bindings.data();
            suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
            CHECK_XRCMD(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
        }
        // Suggest bindings for the Vive Controller.
        {
            XrPath viveControllerInteractionProfilePath;
            CHECK_XRCMD(xrStringToPath(m_instance, "/interaction_profiles/htc/vive_controller", &viveControllerInteractionProfilePath));

            std::vector<XrActionSuggestedBinding> bindings{{{m_input.grabAction, triggerValuePath[Side::LEFT]},
                                                            {m_input.grabAction, triggerValuePath[Side::RIGHT]},
                                                            {m_input.poseAction, posePath[Side::LEFT]},
                                                            {m_input.poseAction, posePath[Side::RIGHT]},
                                                            {m_input.quitAction, menuClickPath[Side::LEFT]},
                                                            {m_input.quitAction, menuClickPath[Side::RIGHT]},
                                                            {m_input.vibrateAction, hapticPath[Side::LEFT]},
                                                            {m_input.vibrateAction, hapticPath[Side::RIGHT]}}};
            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedBindings.interactionProfile = viveControllerInteractionProfilePath;
            suggestedBindings.suggestedBindings = bindings.data();
            suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
            CHECK_XRCMD(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
        }

        // Suggest bindings for the Valve Index Controller.
        {
            XrPath indexControllerInteractionProfilePath;
            CHECK_XRCMD(xrStringToPath(m_instance, "/interaction_profiles/valve/index_controller", &indexControllerInteractionProfilePath));

            std::vector<XrActionSuggestedBinding> bindings{{{m_input.grabAction, squeezeForcePath[Side::LEFT]},
                                                            {m_input.grabAction, squeezeForcePath[Side::RIGHT]},
                                                            {m_input.poseAction, posePath[Side::LEFT]},
                                                            {m_input.poseAction, posePath[Side::RIGHT]},
                                                            {m_input.quitAction, bClickPath[Side::LEFT]},
                                                            {m_input.quitAction, bClickPath[Side::RIGHT]},
                                                            {m_input.vibrateAction, hapticPath[Side::LEFT]},
                                                            {m_input.vibrateAction, hapticPath[Side::RIGHT]}}};

            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedBindings.interactionProfile = indexControllerInteractionProfilePath;
            suggestedBindings.suggestedBindings = bindings.data();
            suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
            CHECK_XRCMD(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
        }

        // Suggest bindings for the Microsoft Mixed Reality Motion Controller.
        {
            XrPath microsoftMixedRealityInteractionProfilePath;
            CHECK_XRCMD(xrStringToPath(m_instance, "/interaction_profiles/microsoft/motion_controller", &microsoftMixedRealityInteractionProfilePath));

            std::vector<XrActionSuggestedBinding> bindings{{{m_input.grabAction, squeezeClickPath[Side::LEFT]},
                                                            {m_input.grabAction, squeezeClickPath[Side::RIGHT]},
                                                            {m_input.poseAction, posePath[Side::LEFT]},
                                                            {m_input.poseAction, posePath[Side::RIGHT]},
                                                            {m_input.quitAction, menuClickPath[Side::LEFT]},
                                                            {m_input.quitAction, menuClickPath[Side::RIGHT]},
                                                            {m_input.vibrateAction, hapticPath[Side::LEFT]},
                                                            {m_input.vibrateAction, hapticPath[Side::RIGHT]}}};

            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedBindings.interactionProfile = microsoftMixedRealityInteractionProfilePath;
            suggestedBindings.suggestedBindings = bindings.data();
            suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
            CHECK_XRCMD(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
        }
        XrActionSpaceCreateInfo actionSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        actionSpaceInfo.action = m_input.poseAction;
        actionSpaceInfo.poseInActionSpace.orientation.w = 1.f;
        actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::LEFT];
        CHECK_XRCMD(xrCreateActionSpace(m_session, &actionSpaceInfo, &m_input.handSpace[Side::LEFT]));
        actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::RIGHT];
        CHECK_XRCMD(xrCreateActionSpace(m_session, &actionSpaceInfo, &m_input.handSpace[Side::RIGHT]));

#if ADD_AIM_POSE
        actionSpaceInfo.action = m_input.aimPoseAction;

        actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::LEFT];
        CHECK_XRCMD(xrCreateActionSpace(m_session, &actionSpaceInfo, &m_input.aimSpace[Side::LEFT]));

        actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::RIGHT];
        CHECK_XRCMD(xrCreateActionSpace(m_session, &actionSpaceInfo, &m_input.aimSpace[Side::RIGHT]));
#endif

        XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attachInfo.countActionSets = 1;
        attachInfo.actionSets = &m_input.actionSet;
        CHECK_XRCMD(xrAttachSessionActionSets(m_session, &attachInfo));
    }

    void CreateVisualizedSpaces() {
        CHECK(m_session != XR_NULL_HANDLE);

        std::string visualizedSpaces[] = {"ViewFront",        "Local", "Stage", "StageLeft", "StageRight", "StageLeftRotated",
                                          "StageRightRotated"};

        for (const auto& visualizedSpace : visualizedSpaces) {
            XrReferenceSpaceCreateInfo referenceSpaceCreateInfo = GetXrReferenceSpaceCreateInfo(visualizedSpace);
            XrSpace space;
            XrResult res = xrCreateReferenceSpace(m_session, &referenceSpaceCreateInfo, &space);
            if (XR_SUCCEEDED(res)) {
                m_visualizedSpaces.push_back(space);
            } else {
                Log::Write(Log::Level::Warning,
                           Fmt("Failed to create reference space %s with error %d", visualizedSpace.c_str(), res));
            }
        }
    }

    void InitializeSession() override 
    {
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_session == XR_NULL_HANDLE);

        {
            Log::Write(Log::Level::Verbose, Fmt("Creating session..."));

            XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
            createInfo.next = m_graphicsPlugin->GetGraphicsBinding();
            createInfo.systemId = m_systemId;
            CHECK_XRCMD(xrCreateSession(m_instance, &createInfo, &m_session));
        }

        LogReferenceSpaces();
        InitializeActions();
        CreateVisualizedSpaces();

        {
            XrReferenceSpaceCreateInfo referenceSpaceCreateInfo = GetXrReferenceSpaceCreateInfo(m_options->AppSpace);
            CHECK_XRCMD(xrCreateReferenceSpace(m_session, &referenceSpaceCreateInfo, &m_appSpace));
        }

#if ENABLE_OPENXR_FB_REFRESH_RATE
        GetMaxRefreshRate();
		SetRefreshRate(DESIRED_REFRESH_RATE);
#endif

#if ENABLE_OPENXR_FB_EYE_TRACKING_SOCIAL
		CreateEyeTracker();
#endif

#if ENABLE_OPENXR_FB_BODY_TRACKING
		CreateBodyTracker();
#endif
    }

    void CreateSwapchains() override 
    {
        CHECK(m_session != XR_NULL_HANDLE);
        CHECK(m_swapchains.empty());
        CHECK(m_configViews.empty());

        // Read graphics properties for preferred swapchain length and logging.
        XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
        CHECK_XRCMD(xrGetSystemProperties(m_instance, m_systemId, &systemProperties));

        // Log system properties.
        Log::Write(Log::Level::Info,
                   Fmt("System Properties: Name=%s VendorId=%d", systemProperties.systemName, systemProperties.vendorId));

        Log::Write(Log::Level::Info, Fmt("System Graphics Properties: MaxWidth=%d MaxHeight=%d MaxLayers=%d",
                                         systemProperties.graphicsProperties.maxSwapchainImageWidth,
                                         systemProperties.graphicsProperties.maxSwapchainImageHeight,
                                         systemProperties.graphicsProperties.maxLayerCount));

        Log::Write(Log::Level::Info, Fmt("System Tracking Properties: OrientationTracking=%s PositionTracking=%s",
                                         systemProperties.trackingProperties.orientationTracking == XR_TRUE ? "True" : "False",
                                         systemProperties.trackingProperties.positionTracking == XR_TRUE ? "True" : "False"));

        // Note: No other view configurations exist at the time this code was written. If this
        // condition is not met, the project will need to be audited to see how support should be
        // added.
        CHECK_MSG(m_options->Parsed.ViewConfigType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                  "Unsupported view configuration type");

        // Query and cache view configuration views.
        uint32_t viewCount = 0;
        CHECK_XRCMD(
            xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_options->Parsed.ViewConfigType, 0, &viewCount, nullptr));
        m_configViews.resize(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_options->Parsed.ViewConfigType, viewCount,
                                                      &viewCount, m_configViews.data()));

        // Create and cache view buffer for xrLocateViews later.
        m_views.resize(viewCount, {XR_TYPE_VIEW});

        // Create the swapchain and get the images.
        if (viewCount > 0) 
        {
            // Select a swapchain format.
            uint32_t swapchainFormatCount;
            CHECK_XRCMD(xrEnumerateSwapchainFormats(m_session, 0, &swapchainFormatCount, nullptr));
            std::vector<int64_t> swapchainFormats(swapchainFormatCount);
            CHECK_XRCMD(xrEnumerateSwapchainFormats(m_session, (uint32_t)swapchainFormats.size(), &swapchainFormatCount,
                                                    swapchainFormats.data()));
            CHECK(swapchainFormatCount == swapchainFormats.size());
            m_colorSwapchainFormat = m_graphicsPlugin->SelectColorSwapchainFormat(swapchainFormats);

            // Print swapchain formats and the selected one.
            {
                std::string swapchainFormatsString;
                for (int64_t format : swapchainFormats) {
                    const bool selected = format == m_colorSwapchainFormat;
                    swapchainFormatsString += " ";
                    if (selected) {
                        swapchainFormatsString += "[";
                    }
                    swapchainFormatsString += std::to_string(format);
                    if (selected) {
                        swapchainFormatsString += "]";
                    }
                }
                Log::Write(Log::Level::Verbose, Fmt("Swapchain Formats: %s", swapchainFormatsString.c_str()));
            }

            // Create a swapchain for each view.
            for (uint32_t i = 0; i < viewCount; i++) 
            {
                const XrViewConfigurationView& vp = m_configViews[i];
                Log::Write(Log::Level::Info,
                           Fmt("Creating swapchain for view %d with dimensions Width=%d Height=%d SampleCount=%d", i,
                               vp.recommendedImageRectWidth, vp.recommendedImageRectHeight, vp.recommendedSwapchainSampleCount));

                // Create the swapchain.
                XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
                swapchainCreateInfo.arraySize = 1;
                swapchainCreateInfo.format = m_colorSwapchainFormat;
                swapchainCreateInfo.width = vp.recommendedImageRectWidth;
                swapchainCreateInfo.height = vp.recommendedImageRectHeight;
                swapchainCreateInfo.mipCount = 1;
                swapchainCreateInfo.faceCount = 1;
                swapchainCreateInfo.sampleCount = m_graphicsPlugin->GetSupportedSwapchainSampleCount(vp);
                swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
                Swapchain swapchain;
                swapchain.width = swapchainCreateInfo.width;
                swapchain.height = swapchainCreateInfo.height;
                CHECK_XRCMD(xrCreateSwapchain(m_session, &swapchainCreateInfo, &swapchain.handle));

                m_swapchains.push_back(swapchain);

                uint32_t imageCount;
                CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, 0, &imageCount, nullptr));
                // XXX This should really just return XrSwapchainImageBaseHeader*
                std::vector<XrSwapchainImageBaseHeader*> swapchainImages =
                    m_graphicsPlugin->AllocateSwapchainImageStructs(imageCount, swapchainCreateInfo);
                CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, imageCount, &imageCount, swapchainImages[0]));

                m_swapchainImages.insert(std::make_pair(swapchain.handle, std::move(swapchainImages)));
            }
        }
    }

#if SUPPORT_SCREENSHOTS
    bool take_screenshot_ = false;

    void TakeScreenShot() override 
    {
        if (!take_screenshot_) 
        {
            Log::Write(Log::Level::Verbose, "TakeScreenShot");
            take_screenshot_ = true;
        }
    }

    void SaveScreenShotIfNecessary()
    {
        if (!take_screenshot_ || !m_graphicsPlugin) 
        {
            return;
        }

        Log::Write(Log::Level::Verbose, "SaveScreenShotIfNecessary");

#ifdef XR_USE_PLATFORM_WIN32
        const std::string filename = "d:\\TEST\\windows_hello_xr_screenshot.png";
#else
        const std::string directory = "/sdcard/Android/data/com.khronos.openxr.hello_xr.opengles/files/";
        const std::string filename = directory + "android_hello_xr_screenshot.png";
#endif

        m_graphicsPlugin->SaveScreenShot(filename);
        take_screenshot_ = false;
    }
#endif

#if ENABLE_OPENXR_FB_REFRESH_RATE
	std::vector<float> supported_refresh_rates_;
	float current_refresh_rate_ = 0.0f;
	float max_refresh_rate_ = 0.0f;

	PFN_xrGetDisplayRefreshRateFB xrGetDisplayRefreshRateFB = nullptr;
	PFN_xrEnumerateDisplayRefreshRatesFB xrEnumerateDisplayRefreshRatesFB = nullptr;
	PFN_xrRequestDisplayRefreshRateFB xrRequestDisplayRefreshRateFB = nullptr;

	const std::vector<float>& GetSupportedRefreshRates() override
	{
		if (!supported_refresh_rates_.empty())
		{
			return supported_refresh_rates_;
		}

		if (supports_refresh_rate_)
		{
			if (xrEnumerateDisplayRefreshRatesFB == nullptr)
			{
				XR_LOAD(m_instance, xrEnumerateDisplayRefreshRatesFB);
			}

			uint32_t num_refresh_rates = 0;
			XrResult result = xrEnumerateDisplayRefreshRatesFB(m_session, 0, &num_refresh_rates, nullptr);

			if ((result == XR_SUCCESS) && (num_refresh_rates > 0))
			{
				supported_refresh_rates_.resize(num_refresh_rates);
				result = xrEnumerateDisplayRefreshRatesFB(m_session, num_refresh_rates, &num_refresh_rates, supported_refresh_rates_.data());

				if (result == XR_SUCCESS)
				{
					std::sort(supported_refresh_rates_.begin(), supported_refresh_rates_.end());
				}

                Log::Write(Log::Level::Info, "OPENXR : GetSupportedRefreshRates:\n");

                for (float refresh_rate : supported_refresh_rates_)
                {
                    Log::Write(Log::Level::Info, Fmt("OPENXR : \t %.2f Hz", refresh_rate));
                }
			}
		}

		return supported_refresh_rates_;
	}

	float GetCurrentRefreshRate() override
	{
		if (current_refresh_rate_ > 0.0f)
		{
			return current_refresh_rate_;
		}

		if (supports_refresh_rate_)
		{
			if (xrGetDisplayRefreshRateFB == nullptr)
			{
				XR_LOAD(m_instance, xrGetDisplayRefreshRateFB);
			}

            XrResult result = xrGetDisplayRefreshRateFB(m_session, &current_refresh_rate_);

            if (result == XR_SUCCESS)
            {
                Log::Write(Log::Level::Info, Fmt("OPENXR : GetCurrentRefreshRate => %.2f Hz", current_refresh_rate_));
            }
		}
		else
		{
			current_refresh_rate_ = DEFAULT_REFRESH_RATE;
		}

		return current_refresh_rate_;
	}

	float GetMaxRefreshRate() override
	{
		if (max_refresh_rate_ > 0.0f)
		{
			return max_refresh_rate_;
		}

		GetSupportedRefreshRates();

        if (supported_refresh_rates_.empty())
        {
            max_refresh_rate_ = DEFAULT_REFRESH_RATE;
        }
        else
		{
			// supported_refresh_rates_ is pre-sorted, so the last element is the highest supported refresh rate
			max_refresh_rate_ = supported_refresh_rates_.back();
            Log::Write(Log::Level::Info, Fmt("OPENXR : GetMaxRefreshRate => %.2f Hz", max_refresh_rate_));
		}

		return max_refresh_rate_;
	}

	bool IsRefreshRateSupported(const float refresh_rate) override
	{
		GetSupportedRefreshRates();

		if (!supported_refresh_rates_.empty())
		{
			const bool found_it = (std::find(supported_refresh_rates_.begin(), supported_refresh_rates_.end(), refresh_rate) != supported_refresh_rates_.end());
			return found_it;
		}

		return (refresh_rate == DEFAULT_REFRESH_RATE);
	}

	// 0.0 = system default
    void SetRefreshRate(const float refresh_rate) override
	{
		if (!supports_refresh_rate_ || !m_session)
		{
			return;
		}

		if (current_refresh_rate_ == 0.0f)
		{
			GetCurrentRefreshRate();
		}

		if (refresh_rate == current_refresh_rate_)
		{
			return;
		}

        if (!IsRefreshRateSupported(refresh_rate))
        {
            return;
        }

		if (xrRequestDisplayRefreshRateFB == nullptr)
		{
			XR_LOAD(m_instance, xrRequestDisplayRefreshRateFB);
		}

		XrResult result = xrRequestDisplayRefreshRateFB(m_session, refresh_rate);

		if (result == XR_SUCCESS)
		{
			Log::Write(Log::Level::Info, Fmt("OPENXR : SetRefreshRate SUCCESSFULLY CHANGED from %.2f TO = %.2f Hz", current_refresh_rate_, refresh_rate));
			current_refresh_rate_ = refresh_rate;
		}
	}
#endif

#if ENABLE_OPENXR_FB_SHARPENING
	bool is_sharpening_supported_ = false;
	bool is_sharpening_enabled_ = false;

	bool IsSharpeningEnabled() const override
	{
		return (is_sharpening_supported_ && is_sharpening_enabled_);
	}

	void SetSharpeningEnabled(const bool enabled) override
	{
		if (!is_sharpening_supported_)
		{
			is_sharpening_enabled_ = false;
			return;
		}

        // Only support one flag, sharpening, for now
        composition_layer_settings_.layerFlags = enabled ? XR_COMPOSITION_LAYER_SETTINGS_QUALITY_SHARPENING_BIT_FB : 0;
		is_sharpening_enabled_ = enabled;
	}
#endif

#if ENABLE_OPENXR_FB_COMPOSITION_LAYER_SETTINGS
	//bool supports_composition_layer_ = false;
	XrCompositionLayerSettingsFB composition_layer_settings_ = { XR_TYPE_COMPOSITION_LAYER_SETTINGS_FB, nullptr, XR_COMPOSITION_LAYER_SETTINGS_QUALITY_SHARPENING_BIT_FB };
#endif


#if ENABLE_OPENXR_FB_LOCAL_DIMMING
	bool is_local_dimming_enabled_ = false;
	XrLocalDimmingFrameEndInfoMETA local_dimming_settings_ = { XR_TYPE_FRAME_END_INFO, nullptr, XR_LOCAL_DIMMING_MODE_ON_META };

	bool IsLocalDimmingEnabled() const override
	{
		return (supports_local_dimming_ && is_local_dimming_enabled_);
	}

	void SetLocalDimmingEnabled(const bool enabled) override
	{
		if (!supports_local_dimming_)
		{
            is_local_dimming_enabled_ = false;
			return;
		}
        
        if (enabled != is_local_dimming_enabled_)
        {
            Log::Write(Log::Level::Info, Fmt("OPENXR LOCAL DIMMING = %s", enabled ? "ON" : "OFF"));
            local_dimming_settings_.localDimmingMode = enabled ? XR_LOCAL_DIMMING_MODE_ON_META : XR_LOCAL_DIMMING_MODE_OFF_META;
            is_local_dimming_enabled_ = enabled;
        }
	}
#endif

#if ENABLE_OPENXR_HAND_TRACKING
#endif

#if ENABLE_OPENXR_FB_EYE_TRACKING_SOCIAL
	PFN_xrCreateEyeTrackerFB xrCreateEyeTrackerFB = nullptr;
	PFN_xrDestroyEyeTrackerFB xrDestroyEyeTrackerFB = nullptr;
	PFN_xrGetEyeGazesFB xrGetEyeGazesFB = nullptr;

    bool eye_tracking_enabled_ = false;
	XrEyeTrackerFB eye_tracker_ = nullptr;
	XrEyeGazesFB eye_gazes_{ XR_TYPE_EYE_GAZES_FB, nullptr };

	bool GetGazePose(const int eye, XrPosef& gaze_pose) override
	{
		if (eye_tracking_enabled_&& eye_gazes_.gaze[eye].isValid)
		{
			gaze_pose = eye_gazes_.gaze[eye].gazePose;
			return true;
		}

		return false;
	}

	void CreateEyeTracker()
	{
		if (supports_eye_tracking_social_ && m_instance && m_session)
		{
			if (xrCreateEyeTrackerFB == nullptr)
			{
				XR_LOAD(m_instance, xrCreateEyeTrackerFB);
			}

			if (xrCreateEyeTrackerFB == nullptr)
			{
				return;
			}

			XrEyeTrackerCreateInfoFB create_info{ XR_TYPE_EYE_TRACKER_CREATE_INFO_FB };
			XrResult result = xrCreateEyeTrackerFB(m_session, &create_info, &eye_tracker_);

			if (result == XR_SUCCESS)
			{
				Log::Write(Log::Level::Info, "OPENXR - Eye tracking enabled and running...");
				eye_tracking_enabled_ = true;
			}
		}
	}

	void DestroyEyeTracker()
	{
		if (eye_tracker_)
		{
			if (xrDestroyEyeTrackerFB == nullptr)
			{
				XR_LOAD(m_instance, xrDestroyEyeTrackerFB);
			}

			if (xrDestroyEyeTrackerFB == nullptr)
			{
				return;
			}

			xrDestroyEyeTrackerFB(eye_tracker_);
			eye_tracker_ = nullptr;
			eye_tracking_enabled_ = false;

			Log::Write(Log::Level::Info, "OPENXR - Eye tracker destroyed...");
		}
	}

	void UpdateEyeTrackerGazes()
	{
		if (eye_tracker_ && eye_tracking_enabled_)
		{
			if (xrGetEyeGazesFB == nullptr)
			{
				XR_LOAD(m_instance, xrGetEyeGazesFB);
			}

			if (xrGetEyeGazesFB == nullptr)
			{
				return;
			}

			XrEyeGazesInfoFB gazes_info{ XR_TYPE_EYE_GAZES_INFO_FB };
			gazes_info.baseSpace = m_appSpace;

#if LOG_EYE_TRACKING_DATA
			XrResult result = xrGetEyeGazesFB(eye_tracker_, &gazes_info, &eye_gazes_);

			if (result == XR_SUCCESS)
			{
				Log::Write(Log::Level::Info, Fmt("OPENXR GAZES: Left Eye => %.2f, %.2f, Right Eye => %.2f, %.2f",
					eye_gazes_.gaze[Side::LEFT].gazePose.orientation.x, eye_gazes_.gaze[Side::LEFT].gazePose.orientation.y,
					eye_gazes_.gaze[Side::RIGHT].gazePose.orientation.x, eye_gazes_.gaze[Side::RIGHT].gazePose.orientation.y));
			}
#else
			xrGetEyeGazesFB(eye_tracker_, &gazes_info, &eye_gazes_);
#endif
		}
	}
#endif

#if ENABLE_OPENXR_FB_FACE_TRACKING

#endif

#if ENABLE_OPENXR_FB_BODY_TRACKING
	PFN_xrCreateBodyTrackerFB xrCreateBodyTrackerFB = nullptr;
	PFN_xrDestroyBodyTrackerFB xrDestroyBodyTrackerFB = nullptr;
	PFN_xrLocateBodyJointsFB xrLocateBodyJointsFB = nullptr;
    
    bool body_tracking_enabled_ = false;
    XrBodyTrackerFB body_tracker_ = nullptr;
    XrBodyJointLocationFB body_joints_[XR_BODY_JOINT_COUNT_FB] = {};
    XrBodyJointLocationsFB body_joint_locations_{ XR_TYPE_BODY_JOINT_LOCATIONS_FB, nullptr };

#if 0
    bool display_skeleton_ = false;
    PFN_xrGetSkeletonFB xrGetSkeletonFB = nullptr;
    XrBodySkeletonJointFB skeleton_joints_[XR_BODY_JOINT_COUNT_FB] = {};
    uint32_t skeleton_change_count_ = 0;
#endif

    void CreateBodyTracker()
    {
        if (!supports_body_tracking_ || !m_instance || !m_session || body_tracker_)
        {
            return;
        }

		if (xrCreateBodyTrackerFB == nullptr)
		{
			XR_LOAD(m_instance, xrCreateBodyTrackerFB);
		}

		if (xrCreateBodyTrackerFB == nullptr)
		{
			return;
		}

		XrBodyTrackerCreateInfoFB create_info{ XR_TYPE_BODY_TRACKER_CREATE_INFO_FB };
        create_info.bodyJointSet = XR_BODY_JOINT_SET_DEFAULT_FB;

		XrResult result = xrCreateBodyTrackerFB(m_session, &create_info, &body_tracker_);

		if (result == XR_SUCCESS)
		{
			Log::Write(Log::Level::Info, "OPENXR - Body tracking enabled and running...");
            body_tracking_enabled_ = true;
		}
    }

	void DestroyBodyTracker()
	{
		if (!supports_body_tracking_ || !body_tracker_)
		{
			return;
		}

		if (xrDestroyBodyTrackerFB == nullptr)
		{
			XR_LOAD(m_instance, xrDestroyBodyTrackerFB);
		}

		if (xrDestroyBodyTrackerFB == nullptr)
		{
			return;
		}

		xrDestroyBodyTrackerFB(body_tracker_);
		body_tracker_ = nullptr;
		body_tracking_enabled_ = false;

		Log::Write(Log::Level::Info, "OPENXR - Body tracker destroyed...");
	}

	void UpdateBodyTrackerLocations(const XrTime& predicted_display_time)
	{
        if (body_tracker_ && body_tracking_enabled_)
        {
            if (xrLocateBodyJointsFB == nullptr)
            {
                XR_LOAD(m_instance, xrLocateBodyJointsFB);
            }

            if (xrLocateBodyJointsFB == nullptr)
            {
                return;
            }

#if 0
			if (xrGetSkeletonFB == nullptr)
			{
				XR_LOAD(m_instance, xrGetSkeletonFB);
			}

			if (xrGetSkeletonFB == nullptr)
			{
				return;
			}
#endif

            XrBodyJointsLocateInfoFB locate_info{ XR_TYPE_BODY_JOINTS_LOCATE_INFO_FB };
            locate_info.baseSpace = m_appSpace;
            locate_info.time = predicted_display_time;

            body_joint_locations_.next = nullptr;
            body_joint_locations_.jointCount = XR_BODY_JOINT_COUNT_FB;
            body_joint_locations_.jointLocations = body_joints_;

#if LOG_BODY_TRACKING_DATA
            XrResult result = xrLocateBodyJointsFB(body_tracker_, &locate_info, &body_joint_locations_);

            if (result == XR_SUCCESS)
            {
                Log::Write(Log::Level::Info, Fmt("OPENXR UPDATE BODY LOCATIONS SUCCEEDED"));
            }
            else
            {
                Log::Write(Log::Level::Info, Fmt("OPENXR UPDATE BODY LOCATIONS FAILED"));
            }
#else
            xrLocateBodyJointsFB(body_tracker_, &locate_info, &body_joint_locations_);
#endif
		}
	}
#endif


    // Return event if one is available, otherwise return null.
    const XrEventDataBaseHeader* TryReadNextEvent() 
    {
        // It is sufficient to clear the just the XrEventDataBuffer header to
        // XR_TYPE_EVENT_DATA_BUFFER
        XrEventDataBaseHeader* baseHeader = reinterpret_cast<XrEventDataBaseHeader*>(&m_eventDataBuffer);
        *baseHeader = {XR_TYPE_EVENT_DATA_BUFFER};
        const XrResult xr = xrPollEvent(m_instance, &m_eventDataBuffer);

        if (xr == XR_SUCCESS) 
        {
            if (baseHeader->type == XR_TYPE_EVENT_DATA_EVENTS_LOST) 
            {
                const XrEventDataEventsLost* const eventsLost = reinterpret_cast<const XrEventDataEventsLost*>(baseHeader);
                Log::Write(Log::Level::Warning, Fmt("%d events lost", eventsLost));
            }

            return baseHeader;
        }

        if (xr == XR_EVENT_UNAVAILABLE) 
        {
            return nullptr;
        }
        THROW_XR(xr, "xrPollEvent");
    }

    void PollEvents(bool* exitRenderLoop, bool* requestRestart) override 
    {
        *exitRenderLoop = *requestRestart = false;

        // Process all pending messages.
        while (const XrEventDataBaseHeader* event = TryReadNextEvent()) 
        {
            switch (event->type) {
                case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
                    const auto& instanceLossPending = *reinterpret_cast<const XrEventDataInstanceLossPending*>(event);
                    Log::Write(Log::Level::Warning, Fmt("XrEventDataInstanceLossPending by %lld", instanceLossPending.lossTime));
                    *exitRenderLoop = true;
                    *requestRestart = true;
                    return;
                }
                case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                    auto sessionStateChangedEvent = *reinterpret_cast<const XrEventDataSessionStateChanged*>(event);
                    HandleSessionStateChangedEvent(sessionStateChangedEvent, exitRenderLoop, requestRestart);
                    break;
                }
                case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                    LogActionSourceName(m_input.grabAction, "Grab");
                    LogActionSourceName(m_input.quitAction, "Quit");
                    LogActionSourceName(m_input.poseAction, "Pose");
                    LogActionSourceName(m_input.vibrateAction, "Vibrate");
                    break;
                case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
                default: {
                    Log::Write(Log::Level::Verbose, Fmt("Ignoring event type %d", event->type));
                    break;
                }
            }
        }
    }

    void HandleSessionStateChangedEvent(const XrEventDataSessionStateChanged& stateChangedEvent, bool* exitRenderLoop, bool* requestRestart) 
    {
        const XrSessionState oldState = m_sessionState;
        m_sessionState = stateChangedEvent.state;

        Log::Write(Log::Level::Info, Fmt("XrEventDataSessionStateChanged: state %s->%s session=%lld time=%lld", to_string(oldState),
                                         to_string(m_sessionState), stateChangedEvent.session, stateChangedEvent.time));

        if ((stateChangedEvent.session != XR_NULL_HANDLE) && (stateChangedEvent.session != m_session)) 
        {
            Log::Write(Log::Level::Error, "XrEventDataSessionStateChanged for unknown session");
            return;
        }

        switch (m_sessionState) {
            case XR_SESSION_STATE_READY: {
                CHECK(m_session != XR_NULL_HANDLE);
                XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                sessionBeginInfo.primaryViewConfigurationType = m_options->Parsed.ViewConfigType;
                CHECK_XRCMD(xrBeginSession(m_session, &sessionBeginInfo));
                m_sessionRunning = true;
                break;
            }
            case XR_SESSION_STATE_STOPPING: {
                CHECK(m_session != XR_NULL_HANDLE);
                m_sessionRunning = false;
                CHECK_XRCMD(xrEndSession(m_session))
                break;
            }
            case XR_SESSION_STATE_EXITING: {
                *exitRenderLoop = true;
                // Do not attempt to restart because user closed this session.
                *requestRestart = false;
                break;
            }
            case XR_SESSION_STATE_LOSS_PENDING: {
                *exitRenderLoop = true;
                // Poll for a new instance.
                *requestRestart = true;
                break;
            }
            default:
                break;
        }
    }

    void LogActionSourceName(XrAction action, const std::string& actionName) const 
    {
        XrBoundSourcesForActionEnumerateInfo getInfo = {XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
        getInfo.action = action;
        uint32_t pathCount = 0;
        CHECK_XRCMD(xrEnumerateBoundSourcesForAction(m_session, &getInfo, 0, &pathCount, nullptr));

        std::vector<XrPath> paths(pathCount);
        CHECK_XRCMD(xrEnumerateBoundSourcesForAction(m_session, &getInfo, uint32_t(paths.size()), &pathCount, paths.data()));

        std::string sourceName;

        for (uint32_t i = 0; i < pathCount; ++i) 
        {
            constexpr XrInputSourceLocalizedNameFlags all = XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT |
                                                            XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT |
                                                            XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;

            XrInputSourceLocalizedNameGetInfo nameInfo = {XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO};
            nameInfo.sourcePath = paths[i];
            nameInfo.whichComponents = all;

            uint32_t size = 0;
            CHECK_XRCMD(xrGetInputSourceLocalizedName(m_session, &nameInfo, 0, &size, nullptr));

            if (size < 1) 
            {
                continue;
            }
            std::vector<char> grabSource(size);
            CHECK_XRCMD(xrGetInputSourceLocalizedName(m_session, &nameInfo, uint32_t(grabSource.size()), &size, grabSource.data()));

            if (!sourceName.empty()) 
            {
                sourceName += " and ";
            }
            sourceName += "'";
            sourceName += std::string(grabSource.data(), size - 1);
            sourceName += "'";
        }

        Log::Write(Log::Level::Info,
                   Fmt("%s action is bound to %s", actionName.c_str(), ((!sourceName.empty()) ? sourceName.c_str() : "nothing")));
    }

    bool IsSessionRunning() const override { return m_sessionRunning; }

    bool IsSessionFocused() const override { return m_sessionState == XR_SESSION_STATE_FOCUSED; }

    void PollActions() override 
    {
        m_input.handActive = {XR_FALSE, XR_FALSE};

        // Sync actions
        const XrActiveActionSet activeActionSet{m_input.actionSet, XR_NULL_PATH};
        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        syncInfo.countActiveActionSets = 1;
        syncInfo.activeActionSets = &activeActionSet;
        CHECK_XRCMD(xrSyncActions(m_session, &syncInfo));

        // Get pose and grab action state and start haptic vibrate when hand is 90% squeezed.
        for (auto hand : {Side::LEFT, Side::RIGHT}) 
        {
            XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
            getInfo.action = m_input.grabAction;
            getInfo.subactionPath = m_input.handSubactionPath[hand];

            XrActionStateFloat grabValue{XR_TYPE_ACTION_STATE_FLOAT};
            CHECK_XRCMD(xrGetActionStateFloat(m_session, &getInfo, &grabValue));

            if (grabValue.isActive == XR_TRUE) 
            {
                // Scale the rendered hand by 1.0f (open) to 0.5f (fully squeezed).
                m_input.handScale[hand] = 1.0f - 0.5f * grabValue.currentState;

                if (grabValue.currentState > 0.9f) 
                {
                    XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
                    vibration.amplitude = 0.5;
                    vibration.duration = XR_MIN_HAPTIC_DURATION;
                    vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

                    XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
                    hapticActionInfo.action = m_input.vibrateAction;
                    hapticActionInfo.subactionPath = m_input.handSubactionPath[hand];
                    CHECK_XRCMD(xrApplyHapticFeedback(m_session, &hapticActionInfo, (XrHapticBaseHeader*)&vibration));
                }

#if TAKE_SCREENSHOT_WITH_LEFT_GRAB
                if (hand == Side::LEFT) 
                {
                    m_input.handScale[hand] = 1.0f;
                    
                    static bool currently_gripping = false;

                    if (!currently_gripping && grabValue.currentState > 0.9f) 
                    {
                        TakeScreenShot();
                        currently_gripping = true;
                    } 
                    else if (currently_gripping && grabValue.currentState < 0.5f) 
                    {
                        currently_gripping = false;
                    }
                }
#endif

#if ENABLE_LOCAL_DIMMING_WITH_RIGHT_GRAB
                if (hand == Side::RIGHT)
                {
                    const bool enable_local_dimming = (grabValue.currentState > 0.9f);
                    SetLocalDimmingEnabled(enable_local_dimming);

                    m_input.handScale[hand] = 1.0f;
                }
#endif

#if USE_THUMBSTICKS_FOR_SMOOTH_LOCOMOTION
                XrActionStateFloat axis_state_x = { XR_TYPE_ACTION_STATE_FLOAT };
                XrActionStateFloat axis_state_y = { XR_TYPE_ACTION_STATE_FLOAT };

                XrActionStateGetInfo action_get_info = { XR_TYPE_ACTION_STATE_GET_INFO };
                action_get_info.subactionPath = m_input.handSubactionPath[hand];

                action_get_info.action = m_input.thumbstickXAction;
				CHECK_XRCMD(xrGetActionStateFloat(m_session, &action_get_info, &axis_state_x));

				action_get_info.action = m_input.thumbstickYAction;
				CHECK_XRCMD(xrGetActionStateFloat(m_session, &action_get_info, &axis_state_y));

                if (hand == Side::LEFT)
                {
                    // Left thumbstick X/Y = Movement (X,0,Z in 3D GLM coords)
                    glm::vec2 left_thumbstick_values = {};

					if (axis_state_x.isActive)
					{
                        left_thumbstick_values.x = axis_state_x.currentState;
					}

					if (axis_state_y.isActive)
					{
						left_thumbstick_values.y = axis_state_y.currentState;
					}

					const bool has_moved = (axis_state_x.isActive || axis_state_y.isActive);

                    if (has_moved)
                    {
                        move_player(left_thumbstick_values);
                    }
                }
                else
                {
                    // Right thumbstick X value = Rotation
					if (axis_state_x.isActive)
					{
						const float right_thumbstick_x_value = axis_state_x.currentState;
						rotate_player(right_thumbstick_x_value);
					}
                }
#endif
            }

            getInfo.action = m_input.poseAction;
            XrActionStatePose poseState{XR_TYPE_ACTION_STATE_POSE};
            CHECK_XRCMD(xrGetActionStatePose(m_session, &getInfo, &poseState));
            m_input.handActive[hand] = poseState.isActive;
        }

        // There were no subaction paths specified for the quit action, because we don't care which hand did it.
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr, m_input.quitAction, XR_NULL_PATH};
        XrActionStateBoolean quitValue{XR_TYPE_ACTION_STATE_BOOLEAN};
        CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &quitValue));

        if ((quitValue.isActive == XR_TRUE) && (quitValue.changedSinceLastSync == XR_TRUE) && (quitValue.currentState == XR_TRUE)) 
        {
            CHECK_XRCMD(xrRequestExitSession(m_session));
        }
    }

    void RenderFrame() override 
    {
        CHECK(m_session != XR_NULL_HANDLE);

        XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        CHECK_XRCMD(xrWaitFrame(m_session, &frameWaitInfo, &frameState));

        XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        CHECK_XRCMD(xrBeginFrame(m_session, &frameBeginInfo));

        std::vector<XrCompositionLayerBaseHeader*> layers;
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};

        std::vector<XrCompositionLayerProjectionView> projectionLayerViews;

        if (frameState.shouldRender == XR_TRUE) 
        {
            if (RenderLayer(frameState.predictedDisplayTime, projectionLayerViews, layer)) 
            {
                XrCompositionLayerBaseHeader* header = reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer);

#if ENABLE_OPENXR_FB_COMPOSITION_LAYER_SETTINGS
				if (supports_composition_layer_)
				{
                    // Sharpening / Upscaling flags are set here. Mobile only (Quest)
                    header->next = &composition_layer_settings_;
				}
#endif
                layers.push_back(header);
            }
        }

        XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
        frameEndInfo.displayTime = frameState.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = m_options->Parsed.EnvironmentBlendMode;
        frameEndInfo.layerCount = (uint32_t)layers.size();
        frameEndInfo.layers = layers.data();

#if ENABLE_OPENXR_FB_LOCAL_DIMMING
		if (supports_local_dimming_)
		{
			local_dimming_settings_.localDimmingMode = is_local_dimming_enabled_ ? XR_LOCAL_DIMMING_MODE_ON_META : XR_LOCAL_DIMMING_MODE_OFF_META;
			frameEndInfo.next = &local_dimming_settings_;
		}
#endif
        CHECK_XRCMD(xrEndFrame(m_session, &frameEndInfo));
    }

    bool RenderLayer(XrTime predictedDisplayTime, std::vector<XrCompositionLayerProjectionView>& projectionLayerViews, XrCompositionLayerProjection& layer) 
    {
        XrResult res;

        XrViewState viewState{XR_TYPE_VIEW_STATE};
        uint32_t viewCapacityInput = (uint32_t)m_views.size();
        uint32_t viewCountOutput;

        XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        viewLocateInfo.viewConfigurationType = m_options->Parsed.ViewConfigType;
        viewLocateInfo.displayTime = predictedDisplayTime;
        viewLocateInfo.space = m_appSpace;

        res = xrLocateViews(m_session, &viewLocateInfo, &viewState, viewCapacityInput, &viewCountOutput, m_views.data());

        CHECK_XRRESULT(res, "xrLocateViews");

        if ((viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0 ||
            (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
            return false;  // There is no valid tracking poses for the views.
        }

        CHECK(viewCountOutput == viewCapacityInput);
        CHECK(viewCountOutput == m_configViews.size());
        CHECK(viewCountOutput == m_swapchains.size());

        projectionLayerViews.resize(viewCountOutput);

        // For each locatable space that we want to visualize, render a 25cm cube.
        std::vector<Cube> cubes;

#if ADD_EXTRA_CUBES
        const int num_cubes_x = 5;
        const int num_cubes_y = 5;
        const int num_cubes_z = 20;

        const float offset_x = (float)(num_cubes_x - 1) * 0.5f;
        const float offset_y = (float)(num_cubes_y - 1) * 0.5f;
        const float offset_z = 1.0f;

#if defined(WIN32)
        const int hand_for_cube_scale = Side::LEFT;
#else
        const int hand_for_cube_scale = Side::RIGHT;
#endif

        XrPosef cube_pose;
        cube_pose.orientation = {0.0f, 0.0f, 0.0f, 1.0f};

        float hand_scale = 0.1f * m_input.handScale[hand_for_cube_scale];
        XrVector3f scale_vec{hand_scale, hand_scale, hand_scale};

        for (int cube_z_index = 0; cube_z_index < num_cubes_z; cube_z_index++) 
        {
            for (int cube_y_index = 0; cube_y_index < num_cubes_y; cube_y_index++) 
            {
                for (int cube_x_index = 0; cube_x_index < num_cubes_x; cube_x_index++) 
                {
                    cube_pose.position =
                    {
                        (float)cube_x_index - offset_x, 
                        (float)cube_y_index - offset_y, 
                        -(float)cube_z_index - offset_z
                    };

                    Cube cube{cube_pose, scale_vec};
                    cubes.push_back(cube);
                }
            }
        }
#endif

#if 0
        for (XrSpace visualizedSpace : m_visualizedSpaces) 
        {
            XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION};
            res = xrLocateSpace(visualizedSpace, m_appSpace, predictedDisplayTime, &spaceLocation);
            
            CHECK_XRRESULT(res, "xrLocateSpace");
            if (XR_UNQUALIFIED_SUCCESS(res)) 
            {
                if ((spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
                    (spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
                    cubes.push_back(Cube{spaceLocation.pose, {0.25f, 0.25f, 0.25f}});
                }
            } else {
                Log::Write(Log::Level::Verbose, Fmt("Unable to locate a visualized reference space in app space: %d", res));
            }
        }
#endif

#if (DRAW_GRIP_POSE || DRAW_AIM_POSE)
        // Render a 10cm cube scaled by grabAction for each hand. Note renderHand will only be
        // true when the application has focus.
        for (int hand : {Side::LEFT, Side::RIGHT}) 
        {
#if DRAW_GRIP_POSE
            XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION};
            res = xrLocateSpace(m_input.handSpace[hand], m_appSpace, predictedDisplayTime, &spaceLocation);
            CHECK_XRRESULT(res, "xrLocateSpace");
            
            if (XR_UNQUALIFIED_SUCCESS(res)) 
            {
                if ((spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
                    (spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
                    float scale = 0.1f * m_input.handScale[hand];
                    cubes.push_back(Cube{spaceLocation.pose, {scale, scale, scale}});
                }
            } 
            else 
            {
                // Tracking loss is expected when the hand is not active so only log a message
                // if the hand is active.
                if (m_input.handActive[hand] == XR_TRUE) {
                    const char* handName[] = {"left", "right"};
                    Log::Write(Log::Level::Verbose,
                               Fmt("Unable to locate %s hand action space in app space: %d", handName[hand], res));
                }
            }
#endif

#if DRAW_AIM_POSE
            {
                XrSpaceLocation aimSpaceLocation{XR_TYPE_SPACE_LOCATION};
                res = xrLocateSpace(m_input.aimSpace[hand], m_appSpace, predictedDisplayTime, &aimSpaceLocation);
                CHECK_XRRESULT(res, "xrLocateSpace");

                if (XR_UNQUALIFIED_SUCCESS(res)) 
                {
                    if ((aimSpaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
                        (aimSpaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) 
                    {
                        float scale = 0.1f * m_input.handScale[hand];
                        cubes.push_back(Cube{aimSpaceLocation.pose, {scale, scale, scale}});
                    }
                } 
            }
#endif
        }
#endif

#if LOG_MATRICES
        static bool log_IPD = true;

        if(log_IPD)
        {
            const XrPosef& left_eye = m_views[Side::LEFT].pose;
            const XrPosef& right_eye = m_views[Side::RIGHT].pose;

            const XrVector3f left_to_right = 
            {
                (right_eye.position.x - left_eye.position.x), 
                (right_eye.position.y - left_eye.position.y), 
                (right_eye.position.z - left_eye.position.z)
            };

            const float IPD = sqrtf((left_to_right.x * left_to_right.x) + (left_to_right.y * left_to_right.y) + (left_to_right.z * left_to_right.z));
            Log::Write(Log::Level::Info, Fmt("Computed IPD (mm) = %.7f", IPD * 1000.0f));
        }
#endif

#if ENABLE_OPENXR_FB_EYE_TRACKING_SOCIAL
        if (eye_tracking_enabled_)
        {
            UpdateEyeTrackerGazes();

            for (int eye : { Side::LEFT, Side::RIGHT }) 
            {
                XrPosef gaze_pose;

                if (GetGazePose(eye, gaze_pose))
                {
                    const float laser_length = 10.1f;
                    const float half_laser_length = laser_length * 0.5f;
                    const float distance_to_eye = 0.0f;

                    // Apply an offset so the lasers aren't overlapping the eye directly
                    XrVector3f local_laser_offset = { 0.0f, 0.0f, (-half_laser_length - distance_to_eye)};

					XrMatrix4x4f rotation_matrix;
					XrMatrix4x4f_CreateFromQuaternion(&rotation_matrix, &gaze_pose.orientation);

                    XrVector3f world_laser_offset;
                    XrMatrix4x4f_TransformVector3f(&world_laser_offset, &rotation_matrix, &local_laser_offset);

                    XrVector3f_Add(&gaze_pose.position, &gaze_pose.position, &world_laser_offset);

                    // Make a slender laser pointer-like box for gazes
                    XrVector3f gaze_cube_scale{ 0.01f, 0.01f, laser_length };
                    cubes.push_back(Cube{ gaze_pose, gaze_cube_scale });
                }
            }
        }
#endif

#if ENABLE_OPENXR_FB_BODY_TRACKING
        if (body_tracking_enabled_)
        {
            UpdateBodyTrackerLocations(predictedDisplayTime);

			if (body_joint_locations_.isActive) 
            {
				for (int i = 0; i < XR_BODY_JOINT_COUNT_FB; ++i) 
                {
					if ((body_joints_[i].locationFlags & (XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT))) 
                    {
						XrVector3f body_joint_scale{ BODY_CUBE_SIZE, BODY_CUBE_SIZE, BODY_CUBE_SIZE };
                        const XrPosef& body_joint_pose = body_joints_[i].pose;

						cubes.push_back(Cube{ body_joint_pose, body_joint_scale });
					}
				}
			}
        }
#endif

        // Render view to the appropriate part of the swapchain image.
        for (uint32_t i = 0; i < viewCountOutput; i++) 
        {
            // Each view has a separate swapchain which is acquired, rendered to, and released.
            const Swapchain viewSwapchain = m_swapchains[i];

            XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};

            uint32_t swapchainImageIndex;
            CHECK_XRCMD(xrAcquireSwapchainImage(viewSwapchain.handle, &acquireInfo, &swapchainImageIndex));

            XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            waitInfo.timeout = XR_INFINITE_DURATION;
            CHECK_XRCMD(xrWaitSwapchainImage(viewSwapchain.handle, &waitInfo));

            projectionLayerViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            projectionLayerViews[i].pose = m_views[i].pose;
            projectionLayerViews[i].fov = m_views[i].fov;
            projectionLayerViews[i].subImage.swapchain = viewSwapchain.handle;
            projectionLayerViews[i].subImage.imageRect.offset = {0, 0};
            projectionLayerViews[i].subImage.imageRect.extent = {viewSwapchain.width, viewSwapchain.height};

#if LOG_MATRICES
            static bool log_projection_matrices = true;

            if (log_projection_matrices) 
            {
				const float tanLeft = tanf(projectionLayerViews[i].fov.angleLeft);
				const float tanRight = tanf(projectionLayerViews[i].fov.angleRight);

				const float tanDown = tanf(projectionLayerViews[i].fov.angleDown);
				const float tanUp = tanf(projectionLayerViews[i].fov.angleUp);

                const std::string side_prefix = (i == Side::LEFT) ? "LEFT " : "RIGHT ";

				Log::Write(Log::Level::Info, side_prefix + Fmt("FOV angleLeft = %.7f (tan = %.7f)", projectionLayerViews[i].fov.angleLeft, tanLeft));
				Log::Write(Log::Level::Info, side_prefix + Fmt("FOV angleRight = %.7f (tan = %.7f)", projectionLayerViews[i].fov.angleRight, tanRight));
				Log::Write(Log::Level::Info, side_prefix + Fmt("FOV angleDown = %.7f (tan = %.7f)", projectionLayerViews[i].fov.angleDown, tanDown));
				Log::Write(Log::Level::Info, side_prefix + Fmt("FOV angleUp = %.7f (tan = %.7f)", projectionLayerViews[i].fov.angleUp, tanUp));

                Log::Write(Log::Level::Info, side_prefix + " Projection matrix:");

                XrMatrix4x4f proj;
                XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_OPENGL, projectionLayerViews[i].fov, 0.05f, 100.0f);

                Log::Write(Log::Level::Info, Fmt("%.7f\t%.7f\t%.7f\t%.7f", proj.m[0], proj.m[4], proj.m[8], proj.m[12]));
                Log::Write(Log::Level::Info, Fmt("%.7f\t%.7f\t%.7f\t%.7f", proj.m[1], proj.m[5], proj.m[9], proj.m[13]));
                Log::Write(Log::Level::Info, Fmt("%.7f\t%.7f\t%.7f\t%.7f", proj.m[2], proj.m[6], proj.m[10], proj.m[14]));
                Log::Write(Log::Level::Info, Fmt("%.7f\t%.7f\t%.7f\t%.7f", proj.m[3], proj.m[7], proj.m[11], proj.m[15]));
            }
#endif

            const XrSwapchainImageBaseHeader* const swapchainImage = m_swapchainImages[viewSwapchain.handle][swapchainImageIndex];
            m_graphicsPlugin->RenderView(projectionLayerViews[i], swapchainImage, m_colorSwapchainFormat, cubes);

#if SUPPORT_SCREENSHOTS
            if (i == Side::LEFT) 
            {
                SaveScreenShotIfNecessary();
            }
#endif

            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            CHECK_XRCMD(xrReleaseSwapchainImage(viewSwapchain.handle, &releaseInfo));
        }

        layer.space = m_appSpace;

        layer.layerFlags = (m_options->Parsed.EnvironmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND) ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT : 0;
        layer.viewCount = (uint32_t)projectionLayerViews.size();
        layer.views = projectionLayerViews.data();
        return true;
    }

   private:
    const std::shared_ptr<const Options> m_options;
    std::shared_ptr<IPlatformPlugin> m_platformPlugin;
    std::shared_ptr<IGraphicsPlugin> m_graphicsPlugin;

    XrInstance m_instance{XR_NULL_HANDLE};
    XrSession m_session{XR_NULL_HANDLE};
    XrSpace m_appSpace{XR_NULL_HANDLE};
    XrSystemId m_systemId{XR_NULL_SYSTEM_ID};

    std::vector<XrViewConfigurationView> m_configViews;
    std::vector<Swapchain> m_swapchains;
    std::map<XrSwapchain, std::vector<XrSwapchainImageBaseHeader*>> m_swapchainImages;
    std::vector<XrView> m_views;
    int64_t m_colorSwapchainFormat{-1};

    std::vector<XrSpace> m_visualizedSpaces;

    // Application's current lifecycle state according to the runtime
    XrSessionState m_sessionState{XR_SESSION_STATE_UNKNOWN};
    bool m_sessionRunning{false};

    XrEventDataBuffer m_eventDataBuffer;
    InputState m_input;

    const std::set<XrEnvironmentBlendMode> m_acceptableBlendModes;
};
}  // namespace

std::shared_ptr<IOpenXrProgram> CreateOpenXrProgram(const std::shared_ptr<Options>& options,
                                                    const std::shared_ptr<IPlatformPlugin>& platformPlugin,
                                                    const std::shared_ptr<IGraphicsPlugin>& graphicsPlugin) {
    return std::make_shared<OpenXrProgram>(options, platformPlugin, graphicsPlugin);
}
