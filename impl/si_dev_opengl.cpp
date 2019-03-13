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

// TODO(cass): list:
// * provide "real" pose predictions
// * create a "real" distortion mesh - more verts, with barrel distortion
// * support all the layer types
// * composite in a separate context
// * do real async timewarp (separate thread)
// * handle compositor timing and stats
// We need to consider how conformance tests are going to work.

#define XR_USE_GRAPHICS_API_OPENGL 1
#include <stdio.h>
#include "impl/si_dev_opengl.h"
#include "impl/si_linear.h"
#include "common/gfxwrapper_opengl.h"

// VS 2013 and earlier don't expose C++11 alignof, so use MS version
#if defined(_MSC_VER) && (_MSC_VER <= 1800)
#define alignof __alignof
#endif

void XRAPI_CALL XrGlSetSystemProcs(System *sys) {
    sys->enumerate_view_configurations = XrGlEnumerateViewConfigurations;
    sys->get_view_configuration_properties = XrGlGetViewConfiguration;
    sys->enumerate_view_configuration_views = XrGlEnumerateViewConfigurationViews;
}

void XRAPI_CALL XrGlSetSwapchainProcs(Swapchain *swch) {
    swch->get_swapchain_images = XrGlGetSwapchainImages;
    swch->acquire_swapchain_image = XrGlAcquireSwapchainImage;
    swch->wait_swapchain_image = XrGlWaitSwapchainImage;
    swch->release_swapchain_image = XrGlReleaseSwapchainImage;
}

void XRAPI_CALL XrGlSetSessionProcs(Session *sess) {
    sess->create_session = XrGlCreateSession;
    sess->destroy_session = XrGlDestroySession;

    sess->enumerate_swapchain_formats = XrGlEnumerateSwapchainFormats;
    sess->create_swapchain = XrGlCreateSwapchain;
    sess->destroy_swapchain = XrGlDestroySwapchain;

    sess->begin_session = XrGlBeginSession;
    sess->end_session = XrGlEndSession;

    sess->wait_frame = XrGlWaitFrame;
    sess->begin_frame = XrGlBeginFrame;
    sess->end_frame = XrGlEndFrame;

    sess->set_swapchain_procs = XrGlSetSwapchainProcs;
}

static XrGlSession *GetXrGlSession(XrSession session) {
    Session *sess = reinterpret_cast<Session *>(session);
    return static_cast<XrGlSession *>(sess->implementation_data);
}

static XrGlSwapchain *GetXrGlSwapchain(XrSwapchain swapchain) {
    Swapchain *swch = reinterpret_cast<Swapchain *>(swapchain);
    return static_cast<XrGlSwapchain *>(swch->implementation_data);
}

static void CheckShader(GLuint shader) {
    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        GLchar msg[4096];
        GLsizei length;
        glGetShaderInfoLog(shader, sizeof(msg), &length, msg);
        printf("Compile shader failed: Message length = %d\n%s\n", length, msg);
    }
}

static void CheckProgram(GLuint prog) {
    GLint status = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLchar msg[4096];
        GLsizei length;
        glGetProgramInfoLog(prog, sizeof(msg), &length, msg);
        printf("Link shader failed: Message length = %d\n%s\n", length, msg);
    }
}

static void CompositorInitialize(XrGlSession *glsess) {
    float pos[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
                   0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f};

    float tc[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
                  0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f};

    float scalex = 0.8f;
    float biasx[2] = {-0.9f, 0.1f};
    float scaley = 1.6f;
    float biasy = -0.8f;

    for (int i = 0; i < 24; i += 2) {
        float &px = pos[i + 0];
        px = px * scalex + biasx[i < 12 ? 0 : 1];
        float &py = pos[i + 1];
        py = py * scaley + biasy;
    }

    glGetError();

    glGenVertexArrays(1, &glsess->distortion_mesh.vao);
    glBindVertexArray(glsess->distortion_mesh.vao);

    glGenBuffers(1, &glsess->distortion_mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, glsess->distortion_mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(pos) + sizeof(tc), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(pos), pos);
    glBufferSubData(GL_ARRAY_BUFFER, sizeof(pos), sizeof(tc), tc);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, static_cast<char *>(NULL) + sizeof(pos));

    glGenVertexArrays(1, &glsess->dummy_vao);
    glBindVertexArray(glsess->dummy_vao);

    GLuint err = glGetError();
    if (err) {
        printf("got GL error: %d\n", err);
    }

    const char *vertex_shader_code =
        "#version 310 es\n"
        "in highp vec2 vp;\n"
        "in highp vec2 tc;\n"
        "out highp vec2 texCoord;\n"
        "void main() {\n"
        "  gl_Position = vec4(vp, 0.0, 1.0);\n"
        "  texCoord = tc;\n"
        "}\n";

    const char *fragment_shader_code =
        "#version 310 es\n"
        "uniform sampler2D texture0;\n"
        "in highp vec2 texCoord;\n"
        "out lowp vec4 frag_color;\n"
        "void main() {\n"
        "  frag_color = 0.9 * texture( texture0, texCoord ) + 0.1 * vec4( texCoord.x, 1.0, texCoord.y, 1.0);\n"
        "}\n";

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_code, NULL);
    glCompileShader(vertex_shader);
    CheckShader(vertex_shader);

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_code, NULL);
    glCompileShader(fragment_shader);
    CheckShader(fragment_shader);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glBindAttribLocation(program, 0, "vp");
    glBindAttribLocation(program, 1, "tc");
    glLinkProgram(program);
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "texture0"), 0);
    CheckProgram(program);

    glsess->comp_prog.vert = vertex_shader;
    glsess->comp_prog.frag = fragment_shader;
    glsess->comp_prog.prog = program;
}

