// Copyright (c) 2017 The Khronos Group Inc.
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

#ifndef SRC_IMPL_SI_DEV_OPENGL_H_
#define SRC_IMPL_SI_DEV_OPENGL_H_

#include <thread>              // NOLINT(build/c++11)
#include <mutex>               // NOLINT(build/c++11)
#include <condition_variable>  // NOLINT(build/c++11)

#include "common/gfxwrapper_opengl.h"
#include "impl/openxr_sample_impl.h"

XrResult XRAPI_CALL XrGlCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session);
XrResult XRAPI_CALL XrGlDestroySession(XrSession session);

XrResult XRAPI_CALL XrGlEnumerateViewConfigurations(XrInstance instance, XrSystemId systemId, uint32_t capacityInput,
                                                    uint32_t* countOutput, XrViewConfigurationType* viewConfigTypes);
XrResult XRAPI_CALL XrGlGetViewConfiguration(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigType,
                                             XrViewConfigurationProperties* configuration);
XrResult XRAPI_CALL XrGlEnumerateViewConfigurationViews(XrInstance instance, XrSystemId systemId,
                                                        XrViewConfigurationType viewConfigType, uint32_t inputCapability,
                                                        uint32_t* outputCount, XrViewConfigurationView* views);
XrResult XRAPI_CALL XrGlSetSupportedViewConfigurations(XrSession session, uint32_t configurationCount,
                                                       const XrViewConfigurationProperties* viewConfigurations);

XrResult XRAPI_CALL XrGlEnumerateSwapchainFormats(XrSession session, uint32_t format_capacity_input, uint32_t* format_count_output,
                                                  int64_t* format_array);
XrResult XRAPI_CALL XrGlCreateSwapchain(XrSession session, const XrSwapchainCreateInfo* info, XrSwapchain* swapchain);
XrResult XRAPI_CALL XrGlDestroySwapchain(XrSwapchain swapchain);

XrResult XRAPI_CALL XrGlGetSwapchainImages(XrSwapchain swapchain, uint32_t imageCapacityInput, uint32_t* imageCountOutput,
                                           XrSwapchainImageBaseHeader* images);
XrResult XRAPI_CALL XrGlAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo,
                                              uint32_t* index);
XrResult XRAPI_CALL XrGlWaitSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageWaitInfo* waitInfo);
XrResult XRAPI_CALL XrGlReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo);

XrResult XRAPI_CALL XrGlBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo);
XrResult XRAPI_CALL XrGlEndSession(XrSession session);

XrResult XRAPI_CALL XrGlWaitFrame(XrSession session, const XrFrameWaitInfo* wait_frame_info, XrFrameState* frame_state);
XrResult XRAPI_CALL XrGlBeginFrame(XrSession session, const XrFrameBeginInfo* begin_frame_info);
XrResult XRAPI_CALL XrGlEndFrame(XrSession session, const XrFrameEndInfo* end_frame_info);

struct XrGlCompositorDistortionMesh {
    GLuint vao;
    GLuint vbo;
};

struct XrGlCompositorProgram {
    GLuint prog;
    GLuint vert;
    GLuint frag;
};

struct XrGlViewConfigurationVrDualViews {
    XrViewConfigurationProperties config;
    XrViewConfigurationView views[2];
};

struct XrGlSession {
    XrGlSession() : width(0), height(0), allow_composite(true), shutdown(false) {}

    int width;
    int height;

    std::thread compositor_thread;
    std::mutex mutex;
    std::condition_variable cond;
    bool allow_composite;
    bool shutdown;

    XrFrameEndInfo end_frame_info;
    // TODO(cass broke my glasses): Handle the implicit chain index better.
    uint32_t chain_indices[4][2];

    ksGpuWindow compositor_window;
    ksGpuContext app_context;
    ksGpuWindowInput* input;

    XrGlCompositorDistortionMesh distortion_mesh;
    XrGlCompositorProgram comp_prog;
    GLuint dummy_vao;
};

struct XrGlSwapchain {
    explicit XrGlSwapchain(const XrSwapchainCreateInfo* info, Swapchain* swapchain) {
        target = GL_TEXTURE_2D;
        swapchain->implementation_data = this;
        if (info->createFlags & XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT) {
            chain_length = 1;
        } else {
            chain_length = 3;
        }
        chain_index_acquire = 0;
        chain_index_release = 0;
        tex = new GLuint[chain_length];
        glGenTextures(chain_length, tex);
        for (GLuint i = 0; i < chain_length; i++) {
            glBindTexture(target, tex[i]);
            glTexStorage2D(target, swapchain->create_info->mipCount, GL_RGBA8, swapchain->create_info->width,
                           swapchain->create_info->height);
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
    }
    ~XrGlSwapchain() {
        // TODO(cass): Make sure all in-flight frame submissions using this swapchain have been retired
        glDeleteTextures(chain_length, tex);
        delete[] tex;
    }

    XrResult GetImages(uint32_t capacityInput, uint32_t* countOutput, XrSwapchainImageBaseHeader* images) {
        if (capacityInput > 0 && capacityInput < chain_length) {
            return XR_ERROR_SIZE_INSUFFICIENT;
        }
        *countOutput = chain_length;
        if (capacityInput == 0) {
            return XR_SUCCESS;
        }
        if (images[0].type == XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR) {
            XrSwapchainImageOpenGLKHR* sc_images_opengl = (XrSwapchainImageOpenGLKHR*)images;
            for (int i = 0; i < (int)chain_length; i++) {
                if (sc_images_opengl[i].type != XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR) {
                    return XR_ERROR_VALIDATION_FAILURE;
                }
                sc_images_opengl[i].image = tex[i];
            }
        }
        return XR_SUCCESS;
    }

    uint32_t Next(uint32_t index) { return (index + 1) % chain_length; }

    uint32_t Prev(uint32_t index) { return (index + chain_length - 1) % chain_length; }

    void Acquire(uint32_t* index) {
        *index = chain_index_acquire;
        chain_index_acquire = Next(chain_index_acquire);
    }

    void Wait(XrDuration /* timeout */) {
        // wait on chain_index_release
        // no impl yet
    }

    void Release() { chain_index_release = Next(chain_index_release); }

    GLenum target;
    GLuint chain_length;
    GLuint chain_index_acquire;
    GLuint chain_index_release;
    GLuint* tex;
};

#endif  // SRC_IMPL_SI_DEV_OPENGL_H_
