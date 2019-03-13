// Copyright (c) 2017-2019 The Khronos Group Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author:
//

//        Issues:
//
//        * To preserve the CreateInfo for objects, we have to make a deep copy.
//          But copying these structures requires casting to remove const.
//          This is a bit icky, but I'm not sure there's an obviously better solution.
//
//        * Sample impl defines opaque structs that include pointers to api-specifc impl
//          data and functions.
//
//        * For internal functions and types, xr_ and Xr_ prefixes are used to distinguish
//          from external ones.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>
#include <time.h>

#include "impl/openxr_sample_impl.h"
#include "impl/si_linear.h"

// This file contains a table of all OpenXR function pointers (PFNs).
// It is generated at build time from the registry to keep it
// up to date with the API definition in the registry.
// TODO(karl): We can probably do a better job handling generated files,
//             by perhaps writing them all to a generated folder.
//             Then we can use more useful paths in places like this one,
//             and avoid the lint message.
//             But defer this change to the CMake cleanup.
#include "xr_generated_dispatch_table.h"  // NOLINT(build/include)

// Loader Interface definitions
#include "common/loader_interfaces.h"

// ImplXrGetInstanceProcAddr:
//
// * ImplXrGetInstanceProcAddr is implemented with full code generation
//   in the file "xr_generated_sample_impl.cpp.".
// * Define and export the function table (ipa_table)
//   to the generated ImplXrGetInstanceProcAddr(), which uses it to
//   fetch proc addresses for the functions.
// * "InitIpaTable()" function fills in this function table.

// TODO(karl): XrGeneratedDispatchTable is a bad choice for this structure name because
//             it violates the Xr namespace.  Changing this forces a change in a lot
//             of the loader code, so best done in another MR.
#ifdef USE_OPENXR_LOADER
extern "C" {
XRAPI_ATTR XrResult XRAPI_CALL ImplxrGetInstanceProcAddr(XrInstance instance, const char *name, PFN_xrVoidFunction *function);
struct XrGeneratedDispatchTable ipa_table;
}
#endif

// Use this to avoid redefining API functions, which would duplicate those
// exported by the loader.
// TODO(karl): This is probably better done by controlling the exporting of the
//             API functions, rather than renaming them to avoid symbol conflicts.
//             Might define a "XRAPI_EXPORT" macro for decorating function
//             declarations, which I think is how GL does it.
//             It is still a macro, but much less ugly than this.
//             (Note that the renaming doesn't conform to the Style Guide, but this is temporary.)
//             Defer to a later cleanup step.
#ifdef USE_OPENXR_LOADER
#define PROC_WRAPPER(x) Impl##x
#else
#define PROC_WRAPPER(x) x
#endif

static XrApplicationInfo DupApplicationInfo(const XrApplicationInfo &appInfo) {
    XrApplicationInfo ai = appInfo;
    strncpy(ai.applicationName, appInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE);
    strncpy(ai.engineName, appInfo.engineName, XR_MAX_ENGINE_NAME_SIZE);
    ai.applicationName[XR_MAX_APPLICATION_NAME_SIZE - 1] = '\0';
    ai.engineName[XR_MAX_ENGINE_NAME_SIZE - 1] = '\0';
    return ai;
}

static void FreeApplicationInfo(const XrApplicationInfo &ai) {}

static XrInstanceCreateInfo *DupInstanceCreateInfo(const XrInstanceCreateInfo *instanceCreateInfo) {
    XrInstanceCreateInfo *ci = new XrInstanceCreateInfo(*instanceCreateInfo);
    ci->next = NULL;  // TODO(cass): traversal fun
    ci->applicationInfo = DupApplicationInfo(ci->applicationInfo);

    char **eln = new char *[ci->enabledApiLayerCount];
    for (uint32_t i = 0; i < ci->enabledApiLayerCount; i++) {
        eln[i] = strdup(ci->enabledApiLayerNames[i]);
    }
    ci->enabledApiLayerNames = eln;

    char **een = new char *[ci->enabledExtensionCount];
    for (uint32_t i = 0; i < ci->enabledExtensionCount; i++) {
        een[i] = strdup(ci->enabledExtensionNames[i]);
    }
    ci->enabledExtensionNames = een;
    return ci;
}

