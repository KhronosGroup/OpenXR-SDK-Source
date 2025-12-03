// Copyright (c) 2017-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "common.h"
#include "geometry.h"
#include "graphicsplugin.h"
#include "graphics_plugin_impl_helpers.h"
#include "options.h"

#ifdef XR_USE_GRAPHICS_API_OPENGL

#include <common/gfxwrapper_opengl.h>
#include <common/xr_linear.h>

namespace {

static const char* VertexShaderGlsl = R"_(
    #version 410

    in vec3 VertexPos;
    in vec3 VertexColor;

    out vec3 PSVertexColor;

    uniform mat4 ModelViewProjection;

    void main() {
       gl_Position = ModelViewProjection * vec4(VertexPos, 1.0);
       PSVertexColor = VertexColor;
    }
    )_";

static const char* FragmentShaderGlsl = R"_(
    #version 410

    in vec3 PSVertexColor;
    out vec4 FragColor;

    void main() {
       FragColor = vec4(PSVertexColor, 1);
    }
    )_";

std::string glResultString(GLenum err) {
    switch (err) {
        case GL_NO_ERROR:
            return "GL_NO_ERROR";
        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY";
        case GL_STACK_UNDERFLOW:
            return "GL_STACK_UNDERFLOW";
        case GL_STACK_OVERFLOW:
            return "GL_STACK_OVERFLOW";
        default:
            return "<unknown " + std::to_string(err) + ">";
    }
}
[[noreturn]] inline void ThrowGLResult(GLenum res, const char* originator = nullptr, const char* sourceLocation = nullptr) {
    Throw("GL failure " + glResultString(res), originator, sourceLocation);
}

inline GLenum CheckThrowGLResult(GLenum res, const char* originator = nullptr, const char* sourceLocation = nullptr) {
    if ((res) != GL_NO_ERROR) {
        ThrowGLResult(res, originator, sourceLocation);
    }

    return res;
}

#define CHECK_GLCMD(cmd) CheckThrowGLResult(((cmd), glGetError()), #cmd, FILE_AND_LINE)

inline GLenum TexTarget(bool isArray, bool isMultisample) {
    if (isArray && isMultisample) {
        return GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
    } else if (isMultisample) {
        return GL_TEXTURE_2D_MULTISAMPLE;
    } else if (isArray) {
        return GL_TEXTURE_2D_ARRAY;
    } else {
        return GL_TEXTURE_2D;
    }
}
struct OpenGLGraphicsPlugin : public IGraphicsPlugin {
    OpenGLGraphicsPlugin(const std::shared_ptr<Options>& options, const std::shared_ptr<IPlatformPlugin> /*unused*/&)
        : m_clearColor(options->GetBackgroundClearColor()) {}

    OpenGLGraphicsPlugin(const OpenGLGraphicsPlugin&) = delete;
    OpenGLGraphicsPlugin& operator=(const OpenGLGraphicsPlugin&) = delete;
    OpenGLGraphicsPlugin(OpenGLGraphicsPlugin&&) = delete;
    OpenGLGraphicsPlugin& operator=(OpenGLGraphicsPlugin&&) = delete;

    ~OpenGLGraphicsPlugin() override {
        if (m_swapchainFramebuffer != 0) {
            glDeleteFramebuffers(1, &m_swapchainFramebuffer);
        }
        if (m_program != 0) {
            glDeleteProgram(m_program);
        }
        if (m_vao != 0) {
            glDeleteVertexArrays(1, &m_vao);
        }
        if (m_cubeVertexBuffer != 0) {
            glDeleteBuffers(1, &m_cubeVertexBuffer);
        }
        if (m_cubeIndexBuffer != 0) {
            glDeleteBuffers(1, &m_cubeIndexBuffer);
        }

        for (auto& colorToDepth : m_colorToDepthMap) {
            if (colorToDepth.second != 0) {
                glDeleteTextures(1, &colorToDepth.second);
            }
        }

        ksGpuWindow_Destroy(&window);
    }

    std::vector<std::string> GetInstanceExtensions() const override { return {XR_KHR_OPENGL_ENABLE_EXTENSION_NAME}; }

    ksGpuWindow window{};

#if !defined(XR_USE_PLATFORM_MACOS)
    void DebugMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message) {
        (void)source;
        (void)type;
        (void)id;
        (void)severity;
        Log::Write(Log::Level::Info, "GL Debug: " + std::string(message, 0, length));
    }
