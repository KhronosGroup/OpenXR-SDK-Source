#!/bin/sh
# Copyright (c) 2019-2021, The Khronos Group Inc.
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
SCRIPTS=$(cd $(dirname $0) && pwd)
cd $(dirname $0)/..
ROOT=$(pwd)
export ROOT

# shellcheck source=common.sh
. $SCRIPTS/common.sh

SDK_TARNAME=OpenXR-SDK

makeSubset "$SDK_TARNAME" $(getSDKFilenames)
(
    cd github/sdk
    # Add the SDK-specific README
    add_to_tar "$SDK_TARNAME" README.md
)

for header in openxr.h openxr_platform.h openxr_reflection.h; do
    generate_spec include/openxr $header "$SDK_TARNAME"
done

# These go just in SDK
generate_src src xr_generated_dispatch_table.c  "$SDK_TARNAME"
generate_src src xr_generated_dispatch_table.h  "$SDK_TARNAME"
generate_src src/loader xr_generated_loader.cpp  "$SDK_TARNAME"
generate_src src/loader xr_generated_loader.hpp  "$SDK_TARNAME"

# If the loader doc has been generated, include it too.
if [ -f specification/out/1.0/loader.html ]; then
    mkdir -p doc/loader
    cp specification/out/1.0/loader.html doc/loader/OpenXR_loader_design.html
    add_to_tar "$SDK_TARNAME" doc/loader/OpenXR_loader_design.html
fi

echo
gzip_a_tar "$SDK_TARNAME"
)