static void FreeInstanceCreateInfo(XrInstanceCreateInfo *ci) {
    FreeApplicationInfo(ci->applicationInfo);

    for (uint32_t i = 0; i < ci->enabledApiLayerCount; i++) {
        free((void *)ci->enabledApiLayerNames[i]);  // NOLINT(readability/casting)
    }
    delete[] ci->enabledApiLayerNames;

    for (uint32_t i = 0; i < ci->enabledExtensionCount; i++) {
        free((void *)ci->enabledExtensionNames[i]);  // NOLINT(readability/casting)
    }
    delete[] ci->enabledExtensionNames;

    delete ci;
}

static XrSessionCreateInfo *DupSessionCreateInfo(const XrSessionCreateInfo *sessionCreateInfo) {
    XrSessionCreateInfo *ci = new XrSessionCreateInfo(*sessionCreateInfo);
    ci->next = NULL;  // TODO(cass):
    return ci;
}

static XrSwapchainCreateInfo *DupSwapchainCreateInfo(const XrSwapchainCreateInfo *imageCreateInfo) {
    XrSwapchainCreateInfo *ci = new XrSwapchainCreateInfo(*imageCreateInfo);
    ci->next = NULL;
    return ci;
}

static void FreeSwapchainCreateInfo(XrSwapchainCreateInfo *ci) {
    // TODO(cass): free next chain
    delete ci;
}

static XrReferenceSpaceCreateInfo *DupReferenceSpaceCreateInfo(const XrReferenceSpaceCreateInfo *referenceSpaceCreateInfo) {
    XrReferenceSpaceCreateInfo *ci = new XrReferenceSpaceCreateInfo(*referenceSpaceCreateInfo);
    ci->next = NULL;
    return ci;
}

static void FreeReferenceSpaceCreateInfo(XrReferenceSpaceCreateInfo *ci) {
    // TODO(cass): free next chain
    delete ci;
}

static XrSessionBeginInfo *DupSessionBeginInfo(const XrSessionBeginInfo *sessionBeginInfo) {
    XrSessionBeginInfo *ci = new XrSessionBeginInfo(*sessionBeginInfo);
    ci->next = NULL;
    return ci;
}

static void FreeSessionBeginInfo(XrSessionBeginInfo *sessionBeginInfo) { delete sessionBeginInfo; }

static System *GetDefaultSystemForFormFactor(Instance *inst, XrFormFactor formFactor) {
    for (unsigned int i = 0; i < inst->systems.size(); i++) {
        if (inst->systems[i].formFactor == formFactor) {
            return &inst->systems[i];
        }
    }
    return NULL;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrEnumerateApiLayerProperties)(uint32_t propertyCapacityInput,
                                                                           uint32_t *propertyCountOutput,
                                                                           XrApiLayerProperties *properties) {
    printf("xrEnumerateApiLayerProperties\n");
    if (propertyCapacityInput == 0) {
        *propertyCountOutput = 0;
        return XR_SUCCESS;
    }
    // populate outputs here
    *propertyCountOutput = 0;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrEnumerateInstanceExtensionProperties)(const char *layerName,
                                                                                    uint32_t propertyCapacityInput,
                                                                                    uint32_t *propertyCountOutput,
                                                                                    XrExtensionProperties *properties) {
    printf("xrEnumerateInstanceExtensionProperties\n");
    if (layerName != NULL) {
        return XR_ERROR_API_LAYER_NOT_PRESENT;
    }

    if (propertyCapacityInput == 0) {
        *propertyCountOutput = 1;
        return XR_SUCCESS;
    }
    // populate outputs here
    *propertyCountOutput = 1;
    strncpy(properties[0].extensionName, XR_KHR_OPENGL_ENABLE_EXTENSION_NAME, XR_MAX_EXTENSION_NAME_SIZE);
    properties[0].specVersion = 1;
    return XR_SUCCESS;
}

