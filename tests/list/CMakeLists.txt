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

add_executable(list
    list.cpp
)
add_dependencies(list
    generate_openxr_header
)
target_include_directories(list
    PRIVATE ${CMAKE_SOURCE_DIR}/src
    PRIVATE ${CMAKE_SOURCE_DIR}/src/common
    PRIVATE ${CMAKE_BINARY_DIR}/include
    PRIVATE ${CMAKE_SOURCE_DIR}/external/include
    ${VulkanHeaders_INCLUDE_DIRS}
)
if(USE_LOADER)
    target_compile_definitions(list PRIVATE USE_OPENXR_LOADER)
    if(WIN32)
        set(OPENXR_LIB openxr_loader-${MAJOR}_${MINOR})
    else()
        set(OPENXR_LIB openxr_loader)
    endif()
else()
    set(OPENXR_LIB openxr_sample_impl)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_options(list PRIVATE /Zc:wchar_t /Zc:forScope /W4 /WX)
    target_link_libraries(list ${OPENXR_LIB} opengl32)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(list PRIVATE -Wall)
    target_link_libraries(list ${OPENXR_LIB} m pthread GL ${XLIB_LIBRARIES} ${XCB_LIBRARIES} ${WAYLAND_LIBRARIES})
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_options(list PRIVATE -Wall -Wno-unused-function -x objective-c++)
    find_library(FOUNDATION_FRAMEWORK Foundation)
    mark_as_advanced(FOUNDATION_FRAMEWORK)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore)
    mark_as_advanced(QUARTZCORE_FRAMEWORK)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics)
    mark_as_advanced(COREGRAPHICS_FRAMEWORK)
    if( IOS )
        find_library(APPKIT_FRAMEWORK UIKit)
        find_library(OPENGL_FRAMEWORK OpenGLES)
    else()
        find_library(APPKIT_FRAMEWORK AppKit)
        find_library(OPENGL_FRAMEWORK OpenGL)
    endif()
    mark_as_advanced(OPENGL_FRAMEWORK)
    mark_as_advanced(APPKIT_FRAMEWORK)
    target_link_libraries(list m -lpthread ${EXTRA_LIBS} ${OPENXR_LIB} ${FOUNDATION_FRAMEWORK} ${OPENGL_FRAMEWORK} ${COREGRAPHICS_FRAMEWORK} ${QUARTZCORE_FRAMEWORK} ${APPKIT_FRAMEWORK})
endif()

set_target_properties(list
    PROPERTIES FOLDER tests_loader
)

