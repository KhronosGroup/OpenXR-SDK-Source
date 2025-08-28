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

# Copy the api_layer_platform_defines.h file and place it in the binary (build) directory.
configure_file(
    ../api_layer_platform_defines.h ${CMAKE_CURRENT_BINARY_DIR} COPYONLY
)

# Basics for best practices API Layer

gen_xr_layer_json(
    "${CMAKE_CURRENT_BINARY_DIR}/../XrApiLayer_best_practices_validation.json"
    KHR_best_practices_validation
    "$<TARGET_FILE_NAME:XrApiLayer_best_practices_validation>"
    1
    "API Layer to modify runtime behavior in conformant but perhaps unexpected ways"
    ""
)

# Flag generated files that aren't generated in this directory.
set_source_files_properties(
    ${COMMON_GENERATED_OUTPUT} PROPERTIES GENERATED TRUE
)

set(GENERATED_OUTPUT)
set(GENERATED_DEPENDS)
set(BEST_PRACTICES_GENERATED_OUTPUT ${GENERATED_OUTPUT})
set(BEST_PRACTICES_GENERATED_DEPENDS ${GENERATED_DEPENDS})
unset(GENERATED_OUTPUT)
unset(GENERATED_DEPENDS)

add_library(
    XrApiLayer_best_practices_validation MODULE
    layer_utils.cpp
    best_practices_validation.cpp
    # target-specific generated files
    ${BEST_PRACTICES_GENERATED_OUTPUT}
    # Dispatch table
    ${COMMON_GENERATED_OUTPUT}
    # Included in this list to force generation
    "${CMAKE_CURRENT_BINARY_DIR}/../XrApiLayer_best_practices_validation.json"
)
set_target_properties(
    XrApiLayer_best_practices_validation PROPERTIES FOLDER ${API_LAYERS_FOLDER}
)

target_link_libraries(
    XrApiLayer_best_practices_validation PRIVATE Threads::Threads
                                                 OpenXR::headers
)
if(ANDROID)
    target_link_libraries(
        XrApiLayer_best_practices_validation PRIVATE ${ANDROID_LOG_LIBRARY}
    )
endif()
target_compile_definitions(
    XrApiLayer_best_practices_validation
    PRIVATE ${OPENXR_ALL_SUPPORTED_DEFINES}
)
add_dependencies(XrApiLayer_best_practices_validation xr_common_generated_files)

target_include_directories(
    XrApiLayer_best_practices_validation
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/common
        # for generated dispatch table
        ../..
        ${CMAKE_CURRENT_BINARY_DIR}/../..
        # for target-specific generated files
        .
        ${CMAKE_CURRENT_BINARY_DIR}
)

if(XR_USE_GRAPHICS_API_VULKAN)
    target_include_directories(
        XrApiLayer_best_practices_validation PRIVATE ${Vulkan_INCLUDE_DIRS}
    )
endif()

if(WIN32)
    # Windows best_practices-specific information
    target_compile_definitions(
        XrApiLayer_best_practices_validation PRIVATE _CRT_SECURE_NO_WARNINGS
    )
    # Turn off transitional "changed behavior" warning message for Visual Studio versions prior to 2015.
    # The changed behavior is that constructor initializers are now fixed to clear the struct members.
    target_compile_options(
        XrApiLayer_best_practices_validation
        PRIVATE
            "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,19>>:/wd4351>"
    )

endif()

# Dynamic Library:
#  - Make build depend on the module definition/version script/export map
#  - Add the linker flag (except windows)
if(WIN32)
    # XrApiLayer_best_practices
    target_sources(
        XrApiLayer_best_practices_validation
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_best_practices_validation.def"
    )

elseif(APPLE)
    # XrApiLayer_best_practices
    set_target_properties(
        XrApiLayer_best_practices_validation
        PROPERTIES
            LINK_FLAGS
            "-Wl,-exported_symbols_list,\"${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_best_practices_validation.expsym\""
    )
    target_sources(
        XrApiLayer_best_practices_validation
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_best_practices_validation.expsym"
    )

else()
    # XrApiLayer_best_practices
    set_target_properties(
        XrApiLayer_best_practices_validation
        PROPERTIES
            LINK_FLAGS
            "-Wl,--version-script=\"${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_best_practices_validation.map\""
    )
    target_sources(
        XrApiLayer_best_practices_validation
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_best_practices_validation.map"
    )

endif()

# Install explicit layers
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LAYER_MANIFEST_INSTALL_DIR
        "${CMAKE_INSTALL_DATAROOTDIR}/openxr/${MAJOR}/api_layers/explicit.d"
    )
    set(LAYER_BINARY_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
elseif(WIN32)

    set(LAYER_MANIFEST_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}/api_layers")
    set(LAYER_BINARY_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}/api_layers")
endif()

if(LAYER_MANIFEST_INSTALL_DIR)
    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/../XrApiLayer_best_practices_validation.json"
        DESTINATION ${LAYER_MANIFEST_INSTALL_DIR}
        COMPONENT Layers
    )
    install(
        TARGETS XrApiLayer_best_practices_validation
        DESTINATION ${LAYER_BINARY_INSTALL_DIR}
        COMPONENT Layers
    )
endif()