static void CompositorRender(XrGlSession *glsess, const XrFrameEndInfo *frameEndInfo) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glViewport(0, 0, glsess->width, glsess->height);
    glScissor(0, 0, glsess->width, glsess->height);
    glClearColor(0.0f, 0.3f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindVertexArray(glsess->distortion_mesh.vao);
    glUseProgram(glsess->comp_prog.prog);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);
    for (uint32_t i = 0; i < frameEndInfo->layerCount; i++) {
        const XrCompositionLayerProjection *proj = reinterpret_cast<const XrCompositionLayerProjection *>(frameEndInfo->layers[i]);

        for (uint32_t j = 0; j < proj->viewCount; j++) {
            XrGlSwapchain *glswapchain = GetXrGlSwapchain(proj->views[j].subImage.swapchain);
            glBindTexture(glswapchain->target, glswapchain->tex[glsess->chain_indices[i][j]]);
            glDrawArrays(GL_TRIANGLES, j * 6, 6);
        }
    }
    glBindVertexArray(glsess->dummy_vao);

    ksGpuWindow_SwapBuffers(&glsess->compositor_window);
}

static void CompositorMainLoop(XrGlSession *glsess) {
    printf("running compositor_main_loop\n");
    ksGpuContext_SetCurrent(&glsess->compositor_window.context);
    CompositorInitialize(glsess);
    {
        std::lock_guard<std::mutex> lg(glsess->mutex);
        glsess->allow_composite = false;
        printf("compositor initialized\n");
    }
    glsess->cond.notify_one();

    while (true) {
        std::unique_lock<std::mutex> lk(glsess->mutex);
        glsess->cond.wait(lk, [&] { return glsess->allow_composite == true; });
        glsess->allow_composite = false;
        XrFrameEndInfo frameEndInfo = glsess->end_frame_info;
        if (glsess->shutdown) {
            lk.unlock();  // TODO(Cass): temporary, until resources are properly synchronized
            break;
        }
        CompositorRender(glsess, &frameEndInfo);
        lk.unlock();  // TODO(Cass): temporary, until resources are properly synchronized
        glsess->cond.notify_one();
    }
    // close GL context and other resources.
    ksGpuContext_WaitIdle(&glsess->compositor_window.context);
}

