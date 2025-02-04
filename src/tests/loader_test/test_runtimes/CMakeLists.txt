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

add_library(test_runtime MODULE runtime_test.cpp)
set_target_properties(test_runtime PROPERTIES FOLDER ${LOADER_TESTS_FOLDER})
target_link_libraries(test_runtime PRIVATE OpenXR::headers)

add_dependencies(test_runtime xr_global_generated_files)
target_include_directories(
    test_runtime PRIVATE ${PROJECT_SOURCE_DIR}/src
                         ${PROJECT_SOURCE_DIR}/src/common
)
if(XR_USE_GRAPHICS_API_VULKAN)
    target_include_directories(test_runtime PRIVATE ${Vulkan_INCLUDE_DIRS})
endif()

macro(gen_xr_runtime_json filename libfile)
    add_custom_command(
        OUTPUT ${filename}
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/src/scripts/generate_runtime_manifest.py" -f
            ${filename} -l ${libfile} ${ARGN}
        DEPENDS "${PROJECT_SOURCE_DIR}/src/scripts/generate_runtime_manifest.py"
        COMMENT
            "Generating Runtime JSON ${filename} using -f ${filename} -l ${libfile} ${ARGN}"
    )
endmacro()

if(WIN32)
    target_compile_definitions(test_runtime PRIVATE _CRT_SECURE_NO_WARNINGS)
    # Turn off transitional "changed behavior" warning message for Visual Studio versions prior to 2015.
    # The changed behavior is that constructor initializers are now fixed to clear the struct members.
    target_compile_options(
        test_runtime
        PRIVATE
            "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,19>>:/wd4351>"
    )
endif()

# Dynamic Library:
#  - Make build depend on the module definition/version script/export map
#  - Add the linker flag (except windows)
if(WIN32)
    target_sources(
        test_runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/test_runtime.def"
    )
elseif(APPLE)
    set_target_properties(
        test_runtime
        PROPERTIES
            LINK_FLAGS
            "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/test_runtime.expsym"
    )
    target_sources(
        test_runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/test_runtime.expsym"
    )
else()
    set_target_properties(
        test_runtime
        PROPERTIES
            LINK_FLAGS
            "-Wl,--version-script=\"${CMAKE_CURRENT_SOURCE_DIR}/test_runtime.map\""
    )
    target_sources(
        test_runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/test_runtime.map"
    )
endif()

gen_xr_runtime_json(
    "${PROJECT_BINARY_DIR}/src/tests/loader_test/resources/runtimes/test_runtime.json"
    "${CMAKE_CURRENT_BINARY_DIR}/$<TARGET_FILE_NAME:test_runtime>"
    -b
)

# Add generated file to our sources so we depend on it, and thus trigger generation.
target_sources(
    test_runtime
    PRIVATE
        "${PROJECT_BINARY_DIR}/src/tests/loader_test/resources/runtimes/test_runtime.json"
)
