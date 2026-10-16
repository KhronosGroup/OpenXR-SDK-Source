// Copyright (c) 2017-2026 The Khronos Group Inc.
//
// OpenXR spatial containers sample.
//
// Check out the latest version of this sample at https://khr.io/hellospatialcontainers.
//
// SPDX-License-Identifier: Apache-2.0

#include "openxr/openxr.h"
#include "pch.h"
#include "common.h"
#include "platformdata.h"
#include "platformplugin.h"
#include "graphicsplugin.h"
#include "swapchain_image_data.h"
#include "openxr_program.h"
#include "openxr_program_spatial_containers.h"
#include <common/xr_linear.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <set>

namespace {

constexpr int kNumContainers = 3;
constexpr int kMaxLayers = 4;

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

namespace Quatf {
static XrQuaternionf Identity() { return XrQuaternionf{0, 0, 0, 1}; }
}  // namespace Quatf

namespace Pose {
static XrPosef Identity() {
    XrPosef t{};
    t.orientation.w = 1;
    return t;
}

}  // namespace Pose
}  // namespace Math

std::vector<Cube> unscaledDuckCubes{// body
                                    {XrPosef{Math::Quatf::Identity(), {0.00, -0.2, 0.0}}, XrVector3f{0.6f, 0.5f, 0.8f}},
                                    // head
                                    {XrPosef{Math::Quatf::Identity(), {0.00, 0.15, 0.4}}, XrVector3f{0.35f, 0.25f, 0.35f}},
                                    // wings
                                    {XrPosef{Math::Quatf::Identity(), {0.3, -0.2, 0.0}}, XrVector3f{0.15f, 0.20f, 0.30f}},
                                    {XrPosef{Math::Quatf::Identity(), {-0.3, -0.2, 0.0}}, XrVector3f{0.15f, 0.20f, 0.30f}},
                                    // tail
                                    {XrPosef{Math::Quatf::Identity(), {0.0, 0.02, -0.38}}, XrVector3f{0.20f, 0.20f, 0.15f}},
                                    // eyes
                                    {XrPosef{Math::Quatf::Identity(), {0.1, 0.2, 0.55}}, XrVector3f{0.1f, 0.1f, 0.1f}},
                                    {XrPosef{Math::Quatf::Identity(), {-0.1, 0.2, 0.55}}, XrVector3f{0.1f, 0.1f, 0.1f}},
                                    // beak
                                    {XrPosef{Math::Quatf::Identity(), {0.0, 0.1, 0.55}}, XrVector3f{0.18f, 0.05f, 0.35f}}};

static inline std::vector<Cube> scaledDuckCubes(float scale) {
    std::vector<Cube> scaledCubes;
    for (const auto& cube : unscaledDuckCubes) {
        Cube c = cube;
        c.Pose.position.x *= scale;
        c.Pose.position.y *= scale;
        c.Pose.position.z *= scale;
        c.Scale.x *= scale;
        c.Scale.y *= scale;
        c.Scale.z *= scale;
        scaledCubes.push_back(c);
    }
    return scaledCubes;
}

struct OpenXrProgram : IOpenXrProgram {
    OpenXrProgram(const std::shared_ptr<IPlatformPlugin>& platformPlugin, const std::shared_ptr<IGraphicsPlugin>& graphicsPlugin)
        : m_platformPlugin(platformPlugin), m_graphicsPlugin(graphicsPlugin) {}

    ~OpenXrProgram() override {
        if (m_input.actionSet != XR_NULL_HANDLE) {
            xrDestroyActionSet(m_input.actionSet);
        }

        for (Swapchain swapchain : m_swapchains) {
            xrDestroySwapchain(swapchain.handle);
        }

        for (XrSpace containerSpace : m_containerSpaces) {
            if (containerSpace != XR_NULL_HANDLE) {
                xrDestroySpace(containerSpace);
            }
        }

        if (m_container2ReferenceSpace != XR_NULL_HANDLE) {
            xrDestroySpace(m_container2ReferenceSpace);
        }

        for (XrSpatialContainerEXT container : m_containers) {
            if (container != XR_NULL_HANDLE) {
                xrDestroySpatialContainerEXT(container);
            }
        }

        if (m_session != XR_NULL_HANDLE) {
            xrDestroySession(m_session);
        }

        if (m_instance != XR_NULL_HANDLE) {
            xrDestroyInstance(m_instance);
        }
    }

    static void LogLayersAndExtensions() {
        // Write out extension properties for a given layer.
        const auto logExtensions = [](const char* layerName, int indent = 0) {
            uint32_t instanceExtensionCount;
            CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(layerName, 0, &instanceExtensionCount, nullptr));
            std::vector<XrExtensionProperties> extensions(instanceExtensionCount, {XR_TYPE_EXTENSION_PROPERTIES});
            CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(layerName, (uint32_t)extensions.size(), &instanceExtensionCount,
                                                               extensions.data()));

