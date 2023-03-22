# Copyright (c) 2017 The Khronos Group Inc.
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
# Author:
#

file(GLOB LOCAL_HEADERS "*.h")
file(GLOB LOCAL_SOURCE "*.cpp")

# For including compiled shaders
include_directories(${CMAKE_CURRENT_BINARY_DIR})

if(ANDROID)
    add_library(list_json MODULE
        ${LOCAL_SOURCE}
        ${LOCAL_HEADERS}
        $<TARGET_OBJECTS:android_native_app_glue>)
    target_link_libraries(list_json ${ANDROID_LIBRARY} ${ANDROID_LOG_LIBRARY})
    target_include_directories(list_json PRIVATE ${ANDROID_NATIVE_APP_GLUE})
else()
    add_executable(list_json
        ${LOCAL_SOURCE}
        ${LOCAL_HEADERS})
endif()
set_target_properties(list_json PROPERTIES FOLDER ${SAMPLES_FOLDER})

source_group("Headers" FILES ${LOCAL_HEADERS})
source_group("Shaders" FILES ${VULKAN_SHADERS})

target_include_directories(list_json
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/common

    # for OpenXR headers
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include
)

if(Vulkan_FOUND)
    target_include_directories(list_json
        PRIVATE
        ${Vulkan_INCLUDE_DIRS}
    )
endif()

target_link_libraries(list_json openxr_loader)
if(WIN32)
    if(MSVC)
        target_compile_definitions(list_json PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(list_json PRIVATE /Zc:wchar_t /Zc:forScope /W4 /WX)
    endif()
    target_link_libraries(list_json ole32)
    if(MSVC)
        # Right now can't build this on MinGW because of directxcolors, etc.
        target_link_libraries(list_json d3d11 d3d12 d3dcompiler dxgi)
    else()
        target_compile_definitions(list_json PRIVATE MISSING_DIRECTX_COLORS)
    endif()
endif()

if(Vulkan_LIBRARY)
    target_link_libraries(list_json ${Vulkan_LIBRARY})
endif()

if(NOT ANDROID)
    install(TARGETS list_json
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT list_json)
endif()

