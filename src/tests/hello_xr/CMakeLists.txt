# Copyright (c) 2017 The Khronos Group Inc.
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
# Author:
#

file(GLOB LOCAL_HEADERS "*.h")
file(GLOB LOCAL_SOURCE "*.cpp")
file(GLOB VULKAN_SHADERS "vulkan_shaders/*.glsl")

# For including compiled shaders
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(hello_xr
    ${LOCAL_SOURCE}
    ${LOCAL_HEADERS}
    ${VULKAN_SHADERS})
set_target_properties(hello_xr PROPERTIES FOLDER ${SAMPLES_FOLDER})

source_group("Headers" FILES ${LOCAL_HEADERS})
source_group("Shaders" FILES ${VULKAN_SHADERS})

compile_glsl(run_hello_xr_glsl_compiles ${VULKAN_SHADERS})

add_dependencies(hello_xr
    generate_openxr_header
    run_hello_xr_glsl_compiles
)

target_include_directories(hello_xr
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/common

    # for OpenXR headers
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include

    # for helper headers
    ${PROJECT_SOURCE_DIR}/external/include
)

if(VulkanHeaders_INCLUDE_DIRS)
    target_include_directories(hello_xr
        PRIVATE
        ${VulkanHeaders_INCLUDE_DIRS}
    )
endif()
if(OPENGL_FOUND)
    target_sources(hello_xr
        PRIVATE
        ${PROJECT_SOURCE_DIR}/src/common/gfxwrapper_opengl.c
    )
endif()

target_link_libraries(hello_xr openxr_loader)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(MSVC)
        target_compile_definitions(hello_xr PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(hello_xr PRIVATE /Zc:wchar_t /Zc:forScope /W4 /WX)
    endif()
    target_link_libraries(hello_xr d3d11 d3dcompiler dxgi ${OPENGL_gl_LIBRARY})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(hello_xr PRIVATE -Wall)
    target_link_libraries(hello_xr m pthread GL ${XLIB_LIBRARIES} ${XCB_LIBRARIES} ${WAYLAND_LIBRARIES})
    target_sources(hello_xr PRIVATE ${WAYLAND_PROTOCOL_SRC})
endif()

if(Vulkan_LIBRARY)
    target_link_libraries(hello_xr ${Vulkan_LIBRARY})
endif()

install(TARGETS hello_xr
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
