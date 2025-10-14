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

include(CheckCCompilerFlag)

# https://gitlab.khronos.org/openxr/openxr/-/merge_requests/4027 highlighted that
# vendors often have stricter compiler settings
check_c_compiler_flag(-Wimplicit-fallthrough HAVE_IMPLICIT_FALLTHROUGH)
check_c_compiler_flag(-Wundef HAVE_UNDEF)
check_c_compiler_flag(-Wtautological-type-limit-compare HAVE_TAUTOLOGICAL_TYPE_LIMIT)
check_c_compiler_flag(-Wmissing-braces HAVE_MISSING_BRACES)
check_c_compiler_flag(-Wunreachable-code HAVE_UNREACHABLE_CODE)

if(HAVE_IMPLICIT_FALLTHROUGH)
    add_compile_options(-Werror=implicit-fallthrough)
endif()
if(HAVE_UNDEF)
    add_compile_options(-Werror=undef)
endif()
if(HAVE_TAUTOLOGICAL_TYPE_LIMIT)
    add_compile_options(-Werror=tautological-type-limit-compare)
endif()
if(HAVE_MISSING_BRACES)
    add_compile_options(-Werror=missing-braces)
endif()
if(HAVE_UNREACHABLE_CODE)
    add_compile_options(-Werror=unreachable-code)
endif()

if (BUILD_SDK_TESTS)
    add_subdirectory(c_compile_test)
    add_subdirectory(hello_xr)
    add_subdirectory(list_json)
    if(NOT ANDROID)
        add_subdirectory(list)
    endif()
endif()

if(BUILD_LOADER AND BUILD_API_LAYERS)
    add_subdirectory(loader_test)
endif()

if(BUILD_CONFORMANCE_TESTS OR (BUILD_LOADER AND BUILD_API_LAYERS))
    add_subdirectory(test_runtimes)
endif()
