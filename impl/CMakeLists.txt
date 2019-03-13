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

# Use this feature for Windows to automatically generate an exports file for the DLL.
# See https://blog.kitware.com/create-dlls-on-windows-without-declspec-using-new-cmake-export-all-feature/
include(GenerateExportHeader)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS true)

add_library(openxr_sample_impl SHARED
    openxr_sample_impl.cpp
    si_display_timing.cpp
    nanoseconds.cpp
    si_dev_opengl.cpp
    ${CMAKE_SOURCE_DIR}/src/common/gfxwrapper_opengl.c
    ${CMAKE_BINARY_DIR}/src/impl/xr_generated_sample_impl.cpp
    ${WAYLAND_PROTOCOL_SRC}
)
set_source_files_properties(
    ${CMAKE_BINARY_DIR}/src/impl/xr_generated_sample_impl.cpp
    PROPERTIES GENERATED TRUE
)
add_dependencies(openxr_sample_impl
    generate_openxr_header
    xr_global_generated_files
    impl_gen_files
    generated_impl_json_files
)
target_include_directories(openxr_sample_impl
    PRIVATE ${CMAKE_SOURCE_DIR}/src
    PRIVATE ${CMAKE_SOURCE_DIR}/src/common
    PRIVATE ${CMAKE_BINARY_DIR}/src
    PRIVATE ${CMAKE_BINARY_DIR}/src/impl
    PRIVATE ${CMAKE_BINARY_DIR}/include
    PRIVATE ${CMAKE_SOURCE_DIR}/external/include
    ${VulkanHeaders_INCLUDE_DIRS}
)
target_compile_definitions(openxr_sample_impl
    PRIVATE API_NAME="OpenXR"
)
if(USE_LOADER)
    target_compile_definitions(openxr_sample_impl
        PRIVATE USE_OPENXR_LOADER
    )
endif()

add_custom_target(impl_gen_files
    DEPENDS
        xr_generated_sample_impl.cpp
)
run_xr_xml_generate(automatic_source_generator.py xr_generated_sample_impl.cpp)

macro(gen_xr_runtime_json filename libfile)
    add_custom_command(OUTPUT ${filename}
        COMMAND 
            ${CMAKE_COMMAND} -E env "PYTHONPATH=${CMAKE_SOURCE_DIR}/specification/scripts:${CMAKE_SOURCE_DIR}/src/scripts:$ENV{PYTHONPATH}"
            ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/src/scripts/generate_runtime_manifest.py -f ${filename} -l ${libfile} ${ARGN}
        DEPENDS
            ${CMAKE_SOURCE_DIR}/src/scripts/generate_runtime_manifest.py
        COMMENT "Generating Runtime JSON ${filename} using -f ${filename} -l ${libfile} ${ARGN}"
    )
endmacro()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(openxr_sample_impl PROPERTIES SOVERSION "${MAJOR}" VERSION "${MAJOR}.${MINOR}.${PATCH}")
    if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(openxr_sample_impl
            PRIVATE -Wno-unused-function -Wno-sign-compare
        )
    endif()
    add_custom_target(libopenxr_sample_impl.so.${MAJOR}.${MINOR} ALL
        COMMAND ${CMAKE_COMMAND} -E create_symlink libopenxr_sample_impl.so.${MAJOR}.${MINOR}.${PATCH} liblibopenxr_sample_implopenxr_loader.so.${MAJOR}.${MINOR})
    gen_xr_runtime_json(
        ${CMAKE_BINARY_DIR}/src/impl/openxr_sample_impl.json
        ${CMAKE_CURRENT_BINARY_DIR}/libopenxr_sample_impl.so.${MAJOR}
    )
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(openxr_sample_impl opengl32)
    target_compile_options(openxr_sample_impl PRIVATE)
    generate_export_header(openxr_sample_impl)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS false)
    # Need to copy DLL to client directories so clients can easily load it.
    if(CMAKE_GENERATOR MATCHES "^Visual Studio.*")
        file(TO_NATIVE_PATH ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/openxr_sample_impl.dll COPY_SRC_PATH)
        file(TO_NATIVE_PATH ${CMAKE_CURRENT_BINARY_DIR}/../tests/triangle/$<CONFIGURATION>/ COPY_DST_TEST_PATH)
        add_custom_command(TARGET openxr_sample_impl POST_BUILD
            COMMAND xcopy /Y /I ${COPY_SRC_PATH} ${COPY_DST_TEST_PATH})
    endif()
    gen_xr_runtime_json(
        ${CMAKE_BINARY_DIR}/src/impl/openxr_sample_impl.json
        ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/openxr_sample_impl.dll
    )
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_MACOSX_RPATH 0)
    if(${CMAKE_VERSION} VERSION_LESS "3.0")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -x objective-c++")
    else()
        target_compile_options(openxr_sample_impl PRIVATE -Wall -Wno-unused-function -x objective-c++)
    endif()
    set_target_properties(openxr_sample_impl PROPERTIES SOVERSION "${MAJOR}" VERSION "${MAJOR}.${MINOR}.${PATCH}")
    find_library(FOUNDATION_FRAMEWORK Foundation)
    mark_as_advanced(FOUNDATION_FRAMEWORK)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore)
    mark_as_advanced(QUARTZCORE_FRAMEWORK)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics)
    mark_as_advanced(COREGRAPHICS_FRAMEWORK)
    if(IOS)
        find_library(APPKIT_FRAMEWORK UIKit)
        find_library(OPENGL_FRAMEWORK OpenGLES)
    else()
        find_library(APPKIT_FRAMEWORK AppKit)
        find_library(OPENGL_FRAMEWORK OpenGL)
    endif()
    mark_as_advanced(OPENGL_FRAMEWORK)
    mark_as_advanced(APPKIT_FRAMEWORK)
    target_link_libraries(openxr_sample_impl ${EXTRA_LIBS} ${FOUNDATION_FRAMEWORK} ${OPENGL_FRAMEWORK} ${COREGRAPHICS_FRAMEWORK} ${QUARTZCORE_FRAMEWORK} ${APPKIT_FRAMEWORK})
    add_custom_target(libopenxr_sample_impl.${MAJOR}.${MINOR}.dylib ALL
        COMMAND ${CMAKE_COMMAND} -E create_symlink libopenxr_sample_impl.${MAJOR}.${MINOR}.${PATCH}.dylib liblibopenxr_sample_implopenxr_loader.${MAJOR}.${MINOR}.dylib)
    gen_xr_runtime_json(
        ${CMAKE_BINARY_DIR}/src/impl/openxr_sample_impl.json
        ${CMAKE_CURRENT_BINARY_DIR}/libopenxr_sample_impl.${MAJOR}.dylib
    )
endif()

add_custom_target(generated_impl_json_files
    DEPENDS
        ${CMAKE_BINARY_DIR}/src/impl/openxr_sample_impl.json
)

