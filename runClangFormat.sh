#!/usr/bin/env bash
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

set -e
(
    PREFERRED_CLANG_FORMAT=clang-format-14
    ACCEPTABLE_CLANG_FORMATS=(
        "${PREFERRED_CLANG_FORMAT}"
        clang-format-11
        clang-format-12
        clang-format-13
        clang-format-14
        clang-format-15
        clang-format-16
        clang-format-17
        clang-format-18
        clang-format-19
        clang-format-20
        clang-format)
    cd "$(dirname "$0")"
    if [ ! "${CLANGFORMAT}" ]; then
        for tool in "${ACCEPTABLE_CLANG_FORMATS[@]}"; do
            if which "$tool" > /dev/null 2> /dev/null; then
                CLANGFORMAT=$tool
                break
            fi
        done
    fi
    if [ ! "${CLANGFORMAT}" ]; then
        echo "Could not find clang-format. Prefer ${PREFERRED_CLANG_FORMAT} but will accept newer." 1>&2
        echo "Looked for the names: ${ACCEPTABLE_CLANG_FORMATS[*]}"
        exit 1
    fi
    echo "'Official' clang-format version recommended is ${PREFERRED_CLANG_FORMAT}. Currently using:"
    ${CLANGFORMAT} --version
    # The "-and -not" lines exclude the Jinja2 templates which are almost but not quite C++-parseable.
    # The trailing + means that find will pass more than one file to a clang-format call,
    # to reduce overhead.
    find . \( -wholename ./src/\* \) \
        -and -not \( -wholename ./src/external/\* \) \
        -and -not \( -wholename ./src/scripts/\* \) \
        -and -not \( -ipath \*/.cxx/\* \) \
        -and \( -name \*.hpp -or -name \*.h -or -name \*.cpp -or -name \*.c \) \
        -exec "${CLANGFORMAT}" -i -style=file {} +

)
