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
endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

# Copy the api_layer_platform_defines.h file and place it in the binary (build) directory.
configure_file(
    api_layer_platform_defines.h ${CMAKE_CURRENT_BINARY_DIR} COPYONLY
)

if(WIN32)
    # Find the layer library adjacent to the manifest
    set(LAYER_MANIFEST_PREFIX "./")
else()
    set(LAYER_MANIFEST_PREFIX)
endif()

# Windows needs bigobj support since we have generated code.
if(MSVC)
    add_compile_options("/bigobj")
elseif(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "^Clang|GNU$")
    add_compile_options("-Wa,-mbig-obj")
endif()

# Basics for api_dump API Layer

gen_xr_layer_json(
    "${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_api_dump.json"
    LUNARG_api_dump
    "${LAYER_MANIFEST_PREFIX}$<TARGET_FILE_NAME:XrApiLayer_api_dump>"
    1
    "API Layer to record api calls as they occur"
    ""
)

# Flag generated files that aren't generated in this directory.
set_source_files_properties(
    ${COMMON_GENERATED_OUTPUT} PROPERTIES GENERATED TRUE
)

set(GENERATED_OUTPUT)
set(GENERATED_DEPENDS)
run_xr_xml_generate(
    api_dump_generator.py xr_generated_api_dump.hpp
    "${PROJECT_SOURCE_DIR}/src/scripts/automatic_source_generator.py"
)
run_xr_xml_generate(
    api_dump_generator.py xr_generated_api_dump.cpp
    "${PROJECT_SOURCE_DIR}/src/scripts/automatic_source_generator.py"
)
set(API_DUMP_GENERATED_OUTPUT ${GENERATED_OUTPUT})
set(API_DUMP_GENERATED_DEPENDS ${GENERATED_DEPENDS})
unset(GENERATED_OUTPUT)
unset(GENERATED_DEPENDS)

add_library(
    XrApiLayer_api_dump MODULE
    api_dump.cpp
    "${PROJECT_SOURCE_DIR}/src/common/hex_and_handles.h"
    # target-specific generated files
    ${API_DUMP_GENERATED_OUTPUT}
    # Dispatch table
    ${COMMON_GENERATED_OUTPUT}
    # Included in this list to force generation
    "${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_api_dump.json"
)
set_target_properties(
    XrApiLayer_api_dump PROPERTIES FOLDER ${API_LAYERS_FOLDER}
)

target_link_libraries(
    XrApiLayer_api_dump PRIVATE Threads::Threads OpenXR::headers
)
if(ANDROID)
    target_link_libraries(
        XrApiLayer_api_dump PRIVATE ${ANDROID_LOG_LIBRARY}
    )
endif()
target_compile_definitions(
    XrApiLayer_api_dump PRIVATE ${OPENXR_ALL_SUPPORTED_DEFINES}
)
add_dependencies(XrApiLayer_api_dump xr_common_generated_files)

target_include_directories(
    XrApiLayer_api_dump
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/common
        # for generated dispatch table
        ..
        ${CMAKE_CURRENT_BINARY_DIR}/..
        # for target-specific generated files
        .
        ${CMAKE_CURRENT_BINARY_DIR}
)
if(XR_USE_GRAPHICS_API_VULKAN)
    target_include_directories(
        XrApiLayer_api_dump PRIVATE ${Vulkan_INCLUDE_DIRS}
    )
endif()
if(BUILD_WITH_WAYLAND_HEADERS)
    target_include_directories(
        XrApiLayer_api_dump PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIRS}
    )
endif()

# Basics for core_validation API Layer

