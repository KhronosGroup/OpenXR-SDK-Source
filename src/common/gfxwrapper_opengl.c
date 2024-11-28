/*
Copyright (c) 2017-2024, The Khronos Group Inc.
Copyright (c) 2016 Oculus VR, LLC.
Portions of macOS, iOS, functionality copyright (c) 2016 The Brenwill Workshop Ltd.

SPDX-License-Identifier: Apache-2.0
*/

#include "gfxwrapper_opengl.h"

/*
================================================================================================================================

System level functionality

================================================================================================================================
*/

static void Error(const char *format, ...) {
#if defined(OS_WINDOWS)
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf_s(buffer, 4096, _TRUNCATE, format, args);
    va_end(args);

    OutputDebugStringA(buffer);

    // MessageBoxA(NULL, buffer, "ERROR", MB_OK | MB_ICONINFORMATION);
#elif defined(OS_LINUX)
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
#elif defined(OS_APPLE_MACOS)
    char buffer[4096];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(buffer, 4096, format, args);
    va_end(args);

    NSLog(@"%s\n", buffer);

    if ([NSThread isMainThread]) {
        NSString *string = [[NSString alloc] initWithBytes:buffer length:length encoding:NSASCIIStringEncoding];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        NSAlert *alert = [[NSAlert alloc] init];
        [alert addButtonWithTitle:@"OK"];
        [alert setMessageText:@"Error"];
        [alert setInformativeText:string];
        [alert setAlertStyle:NSWarningAlertStyle];
        [alert runModal];
#pragma GCC diagnostic pop
    }
#elif defined(OS_APPLE_IOS)
    char buffer[4096];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(buffer, 4096, format, args);
    va_end(args);

    NSLog(@"%s\n", buffer);

    if ([NSThread isMainThread]) {
        NSString *string = [[NSString alloc] initWithBytes:buffer length:length encoding:NSASCIIStringEncoding];
        UIAlertController *alert =
            [UIAlertController alertControllerWithTitle:@"Error" message:string preferredStyle:UIAlertControllerStyleAlert];
        [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                                  style:UIAlertActionStyleDefault
                                                handler:^(UIAlertAction *action){
                                                }]];
        [UIApplication.sharedApplication.keyWindow.rootViewController presentViewController:alert animated:YES completion:nil];
    }
#elif defined(OS_ANDROID)
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, 4096, format, args);
    va_end(args);

    __android_log_print(ANDROID_LOG_ERROR, "atw", "%s", buffer);
#endif
    // Without exiting, the application will likely crash.
    if (format != NULL) {
        exit(1);
    }
}

/*
================================================================================================================================

OpenGL error checking.

================================================================================================================================
*/

#if !defined(NDEBUG)
#define GL(func) \
    func;        \
    GlCheckErrors(#func);
#else
#define GL(func) func;
#endif

#if !defined(NDEBUG)
#define EGL(func)                                                  \
    if (func == EGL_FALSE) {                                       \
        Error(#func " failed: %s", EglErrorString(eglGetError())); \
    }
#else
#define EGL(func)                                                  \
    if (func == EGL_FALSE) {                                       \
        Error(#func " failed: %s", EglErrorString(eglGetError())); \
    }
#endif

#if defined(OS_ANDROID) || defined(OS_LINUX_WAYLAND)
static const char *EglErrorString(const EGLint error) {
    switch (error) {
        case EGL_SUCCESS:
            return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:
            return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:
            return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:
            return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:
            return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT:
            return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG:
            return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE:
            return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:
            return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE:
            return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH:
            return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER:
            return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP:
            return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW:
            return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST:
            return "EGL_CONTEXT_LOST";
        default:
            return "unknown";
    }
}
#endif

#if !defined(NDEBUG)
static const char *GlErrorString(GLenum error) {
    switch (error) {
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
#if !defined(OS_APPLE_MACOS) && !defined(OS_ANDROID) && !defined(OS_APPLE_IOS)
        case GL_STACK_UNDERFLOW:
            return "GL_STACK_UNDERFLOW";
        case GL_STACK_OVERFLOW:
            return "GL_STACK_OVERFLOW";
#endif
        default:
            return "unknown";
    }
}

static void GlCheckErrors(const char *function) {
    for (int i = 0; i < 10; i++) {
        const GLenum error = glGetError();
        if (error == GL_NO_ERROR) {
            break;
        }
        Error("GL error: %s: %s", function, GlErrorString(error));
    }
}
#endif  // !defined(NDEBUG)

/*
================================================================================================================================

OpenGL extensions.

================================================================================================================================
*/

typedef struct {
    bool timer_query;                       // GL_ARB_timer_query, GL_EXT_disjoint_timer_query
    bool texture_clamp_to_border;           // GL_EXT_texture_border_clamp, GL_OES_texture_border_clamp
    bool buffer_storage;                    // GL_ARB_buffer_storage
    bool multi_sampled_storage;             // GL_ARB_texture_storage_multisample
    bool multi_view;                        // GL_OVR_multiview, GL_OVR_multiview2
    bool multi_sampled_resolve;             // GL_EXT_multisampled_render_to_texture
    bool multi_view_multi_sampled_resolve;  // GL_OVR_multiview_multisampled_render_to_texture

    int texture_clamp_to_border_id;
} ksOpenGLExtensions;

ksOpenGLExtensions glExtensions;

/*
================================
Get proc address / extensions
================================
*/

#if defined(OS_WINDOWS)
PROC GetExtension(const char *functionName) { return wglGetProcAddress(functionName); }
#elif defined(OS_APPLE)
void (*GetExtension(const char *functionName))(void) {
    (void)functionName;
    return NULL;
}
#elif defined(OS_LINUX_XCB) || defined(OS_LINUX_XLIB) || defined(OS_LINUX_XCB_GLX)
void (*GetExtension(const char *functionName))(void) { return glXGetProcAddress((const GLubyte *)functionName); }
#elif defined(OS_ANDROID) || defined(OS_LINUX_WAYLAND)
void (*GetExtension(const char *functionName))(void) { return eglGetProcAddress(functionName); }
#endif

GLint glGetInteger(GLenum pname) {
    GLint i;
    GL(glGetIntegerv(pname, &i));
    return i;
}

static bool GlCheckExtension(const char *extension) {
#if defined(OS_WINDOWS) || defined(OS_LINUX)
    PFNGLGETSTRINGIPROC glGetStringi = (PFNGLGETSTRINGIPROC)GetExtension("glGetStringi");
#endif
    GL(const GLint numExtensions = glGetInteger(GL_NUM_EXTENSIONS));
    for (int i = 0; i < numExtensions; i++) {
        GL(const GLubyte *string = glGetStringi(GL_EXTENSIONS, i));
        if (strcmp((const char *)string, extension) == 0) {
            return true;
        }
    }
    return false;
}

#if defined(OS_WINDOWS) || defined(OS_LINUX)

PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer;
PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers;
PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers;
PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer;
PFNGLISRENDERBUFFERPROC glIsRenderbuffer;
PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
PFNGLFRAMEBUFFERTEXTURELAYERPROC glFramebufferTextureLayer;
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT;
PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glFramebufferTextureMultiviewOVR;
PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC glFramebufferTextureMultisampleMultiviewOVR;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC glCheckNamedFramebufferStatus;

PFNGLGENBUFFERSPROC glGenBuffers;
PFNGLDELETEBUFFERSPROC glDeleteBuffers;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLBINDBUFFERBASEPROC glBindBufferBase;
PFNGLBUFFERDATAPROC glBufferData;
PFNGLBUFFERSUBDATAPROC glBufferSubData;
PFNGLBUFFERSTORAGEPROC glBufferStorage;
PFNGLMAPBUFFERPROC glMapBuffer;
PFNGLMAPBUFFERRANGEPROC glMapBufferRange;
PFNGLUNMAPBUFFERPROC glUnmapBuffer;

PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
PFNGLVERTEXATTRIBIPOINTERPROC glVertexAttribIPointer;
PFNGLVERTEXATTRIBDIVISORPROC glVertexAttribDivisor;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;

#if defined(OS_WINDOWS)
PFNGLACTIVETEXTUREPROC glActiveTexture;
PFNGLTEXIMAGE3DPROC glTexImage3D;
PFNGLCOMPRESSEDTEXIMAGE2DPROC glCompressedTexImage2D;
PFNGLCOMPRESSEDTEXIMAGE3DPROC glCompressedTexImage3D;
PFNGLTEXSUBIMAGE3DPROC glTexSubImage3D;
PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glCompressedTexSubImage2D;
PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glCompressedTexSubImage3D;
#endif
PFNGLTEXSTORAGE2DPROC glTexStorage2D;
PFNGLTEXSTORAGE3DPROC glTexStorage3D;
PFNGLTEXIMAGE2DMULTISAMPLEPROC glTexImage2DMultisample;
PFNGLTEXIMAGE3DMULTISAMPLEPROC glTexImage3DMultisample;
PFNGLTEXSTORAGE2DMULTISAMPLEPROC glTexStorage2DMultisample;
PFNGLTEXSTORAGE3DMULTISAMPLEPROC glTexStorage3DMultisample;
PFNGLGENERATEMIPMAPPROC glGenerateMipmap;
PFNGLBINDIMAGETEXTUREPROC glBindImageTexture;

PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLDELETEPROGRAMPROC glDeleteProgram;
PFNGLCREATESHADERPROC glCreateShader;
PFNGLDELETESHADERPROC glDeleteShader;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLGETSHADERIVPROC glGetShaderiv;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
PFNGLUSEPROGRAMPROC glUseProgram;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLGETPROGRAMIVPROC glGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLGETUNIFORMBLOCKINDEXPROC glGetUniformBlockIndex;
PFNGLGETPROGRAMRESOURCEINDEXPROC glGetProgramResourceIndex;
PFNGLUNIFORMBLOCKBINDINGPROC glUniformBlockBinding;
PFNGLSHADERSTORAGEBLOCKBINDINGPROC glShaderStorageBlockBinding;
PFNGLPROGRAMUNIFORM1IPROC glProgramUniform1i;
PFNGLUNIFORM1IPROC glUniform1i;
PFNGLUNIFORM1IVPROC glUniform1iv;
PFNGLUNIFORM2IVPROC glUniform2iv;
PFNGLUNIFORM3IVPROC glUniform3iv;
PFNGLUNIFORM4IVPROC glUniform4iv;
PFNGLUNIFORM1FPROC glUniform1f;
PFNGLUNIFORM1FVPROC glUniform1fv;
PFNGLUNIFORM2FVPROC glUniform2fv;
PFNGLUNIFORM3FVPROC glUniform3fv;
PFNGLUNIFORM4FVPROC glUniform4fv;
PFNGLUNIFORMMATRIX2FVPROC glUniformMatrix2fv;
PFNGLUNIFORMMATRIX2X3FVPROC glUniformMatrix2x3fv;
PFNGLUNIFORMMATRIX2X4FVPROC glUniformMatrix2x4fv;
PFNGLUNIFORMMATRIX3X2FVPROC glUniformMatrix3x2fv;
PFNGLUNIFORMMATRIX3FVPROC glUniformMatrix3fv;
PFNGLUNIFORMMATRIX3X4FVPROC glUniformMatrix3x4fv;
PFNGLUNIFORMMATRIX4X2FVPROC glUniformMatrix4x2fv;
PFNGLUNIFORMMATRIX4X3FVPROC glUniformMatrix4x3fv;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;

PFNGLDRAWELEMENTSINSTANCEDPROC glDrawElementsInstanced;
PFNGLDISPATCHCOMPUTEPROC glDispatchCompute;
PFNGLMEMORYBARRIERPROC glMemoryBarrier;

PFNGLGENQUERIESPROC glGenQueries;
PFNGLDELETEQUERIESPROC glDeleteQueries;
PFNGLISQUERYPROC glIsQuery;
PFNGLBEGINQUERYPROC glBeginQuery;
PFNGLENDQUERYPROC glEndQuery;
PFNGLQUERYCOUNTERPROC glQueryCounter;
PFNGLGETQUERYIVPROC glGetQueryiv;
PFNGLGETQUERYOBJECTIVPROC glGetQueryObjectiv;
PFNGLGETQUERYOBJECTUIVPROC glGetQueryObjectuiv;
PFNGLGETQUERYOBJECTI64VPROC glGetQueryObjecti64v;
PFNGLGETQUERYOBJECTUI64VPROC glGetQueryObjectui64v;

PFNGLFENCESYNCPROC glFenceSync;
PFNGLCLIENTWAITSYNCPROC glClientWaitSync;
PFNGLDELETESYNCPROC glDeleteSync;
PFNGLISSYNCPROC glIsSync;

PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate;

PFNGLDEBUGMESSAGECONTROLPROC glDebugMessageControl;
PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback;

#if defined(OS_WINDOWS)
PFNGLBLENDCOLORPROC glBlendColor;
PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
PFNWGLDELAYBEFORESWAPNVPROC wglDelayBeforeSwapNV;
#elif defined(OS_LINUX) && !defined(OS_LINUX_WAYLAND)
PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB;
PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT;
PFNGLXDELAYBEFORESWAPNVPROC glXDelayBeforeSwapNV;
#endif

PFNGLBINDSAMPLERPROC glBindSampler;
PFNGLDELETESAMPLERSPROC glDeleteSamplers;
PFNGLGENSAMPLERSPROC glGenSamplers;
PFNGLGETSAMPLERPARAMETERIIVPROC glGetSamplerParameterIiv;
PFNGLGETSAMPLERPARAMETERIUIVPROC glGetSamplerParameterIuiv;
PFNGLGETSAMPLERPARAMETERFVPROC glGetSamplerParameterfv;
PFNGLGETSAMPLERPARAMETERIVPROC glGetSamplerParameteriv;
PFNGLISSAMPLERPROC glIsSampler;
PFNGLSAMPLERPARAMETERIIVPROC glSamplerParameterIiv;
PFNGLSAMPLERPARAMETERIUIVPROC glSamplerParameterIuiv;
PFNGLSAMPLERPARAMETERFPROC glSamplerParameterf;
PFNGLSAMPLERPARAMETERFVPROC glSamplerParameterfv;
PFNGLSAMPLERPARAMETERIPROC glSamplerParameteri;
PFNGLSAMPLERPARAMETERIVPROC glSamplerParameteriv;

void GlBootstrapExtensions() {
#if defined(OS_WINDOWS)
    wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)GetExtension("wglChoosePixelFormatARB");
    wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)GetExtension("wglCreateContextAttribsARB");
    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)GetExtension("wglSwapIntervalEXT");
    wglDelayBeforeSwapNV = (PFNWGLDELAYBEFORESWAPNVPROC)GetExtension("wglDelayBeforeSwapNV");
#elif defined(OS_LINUX) && !defined(OS_LINUX_WAYLAND)
    glXCreateContextAttribsARB = (PFNGLXCREATECONTEXTATTRIBSARBPROC)GetExtension("glXCreateContextAttribsARB");
    glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)GetExtension("glXSwapIntervalEXT");
    glXDelayBeforeSwapNV = (PFNGLXDELAYBEFORESWAPNVPROC)GetExtension("glXDelayBeforeSwapNV");
#endif
}

void GlInitExtensions() {
    glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)GetExtension("glGenFramebuffers");
    glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)GetExtension("glDeleteFramebuffers");
    glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)GetExtension("glBindFramebuffer");
    glBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC)GetExtension("glBlitFramebuffer");
    glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC)GetExtension("glGenRenderbuffers");
    glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC)GetExtension("glDeleteRenderbuffers");
    glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC)GetExtension("glBindRenderbuffer");
    glIsRenderbuffer = (PFNGLISRENDERBUFFERPROC)GetExtension("glIsRenderbuffer");
    glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)GetExtension("glRenderbufferStorage");
    glRenderbufferStorageMultisample = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC)GetExtension("glRenderbufferStorageMultisample");
    glRenderbufferStorageMultisampleEXT =
        (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)GetExtension("glRenderbufferStorageMultisampleEXT");
    glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)GetExtension("glFramebufferRenderbuffer");
    glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)GetExtension("glFramebufferTexture2D");
    glFramebufferTextureLayer = (PFNGLFRAMEBUFFERTEXTURELAYERPROC)GetExtension("glFramebufferTextureLayer");
    glFramebufferTexture2DMultisampleEXT =
        (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)GetExtension("glFramebufferTexture2DMultisampleEXT");
    glFramebufferTextureMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)GetExtension("glFramebufferTextureMultiviewOVR");
    glFramebufferTextureMultisampleMultiviewOVR =
        (PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)GetExtension("glFramebufferTextureMultisampleMultiviewOVR");
    glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)GetExtension("glCheckFramebufferStatus");
    glCheckNamedFramebufferStatus = (PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC)GetExtension("glCheckNamedFramebufferStatus");

    glGenBuffers = (PFNGLGENBUFFERSPROC)GetExtension("glGenBuffers");
    glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)GetExtension("glDeleteBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)GetExtension("glBindBuffer");
    glBindBufferBase = (PFNGLBINDBUFFERBASEPROC)GetExtension("glBindBufferBase");
    glBufferData = (PFNGLBUFFERDATAPROC)GetExtension("glBufferData");
    glBufferSubData = (PFNGLBUFFERSUBDATAPROC)GetExtension("glBufferSubData");
    glBufferStorage = (PFNGLBUFFERSTORAGEPROC)GetExtension("glBufferStorage");
    glMapBuffer = (PFNGLMAPBUFFERPROC)GetExtension("glMapBuffer");
    glMapBufferRange = (PFNGLMAPBUFFERRANGEPROC)GetExtension("glMapBufferRange");
    glUnmapBuffer = (PFNGLUNMAPBUFFERPROC)GetExtension("glUnmapBuffer");

    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)GetExtension("glGenVertexArrays");
    glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)GetExtension("glDeleteVertexArrays");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)GetExtension("glBindVertexArray");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)GetExtension("glVertexAttribPointer");
    glVertexAttribIPointer = (PFNGLVERTEXATTRIBIPOINTERPROC)GetExtension("glVertexAttribIPointer");
    glVertexAttribDivisor = (PFNGLVERTEXATTRIBDIVISORPROC)GetExtension("glVertexAttribDivisor");
    glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)GetExtension("glDisableVertexAttribArray");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)GetExtension("glEnableVertexAttribArray");