XrResult XRAPI_CALL XrGlCreateSession(XrInstance instance, const XrSessionCreateInfo *createInfo, XrSession *session) {
    printf("XrGlCreateSession start\n");
    XrGlSession *glsess = new XrGlSession;
    Session *sess = reinterpret_cast<Session *>(*session);
    sess->implementation_data = glsess;

    glsess->width = 1024;
    glsess->height = 512;

    ksDriverInstance dummyInstance;
    const ksGpuQueueInfo dummyQueueInfo = {};
    const int kDummyQueueIdx = 0;

    bool window_created = ksGpuWindow_Create(&glsess->compositor_window, &dummyInstance, &dummyQueueInfo, kDummyQueueIdx,
                                             KS_GPU_SURFACE_COLOR_FORMAT_R8G8B8A8, KS_GPU_SURFACE_DEPTH_FORMAT_NONE,
                                             KS_GPU_SAMPLE_COUNT_1, glsess->width, glsess->height, false);

    if (!window_created) {
        fprintf(stderr, "ERROR: ksGpuWindow_Create failed.\n");
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    bool context_created = ksGpuContext_CreateShared(&glsess->app_context, &glsess->compositor_window.context, kDummyQueueIdx);
    if (!context_created) {
        fprintf(stderr, "ERROR: ksGpuContext_CreateShared for app context FAILED\n");
        ksGpuWindow_Destroy(&glsess->compositor_window);
        return XR_ERROR_INITIALIZATION_FAILED;
    }

    ksGpuContext_SetCurrent(&glsess->app_context);

    glsess->compositor_thread = std::thread(CompositorMainLoop, glsess);

    std::unique_lock<std::mutex> lk(glsess->mutex);
    glsess->cond.wait(lk, [&] { return glsess->allow_composite == false; });
    lk.unlock();
    printf("XrGlCreateSession end\n");

    return XR_SUCCESS;
}

XrResult XRAPI_CALL XrGlDestroySession(XrSession session) {
    printf("XrGlDestroySession\n");
    XrGlSession *glsess = GetXrGlSession(session);
    glsess->shutdown = true;
    glsess->allow_composite = true;
    glsess->cond.notify_one();
    glsess->compositor_thread.join();

    // TODO: ref impl doesn't actually keep lists of child objects to destroy

    // ksGpuContext_Destroy( &glsess->app_context );
    ksGpuWindow_Destroy(&glsess->compositor_window);

    delete glsess;
    Session *sess = reinterpret_cast<Session *>(session);
    sess->implementation_data = NULL;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL XrGlEnumerateViewConfigurations(XrInstance instance, XrSystemId /*systemId*/, uint32_t capacityInput,
                                                    uint32_t *countOutput, XrViewConfigurationType *viewConfigTypes) {
    // We ignore the systemId parameter because this function will currently only ever get called
    // with the one systemId we support, and we support only one kind of view configuration.
    Instance *inst = reinterpret_cast<Instance *>(instance);

    *countOutput = 1;
    if (capacityInput < 1) {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }

    viewConfigTypes[0] = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL XrGlGetViewConfiguration(XrInstance instance, XrSystemId /*systemId*/, XrViewConfigurationType viewConfigType,
                                             XrViewConfigurationProperties *configuration) {
    Instance *inst = reinterpret_cast<Instance *>(instance);

    // We support only a single configuration
    if (viewConfigType != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) return XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
    configuration->viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    configuration->fovMutable = XR_TRUE;

    return XR_SUCCESS;
}

XrResult XRAPI_CALL XrGlEnumerateViewConfigurationViews(XrInstance instance, XrSystemId systemId,
                                                        XrViewConfigurationType viewConfigType, uint32_t capacityInput,
                                                        uint32_t *countOutput, XrViewConfigurationView *views) {
    if (countOutput) {
        *countOutput = 2;
    }

    if (!views && capacityInput == 0) {
        return XR_SUCCESS;
    }

    if (capacityInput < 2) {
        return XR_ERROR_SIZE_INSUFFICIENT;
    }

    if (capacityInput != 2) {
        return XR_ERROR_VALIDATION_FAILURE;
    }

    views[0].recommendedImageRectWidth = views[1].recommendedImageRectWidth = 1024;
    views[0].maxImageRectWidth = views[1].maxImageRectWidth = 1024;
    views[0].recommendedImageRectHeight = views[1].recommendedImageRectHeight = 1024;
    views[0].maxImageRectHeight = views[1].maxImageRectHeight = 1024;
    views[0].recommendedSwapchainSampleCount = views[1].recommendedSwapchainSampleCount = 4;
    views[0].maxSwapchainSampleCount = views[1].maxSwapchainSampleCount = 4;

    return XR_SUCCESS;
}

XrResult XRAPI_CALL XrGlEnumerateSwapchainFormats(XrSession session, uint32_t format_capacity_input, uint32_t *format_count_output,
                                                  int64_t *format_array) {
    printf("XrGlEnumerateSwapchainFormats\n");
    XrResult result = XR_SUCCESS;
    const GLenum kGLFormats[] = {GL_RGBA8, GL_RGB8, GL_RGBA8_SNORM, GL_RGB8_SNORM};
    const uint32_t kNumGLFormats = sizeof(kGLFormats) / sizeof(GLenum);

    if (format_capacity_input == 0) {
        *format_count_output = kNumGLFormats;
        return XR_SUCCESS;
    }

    // populate output here
    uint32_t count = kNumGLFormats;
    if (format_capacity_input < count) {
        count = format_capacity_input;
        result = XR_ERROR_SIZE_INSUFFICIENT;
    }

    for (uint32_t format = 0; format < count; ++format) {
        format_array[format] = static_cast<int64_t>(kGLFormats[format]);
    }

    *format_count_output = count;
    return result;
}

XrResult XRAPI_CALL XrGlCreateSwapchain(XrSession session, const XrSwapchainCreateInfo *info, XrSwapchain *swapchain) {
    new XrGlSwapchain(info, reinterpret_cast<Swapchain *>(*swapchain));
    return XR_SUCCESS;
}

XrResult XRAPI_CALL XrGlDestroySwapchain(XrSwapchain swapchain) {
    XrGlSwapchain *glswapchain = GetXrGlSwapchain(swapchain);
    delete glswapchain;
    Swapchain *swch = reinterpret_cast<Swapchain *>(swapchain);
    swch->implementation_data = NULL;
    return XR_SUCCESS;
}

XrResult XRAPI_CALL XrGlGetSwapchainImages(XrSwapchain swapchain, uint32_t capacityInput, uint32_t *countOutput,
                                           XrSwapchainImageBaseHeader *image) {
    XrGlSwapchain *glswapchain = GetXrGlSwapchain(swapchain);
    glswapchain->GetImages(capacityInput, countOutput, image);
    return XR_SUCCESS;
}

XrResult XRAPI_CALL XrGlAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo * /*acquireInfo*/,
                                              uint32_t *index) {
    XrGlSwapchain *glswapchain = GetXrGlSwapchain(swapchain);
    glswapchain->Acquire(index);
    return XR_SUCCESS;
}

XrResult XRAPI_CALL XrGlWaitSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageWaitInfo *waitInfo) {
    XrGlSwapchain *glswapchain = GetXrGlSwapchain(swapchain);
    glswapchain->Wait(waitInfo->timeout);
    return XR_SUCCESS;
}

XrResult XRAPI_CALL XrGlReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo * /*releaseInfo*/) {
    XrGlSwapchain *glswapchain = GetXrGlSwapchain(swapchain);
    glswapchain->Release();
    return XR_SUCCESS;
}