XrPath Instance::StringToPath(const std::string &s) {
    auto it = std::find(atom_table.begin(), atom_table.end(), s);
    if (it == atom_table.end()) {
        atom_table.push_back(s);
        it = atom_table.end() - 1;
    }
    return it - atom_table.begin();
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrCreateInstance)(const XrInstanceCreateInfo *info, XrInstance *instance) {
    printf("xrCreateInstance\n");
    if (info == NULL) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    Instance *inst = new Instance;

    // Make sure that the constructor was run.
    if (!inst->IsValid()) {
        delete inst;
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    inst->create_info = DupInstanceCreateInfo(info);
    System sys;
    sys.instance = (XrInstance)inst;
    sys.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    inst->systems.push_back(sys);
    *instance = (XrInstance)inst;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrDestroyInstance)(XrInstance instance) {
    printf("xrDestroyInstance\n");
    Instance *inst = reinterpret_cast<Instance *>(instance);
    if (inst == NULL) {
        return XR_SUCCESS;
    }
    FreeInstanceCreateInfo(inst->create_info);
    delete inst;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrGetInstanceProperties)(XrInstance instance,
                                                                     XrInstanceProperties *instanceProperties) {
    printf("xrGetInstanceProperties\n");
    Instance *inst = reinterpret_cast<Instance *>(instance);
    if (inst == NULL || !inst->IsValid()) {
        return XR_ERROR_HANDLE_INVALID;
    }
    if (instanceProperties == NULL || instanceProperties->type != XR_TYPE_INSTANCE_PROPERTIES || instanceProperties->next != NULL) {
        return XR_ERROR_VALIDATION_FAILURE;
    }
    // TODO: Just tie the runtime version to the API version for now.
    instanceProperties->runtimeVersion = XR_CURRENT_API_VERSION;
    strcpy(instanceProperties->runtimeName, "OpenXR Sample Runtime Implementation");
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrStringToPath)(XrInstance instance, const char *path_string, XrPath *path) {
    Instance *inst = reinterpret_cast<Instance *>(instance);
    if (!inst->IsValid()) {
        return XR_ERROR_HANDLE_INVALID;
    }
    *path = inst->StringToPath(path_string);
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrPathToString)(XrInstance instance, XrPath path, uint32_t buffer_capacity_input,
                                                            uint32_t *buffer_count_output, char *buffer) {
    Instance *inst = reinterpret_cast<Instance *>(instance);
    if (!inst->IsValid()) {
        return XR_ERROR_HANDLE_INVALID;
    }
    if (path >= inst->atom_table.size()) {
        return XR_ERROR_PATH_INVALID;
    }
    const std::string &s = inst->atom_table[static_cast<std::size_t>(path)];
    if (buffer_capacity_input == 0) {
        *buffer_count_output = (uint32_t)s.size() + 1;
        return XR_SUCCESS;
    }
    if (s.size() + 1 > buffer_capacity_input) {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }
    memcpy(buffer, s.c_str(), s.size());
    buffer[s.size()] = 0;
    *buffer_count_output = (uint32_t)s.size() + 1;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrGetSystem)(XrInstance instance, const XrSystemGetInfo *getInfo,
                                                         XrSystemId *systemId) {
    printf("xrGetSystem\n");
    Instance *inst = reinterpret_cast<Instance *>(instance);
    if (!inst->IsValid()) {
        return XR_ERROR_HANDLE_INVALID;
    }

    System *sys = GetDefaultSystemForFormFactor(inst, getInfo->formFactor);
    if (NULL == sys) {
        return XR_ERROR_FORM_FACTOR_UNAVAILABLE;
    }

    XrGlSetSystemProcs(sys);

    *systemId = (XrSystemId)sys;

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrGetSystemProperties)(XrInstance instance, XrSystemId systemId,
                                                                   XrSystemProperties *properties) {
    printf("xrGetSystemProperties\n");
    System *sys = reinterpret_cast<System *>(systemId);
    if (NULL == sys) {
        return XR_ERROR_SYSTEM_INVALID;
    }
    if (NULL != properties) {
        properties->type = XR_TYPE_SYSTEM_PROPERTIES;
        properties->next = NULL;
        properties->vendorId = 0xBAADC0DE;
        properties->systemId = UINT64_C(0xDECAFBADDEADBEEF);
        strcpy(properties->systemName, "Sample Implementation System");
        properties->graphicsProperties = sys->graphics.props;
    }

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrGetOpenGLGraphicsRequirementsKHR)(
    XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsOpenGLKHR *graphicsRequirements) {
    printf("xrGetOpenGLGraphicsRequirementsKHR\n");
    Instance *inst = reinterpret_cast<Instance *>(instance);
    if (!inst->IsValid()) {
        return XR_ERROR_HANDLE_INVALID;
    }

    if (graphicsRequirements == NULL) {
        return XR_ERROR_VALIDATION_FAILURE;
    }

    graphicsRequirements->minApiVersionSupported = XR_MAKE_VERSION(3, 0, 0);
    graphicsRequirements->maxApiVersionSupported = XR_MAKE_VERSION(4, 5, 0);

    return XR_SUCCESS;
}

struct Xr_StructureBase {
    XrStructureType type;
    const void *next;
};

template <typename T>
static const Xr_StructureBase *find_in_chain(const T *chain, XrStructureType type) {
    const Xr_StructureBase *el = reinterpret_cast<const Xr_StructureBase *>(chain);
    while (el) {
        if (el->type == type) {
            return el;
        }
        el = reinterpret_cast<const Xr_StructureBase *>(el->next);
    }
    return nullptr;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrCreateSession)(XrInstance instance, const XrSessionCreateInfo *info,
                                                             XrSession *session) {
    printf("xrCreateSession\n");
    Instance *inst = reinterpret_cast<Instance *>(instance);
    if (!inst->IsValid()) {
        return XR_ERROR_HANDLE_INVALID;
    }

    if (info == NULL) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // allocate device object
    Session *sess = new Session;
    sess->create_info = DupSessionCreateInfo(info);
    sess->instance = inst;
    sess->system = reinterpret_cast<System *>(info->systemId);
    sess->begin_info = NULL;

    sess->interpupillaryDistance = 0.04f;
    sess->viewConfiguration = 0;

    sess->implementation_data = NULL;

    if (find_in_chain(info, XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR) != nullptr ||
        find_in_chain(info, XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR) != nullptr ||
        find_in_chain(info, XR_TYPE_GRAPHICS_BINDING_OPENGL_XCB_KHR) != nullptr ||
        find_in_chain(info, XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR) != nullptr) {
        XrGlSetSessionProcs(sess);
    } else {
        delete sess;
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    sess->spaces.push_back(XR_REFERENCE_SPACE_TYPE_VIEW);
    sess->spaces.push_back(XR_REFERENCE_SPACE_TYPE_LOCAL);
    sess->spaces.push_back(XR_REFERENCE_SPACE_TYPE_STAGE);
    XrResult result = sess->create_session(instance, info, (XrSession *)&sess);
    if (result != XR_SUCCESS) {
        delete sess;
        return result;
    }

    *session = (XrSession)sess;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrDestroySession)(XrSession session) {
    printf("xrDestroySession\n");
    Session *sess = reinterpret_cast<Session *>(session);
    XrResult result = sess->destroy_session(session);
    delete sess;
    return result;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrEnumerateViewConfigurations)(XrInstance instance, XrSystemId systemId,
                                                                           uint32_t viewConfigurationTypeCapacityInput,
                                                                           uint32_t *viewConfigurationTypeCountOutput,
                                                                           XrViewConfigurationType *viewConfigurationTypes) {
    printf("xrEnumerateViewConfigurations\n");
    System *sys = reinterpret_cast<System *>(systemId);
    XrResult result = sys->enumerate_view_configurations(instance, systemId, viewConfigurationTypeCapacityInput,
                                                         viewConfigurationTypeCountOutput, viewConfigurationTypes);
    return result;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrGetViewConfigurationProperties)(XrInstance instance, XrSystemId systemId,
                                                                              XrViewConfigurationType viewConfigurationType,
                                                                              XrViewConfigurationProperties *configuration) {
    printf("xrGetViewConfigurationProperties\n");
    System *sys = reinterpret_cast<System *>(systemId);
    XrResult result = sys->get_view_configuration_properties(instance, systemId, viewConfigurationType, configuration);
    return result;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrEnumerateViewConfigurationViews)(XrInstance instance, XrSystemId systemId,
                                                                               XrViewConfigurationType viewConfigurationType,
                                                                               uint32_t viewCountInput, uint32_t *viewCountOutput,
                                                                               XrViewConfigurationView *views) {
    printf("xrGetViewConfigurationProperties\n");
    System *sys = reinterpret_cast<System *>(systemId);
    XrResult result =
        sys->enumerate_view_configuration_views(instance, systemId, viewConfigurationType, viewCountInput, viewCountOutput, views);
    return result;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrEnumerateSwapchainFormats)(XrSession session, uint32_t formatCapacityInput,
                                                                         uint32_t *formatCountOutput, int64_t *formats) {
    printf("xrEnumerateSwapchainFormats\n");
    Session *sess = reinterpret_cast<Session *>(session);
    return sess->enumerate_swapchain_formats(session, formatCapacityInput, formatCountOutput, formats);
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrCreateSwapchain)(XrSession session, const XrSwapchainCreateInfo *info,
                                                               XrSwapchain *swapchain) {
    printf("xrCreateSwapchain\n");
    Swapchain *swch = new Swapchain;
    swch->session = reinterpret_cast<Session *>(session);
    swch->create_info = DupSwapchainCreateInfo(info);
    swch->implementation_data = NULL;
    *swapchain = (XrSwapchain)swch;
    Session *sess = reinterpret_cast<Session *>(session);
    sess->set_swapchain_procs(swch);
    return sess->create_swapchain(session, info, swapchain);
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrDestroySwapchain)(XrSwapchain swapchain) {
    printf("xrDestroySwapchain\n");
    Swapchain *swch = reinterpret_cast<Swapchain *>(swapchain);
    FreeSwapchainCreateInfo(swch->create_info);
    XrResult result = swch->session->destroy_swapchain(swapchain);
    delete swch;
    return result;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrEnumerateSwapchainImages)(XrSwapchain swapchain, uint32_t imageCapacityInput,
                                                                        uint32_t *imageCountOutput,
                                                                        XrSwapchainImageBaseHeader *images) {
    // printf( "xrEnumerateSwapchainImages\n" );
    Swapchain *swch = reinterpret_cast<Swapchain *>(swapchain);
    return swch->get_swapchain_images(swapchain, imageCapacityInput, imageCountOutput, images);
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrAcquireSwapchainImage)(XrSwapchain swapchain,
                                                                     const XrSwapchainImageAcquireInfo *acquireInfo,
                                                                     uint32_t *index) {
    // printf( "xrAcquireSwapchainImage\n" );
    Swapchain *swch = reinterpret_cast<Swapchain *>(swapchain);
    return swch->acquire_swapchain_image(swapchain, acquireInfo, index);
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrWaitSwapchainImage)(XrSwapchain swapchain, const XrSwapchainImageWaitInfo *waitInfo) {
    // printf( "xrWaitSwapchainImage\n" );
    Swapchain *swch = reinterpret_cast<Swapchain *>(swapchain);
    return swch->wait_swapchain_image(swapchain, waitInfo);
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrReleaseSwapchainImage)(XrSwapchain swapchain,
                                                                     const XrSwapchainImageReleaseInfo *releaseInfo) {
    // printf( "xrReleaseSwapchainImage\n" );
    Swapchain *swch = reinterpret_cast<Swapchain *>(swapchain);
    return swch->release_swapchain_image(swapchain, releaseInfo);
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrEnumerateReferenceSpaces)(XrSession session, uint32_t spaceCapacityInput,
                                                                        uint32_t *spaceCountOutput, XrReferenceSpaceType *spaces) {
    printf("xrEnumerateReferenceSpaces\n");
    Session *sess = reinterpret_cast<Session *>(session);

    if (spaceCapacityInput == 0) {
        *spaceCountOutput = (uint32_t)sess->spaces.size();
        return XR_SUCCESS;
    }

    // populate outputs here
    if (spaces != NULL) {
        uint32_t copyCount = spaceCapacityInput;
        if (spaceCapacityInput > (uint32_t)sess->spaces.size()) {
            copyCount = (uint32_t)sess->spaces.size();
        }
        for (uint32_t i = 0; i < copyCount; i++) {
            spaces[i] = sess->spaces[i];
        }
        *spaceCountOutput = copyCount;
    } else {
        *spaceCountOutput = 0;
    }

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrCreateReferenceSpace)(XrSession session, const XrReferenceSpaceCreateInfo *createInfo,
                                                                    XrSpace *space) {
    printf("xrCreateReferenceSpace\n");
    Session *sess = reinterpret_cast<Session *>(session);
    // TODO: This should add a space to the list in session
    sess->space = new Space;  // To do: Check for pre-existence of the space.
    sess->space->session = session;
    sess->space->create_info = DupReferenceSpaceCreateInfo(createInfo);
    *space = (XrSpace)sess->space;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrDestroySpace)(XrSpace space) {
    printf("xrDestroySpace\n");
    Space *spc = reinterpret_cast<Space *>(space);
    FreeReferenceSpaceCreateInfo(spc->create_info);
    Session *sess = reinterpret_cast<Session *>(spc->session);
    sess->space = NULL;
    delete spc;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrBeginSession)(XrSession session, const XrSessionBeginInfo *beginInfo) {
    printf("xrBeginSession\n");
    Session *sess = reinterpret_cast<Session *>(session);

    if (sess->begin_info)  // If already begun...
        return XR_ERROR_SESSION_RUNNING;

    // We support only the VR view configuration here.
    if (beginInfo->primaryViewConfigurationType != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
        return XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;

    sess->begin_info = DupSessionBeginInfo(beginInfo);
    sess->begin_frame_allowed = true;
    return sess->begin_session(session, beginInfo);
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrEndSession)(XrSession session) {
    printf("xrEndSession\n");
    Session *sess = reinterpret_cast<Session *>(session);
    XrResult result = sess->end_session(session);
    FreeSessionBeginInfo(sess->begin_info);
    sess->begin_info = NULL;
    return result;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrLocateViews)(XrSession session, const XrViewLocateInfo *viewLocateInfo,
                                                           XrViewState *viewState, uint32_t viewCapacityInput,
                                                           uint32_t *viewCountOutput, XrView *views) {
    // printf( "xrLocateViews\n" );
    // Apply the coordinateSystem's reference pose here.
    Session *sess = reinterpret_cast<Session *>(session);

    const XrVector3f axis = {0.0f, 0.0f, -1.0f};
    XrQuaternionf rot;
    XrQuaternionf_CreateFromAxisAngle(&rot, &axis,
                                      static_cast<float>((static_cast<double>(viewLocateInfo->displayTime)) * 1.0e-10));

    if (viewCapacityInput < 2) {
        if (viewCountOutput) *viewCountOutput = 2;
        return XR_ERROR_SIZE_INSUFFICIENT;
    }
    *viewCountOutput = 2;

    views[0].pose = {};
    views[0].pose.orientation = rot;
    views[0].pose.position.x = -sess->interpupillaryDistance / 2.0f;
    views[0].pose.position.y = 0.f;
    views[0].pose.position.z = 0.f;

    views[1].pose = {};
    views[1].pose.orientation = rot;
    views[1].pose.position.x = sess->interpupillaryDistance / 2.0f;
    views[1].pose.position.y = 0.f;
    views[1].pose.position.z = 0.f;

    // Apply space transform here.
    // To do: We need to implement reference space support in this sample in order to validate space.
    // if (space is invalid)
    //     return XR_ERROR_HANDLE_INVALID;
    (void)viewLocateInfo->space;

    for (int eye = 0; eye < 2; ++eye) {
        views[eye].fov.angleLeft = MATH_PI / 4;
        views[eye].fov.angleRight = MATH_PI / 4;
        views[eye].fov.angleUp = MATH_PI / 4;
        views[eye].fov.angleDown = MATH_PI / 4;
    }

    viewState->viewStateFlags = 0;

    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrWaitFrame)(XrSession session, const XrFrameWaitInfo *frameWaitInfo,
                                                         XrFrameState *frameState) {
    // printf( "xrWaitFrame\n" );
    Session *sess = reinterpret_cast<Session *>(session);
    sess->display_timing.WaitForNextFrame();
    if (frameState && frameState->type == XR_TYPE_FRAME_STATE) {
        frameState->predictedDisplayTime = sess->display_timing.next;
        frameState->predictedDisplayPeriod = (uint64_t)sess->display_timing.period;
    }
    return sess->wait_frame(session, frameWaitInfo, frameState);
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrBeginFrame)(XrSession session, const XrFrameBeginInfo *frameBeginInfo) {
    // printf( "xrBeginFrame\n" );
    Session *sess = reinterpret_cast<Session *>(session);
    if (sess->begin_frame_allowed == false) {
        return XR_ERROR_CALL_ORDER_INVALID;
    }
    XrResult ret = sess->begin_frame(session, frameBeginInfo);
    sess->begin_frame_allowed = false;
    return ret;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrEndFrame)(XrSession session, const XrFrameEndInfo *frameEndInfo) {
    // printf( "xrEndFrame\n" );
    Session *sess = reinterpret_cast<Session *>(session);
    if (sess->begin_frame_allowed) {
        return XR_ERROR_CALL_ORDER_INVALID;
    }
    XrResult ret = sess->end_frame(session, frameEndInfo);
    sess->begin_frame_allowed = true;
    return ret;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrApplyHapticFeedback)(XrAction hapticAction, uint32_t countSubactionPaths,
                                                                   const XrPath *subactionPaths,
                                                                   const XrHapticBaseHeader *hapticEvent) {
    printf("xrApplyHapticFeedback\n");
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrConvertTimeToTimespecTimeKHR)(XrInstance instance, XrTime time,
                                                                            struct timespec *timespecTime) {
    // Our implementation of runtime time (GetTimeNanosecondsXr) implements runtime time to be the same as
    // system time. Thus the conversion from one to the other is simply a copy as-is. A production runtime
    // may find it to be a good idea to make these different so that overly clever developers don't attempt
    // to omit the conversion operation because they assume this equality is portable.
    timespecTime->tv_sec = (time / 1000000000LL);
    timespecTime->tv_nsec = (time % 1000000000LL);
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrConvertTimespecTimeToTimeKHR)(XrInstance instance,
                                                                            const struct timespec *timespecTime, XrTime *time) {
    *time = ((timespecTime->tv_sec * 1000000000ULL) + timespecTime->tv_nsec);
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrPollEvent)(XrInstance instance, XrEventDataBuffer *eventBuffer,
                                                         XrEventDataBaseHeader *eventData) {
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL PROC_WRAPPER(xrLocateSpace)(XrSpace space, XrSpace baseSpace, XrTime time,
                                                           XrSpaceRelation *relation) {
    return XR_SUCCESS;
}

#ifdef USE_OPENXR_LOADER
static void InitIpaTable() {
    memset(&ipa_table, 0, sizeof(ipa_table));
    // Set only the functions that are implemented.
    ipa_table.GetInstanceProcAddr = ImplxrGetInstanceProcAddr;
    ipa_table.EnumerateInstanceExtensionProperties = ImplxrEnumerateInstanceExtensionProperties;
    ipa_table.CreateInstance = ImplxrCreateInstance;
    ipa_table.DestroyInstance = ImplxrDestroyInstance;
    ipa_table.GetInstanceProperties = ImplxrGetInstanceProperties;
    ipa_table.GetSystem = ImplxrGetSystem;
    ipa_table.GetSystemProperties = ImplxrGetSystemProperties;
    ipa_table.CreateSession = ImplxrCreateSession;
    ipa_table.DestroySession = ImplxrDestroySession;
    ipa_table.EnumerateViewConfigurations = ImplxrEnumerateViewConfigurations;
    ipa_table.GetViewConfigurationProperties = ImplxrGetViewConfigurationProperties;
    ipa_table.EnumerateViewConfigurationViews = ImplxrEnumerateViewConfigurationViews;
    ipa_table.EnumerateSwapchainFormats = ImplxrEnumerateSwapchainFormats;
    ipa_table.CreateSwapchain = ImplxrCreateSwapchain;
    ipa_table.DestroySwapchain = ImplxrDestroySwapchain;
    ipa_table.EnumerateSwapchainImages = ImplxrEnumerateSwapchainImages;
    ipa_table.AcquireSwapchainImage = ImplxrAcquireSwapchainImage;
    ipa_table.WaitSwapchainImage = ImplxrWaitSwapchainImage;
    ipa_table.ReleaseSwapchainImage = ImplxrReleaseSwapchainImage;
    ipa_table.EnumerateReferenceSpaces = ImplxrEnumerateReferenceSpaces;
    ipa_table.CreateReferenceSpace = ImplxrCreateReferenceSpace;
    ipa_table.DestroySpace = ImplxrDestroySpace;
    ipa_table.BeginSession = ImplxrBeginSession;
    ipa_table.EndSession = ImplxrEndSession;
    ipa_table.LocateViews = ImplxrLocateViews;
    ipa_table.WaitFrame = ImplxrWaitFrame;
    ipa_table.BeginFrame = ImplxrBeginFrame;
    ipa_table.EndFrame = ImplxrEndFrame;
    ipa_table.ApplyHapticFeedback = ImplxrApplyHapticFeedback;
    ipa_table.StringToPath = ImplxrStringToPath;
    ipa_table.PathToString = ImplxrPathToString;
    ipa_table.GetOpenGLGraphicsRequirementsKHR = ImplxrGetOpenGLGraphicsRequirementsKHR;
}
#endif

#ifdef USE_OPENXR_LOADER
// This function must be exported so that the loader can find it.
// The loader calls this function after loading the runtime to pass interface
// version information back and forth and to pass the xrGetInstanceProcAddr PFN
// back to the loader.  The loader then calls xrGetInstanceProcAddr to discover the
// PFNs for the rest of the API.
// This API is part of the loader API and intentionally uses the XR API prefixes.
extern "C" {
XRAPI_ATTR XrResult XRAPI_CALL xrNegotiateLoaderRuntimeInterface(const XrNegotiateLoaderInfo *loaderInfo,
                                                                 XrNegotiateRuntimeRequest *runtimeRequest) {
    // Make sure that the incoming struct is OK.
    if (loaderInfo->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
        sizeof(XrNegotiateLoaderInfo) != loaderInfo->structSize || loaderInfo->structVersion != XR_LOADER_INFO_STRUCT_VERSION) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Make sure that the incoming struct is OK.
    if (runtimeRequest->structType != XR_LOADER_INTERFACE_STRUCT_RUNTIME_REQUEST ||
        sizeof(XrNegotiateRuntimeRequest) != runtimeRequest->structSize ||
        runtimeRequest->structVersion != XR_RUNTIME_INFO_STRUCT_VERSION) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // This runtime implementation supports Loader-Runtime Interface Version 1.
    const uint32_t kMyLoaderRuntimeVersion = 1;

    // Make sure that our supported interface version is within the range
    // that the loader supports.
    if (kMyLoaderRuntimeVersion < loaderInfo->minInterfaceVersion || kMyLoaderRuntimeVersion > loaderInfo->maxInterfaceVersion) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    // Initialize the "Get Instance Proc Addr" table.
    InitIpaTable();

    // All seems good.  Return the requested info to the loader.
    runtimeRequest->getInstanceProcAddr = ImplxrGetInstanceProcAddr;
    runtimeRequest->runtimeInterfaceVersion = kMyLoaderRuntimeVersion;
    runtimeRequest->runtimeXrVersion = XR_CURRENT_API_VERSION;

    return XR_SUCCESS;
}
}
#endif
