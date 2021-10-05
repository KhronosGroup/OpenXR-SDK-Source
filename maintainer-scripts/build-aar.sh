#!/usr/bin/env bash
# Copyright (c) 2020-2021 Collabora, Ltd.
#
# SPDX-License-Identifier: Apache-2.0

set -e
ROOT=$(cd $(dirname $0) && cd .. && pwd)

OPENXR_ANDROID_VERSION_SUFFIX=
if [ -f "${ROOT}/SNAPSHOT" ]; then
    OPENXR_ANDROID_VERSION_SUFFIX=-SNAPSHOT
    echo "Building a -SNAPSHOT version"
fi

BUILD_DIR=${BUILD_DIR:-${ROOT}/build-android}
INSTALL_DIR=${INSTALL_DIR:-${ROOT}/build-android-install}

ANDROID_STL=c++_shared

rm -rf "${INSTALL_DIR}"
if [ -d "${BUILD_DIR}" ]; then
    find "${BUILD_DIR}" -name "*.pom" -delete
fi

for arch in x86 x86_64 armeabi-v7a arm64-v8a; do
    cmake -S "${ROOT}" \
      -B "${BUILD_DIR}/${arch}" \
      -G Ninja \
      "-DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake" \
      "-DCMAKE_ANDROID_NDK=${ANDROID_NDK_HOME}" \
      -DANDROID_ABI=${arch} \
      -DANDROID_PLATFORM=24 \
      -DANDROID_STL=${ANDROID_STL} \
      "-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}/openxr" \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DINSTALL_TO_ARCHITECTURE_PREFIXES=ON \
      -DBUILD_TESTS=OFF \
      -DBUILD_CONFORMANCE_TESTS=OFF \
      -DBUILD_LOADER=ON \
      -DBUILD_API_LAYERS=OFF \
      "-DOPENXR_ANDROID_VERSION_SUFFIX=${OPENXR_ANDROID_VERSION_SUFFIX}"
    ninja -C "${BUILD_DIR}/${arch}"
    # ninja -C "${BUILD_DIR}/${arch}" install
    for comp in License Headers Loader Prefab; do
        cmake -DCMAKE_INSTALL_COMPONENT=${comp} -P "${BUILD_DIR}/${arch}/cmake_install.cmake"
    done
done
DECORATED=$(cd "${BUILD_DIR}/x86/src/loader" && ls openxr_loader*.pom)
cp "${BUILD_DIR}/x86/src/loader/${DECORATED}" .

DIR=$(pwd)
(
    cd "$INSTALL_DIR/openxr"
    7za a -r ../openxr.zip *
    mv ../openxr.zip "$DIR/${DECORATED%.pom}.aar"
)
