#!/usr/bin/env bash
# Copyright (c) 2020-2022 Collabora, Ltd.
#
# SPDX-License-Identifier: Apache-2.0
set -e
MAINT_SCRIPTS=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$(dirname "$0")" && cd .. && pwd)

# This file would tell the build scripts to append -SNAPSHOT to the version, make sure it's not there.
rm -f "${ROOT}/SNAPSHOT"

# Build AAR
"${MAINT_SCRIPTS}/build-aar.sh"

# Publish AAR using Gradle
cd "${MAINT_SCRIPTS}/publish-aar"
./gradlew publishMavenPublicationToBuildDirRepository publishMavenPublicationToOSSRHRepository

# Need to explicitly "close and release" it then
# TODO disabled because it errors out
# ./gradlew closeAndReleaseRepository