#endif  // !defined(XR_USE_PLATFORM_MACOS)

    void InitializeDevice(XrInstance instance, XrSystemId systemId) override {
        // Extension function must be loaded by name
        PFN_xrGetOpenGLGraphicsRequirementsKHR pfnGetOpenGLGraphicsRequirementsKHR = nullptr;
        CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetOpenGLGraphicsRequirementsKHR)));

        XrGraphicsRequirementsOpenGLKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
        CHECK_XRCMD(pfnGetOpenGLGraphicsRequirementsKHR(instance, systemId, &graphicsRequirements));

        // Initialize the gl extensions. Note we have to open a window.
        ksDriverInstance driverInstance{};
        ksGpuQueueInfo queueInfo{};
        ksGpuSurfaceColorFormat colorFormat{KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8};
        ksGpuSurfaceDepthFormat depthFormat{KS_GPU_SURFACE_DEPTH_FORMAT_D24};
        ksGpuSampleCount sampleCount{KS_GPU_SAMPLE_COUNT_1};
        if (!ksGpuWindow_Create(&window, &driverInstance, &queueInfo, 0, colorFormat, depthFormat, sampleCount, 640, 480, false)) {
            THROW("Unable to create GL context");
        }

        GLint major = 0;
        GLint minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);

        const XrVersion desiredApiVersion = XR_MAKE_VERSION(major, minor, 0);
        if (graphicsRequirements.minApiVersionSupported > desiredApiVersion) {
            THROW("Runtime does not support desired Graphics API and/or version");
        }

#ifdef XR_USE_PLATFORM_WIN32
        m_graphicsBinding.hDC = window.context.hDC;
        m_graphicsBinding.hGLRC = window.context.hGLRC;
#elif defined(XR_USE_PLATFORM_XLIB)
        m_graphicsBinding.xDisplay = window.context.xDisplay;
        m_graphicsBinding.visualid = window.context.visualid;
        m_graphicsBinding.glxFBConfig = window.context.glxFBConfig;
        m_graphicsBinding.glxDrawable = window.context.glxDrawable;
        m_graphicsBinding.glxContext = window.context.glxContext;
#elif defined(XR_USE_PLATFORM_XCB)
        // TODO: Still missing the platform adapter, and some items to make this usable.
        m_graphicsBinding.connection = window.connection;
        // m_graphicsBinding.screenNumber = window.context.screenNumber;
        // m_graphicsBinding.fbconfigid = window.context.fbconfigid;
        m_graphicsBinding.visualid = window.context.visualid;
        m_graphicsBinding.glxDrawable = window.context.glxDrawable;
        // m_graphicsBinding.glxContext = window.context.glxContext;
#elif defined(XR_USE_PLATFORM_WAYLAND)
        // TODO: Just need something other than NULL here for now (for validation).  Eventually need
        //       to correctly put in a valid pointer to an wl_display
        m_graphicsBinding.display = reinterpret_cast<wl_display*>(0xFFFFFFFF);
#elif defined(XR_USE_PLATFORM_MACOS)
#error OpenGL bindings for Mac have not been implemented
#else
#error Platform not supported
#endif

#if !defined(XR_USE_PLATFORM_MACOS)
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(
            [](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message,
               const void* userParam) {
                ((OpenGLGraphicsPlugin*)userParam)->DebugMessageCallback(source, type, id, severity, length, message);
            },
            this);