#if defined(OS_WINDOWS)
    glActiveTexture = (PFNGLACTIVETEXTUREPROC)GetExtension("glActiveTexture");
    glTexImage3D = (PFNGLTEXIMAGE3DPROC)GetExtension("glTexImage3D");
    glCompressedTexImage2D = (PFNGLCOMPRESSEDTEXIMAGE2DPROC)GetExtension("glCompressedTexImage2D");
    glCompressedTexImage3D = (PFNGLCOMPRESSEDTEXIMAGE3DPROC)GetExtension("glCompressedTexImage3D");
    glTexSubImage3D = (PFNGLTEXSUBIMAGE3DPROC)GetExtension("glTexSubImage3D");
    glCompressedTexSubImage2D = (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)GetExtension("glCompressedTexSubImage2D");
    glCompressedTexSubImage3D = (PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC)GetExtension("glCompressedTexSubImage3D");
#endif
    glTexStorage2D = (PFNGLTEXSTORAGE2DPROC)GetExtension("glTexStorage2D");
    glTexStorage3D = (PFNGLTEXSTORAGE3DPROC)GetExtension("glTexStorage3D");
    glTexImage2DMultisample = (PFNGLTEXIMAGE2DMULTISAMPLEPROC)GetExtension("glTexImage2DMultisample");
    glTexImage3DMultisample = (PFNGLTEXIMAGE3DMULTISAMPLEPROC)GetExtension("glTexImage3DMultisample");
    glTexStorage2DMultisample = (PFNGLTEXSTORAGE2DMULTISAMPLEPROC)GetExtension("glTexStorage2DMultisample");
    glTexStorage3DMultisample = (PFNGLTEXSTORAGE3DMULTISAMPLEPROC)GetExtension("glTexStorage3DMultisample");
    glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC)GetExtension("glGenerateMipmap");
    glBindImageTexture = (PFNGLBINDIMAGETEXTUREPROC)GetExtension("glBindImageTexture");

    glCreateProgram = (PFNGLCREATEPROGRAMPROC)GetExtension("glCreateProgram");
    glDeleteProgram = (PFNGLDELETEPROGRAMPROC)GetExtension("glDeleteProgram");
    glCreateShader = (PFNGLCREATESHADERPROC)GetExtension("glCreateShader");
    glDeleteShader = (PFNGLDELETESHADERPROC)GetExtension("glDeleteShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)GetExtension("glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)GetExtension("glCompileShader");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)GetExtension("glGetShaderiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)GetExtension("glGetShaderInfoLog");
    glUseProgram = (PFNGLUSEPROGRAMPROC)GetExtension("glUseProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)GetExtension("glAttachShader");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)GetExtension("glLinkProgram");
    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)GetExtension("glGetProgramiv");
    glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)GetExtension("glGetProgramInfoLog");
    glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)GetExtension("glGetAttribLocation");
    glBindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC)GetExtension("glBindAttribLocation");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)GetExtension("glGetUniformLocation");
    glGetUniformBlockIndex = (PFNGLGETUNIFORMBLOCKINDEXPROC)GetExtension("glGetUniformBlockIndex");
    glProgramUniform1i = (PFNGLPROGRAMUNIFORM1IPROC)GetExtension("glProgramUniform1i");
    glUniform1i = (PFNGLUNIFORM1IPROC)GetExtension("glUniform1i");
    glUniform1iv = (PFNGLUNIFORM1IVPROC)GetExtension("glUniform1iv");
    glUniform2iv = (PFNGLUNIFORM2IVPROC)GetExtension("glUniform2iv");
    glUniform3iv = (PFNGLUNIFORM3IVPROC)GetExtension("glUniform3iv");
    glUniform4iv = (PFNGLUNIFORM4IVPROC)GetExtension("glUniform4iv");
    glUniform1f = (PFNGLUNIFORM1FPROC)GetExtension("glUniform1f");
    glUniform1fv = (PFNGLUNIFORM1FVPROC)GetExtension("glUniform1fv");
    glUniform2fv = (PFNGLUNIFORM2FVPROC)GetExtension("glUniform2fv");
    glUniform3fv = (PFNGLUNIFORM3FVPROC)GetExtension("glUniform3fv");
    glUniform4fv = (PFNGLUNIFORM4FVPROC)GetExtension("glUniform4fv");
    glUniformMatrix2fv = (PFNGLUNIFORMMATRIX2FVPROC)GetExtension("glUniformMatrix2fv");
    glUniformMatrix2x3fv = (PFNGLUNIFORMMATRIX2X3FVPROC)GetExtension("glUniformMatrix2x3fv");
    glUniformMatrix2x4fv = (PFNGLUNIFORMMATRIX2X4FVPROC)GetExtension("glUniformMatrix2x4fv");
    glUniformMatrix3x2fv = (PFNGLUNIFORMMATRIX3X2FVPROC)GetExtension("glUniformMatrix3x2fv");
    glUniformMatrix3fv = (PFNGLUNIFORMMATRIX3FVPROC)GetExtension("glUniformMatrix3fv");
    glUniformMatrix3x4fv = (PFNGLUNIFORMMATRIX3X4FVPROC)GetExtension("glUniformMatrix3x4fv");
    glUniformMatrix4x2fv = (PFNGLUNIFORMMATRIX4X2FVPROC)GetExtension("glUniformMatrix4x2fv");
    glUniformMatrix4x3fv = (PFNGLUNIFORMMATRIX4X3FVPROC)GetExtension("glUniformMatrix4x3fv");
    glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)GetExtension("glUniformMatrix4fv");
    glGetProgramResourceIndex = (PFNGLGETPROGRAMRESOURCEINDEXPROC)GetExtension("glGetProgramResourceIndex");
    glUniformBlockBinding = (PFNGLUNIFORMBLOCKBINDINGPROC)GetExtension("glUniformBlockBinding");
    glShaderStorageBlockBinding = (PFNGLSHADERSTORAGEBLOCKBINDINGPROC)GetExtension("glShaderStorageBlockBinding");

    glDrawElementsInstanced = (PFNGLDRAWELEMENTSINSTANCEDPROC)GetExtension("glDrawElementsInstanced");
    glDispatchCompute = (PFNGLDISPATCHCOMPUTEPROC)GetExtension("glDispatchCompute");
    glMemoryBarrier = (PFNGLMEMORYBARRIERPROC)GetExtension("glMemoryBarrier");

    glGenQueries = (PFNGLGENQUERIESPROC)GetExtension("glGenQueries");
    glDeleteQueries = (PFNGLDELETEQUERIESPROC)GetExtension("glDeleteQueries");
    glIsQuery = (PFNGLISQUERYPROC)GetExtension("glIsQuery");
    glBeginQuery = (PFNGLBEGINQUERYPROC)GetExtension("glBeginQuery");
    glEndQuery = (PFNGLENDQUERYPROC)GetExtension("glEndQuery");
    glQueryCounter = (PFNGLQUERYCOUNTERPROC)GetExtension("glQueryCounter");
    glGetQueryiv = (PFNGLGETQUERYIVPROC)GetExtension("glGetQueryiv");
    glGetQueryObjectiv = (PFNGLGETQUERYOBJECTIVPROC)GetExtension("glGetQueryObjectiv");
    glGetQueryObjectuiv = (PFNGLGETQUERYOBJECTUIVPROC)GetExtension("glGetQueryObjectuiv");
    glGetQueryObjecti64v = (PFNGLGETQUERYOBJECTI64VPROC)GetExtension("glGetQueryObjecti64v");
    glGetQueryObjectui64v = (PFNGLGETQUERYOBJECTUI64VPROC)GetExtension("glGetQueryObjectui64v");

    glFenceSync = (PFNGLFENCESYNCPROC)GetExtension("glFenceSync");
    glClientWaitSync = (PFNGLCLIENTWAITSYNCPROC)GetExtension("glClientWaitSync");
    glDeleteSync = (PFNGLDELETESYNCPROC)GetExtension("glDeleteSync");
    glIsSync = (PFNGLISSYNCPROC)GetExtension("glIsSync");

    glBlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEPROC)GetExtension("glBlendFuncSeparate");
    glBlendEquationSeparate = (PFNGLBLENDEQUATIONSEPARATEPROC)GetExtension("glBlendEquationSeparate");

#if defined(OS_WINDOWS)
    glBlendColor = (PFNGLBLENDCOLORPROC)GetExtension("glBlendColor");
#endif

    glDebugMessageControl = (PFNGLDEBUGMESSAGECONTROLPROC)GetExtension("glDebugMessageControl");
    glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC)GetExtension("glDebugMessageCallback");

    glBindSampler = (PFNGLBINDSAMPLERPROC)GetExtension("glBindSampler");
    glDeleteSamplers = (PFNGLDELETESAMPLERSPROC)GetExtension("glDeleteSamplers");
    glGenSamplers = (PFNGLGENSAMPLERSPROC)GetExtension("glGenSamplers");
    glGetSamplerParameterIiv = (PFNGLGETSAMPLERPARAMETERIIVPROC)GetExtension("glGetSamplerParameterIiv");
    glGetSamplerParameterIuiv = (PFNGLGETSAMPLERPARAMETERIUIVPROC)GetExtension("glGetSamplerParameterIuiv");
    glGetSamplerParameterfv = (PFNGLGETSAMPLERPARAMETERFVPROC)GetExtension("glGetSamplerParameterfv");
    glGetSamplerParameteriv = (PFNGLGETSAMPLERPARAMETERIVPROC)GetExtension("glGetSamplerParameteriv");
    glIsSampler = (PFNGLISSAMPLERPROC)GetExtension("glIsSampler");
    glSamplerParameterIiv = (PFNGLSAMPLERPARAMETERIIVPROC)GetExtension("glSamplerParameterIiv");
    glSamplerParameterIuiv = (PFNGLSAMPLERPARAMETERIUIVPROC)GetExtension("glSamplerParameterIuiv");
    glSamplerParameterf = (PFNGLSAMPLERPARAMETERFPROC)GetExtension("glSamplerParameterf");
    glSamplerParameterfv = (PFNGLSAMPLERPARAMETERFVPROC)GetExtension("glSamplerParameterfv");
    glSamplerParameteri = (PFNGLSAMPLERPARAMETERIPROC)GetExtension("glSamplerParameteri");
    glSamplerParameteriv = (PFNGLSAMPLERPARAMETERIVPROC)GetExtension("glSamplerParameteriv");

    glExtensions.timer_query = GlCheckExtension("GL_EXT_timer_query");
    glExtensions.texture_clamp_to_border = true;  // always available
    glExtensions.buffer_storage =
        GlCheckExtension("GL_EXT_buffer_storage") || (OPENGL_VERSION_MAJOR * 10 + OPENGL_VERSION_MINOR >= 44);
    glExtensions.multi_sampled_storage =
        GlCheckExtension("GL_ARB_texture_storage_multisample") || (OPENGL_VERSION_MAJOR * 10 + OPENGL_VERSION_MINOR >= 43);
    glExtensions.multi_view = GlCheckExtension("GL_OVR_multiview2");
    glExtensions.multi_sampled_resolve = GlCheckExtension("GL_EXT_multisampled_render_to_texture");
    glExtensions.multi_view_multi_sampled_resolve = GlCheckExtension("GL_OVR_multiview_multisampled_render_to_texture");

    glExtensions.texture_clamp_to_border_id = GL_CLAMP_TO_BORDER;
}

#elif defined(OS_APPLE_MACOS)

PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glFramebufferTextureMultiviewOVR;
PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC glFramebufferTextureMultisampleMultiviewOVR;
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT;

void GlInitExtensions() {
    glExtensions.timer_query = GlCheckExtension("GL_EXT_timer_query");
    glExtensions.texture_clamp_to_border = true;  // always available
    glExtensions.buffer_storage =
        GlCheckExtension("GL_EXT_buffer_storage") || (OPENGL_VERSION_MAJOR * 10 + OPENGL_VERSION_MINOR >= 44);
    glExtensions.multi_sampled_storage =
        GlCheckExtension("GL_ARB_texture_storage_multisample") || (OPENGL_VERSION_MAJOR * 10 + OPENGL_VERSION_MINOR >= 43);
    glExtensions.multi_view = GlCheckExtension("GL_OVR_multiview2");
    glExtensions.multi_sampled_resolve = GlCheckExtension("GL_EXT_multisampled_render_to_texture");
    glExtensions.multi_view_multi_sampled_resolve = GlCheckExtension("GL_OVR_multiview_multisampled_render_to_texture");

    glExtensions.texture_clamp_to_border_id = GL_CLAMP_TO_BORDER;
}

#elif defined(OS_ANDROID)

// GL_EXT_disjoint_timer_query without _EXT
#if !defined(GL_TIMESTAMP)
#define GL_QUERY_COUNTER_BITS GL_QUERY_COUNTER_BITS_EXT
#define GL_TIME_ELAPSED GL_TIME_ELAPSED_EXT
#define GL_TIMESTAMP GL_TIMESTAMP_EXT
#define GL_GPU_DISJOINT GL_GPU_DISJOINT_EXT
#endif

// GL_EXT_buffer_storage without _EXT
#if !defined(GL_BUFFER_STORAGE_FLAGS)
#define GL_MAP_READ_BIT 0x0001                          // GL_MAP_READ_BIT_EXT
#define GL_MAP_WRITE_BIT 0x0002                         // GL_MAP_WRITE_BIT_EXT
#define GL_MAP_PERSISTENT_BIT 0x0040                    // GL_MAP_PERSISTENT_BIT_EXT
#define GL_MAP_COHERENT_BIT 0x0080                      // GL_MAP_COHERENT_BIT_EXT
#define GL_DYNAMIC_STORAGE_BIT 0x0100                   // GL_DYNAMIC_STORAGE_BIT_EXT
#define GL_CLIENT_STORAGE_BIT 0x0200                    // GL_CLIENT_STORAGE_BIT_EXT
#define GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT 0x00004000  // GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT_EXT
#define GL_BUFFER_IMMUTABLE_STORAGE 0x821F              // GL_BUFFER_IMMUTABLE_STORAGE_EXT
#define GL_BUFFER_STORAGE_FLAGS 0x8220                  // GL_BUFFER_STORAGE_FLAGS_EXT
#endif

typedef void(GL_APIENTRY *PFNGLBUFFERSTORAGEEXTPROC)(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags);
typedef void(GL_APIENTRY *PFNGLTEXSTORAGE3DMULTISAMPLEPROC)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
                                                            GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);

// EGL_KHR_fence_sync, GL_OES_EGL_sync, VG_KHR_EGL_sync
PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR;
PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR;
PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR;
PFNEGLGETSYNCATTRIBKHRPROC eglGetSyncAttribKHR;

// GL_EXT_disjoint_timer_query
PFNGLQUERYCOUNTEREXTPROC glQueryCounter;
PFNGLGETQUERYOBJECTI64VEXTPROC glGetQueryObjecti64v;
PFNGLGETQUERYOBJECTUI64VEXTPROC glGetQueryObjectui64v;

// GL_EXT_buffer_storage
PFNGLBUFFERSTORAGEEXTPROC glBufferStorage;

// GL_OVR_multiview
PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glFramebufferTextureMultiviewOVR;

// GL_EXT_multisampled_render_to_texture
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT;
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT;

// GL_OVR_multiview_multisampled_render_to_texture
PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC glFramebufferTextureMultisampleMultiviewOVR;

#ifndef GL_ES_VERSION_3_2
PFNGLTEXSTORAGE3DMULTISAMPLEPROC glTexStorage3DMultisample;
#endif

#if !defined(EGL_OPENGL_ES3_BIT)
#define EGL_OPENGL_ES3_BIT 0x0040
#endif

// GL_EXT_texture_cube_map_array
#if !defined(GL_TEXTURE_CUBE_MAP_ARRAY)
#define GL_TEXTURE_CUBE_MAP_ARRAY 0x9009
#endif

// GL_EXT_texture_filter_anisotropic
#if !defined(GL_TEXTURE_MAX_ANISOTROPY_EXT)
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

// GL_EXT_texture_border_clamp or GL_OES_texture_border_clamp
#if !defined(GL_CLAMP_TO_BORDER)
#define GL_CLAMP_TO_BORDER 0x812D
#endif

// No 1D textures in OpenGL ES.
#if !defined(GL_TEXTURE_1D)
#define GL_TEXTURE_1D 0x0DE0
#endif

// No 1D texture arrays in OpenGL ES.
#if !defined(GL_TEXTURE_1D_ARRAY)
#define GL_TEXTURE_1D_ARRAY 0x8C18
#endif

// No multi-sampled texture arrays in OpenGL ES.
#if !defined(GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
#define GL_TEXTURE_2D_MULTISAMPLE_ARRAY 0x9102
#endif

