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

add_subdirectory(c_compile_test)
add_subdirectory(hello_xr)
add_subdirectory(list_json)

if(NOT ANDROID)
    add_subdirectory(list)
endif()

if(BUILD_LOADER AND BUILD_API_LAYERS)
    add_subdirectory(loader_test)
endif()
