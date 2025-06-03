# Copyright (c) 2017-2025 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

set(LOCAL_HEADERS
    check.h
    common.h
    d3d_common.h
    geometry.h
    graphicsapi.h
    graphicsplugin.h
    logger.h
    openxr_program.h
    options.h
    pch.h
    platformdata.h
    platformplugin.h
)
set(LOCAL_SOURCE
    d3d_common.cpp
    graphicsplugin_d3d11.cpp
    graphicsplugin_d3d12.cpp
    graphicsplugin_factory.cpp
    graphicsplugin_opengl.cpp
    graphicsplugin_opengles.cpp
    graphicsplugin_vulkan.cpp
    graphicsplugin_metal.cpp
    logger.cpp
    main.cpp
    openxr_program.cpp
    pch.cpp
    platformplugin_android.cpp
    platformplugin_factory.cpp
    platformplugin_posix.cpp
    platformplugin_win32.cpp
)
set(VULKAN_SHADERS vulkan_shaders/frag.glsl vulkan_shaders/vert.glsl)

if(ANDROID)
    add_library(
        hello_xr MODULE
        ${LOCAL_SOURCE}
        ${LOCAL_HEADERS}
        ${VULKAN_SHADERS}
        $<TARGET_OBJECTS:android_native_app_glue>
    )
    target_link_libraries(
        hello_xr PRIVATE ${ANDROID_LIBRARY} ${ANDROID_LOG_LIBRARY}
    )

    # Only for Android because it lacks a command line
    set(HELLOXR_DEFAULT_GRAPHICS_PLUGIN
        "OpenGLES"
        CACHE
            STRING
            "Which graphics plugin should be used by default for Hello XR artifacts?"
    )
    # These are the recognized options: provide them as a helper in the CMake GUI
    set_property(
        CACHE HELLOXR_DEFAULT_GRAPHICS_PLUGIN PROPERTY STRINGS "OpenGLES"
                                                       "Vulkan"
    )

    # Now handle the value, normally provided by Gradle
    if(HELLOXR_DEFAULT_GRAPHICS_PLUGIN STREQUAL "OpenGLES")
        message(STATUS "hello_xr will default to OpenGL ES graphics plugin")
        target_compile_definitions(
            hello_xr PRIVATE DEFAULT_GRAPHICS_PLUGIN_OPENGLES
        )
    elseif(HELLOXR_DEFAULT_GRAPHICS_PLUGIN STREQUAL "Vulkan")
        message(STATUS "hello_xr will default to Vulkan graphics plugin")
        target_compile_definitions(
            hello_xr PRIVATE DEFAULT_GRAPHICS_PLUGIN_VULKAN
        )
    endif()

else()
    add_executable(hello_xr ${LOCAL_SOURCE} ${LOCAL_HEADERS} ${VULKAN_SHADERS})
endif()

if(APPLE)
    # use C++17 since there is a dependency on metal-cpp
    target_compile_features(hello_xr PUBLIC cxx_std_17)
endif()

set_target_properties(hello_xr PROPERTIES FOLDER ${SAMPLES_FOLDER})
source_group("Headers" FILES ${LOCAL_HEADERS})
source_group("Shaders" FILES ${VULKAN_SHADERS})

target_link_libraries(hello_xr PRIVATE OpenXR::openxr_loader)

compile_glsl(run_hello_xr_glsl_compiles ${VULKAN_SHADERS})

add_dependencies(hello_xr run_hello_xr_glsl_compiles)

target_include_directories(
    hello_xr
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
        "${PROJECT_SOURCE_DIR}/src/common"
        "${PROJECT_SOURCE_DIR}/src/external/metal-cpp"
        # For including compiled shaders
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(hello_xr PRIVATE OpenXR::openxr_loader)

if(GLSLANG_VALIDATOR AND NOT GLSLC_COMMAND)
    target_compile_definitions(hello_xr PRIVATE USE_GLSLANGVALIDATOR)
endif()

if(XR_USE_GRAPHICS_API_VULKAN)
    target_include_directories(hello_xr PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(hello_xr PRIVATE ${Vulkan_LIBRARY})
endif()

if(TARGET openxr-gfxwrapper)
    target_link_libraries(hello_xr PRIVATE openxr-gfxwrapper)
endif()
if(WIN32)
    target_link_libraries(hello_xr PRIVATE ole32)
    if(MSVC)
        target_compile_definitions(hello_xr PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(hello_xr PRIVATE /Zc:wchar_t /Zc:forScope /W4)
        if(NOT
           CMAKE_CXX_COMPILER_ID
           STREQUAL
           "Clang"
        )
            # If actually msvc and not clang-cl
            target_compile_options(openxr_c_compile_test PRIVATE /WX)
        endif()

        # Right now can't build d3d features on MinGW because of directxcolors, directxmath, etc.
        target_link_libraries(
            hello_xr
            PRIVATE
                d3d11
                d3d12
                d3dcompiler
                dxgi
        )
    endif()
endif()

if(APPLE)
    target_link_libraries(
        hello_xr PRIVATE "-framework Foundation" "-framework CoreGraphics"
                         "-framework Metal"
    )
endif()

if(NOT ANDROID)
    install(
        TARGETS hello_xr RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                                 COMPONENT hello_xr
    )
    if(NOT WIN32)
        install(
            FILES hello_xr.1
            DESTINATION ${CMAKE_INSTALL_MANDIR}/man1/
            COMPONENT ManPages
        )
    endif()
endif()