void GlInitExtensions() {
    eglCreateSyncKHR = (PFNEGLCREATESYNCKHRPROC)GetExtension("eglCreateSyncKHR");
    eglDestroySyncKHR = (PFNEGLDESTROYSYNCKHRPROC)GetExtension("eglDestroySyncKHR");
    eglClientWaitSyncKHR = (PFNEGLCLIENTWAITSYNCKHRPROC)GetExtension("eglClientWaitSyncKHR");
    eglGetSyncAttribKHR = (PFNEGLGETSYNCATTRIBKHRPROC)GetExtension("eglGetSyncAttribKHR");

    glQueryCounter = (PFNGLQUERYCOUNTEREXTPROC)GetExtension("glQueryCounterEXT");
    glGetQueryObjecti64v = (PFNGLGETQUERYOBJECTI64VEXTPROC)GetExtension("glGetQueryObjecti64vEXT");
    glGetQueryObjectui64v = (PFNGLGETQUERYOBJECTUI64VEXTPROC)GetExtension("glGetQueryObjectui64vEXT");

    glBufferStorage = (PFNGLBUFFERSTORAGEEXTPROC)GetExtension("glBufferStorageEXT");

    glRenderbufferStorageMultisampleEXT =
        (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)GetExtension("glRenderbufferStorageMultisampleEXT");
    glFramebufferTexture2DMultisampleEXT =
        (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)GetExtension("glFramebufferTexture2DMultisampleEXT");
    glFramebufferTextureMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)GetExtension("glFramebufferTextureMultiviewOVR");
    glFramebufferTextureMultisampleMultiviewOVR =
        (PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)GetExtension("glFramebufferTextureMultisampleMultiviewOVR");

#ifndef GL_ES_VERSION_3_2
    glTexStorage3DMultisample = (PFNGLTEXSTORAGE3DMULTISAMPLEPROC)GetExtension("glTexStorage3DMultisample");
#endif

    glExtensions.timer_query = GlCheckExtension("GL_EXT_disjoint_timer_query");
    glExtensions.texture_clamp_to_border =
        GlCheckExtension("GL_EXT_texture_border_clamp") || GlCheckExtension("GL_OES_texture_border_clamp");
    glExtensions.buffer_storage = GlCheckExtension("GL_EXT_buffer_storage");
    glExtensions.multi_view = GlCheckExtension("GL_OVR_multiview2");
    glExtensions.multi_sampled_resolve = GlCheckExtension("GL_EXT_multisampled_render_to_texture");
    glExtensions.multi_view_multi_sampled_resolve = GlCheckExtension("GL_OVR_multiview_multisampled_render_to_texture");

    glExtensions.texture_clamp_to_border_id =
        (GlCheckExtension("GL_OES_texture_border_clamp")
             ? GL_CLAMP_TO_BORDER
             : (GlCheckExtension("GL_EXT_texture_border_clamp") ? GL_CLAMP_TO_BORDER : (GL_CLAMP_TO_EDGE)));
}

#endif

/*
================================================================================================================================

Driver Instance.

================================================================================================================================
*/

bool ksDriverInstance_Create(ksDriverInstance *instance) {
    memset(instance, 0, sizeof(ksDriverInstance));
    return true;
}

void ksDriverInstance_Destroy(ksDriverInstance *instance) { memset(instance, 0, sizeof(ksDriverInstance)); }

/*
================================================================================================================================

GPU Device.

================================================================================================================================
*/

bool ksGpuDevice_Create(ksGpuDevice *device, ksDriverInstance *instance, const ksGpuQueueInfo *queueInfo) {
    /*
            Use an extensions to select the appropriate device:
            https://www.opengl.org/registry/specs/NV/gpu_affinity.txt
            https://www.opengl.org/registry/specs/AMD/wgl_gpu_association.txt
            https://www.opengl.org/registry/specs/AMD/glx_gpu_association.txt

            On Linux configure each GPU to use a separate X screen and then select
            the X screen to render to.
    */

    memset(device, 0, sizeof(ksGpuDevice));

    device->instance = instance;
    device->queueInfo = *queueInfo;

    return true;
}

void ksGpuDevice_Destroy(ksGpuDevice *device) { memset(device, 0, sizeof(ksGpuDevice)); }

/*
================================================================================================================================

GPU Context.

================================================================================================================================
*/

ksGpuSurfaceBits ksGpuContext_BitsForSurfaceFormat(const ksGpuSurfaceColorFormat colorFormat,
                                                   const ksGpuSurfaceDepthFormat depthFormat) {
    ksGpuSurfaceBits bits;
    bits.redBits = ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R8G8B8A8)
                        ? 8
                        : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8)
                               ? 8
                               : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R5G6B5)
                                      ? 5
                                      : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B5G6R5) ? 5 : 8))));
    bits.greenBits = ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R8G8B8A8)
                          ? 8
                          : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8)
                                 ? 8
                                 : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R5G6B5)
                                        ? 6
                                        : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B5G6R5) ? 6 : 8))));
    bits.blueBits = ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R8G8B8A8)
                         ? 8
                         : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8)
                                ? 8
                                : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R5G6B5)
                                       ? 5
                                       : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B5G6R5) ? 5 : 8))));
    bits.alphaBits = ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R8G8B8A8)
                          ? 8
                          : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8)
                                 ? 8
                                 : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R5G6B5)
                                        ? 0
                                        : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B5G6R5) ? 0 : 8))));
    bits.colorBits = bits.redBits + bits.greenBits + bits.blueBits + bits.alphaBits;
    bits.depthBits =
        ((depthFormat == KS_GPU_SURFACE_DEPTH_FORMAT_D16) ? 16 : ((depthFormat == KS_GPU_SURFACE_DEPTH_FORMAT_D24) ? 24 : 0));
    return bits;
}

GLenum ksGpuContext_InternalSurfaceColorFormat(const ksGpuSurfaceColorFormat colorFormat) {
    return ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R8G8B8A8)
                ? GL_RGBA8
                : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8)
                       ? GL_RGBA8
                       : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R5G6B5)
                              ? GL_RGB565
                              : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B5G6R5) ? GL_RGB565 : GL_RGBA8))));
}

GLenum ksGpuContext_InternalSurfaceDepthFormat(const ksGpuSurfaceDepthFormat depthFormat) {
    return ((depthFormat == KS_GPU_SURFACE_DEPTH_FORMAT_D16)
                ? GL_DEPTH_COMPONENT16
                : ((depthFormat == KS_GPU_SURFACE_DEPTH_FORMAT_D24) ? GL_DEPTH_COMPONENT24 : GL_DEPTH_COMPONENT24));
}

#if defined(OS_WINDOWS)

static bool ksGpuContext_CreateForSurface(ksGpuContext *context, const ksGpuDevice *device, const int queueIndex,
                                          const ksGpuSurfaceColorFormat colorFormat, const ksGpuSurfaceDepthFormat depthFormat,
                                          const ksGpuSampleCount sampleCount, HINSTANCE hInstance, HDC hDC) {
    UNUSED_PARM(queueIndex);

    context->device = device;

    const ksGpuSurfaceBits bits = ksGpuContext_BitsForSurfaceFormat(colorFormat, depthFormat);

    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,                        // version
        PFD_DRAW_TO_WINDOW |      // must support windowed
            PFD_SUPPORT_OPENGL |  // must support OpenGL
            PFD_DOUBLEBUFFER,     // must support double buffering
        PFD_TYPE_RGBA,            // iPixelType
        bits.colorBits,           // cColorBits
        0,
        0,  // cRedBits, cRedShift
        0,
        0,  // cGreenBits, cGreenShift
        0,
        0,  // cBlueBits, cBlueShift
        0,
        0,               // cAlphaBits, cAlphaShift
        0,               // cAccumBits
        0,               // cAccumRedBits
        0,               // cAccumGreenBits
        0,               // cAccumBlueBits
        0,               // cAccumAlphaBits
        bits.depthBits,  // cDepthBits
        0,               // cStencilBits
        0,               // cAuxBuffers
        PFD_MAIN_PLANE,  // iLayerType
        0,               // bReserved
        0,               // dwLayerMask
        0,               // dwVisibleMask
        0                // dwDamageMask
    };

    HWND localWnd = NULL;
    HDC localDC = hDC;

    if (sampleCount > KS_GPU_SAMPLE_COUNT_1) {
        // A valid OpenGL context is needed to get OpenGL extensions including wglChoosePixelFormatARB
        // and wglCreateContextAttribsARB. A device context with a valid pixel format is needed to create
        // an OpenGL context. However, once a pixel format is set on a device context it is final.
        // Therefore a pixel format is set on the device context of a temporary window to create a context
        // to get the extensions for multi-sampling.
        localWnd = CreateWindowA(APPLICATION_NAME, "temp", 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);
        localDC = GetDC(localWnd);
    }

    int pixelFormat = ChoosePixelFormat(localDC, &pfd);
    if (pixelFormat == 0) {
        Error("Failed to find a suitable pixel format.");
        return false;
    }

    if (!SetPixelFormat(localDC, pixelFormat, &pfd)) {
        Error("Failed to set the pixel format.");
        return false;
    }

    // Now that the pixel format is set, create a temporary context to get the extensions.
    {
        HGLRC hGLRC = wglCreateContext(localDC);
        wglMakeCurrent(localDC, hGLRC);

        GlBootstrapExtensions();

        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(hGLRC);
    }

    if (sampleCount > KS_GPU_SAMPLE_COUNT_1) {
        // Release the device context and destroy the window that were created to get extensions.
        ReleaseDC(localWnd, localDC);
        DestroyWindow(localWnd);

        int pixelFormatAttribs[] = {WGL_DRAW_TO_WINDOW_ARB,
                                    GL_TRUE,
                                    WGL_SUPPORT_OPENGL_ARB,
                                    GL_TRUE,
                                    WGL_DOUBLE_BUFFER_ARB,
                                    GL_TRUE,
                                    WGL_PIXEL_TYPE_ARB,
                                    WGL_TYPE_RGBA_ARB,
                                    WGL_COLOR_BITS_ARB,
                                    bits.colorBits,
                                    WGL_DEPTH_BITS_ARB,
                                    bits.depthBits,
                                    WGL_SAMPLE_BUFFERS_ARB,
                                    1,
                                    WGL_SAMPLES_ARB,
                                    sampleCount,
                                    0};

        unsigned int numPixelFormats = 0;

        if (!wglChoosePixelFormatARB(hDC, pixelFormatAttribs, NULL, 1, &pixelFormat, &numPixelFormats) || numPixelFormats == 0) {
            Error("Failed to find MSAA pixel format.");
            return false;
        }

        memset(&pfd, 0, sizeof(pfd));

        if (!DescribePixelFormat(hDC, pixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &pfd)) {
            Error("Failed to describe the pixel format.");
            return false;
        }

        if (!SetPixelFormat(hDC, pixelFormat, &pfd)) {
            Error("Failed to set the pixel format.");
            return false;
        }
    }

    int contextAttribs[] = {WGL_CONTEXT_MAJOR_VERSION_ARB,
                            OPENGL_VERSION_MAJOR,
                            WGL_CONTEXT_MINOR_VERSION_ARB,
                            OPENGL_VERSION_MINOR,
                            WGL_CONTEXT_PROFILE_MASK_ARB,
                            WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                            WGL_CONTEXT_FLAGS_ARB,
                            WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB | WGL_CONTEXT_DEBUG_BIT_ARB,
                            0};

    context->hDC = hDC;
    context->hGLRC = wglCreateContextAttribsARB(hDC, NULL, contextAttribs);
    if (!context->hGLRC) {
        Error("Failed to create GL context.");
        return false;
    }

    wglMakeCurrent(hDC, context->hGLRC);

    GlInitExtensions();

    return true;
}

#elif defined(OS_LINUX_XLIB) || defined(OS_LINUX_XCB_GLX)

static int glxGetFBConfigAttrib2(Display *dpy, GLXFBConfig config, int attribute) {
    int value;
    glXGetFBConfigAttrib(dpy, config, attribute, &value);
    return value;
}

static bool ksGpuContext_CreateForSurface(ksGpuContext *context, const ksGpuDevice *device, const int queueIndex,
                                          const ksGpuSurfaceColorFormat colorFormat, const ksGpuSurfaceDepthFormat depthFormat,
                                          const ksGpuSampleCount sampleCount, Display *xDisplay, int xScreen) {
    UNUSED_PARM(queueIndex);

    context->device = device;

    int glxErrorBase;
    int glxEventBase;
    if (!glXQueryExtension(xDisplay, &glxErrorBase, &glxEventBase)) {
        Error("X display does not support the GLX extension.");
        return false;
    }

    int glxVersionMajor;
    int glxVersionMinor;
    if (!glXQueryVersion(xDisplay, &glxVersionMajor, &glxVersionMinor)) {
        Error("Unable to retrieve GLX version.");
        return false;
    }

    int fbConfigCount = 0;
    GLXFBConfig *fbConfigs = glXGetFBConfigs(xDisplay, xScreen, &fbConfigCount);
    if (fbConfigCount == 0) {
        Error("No valid framebuffer configurations found.");
        return false;
    }

    const ksGpuSurfaceBits bits = ksGpuContext_BitsForSurfaceFormat(colorFormat, depthFormat);

    bool foundFbConfig = false;
    for (int i = 0; i < fbConfigCount; i++) {
        if (glxGetFBConfigAttrib2(xDisplay, fbConfigs[i], GLX_FBCONFIG_ID) == 0) {
            continue;
        }
        if (glxGetFBConfigAttrib2(xDisplay, fbConfigs[i], GLX_VISUAL_ID) == 0) {
            continue;
        }
        if (glxGetFBConfigAttrib2(xDisplay, fbConfigs[i], GLX_DOUBLEBUFFER) == 0) {
            continue;
        }
        if ((glxGetFBConfigAttrib2(xDisplay, fbConfigs[i], GLX_RENDER_TYPE) & GLX_RGBA_BIT) == 0) {
            continue;
        }
        if ((glxGetFBConfigAttrib2(xDisplay, fbConfigs[i], GLX_DRAWABLE_TYPE) & GLX_WINDOW_BIT) == 0) {
            continue;
        }
        if (glxGetFBConfigAttrib2(xDisplay, fbConfigs[i], GLX_RED_SIZE) != bits.redBits) {
            continue;
        }
        if (glxGetFBConfigAttrib2(xDisplay, fbConfigs[i], GLX_GREEN_SIZE) != bits.greenBits) {
            continue;
        }
        if (glxGetFBConfigAttrib2(xDisplay, fbConfigs[i], GLX_BLUE_SIZE) != bits.blueBits) {
            continue;
        }
        if (glxGetFBConfigAttrib2(xDisplay, fbConfigs[i], GLX_ALPHA_SIZE) != bits.alphaBits) {
            continue;
        }
        if (glxGetFBConfigAttrib2(xDisplay, fbConfigs[i], GLX_DEPTH_SIZE) != bits.depthBits) {
            continue;
        }
        if (sampleCount > KS_GPU_SAMPLE_COUNT_1) {
            if (glxGetFBConfigAttrib2(xDisplay, fbConfigs[i], GLX_SAMPLE_BUFFERS) != 1) {
                continue;
            }
            if (glxGetFBConfigAttrib2(xDisplay, fbConfigs[i], GLX_SAMPLES) != (int)sampleCount) {
                continue;
            }
        }

        context->visualid = glxGetFBConfigAttrib2(xDisplay, fbConfigs[i], GLX_VISUAL_ID);
        context->glxFBConfig = fbConfigs[i];
        foundFbConfig = true;
        break;
    }

    XFree(fbConfigs);

    if (!foundFbConfig) {
        Error("Failed to to find desired framebuffer configuration.");
        return false;
    }

    context->xDisplay = xDisplay;

    int attribs[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
                     OPENGL_VERSION_MAJOR,
                     GLX_CONTEXT_MINOR_VERSION_ARB,
                     OPENGL_VERSION_MINOR,
                     GLX_CONTEXT_PROFILE_MASK_ARB,
                     GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
                     GLX_CONTEXT_FLAGS_ARB,
                     GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
                     0};

    glXCreateContextAttribsARB = (PFNGLXCREATECONTEXTATTRIBSARBPROC)GetExtension("glXCreateContextAttribsARB");

    context->glxContext = glXCreateContextAttribsARB(xDisplay,              // Display *	dpy
                                                     context->glxFBConfig,  // GLXFBConfig	config
                                                     NULL,                  // GLXContext	share_context
                                                     True,                  // Bool			direct
                                                     attribs);              // const int *	attrib_list

    if (context->glxContext == NULL) {
        Error("Unable to create GLX context.");
        return false;
    }

    if (!glXIsDirect(xDisplay, context->glxContext)) {
        Error("Unable to create direct rendering context.");
        return false;
    }

    return true;
}

#elif defined(OS_LINUX_XCB)

static uint32_t xcb_glx_get_property(const uint32_t *properties, const uint32_t numProperties, uint32_t propertyName) {
    for (uint32_t i = 0; i < numProperties; i++) {
        if (properties[i * 2 + 0] == propertyName) {
            return properties[i * 2 + 1];
        }
    }
    return 0;
}

