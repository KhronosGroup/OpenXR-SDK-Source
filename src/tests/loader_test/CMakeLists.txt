# Copyright (c) 2017-2026 The Khronos Group Inc.
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
        loader_test MODULE loader_test_utils.cpp loader_test.cpp
                           $<TARGET_OBJECTS:android_native_app_glue>
    )
    target_link_libraries(
        loader_test PRIVATE ${ANDROID_LIBRARY} ${ANDROID_LOG_LIBRARY}
    )
    target_include_directories(loader_test PRIVATE ${ANDROID_NATIVE_APP_GLUE})
else()
    add_executable(loader_test loader_test_utils.cpp loader_test.cpp)
    add_sanitizers(loader_test)
endif()

openxr_add_filesystem_utils(loader_test)
set_target_properties(loader_test PROPERTIES FOLDER ${LOADER_TESTS_FOLDER})
target_link_libraries(
    loader_test PRIVATE OpenXR::openxr_loader Catch2::Catch2
                        Catch2::Catch2WithMain
)

add_dependencies(
    loader_test
    XrApiLayer_test
    XrApiLayer_api_dump
    XrApiLayer_core_validation
    test_runtime
)

target_include_directories(
    loader_test
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${PROJECT_BINARY_DIR}/src"
            "${PROJECT_SOURCE_DIR}/src/common"
)
if(XR_USE_GRAPHICS_API_VULKAN)
    target_include_directories(loader_test PRIVATE ${Vulkan_INCLUDE_DIRS})
endif()

if(WIN32)
    if(MSVC)
        target_compile_definitions(loader_test PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(loader_test PRIVATE /Zc:wchar_t /Zc:forScope /W4)
        if(NOT
           CMAKE_CXX_COMPILER_ID
           STREQUAL
           "Clang"
        )
            # If actually msvc and not clang-cl
            target_compile_options(loader_test PRIVATE /WX)
        endif()
        target_link_libraries(loader_test PRIVATE d3d11)
    endif()
endif()

add_test(
    NAME loader_test
    COMMAND loader_test
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:loader_test>"
)

file(
    MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/resources/layers"
    "${CMAKE_CURRENT_BINARY_DIR}/resources/runtimes"
)

gen_xr_layer_json(
    "${PROJECT_BINARY_DIR}/src/tests/loader_test/resources/layers/XrApiLayer_api_dump.json"
    LUNARG_api_dump
    "$<TARGET_FILE:XrApiLayer_api_dump>"
    1
    "API Layer to record api calls as they occur"
    ""
)

gen_xr_layer_json(
    "${PROJECT_BINARY_DIR}/src/tests/loader_test/resources/layers/XrApiLayer_core_validation.json"
    LUNARG_core_validation
    $<TARGET_FILE:XrApiLayer_core_validation>
    1
    "API Layer to perform validation of api calls and parameters as they occur"
    ""
)

# Add generated file to our sources so we depend on it, and thus trigger generation.
target_sources(
    loader_test
    PRIVATE
        "${PROJECT_BINARY_DIR}/src/tests/loader_test/resources/layers/XrApiLayer_core_validation.json"
        "${PROJECT_BINARY_DIR}/src/tests/loader_test/resources/layers/XrApiLayer_api_dump.json"

)

add_subdirectory(test_layers)