            const std::string indentStr(indent, ' ');
            Log::Write(Log::Level::Verbose, Fmt("%sAvailable Extensions: (%d)", indentStr.c_str(), instanceExtensionCount));
            for (const XrExtensionProperties& extension : extensions) {
                Log::Write(Log::Level::Verbose, Fmt("%s  Name=%s SpecVersion=%d", indentStr.c_str(), extension.extensionName,
                                                    extension.extensionVersion));
            }
        };

        // Log non-layer extensions (layerName==nullptr).
        logExtensions(nullptr);

        // Log layers and any of their extensions.
        {
            uint32_t layerCount;
            CHECK_XRCMD(xrEnumerateApiLayerProperties(0, &layerCount, nullptr));
            std::vector<XrApiLayerProperties> layers(layerCount, {XR_TYPE_API_LAYER_PROPERTIES});
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

    void CreateInstanceInternal() {
        CHECK(m_instance == XR_NULL_HANDLE);

        // Create union of extensions required by platform and graphics plugins.
        std::vector<const char*> extensions{XR_EXT_SPATIAL_CONTAINER_EXTENSION_NAME,
                                            XR_EXT_SPATIAL_CONTAINER_SELF_RENDERING_EXTENSION_NAME};

        // Transform platform and graphics extension std::strings to C strings.
        const std::vector<std::string> platformExtensions = m_platformPlugin->GetInstanceExtensions();
        std::transform(platformExtensions.begin(), platformExtensions.end(), std::back_inserter(extensions),
                       [](const std::string& ext) { return ext.c_str(); });
        const std::vector<std::string> graphicsExtensions = m_graphicsPlugin->GetInstanceExtensions();
        std::transform(graphicsExtensions.begin(), graphicsExtensions.end(), std::back_inserter(extensions),
                       [](const std::string& ext) { return ext.c_str(); });

        uint32_t instanceExtensionCount;
        CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(nullptr, 0, &instanceExtensionCount, nullptr));
        std::vector<XrExtensionProperties> extensionProperties =
            std::vector<XrExtensionProperties>(instanceExtensionCount, {XR_TYPE_EXTENSION_PROPERTIES});
        CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(nullptr, (uint32_t)extensionProperties.size(), &instanceExtensionCount,
                                                           extensionProperties.data()));

        // enable depth extension if supported
        auto depthExtensionProperties =
            std::find_if(extensionProperties.begin(), extensionProperties.end(), [](const XrExtensionProperties& item) {
                return 0 == strcmp(item.extensionName, XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
            });
        if (depthExtensionProperties != extensionProperties.end()) {
            Log::Write(Log::Level::Info, Fmt("Depth submission supported (%s)", depthExtensionProperties->extensionName));
            extensions.push_back(depthExtensionProperties->extensionName);
            m_supportsDepthLayer = true;
        } else {
            Log::Write(Log::Level::Info, Fmt("Depth submission NOT supported (%s)", XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME));
        }

        XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
        createInfo.next = m_platformPlugin->GetInstanceCreateExtension();
        createInfo.enabledExtensionCount = (uint32_t)extensions.size();
        createInfo.enabledExtensionNames = extensions.data();

        strcpy(createInfo.applicationInfo.applicationName, "HelloXR");

        // Current version is 1.1.x, but hello_xr only requires 1.0.x
        createInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0;

        CHECK_XRCMD(xrCreateInstance(&createInfo, &m_instance));
    }

    void CreateInstance() override {
        LogLayersAndExtensions();

        CreateInstanceInternal();

        LogInstanceInfo();
    }

    void LogViewConfigurations() {
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_systemId != XR_NULL_SYSTEM_ID);

        uint32_t viewConfigTypeCount;
        CHECK_XRCMD(xrEnumerateViewConfigurations(m_instance, m_systemId, 0, &viewConfigTypeCount, nullptr));
        std::vector<XrViewConfigurationType> viewConfigTypes(viewConfigTypeCount);
        CHECK_XRCMD(xrEnumerateViewConfigurations(m_instance, m_systemId, viewConfigTypeCount, &viewConfigTypeCount,
                                                  viewConfigTypes.data()));
        CHECK((uint32_t)viewConfigTypes.size() == viewConfigTypeCount);

        Log::Write(Log::Level::Info, Fmt("Available View Configuration Types: (%d)", viewConfigTypeCount));
        for (XrViewConfigurationType viewConfigType : viewConfigTypes) {
            Log::Write(Log::Level::Verbose, Fmt("  View Configuration Type: %s %s", to_string(viewConfigType),
                                                viewConfigType == m_viewConfigType ? "(Selected)" : ""));

            XrViewConfigurationProperties viewConfigProperties{XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
            CHECK_XRCMD(xrGetViewConfigurationProperties(m_instance, m_systemId, viewConfigType, &viewConfigProperties));

            Log::Write(Log::Level::Verbose,
                       Fmt("  View configuration FovMutable=%s", viewConfigProperties.fovMutable == XR_TRUE ? "True" : "False"));

            uint32_t viewCount;
            CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, viewConfigType, 0, &viewCount, nullptr));
            if (viewCount > 0) {
                std::vector<XrViewConfigurationView> views(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
                CHECK_XRCMD(
                    xrEnumerateViewConfigurationViews(m_instance, m_systemId, viewConfigType, viewCount, &viewCount, views.data()));

                for (uint32_t i = 0; i < views.size(); i++) {
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

        Log::Write(Log::Level::Info, Fmt("  Available Environment Blend Mode count : (%d)", count));

        std::vector<XrEnvironmentBlendMode> blendModes(count);
        CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(m_instance, m_systemId, type, count, &count, blendModes.data()));

        for (XrEnvironmentBlendMode mode : blendModes) {
            const bool blendModeMatch = (mode == m_blendMode);
            Log::Write(Log::Level::Info,
                       Fmt("    Environment Blend Mode (%s) : %s", to_string(mode), blendModeMatch ? "(Selected)" : ""));
        }
    }

    std::array<float, 4> GetBackgroundClearColor() const override {
        static const std::array<float, 4> SlateGrey{{0.184313729f, 0.309803933f, 0.309803933f, 1.0f}};
        static const std::array<float, 4> TransparentBlack{{0.0f, 0.0f, 0.0f, 0.0f}};
        static const std::array<float, 4> Black{{0.0f, 0.0f, 0.0f, 1.0f}};

        switch (m_blendMode) {
            case XR_ENVIRONMENT_BLEND_MODE_OPAQUE:
                return SlateGrey;
            case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE:
                return Black;
            case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND:
                return TransparentBlack;
            default:
                return SlateGrey;
        }
    }

    void InitializeSystem(XrFormFactor formFactor, XrViewConfigurationType viewConfigType, bool ebmOverride,
                          XrEnvironmentBlendMode ebm) override {
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_systemId == XR_NULL_SYSTEM_ID);

        XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
        systemInfo.formFactor = formFactor;
        CHECK_XRCMD(xrGetSystem(m_instance, &systemInfo, &m_systemId));

        Log::Write(Log::Level::Verbose, Fmt("Using system %d for form factor %s", m_systemId, to_string(formFactor)));
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_systemId != XR_NULL_SYSTEM_ID);

        {
            // Note: If this condition is not met, the project will need to be audited
            // to see how support should be added.
            CHECK_MSG(viewConfigType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO ||
                          viewConfigType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                      "Unsupported view configuration type");

            m_viewConfigType = viewConfigType;
        }

        {
            uint32_t count;
            CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(m_instance, m_systemId, m_viewConfigType, 0, &count, nullptr));
            CHECK(count > 0);
            std::vector<XrEnvironmentBlendMode> blendModes(count);
            CHECK_XRCMD(
                xrEnumerateEnvironmentBlendModes(m_instance, m_systemId, m_viewConfigType, count, &count, blendModes.data()));
            if (ebmOverride) {
                if (std::find(blendModes.begin(), blendModes.end(), ebm) == blendModes.end()) {
                    THROW("Selected blendmode is not available from runtime");
                }
                m_blendMode = ebm;
            } else {
                bool isBounded = false;
                XrSystemSpatialContainerPropertiesEXT spatialContainerProperties{XR_TYPE_SYSTEM_SPATIAL_CONTAINER_PROPERTIES_EXT};
                XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
                systemProperties.next = &spatialContainerProperties;
                if (XR_SUCCEEDED(xrGetSystemProperties(m_instance, m_systemId, &systemProperties))) {
                    if (spatialContainerProperties.supportsBounded && spatialContainerProperties.supportsImmersive) {
                        isBounded = true;
                    }
                }
                if (isBounded) {
                    auto alphaIt = std::find(blendModes.begin(), blendModes.end(), XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND);
                    auto additiveIt = std::find(blendModes.begin(), blendModes.end(), XR_ENVIRONMENT_BLEND_MODE_ADDITIVE);
                    if (alphaIt != blendModes.end()) {
                        m_blendMode = XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
                    } else if (additiveIt != blendModes.end()) {
                        m_blendMode = XR_ENVIRONMENT_BLEND_MODE_ADDITIVE;
                    } else {
                        m_blendMode = blendModes[0];
                    }
                } else {
                    m_blendMode = blendModes[0];
                }
            }
        }
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
        XrAction quitAction{XR_NULL_HANDLE};
        XrAction toggleBoundsModeAction{XR_NULL_HANDLE};
        std::array<XrPath, Side::COUNT> handSubactionPath;
        std::array<float, Side::COUNT> handScale = {{1.0f, 1.0f}};
        std::array<XrBool32, Side::COUNT> handActive;
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
            XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};

            // Create input actions for quitting the session using the left and right controller.
            // Since it doesn't matter which hand did this, we do not specify subaction paths for it.
            // We will just suggest bindings for both hands, where possible.
            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "quit_session");
            strcpy_s(actionInfo.localizedActionName, "Quit Session");
            actionInfo.countSubactionPaths = 0;
            actionInfo.subactionPaths = nullptr;
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.quitAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "toggle_bounds_mode");
            strcpy_s(actionInfo.localizedActionName, "Toggle Bounds Mode");
            actionInfo.countSubactionPaths = 0;
            actionInfo.subactionPaths = nullptr;
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.toggleBoundsModeAction));
        }

        std::array<XrPath, Side::COUNT> selectPath;
        std::array<XrPath, Side::COUNT> menuClickPath;
        std::array<XrPath, Side::COUNT> bClickPath;
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/select/click", &selectPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/select/click", &selectPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/menu/click", &menuClickPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/menu/click", &menuClickPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/b/click", &bClickPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/b/click", &bClickPath[Side::RIGHT]));
        // Suggest bindings for KHR Simple.
        {
            XrPath khrSimpleInteractionProfilePath;
            CHECK_XRCMD(
                xrStringToPath(m_instance, "/interaction_profiles/khr/simple_controller", &khrSimpleInteractionProfilePath));
            std::vector<XrActionSuggestedBinding> bindings{{{m_input.toggleBoundsModeAction, selectPath[Side::LEFT]},
                                                            {m_input.toggleBoundsModeAction, menuClickPath[Side::RIGHT]}}};
            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedBindings.interactionProfile = khrSimpleInteractionProfilePath;
            suggestedBindings.suggestedBindings = bindings.data();
            suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
            CHECK_XRCMD(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
        }
        // Suggest bindings for the Oculus Touch.
        {
            XrPath oculusTouchInteractionProfilePath;
            CHECK_XRCMD(
                xrStringToPath(m_instance, "/interaction_profiles/oculus/touch_controller", &oculusTouchInteractionProfilePath));
            std::vector<XrActionSuggestedBinding> bindings{{{m_input.toggleBoundsModeAction, menuClickPath[Side::LEFT]}}};
            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedBindings.interactionProfile = oculusTouchInteractionProfilePath;
            suggestedBindings.suggestedBindings = bindings.data();
            suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
            CHECK_XRCMD(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
        }
        // Suggest bindings for the Vive Controller.
        {
            XrPath viveControllerInteractionProfilePath;
            CHECK_XRCMD(
                xrStringToPath(m_instance, "/interaction_profiles/htc/vive_controller", &viveControllerInteractionProfilePath));
            std::vector<XrActionSuggestedBinding> bindings{{{m_input.toggleBoundsModeAction, menuClickPath[Side::LEFT]},
                                                            {m_input.toggleBoundsModeAction, menuClickPath[Side::RIGHT]}}};
            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedBindings.interactionProfile = viveControllerInteractionProfilePath;
            suggestedBindings.suggestedBindings = bindings.data();
            suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
            CHECK_XRCMD(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
        }

        // Suggest bindings for the Valve Index Controller.
        {
            XrPath indexControllerInteractionProfilePath;
            CHECK_XRCMD(
                xrStringToPath(m_instance, "/interaction_profiles/valve/index_controller", &indexControllerInteractionProfilePath));
            std::vector<XrActionSuggestedBinding> bindings{{{m_input.toggleBoundsModeAction, bClickPath[Side::LEFT]},
                                                            {m_input.toggleBoundsModeAction, bClickPath[Side::RIGHT]}}};
            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedBindings.interactionProfile = indexControllerInteractionProfilePath;
            suggestedBindings.suggestedBindings = bindings.data();
            suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
            CHECK_XRCMD(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
        }

        // Suggest bindings for the Microsoft Mixed Reality Motion Controller.
        {
            XrPath microsoftMixedRealityInteractionProfilePath;
            CHECK_XRCMD(xrStringToPath(m_instance, "/interaction_profiles/microsoft/motion_controller",
                                       &microsoftMixedRealityInteractionProfilePath));
            std::vector<XrActionSuggestedBinding> bindings{{{m_input.toggleBoundsModeAction, menuClickPath[Side::LEFT]},
                                                            {m_input.toggleBoundsModeAction, menuClickPath[Side::RIGHT]}}};
            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedBindings.interactionProfile = microsoftMixedRealityInteractionProfilePath;
            suggestedBindings.suggestedBindings = bindings.data();
            suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
            CHECK_XRCMD(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
        }

        XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attachInfo.countActionSets = 1;
        attachInfo.actionSets = &m_input.actionSet;
        CHECK_XRCMD(xrAttachSessionActionSets(m_session, &attachInfo));
    }

    void InitializeSession([[maybe_unused]] std::string appSpace /* This sample always uses the container space */) override {
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_session == XR_NULL_HANDLE);

        {
            Log::Write(Log::Level::Verbose, Fmt("Creating session..."));

            XrSessionCreateInfoSpatialContainersEXT spatialContainersCreateInfo{XR_TYPE_SESSION_CREATE_INFO_SPATIAL_CONTAINERS_EXT};
            spatialContainersCreateInfo.next = m_graphicsPlugin->GetGraphicsBinding();
            XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
            createInfo.next = &spatialContainersCreateInfo;
            createInfo.systemId = m_systemId;
            CHECK_XRCMD(xrCreateSession(m_instance, &createInfo, &m_session));
        }

        LogReferenceSpaces();
        InitializeActions();

        {
            uint32_t spaceCount;
            CHECK_XRCMD(xrEnumerateReferenceSpaces(m_session, 0, &spaceCount, nullptr));
            std::vector<XrReferenceSpaceType> spaces(spaceCount);
            CHECK_XRCMD(xrEnumerateReferenceSpaces(m_session, spaceCount, &spaceCount, spaces.data()));
            bool stageSupported = std::find(spaces.begin(), spaces.end(), XR_REFERENCE_SPACE_TYPE_STAGE) != spaces.end();

            XrReferenceSpaceCreateInfo refSpaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            refSpaceInfo.referenceSpaceType = stageSupported ? XR_REFERENCE_SPACE_TYPE_STAGE : XR_REFERENCE_SPACE_TYPE_LOCAL;
            refSpaceInfo.poseInReferenceSpace = Math::Pose::Identity();
            CHECK_XRCMD(xrCreateReferenceSpace(m_session, &refSpaceInfo, &m_container2ReferenceSpace));
        }

        uint32_t maxSpatialContainerCount = 0;
        {
            XrSystemSpatialContainerPropertiesEXT spatialContainerProperties{XR_TYPE_SYSTEM_SPATIAL_CONTAINER_PROPERTIES_EXT};
            XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
            systemProperties.next = &spatialContainerProperties;
            CHECK_XRCMD(xrGetSystemProperties(m_instance, m_systemId, &systemProperties));
            Log::Write(Log::Level::Info,
                       Fmt("System Spatial Container Properties: maxCount=%u supportsBounded=%d supportsImmersive=%d",
                           spatialContainerProperties.maxSpatialContainerCount, spatialContainerProperties.supportsBounded,
                           spatialContainerProperties.supportsImmersive));
            maxSpatialContainerCount = spatialContainerProperties.maxSpatialContainerCount;
            m_supportsBounded = spatialContainerProperties.supportsBounded == XR_TRUE;
            m_supportsImmersive = spatialContainerProperties.supportsImmersive == XR_TRUE;

            if (m_supportsBounded) {
                m_lastContainerBoundsMode = XR_SPATIAL_CONTAINER_BOUNDS_MODE_BOUNDED_EXT;
            } else if (m_supportsImmersive) {
                m_lastContainerBoundsMode = XR_SPATIAL_CONTAINER_BOUNDS_MODE_IMMERSIVE_EXT;
            } else {
                Log::Write(Log::Level::Warning, "Neither Immersive nor Bounded spatial containers are supported.");
            }
        }

        CHECK_XRCMD(xrGetInstanceProcAddr(m_instance, "xrCreateSpatialContainerEXT",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&xrCreateSpatialContainerEXT)));
        CHECK_XRCMD(xrGetInstanceProcAddr(m_instance, "xrDestroySpatialContainerEXT",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&xrDestroySpatialContainerEXT)));
        CHECK_XRCMD(xrGetInstanceProcAddr(m_instance, "xrCreateSpatialContainerSpaceEXT",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&xrCreateSpatialContainerSpaceEXT)));
        CHECK_XRCMD(xrGetInstanceProcAddr(m_instance, "xrRequestSpatialContainerVisibleEXT",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&xrRequestSpatialContainerVisibleEXT)));
        CHECK_XRCMD(xrGetInstanceProcAddr(m_instance, "xrRequestSpatialContainerBoundsModeEXT",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&xrRequestSpatialContainerBoundsModeEXT)));
        CHECK_XRCMD(xrGetInstanceProcAddr(m_instance, "xrGetSpatialContainerBoundsEXT",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&xrGetSpatialContainerBoundsEXT)));
        CHECK_XRCMD(xrGetInstanceProcAddr(m_instance, "xrGetSpatialContainerStateEXT",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&xrGetSpatialContainerStateEXT)));
        CHECK_XRCMD(xrGetInstanceProcAddr(m_instance, "xrBeginSpatialContainerRenderingEXT",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&xrBeginSpatialContainerRenderingEXT)));
        CHECK_XRCMD(xrGetInstanceProcAddr(m_instance, "xrEndSpatialContainerRenderingEXT",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&xrEndSpatialContainerRenderingEXT)));
        CHECK_XRCMD(xrGetInstanceProcAddr(m_instance, "xrLocateSpatialContainerViewsEXT",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&xrLocateSpatialContainerViewsEXT)));

        uint32_t numContainersToCreate = std::min((uint32_t)m_containers.size(), maxSpatialContainerCount);
        for (uint32_t i = 0; i < numContainersToCreate; i++) {
            m_lastContainerIndex = i;
            auto& container = m_containers[i];
            auto& containerSpace = m_containerSpaces[i];
            {
                XrSpatialContainerCreateInfoEXT createInfo{XR_TYPE_SPATIAL_CONTAINER_CREATE_INFO_EXT};
                createInfo.graphicsPresentation = XR_SPATIAL_CONTAINER_GRAPHICS_PRESENTATION_SELF_RENDERING_EXT;
                createInfo.suggestedBounds = m_containerBounds[i];

                CHECK_XRCMD(xrCreateSpatialContainerEXT(m_session, &createInfo, &container));
            }
            {
                XrSpatialContainerSpaceCreateInfoEXT createInfo{XR_TYPE_SPATIAL_CONTAINER_SPACE_CREATE_INFO_EXT};
                createInfo.spatialContainer = container;

                CHECK_XRCMD(xrCreateSpatialContainerSpaceEXT(m_session, &createInfo, &containerSpace));
            }

            // Setup bounds mode BEFORE making visible
            if (!m_supportsBounded) {
                // If bounded is unsupported, ALL containers must be IMMERSIVE
                XrSpatialContainerBoundsModeRequestInfoEXT boundsModeInfo{XR_TYPE_SPATIAL_CONTAINER_BOUNDS_MODE_REQUEST_INFO_EXT};
                boundsModeInfo.boundsMode = XR_SPATIAL_CONTAINER_BOUNDS_MODE_IMMERSIVE_EXT;
                CHECK_XRCMD(xrRequestSpatialContainerBoundsModeEXT(container, &boundsModeInfo));
            } else if (i == 2 && m_lastContainerBoundsMode != XR_SPATIAL_CONTAINER_BOUNDS_MODE_BOUNDED_EXT) {
                // For container 2, if we want it to be something else (IMMERSIVE), request it
                XrSpatialContainerBoundsModeRequestInfoEXT boundsModeInfo{XR_TYPE_SPATIAL_CONTAINER_BOUNDS_MODE_REQUEST_INFO_EXT};
                boundsModeInfo.boundsMode = m_lastContainerBoundsMode;
                CHECK_XRCMD(xrRequestSpatialContainerBoundsModeEXT(container, &boundsModeInfo));
            }

            {
                XrSpatialContainerVisibleRequestInfoEXT info{XR_TYPE_SPATIAL_CONTAINER_VISIBLE_REQUEST_INFO_EXT};
                info.visible = XR_TRUE;

                CHECK_XRCMD(xrRequestSpatialContainerVisibleEXT(container, &info));
            }
        }
    }

    void CreateSwapchains() override {
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

        // Query and cache view configuration views.
        uint32_t viewCount;
        CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_viewConfigType, 0, &viewCount, nullptr));
        m_configViews.resize(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_viewConfigType, viewCount, &viewCount,
                                                      m_configViews.data()));

        // Create and cache view buffer for xrLocateViews later.
        m_views.resize(viewCount, {XR_TYPE_VIEW});

        // Create the swapchain and get the images.
        if (viewCount > 0) {
            // Select a swapchain format.
            uint32_t swapchainFormatCount;
            CHECK_XRCMD(xrEnumerateSwapchainFormats(m_session, 0, &swapchainFormatCount, nullptr));
            std::vector<int64_t> swapchainFormats(swapchainFormatCount);
            CHECK_XRCMD(xrEnumerateSwapchainFormats(m_session, (uint32_t)swapchainFormats.size(), &swapchainFormatCount,
                                                    swapchainFormats.data()));
            CHECK(swapchainFormatCount == swapchainFormats.size());
            m_colorSwapchainFormat = m_graphicsPlugin->SelectColorSwapchainFormat(true, swapchainFormats);
            m_depthSwapchainFormat = m_graphicsPlugin->SelectDepthSwapchainFormat(false, swapchainFormats);

            if (m_depthSwapchainFormat == -1) {
                Log::Write(Log::Level::Info,
                           "Runtime does not support creating swapchains with a suitable depth format. Using our own  fallback "
                           "depth textures!");
                // can't submit our own fallback texture
                m_supportsDepthLayer = false;
            }

            // Print swapchain formats and the selected ones.
            {
                std::string swapchainFormatsString;
                for (int64_t format : swapchainFormats) {
                    const bool selected = (format == m_colorSwapchainFormat || format == m_depthSwapchainFormat);
                    swapchainFormatsString += ' ';
                    if (selected) {
                        swapchainFormatsString += '[';
                    }
                    swapchainFormatsString += std::to_string(format);
                    if (selected) {
                        swapchainFormatsString += ']';
                    }
                }
                Log::Write(Log::Level::Verbose, Fmt("Swapchain Formats: %s", swapchainFormatsString.c_str()));
            }

            // Create a swapchain for each view.
            for (uint32_t i = 0; i < viewCount * kMaxLayers; i++) {
                const XrViewConfigurationView& vp = m_configViews[i % viewCount];
                Log::Write(Log::Level::Info,
                           Fmt("Creating color %s swapchain for view %d with dimensions Width=%d Height=%d SampleCount=%d",
                               m_depthSwapchainFormat == -1 ? "" : "and depth", i, vp.recommendedImageRectWidth,
                               vp.recommendedImageRectHeight, vp.recommendedSwapchainSampleCount));

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

                if (m_depthSwapchainFormat != -1) {
                    XrSwapchainCreateInfo depthSwapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
                    depthSwapchainCreateInfo.arraySize = 1;
                    depthSwapchainCreateInfo.format = m_depthSwapchainFormat;
                    depthSwapchainCreateInfo.width = vp.recommendedImageRectWidth;
                    depthSwapchainCreateInfo.height = vp.recommendedImageRectHeight;
                    depthSwapchainCreateInfo.mipCount = 1;
                    depthSwapchainCreateInfo.faceCount = 1;
                    depthSwapchainCreateInfo.sampleCount = m_graphicsPlugin->GetSupportedSwapchainSampleCount(vp);
                    depthSwapchainCreateInfo.usageFlags =
                        XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
                    Swapchain depthSwapchain;
                    depthSwapchain.width = depthSwapchainCreateInfo.width;
                    depthSwapchain.height = depthSwapchainCreateInfo.height;
                    CHECK_XRCMD(xrCreateSwapchain(m_session, &depthSwapchainCreateInfo, &depthSwapchain.handle));

                    m_depthSwapchains.push_back(depthSwapchain);

                    uint32_t depthImageCount;
                    CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, 0, &depthImageCount, nullptr));

                    // TODO: support this
                    if (depthImageCount != imageCount) {
                        THROW("This runtime has different color and depth swapchain lengths");
                    }

                    ISwapchainImageData* swapchainImages = m_graphicsPlugin->AllocateSwapchainImageDataWithDepthSwapchain(
                        imageCount, swapchainCreateInfo, depthSwapchain.handle, depthSwapchainCreateInfo);
                    CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, imageCount, &imageCount,
                                                           swapchainImages->GetColorImageArray()));

                    CHECK_XRCMD(xrEnumerateSwapchainImages(depthSwapchain.handle, depthImageCount, &depthImageCount,
                                                           swapchainImages->GetDepthImageArray()));

                    m_swapchainImages.insert(std::make_pair(swapchain.handle, std::move(swapchainImages)));
                } else {
                    ISwapchainImageData* swapchainImages =
                        m_graphicsPlugin->AllocateSwapchainImageData(imageCount, swapchainCreateInfo);
                    CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, imageCount, &imageCount,
                                                           swapchainImages->GetColorImageArray()));

                    m_swapchainImages.insert(std::make_pair(swapchain.handle, std::move(swapchainImages)));
                }
            }
        }
    }

    // Return event if one is available, otherwise return null.
    const XrEventDataBaseHeader* TryReadNextEvent() {
        // It is sufficient to clear the just the XrEventDataBuffer header to
        // XR_TYPE_EVENT_DATA_BUFFER
        XrEventDataBaseHeader* baseHeader = reinterpret_cast<XrEventDataBaseHeader*>(&m_eventDataBuffer);
        *baseHeader = {XR_TYPE_EVENT_DATA_BUFFER};
        const XrResult xr = xrPollEvent(m_instance, &m_eventDataBuffer);
        if (xr == XR_SUCCESS) {
            if (baseHeader->type == XR_TYPE_EVENT_DATA_EVENTS_LOST) {
                const XrEventDataEventsLost* const eventsLost = reinterpret_cast<const XrEventDataEventsLost*>(baseHeader);
                Log::Write(Log::Level::Warning, Fmt("%d events lost", eventsLost->lostEventCount));
            }

            return baseHeader;
        }
        if (xr == XR_EVENT_UNAVAILABLE) {
            return nullptr;
        }
        THROW_XR(xr, "xrPollEvent");
    }

    void PollEvents(bool* exitRenderLoop, bool* requestRestart) override {
        *exitRenderLoop = *requestRestart = false;
        if (m_sessionState == XR_SESSION_STATE_EXITING) {
            *exitRenderLoop = true;
            return;
        }

        // Process all pending messages.
        while (const XrEventDataBaseHeader* event = TryReadNextEvent()) {
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
                case XR_TYPE_EVENT_DATA_SPATIAL_CONTAINER_VISIBLE_CHANGED_EXT: {
                    const auto& visibleChanged = *reinterpret_cast<const XrEventDataSpatialContainerVisibleChangedEXT*>(event);
                    Log::Write(Log::Level::Info, Fmt("XrEventDataSpatialContainerVisibleChangedEXT: container=%p visible=%d",
                                                     visibleChanged.spatialContainer, visibleChanged.visible));
                    for (uint32_t i = 0; i <= m_lastContainerIndex; ++i) {
                        if (m_containers[i] == visibleChanged.spatialContainer) {
                            m_containerVisible[i] = (visibleChanged.visible == XR_TRUE);
                            break;
                        }
                    }
                    break;
                }
                case XR_TYPE_EVENT_DATA_SPATIAL_CONTAINER_VISIBLE_REQUEST_DENIED_EXT: {
                    const auto& visibleRequestDenied =
                        *reinterpret_cast<const XrEventDataSpatialContainerVisibleRequestDeniedEXT*>(event);
                    Log::Write(Log::Level::Info, Fmt("XrEventDataSpatialContainerVisibleRequestDeniedEXT: container=%p",
                                                     visibleRequestDenied.spatialContainer));
                    break;
                }
                case XR_TYPE_EVENT_DATA_SPATIAL_CONTAINER_CLOSED_EXT: {
                    const auto& closed = *reinterpret_cast<const XrEventDataSpatialContainerClosedEXT*>(event);
                    Log::Write(Log::Level::Info,
                               Fmt("XrEventDataSpatialContainerClosedEXT: container=%p", closed.spatialContainer));
                    for (uint32_t i = 0; i <= m_lastContainerIndex; ++i) {
                        if (m_containers[i] == closed.spatialContainer) {
                            m_containerVisible[i] = false;
                            m_containerClosed[i] = true;
                            break;
                        }
                    }
                    break;
                }
                case XR_TYPE_EVENT_DATA_SPATIAL_CONTAINER_BOUNDS_CHANGED_EXT: {
                    const auto& modeChanged = *reinterpret_cast<const XrEventDataSpatialContainerBoundsChangedEXT*>(event);
                    Log::Write(Log::Level::Info, Fmt("XrEventDataSpatialContainerBoundsModeChangedEXT: container=%p boundsMode=%d",
                                                     modeChanged.spatialContainer, modeChanged.boundsMode));
                    if (modeChanged.spatialContainer == m_containers[2]) {
                        m_lastContainerBoundsMode = modeChanged.boundsMode;
                    }
                    break;
                }
                case XR_TYPE_EVENT_DATA_SPATIAL_CONTAINER_BOUNDS_MODE_REQUEST_DENIED_EXT: {
                    const auto& boundsModeRequestDenied =
                        *reinterpret_cast<const XrEventDataSpatialContainerBoundsModeRequestDeniedEXT*>(event);
                    Log::Write(Log::Level::Info, Fmt("XrEventDataSpatialContainerBoundsModeRequestDeniedEXT: container=%p",
                                                     boundsModeRequestDenied.spatialContainer));
                    break;
                }
                case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                    LogActionSourceName(m_input.quitAction, "Quit");
                    LogActionSourceName(m_input.toggleBoundsModeAction, "ToggleBoundsMode");
                    break;
                case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
                default: {
                    Log::Write(Log::Level::Verbose, Fmt("Ignoring event type %d", event->type));
                    break;
                }
            }
        }
    }

    void HandleSessionStateChangedEvent(const XrEventDataSessionStateChanged& stateChangedEvent, bool* exitRenderLoop,
                                        bool* requestRestart) {
        const XrSessionState oldState = m_sessionState;
        m_sessionState = stateChangedEvent.state;

        Log::Write(Log::Level::Info, Fmt("XrEventDataSessionStateChanged: state %s->%s session=%lld time=%lld", to_string(oldState),
                                         to_string(m_sessionState), stateChangedEvent.session, stateChangedEvent.time));

        if ((stateChangedEvent.session != XR_NULL_HANDLE) && (stateChangedEvent.session != m_session)) {
            Log::Write(Log::Level::Error, "XrEventDataSessionStateChanged for unknown session");
            return;
        }

        switch (m_sessionState) {
            case XR_SESSION_STATE_IDLE: {
                m_sessionRunning = true;
                break;
            }
            case XR_SESSION_STATE_READY: {
                m_sessionRunning = true;
                break;
            }
            case XR_SESSION_STATE_STOPPING: {
                m_sessionRunning = false;
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

    void LogActionSourceName(XrAction action, const std::string& actionName) const {
        XrBoundSourcesForActionEnumerateInfo getInfo = {XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
        getInfo.action = action;
        uint32_t pathCount = 0;
        CHECK_XRCMD(xrEnumerateBoundSourcesForAction(m_session, &getInfo, 0, &pathCount, nullptr));
        std::vector<XrPath> paths(pathCount);
        CHECK_XRCMD(xrEnumerateBoundSourcesForAction(m_session, &getInfo, uint32_t(paths.size()), &pathCount, paths.data()));

        std::string sourceName;
        for (uint32_t i = 0; i < pathCount; ++i) {
            constexpr XrInputSourceLocalizedNameFlags all = XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT |
                                                            XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT |
                                                            XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;

            XrInputSourceLocalizedNameGetInfo nameInfo = {XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO};
            nameInfo.sourcePath = paths[i];
            nameInfo.whichComponents = all;

            uint32_t size = 0;
            CHECK_XRCMD(xrGetInputSourceLocalizedName(m_session, &nameInfo, 0, &size, nullptr));
            if (size < 1) {
                continue;
            }
        }

        Log::Write(Log::Level::Info,
                   Fmt("%s action is bound to %s", actionName.c_str(), ((!sourceName.empty()) ? sourceName.c_str() : "nothing")));
    }

    bool IsSessionRunning() const override { return m_sessionRunning; }

    bool IsSessionFocused() const override {
        if (m_sessionState == XR_SESSION_STATE_IDLE) {
            for (auto container : m_containers) {
                if (container != XR_NULL_HANDLE) {
                    XrSpatialContainerStateEXT state{XR_TYPE_SPATIAL_CONTAINER_STATE_EXT};
                    XrSpatialContainerStateGetInfoEXT getInfo{XR_TYPE_SPATIAL_CONTAINER_STATE_GET_INFO_EXT};
                    if (XR_SUCCEEDED(xrGetSpatialContainerStateEXT(container, &getInfo, &state))) {
                        if (state.interactable) return true;
                    }
                }
            }
        }
        return false;
    }

    void PollActions() override {
        m_input.handActive = {{XR_FALSE, XR_FALSE}};

        // Sync actions
        const XrActiveActionSet activeActionSet{m_input.actionSet, XR_NULL_PATH};
        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        syncInfo.countActiveActionSets = 1;
        syncInfo.activeActionSets = &activeActionSet;
        CHECK_XRCMD(xrSyncActions(m_session, &syncInfo));

        // There were no subaction paths specified for the quit action, because we don't care which hand did it.
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr, m_input.quitAction, XR_NULL_PATH};
        XrActionStateBoolean quitValue{XR_TYPE_ACTION_STATE_BOOLEAN};
        CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &quitValue));
        if ((quitValue.isActive == XR_TRUE) && (quitValue.changedSinceLastSync == XR_TRUE) && (quitValue.currentState == XR_TRUE)) {
            m_sessionState = XR_SESSION_STATE_EXITING;
            m_sessionRunning = false;
        }

        getInfo.action = m_input.toggleBoundsModeAction;
        XrActionStateBoolean toggleValue{XR_TYPE_ACTION_STATE_BOOLEAN};
        if (XR_SUCCEEDED(xrGetActionStateBoolean(m_session, &getInfo, &toggleValue))) {
            if ((toggleValue.isActive == XR_TRUE) && (toggleValue.changedSinceLastSync == XR_TRUE) &&
                (toggleValue.currentState == XR_TRUE)) {
                if (m_containers[m_lastContainerIndex] != XR_NULL_HANDLE) {
                    XrSpatialContainerBoundsModeEXT desiredMode = m_lastContainerBoundsMode;
                    if (m_lastContainerBoundsMode == XR_SPATIAL_CONTAINER_BOUNDS_MODE_BOUNDED_EXT) {
                        if (m_supportsImmersive) {
                            desiredMode = XR_SPATIAL_CONTAINER_BOUNDS_MODE_IMMERSIVE_EXT;
                        } else {
                            Log::Write(Log::Level::Warning, "Cannot toggle to Immersive: not supported");
                        }
                    } else {
                        if (m_supportsBounded) {
                            desiredMode = XR_SPATIAL_CONTAINER_BOUNDS_MODE_BOUNDED_EXT;
                        } else {
                            Log::Write(Log::Level::Warning, "Cannot toggle to Bounded: not supported");
                        }
                    }

                    if (desiredMode != m_lastContainerBoundsMode) {
                        m_lastContainerBoundsMode = desiredMode;
                        Log::Write(Log::Level::Info, Fmt("Toggling Container 2 bounds mode to %d", m_lastContainerBoundsMode));
                        XrSpatialContainerBoundsModeRequestInfoEXT boundsModeInfo{
                            XR_TYPE_SPATIAL_CONTAINER_BOUNDS_MODE_REQUEST_INFO_EXT};
                        boundsModeInfo.boundsMode = m_lastContainerBoundsMode;
                        CHECK_XRCMD(xrRequestSpatialContainerBoundsModeEXT(m_containers[m_lastContainerIndex], &boundsModeInfo));
                    }
                }
            }
        }
    }

    void RenderFrame() override {
        CHECK(m_session != XR_NULL_HANDLE);

        XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        CHECK_XRCMD(xrWaitFrame(m_session, &frameWaitInfo, &frameState));

        for (uint32_t i = 0; i <= m_lastContainerIndex; ++i) {
            if (m_containerVisible[i] && !m_containerRendering[i]) {
                XrSpatialContainerBeginInfoEXT beginInfo{XR_TYPE_SPATIAL_CONTAINER_BEGIN_INFO_EXT};
                beginInfo.spatialContainer = m_containers[i];
                beginInfo.primaryViewConfigurationType = m_viewConfigType;
                CHECK_XRCMD(xrBeginSpatialContainerRenderingEXT(m_session, &beginInfo));
                m_containerRendering[i] = true;
            } else if (!m_containerVisible[i] && m_containerRendering[i]) {
                XrSpatialContainerEndInfoEXT endInfo{XR_TYPE_SPATIAL_CONTAINER_END_INFO_EXT};
                endInfo.spatialContainer = m_containers[i];
                CHECK_XRCMD(xrEndSpatialContainerRenderingEXT(m_session, &endInfo));
                m_containerRendering[i] = false;
            }
            if (m_containerClosed[i]) {
                CHECK_XRCMD(xrDestroySpatialContainerEXT(m_containers[i]));
                m_containers[i] = XR_NULL_HANDLE;
                m_containerClosed[i] = false;
            }
        }

        XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        CHECK_XRCMD(xrBeginFrame(m_session, &frameBeginInfo));

        std::vector<int> activeIndices;
        for (uint32_t i = 0; i <= m_lastContainerIndex; ++i) {
            if (m_containerRendering[i]) {
                activeIndices.push_back(i);
            }
        }

        struct LayerSubmission {
            int containerIndex;
            XrSpace space;
            int layerType;  // 0 = standard container cube, 1 = sea of cubes
            XrCompositionLayerProjection projectionLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
            XrSpatialContainerCompositionLayerViewConfigurationEXT containerViewConfig{
                XR_TYPE_SPATIAL_CONTAINER_COMPOSITION_LAYER_VIEW_CONFIGURATION_EXT};
            std::vector<XrCompositionLayerProjectionView> projectionLayerViews;
            std::vector<XrCompositionLayerDepthInfoKHR> depthInfos;
            bool rendered{false};
        };

        std::vector<LayerSubmission> submissions;
        for (uint32_t i = 0; i <= m_lastContainerIndex; ++i) {
            if (m_containerRendering[i]) {
                if (i == 2) {
                    LayerSubmission subSea;
                    subSea.containerIndex = i;
                    subSea.space = m_container2ReferenceSpace;
                    subSea.layerType = 1;
                    submissions.push_back(std::move(subSea));

                    LayerSubmission subMiddle;
                    subMiddle.containerIndex = i;
                    subMiddle.space = m_containerSpaces[i];
                    subMiddle.layerType = 0;
                    submissions.push_back(std::move(subMiddle));
                } else {
                    LayerSubmission sub0;
                    sub0.containerIndex = i;
                    sub0.space = m_containerSpaces[i];
                    sub0.layerType = 0;
                    submissions.push_back(std::move(sub0));
                }
            }
        }

        std::vector<XrSpatialContainerLayerVolumeClippingEXT> containerVolumeClippings;
        std::vector<XrSpatialContainerLayerEXT> containerLayers;
        std::array<std::vector<const XrCompositionLayerBaseHeader*>, kNumContainers> layerPointersPerContainer;

        if (!submissions.empty()) {
            uint32_t locateCount = (uint32_t)submissions.size();
            std::vector<XrSpatialContainerViewLocateInfoEXT> locateInfos(locateCount,
                                                                         {XR_TYPE_SPATIAL_CONTAINER_VIEW_LOCATE_INFO_EXT});
            for (uint32_t idx = 0; idx < locateCount; ++idx) {
                locateInfos[idx].viewConfigurationType = m_viewConfigType;
                locateInfos[idx].space = submissions[idx].space;
                locateInfos[idx].spatialContainer = m_containers[submissions[idx].containerIndex];
            }

            XrSpatialContainerViewsLocateInfoEXT locateInfo{XR_TYPE_SPATIAL_CONTAINER_VIEWS_LOCATE_INFO_EXT};
            locateInfo.displayTime = frameState.predictedDisplayTime;
            locateInfo.viewLocateInfoCount = locateCount;
            locateInfo.viewLocateInfos = locateInfos.data();

            std::vector<XrSpatialContainerViewStateEXT> viewStates(locateCount, {XR_TYPE_SPATIAL_CONTAINER_VIEW_STATE_EXT});
            uint32_t viewsPerConfig = (uint32_t)m_configViews.size();
            uint32_t totalViewCount = locateCount * viewsPerConfig;
            std::vector<XrView> locatedViews(totalViewCount, {XR_TYPE_VIEW});

            CHECK_XRCMD(xrLocateSpatialContainerViewsEXT(m_session, &locateInfo, locateCount, viewStates.data(), totalViewCount,
                                                         locatedViews.data()));

            for (uint32_t idx = 0; idx < locateCount; ++idx) {
                auto& sub = submissions[idx];
                int i = sub.containerIndex;
                auto& container = m_containers[i];
                auto& projectionLayer = sub.projectionLayer;
                auto& containerViewConfig = sub.containerViewConfig;
                auto& projectionLayerViews = sub.projectionLayerViews;
                auto& depthInfos = sub.depthInfos;
                std::vector<Swapchain> swapchains;
                swapchains.reserve(viewsPerConfig);
                for (uint32_t v = 0; v < viewsPerConfig; ++v) {
                    swapchains.push_back(m_swapchains[idx * viewsPerConfig + v]);
                }
                const XrSpatialContainerViewStateEXT& viewState = viewStates[idx];
                const XrView* containerViews = &locatedViews[idx * viewsPerConfig];

                if (viewState.shouldSubmitLayers == XR_TRUE && (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) != 0 &&
                    (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) != 0) {
                    projectionLayer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
                    containerViewConfig.type = XR_TYPE_SPATIAL_CONTAINER_COMPOSITION_LAYER_VIEW_CONFIGURATION_EXT;
                    containerViewConfig.viewConfigurationType = m_viewConfigType;
                    projectionLayer.next = &containerViewConfig;

                    if (RenderLayer(containerViews, viewState, projectionLayerViews, depthInfos, projectionLayer, swapchains,
                                    container, sub.space, (int)idx, sub.layerType, frameState.predictedDisplayTime)) {
                        sub.rendered = true;
                        layerPointersPerContainer[i].push_back(
                            reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projectionLayer));
                    }
                }
            }
        }

        for (uint32_t i = 0; i <= m_lastContainerIndex; ++i) {
            if (m_containerRendering[i]) {
                XrSpatialContainerLayerEXT containerLayer{XR_TYPE_SPATIAL_CONTAINER_LAYER_EXT};
                containerLayer.spatialContainer = m_containers[i];
                if (!layerPointersPerContainer[i].empty()) {
                    containerLayer.retainPreviousSubmission = XR_FALSE;
                    containerLayer.layerCount = (uint32_t)layerPointersPerContainer[i].size();
                    containerLayer.layers = layerPointersPerContainer[i].data();
                } else {
                    containerLayer.retainPreviousSubmission = XR_TRUE;
                    containerLayer.layerCount = 0;
                    containerLayer.layers = nullptr;
                }
                containerLayers.push_back(containerLayer);

                XrSpatialContainerLayerVolumeClippingEXT containerVolumeClipping{
                    XR_TYPE_SPATIAL_CONTAINER_LAYER_VOLUME_CLIPPING_EXT};
                if (i == 2) {
                    // Last container renders a "portal" effect
                    containerVolumeClipping.volumeClipping = XR_SPATIAL_CONTAINER_VOLUME_CLIPPING_NONE_EXT;
                } else {
                    // Other containers render only inside the container bounds
                    containerVolumeClipping.volumeClipping = XR_SPATIAL_CONTAINER_VOLUME_CLIPPING_STRICT_EXT;
                }
                containerVolumeClippings.push_back(containerVolumeClipping);

                containerLayers.back().next = &containerVolumeClippings.back();
            }
        }

        XrSpatialContainerLayerFrameEndInfoEXT spatialEndInfo{XR_TYPE_SPATIAL_CONTAINER_LAYER_FRAME_END_INFO_EXT};
        spatialEndInfo.containerLayerCount = (uint32_t)containerLayers.size();
        spatialEndInfo.containerLayers = containerLayers.data();

        XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
        frameEndInfo.next = &spatialEndInfo;
        frameEndInfo.displayTime = frameState.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = m_blendMode;
        frameEndInfo.layerCount = 0;
        CHECK_XRCMD(xrEndFrame(m_session, &frameEndInfo));
    }

    bool RenderLayer(
        const XrView* containerViews, const XrSpatialContainerViewStateEXT& viewState,
        std::vector<XrCompositionLayerProjectionView>& projectionLayerViews,
        std::vector<XrCompositionLayerDepthInfoKHR>& depthInfos, XrCompositionLayerProjection& layer,
        std::vector<Swapchain>& swapchains, XrSpatialContainerEXT container, XrSpace appSpace, int index, int layerType,
        [[maybe_unused]] XrTime
            predictedDisplayTime /* This sample renders a statically positioned duck, therefore display time is not needed */) {
        uint32_t viewCountOutput = (uint32_t)m_configViews.size();
        CHECK(viewCountOutput == swapchains.size());

        XrSpatialContainerStateEXT containerState{XR_TYPE_SPATIAL_CONTAINER_STATE_EXT};
        XrSpatialContainerStateGetInfoEXT getInfo{XR_TYPE_SPATIAL_CONTAINER_STATE_GET_INFO_EXT};
        CHECK_XRCMD(xrGetSpatialContainerStateEXT(container, &getInfo, &containerState));

        if (!containerState.visible) {
            return false;
        }

        projectionLayerViews.resize(viewCountOutput);
        if (m_supportsDepthLayer) {
            depthInfos.resize(viewCountOutput);
        }

        std::vector<Cube> cubes;

        if (layerType == 1) {
            for (float x = -2.5f; x <= 2.5f; x += 0.5f) {
                for (float z = -2.5f; z <= 2.5f; z += 0.5f) {
                    Cube c;
                    c.Pose.orientation = {0.0f, 0.0f, 0.0f, 1.0f};
                    c.Pose.position = {x, 0.0f, z};
                    c.Scale = {0.1f, 0.1f, 0.1f};
                    cubes.push_back(c);
                }
            }
        } else {
            XrSpatialContainerBoundsEXT containerBounds{XR_TYPE_SPATIAL_CONTAINER_BOUNDS_EXT};
            XrSpatialContainerBoundsGetInfoEXT getBoundsInfo{XR_TYPE_SPATIAL_CONTAINER_BOUNDS_GET_INFO_EXT};
            CHECK_XRCMD(xrGetSpatialContainerBoundsEXT(container, &getBoundsInfo, &containerBounds));

            XrExtent3Df bounds = containerBounds.bounds;
            if (bounds.width <= 0.0f) bounds.width = 0.5f;
            if (bounds.height <= 0.0f) bounds.height = 0.5f;
            if (bounds.depth <= 0.0f) bounds.depth = 0.5f;

            const float W = bounds.width;
            const float H = bounds.height;
            const float D = bounds.depth;
            const float w = W / 2.0f;
            const float h = H / 2.0f;
            const float d = D / 2.0f;

            const float smallestExtent = std::min({W, H, D});
            const float t = smallestExtent * 0.05f;
            const float centerSize = smallestExtent * 0.5f;

            auto addCube = [&](float x, float y, float z, float sx, float sy, float sz) {
                Cube c;
                c.Pose.orientation = {0.0f, 0.0f, 0.0f, 1.0f};
                c.Pose.position = {x, y, z};
                c.Scale = {sx, sy, sz};
                cubes.push_back(c);
            };

            auto duckCubes = scaledDuckCubes(centerSize);
            for (const auto& duckCube : duckCubes) {
                cubes.push_back(duckCube);
            }

            // Show the bounds of each container too.
            addCube(0.0f, h, d, W, t, t);
            addCube(0.0f, h, -d, W, t, t);
            addCube(0.0f, -h, d, W, t, t);
            addCube(0.0f, -h, -d, W, t, t);

            addCube(-w, 0.0f, d, t, H, t);
            addCube(w, 0.0f, d, t, H, t);
            addCube(-w, 0.0f, -d, t, H, t);
            addCube(w, 0.0f, -d, t, H, t);

            addCube(-w, h, 0.0f, t, t, D);
            addCube(w, h, 0.0f, t, t, D);
            addCube(-w, -h, 0.0f, t, t, D);
            addCube(w, -h, 0.0f, t, t, D);
        }

        for (uint32_t i = 0; i < viewCountOutput; i++) {
            const Swapchain viewSwapchain = swapchains[i];

            XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};

            uint32_t swapchainImageIndex;
            CHECK_XRCMD(xrAcquireSwapchainImage(viewSwapchain.handle, &acquireInfo, &swapchainImageIndex));

            XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            waitInfo.timeout = XR_INFINITE_DURATION;
            CHECK_XRCMD(xrWaitSwapchainImage(viewSwapchain.handle, &waitInfo));

            m_swapchainImages[viewSwapchain.handle]->AcquireAndWaitDepthSwapchainImage(swapchainImageIndex);

            projectionLayerViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            projectionLayerViews[i].pose = containerViews[i].pose;
            projectionLayerViews[i].fov = containerViews[i].fov;
            projectionLayerViews[i].subImage.swapchain = viewSwapchain.handle;
            projectionLayerViews[i].subImage.imageRect.offset = {0, 0};
            // The recommended image extent will always be less than the XrViewConfigurationView's
            // recommended extent. If the swapchain is created smaller than the recommended extent,
            // the render size will need to be clamped.
            projectionLayerViews[i].subImage.imageRect.extent = {viewState.recommendedImageExtent.width,
                                                                 viewState.recommendedImageExtent.height};

            if (m_supportsDepthLayer) {
                const uint32_t globalIndex = index * viewCountOutput + i;
                projectionLayerViews[i].next = &depthInfos[i];
                depthInfos[i].type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR;
                depthInfos[i].subImage.swapchain = m_depthSwapchains[globalIndex].handle;
                depthInfos[i].subImage.imageRect.offset = {0, 0};
                // Similar to above, if the size here is smaller than the recommended extent, the
                // render size will need to be clamped.
                depthInfos[i].subImage.imageRect.extent = {viewState.recommendedImageExtent.width,
                                                           viewState.recommendedImageExtent.height};
                depthInfos[i].minDepth = 0;
                depthInfos[i].maxDepth = 1;
                depthInfos[i].nearZ = 0.05f;
                depthInfos[i].farZ = 100.0f;
            }

            const XrSwapchainImageBaseHeader* const swapchainImage =
                m_swapchainImages[viewSwapchain.handle]->GetGenericColorImage(swapchainImageIndex);

            m_graphicsPlugin->RenderView(projectionLayerViews[i], swapchainImage, m_colorSwapchainFormat, cubes);

            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            CHECK_XRCMD(xrReleaseSwapchainImage(viewSwapchain.handle, &releaseInfo));

            m_swapchainImages[viewSwapchain.handle]->ReleaseDepthSwapchainImage();
        }

        layer.space = appSpace;
        layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        layer.viewCount = (uint32_t)projectionLayerViews.size();
        layer.views = projectionLayerViews.data();
        return true;
    }

   private:
    XrEnvironmentBlendMode m_blendMode{XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM};
    XrViewConfigurationType m_viewConfigType{XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM};

    std::shared_ptr<IPlatformPlugin> m_platformPlugin;
    std::shared_ptr<IGraphicsPlugin> m_graphicsPlugin;
    XrInstance m_instance{XR_NULL_HANDLE};
    XrSession m_session{XR_NULL_HANDLE};
    std::array<XrExtent3Df, kNumContainers> m_containerBounds{
        {XrExtent3Df{1.0f, 1.0f, 1.0f}, XrExtent3Df{1.0f, 1.0f, 0.025f}, XrExtent3Df{1.0f, 0.25f, 0.25f}}};
    XrSystemId m_systemId{XR_NULL_SYSTEM_ID};

    // We may still use a runtime allocated depth swapchain but not submit depth if false
    bool m_supportsDepthLayer{false};
    bool m_supportsBounded{false};
    bool m_supportsImmersive{false};

    // Arrays to hold container data
    std::array<XrSpace, kNumContainers> m_containerSpaces{};           // Initialize to null
    std::array<XrSpatialContainerEXT, kNumContainers> m_containers{};  // Initialize to null
    std::array<bool, kNumContainers> m_containerRendering{};
    std::array<bool, kNumContainers> m_containerVisible{};
    std::array<bool, kNumContainers> m_containerClosed{};

    // Spatial Container functions
    PFN_xrCreateSpatialContainerEXT xrCreateSpatialContainerEXT{};
    PFN_xrDestroySpatialContainerEXT xrDestroySpatialContainerEXT{};
    PFN_xrCreateSpatialContainerSpaceEXT xrCreateSpatialContainerSpaceEXT{};
    PFN_xrRequestSpatialContainerVisibleEXT xrRequestSpatialContainerVisibleEXT{};
    PFN_xrRequestSpatialContainerBoundsModeEXT xrRequestSpatialContainerBoundsModeEXT{};
    PFN_xrGetSpatialContainerBoundsEXT xrGetSpatialContainerBoundsEXT{};
    PFN_xrGetSpatialContainerStateEXT xrGetSpatialContainerStateEXT{};
    PFN_xrBeginSpatialContainerRenderingEXT xrBeginSpatialContainerRenderingEXT{};
    PFN_xrEndSpatialContainerRenderingEXT xrEndSpatialContainerRenderingEXT{};
    PFN_xrLocateSpatialContainerViewsEXT xrLocateSpatialContainerViewsEXT{};

    std::vector<XrViewConfigurationView> m_configViews;
    std::vector<Swapchain> m_swapchains;
    std::vector<Swapchain> m_depthSwapchains;
    std::map<XrSwapchain, ISwapchainImageData*> m_swapchainImages;
    std::vector<XrView> m_views;
    int64_t m_colorSwapchainFormat{-1};
    int64_t m_depthSwapchainFormat{-1};

    // Application's current lifecycle state according to the runtime
    XrSessionState m_sessionState{XR_SESSION_STATE_UNKNOWN};
    bool m_sessionRunning{false};

    XrSpace m_container2ReferenceSpace{XR_NULL_HANDLE};
    uint32_t m_lastContainerIndex{0};
    XrSpatialContainerBoundsModeEXT m_lastContainerBoundsMode{XR_SPATIAL_CONTAINER_BOUNDS_MODE_BOUNDED_EXT};
    XrEventDataBuffer m_eventDataBuffer;
    InputState m_input;
};
}  // namespace

std::shared_ptr<IOpenXrProgram> CreateOpenXrProgram_SpatialContainers(const std::shared_ptr<IPlatformPlugin>& platformPlugin,
                                                                      const std::shared_ptr<IGraphicsPlugin>& graphicsPlugin) {
    return std::make_shared<OpenXrProgram>(platformPlugin, graphicsPlugin);
}