static bool ksGpuContext_CreateForSurface(ksGpuContext *context, const ksGpuDevice *device, const int queueIndex,
                                          const ksGpuSurfaceColorFormat colorFormat, const ksGpuSurfaceDepthFormat depthFormat,
                                          const ksGpuSampleCount sampleCount, xcb_connection_t *connection, int screen_number) {
    UNUSED_PARM(queueIndex);

    context->device = device;

    GlInitExtensions();

    xcb_glx_query_version_cookie_t glx_query_version_cookie =
        xcb_glx_query_version(connection, OPENGL_VERSION_MAJOR, OPENGL_VERSION_MINOR);
    xcb_glx_query_version_reply_t *glx_query_version_reply =
        xcb_glx_query_version_reply(connection, glx_query_version_cookie, NULL);
    if (glx_query_version_reply == NULL) {
        Error("Unable to retrieve GLX version.");
        return false;
    }
    free(glx_query_version_reply);

    xcb_glx_get_fb_configs_cookie_t get_fb_configs_cookie = xcb_glx_get_fb_configs(connection, screen_number);
    xcb_glx_get_fb_configs_reply_t *get_fb_configs_reply = xcb_glx_get_fb_configs_reply(connection, get_fb_configs_cookie, NULL);

    if (get_fb_configs_reply == NULL || get_fb_configs_reply->num_FB_configs == 0) {
        Error("No valid framebuffer configurations found.");
        return false;
    }

    const ksGpuSurfaceBits bits = ksGpuContext_BitsForSurfaceFormat(colorFormat, depthFormat);

    const uint32_t *fb_configs_properties = xcb_glx_get_fb_configs_property_list(get_fb_configs_reply);
    const uint32_t fb_configs_num_properties = get_fb_configs_reply->num_properties;

    bool foundFbConfig = false;
    for (uint32_t i = 0; i < get_fb_configs_reply->num_FB_configs; i++) {
        const uint32_t *fb_config = fb_configs_properties + i * fb_configs_num_properties * 2;

        if (xcb_glx_get_property(fb_config, fb_configs_num_properties, GLX_FBCONFIG_ID) == 0) {
            continue;
        }
        if (xcb_glx_get_property(fb_config, fb_configs_num_properties, GLX_VISUAL_ID) == 0) {
            continue;
        }
        if (xcb_glx_get_property(fb_config, fb_configs_num_properties, GLX_DOUBLEBUFFER) == 0) {
            continue;
        }
        if ((xcb_glx_get_property(fb_config, fb_configs_num_properties, GLX_RENDER_TYPE) & GLX_RGBA_BIT) == 0) {
            continue;
        }
        if ((xcb_glx_get_property(fb_config, fb_configs_num_properties, GLX_DRAWABLE_TYPE) & GLX_WINDOW_BIT) == 0) {
            continue;
        }
        if (xcb_glx_get_property(fb_config, fb_configs_num_properties, GLX_RED_SIZE) != bits.redBits) {
            continue;
        }
        if (xcb_glx_get_property(fb_config, fb_configs_num_properties, GLX_GREEN_SIZE) != bits.greenBits) {
            continue;
        }
        if (xcb_glx_get_property(fb_config, fb_configs_num_properties, GLX_BLUE_SIZE) != bits.blueBits) {
            continue;
        }
        if (xcb_glx_get_property(fb_config, fb_configs_num_properties, GLX_ALPHA_SIZE) != bits.alphaBits) {
            continue;
        }
        if (xcb_glx_get_property(fb_config, fb_configs_num_properties, GLX_DEPTH_SIZE) != bits.depthBits) {
            continue;
        }
        if (sampleCount > KS_GPU_SAMPLE_COUNT_1) {
            if (xcb_glx_get_property(fb_config, fb_configs_num_properties, GLX_SAMPLE_BUFFERS) != 1) {
                continue;
            }
            if (xcb_glx_get_property(fb_config, fb_configs_num_properties, GLX_SAMPLES) != sampleCount) {
                continue;
            }
        }

        context->fbconfigid = xcb_glx_get_property(fb_config, fb_configs_num_properties, GLX_FBCONFIG_ID);
        context->visualid = xcb_glx_get_property(fb_config, fb_configs_num_properties, GLX_VISUAL_ID);
        foundFbConfig = true;
        break;
    }

    free(get_fb_configs_reply);

    if (!foundFbConfig) {
        Error("Failed to to find desired framebuffer configuration.");
        return false;
    }

    context->connection = connection;
    context->screen_number = screen_number;

    // Create the context.
    uint32_t attribs[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
                          OPENGL_VERSION_MAJOR,
                          GLX_CONTEXT_MINOR_VERSION_ARB,
                          OPENGL_VERSION_MINOR,
                          GLX_CONTEXT_PROFILE_MASK_ARB,
                          GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
                          GLX_CONTEXT_FLAGS_ARB,
                          GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
                          0};

    context->glxContext = xcb_generate_id(connection);
    xcb_glx_create_context_attribs_arb(connection,           // xcb_connection_t *	connection
                                       context->glxContext,  // xcb_glx_context_t	context
                                       context->fbconfigid,  // xcb_glx_fbconfig_t	fbconfig
                                       screen_number,        // uint32_t				screen
                                       0,                    // xcb_glx_context_t	share_list
                                       1,                    // uint8_t				is_direct
                                       4,                    // uint32_t				num_attribs
                                       attribs);             // const uint32_t *		attribs

    // Make sure the context is direct.
    xcb_generic_error_t *error;
    xcb_glx_is_direct_cookie_t glx_is_direct_cookie = xcb_glx_is_direct_unchecked(connection, context->glxContext);
    xcb_glx_is_direct_reply_t *glx_is_direct_reply = xcb_glx_is_direct_reply(connection, glx_is_direct_cookie, &error);
    const bool is_direct = (glx_is_direct_reply != NULL && glx_is_direct_reply->is_direct);
    free(glx_is_direct_reply);

    if (!is_direct) {
        Error("Unable to create direct rendering context.");
        return false;
    }

    return true;
}

#elif defined(OS_LINUX_WAYLAND)

static bool ksGpuContext_CreateForSurface(ksGpuContext *context, const ksGpuDevice *device, struct wl_display *native_display) {
    context->device = device;

    EGLint numConfigs;
    EGLint majorVersion;
    EGLint minorVersion;

    EGLint fbAttribs[] = {EGL_SURFACE_TYPE,
                          EGL_WINDOW_BIT,
                          EGL_RENDERABLE_TYPE,
                          EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                          EGL_RED_SIZE,
                          8,
                          EGL_GREEN_SIZE,
                          8,
                          EGL_BLUE_SIZE,
                          8,
                          EGL_NONE};

    EGLint contextAttribs[] = {EGL_CONTEXT_OPENGL_PROFILE_MASK,
                               EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                               EGL_CONTEXT_CLIENT_VERSION,
                               OPENGL_VERSION_MAJOR,
                               EGL_CONTEXT_MINOR_VERSION,
                               OPENGL_VERSION_MINOR,
                               EGL_NONE};

    context->display = eglGetDisplay(native_display);
    if (context->display == EGL_NO_DISPLAY) {
        Error("Could not create EGL Display.");
        return false;
    }

    if (!eglInitialize(context->display, &majorVersion, &minorVersion)) {
        Error("eglInitialize failed.");
        return false;
    }

    printf("Initialized EGL context version %d.%d\n", majorVersion, minorVersion);

    EGLBoolean ret = eglGetConfigs(context->display, NULL, 0, &numConfigs);
    if (ret != EGL_TRUE || numConfigs == 0) {
        Error("eglGetConfigs failed.");
        return false;
    }

    ret = eglChooseConfig(context->display, fbAttribs, &context->config, 1, &numConfigs);
    if (ret != EGL_TRUE || numConfigs != 1) {
        Error("eglChooseConfig failed.");
        return false;
    }

    context->mainSurface = eglCreateWindowSurface(context->display, context->config, context->native_window, NULL);
    if (context->mainSurface == EGL_NO_SURFACE) {
        Error("eglCreateWindowSurface failed");
        return false;
    }

    eglBindAPI(EGL_OPENGL_API);

    context->context = eglCreateContext(context->display, context->config, EGL_NO_CONTEXT, contextAttribs);
    if (context->context == EGL_NO_CONTEXT) {
        Error("Could not create OpenGL context.");
        return false;
    }

    if (!eglMakeCurrent(context->display, context->mainSurface, context->mainSurface, context->context)) {
        Error("Could not make the current context current.");
        return false;
    }

    GlInitExtensions();

    return true;
}

#elif defined(OS_APPLE_MACOS)

static bool ksGpuContext_CreateForSurface(ksGpuContext *context, const ksGpuDevice *device, const int queueIndex,
                                          const ksGpuSurfaceColorFormat colorFormat, const ksGpuSurfaceDepthFormat depthFormat,
                                          const ksGpuSampleCount sampleCount, CGDirectDisplayID display) {
    UNUSED_PARM(queueIndex);

    context->device = device;

    const ksGpuSurfaceBits bits = ksGpuContext_BitsForSurfaceFormat(colorFormat, depthFormat);

    NSOpenGLPixelFormatAttribute pixelFormatAttributes[] = {NSOpenGLPFAMinimumPolicy,
                                                            1,
                                                            NSOpenGLPFAScreenMask,
                                                            CGDisplayIDToOpenGLDisplayMask(display),
                                                            NSOpenGLPFAAccelerated,
                                                            NSOpenGLPFAOpenGLProfile,
                                                            NSOpenGLProfileVersion3_2Core,
                                                            NSOpenGLPFADoubleBuffer,
                                                            NSOpenGLPFAColorSize,
                                                            bits.colorBits,
                                                            NSOpenGLPFADepthSize,
                                                            bits.depthBits,
                                                            NSOpenGLPFASampleBuffers,
                                                            (sampleCount > KS_GPU_SAMPLE_COUNT_1),
                                                            NSOpenGLPFASamples,
                                                            sampleCount,
                                                            0};

    NSOpenGLPixelFormat *pixelFormat = [[[NSOpenGLPixelFormat alloc] initWithAttributes:pixelFormatAttributes] autorelease];
    if (pixelFormat == nil) {
        Error("Failed : NSOpenGLPixelFormat.");
        return false;
    }
    context->nsContext = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil];
    if (context->nsContext == nil) {
        Error("Failed : NSOpenGLContext.");
        return false;
    }

    context->cglContext = [context->nsContext CGLContextObj];

    GlInitExtensions();

    return true;
}

#elif defined(OS_ANDROID)

static bool ksGpuContext_CreateForSurface(ksGpuContext *context, const ksGpuDevice *device, const int queueIndex,
                                          const ksGpuSurfaceColorFormat colorFormat, const ksGpuSurfaceDepthFormat depthFormat,
                                          const ksGpuSampleCount sampleCount, EGLDisplay display) {
    context->device = device;

    context->display = display;

    // Do NOT use eglChooseConfig, because the Android EGL code pushes in multisample
    // flags in eglChooseConfig when the user has selected the "force 4x MSAA" option in
    // settings, and that is completely wasted on the time warped frontbuffer.
    enum { MAX_CONFIGS = 1024 };
    EGLConfig configs[MAX_CONFIGS];
    EGLint numConfigs = 0;
    EGL(eglGetConfigs(display, configs, MAX_CONFIGS, &numConfigs));

    const ksGpuSurfaceBits bits = ksGpuContext_BitsForSurfaceFormat(colorFormat, depthFormat);

    const EGLint configAttribs[] = {EGL_RED_SIZE, bits.greenBits, EGL_GREEN_SIZE, bits.redBits, EGL_BLUE_SIZE, bits.blueBits,
                                    EGL_ALPHA_SIZE, bits.alphaBits, EGL_DEPTH_SIZE, bits.depthBits,
                                    // EGL_STENCIL_SIZE,	0,
                                    EGL_SAMPLE_BUFFERS, (sampleCount > KS_GPU_SAMPLE_COUNT_1), EGL_SAMPLES,
                                    (sampleCount > KS_GPU_SAMPLE_COUNT_1) ? sampleCount : 0, EGL_NONE};

    context->config = 0;
    for (int i = 0; i < numConfigs; i++) {
        EGLint value = 0;

        eglGetConfigAttrib(display, configs[i], EGL_RENDERABLE_TYPE, &value);
        if ((value & EGL_OPENGL_ES3_BIT) != EGL_OPENGL_ES3_BIT) {
            continue;
        }

        // Without EGL_KHR_surfaceless_context, the config needs to support both pbuffers and window surfaces.
        eglGetConfigAttrib(display, configs[i], EGL_SURFACE_TYPE, &value);
        if ((value & (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) != (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) {
            continue;
        }

        int j = 0;
        for (; configAttribs[j] != EGL_NONE; j += 2) {
            eglGetConfigAttrib(display, configs[i], configAttribs[j], &value);
            if (value != configAttribs[j + 1]) {
                break;
            }
        }
        if (configAttribs[j] == EGL_NONE) {
            context->config = configs[i];
            break;
        }
    }
    if (context->config == 0) {
        Error("Failed to find EGLConfig");
        return false;
    }

    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, OPENGL_VERSION_MAJOR, EGL_NONE, EGL_NONE, EGL_NONE};
    // Use the default priority if KS_GPU_QUEUE_PRIORITY_MEDIUM is selected.
    const ksGpuQueuePriority priority = device->queueInfo.queuePriorities[queueIndex];
    if (priority != KS_GPU_QUEUE_PRIORITY_MEDIUM) {
        contextAttribs[2] = EGL_CONTEXT_PRIORITY_LEVEL_IMG;
        contextAttribs[3] = (priority == KS_GPU_QUEUE_PRIORITY_LOW) ? EGL_CONTEXT_PRIORITY_LOW_IMG : EGL_CONTEXT_PRIORITY_HIGH_IMG;
    }
    context->context = eglCreateContext(display, context->config, EGL_NO_CONTEXT, contextAttribs);
    if (context->context == EGL_NO_CONTEXT) {
        Error("eglCreateContext() failed: %s", EglErrorString(eglGetError()));
        return false;
    }

    const EGLint surfaceAttribs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    context->tinySurface = eglCreatePbufferSurface(display, context->config, surfaceAttribs);
    if (context->tinySurface == EGL_NO_SURFACE) {
        Error("eglCreatePbufferSurface() failed: %s", EglErrorString(eglGetError()));
        eglDestroyContext(display, context->context);
        context->context = EGL_NO_CONTEXT;
        return false;
    }
    context->mainSurface = context->tinySurface;

    return true;
}

#endif

bool ksGpuContext_CreateShared(ksGpuContext *context, const ksGpuContext *other, int queueIndex) {
    UNUSED_PARM(queueIndex);

    memset(context, 0, sizeof(ksGpuContext));

    context->device = other->device;

#if defined(OS_WINDOWS)
    context->hDC = other->hDC;
    context->hGLRC = wglCreateContext(other->hDC);
    if (!wglShareLists(other->hGLRC, context->hGLRC)) {
        return false;
    }
#elif defined(OS_LINUX_XLIB) || defined(OS_LINUX_XCB_GLX)
    context->xDisplay = other->xDisplay;
    context->visualid = other->visualid;
    context->glxFBConfig = other->glxFBConfig;
    context->glxDrawable = other->glxDrawable;
    context->glxContext = glXCreateNewContext(other->xDisplay, other->glxFBConfig, GLX_RGBA_TYPE, other->glxContext, True);
    if (context->glxContext == NULL) {
        return false;
    }
#elif defined(OS_LINUX_XCB)
    context->connection = other->connection;
    context->screen_number = other->screen_number;
    context->fbconfigid = other->fbconfigid;
    context->visualid = other->visualid;
    context->glxDrawable = other->glxDrawable;
    context->glxContext = xcb_generate_id(other->connection);
    xcb_glx_create_context(other->connection, context->glxContext, other->visualid, other->screen_number, other->glxContext, 1);
    context->glxContextTag = 0;
#elif defined(OS_APPLE_MACOS)
    context->nsContext = NULL;
    CGLPixelFormatObj pf = CGLGetPixelFormat(other->cglContext);
    if (CGLCreateContext(pf, other->cglContext, &context->cglContext) != kCGLNoError) {
        Error("Failed : CGLCreateContext.");
        return false;
    }
    CGSConnectionID cid;
    CGSWindowID wid;
    CGSSurfaceID sid;
    if (CGLGetSurface(other->cglContext, &cid, &wid, &sid) != kCGLNoError) {
        Error("Failed : CGLGetSurface.");
        return false;
    }
    if (CGLSetSurface(context->cglContext, cid, wid, sid) != kCGLNoError) {
        Error("Failed : CGLSetSurface.");
        return false;
    }
#elif defined(OS_ANDROID) || defined(OS_LINUX_WAYLAND)
    context->display = other->display;
    EGLint configID;
    if (!eglQueryContext(context->display, other->context, EGL_CONFIG_ID, &configID)) {
        Error("eglQueryContext EGL_CONFIG_ID failed: %s", EglErrorString(eglGetError()));
        return false;
    }
    enum { MAX_CONFIGS = 1024 };
    EGLConfig configs[MAX_CONFIGS];
    EGLint numConfigs = 0;
    EGL(eglGetConfigs(context->display, configs, MAX_CONFIGS, &numConfigs));
    context->config = 0;
    for (int i = 0; i < numConfigs; i++) {
        EGLint value = 0;
        eglGetConfigAttrib(context->display, configs[i], EGL_CONFIG_ID, &value);
        if (value == configID) {
            context->config = configs[i];
            break;
        }
    }
    if (context->config == 0) {
        Error("Failed to find share context config.");
        return false;
    }
    EGLint surfaceType = 0;
    eglGetConfigAttrib(context->display, context->config, EGL_SURFACE_TYPE, &surfaceType);

#if defined(OS_ANDROID)
    if ((surfaceType & EGL_PBUFFER_BIT) == 0) {
        Error("Share context config does have EGL_PBUFFER_BIT.");
        return false;
    }
#endif
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, OPENGL_VERSION_MAJOR, EGL_NONE};
    context->context = eglCreateContext(context->display, context->config, other->context, contextAttribs);
    if (context->context == EGL_NO_CONTEXT) {
        Error("eglCreateContext() failed: %s", EglErrorString(eglGetError()));
        return false;
    }
#if defined(OS_ANDROID)
    const EGLint surfaceAttribs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    context->tinySurface = eglCreatePbufferSurface(context->display, context->config, surfaceAttribs);
    if (context->tinySurface == EGL_NO_SURFACE) {
        Error("eglCreatePbufferSurface() failed: %s", EglErrorString(eglGetError()));
        eglDestroyContext(context->display, context->context);
        context->context = EGL_NO_CONTEXT;
        return false;
    }
    context->mainSurface = context->tinySurface;
#endif
#endif
    return true;
}