#endif  // !defined(XR_USE_PLATFORM_MACOS)

        InitializeResources();
    }

    void InitializeResources() {
        glGenFramebuffers(1, &m_swapchainFramebuffer);

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &VertexShaderGlsl, nullptr);
        glCompileShader(vertexShader);
        CheckShader(vertexShader);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &FragmentShaderGlsl, nullptr);
        glCompileShader(fragmentShader);
        CheckShader(fragmentShader);

        m_program = glCreateProgram();
        glAttachShader(m_program, vertexShader);
        glAttachShader(m_program, fragmentShader);
        glLinkProgram(m_program);
        CheckProgram(m_program);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        m_modelViewProjectionUniformLocation = glGetUniformLocation(m_program, "ModelViewProjection");

        m_vertexAttribCoords = glGetAttribLocation(m_program, "VertexPos");
        m_vertexAttribColor = glGetAttribLocation(m_program, "VertexColor");

        glGenBuffers(1, &m_cubeVertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, m_cubeVertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Geometry::c_cubeVertices), Geometry::c_cubeVertices, GL_STATIC_DRAW);

        glGenBuffers(1, &m_cubeIndexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cubeIndexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(Geometry::c_cubeIndices), Geometry::c_cubeIndices, GL_STATIC_DRAW);

        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);
        glEnableVertexAttribArray(m_vertexAttribCoords);
        glEnableVertexAttribArray(m_vertexAttribColor);
        glBindBuffer(GL_ARRAY_BUFFER, m_cubeVertexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cubeIndexBuffer);
        glVertexAttribPointer(m_vertexAttribCoords, 3, GL_FLOAT, GL_FALSE, sizeof(Geometry::Vertex), nullptr);
        glVertexAttribPointer(m_vertexAttribColor, 3, GL_FLOAT, GL_FALSE, sizeof(Geometry::Vertex),
                              reinterpret_cast<const void*>(sizeof(XrVector3f)));
    }

    void CheckShader(GLuint shader) {
        GLint r = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &r);
        if (r == GL_FALSE) {
            GLchar msg[4096] = {};
            GLsizei length;
            glGetShaderInfoLog(shader, sizeof(msg), &length, msg);
            THROW(Fmt("Compile shader failed: %s", msg));
        }
    }

    void CheckProgram(GLuint prog) {
        GLint r = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &r);
        if (r == GL_FALSE) {
            GLchar msg[4096] = {};
            GLsizei length;
            glGetProgramInfoLog(prog, sizeof(msg), &length, msg);
            THROW(Fmt("Link program failed: %s", msg));
        }
    }

    int64_t SelectColorSwapchainFormat(bool throwIfNotFound, span<const int64_t> imageFormatArray) const override {
        // List of supported color swapchain formats, note sRGB formats skipped due to CTS bug.
        // The order of this list does not effect the priority of selecting formats, the runtime list defines that.
        return SelectSwapchainFormat(  //
            throwIfNotFound, imageFormatArray,
            {
                GL_RGB10_A2,
                GL_RGBA16,
                GL_RGBA16F,
                GL_RGBA32F,
                // The two below should only be used as a fallback, as they are linear color formats without enough bits for color
                // depth, thus leading to banding.
                GL_RGBA8,
                GL_RGBA8_SNORM,
            });
    }

    int64_t SelectDepthSwapchainFormat(bool throwIfNotFound, span<const int64_t> imageFormatArray) const override {
        // List of supported depth swapchain formats.
        return SelectSwapchainFormat(  //
            throwIfNotFound, imageFormatArray,
            {
                GL_DEPTH24_STENCIL8,
                GL_DEPTH32F_STENCIL8,
                GL_DEPTH_COMPONENT24,
                GL_DEPTH_COMPONENT32F,
                GL_DEPTH_COMPONENT16,
            });
    }

    const XrBaseInStructure* GetGraphicsBinding() const override {
        return reinterpret_cast<const XrBaseInStructure*>(&m_graphicsBinding);
    }

    struct OpenGLFallbackDepthTexture {
       public:
        OpenGLFallbackDepthTexture() = default;
        ~OpenGLFallbackDepthTexture() {
            if (Allocated()) {
                // As implementation as ::Reset(), but should not throw in destructor
                glDeleteTextures(1, &m_texture);
            }
            m_texture = 0;
        }
        void Reset() {
            if (Allocated()) {
                CHECK_GLCMD(glDeleteTextures(1, &m_texture));
            }
            m_texture = 0;
        }
        bool Allocated() const { return m_texture != 0; }

        void Allocate(GLuint width, GLuint height, uint32_t arraySize, uint32_t sampleCount) {
            Reset();
            const bool isArray = arraySize > 1;
            const bool isMultisample = sampleCount > 1;
            GLenum target = TexTarget(isArray, isMultisample);
            CHECK_GLCMD(glGenTextures(1, &m_texture));
            CHECK_GLCMD(glBindTexture(target, m_texture));
            if (!isMultisample) {
                CHECK_GLCMD(glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
                CHECK_GLCMD(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
                CHECK_GLCMD(glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
                CHECK_GLCMD(glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
            }
            if (isMultisample) {
                if (isArray) {
                    CHECK_GLCMD(glTexImage3DMultisample(target, sampleCount, GL_DEPTH_COMPONENT32, width, height, arraySize, true));
                } else {
                    CHECK_GLCMD(glTexImage2DMultisample(target, sampleCount, GL_DEPTH_COMPONENT32, width, height, true));
                }
            } else {
                if (isArray) {
                    CHECK_GLCMD(glTexImage3D(target, 0, GL_DEPTH_COMPONENT32, width, height, arraySize, 0, GL_DEPTH_COMPONENT,
                                             GL_FLOAT, nullptr));
                } else {
                    CHECK_GLCMD(
                        glTexImage2D(target, 0, GL_DEPTH_COMPONENT32, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr));
                }
            }
            m_image.image = m_texture;
        }
        const XrSwapchainImageOpenGLKHR& GetTexture() const { return m_image; }

       private:
        uint32_t m_texture{0};
        XrSwapchainImageOpenGLKHR m_image{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR, NULL, 0};
    };
    class OpenGLSwapchainImageData : public SwapchainImageDataBase<XrSwapchainImageOpenGLKHR> {
       public:
        OpenGLSwapchainImageData(uint32_t capacity, const XrSwapchainCreateInfo& createInfo)
            : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR, capacity, createInfo), m_internalDepthTextures(capacity) {}

        OpenGLSwapchainImageData(uint32_t capacity, const XrSwapchainCreateInfo& createInfo, XrSwapchain depthSwapchain,
                                 const XrSwapchainCreateInfo& depthCreateInfo)
            : SwapchainImageDataBase(XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR, capacity, createInfo, depthSwapchain, depthCreateInfo),
              m_internalDepthTextures(capacity) {}

       protected:
        const XrSwapchainImageOpenGLKHR& GetFallbackDepthSwapchainImage(uint32_t i) override {
            if (!m_internalDepthTextures[i].Allocated()) {
                m_internalDepthTextures[i].Allocate(this->Width(), this->Height(), this->ArraySize(), this->SampleCount());
            }

            return m_internalDepthTextures[i].GetTexture();
        }

       private:
        std::vector<OpenGLFallbackDepthTexture> m_internalDepthTextures;
    };

    ISwapchainImageData* AllocateSwapchainImageData(size_t size, const XrSwapchainCreateInfo& swapchainCreateInfo) override {
        auto typedResult = std::make_unique<OpenGLSwapchainImageData>(uint32_t(size), swapchainCreateInfo);

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    inline ISwapchainImageData* AllocateSwapchainImageDataWithDepthSwapchain(
        size_t size, const XrSwapchainCreateInfo& colorSwapchainCreateInfo, XrSwapchain depthSwapchain,
        const XrSwapchainCreateInfo& depthSwapchainCreateInfo) override {
        auto typedResult = std::make_unique<OpenGLSwapchainImageData>(uint32_t(size), colorSwapchainCreateInfo, depthSwapchain,
                                                                      depthSwapchainCreateInfo);

        // Cast our derived type to the caller-expected type.
        auto ret = static_cast<ISwapchainImageData*>(typedResult.get());

        m_swapchainImageDataMap.Adopt(std::move(typedResult));

        return ret;
    }

    uint32_t GetDepthTexture(uint32_t colorTexture) {
        // If a depth-stencil view has already been created for this back-buffer, use it.
        auto depthBufferIt = m_colorToDepthMap.find(colorTexture);
        if (depthBufferIt != m_colorToDepthMap.end()) {
            return depthBufferIt->second;
        }

        // This back-buffer has no corresponding depth-stencil texture, so create one with matching dimensions.

        GLint width;
        GLint height;
        glBindTexture(GL_TEXTURE_2D, colorTexture);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

        uint32_t depthTexture;
        glGenTextures(1, &depthTexture);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

        m_colorToDepthMap.insert(std::make_pair(colorTexture, depthTexture));

        return depthTexture;
    }

    void RenderView(const XrCompositionLayerProjectionView& layerView, const XrSwapchainImageBaseHeader* swapchainImage,
                    int64_t swapchainFormat, const std::vector<Cube>& cubes) override {
        CHECK(layerView.subImage.imageArrayIndex == 0);  // Texture arrays not supported.
        UNUSED_PARM(swapchainFormat);                    // Not used in this function for now.

        OpenGLSwapchainImageData* swapchainData;
        uint32_t imageIndex;
        std::tie(swapchainData, imageIndex) = m_swapchainImageDataMap.GetDataAndIndexFromBasePointer(swapchainImage);

        glBindFramebuffer(GL_FRAMEBUFFER, m_swapchainFramebuffer);

        const uint32_t colorTexture = reinterpret_cast<const XrSwapchainImageOpenGLKHR*>(swapchainImage)->image;
        const GLuint depthTexture = swapchainData->GetDepthImageForColorIndex(imageIndex).image;

        glViewport(static_cast<GLint>(layerView.subImage.imageRect.offset.x),
                   static_cast<GLint>(layerView.subImage.imageRect.offset.y),
                   static_cast<GLsizei>(layerView.subImage.imageRect.extent.width),
                   static_cast<GLsizei>(layerView.subImage.imageRect.extent.height));

        glFrontFace(GL_CW);
        glCullFace(GL_BACK);
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

        // Clear swapchain and depth buffer.
        glClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);
        glClearDepth(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        // Set shaders and uniform variables.
        glUseProgram(m_program);

        const auto& pose = layerView.pose;
        XrMatrix4x4f proj;
        XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_OPENGL, layerView.fov, 0.05f, 100.0f);
        XrMatrix4x4f toView;
        XrMatrix4x4f_CreateFromRigidTransform(&toView, &pose);
        XrMatrix4x4f view;
        XrMatrix4x4f_InvertRigidBody(&view, &toView);
        XrMatrix4x4f vp;
        XrMatrix4x4f_Multiply(&vp, &proj, &view);

        // Set cube primitive data.
        glBindVertexArray(m_vao);

        // Render each cube
        for (const Cube& cube : cubes) {
            // Compute the model-view-projection transform and set it..
            XrMatrix4x4f model;
            XrMatrix4x4f_CreateTranslationRotationScale(&model, &cube.Pose.position, &cube.Pose.orientation, &cube.Scale);
            XrMatrix4x4f mvp;
            XrMatrix4x4f_Multiply(&mvp, &vp, &model);
            glUniformMatrix4fv(m_modelViewProjectionUniformLocation, 1, GL_FALSE, reinterpret_cast<const GLfloat*>(&mvp));

            // Draw the cube.
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(ArraySize(Geometry::c_cubeIndices)), GL_UNSIGNED_SHORT, nullptr);
        }

        glBindVertexArray(0);
        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    uint32_t GetSupportedSwapchainSampleCount(const XrViewConfigurationView&) override { return 1; }

    void UpdateOptions(const std::shared_ptr<Options>& options) override { m_clearColor = options->GetBackgroundClearColor(); }

   private:
#ifdef XR_USE_PLATFORM_WIN32
    XrGraphicsBindingOpenGLWin32KHR m_graphicsBinding{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
#elif defined(XR_USE_PLATFORM_XLIB)
    XrGraphicsBindingOpenGLXlibKHR m_graphicsBinding{XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
#elif defined(XR_USE_PLATFORM_XCB)
    XrGraphicsBindingOpenGLXcbKHR m_graphicsBinding{XR_TYPE_GRAPHICS_BINDING_OPENGL_XCB_KHR};
#elif defined(XR_USE_PLATFORM_WAYLAND)
    XrGraphicsBindingOpenGLWaylandKHR m_graphicsBinding{XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR};
#elif defined(XR_USE_PLATFORM_MACOS)
#error OpenGL bindings for Mac have not been implemented
#else
#error Platform not supported
#endif

    SwapchainImageDataMap<OpenGLSwapchainImageData> m_swapchainImageDataMap;
    GLuint m_swapchainFramebuffer{0};
    GLuint m_program{0};
    GLint m_modelViewProjectionUniformLocation{0};
    GLint m_vertexAttribCoords{0};
    GLint m_vertexAttribColor{0};
    GLuint m_vao{0};
    GLuint m_cubeVertexBuffer{0};
    GLuint m_cubeIndexBuffer{0};

    // Map color buffer to associated depth buffer. This map is populated on demand.
    std::map<uint32_t, uint32_t> m_colorToDepthMap;
    std::array<float, 4> m_clearColor;
};
}  // namespace

std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin_OpenGL(const std::shared_ptr<Options>& options,
                                                             std::shared_ptr<IPlatformPlugin> platformPlugin) {
    return std::make_shared<OpenGLGraphicsPlugin>(options, platformPlugin);
}

#endif
