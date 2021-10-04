# Copyright (c) 2017-2021, The Khronos Group Inc.
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

# Copy the api_layer_platform_defines.h file and place it in the binary (build) directory.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/api_layer_platform_defines.h ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)

if(WIN32)
    set(LAYER_MANIFEST_PREFIX "./")
else()
    set(LAYER_MANIFEST_PREFIX)
endif()

# Basics for api_dump API Layer

gen_xr_layer_json(
    ${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_api_dump.json
    LUNARG_api_dump
    ${LAYER_MANIFEST_PREFIX}$<TARGET_FILE_NAME:XrApiLayer_api_dump>
    1
    "API Layer to record api calls as they occur"
    ""
)

# Flag generated files that aren't generated in this directory.
set_source_files_properties(
    ${COMMON_GENERATED_OUTPUT}
    PROPERTIES GENERATED TRUE
)

set(GENERATED_OUTPUT)
set(GENERATED_DEPENDS)
run_xr_xml_generate(api_dump_generator.py xr_generated_api_dump.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/automatic_source_generator.py)
run_xr_xml_generate(api_dump_generator.py xr_generated_api_dump.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/automatic_source_generator.py)

add_library(XrApiLayer_api_dump SHARED
    api_dump.cpp
    ${PROJECT_SOURCE_DIR}/src/common/hex_and_handles.h
    # target-specific generated files
    ${GENERATED_OUTPUT}

    # Dispatch table
    ${COMMON_GENERATED_OUTPUT}

    # Included in this list to force generation
    ${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_api_dump.json
)
set_target_properties(XrApiLayer_api_dump PROPERTIES FOLDER ${API_LAYERS_FOLDER})

target_link_libraries(XrApiLayer_api_dump PRIVATE Threads::Threads)
target_compile_definitions(XrApiLayer_api_dump PRIVATE ${OPENXR_ALL_SUPPORTED_DEFINES})
add_dependencies(XrApiLayer_api_dump
    generate_openxr_header
    xr_global_generated_files
)

target_include_directories(XrApiLayer_api_dump
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src/common
    ${CMAKE_CURRENT_SOURCE_DIR}

    # for OpenXR headers
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include

    # for generated dispatch table
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_BINARY_DIR}/..

    # for target-specific generated files
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
if(Vulkan_FOUND)
    target_include_directories(XrApiLayer_api_dump
        PRIVATE ${Vulkan_INCLUDE_DIRS}
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
run_xr_xml_generate(validation_layer_generator.py xr_generated_core_validation.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/automatic_source_generator.py)
run_xr_xml_generate(validation_layer_generator.py xr_generated_core_validation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/automatic_source_generator.py)

add_library(XrApiLayer_core_validation SHARED
    core_validation.cpp
    ${PROJECT_SOURCE_DIR}/src/common/hex_and_handles.h
    ${PROJECT_SOURCE_DIR}/src/common/object_info.cpp
    ${PROJECT_SOURCE_DIR}/src/common/object_info.h

    # target-specific generated files
    ${GENERATED_OUTPUT}

    # Dispatch table
    ${COMMON_GENERATED_OUTPUT}

    # Included in this list to force generation
    ${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_core_validation.json
)
set_target_properties(XrApiLayer_core_validation PROPERTIES FOLDER ${API_LAYERS_FOLDER})

target_link_libraries(XrApiLayer_core_validation PRIVATE Threads::Threads)
target_compile_definitions(XrApiLayer_core_validation PRIVATE ${OPENXR_ALL_SUPPORTED_DEFINES})
add_dependencies(XrApiLayer_core_validation
    generate_openxr_header
    xr_global_generated_files
)
target_include_directories(XrApiLayer_core_validation
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src/common

    # for OpenXR headers
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include

    # for generated dispatch table
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_BINARY_DIR}/..

    # for target-specific generated files
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}

)
if(Vulkan_FOUND)
    target_include_directories(XrApiLayer_core_validation
        PRIVATE ${Vulkan_INCLUDE_DIRS}
    )
endif()

if(WIN32)
    # Windows api_dump-specific information
    target_compile_definitions(XrApiLayer_api_dump PRIVATE _CRT_SECURE_NO_WARNINGS)
    # Turn off transitional "changed behavior" warning message for Visual Studio versions prior to 2015.
    # The changed behavior is that constructor initializers are now fixed to clear the struct members.
    target_compile_options(XrApiLayer_api_dump PRIVATE "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,19>>:/wd4351>")

    # Windows core_validation-specific information
    target_compile_definitions(XrApiLayer_core_validation PRIVATE _CRT_SECURE_NO_WARNINGS)
    # Turn off transitional "changed behavior" warning message for Visual Studio versions prior to 2015.
    # The changed behavior is that constructor initializers are now fixed to clear the struct members.
    target_compile_options(XrApiLayer_core_validation PRIVATE "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,19>>:/wd4351>")

elseif(APPLE)
    # Apple api_dump-specific information
    set_target_properties(XrApiLayer_api_dump PROPERTIES LINK_FLAGS "-Wl")

    # Apple core_validation-specific information
    set_target_properties(XrApiLayer_core_validation PROPERTIES LINK_FLAGS "-Wl")

else()
    # Linux api_dump-specific information
    set_target_properties(XrApiLayer_api_dump PROPERTIES LINK_FLAGS "-Wl,-Bsymbolic,--exclude-libs,ALL")

    # Linux core_validation-specific information
    set_target_properties(XrApiLayer_core_validation PROPERTIES LINK_FLAGS "-Wl,-Bsymbolic,--exclude-libs,ALL")
endif()

# Install explicit layers
set(TARGET_NAMES
    XrApiLayer_api_dump
    XrApiLayer_core_validation)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    foreach(TARGET_NAME ${TARGET_NAMES})
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.json
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/openxr/${MAJOR}/api_layers/explicit.d
            COMPONENT Layers)
        install(TARGETS ${TARGET_NAME}
            DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT Layers)
    endforeach()
elseif(WIN32)
    foreach(TARGET_NAME ${TARGET_NAMES})
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.json
            DESTINATION ${CMAKE_INSTALL_BINDIR}/api_layers
            COMPONENT Layers)
        install(TARGETS ${TARGET_NAME} 
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/api_layers
            COMPONENT Layers)
    endforeach()
endif()