void ksGpuContext_Destroy(ksGpuContext *context) {
#if defined(OS_WINDOWS)
    if (context->hGLRC) {
        if (!wglMakeCurrent(NULL, NULL)) {
            DWORD error = GetLastError();
            Error("Failed to release context error code (%d).", error);
        }

        if (!wglDeleteContext(context->hGLRC)) {
            DWORD error = GetLastError();
            Error("Failed to delete context error code (%d).", error);
        }
        context->hGLRC = NULL;
    }
    context->hDC = NULL;
#elif defined(OS_LINUX_XLIB) || defined(OS_LINUX_XCB_GLX)
    glXDestroyContext(context->xDisplay, context->glxContext);
    context->xDisplay = NULL;
    context->visualid = 0;
    context->glxFBConfig = NULL;
    context->glxDrawable = 0;
    context->glxContext = NULL;
#elif defined(OS_LINUX_XCB)
    xcb_glx_destroy_context(context->connection, context->glxContext);
    context->connection = NULL;
    context->screen_number = 0;
    context->fbconfigid = 0;
    context->visualid = 0;
    context->glxDrawable = 0;
    context->glxContext = 0;
    context->glxContextTag = 0;
#elif defined(OS_APPLE_MACOS)
    CGLSetCurrentContext(NULL);
    if (context->nsContext != NULL) {
        [context->nsContext clearDrawable];
        [context->nsContext release];
        context->nsContext = nil;
    } else {
        CGLDestroyContext(context->cglContext);
    }
    context->cglContext = nil;
#elif defined(OS_ANDROID) || defined(OS_LINUX_WAYLAND)
    if (context->display != 0) {
        EGL(eglMakeCurrent(context->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    }
    if (context->context != EGL_NO_CONTEXT) {
        EGL(eglDestroyContext(context->display, context->context));
    }

#if defined(OS_ANDROID)
    if (context->mainSurface != context->tinySurface) {
        EGL(eglDestroySurface(context->display, context->mainSurface));
    }
    if (context->tinySurface != EGL_NO_SURFACE) {
        EGL(eglDestroySurface(context->display, context->tinySurface));
    }
    context->tinySurface = EGL_NO_SURFACE;
#elif defined(OS_LINUX_WAYLAND)
    if (context->mainSurface != EGL_NO_SURFACE) {
        EGL(eglDestroySurface(context->display, context->mainSurface));
    }
#endif
    context->display = 0;
    context->config = 0;
    context->mainSurface = EGL_NO_SURFACE;
    context->context = EGL_NO_CONTEXT;
#endif
}

void ksGpuContext_SetCurrent(ksGpuContext *context) {
#if defined(OS_WINDOWS)
    wglMakeCurrent(context->hDC, context->hGLRC);
#elif defined(OS_LINUX_XLIB) || defined(OS_LINUX_XCB_GLX)
    glXMakeCurrent(context->xDisplay, context->glxDrawable, context->glxContext);
    static int firstTime = 1;
    if (firstTime) {
        GlInitExtensions();
        firstTime = 0;
    }

#elif defined(OS_LINUX_XCB)
    xcb_glx_make_current_cookie_t glx_make_current_cookie =
        xcb_glx_make_current(context->connection, context->glxDrawable, context->glxContext, 0);
    xcb_glx_make_current_reply_t *glx_make_current_reply =
        xcb_glx_make_current_reply(context->connection, glx_make_current_cookie, NULL);
    context->glxContextTag = glx_make_current_reply->context_tag;
    free(glx_make_current_reply);
#elif defined(OS_APPLE_MACOS)
    CGLSetCurrentContext(context->cglContext);
#elif defined(OS_ANDROID) || defined(OS_LINUX_WAYLAND)
    EGL(eglMakeCurrent(context->display, context->mainSurface, context->mainSurface, context->context));
#endif
}

void ksGpuContext_UnsetCurrent(ksGpuContext *context) {
#if defined(OS_WINDOWS)
    wglMakeCurrent(context->hDC, NULL);
#elif defined(OS_LINUX_XLIB) || defined(OS_LINUX_XCB_GLX)
    glXMakeCurrent(context->xDisplay, /* None */ 0L, NULL);
#elif defined(OS_LINUX_XCB)
    xcb_glx_make_current(context->connection, 0, 0, 0);
#elif defined(OS_APPLE_MACOS)
    (void)context;
    CGLSetCurrentContext(NULL);
#elif defined(OS_ANDROID) || defined(OS_LINUX_WAYLAND)
    EGL(eglMakeCurrent(context->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
#endif
}

bool ksGpuContext_CheckCurrent(ksGpuContext *context) {
#if defined(OS_WINDOWS)
    return (wglGetCurrentContext() == context->hGLRC);
#elif defined(OS_LINUX_XLIB) || defined(OS_LINUX_XCB_GLX)
    return (glXGetCurrentContext() == context->glxContext);
#elif defined(OS_LINUX_XCB)
    return true;
#elif defined(OS_APPLE_MACOS)
    return (CGLGetCurrentContext() == context->cglContext);
#elif defined(OS_APPLE_IOS)
    return (false);  // TODO: pick current context off the UIView
#elif defined(OS_ANDROID) || defined(OS_LINUX_WAYLAND)
    return (eglGetCurrentContext() == context->context);
#endif
}

/*
================================================================================================================================

GPU Window.

================================================================================================================================
*/

#if defined(OS_WINDOWS)

typedef enum {
    KEY_A = 0x41,
    KEY_B = 0x42,
    KEY_C = 0x43,
    KEY_D = 0x44,
    KEY_E = 0x45,
    KEY_F = 0x46,
    KEY_G = 0x47,
    KEY_H = 0x48,
    KEY_I = 0x49,
    KEY_J = 0x4A,
    KEY_K = 0x4B,
    KEY_L = 0x4C,
    KEY_M = 0x4D,
    KEY_N = 0x4E,
    KEY_O = 0x4F,
    KEY_P = 0x50,
    KEY_Q = 0x51,
    KEY_R = 0x52,
    KEY_S = 0x53,
    KEY_T = 0x54,
    KEY_U = 0x55,
    KEY_V = 0x56,
    KEY_W = 0x57,
    KEY_X = 0x58,
    KEY_Y = 0x59,
    KEY_Z = 0x5A,
    KEY_RETURN = VK_RETURN,
    KEY_TAB = VK_TAB,
    KEY_ESCAPE = VK_ESCAPE,
    KEY_SHIFT_LEFT = VK_LSHIFT,
    KEY_CTRL_LEFT = VK_LCONTROL,
    KEY_ALT_LEFT = VK_LMENU,
    KEY_CURSOR_UP = VK_UP,
    KEY_CURSOR_DOWN = VK_DOWN,
    KEY_CURSOR_LEFT = VK_LEFT,
    KEY_CURSOR_RIGHT = VK_RIGHT
} ksKeyboardKey;

typedef enum { MOUSE_LEFT = 0, MOUSE_RIGHT = 1 } ksMouseButton;

LRESULT APIENTRY WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    ksGpuWindow *window = (ksGpuWindow *)GetWindowLongPtrA(hWnd, GWLP_USERDATA);

    switch (message) {
        case WM_SIZE: {
            if (window != NULL) {
                window->windowWidth = (int)LOWORD(lParam);
                window->windowHeight = (int)HIWORD(lParam);
            }
            return 0;
        }
        case WM_ACTIVATE: {
            if (window != NULL) {
                window->windowActiveState = !HIWORD(wParam);
            }
            return 0;
        }
        case WM_ERASEBKGND: {
            return 0;
        }
        case WM_CLOSE: {
            PostQuitMessage(0);
            return 0;
        }
        case WM_KEYDOWN: {
            if (window != NULL) {
                if ((int)wParam >= 0 && (int)wParam < 256) {
                    if ((int)wParam != KEY_SHIFT_LEFT && (int)wParam != KEY_CTRL_LEFT && (int)wParam != KEY_ALT_LEFT &&
                        (int)wParam != KEY_CURSOR_UP && (int)wParam != KEY_CURSOR_DOWN && (int)wParam != KEY_CURSOR_LEFT &&
                        (int)wParam != KEY_CURSOR_RIGHT) {
                        window->input.keyInput[(int)wParam] = true;
                    }
                }
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            window->input.mouseInput[MOUSE_LEFT] = true;
            window->input.mouseInputX[MOUSE_LEFT] = LOWORD(lParam);
            window->input.mouseInputY[MOUSE_LEFT] = window->windowHeight - HIWORD(lParam);
            break;
        }
        case WM_RBUTTONDOWN: {
            window->input.mouseInput[MOUSE_RIGHT] = true;
            window->input.mouseInputX[MOUSE_RIGHT] = LOWORD(lParam);
            window->input.mouseInputY[MOUSE_RIGHT] = window->windowHeight - HIWORD(lParam);
            break;
        }
    }
    return DefWindowProcA(hWnd, message, wParam, lParam);
}

void ksGpuWindow_Destroy(ksGpuWindow *window) {
    ksGpuContext_Destroy(&window->context);
    ksGpuDevice_Destroy(&window->device);

    if (window->windowFullscreen) {
        ChangeDisplaySettingsA(NULL, 0);
        ShowCursor(TRUE);
    }

    if (window->hDC) {
        if (!ReleaseDC(window->hWnd, window->hDC)) {
            Error("Failed to release device context.");
        }
        window->hDC = NULL;
    }

    if (window->hWnd) {
        if (!DestroyWindow(window->hWnd)) {
            Error("Failed to destroy the window.");
        }
        window->hWnd = NULL;
    }

    if (window->hInstance) {
        if (!UnregisterClassA(APPLICATION_NAME, window->hInstance)) {
            Error("Failed to unregister window class.");
        }
        window->hInstance = NULL;
    }
}

bool ksGpuWindow_Create(ksGpuWindow *window, ksDriverInstance *instance, const ksGpuQueueInfo *queueInfo, int queueIndex,
                        ksGpuSurfaceColorFormat colorFormat, ksGpuSurfaceDepthFormat depthFormat, ksGpuSampleCount sampleCount,
                        int width, int height, bool fullscreen) {
    memset(window, 0, sizeof(ksGpuWindow));

    window->colorFormat = colorFormat;
    window->depthFormat = depthFormat;
    window->sampleCount = sampleCount;
    window->windowWidth = width;
    window->windowHeight = height;
    window->windowSwapInterval = 1;
    window->windowRefreshRate = 60.0f;
    window->windowFullscreen = fullscreen;
    window->windowActive = false;
    window->windowExit = false;
    window->windowActiveState = false;

    const LPCSTR displayDevice = NULL;

    if (window->windowFullscreen) {
        DEVMODEA dmScreenSettings;
        memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
        dmScreenSettings.dmSize = sizeof(dmScreenSettings);
        dmScreenSettings.dmPelsWidth = width;
        dmScreenSettings.dmPelsHeight = height;
        dmScreenSettings.dmBitsPerPel = 32;
        dmScreenSettings.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;

        if (ChangeDisplaySettingsExA(displayDevice, &dmScreenSettings, NULL, CDS_FULLSCREEN, NULL) != DISP_CHANGE_SUCCESSFUL) {
            Error("The requested fullscreen mode is not supported.");
            return false;
        }
    }

    DEVMODEA lpDevMode;
    memset(&lpDevMode, 0, sizeof(DEVMODEA));
    lpDevMode.dmSize = sizeof(DEVMODEA);
    lpDevMode.dmDriverExtra = 0;

    if (EnumDisplaySettingsA(displayDevice, ENUM_CURRENT_SETTINGS, &lpDevMode) != FALSE) {
        window->windowRefreshRate = (float)lpDevMode.dmDisplayFrequency;
    }

    window->hInstance = GetModuleHandleA(NULL);

    WNDCLASSA wc;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = (WNDPROC)WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = window->hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = APPLICATION_NAME;

    if (!RegisterClassA(&wc)) {
        Error("Failed to register window class.");
        return false;
    }

    DWORD dwExStyle = 0;
    DWORD dwStyle = 0;
    if (window->windowFullscreen) {
        dwExStyle = WS_EX_APPWINDOW;
        dwStyle = WS_POPUP;
        ShowCursor(FALSE);
    } else {
        // Fixed size window.
        dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
        dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    }

    RECT windowRect;
    windowRect.left = (long)0;
    windowRect.right = (long)width;
    windowRect.top = (long)0;
    windowRect.bottom = (long)height;

    AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);

    if (!window->windowFullscreen) {
        RECT desktopRect;
        GetWindowRect(GetDesktopWindow(), &desktopRect);

        const int offsetX = (desktopRect.right - (windowRect.right - windowRect.left)) / 2;
        const int offsetY = (desktopRect.bottom - (windowRect.bottom - windowRect.top)) / 2;

        windowRect.left += offsetX;
        windowRect.right += offsetX;
        windowRect.top += offsetY;
        windowRect.bottom += offsetY;
    }

    window->hWnd = CreateWindowExA(dwExStyle,                           // Extended style for the window
                                   APPLICATION_NAME,                    // Class name
                                   WINDOW_TITLE,                        // Window title
                                   dwStyle |                            // Defined window style
                                       WS_CLIPSIBLINGS |                // Required window style
                                       WS_CLIPCHILDREN,                 // Required window style
                                   windowRect.left,                     // Window X position
                                   windowRect.top,                      // Window Y position
                                   windowRect.right - windowRect.left,  // Window width
                                   windowRect.bottom - windowRect.top,  // Window height
                                   NULL,                                // No parent window
                                   NULL,                                // No menu
                                   window->hInstance,                   // Instance
                                   NULL);                               // No WM_CREATE parameter
    if (!window->hWnd) {
        ksGpuWindow_Destroy(window);
        Error("Failed to create window.");
        return false;
    }

    SetWindowLongPtrA(window->hWnd, GWLP_USERDATA, (LONG_PTR)window);

    window->hDC = GetDC(window->hWnd);
    if (!window->hDC) {
        ksGpuWindow_Destroy(window);
        Error("Failed to acquire device context.");
        return false;
    }

    ksGpuDevice_Create(&window->device, instance, queueInfo);
    ksGpuContext_CreateForSurface(&window->context, &window->device, queueIndex, colorFormat, depthFormat, sampleCount,
                                  window->hInstance, window->hDC);
    ksGpuContext_SetCurrent(&window->context);

    ShowWindow(window->hWnd, SW_SHOW);
    SetForegroundWindow(window->hWnd);
    SetFocus(window->hWnd);

    return true;
}

#elif defined(OS_LINUX_XLIB)

typedef enum  // keysym.h
{ KEY_A = XK_a,
  KEY_B = XK_b,
  KEY_C = XK_c,
  KEY_D = XK_d,
  KEY_E = XK_e,
  KEY_F = XK_f,
  KEY_G = XK_g,
  KEY_H = XK_h,
  KEY_I = XK_i,
  KEY_J = XK_j,
  KEY_K = XK_k,
  KEY_L = XK_l,
  KEY_M = XK_m,
  KEY_N = XK_n,
  KEY_O = XK_o,
  KEY_P = XK_p,
  KEY_Q = XK_q,
  KEY_R = XK_r,
  KEY_S = XK_s,
  KEY_T = XK_t,
  KEY_U = XK_u,
  KEY_V = XK_v,
  KEY_W = XK_w,
  KEY_X = XK_x,
  KEY_Y = XK_y,
  KEY_Z = XK_z,
  KEY_RETURN = (XK_Return & 0xFF),
  KEY_TAB = (XK_Tab & 0xFF),
  KEY_ESCAPE = (XK_Escape & 0xFF),
  KEY_SHIFT_LEFT = (XK_Shift_L & 0xFF),
  KEY_CTRL_LEFT = (XK_Control_L & 0xFF),
  KEY_ALT_LEFT = (XK_Alt_L & 0xFF),
  KEY_CURSOR_UP = (XK_Up & 0xFF),
  KEY_CURSOR_DOWN = (XK_Down & 0xFF),
  KEY_CURSOR_LEFT = (XK_Left & 0xFF),
  KEY_CURSOR_RIGHT = (XK_Right & 0xFF) } ksKeyboardKey;

typedef enum { MOUSE_LEFT = Button1, MOUSE_RIGHT = Button2 } ksMouseButton;

/*
        Change video mode using the XFree86-VidMode X extension.

        While the XFree86-VidMode X extension should be superseded by the XRandR X extension,
        this still appears to be the most reliable way to change video modes for a single
        monitor configuration.
*/
static bool ChangeVideoMode_XF86VidMode(Display *xDisplay, int xScreen, int *currentWidth, int *currentHeight,
                                        float *currentRefreshRate, int *desiredWidth, int *desiredHeight,
                                        float *desiredRefreshRate) {
    int videoModeCount;
    XF86VidModeModeInfo **videoModeInfos;

    XF86VidModeGetAllModeLines(xDisplay, xScreen, &videoModeCount, &videoModeInfos);

    if (currentWidth != NULL && currentHeight != NULL && currentRefreshRate != NULL) {
        XF86VidModeModeInfo *mode = videoModeInfos[0];
        *currentWidth = mode->hdisplay;
        *currentHeight = mode->vdisplay;
        *currentRefreshRate = (mode->dotclock * 1000.0f) / (mode->htotal * mode->vtotal);
    }

    if (desiredWidth != NULL && desiredHeight != NULL && desiredRefreshRate != NULL) {
        XF86VidModeModeInfo *bestMode = NULL;
        int bestModeWidth = 0;
        int bestModeHeight = 0;
        float bestModeRefreshRate = 0.0f;
        int bestSizeError = 0x7FFFFFFF;
        float bestRefreshRateError = 1e6f;
        for (int j = 0; j < videoModeCount; j++) {
            XF86VidModeModeInfo *mode = videoModeInfos[j];
            const int modeWidth = mode->hdisplay;
            const int modeHeight = mode->vdisplay;
            const float modeRefreshRate = (mode->dotclock * 1000.0f) / (mode->htotal * mode->vtotal);

            const int dw = modeWidth - *desiredWidth;
            const int dh = modeHeight - *desiredHeight;
            const int sizeError = dw * dw + dh * dh;
            const float refreshRateError = fabsf(modeRefreshRate - *desiredRefreshRate);
            if (sizeError < bestSizeError || (sizeError == bestSizeError && refreshRateError < bestRefreshRateError)) {
                bestSizeError = sizeError;
                bestRefreshRateError = refreshRateError;
                bestMode = mode;
                bestModeWidth = modeWidth;
                bestModeHeight = modeHeight;
                bestModeRefreshRate = modeRefreshRate;
            }
        }

        XF86VidModeSwitchToMode(xDisplay, xScreen, bestMode);
        XF86VidModeSetViewPort(xDisplay, xScreen, 0, 0);

        *desiredWidth = bestModeWidth;
        *desiredHeight = bestModeHeight;
        *desiredRefreshRate = bestModeRefreshRate;
    }

    for (int i = 0; i < videoModeCount; i++) {
        if (videoModeInfos[i]->privsize > 0) {
            XFree(videoModeInfos[i]->private);
        }
    }
    XFree(videoModeInfos);

    return true;
}

void ksGpuWindow_Destroy(ksGpuWindow *window) {
    ksGpuContext_Destroy(&window->context);
    ksGpuDevice_Destroy(&window->device);

    if (window->windowFullscreen) {
        ChangeVideoMode_XF86VidMode(window->xDisplay, window->xScreen, NULL, NULL, NULL, &window->desktopWidth,
                                    &window->desktopHeight, &window->desktopRefreshRate);

        XUngrabPointer(window->xDisplay, CurrentTime);
        XUngrabKeyboard(window->xDisplay, CurrentTime);
    }

    if (window->xWindow) {
        XUnmapWindow(window->xDisplay, window->xWindow);
        XDestroyWindow(window->xDisplay, window->xWindow);
        window->xWindow = 0;
    }

    if (window->xColormap) {
        XFreeColormap(window->xDisplay, window->xColormap);
        window->xColormap = 0;
    }

    if (window->xVisual) {
        XFree(window->xVisual);
        window->xVisual = NULL;
    }

    XFlush(window->xDisplay);
    XCloseDisplay(window->xDisplay);
    window->xDisplay = NULL;
}

bool ksGpuWindow_Create(ksGpuWindow *window, ksDriverInstance *instance, const ksGpuQueueInfo *queueInfo, const int queueIndex,
                        const ksGpuSurfaceColorFormat colorFormat, const ksGpuSurfaceDepthFormat depthFormat,
                        const ksGpuSampleCount sampleCount, const int width, const int height, const bool fullscreen) {
    memset(window, 0, sizeof(ksGpuWindow));

    window->colorFormat = colorFormat;
    window->depthFormat = depthFormat;
    window->sampleCount = sampleCount;
    window->windowWidth = width;
    window->windowHeight = height;
    window->windowSwapInterval = 1;
    window->windowRefreshRate = 60.0f;
    window->windowFullscreen = fullscreen;
    window->windowActive = false;
    window->windowExit = false;

    const char *displayName = NULL;
    window->xDisplay = XOpenDisplay(displayName);
    if (!window->xDisplay) {
        Error("Unable to open X Display.");
        return false;
    }

    window->xScreen = XDefaultScreen(window->xDisplay);
    window->xRoot = XRootWindow(window->xDisplay, window->xScreen);

    if (window->windowFullscreen) {
        ChangeVideoMode_XF86VidMode(window->xDisplay, window->xScreen, &window->desktopWidth, &window->desktopHeight,
                                    &window->desktopRefreshRate, &window->windowWidth, &window->windowHeight,
                                    &window->windowRefreshRate);
    } else {
        ChangeVideoMode_XF86VidMode(window->xDisplay, window->xScreen, &window->desktopWidth, &window->desktopHeight,
                                    &window->desktopRefreshRate, NULL, NULL, NULL);
        window->windowRefreshRate = window->desktopRefreshRate;
    }

    ksGpuDevice_Create(&window->device, instance, queueInfo);
    ksGpuContext_CreateForSurface(&window->context, &window->device, queueIndex, colorFormat, depthFormat, sampleCount,
                                  window->xDisplay, window->xScreen);

    window->xVisual = glXGetVisualFromFBConfig(window->xDisplay, window->context.glxFBConfig);
    if (window->xVisual == NULL) {
        Error("Failed to retrieve visual for framebuffer config.");
        ksGpuWindow_Destroy(window);
        return false;
    }

    window->xColormap = XCreateColormap(window->xDisplay, window->xRoot, window->xVisual->visual, AllocNone);

    const unsigned long wamask = CWColormap | CWEventMask | (window->windowFullscreen ? 0 : CWBorderPixel);

    XSetWindowAttributes wa;
    memset(&wa, 0, sizeof(wa));
    wa.colormap = window->xColormap;
    wa.border_pixel = 0;
    wa.event_mask = StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask | KeyPressMask | KeyReleaseMask |
                    ButtonPressMask | ButtonReleaseMask | FocusChangeMask | ExposureMask | VisibilityChangeMask | EnterWindowMask |
                    LeaveWindowMask;

    window->xWindow = XCreateWindow(window->xDisplay,         // Display * display
                                    window->xRoot,            // Window parent
                                    0,                        // int x
                                    0,                        // int y
                                    window->windowWidth,      // unsigned int width
                                    window->windowHeight,     // unsigned int height
                                    0,                        // unsigned int border_width
                                    window->xVisual->depth,   // int depth
                                    InputOutput,              // unsigned int class
                                    window->xVisual->visual,  // Visual * visual
                                    wamask,                   // unsigned long valuemask
                                    &wa);                     // XSetWindowAttributes * attributes

    if (!window->xWindow) {
        Error("Failed to create window.");
        ksGpuWindow_Destroy(window);
        return false;
    }

    // Change the window title.
    Atom _NET_WM_NAME = XInternAtom(window->xDisplay, "_NET_WM_NAME", False);
    XChangeProperty(window->xDisplay, window->xWindow, _NET_WM_NAME, XA_STRING, 8, PropModeReplace,
                    (const unsigned char *)WINDOW_TITLE, strlen(WINDOW_TITLE));

    if (window->windowFullscreen) {
        // Bypass the compositor in fullscreen mode.
        const unsigned long bypass = 1;
        Atom _NET_WM_BYPASS_COMPOSITOR = XInternAtom(window->xDisplay, "_NET_WM_BYPASS_COMPOSITOR", False);
        XChangeProperty(window->xDisplay, window->xWindow, _NET_WM_BYPASS_COMPOSITOR, XA_CARDINAL, 32, PropModeReplace,
                        (const unsigned char *)&bypass, 1);

        // Completely disassociate window from window manager.
        XSetWindowAttributes attributes;
        attributes.override_redirect = True;
        XChangeWindowAttributes(window->xDisplay, window->xWindow, CWOverrideRedirect, &attributes);

        // Make the window visible.
        XMapRaised(window->xDisplay, window->xWindow);
        XMoveResizeWindow(window->xDisplay, window->xWindow, 0, 0, window->windowWidth, window->windowHeight);
        XFlush(window->xDisplay);

        // Grab mouse and keyboard input now that the window is disassociated from the window manager.
        XGrabPointer(window->xDisplay, window->xWindow, True, 0, GrabModeAsync, GrabModeAsync, window->xWindow, 0L, CurrentTime);
        XGrabKeyboard(window->xDisplay, window->xWindow, True, GrabModeAsync, GrabModeAsync, CurrentTime);
    } else {
        // Make the window fixed size.
        XSizeHints *hints = XAllocSizeHints();
        hints->flags = (PMinSize | PMaxSize);
        hints->min_width = window->windowWidth;
        hints->max_width = window->windowWidth;
        hints->min_height = window->windowHeight;
        hints->max_height = window->windowHeight;
        XSetWMNormalHints(window->xDisplay, window->xWindow, hints);
        XFree(hints);

        // First map the window and then center the window on the screen.
        XMapRaised(window->xDisplay, window->xWindow);
        const int x = (window->desktopWidth - window->windowWidth) / 2;
        const int y = (window->desktopHeight - window->windowHeight) / 2;
        XMoveResizeWindow(window->xDisplay, window->xWindow, x, y, window->windowWidth, window->windowHeight);
        XFlush(window->xDisplay);
    }

    window->context.glxDrawable = window->xWindow;

    ksGpuContext_SetCurrent(&window->context);

    return true;
}

#elif defined(OS_LINUX_XCB) || defined(OS_LINUX_XCB_GLX)

typedef enum  // keysym.h
{ KEY_A = XK_a,
  KEY_B = XK_b,
  KEY_C = XK_c,
  KEY_D = XK_d,
  KEY_E = XK_e,
  KEY_F = XK_f,
  KEY_G = XK_g,
  KEY_H = XK_h,
  KEY_I = XK_i,
  KEY_J = XK_j,
  KEY_K = XK_k,
  KEY_L = XK_l,
  KEY_M = XK_m,
  KEY_N = XK_n,
  KEY_O = XK_o,
  KEY_P = XK_p,
  KEY_Q = XK_q,
  KEY_R = XK_r,
  KEY_S = XK_s,
  KEY_T = XK_t,
  KEY_U = XK_u,
  KEY_V = XK_v,
  KEY_W = XK_w,
  KEY_X = XK_x,
  KEY_Y = XK_y,
  KEY_Z = XK_z,
  KEY_RETURN = (XK_Return & 0xFF),
  KEY_TAB = (XK_Tab & 0xFF),
  KEY_ESCAPE = (XK_Escape & 0xFF),
  KEY_SHIFT_LEFT = (XK_Shift_L & 0xFF),
  KEY_CTRL_LEFT = (XK_Control_L & 0xFF),
  KEY_ALT_LEFT = (XK_Alt_L & 0xFF),
  KEY_CURSOR_UP = (XK_Up & 0xFF),
  KEY_CURSOR_DOWN = (XK_Down & 0xFF),
  KEY_CURSOR_LEFT = (XK_Left & 0xFF),
  KEY_CURSOR_RIGHT = (XK_Right & 0xFF) } ksKeyboardKey;

typedef enum { MOUSE_LEFT = 0, MOUSE_RIGHT = 1 } ksMouseButton;

typedef enum {
    XCB_SIZE_HINT_US_POSITION = 1 << 0,
    XCB_SIZE_HINT_US_SIZE = 1 << 1,
    XCB_SIZE_HINT_P_POSITION = 1 << 2,
    XCB_SIZE_HINT_P_SIZE = 1 << 3,
    XCB_SIZE_HINT_P_MIN_SIZE = 1 << 4,
    XCB_SIZE_HINT_P_MAX_SIZE = 1 << 5,
    XCB_SIZE_HINT_P_RESIZE_INC = 1 << 6,
    XCB_SIZE_HINT_P_ASPECT = 1 << 7,
    XCB_SIZE_HINT_BASE_SIZE = 1 << 8,
    XCB_SIZE_HINT_P_WIN_GRAVITY = 1 << 9
} xcb_size_hints_flags_t;

static const int _NET_WM_STATE_ADD = 1;  // add/set property

/*
        Change video mode using the RandR X extension version 1.4

        The following code does not necessarily work out of the box, because on
        some configurations the modes list returned by XRRGetScreenResources()
        is populated with nothing other than the maximum display resolution,
        even though XF86VidModeGetAllModeLines() and XRRConfigSizes() *will*
        list all resolutions for the same display.

        The user can manually add new modes from the command-line using the
        xrandr utility:

        xrandr --newmode <modeline>

        Where <modeline> is generated with a utility that implements either
        the General Timing Formula (GTF) or the Coordinated Video Timing (CVT)
        standard put forth by the Video Electronics Standards Association (VESA):

        gft <width> <height> <Hz>	// http://gtf.sourceforge.net/
        cvt <width> <height> <Hz>	// http://www.uruk.org/~erich/projects/cvt/

        Alternatively, new modes can be added in code using XRRCreateMode().
        However, this requires calculating all the timing information in code
        because there is no standard library that implements the GTF or CVT.
*/
static bool ChangeVideoMode_XcbRandR_1_4(xcb_connection_t *connection, xcb_screen_t *screen, int *currentWidth, int *currentHeight,
                                         float *currentRefreshRate, int *desiredWidth, int *desiredHeight,
                                         float *desiredRefreshRate) {
    /*
            Screen	- virtual screenspace which may be covered by multiple CRTCs
            CRTC	- display controller
            Output	- display/monitor connected to a CRTC
            Clones	- outputs that are simultaneously connected to the same CRTC
    */

    xcb_randr_get_screen_resources_cookie_t screen_resources_cookie = xcb_randr_get_screen_resources(connection, screen->root);
    xcb_randr_get_screen_resources_reply_t *screen_resources_reply =
        xcb_randr_get_screen_resources_reply(connection, screen_resources_cookie, 0);
    if (screen_resources_reply == NULL) {
        return false;
    }

    xcb_randr_mode_info_t *mode_info = xcb_randr_get_screen_resources_modes(screen_resources_reply);
    const int modes_length = xcb_randr_get_screen_resources_modes_length(screen_resources_reply);
    assert(modes_length > 0);

    xcb_randr_crtc_t *crtcs = xcb_randr_get_screen_resources_crtcs(screen_resources_reply);
    const int crtcs_length = xcb_randr_get_screen_resources_crtcs_length(screen_resources_reply);
    assert(crtcs_length > 0);
    UNUSED_PARM(crtcs_length);

    const int PRIMARY_CRTC_INDEX = 0;
    const int PRIMARY_OUTPUT_INDEX = 0;

    xcb_randr_get_crtc_info_cookie_t primary_crtc_info_cookie = xcb_randr_get_crtc_info(connection, crtcs[PRIMARY_CRTC_INDEX], 0);
    xcb_randr_get_crtc_info_reply_t *primary_crtc_info_reply =
        xcb_randr_get_crtc_info_reply(connection, primary_crtc_info_cookie, NULL);

    xcb_randr_output_t *crtc_outputs = xcb_randr_get_crtc_info_outputs(primary_crtc_info_reply);

    xcb_randr_get_output_info_cookie_t primary_output_info_cookie =
        xcb_randr_get_output_info(connection, crtc_outputs[PRIMARY_OUTPUT_INDEX], 0);
    xcb_randr_get_output_info_reply_t *primary_output_info_reply =
        xcb_randr_get_output_info_reply(connection, primary_output_info_cookie, NULL);

    if (currentWidth != NULL && currentHeight != NULL && currentRefreshRate != NULL) {
        for (int i = 0; i < modes_length; i++) {
            if (mode_info[i].id == primary_crtc_info_reply->mode) {
                *currentWidth = mode_info[i].width;
                *currentHeight = mode_info[i].height;
                *currentRefreshRate = mode_info[i].dot_clock / ((float)mode_info[i].htotal * (float)mode_info[i].vtotal);
                break;
            }
        }
    }

    if (desiredWidth != NULL && desiredHeight != NULL && desiredRefreshRate != NULL) {
        xcb_randr_mode_t bestMode = 0;
        int bestModeWidth = 0;
        int bestModeHeight = 0;
        float bestModeRefreshRate = 0.0f;
        int bestSizeError = 0x7FFFFFFF;
        float bestRefreshRateError = 1e6f;
        for (int i = 0; i < modes_length; i++) {
            if (mode_info[i].mode_flags & XCB_RANDR_MODE_FLAG_INTERLACE) {
                continue;
            }

            xcb_randr_mode_t *primary_output_info_modes = xcb_randr_get_output_info_modes(primary_output_info_reply);
            int primary_output_info_modes_length = xcb_randr_get_output_info_modes_length(primary_output_info_reply);

            bool validOutputMode = false;
            for (int j = 0; j < primary_output_info_modes_length; j++) {
                if (mode_info[i].id == primary_output_info_modes[j]) {
                    validOutputMode = true;
                    break;
                }
            }
            if (!validOutputMode) {
                continue;
            }

            const int modeWidth = mode_info[i].width;
            const int modeHeight = mode_info[i].height;
            const float modeRefreshRate = mode_info[i].dot_clock / ((float)mode_info[i].htotal * (float)mode_info[i].vtotal);

            const int dw = modeWidth - *desiredWidth;
            const int dh = modeHeight - *desiredHeight;
            const int sizeError = dw * dw + dh * dh;
            const float refreshRateError = fabs(modeRefreshRate - *desiredRefreshRate);
            if (sizeError < bestSizeError || (sizeError == bestSizeError && refreshRateError < bestRefreshRateError)) {
                bestSizeError = sizeError;
                bestRefreshRateError = refreshRateError;
                bestMode = mode_info[i].id;
                bestModeWidth = modeWidth;
                bestModeHeight = modeHeight;
                bestModeRefreshRate = modeRefreshRate;
            }
        }

        xcb_randr_output_t *primary_crtc_info_outputs = xcb_randr_get_crtc_info_outputs(primary_crtc_info_reply);
        int primary_crtc_info_outputs_length = xcb_randr_get_crtc_info_outputs_length(primary_crtc_info_reply);

        xcb_randr_set_crtc_config(connection, primary_output_info_reply->crtc, XCB_TIME_CURRENT_TIME, XCB_TIME_CURRENT_TIME,
                                  primary_crtc_info_reply->x, primary_crtc_info_reply->y, bestMode,
                                  primary_crtc_info_reply->rotation, primary_crtc_info_outputs_length, primary_crtc_info_outputs);

        *desiredWidth = bestModeWidth;
        *desiredHeight = bestModeHeight;
        *desiredRefreshRate = bestModeRefreshRate;
    }

    free(primary_output_info_reply);
    free(primary_crtc_info_reply);
    free(screen_resources_reply);

    return true;
}

void ksGpuWindow_Destroy(ksGpuWindow *window) {
    ksGpuContext_Destroy(&window->context);
    ksGpuDevice_Destroy(&window->device);

#if defined(OS_LINUX_XCB_GLX)
    glXDestroyWindow(window->xDisplay, window->glxWindow);
    XFlush(window->xDisplay);
    XCloseDisplay(window->xDisplay);
    window->xDisplay = NULL;
#else
    xcb_glx_delete_window(window->connection, window->glxWindow);
#endif

    if (window->windowFullscreen) {
        ChangeVideoMode_XcbRandR_1_4(window->connection, window->screen, NULL, NULL, NULL, &window->desktopWidth,
                                     &window->desktopHeight, &window->desktopRefreshRate);
    }

    xcb_destroy_window(window->connection, window->window);
    xcb_free_colormap(window->connection, window->colormap);
    xcb_flush(window->connection);
    xcb_disconnect(window->connection);
    xcb_key_symbols_free(window->key_symbols);
}

bool ksGpuWindow_Create(ksGpuWindow *window, ksDriverInstance *instance, const ksGpuQueueInfo *queueInfo, const int queueIndex,
                        const ksGpuSurfaceColorFormat colorFormat, const ksGpuSurfaceDepthFormat depthFormat,
                        const ksGpuSampleCount sampleCount, const int width, const int height, const bool fullscreen) {
    memset(window, 0, sizeof(ksGpuWindow));

    window->colorFormat = colorFormat;
    window->depthFormat = depthFormat;
    window->sampleCount = sampleCount;
    window->windowWidth = width;
    window->windowHeight = height;
    window->windowSwapInterval = 1;
    window->windowRefreshRate = 60.0f;
    window->windowFullscreen = fullscreen;
    window->windowActive = false;
    window->windowExit = false;

    const char *displayName = NULL;
    int screen_number = 0;
    window->connection = xcb_connect(displayName, &screen_number);
    if (xcb_connection_has_error(window->connection)) {
        ksGpuWindow_Destroy(window);
        Error("Failed to open XCB connection.");
        return false;
    }

    const xcb_setup_t *setup = xcb_get_setup(window->connection);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screen_number; i++) {
        xcb_screen_next(&iter);
    }
    window->screen = iter.data;

    if (window->windowFullscreen) {
        ChangeVideoMode_XcbRandR_1_4(window->connection, window->screen, &window->desktopWidth, &window->desktopHeight,
                                     &window->desktopRefreshRate, &window->windowWidth, &window->windowHeight,
                                     &window->windowRefreshRate);
    } else {
        ChangeVideoMode_XcbRandR_1_4(window->connection, window->screen, &window->desktopWidth, &window->desktopHeight,
                                     &window->desktopRefreshRate, NULL, NULL, NULL);
        window->windowRefreshRate = window->desktopRefreshRate;
    }

    ksGpuDevice_Create(&window->device, instance, queueInfo);
#if defined(OS_LINUX_XCB_GLX)
    window->xDisplay = XOpenDisplay(displayName);
    ksGpuContext_CreateForSurface(&window->context, &window->device, queueIndex, colorFormat, depthFormat, sampleCount,
                                  window->xDisplay, screen_number);
#else
    ksGpuContext_CreateForSurface(&window->context, &window->device, queueIndex, colorFormat, depthFormat, sampleCount,
                                  window->connection, screen_number);
#endif

    // Create the color map.
    window->colormap = xcb_generate_id(window->connection);
    xcb_create_colormap(window->connection, XCB_COLORMAP_ALLOC_NONE, window->colormap, window->screen->root,
                        window->context.visualid);

    // Create the window.
    uint32_t value_mask = XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;
    uint32_t value_list[5];
    value_list[0] = window->screen->black_pixel;
    value_list[1] = 0;
    value_list[2] = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_BUTTON_PRESS;
    value_list[3] = window->colormap;
    value_list[4] = 0;

    window->window = xcb_generate_id(window->connection);
    xcb_create_window(window->connection,             // xcb_connection_t *	connection
                      XCB_COPY_FROM_PARENT,           // uint8_t				depth
                      window->window,                 // xcb_window_t			wid
                      window->screen->root,           // xcb_window_t			parent
                      0,                              // int16_t				x
                      0,                              // int16_t				y
                      window->windowWidth,            // uint16_t				width
                      window->windowHeight,           // uint16_t				height
                      0,                              // uint16_t				border_width
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,  // uint16_t				_class
                      window->context.visualid,       // xcb_visualid_t		visual
                      value_mask,                     // uint32_t				value_mask
                      value_list);                    // const uint32_t *		value_list

    // Change the window title.
    xcb_change_property(window->connection, XCB_PROP_MODE_REPLACE, window->window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                        strlen(WINDOW_TITLE), WINDOW_TITLE);

    // Setup code that will send a notification when the window is destroyed.
    xcb_intern_atom_cookie_t wm_protocols_cookie = xcb_intern_atom(window->connection, 1, 12, "WM_PROTOCOLS");
    xcb_intern_atom_cookie_t wm_delete_window_cookie = xcb_intern_atom(window->connection, 0, 16, "WM_DELETE_WINDOW");
    xcb_intern_atom_reply_t *wm_protocols_reply = xcb_intern_atom_reply(window->connection, wm_protocols_cookie, 0);
    xcb_intern_atom_reply_t *wm_delete_window_reply = xcb_intern_atom_reply(window->connection, wm_delete_window_cookie, 0);

    window->wm_delete_window_atom = wm_delete_window_reply->atom;
    xcb_change_property(window->connection, XCB_PROP_MODE_REPLACE, window->window, wm_protocols_reply->atom, XCB_ATOM_ATOM, 32, 1,
                        &wm_delete_window_reply->atom);

    free(wm_protocols_reply);
    free(wm_delete_window_reply);

    if (window->windowFullscreen) {
        // Change the window to fullscreen
        xcb_intern_atom_cookie_t wm_state_cookie = xcb_intern_atom(window->connection, 0, 13, "_NET_WM_STATE");
        xcb_intern_atom_cookie_t wm_state_fullscreen_cookie =
            xcb_intern_atom(window->connection, 0, 24, "_NET_WM_STATE_FULLSCREEN");
        xcb_intern_atom_reply_t *wm_state_reply = xcb_intern_atom_reply(window->connection, wm_state_cookie, 0);
        xcb_intern_atom_reply_t *wm_state_fullscreen_reply =
            xcb_intern_atom_reply(window->connection, wm_state_fullscreen_cookie, 0);

        xcb_client_message_event_t ev;
        ev.response_type = XCB_CLIENT_MESSAGE;
        ev.format = 32;
        ev.sequence = 0;
        ev.window = window->window;
        ev.type = wm_state_reply->atom;
        ev.data.data32[0] = _NET_WM_STATE_ADD;
        ev.data.data32[1] = wm_state_fullscreen_reply->atom;
        ev.data.data32[2] = XCB_ATOM_NONE;
        ev.data.data32[3] = 0;
        ev.data.data32[4] = 0;

        xcb_send_event(window->connection, 1, window->window,
                       XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY, (const char *)(&ev));

        free(wm_state_reply);
        free(wm_state_fullscreen_reply);

        xcb_map_window(window->connection, window->window);
        xcb_flush(window->connection);
    } else {
        // Make the window fixed size.
        xcb_size_hints_t hints;
        memset(&hints, 0, sizeof(hints));
        hints.flags = XCB_SIZE_HINT_US_SIZE | XCB_SIZE_HINT_P_SIZE | XCB_SIZE_HINT_P_MIN_SIZE | XCB_SIZE_HINT_P_MAX_SIZE;
        hints.min_width = window->windowWidth;
        hints.max_width = window->windowWidth;
        hints.min_height = window->windowHeight;
        hints.max_height = window->windowHeight;

        xcb_change_property(window->connection, XCB_PROP_MODE_REPLACE, window->window, XCB_ATOM_WM_NORMAL_HINTS,
                            XCB_ATOM_WM_SIZE_HINTS, 32, sizeof(hints) / 4, &hints);

        // First map the window and then center the window on the screen.
        xcb_map_window(window->connection, window->window);
        const uint32_t coords[] = {(window->desktopWidth - window->windowWidth) / 2,
                                   (window->desktopHeight - window->windowHeight) / 2};
        xcb_configure_window(window->connection, window->window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, coords);
        xcb_flush(window->connection);
    }

    window->key_symbols = xcb_key_symbols_alloc(window->connection);

#if defined(OS_LINUX_XCB_GLX)
    window->glxWindow = glXCreateWindow(window->xDisplay, window->context.glxFBConfig, window->window, NULL);
#else
    window->glxWindow = xcb_generate_id(window->connection);
    xcb_glx_create_window(window->connection, screen_number, window->context.fbconfigid, window->window, window->glxWindow, 0,
                          NULL);
#endif

    window->context.glxDrawable = window->glxWindow;

    ksGpuContext_SetCurrent(&window->context);

    return true;
}

#elif defined(OS_LINUX_WAYLAND)

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
static void _keyboard_keymap_cb(void *data, struct wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size) { close(fd); }
static void _keyboard_modifiers_cb(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed,
                                   uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {}

static void _keyboard_enter_cb(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface,
                               struct wl_array *keys) {}

static void _keyboard_leave_cb(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface) {}

static void _pointer_leave_cb(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface) {}

static void _pointer_enter_cb(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t sx,
                              wl_fixed_t sy) {
    wl_pointer_set_cursor(pointer, serial, NULL, 0, 0);
}

static void _pointer_motion_cb(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    ksGpuWindow *window = (ksGpuWindow *)data;
    window->input.mouseInputX[0] = wl_fixed_to_int(x);
    window->input.mouseInputY[0] = wl_fixed_to_int(y);
}

static void _pointer_button_cb(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button,
                               uint32_t state) {
    ksGpuWindow *window = (ksGpuWindow *)data;

    uint32_t button_id = 0;
    switch (button) {
        case BTN_LEFT:
            button_id = 0;
            break;
        case BTN_MIDDLE:
            button_id = 1;
            break;
        case BTN_RIGHT:
            button_id = 2;
            break;
    }

    window->input.mouseInput[button_id] = state;
}

static void _pointer_axis_cb(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {}

static void _keyboard_key_cb(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key,
                             uint32_t state) {
    ksGpuWindow *window = (ksGpuWindow *)data;
    if (key == KEY_ESC) window->windowExit = true;

    if (state) window->input.keyInput[key] = state;
}

const struct wl_pointer_listener pointer_listener = {
    _pointer_enter_cb, _pointer_leave_cb, _pointer_motion_cb, _pointer_button_cb, _pointer_axis_cb,
};

const struct wl_keyboard_listener keyboard_listener = {
    _keyboard_keymap_cb, _keyboard_enter_cb, _keyboard_leave_cb, _keyboard_key_cb, _keyboard_modifiers_cb,
};

static void _seat_capabilities_cb(void *data, struct wl_seat *seat, uint32_t caps) {
    ksGpuWindow *window = (ksGpuWindow *)data;
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !window->pointer) {
        window->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(window->pointer, &pointer_listener, window);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && window->pointer) {
        wl_pointer_destroy(window->pointer);
        window->pointer = NULL;
    }

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !window->keyboard) {
        window->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(window->keyboard, &keyboard_listener, window);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && window->keyboard) {
        wl_keyboard_destroy(window->keyboard);
        window->keyboard = NULL;
    }
}

const struct wl_seat_listener seat_listener = {
    _seat_capabilities_cb,
};

static void _xdg_surface_configure_cb(void *data, struct zxdg_surface_v6 *surface, uint32_t serial) {
    zxdg_surface_v6_ack_configure(surface, serial);
}

const struct zxdg_surface_v6_listener xdg_surface_listener = {
    _xdg_surface_configure_cb,
};

static void _xdg_shell_ping_cb(void *data, struct zxdg_shell_v6 *shell, uint32_t serial) { zxdg_shell_v6_pong(shell, serial); }

const struct zxdg_shell_v6_listener xdg_shell_listener = {
    _xdg_shell_ping_cb,
};

static void _xdg_toplevel_configure_cb(void *data, struct zxdg_toplevel_v6 *toplevel, int32_t width, int32_t height,
                                       struct wl_array *states) {
    ksGpuWindow *window = (ksGpuWindow *)data;

    window->windowActive = false;

    enum zxdg_toplevel_v6_state *state;
    wl_array_for_each(state, states) {
        switch (*state) {
            case ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN:
                break;
            case ZXDG_TOPLEVEL_V6_STATE_RESIZING:
                window->windowWidth = width;
                window->windowWidth = height;
                break;
            case ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED:
                break;
            case ZXDG_TOPLEVEL_V6_STATE_ACTIVATED:
                window->windowActive = true;
                break;
        }
    }
}

static void _xdg_toplevel_close_cb(void *data, struct zxdg_toplevel_v6 *toplevel) {
    ksGpuWindow *window = (ksGpuWindow *)data;
    window->windowExit = true;
}

const struct zxdg_toplevel_v6_listener xdg_toplevel_listener = {
    _xdg_toplevel_configure_cb,
    _xdg_toplevel_close_cb,
};

static void _registry_cb(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version) {
    ksGpuWindow *window = (ksGpuWindow *)data;

    if (strcmp(interface, "wl_compositor") == 0) {
        window->compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    } else if (strcmp(interface, "zxdg_shell_v6") == 0) {
        window->shell = wl_registry_bind(registry, id, &zxdg_shell_v6_interface, 1);
        zxdg_shell_v6_add_listener(window->shell, &xdg_shell_listener, NULL);
    } else if (strcmp(interface, "wl_seat") == 0) {
        window->seat = wl_registry_bind(registry, id, &wl_seat_interface, 1);
        wl_seat_add_listener(window->seat, &seat_listener, window);
    }
}

static void _registry_remove_cb(void *data, struct wl_registry *registry, uint32_t id) {}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

const struct wl_registry_listener registry_listener = {_registry_cb, _registry_remove_cb};

bool ksGpuWindow_Create(ksGpuWindow *window, ksDriverInstance *instance, const ksGpuQueueInfo *queueInfo, const int queueIndex,
                        const ksGpuSurfaceColorFormat colorFormat, const ksGpuSurfaceDepthFormat depthFormat,
                        const ksGpuSampleCount sampleCount, const int width, const int height, const bool fullscreen) {
    (void)queueIndex;
    memset(window, 0, sizeof(ksGpuWindow));

    window->display = NULL;
    window->surface = NULL;
    window->registry = NULL;
    window->compositor = NULL;
    window->shell = NULL;
    window->shell_surface = NULL;

    window->keyboard = NULL;
    window->pointer = NULL;
    window->seat = NULL;

    window->colorFormat = colorFormat;
    window->depthFormat = depthFormat;
    window->sampleCount = sampleCount;
    window->windowWidth = width;
    window->windowHeight = height;
    window->windowSwapInterval = 1;
    window->windowRefreshRate = 60.0f;
    window->windowFullscreen = fullscreen;
    window->windowActive = false;
    window->windowExit = false;

    window->display = wl_display_connect(NULL);
    if (window->display == NULL) {
        Error("Can't connect to wayland display.");
        return false;
    }

    window->registry = wl_display_get_registry(window->display);
    wl_registry_add_listener(window->registry, &registry_listener, window);

    wl_display_roundtrip(window->display);

    if (window->compositor == NULL) {
        Error("Compositor protocol failed to bind");
        return false;
    }

    if (window->shell == NULL) {
        Error("Compositor is missing support for zxdg_shell_v6.");
        return false;
    }

    window->surface = wl_compositor_create_surface(window->compositor);
    if (window->surface == NULL) {
        Error("Could not create compositor surface.");
        return false;
    }

    window->shell_surface = zxdg_shell_v6_get_xdg_surface(window->shell, window->surface);
    if (window->shell_surface == NULL) {
        Error("Could not get shell surface.");
        return false;
    }

    zxdg_surface_v6_add_listener(window->shell_surface, &xdg_surface_listener, window);

    struct zxdg_toplevel_v6 *toplevel = zxdg_surface_v6_get_toplevel(window->shell_surface);
    if (toplevel == NULL) {
        Error("Could not get surface toplevel.");
        return false;
    }

    zxdg_toplevel_v6_add_listener(toplevel, &xdg_toplevel_listener, window);

    zxdg_toplevel_v6_set_title(toplevel, WINDOW_TITLE);
    zxdg_toplevel_v6_set_app_id(toplevel, APPLICATION_NAME);
    zxdg_toplevel_v6_set_min_size(toplevel, width, height);
    zxdg_toplevel_v6_set_max_size(toplevel, width, height);

    wl_surface_commit(window->surface);

    window->context.native_window = wl_egl_window_create(window->surface, width, height);

    if (window->context.native_window == EGL_NO_SURFACE) {
        ksGpuWindow_Destroy(window);
        Error("Could not create wayland egl window.");
        return false;
    }

    ksGpuDevice_Create(&window->device, instance, queueInfo);

    ksGpuContext_CreateForSurface(&window->context, &window->device, window->display);

    ksGpuContext_SetCurrent(&window->context);

    return true;
}

void ksGpuWindow_Destroy(ksGpuWindow *window) {
    if (window->pointer != NULL) wl_pointer_destroy(window->pointer);
    if (window->keyboard != NULL) wl_keyboard_destroy(window->keyboard);
    if (window->seat != NULL) wl_seat_destroy(window->seat);

    wl_egl_window_destroy(window->context.native_window);

    if (window->compositor != NULL) wl_compositor_destroy(window->compositor);
    if (window->registry != NULL) wl_registry_destroy(window->registry);
    if (window->shell_surface != NULL) zxdg_surface_v6_destroy(window->shell_surface);
    if (window->shell != NULL) zxdg_shell_v6_destroy(window->shell);
    if (window->surface != NULL) wl_surface_destroy(window->surface);
    if (window->display != NULL) wl_display_disconnect(window->display);

    ksGpuContext_Destroy(&window->context);
    ksGpuDevice_Destroy(&window->device);
}

/*
 * TODO:
 * This is a work around for ksKeyboardKey naming collision
 * with the definitions from <linux/input.h>.
 * The proper fix for this is to rename the key enums.
 */

#undef KEY_A
#undef KEY_B
#undef KEY_C
#undef KEY_D
#undef KEY_E
#undef KEY_F
#undef KEY_G
#undef KEY_H
#undef KEY_I
#undef KEY_J
#undef KEY_K
#undef KEY_L
#undef KEY_M
#undef KEY_N
#undef KEY_O
#undef KEY_P
#undef KEY_Q
#undef KEY_R
#undef KEY_S
#undef KEY_T
#undef KEY_U
#undef KEY_V
#undef KEY_W
#undef KEY_X
#undef KEY_Y
#undef KEY_Z
#undef KEY_TAB

typedef enum  // from <linux/input.h>
{ KEY_A = 30,
  KEY_B = 48,
  KEY_C = 46,
  KEY_D = 32,
  KEY_E = 18,
  KEY_F = 33,
  KEY_G = 34,
  KEY_H = 35,
  KEY_I = 23,
  KEY_J = 36,
  KEY_K = 37,
  KEY_L = 38,
  KEY_M = 50,
  KEY_N = 49,
  KEY_O = 24,
  KEY_P = 25,
  KEY_Q = 16,
  KEY_R = 19,
  KEY_S = 31,
  KEY_T = 20,
  KEY_U = 22,
  KEY_V = 47,
  KEY_W = 17,
  KEY_X = 45,
  KEY_Y = 21,
  KEY_Z = 44,
  KEY_TAB = 15,
  KEY_RETURN = KEY_ENTER,
  KEY_ESCAPE = KEY_ESC,
  KEY_SHIFT_LEFT = KEY_LEFTSHIFT,
  KEY_CTRL_LEFT = KEY_LEFTCTRL,
  KEY_ALT_LEFT = KEY_LEFTALT,
  KEY_CURSOR_UP = KEY_UP,
  KEY_CURSOR_DOWN = KEY_DOWN,
  KEY_CURSOR_LEFT = KEY_LEFT,
  KEY_CURSOR_RIGHT = KEY_RIGHT } ksKeyboardKey;

typedef enum { MOUSE_LEFT = BTN_LEFT, MOUSE_MIDDLE = BTN_MIDDLE, MOUSE_RIGHT = BTN_RIGHT } ksMouseButton;

#elif defined(OS_APPLE_MACOS)

typedef enum {
    KEY_A = 0x00,
    KEY_B = 0x0B,
    KEY_C = 0x08,
    KEY_D = 0x02,
    KEY_E = 0x0E,
    KEY_F = 0x03,
    KEY_G = 0x05,
    KEY_H = 0x04,
    KEY_I = 0x22,
    KEY_J = 0x26,
    KEY_K = 0x28,
    KEY_L = 0x25,
    KEY_M = 0x2E,
    KEY_N = 0x2D,
    KEY_O = 0x1F,
    KEY_P = 0x23,
    KEY_Q = 0x0C,
    KEY_R = 0x0F,
    KEY_S = 0x01,
    KEY_T = 0x11,
    KEY_U = 0x20,
    KEY_V = 0x09,
    KEY_W = 0x0D,
    KEY_X = 0x07,
    KEY_Y = 0x10,
    KEY_Z = 0x06,
    KEY_RETURN = 0x24,
    KEY_TAB = 0x30,
    KEY_ESCAPE = 0x35,
    KEY_SHIFT_LEFT = 0x38,
    KEY_CTRL_LEFT = 0x3B,
    KEY_ALT_LEFT = 0x3A,
    KEY_CURSOR_UP = 0x7E,
    KEY_CURSOR_DOWN = 0x7D,
    KEY_CURSOR_LEFT = 0x7B,
    KEY_CURSOR_RIGHT = 0x7C
} ksKeyboardKey;

typedef enum { MOUSE_LEFT = 0, MOUSE_RIGHT = 1 } ksMouseButton;

NSAutoreleasePool *autoReleasePool;

@interface MyNSWindow : NSWindow
- (BOOL)canBecomeMainWindow;
- (BOOL)canBecomeKeyWindow;
- (BOOL)acceptsFirstResponder;
- (void)keyDown:(NSEvent *)event;
@end

@implementation MyNSWindow
- (BOOL)canBecomeMainWindow {
    return YES;
}
- (BOOL)canBecomeKeyWindow {
    return YES;
}
- (BOOL)acceptsFirstResponder {
    return YES;
}
- (void)keyDown:(NSEvent *)event {
    (void)event;
}
@end

@interface MyNSView : NSView
- (BOOL)acceptsFirstResponder;
- (void)keyDown:(NSEvent *)event;
@end

@implementation MyNSView
- (BOOL)acceptsFirstResponder {
    return YES;
}
- (void)keyDown:(NSEvent *)event {
    (void)event;
}
@end

void ksGpuWindow_Destroy(ksGpuWindow *window) {
    ksGpuContext_Destroy(&window->context);
    ksGpuDevice_Destroy(&window->device);

    if (window->windowFullscreen) {
        CGDisplaySetDisplayMode(window->display, window->desktopDisplayMode, NULL);
        CGDisplayModeRelease(window->desktopDisplayMode);
        window->desktopDisplayMode = NULL;
    }
    if (window->nsWindow) {
        [window->nsWindow release];
        window->nsWindow = nil;
    }
    if (window->nsView) {
        [window->nsView release];
        window->nsView = nil;
    }
}

bool ksGpuWindow_Create(ksGpuWindow *window, ksDriverInstance *instance, const ksGpuQueueInfo *queueInfo, const int queueIndex,
                        const ksGpuSurfaceColorFormat colorFormat, const ksGpuSurfaceDepthFormat depthFormat,
                        const ksGpuSampleCount sampleCount, const int width, const int height, const bool fullscreen) {
    memset(window, 0, sizeof(ksGpuWindow));

    window->colorFormat = colorFormat;
    window->depthFormat = depthFormat;
    window->sampleCount = sampleCount;
    window->windowWidth = width;
    window->windowHeight = height;
    window->windowSwapInterval = 1;
    window->windowRefreshRate = 60.0f;
    window->windowFullscreen = fullscreen;
    window->windowActive = false;
    window->windowExit = false;

    // Get a list of all available displays.
    CGDirectDisplayID displays[32];
    CGDisplayCount displayCount = 0;
    CGDisplayErr err = CGGetActiveDisplayList(32, displays, &displayCount);
    if (err != CGDisplayNoErr) {
        return false;
    }
    // Use the main display.
    window->display = displays[0];
    window->desktopDisplayMode = CGDisplayCopyDisplayMode(window->display);

    // If fullscreen then switch to the best matching display mode.
    if (window->windowFullscreen) {
        CFArrayRef displayModes = CGDisplayCopyAllDisplayModes(window->display, NULL);
        CFIndex displayModeCount = CFArrayGetCount(displayModes);
        CGDisplayModeRef bestDisplayMode = nil;
        size_t bestDisplayWidth = 0;
        size_t bestDisplayHeight = 0;
        float bestDisplayRefreshRate = 0;
        size_t bestError = 0x7FFFFFFF;
        for (CFIndex i = 0; i < displayModeCount; i++) {
            CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(displayModes, i);

            const size_t modeWidth = CGDisplayModeGetWidth(mode);
            const size_t modeHeight = CGDisplayModeGetHeight(mode);
            const double modeRefreshRate = CGDisplayModeGetRefreshRate(mode);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            CFStringRef modePixelEncoding = CGDisplayModeCopyPixelEncoding(mode);
#pragma GCC diagnostic pop
            const bool modeBitsPerPixelIs32 =
                (CFStringCompare(modePixelEncoding, CFSTR(IO32BitDirectPixels), 0) == kCFCompareEqualTo);
            CFRelease(modePixelEncoding);

            if (modeBitsPerPixelIs32) {
                const size_t dw = modeWidth - width;
                const size_t dh = modeHeight - height;
                const size_t error = dw * dw + dh * dh;
                if (error < bestError) {
                    bestError = error;
                    bestDisplayMode = mode;
                    bestDisplayWidth = modeWidth;
                    bestDisplayHeight = modeHeight;
                    bestDisplayRefreshRate = (float)modeRefreshRate;
                }
            }
        }
        CGDisplayErr cgderr = CGDisplaySetDisplayMode(window->display, bestDisplayMode, NULL);
        if (cgderr != CGDisplayNoErr) {
            CFRelease(displayModes);
            return false;
        }
        CFRelease(displayModes);
        window->windowWidth = (int)bestDisplayWidth;
        window->windowHeight = (int)bestDisplayHeight;
        window->windowRefreshRate = (bestDisplayRefreshRate > 0.0f) ? bestDisplayRefreshRate : 60.0f;
    } else {
        const float desktopDisplayRefreshRate = (float)CGDisplayModeGetRefreshRate(window->desktopDisplayMode);
        window->windowRefreshRate = (desktopDisplayRefreshRate > 0.0f) ? desktopDisplayRefreshRate : 60.0f;
    }

    if (window->windowFullscreen) {
        NSScreen *screen = [NSScreen mainScreen];
        NSRect screenRect = [screen frame];

        window->nsView = [MyNSView alloc];
        [window->nsView initWithFrame:screenRect];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        const int style = NSBorderlessWindowMask;
#pragma GCC diagnostic pop

        window->nsWindow = [MyNSWindow alloc];
        [window->nsWindow initWithContentRect:screenRect styleMask:style backing:NSBackingStoreBuffered defer:NO screen:screen];
        [window->nsWindow setOpaque:YES];
        [window->nsWindow setLevel:NSMainMenuWindowLevel + 1];
        [window->nsWindow setContentView:window->nsView];
        [window->nsWindow makeMainWindow];
        [window->nsWindow makeKeyAndOrderFront:nil];
        [window->nsWindow makeFirstResponder:nil];
    } else {
        NSScreen *screen = [NSScreen mainScreen];
        NSRect screenRect = [screen frame];

        NSRect windowRect;
        windowRect.origin.x = (screenRect.size.width - width) / 2;
        windowRect.origin.y = (screenRect.size.height - height) / 2;
        windowRect.size.width = width;
        windowRect.size.height = height;

        window->nsView = [MyNSView alloc];
        [window->nsView initWithFrame:windowRect];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        // Fixed size window.
        const int style = NSTitledWindowMask;  // | NSClosableWindowMask | NSResizableWindowMask;
#pragma GCC diagnostic pop

        window->nsWindow = [MyNSWindow alloc];
        [window->nsWindow initWithContentRect:windowRect styleMask:style backing:NSBackingStoreBuffered defer:NO screen:screen];
        [window->nsWindow setTitle:@WINDOW_TITLE];
        [window->nsWindow setOpaque:YES];
        [window->nsWindow setContentView:window->nsView];
        [window->nsWindow makeMainWindow];
        [window->nsWindow makeKeyAndOrderFront:nil];
        [window->nsWindow makeFirstResponder:nil];
    }

    ksGpuDevice_Create(&window->device, instance, queueInfo);
    ksGpuContext_CreateForSurface(&window->context, &window->device, queueIndex, colorFormat, depthFormat, sampleCount,
                                  window->display);

    [window->context.nsContext setView:window->nsView];

    ksGpuContext_SetCurrent(&window->context);

    // The color buffers are not cleared by default.
    for (int i = 0; i < 2; i++) {
        GL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
        GL(glClear(GL_COLOR_BUFFER_BIT));
        CGLFlushDrawable(window->context.cglContext);
    }

    return true;
}

#elif defined(OS_APPLE_IOS)

typedef enum {
    KEY_A = 0x00,
    KEY_B = 0x0B,
    KEY_C = 0x08,
    KEY_D = 0x02,
    KEY_E = 0x0E,
    KEY_F = 0x03,
    KEY_G = 0x05,
    KEY_H = 0x04,
    KEY_I = 0x22,
    KEY_J = 0x26,
    KEY_K = 0x28,
    KEY_L = 0x25,
    KEY_M = 0x2E,
    KEY_N = 0x2D,
    KEY_O = 0x1F,
    KEY_P = 0x23,
    KEY_Q = 0x0C,
    KEY_R = 0x0F,
    KEY_S = 0x01,
    KEY_T = 0x11,
    KEY_U = 0x20,
    KEY_V = 0x09,
    KEY_W = 0x0D,
    KEY_X = 0x07,
    KEY_Y = 0x10,
    KEY_Z = 0x06,
    KEY_RETURN = 0x24,
    KEY_TAB = 0x30,
    KEY_ESCAPE = 0x35,
    KEY_SHIFT_LEFT = 0x38,
    KEY_CTRL_LEFT = 0x3B,
    KEY_ALT_LEFT = 0x3A,
    KEY_CURSOR_UP = 0x7E,
    KEY_CURSOR_DOWN = 0x7D,
    KEY_CURSOR_LEFT = 0x7B,
    KEY_CURSOR_RIGHT = 0x7C
} ksKeyboardKey;

typedef enum { MOUSE_LEFT = 0, MOUSE_RIGHT = 1 } ksMouseButton;

static UIView *myUIView;
static UIWindow *myUIWindow;

@interface MyUIView : UIView
@end

@implementation MyUIView

- (instancetype)initWithFrame:(CGRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        self.contentScaleFactor = UIScreen.mainScreen.nativeScale;
    }
    return self;
}

+ (Class)layerClass {
    return [CAEAGLLayer class];
}

@end

@interface MyUIViewController : UIViewController
@end

@implementation MyUIViewController

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskLandscape;
}

