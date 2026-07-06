#!/usr/bin/env bash
# Copyright (c) 2020-2026 Collabora, Ltd.
#
# SPDX-License-Identifier: Apache-2.0

# Builds the .aar Android artifacts for the standard API layers.
# Depends on the tools `cmake`, `ninja`, and `jar` (usually shipped with your JDK)
# in addition to an Android NDK.
#
# Requires that ANDROID_NDK_HOME be set in the environment.
# Pass the argument "clean" to wipe build directories before building.
#
# Touch a file named SNAPSHOT in the root directory to
# make a version suffixed with -SNAPSHOT

# Closely related to build-aar.sh: some additional machinery left in here
# to minimize diff and to ease maintenance.

set -e

logMsg() {
    echo
    echo "**** $1"
    echo
}

usage() {
    echo "Usage: $0 <options> [clean]" 1>&2
    echo "" 1>&2
    echo "   clean  Make a clean build. Important for releases, though they are typically done on CI." 1>&2
    echo "   -h     Show this help" 1>&2
    exit 1
}

ROOT=$(cd "$(dirname "$0")" && cd .. && pwd)

OPENXR_ANDROID_VERSION_SUFFIX=
if [ -f "${ROOT}/SNAPSHOT" ]; then
    OPENXR_ANDROID_VERSION_SUFFIX=-SNAPSHOT
    logMsg "Building a -SNAPSHOT version"
fi

BUILD_DIR=${BUILD_DIR:-${ROOT}/build-android-layers}
INSTALL_DIR=${INSTALL_DIR:-${ROOT}/build-android-layers-install}
INSTALL_COMPONENTS=(License Layers)

ANDROID_STL=c++_static
CONFIG=Release
ANDROID_PLATFORM=24

if [ -z "$ANDROID_NDK_HOME" ]; then
    logMsg "No ANDROID_NDK_HOME defined!"
    exit 1
fi

while getopts ":h" o; do

    case "${o}" in
        h)
            usage
        ;;
        *)
            usage
        ;;
    esac
done

logMsg "ANDROID_NDK_HOME: ${ANDROID_NDK_HOME}"
logMsg "ANDROID_STL: ${ANDROID_STL}"
logMsg "CONFIG: ${CONFIG}"
logMsg "ANDROID_PLATFORM: ${ANDROID_PLATFORM}"

logMsg "BUILD_DIR: ${BUILD_DIR}"
logMsg "INSTALL_DIR: ${INSTALL_DIR}"
logMsg "INSTALL_COMPONENTS: ${INSTALL_COMPONENTS[*]}"

logMsg "Wiping install staging dir"
rm -rf "${INSTALL_DIR}"

if [ "$1" = "clean" ]; then
    logMsg "Wiping build dir completely"
    rm -rf "${BUILD_DIR}"
fi

if [ -d "${BUILD_DIR}" ]; then
    logMsg "Removing POM files from build dir"
    find "${BUILD_DIR}" -name "*.pom" -delete
fi

for arch in x86 x86_64 armeabi-v7a arm64-v8a; do
    logMsg "Configuring and building for arch ${arch}"
    cmake -S "${ROOT}" \
    -B "${BUILD_DIR}/${arch}" \
    -G Ninja \
    "-DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake" \
    "-DCMAKE_ANDROID_NDK=${ANDROID_NDK_HOME}" \
    -DANDROID_ABI=${arch} \
    -DANDROID_PLATFORM=${ANDROID_PLATFORM} \
    -DANDROID_STL=${ANDROID_STL} \
    "-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}/openxr" \
    -DCMAKE_BUILD_TYPE=${CONFIG} \
    -DINSTALL_TO_ARCHITECTURE_PREFIXES=ON \
    -DBUILD_TESTS=OFF \
    -DBUILD_CONFORMANCE_TESTS=OFF \
    -DBUILD_LOADER=OFF \
    -DBUILD_API_LAYERS=ON \
    "-DOPENXR_ANDROID_VERSION_SUFFIX=${OPENXR_ANDROID_VERSION_SUFFIX}"

    ninja -C "${BUILD_DIR}/${arch}"

    logMsg "Installing for arch ${arch}"

    # Component-wise install of the build, since we do not want all components
    for comp in "${INSTALL_COMPONENTS[@]}"; do
        cmake --install "${BUILD_DIR}/${arch}/" --strip --component "${comp}"
    done
done

# The name of the POM file that CMake made with our decorated version number
for layer in api_dump core_validation best_practices_validation; do
    TARGET="XrApiLayer_${layer}"
    ARTIFACT="apilayer_${layer}"
    DECORATED=$(cd "${INSTALL_DIR}/openxr" && ls ${ARTIFACT}*.pom)
    cp "${INSTALL_DIR}/openxr/${DECORATED}" .

    # EXTRA_MANIFEST_ENTRIES="${BUILD_DIR}/x86/src/loader/additional_manifest.mf"

    (
        logMsg "Packing AAR files"
        cd "$INSTALL_DIR/openxr"
        AARFILE="$ROOT/${DECORATED%.pom}.aar"
        cd "${TARGET}"
        jar --create --file="${AARFILE}" ./*
        logMsg "AAR file created: ${AARFILE}"
    )

done
