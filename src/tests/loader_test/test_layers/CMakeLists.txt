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

# Force all compilers to output to binary folder without additional output (like Windows adds "Debug" and "Release" folders)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG}
        ${CMAKE_CURRENT_BINARY_DIR}
    )
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG}
        ${CMAKE_CURRENT_BINARY_DIR}
    )
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG}
        ${CMAKE_CURRENT_BINARY_DIR}
    )
endforeach()

add_library(XrApiLayer_test MODULE layer_test.cpp)
set_target_properties(XrApiLayer_test PROPERTIES FOLDER ${LOADER_TESTS_FOLDER})

target_link_libraries(XrApiLayer_test PRIVATE OpenXR::headers)
add_dependencies(XrApiLayer_test xr_global_generated_files)
target_include_directories(
    XrApiLayer_test PRIVATE "${PROJECT_SOURCE_DIR}/src"
                            "${PROJECT_SOURCE_DIR}/src/common"
)
if(XR_USE_GRAPHICS_API_VULKAN)
    target_include_directories(XrApiLayer_test PRIVATE ${Vulkan_INCLUDE_DIRS})
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(XrApiLayer_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    # Turn off transitional "changed behavior" warning message for Visual Studio versions prior to 2015.
    # The changed behavior is that constructor initializers are now fixed to clear the struct members.
    target_compile_options(
        XrApiLayer_test
        PRIVATE
            "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,19>>:/wd4351>"
    )
endif()

# Dynamic Library:
#  - Make build depend on the module definition/version script/export map
#  - Add the linker flag (except windows)
if(WIN32)
    target_sources(
        XrApiLayer_test
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_test.def"
    )
elseif(APPLE)
    set_target_properties(
        XrApiLayer_test
        PROPERTIES
            LINK_FLAGS
            "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_test.expsym"
    )
    target_sources(
        XrApiLayer_test
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_test.expsym"
    )
else()
    set_target_properties(
        XrApiLayer_test
        PROPERTIES
            LINK_FLAGS
            "-Wl,--version-script=\"${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_test.map\""
    )
    target_sources(
        XrApiLayer_test
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_test.map"
    )
endif()

gen_xr_layer_json(
    "${PROJECT_BINARY_DIR}/src/tests/loader_test/resources/layers/XrApiLayer_test.json"
    test
    "${CMAKE_CURRENT_BINARY_DIR}/$<TARGET_FILE_NAME:XrApiLayer_test>"
    1
    Test_description
    -b
)

# Add generated file to our sources so we depend on it, and thus trigger generation.
target_sources(
    XrApiLayer_test
    PRIVATE
        "${PROJECT_BINARY_DIR}/src/tests/loader_test/resources/layers/XrApiLayer_test.json"
)