- (BOOL)shouldAutorotate {
    return TRUE;
}

@end

void ksGpuWindow_Destroy(ksGpuWindow *window) {
    ksGpuContext_Destroy(&window->context);
    ksGpuDevice_Destroy(&window->device);
    window->uiWindow = nil;
    window->uiView = nil;
}

bool ksGpuWindow_Create(ksGpuWindow *window, ksDriverInstance *instance, const ksGpuQueueInfo *queueInfo, const int queueIndex,
                        const ksGpuSurfaceColorFormat colorFormat, const ksGpuSurfaceDepthFormat depthFormat,
                        const ksGpuSampleCount sampleCount, const int width, const int height, const bool fullscreen) {
    memset(window, 0, sizeof(ksGpuWindow));

    window->colorFormat = colorFormat;
    window->depthFormat = depthFormat;
    window->sampleCount = sampleCount;
    window->windowWidth = width;
    window->windowHeight = height;
    window->windowSwapInterval = 1;
    window->windowRefreshRate = 60.0f;
    window->windowFullscreen = fullscreen;
    window->windowActive = false;
    window->windowExit = false;
    window->uiView = myUIView;
    window->uiWindow = myUIWindow;

    ksGpuDevice_Create(&window->device, instance, queueInfo);
    // ksGpuContext_CreateForSurface(&window->context, &window->device, queueIndex, colorFormat, depthFormat, sampleCount,
    //                              window->display);

    return true;
}

