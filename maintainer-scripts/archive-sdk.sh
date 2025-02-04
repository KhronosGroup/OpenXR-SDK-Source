#!/bin/sh
# Copyright (c) 2019-2025 The Khronos Group Inc.
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

# archive-sdk.sh - Generate a tarball containing the repo subset and
# generated files for OpenXR-SDK
#
# Usage: ./archive-sdk.sh

set -e

(
# shellcheck disable=SC2086
SCRIPTS=$(cd "$(dirname $0)" && pwd)
# shellcheck disable=SC2086
cd "$(dirname $0)/.."
ROOT=$(pwd)
export ROOT

# shellcheck disable=SC1091
. "$SCRIPTS/common.sh"

TARNAME=OpenXR-SDK

# shellcheck disable=SC2046
makeSubset "$TARNAME" $(getSDKFilenames)
(
    if [ -f COPYING.adoc ]; then
        # Add the shared COPYING.adoc used in all GitHub projects derived from the internal openxr repo
        add_to_tar "$TARNAME" COPYING.adoc
    fi

    cd github

    # Add the shared public .mailmap used in all GitHub projects derived from the internal openxr repo
    if [ -f .mailmap ]; then
        # It's in the github folder.
        add_to_tar "$TARNAME" .mailmap
    elif [ -f ../.mailmap ]; then
        # It's in the root.
        cd ..
        add_to_tar "$TARNAME" .mailmap
        cd github
    fi

    if [ -f COPYING.adoc ] && ! [ -f ../COPYING.adoc ]; then
        # If we didn't get it before, maybe we got it now.
        add_to_tar "$TARNAME" COPYING.adoc
    fi

    cd sdk
    # Add the SDK-specific README
    add_to_tar "$TARNAME" README.md
    # Add the pull request template
    add_to_tar "$TARNAME" .github/pull_request_template.md
)

# Read the list of headers we should generate, and generate them.
while read -r header; do
    header=$(echo "$header" | sed 's/[[:space:]]*$//')
    generate_spec include/openxr "$header" "$TARNAME"
done < include/generated_header_list.txt

# These go just in SDK
generate_src src xr_generated_dispatch_table_core.c  "$TARNAME"
generate_src src xr_generated_dispatch_table_core.h  "$TARNAME"
generate_src src xr_generated_dispatch_table.c  "$TARNAME"
generate_src src xr_generated_dispatch_table.h  "$TARNAME"
generate_src src/loader xr_generated_loader.cpp  "$TARNAME"
generate_src src/loader xr_generated_loader.hpp  "$TARNAME"

# If the loader doc has been generated, include it too.
if [ -f specification/generated/out/1.1/loader.html ]; then
    mkdir -p doc/loader
    cp specification/generated/out/1.1/loader.html doc/loader/OpenXR_loader_design.html
    add_to_tar "$TARNAME" doc/loader/OpenXR_loader_design.html
fi

echo
gzip_a_tar "$TARNAME"
)