XrResult XRAPI_CALL XrGlBeginSession(XrSession session, const XrSessionBeginInfo *beginInfo) {
    XrGlSession *glsess = GetXrGlSession(session);
    (void)glsess;
    (void)beginInfo;
    // We don't currently use anything from beginInfo.
    return XR_SUCCESS;
}

XrResult XRAPI_CALL XrGlEndSession(XrSession session) { return XR_SUCCESS; }

XrResult XRAPI_CALL XrGlWaitFrame(XrSession session, const XrFrameWaitInfo *wait_frame_info, XrFrameState *frame_state) {
    return XR_SUCCESS;
}

XrResult XRAPI_CALL XrGlBeginFrame(XrSession session, const XrFrameBeginInfo *begin_frame_info) { return XR_SUCCESS; }

XrResult XRAPI_CALL XrGlEndFrame(XrSession session, const XrFrameEndInfo *end_frame_info) {
    XrGlSession *glsess = GetXrGlSession(session);

    const ksGpuWindowEvent handle_event = ksGpuWindow_ProcessEvents(&glsess->compositor_window);
    if (handle_event == KS_GPU_WINDOW_EVENT_EXIT) {
        return XR_ERROR_INSTANCE_LOST;
    }

    // TODO(cass): do real synchronization here
    glFinish();

    {
        std::lock_guard<std::mutex> lg(glsess->mutex);
        glsess->end_frame_info = *end_frame_info;
        for (uint32_t layer = 0; layer < end_frame_info->layerCount; layer++) {
            const XrCompositionLayerProjection *proj =
                reinterpret_cast<const XrCompositionLayerProjection *>(end_frame_info->layers[layer]);
            for (uint32_t eye = 0; eye < proj->viewCount; eye++) {
                XrGlSwapchain *glswapchain = GetXrGlSwapchain(proj->views[eye].subImage.swapchain);
                glsess->chain_indices[layer][eye] = glswapchain->Prev(glswapchain->chain_index_release);
            }
        }
        glsess->allow_composite = true;
        glsess->cond.notify_one();
    }

    {
        std::unique_lock<std::mutex> lk(glsess->mutex);
        glsess->cond.wait(lk, [&] { return glsess->allow_composite == false; });
        lk.unlock();
    }

    return XR_SUCCESS;
}
