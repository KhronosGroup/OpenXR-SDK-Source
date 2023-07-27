#!/usr/bin/env bash
# Copyright (c) 2020-2023 Collabora, Ltd.
#
# SPDX-License-Identifier: Apache-2.0

# Builds the .aar Android prefab artifact for the OpenXR Loader.
# Depends on the tools `cmake`, `ninja`, and `jar` (usually shipped with your JDK)
# in addition to an Android NDK.
#
# Requires that ANDROID_NDK_HOME be set in the environment.
# Pass the argument "clean" to wipe build directories before building.
#
# Touch a file named SNAPSHOT in the root directory to
# make a version suffixed with -SNAPSHOT

set -e

logMsg() {
    echo
    echo "**** $1"
    echo
}

ROOT=$(cd "$(dirname "$0")" && cd .. && pwd)

OPENXR_ANDROID_VERSION_SUFFIX=
if [ -f "${ROOT}/SNAPSHOT" ]; then
    OPENXR_ANDROID_VERSION_SUFFIX=-SNAPSHOT
    logMsg "Building a -SNAPSHOT version"
fi

BUILD_DIR=${BUILD_DIR:-${ROOT}/build-android}
INSTALL_DIR=${INSTALL_DIR:-${ROOT}/build-android-install}

ANDROID_STL=c++_static
CONFIG=Release
ANDROID_PLATFORM=24

logMsg "ANDROID_NDK_HOME: ${ANDROID_NDK_HOME}"
logMsg "ANDROID_STL: ${ANDROID_STL}"
logMsg "CONFIG: ${CONFIG}"
logMsg "ANDROID_PLATFORM: ${ANDROID_PLATFORM}"

logMsg "BUILD_DIR: ${BUILD_DIR}"
logMsg "INSTALL_DIR: ${INSTALL_DIR}"

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
      -DBUILD_LOADER=ON \
      -DBUILD_API_LAYERS=OFF \
      "-DOPENXR_ANDROID_VERSION_SUFFIX=${OPENXR_ANDROID_VERSION_SUFFIX}"

    ninja -C "${BUILD_DIR}/${arch}"

    logMsg "Installing for arch ${arch}"

    for comp in License Headers Loader Prefab; do
        # Component-wise install of the build, since we do not want all components
        cmake -DCMAKE_INSTALL_COMPONENT=${comp} -P "${BUILD_DIR}/${arch}/cmake_install.cmake"
    done
done

# The name of the POM file that CMake made with our decorated version number
DECORATED=$(cd "${BUILD_DIR}/x86/src/loader" && ls openxr_loader*.pom)
cp "${BUILD_DIR}/x86/src/loader/${DECORATED}" .

DIR=$(pwd)
(
    logMsg "Packing AAR file"
    cd "$INSTALL_DIR/openxr"
    AARFILE="$DIR/${DECORATED%.pom}.aar"
    jar --create --file="${AARFILE}" ./*
    logMsg "AAR file created: ${AARFILE}"
)
