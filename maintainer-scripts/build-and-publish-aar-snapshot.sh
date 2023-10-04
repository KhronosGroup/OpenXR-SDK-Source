#!/usr/bin/env bash
# Copyright (c) 2020-2021 Collabora, Ltd.
#
# SPDX-License-Identifier: Apache-2.0
set -e
MAINT_SCRIPTS=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$(dirname "$0")" && cd .. && pwd)

# This file tells the build scripts to append -SNAPSHOT to the version.
touch "${ROOT}/SNAPSHOT"

# Build AAR
"${MAINT_SCRIPTS}/build-aar.sh"

# Publish AAR using Gradle
cd "${MAINT_SCRIPTS}/publish-aar"
./gradlew publishMavenPublicationToBuildDirRepository publishMavenPublicationToOSSRH-SnapshotsRepository
