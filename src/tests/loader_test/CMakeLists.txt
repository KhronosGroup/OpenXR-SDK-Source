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

add_executable(loader_test
    loader_test_utils.cpp
    loader_test.cpp
)
openxr_add_filesystem_utils(loader_test)
set_target_properties(loader_test PROPERTIES FOLDER ${LOADER_TESTS_FOLDER})
target_link_libraries(loader_test PRIVATE OpenXR::openxr_loader)

add_dependencies(loader_test
    generate_openxr_header
    XrApiLayer_test
    XrApiLayer_api_dump
    test_runtime
)
if(TARGET openxr-gfxwrapper)
    target_link_libraries(loader_test PRIVATE openxr-gfxwrapper)
endif()
target_include_directories(
    loader_test
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
    PRIVATE ${PROJECT_BINARY_DIR}/src
    PRIVATE ${PROJECT_BINARY_DIR}/include
    PRIVATE ${PROJECT_SOURCE_DIR}/src/common
    PRIVATE ${PROJECT_SOURCE_DIR}/external/include
)
if(Vulkan_FOUND)
    target_include_directories(loader_test
        PRIVATE ${Vulkan_INCLUDE_DIRS}
    )
endif()

if(WIN32)
    if(MSVC)
        target_compile_definitions(loader_test PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(loader_test PRIVATE /Zc:wchar_t /Zc:forScope /W4)
        if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            # If actually msvc and not clang-cl
            target_compile_options(loader_test PRIVATE /WX)
        endif()
    endif()
    target_link_libraries(loader_test PRIVATE d3d11)
endif()

add_test(NAME loader_test
    COMMAND loader_test
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:loader_test>")

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resources)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resources/layers)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resources/runtimes)

add_subdirectory(test_layers)
add_subdirectory(test_runtimes)
