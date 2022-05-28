# Copyright (c) 2017 The Khronos Group Inc.
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

add_executable(openxr_c_compile_test
    main.c
)
add_dependencies(openxr_c_compile_test
    generate_openxr_header
)
target_include_directories(openxr_c_compile_test
    PRIVATE ${PROJECT_SOURCE_DIR}/src
    PRIVATE ${PROJECT_SOURCE_DIR}/src/common
    PRIVATE ${PROJECT_BINARY_DIR}/include
    PRIVATE ${PROJECT_SOURCE_DIR}/external/include
)
if(Vulkan_FOUND)
    target_include_directories(openxr_c_compile_test
        PRIVATE ${Vulkan_INCLUDE_DIRS}
    )
endif()

target_link_libraries(openxr_c_compile_test openxr_loader)
if(MSVC)
    target_compile_options(openxr_c_compile_test PRIVATE /Zc:wchar_t /Zc:forScope /W4 /WX)
endif()

set_target_properties(openxr_c_compile_test PROPERTIES FOLDER ${TESTS_FOLDER})
