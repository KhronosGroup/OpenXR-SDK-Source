# Copyright (c) 2017-2023, The Khronos Group Inc.
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
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_CURRENT_BINARY_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_CURRENT_BINARY_DIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_CURRENT_BINARY_DIR})
endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

add_library(test_runtime SHARED
    runtime_test.cpp
    test_runtime.def
)
set_target_properties(test_runtime PROPERTIES FOLDER ${LOADER_TESTS_FOLDER})

add_dependencies(test_runtime
    xr_global_generated_files
    generate_openxr_header
)
target_include_directories(test_runtime
    PRIVATE ${PROJECT_SOURCE_DIR}/src
    PRIVATE ${PROJECT_SOURCE_DIR}/src/common
    PRIVATE ${PROJECT_BINARY_DIR}/include
)
if(Vulkan_FOUND)
    target_include_directories(test_runtime
        PRIVATE ${Vulkan_INCLUDE_DIRS}
    )
endif()

macro(gen_xr_runtime_json filename libfile)
    add_custom_command(OUTPUT ${filename}
        COMMAND
            ${CMAKE_COMMAND} -E env
                ${PYTHON_EXECUTABLE}
                    ${PROJECT_SOURCE_DIR}/src/scripts/generate_runtime_manifest.py -f ${filename} -l ${libfile} ${ARGN}
    DEPENDS
        ${PROJECT_SOURCE_DIR}/src/scripts/generate_runtime_manifest.py
    COMMENT "Generating Runtime JSON ${filename} using -f ${filename} -l ${libfile} ${ARGN}"
    )
endmacro()

if(WIN32)
    target_compile_definitions(test_runtime PRIVATE _CRT_SECURE_NO_WARNINGS)
    # Turn off transitional "changed behavior" warning message for Visual Studio versions prior to 2015.
    # The changed behavior is that constructor initializers are now fixed to clear the struct members.
    target_compile_options(test_runtime PRIVATE "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,19>>:/wd4351>")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(test_runtime PROPERTIES LINK_FLAGS "-Wl,-Bsymbolic,--exclude-libs,ALL")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_target_properties(test_runtime PROPERTIES LINK_FLAGS "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/test_runtime.expsym")
endif()

gen_xr_runtime_json(
    ${PROJECT_BINARY_DIR}/src/tests/loader_test/resources/runtimes/test_runtime.json
    ${CMAKE_CURRENT_BINARY_DIR}/$<TARGET_FILE_NAME:test_runtime>
    -b
)

# Add generated file to our sources so we depend on it, and thus trigger generation.
target_sources(test_runtime PRIVATE ${PROJECT_BINARY_DIR}/src/tests/loader_test/resources/runtimes/test_runtime.json)