gen_xr_layer_json(
    ${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_core_validation.json
    LUNARG_core_validation
    ${LAYER_MANIFEST_PREFIX}$<TARGET_FILE_NAME:XrApiLayer_core_validation>
    1
    "API Layer to perform validation of api calls and parameters as they occur"
    ""
)

set(GENERATED_OUTPUT)
set(GENERATED_DEPENDS)
run_xr_xml_generate(
    validation_layer_generator.py xr_generated_core_validation.hpp
    "${PROJECT_SOURCE_DIR}/src/scripts/automatic_source_generator.py"
)
run_xr_xml_generate(
    validation_layer_generator.py xr_generated_core_validation.cpp
    "${PROJECT_SOURCE_DIR}/src/scripts/automatic_source_generator.py"
)
set(CORE_VALIDATION_GENERATED_OUTPUT ${GENERATED_OUTPUT})
set(CORE_VALIDATION_GENERATED_DEPENDS ${GENERATED_DEPENDS})
unset(GENERATED_OUTPUT)
unset(GENERATED_DEPENDS)

add_library(
    XrApiLayer_core_validation MODULE
    core_validation.cpp
    ${PROJECT_SOURCE_DIR}/src/common/hex_and_handles.h
    ${PROJECT_SOURCE_DIR}/src/common/object_info.cpp
    ${PROJECT_SOURCE_DIR}/src/common/object_info.h
    # target-specific generated files
    ${CORE_VALIDATION_GENERATED_OUTPUT}
    # Dispatch table
    ${COMMON_GENERATED_OUTPUT}
    # Included in this list to force generation
    ${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_core_validation.json
)
set_target_properties(
    XrApiLayer_core_validation PROPERTIES FOLDER ${API_LAYERS_FOLDER}
)

target_link_libraries(
    XrApiLayer_core_validation PRIVATE Threads::Threads OpenXR::headers
)
if(ANDROID)
    target_link_libraries(
        XrApiLayer_core_validation PRIVATE ${ANDROID_LOG_LIBRARY}
    )
endif()
target_compile_definitions(
    XrApiLayer_core_validation PRIVATE ${OPENXR_ALL_SUPPORTED_DEFINES}
)
add_dependencies(XrApiLayer_core_validation xr_common_generated_files)
target_include_directories(
    XrApiLayer_core_validation
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/common
        # for generated dispatch table
        ..
        ${CMAKE_CURRENT_BINARY_DIR}/..
        # for target-specific generated files
        .
        ${CMAKE_CURRENT_BINARY_DIR}
)
if(XR_USE_GRAPHICS_API_VULKAN)
    target_include_directories(
        XrApiLayer_core_validation PRIVATE ${Vulkan_INCLUDE_DIRS}
    )
endif()
if(BUILD_WITH_WAYLAND_HEADERS)
    target_include_directories(
        XrApiLayer_core_validation PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIRS}
    )
endif()

if(WIN32)
    # Windows api_dump-specific information
    target_compile_definitions(
        XrApiLayer_api_dump PRIVATE _CRT_SECURE_NO_WARNINGS
    )
    # Turn off transitional "changed behavior" warning message for Visual Studio versions prior to 2015.
    # The changed behavior is that constructor initializers are now fixed to clear the struct members.
    target_compile_options(
        XrApiLayer_api_dump
        PRIVATE
            "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,19>>:/wd4351>"
    )

    # Windows core_validation-specific information
    target_compile_definitions(
        XrApiLayer_core_validation PRIVATE _CRT_SECURE_NO_WARNINGS
    )
    # Turn off transitional "changed behavior" warning message for Visual Studio versions prior to 2015.
    # The changed behavior is that constructor initializers are now fixed to clear the struct members.
    target_compile_options(
        XrApiLayer_core_validation
        PRIVATE
            "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,19>>:/wd4351>"
    )
endif()

# Dynamic Library:
#  - Make build depend on the module definition/version script/export map
#  - Add the linker flag (except windows)
if(WIN32)
    # XrApiLayer_api_dump
    target_sources(
        XrApiLayer_api_dump
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_api_dump.def"
    )

    # XrApiLayer_core_validation
    target_sources(
        XrApiLayer_core_validation
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_core_validation.def"
    )
elseif(APPLE)
    # XrApiLayer_api_dump
    set_target_properties(
        XrApiLayer_api_dump
        PROPERTIES
            LINK_FLAGS
            "-Wl,-exported_symbols_list,\"${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_api_dump.expsym\""
    )
    target_sources(
        XrApiLayer_api_dump
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_api_dump.expsym"
    )

    # XrApiLayer_core_validation
    set_target_properties(
        XrApiLayer_core_validation
        PROPERTIES
            LINK_FLAGS
            "-Wl,-exported_symbols_list,\"${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_core_validation.expsym\""
    )
    target_sources(
        XrApiLayer_core_validation
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_core_validation.expsym"
    )
else()
    # XrApiLayer_api_dump
    set_target_properties(
        XrApiLayer_api_dump
        PROPERTIES
            LINK_FLAGS
            "-Wl,--version-script=\"${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_api_dump.map\""
    )
    target_sources(
        XrApiLayer_api_dump
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_api_dump.map"
    )

    # XrApiLayer_core_validation
    set_target_properties(
        XrApiLayer_core_validation
        PROPERTIES
            LINK_FLAGS
            "-Wl,--version-script=\"${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_core_validation.map\""
    )
    target_sources(
        XrApiLayer_core_validation
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_core_validation.map"
    )
endif()

# Install explicit layers
set(TARGET_NAMES XrApiLayer_api_dump XrApiLayer_core_validation)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    foreach(TARGET_NAME ${TARGET_NAMES})
        install(
            FILES "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.json"
            DESTINATION
                "${CMAKE_INSTALL_DATAROOTDIR}/openxr/${MAJOR}/api_layers/explicit.d"
            COMPONENT Layers
        )
        install(
            TARGETS ${TARGET_NAME}
            DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT Layers
        )
    endforeach()
elseif(WIN32)
    foreach(TARGET_NAME ${TARGET_NAMES})
        install(
            FILES "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.json"
            DESTINATION "${CMAKE_INSTALL_BINDIR}/api_layers"
            COMPONENT Layers
        )
        install(
            TARGETS ${TARGET_NAME}
            DESTINATION "${CMAKE_INSTALL_BINDIR}/api_layers"
            COMPONENT Layers
        )
    endforeach()
endif()