#elif defined(OS_ANDROID)

void ksGpuWindow_Destroy(ksGpuWindow *window) {
    ksGpuContext_Destroy(&window->context);
    ksGpuDevice_Destroy(&window->device);

    if (window->display != 0) {
        EGL(eglTerminate(window->display));
        window->display = 0;
    }
}

bool ksGpuWindow_Create(ksGpuWindow *window, ksDriverInstance *instance, const ksGpuQueueInfo *queueInfo, const int queueIndex,
                        const ksGpuSurfaceColorFormat colorFormat, const ksGpuSurfaceDepthFormat depthFormat,
                        const ksGpuSampleCount sampleCount, const int width, const int height, const bool fullscreen) {
    memset(window, 0, sizeof(ksGpuWindow));
    (void)fullscreen;

    window->colorFormat = colorFormat;
    window->depthFormat = depthFormat;
    window->sampleCount = sampleCount;
    window->windowWidth = width;
    window->windowHeight = height;
    window->windowSwapInterval = 1;
    window->windowRefreshRate = 60.0f;
    window->windowFullscreen = true;
    window->windowActive = false;
    window->windowExit = false;

    window->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGL(eglInitialize(window->display, &window->majorVersion, &window->minorVersion));

    ksGpuDevice_Create(&window->device, instance, queueInfo);
    ksGpuContext_CreateForSurface(&window->context, &window->device, queueIndex, colorFormat, depthFormat, sampleCount,
                                  window->display);
    ksGpuContext_SetCurrent(&window->context);

    GlInitExtensions();

    return true;
}

#endif
