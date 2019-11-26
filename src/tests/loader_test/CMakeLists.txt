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

add_executable(loader_test
    loader_test_utils.cpp
    loader_test.cpp
    ${PROJECT_SOURCE_DIR}/src/common/gfxwrapper_opengl.c
    ${PROJECT_SOURCE_DIR}/src/common/filesystem_utils.cpp
)
set_target_properties(loader_test PROPERTIES FOLDER ${TESTS_FOLDER})

add_dependencies(loader_test
    generate_openxr_header
)
target_include_directories(loader_test
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
    PRIVATE ${PROJECT_BINARY_DIR}/src
    PRIVATE ${PROJECT_BINARY_DIR}/include
    PRIVATE ${PROJECT_SOURCE_DIR}/src/common
    PRIVATE ${PROJECT_SOURCE_DIR}/external/include
)
if(VulkanHeaders_FOUND)
    target_include_directories(loader_test
        PRIVATE ${VulkanHeaders_INCLUDE_DIRS}
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(loader_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(loader_test PRIVATE /Zc:wchar_t /Zc:forScope /W4 /WX)
    target_link_libraries(loader_test openxr_loader opengl32 d3d11)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(
        loader_test PRIVATE -Wall -Wno-unused-function -Wno-format-truncation
    )

    if(PRESENTATION_BACKEND MATCHES "xlib")
        target_link_libraries(
            loader_test ${X11_Xxf86vm_LIB} ${X11_Xrandr_LIB} ${X11_LIBRARIES}
        )
    elseif(PRESENTATION_BACKEND MATCHES "xcb")
        target_link_libraries(loader_test ${XCB_LIBRARIES})
    elseif(PRESENTATION_BACKEND MATCHES "wayland")
        target_link_libraries(loader_test ${WAYLAND_LIBRARIES})
        target_sources(loader_test PRIVATE ${WAYLAND_PROTOCOL_SRC})
    endif()
    target_link_libraries(loader_test -lstdc++fs openxr_loader m -lpthread GL)
else()
    message(FATAL_ERROR "Unsupported Platform")
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resources)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resources/layers)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resources/runtimes)

add_subdirectory(test_layers)
add_subdirectory(test_runtimes)
