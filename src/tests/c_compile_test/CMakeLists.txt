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

if(ANDROID)
    add_library(
        openxr_c_compile_test MODULE main.c
                                     $<TARGET_OBJECTS:android_native_app_glue>
    )
    target_link_libraries(
        openxr_c_compile_test ${ANDROID_LIBRARY} ${ANDROID_LOG_LIBRARY}
    )
    target_include_directories(
        openxr_c_compile_test PRIVATE ${ANDROID_NATIVE_APP_GLUE}
    )
else()
    add_executable(openxr_c_compile_test main.c)
endif()
set_target_properties(openxr_c_compile_test PROPERTIES FOLDER ${TESTS_FOLDER})

target_include_directories(
    openxr_c_compile_test PRIVATE "${PROJECT_SOURCE_DIR}/src"
                                  "${PROJECT_SOURCE_DIR}/src/common"
)
if(XR_USE_GRAPHICS_API_VULKAN)
    target_include_directories(
        openxr_c_compile_test PRIVATE ${Vulkan_INCLUDE_DIRS}
    )
endif()

target_link_libraries(openxr_c_compile_test OpenXR::openxr_loader)
if(MSVC)
    target_compile_options(
        openxr_c_compile_test PRIVATE /Zc:wchar_t /Zc:forScope /W4
    )
    if(NOT
       CMAKE_CXX_COMPILER_ID
       STREQUAL
       "Clang"
    )
        # If actually msvc and not clang-cl
        target_compile_options(openxr_c_compile_test PRIVATE /WX)
    endif()
endif()
