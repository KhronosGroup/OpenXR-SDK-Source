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

usage() {
    echo "Usage: $0 <options> [clean]" 1>&2
    echo "" 1>&2
    echo "   clean  Make a clean build. Important for releases, though they are typically done on CI." 1>&2
    echo "   -s     Skip building sources jar" 1>&2
    echo "   -C     Install CMake config files, only needed if not using Gradle to consume" 1>&2
    echo "   -h     Show this help" 1>&2
    exit 1
}

ROOT=$(cd "$(dirname "$0")" && cd .. && pwd)

OPENXR_ANDROID_VERSION_SUFFIX=
if [ -f "${ROOT}/SNAPSHOT" ]; then
    OPENXR_ANDROID_VERSION_SUFFIX=-SNAPSHOT
    logMsg "Building a -SNAPSHOT version"
fi

BUILD_DIR=${BUILD_DIR:-${ROOT}/build-android}
INSTALL_DIR=${INSTALL_DIR:-${ROOT}/build-android-install}
SKIP_SOURCES_JAR=false
INSTALL_COMPONENTS=(License Headers Loader Prefab)

ANDROID_STL=c++_static
CONFIG=Release
ANDROID_PLATFORM=24

if [ -z "$ANDROID_NDK_HOME" ]; then
    logMsg "No ANDROID_NDK_HOME defined!"
    exit 1
fi

if [ ! -f "maintainer-scripts/archive-sdk.sh" ]; then
    logMsg "archive-sdk.sh not found, assuming building from a source jar. Must skip generating a new one."
    SKIP_SOURCES_JAR=true
fi

while getopts ":hsC" o; do

    case "${o}" in
        C)
            INSTALL_COMPONENTS+=(CMakeConfigs)
        ;;
        s)
            SKIP_SOURCES_JAR=true
        ;;
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
logMsg "SKIP_SOURCES_JAR: ${SKIP_SOURCES_JAR}"

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

    # Component-wise install of the build, since we do not want all components
    for comp in "${INSTALL_COMPONENTS[@]}"; do
        cmake --install "${BUILD_DIR}/${arch}/" --strip --component "${comp}"
    done
done

# The name of the POM file that CMake made with our decorated version number
DECORATED=$(cd "${BUILD_DIR}/x86/src/loader" && ls openxr_loader*.pom)
cp "${BUILD_DIR}/x86/src/loader/${DECORATED}" .

EXTRA_MANIFEST_ENTRIES="${BUILD_DIR}/x86/src/loader/additional_manifest.mf"

(
    logMsg "Packing AAR file"
    cd "$INSTALL_DIR/openxr"
    AARFILE="$ROOT/${DECORATED%.pom}.aar"
    jar --create --file="${AARFILE}" "--manifest=${EXTRA_MANIFEST_ENTRIES}" ./*
    logMsg "AAR file created: ${AARFILE}"
)

if $SKIP_SOURCES_JAR; then
    logMsg "Skipping sources jar"
else
    logMsg "Archiving SDK sources"
    SOURCES_JAR_ROOT=sources-jar
    SOURCES_NAMESPACE_PATH=org/khronos/openxr/openxr_loader_for_android
    rm -f OpenXR-SDK.tar.gz
    rm -rf ${SOURCES_JAR_ROOT}
    mkdir -p ${SOURCES_JAR_ROOT}/${SOURCES_NAMESPACE_PATH}
    bash maintainer-scripts/archive-sdk.sh

    logMsg "Repacking SDK sources as a sources jar"
    SOURCESFILE="$ROOT/${DECORATED%.pom}-sources.jar"
    (
        cd ${SOURCES_JAR_ROOT}/${SOURCES_NAMESPACE_PATH}
        tar xf "${ROOT}/OpenXR-SDK.tar.gz"

        # Add files we want, for reproducibility
        mkdir -p maintainer-scripts
        cp "${ROOT}/maintainer-scripts/build-aar.sh" maintainer-scripts

        # Drop files/dirs we don't care about
        rm -rf \
        .appveyor.yml \
        .azure-pipelines \
        .editorconfig \
        .git-blame-ignore-revs \
        .gitattributes \
        .github \
        .gitignore \
        doc

        # Drop licenses not actually used in this code, based on `reuse lint` in OpenXR-SDK
        rm -f \
        LICENSES/OFL-1.1-RFN.txt \
        LICENSES/Unlicense.txt \
        LICENSES/LicenseRef-Khronos-Free-Use-License-for-Software-and-Documentation.txt \
        LICENSES/ISC.txt \
        LICENSES/LicenseRef-KhronosSpecCopyright-WithNormativeWording-v10.txt \
        LICENSES/Zlib.txt

        # Remove the bare copy of Apache-2.0 named LICENSE since it is confusing.
        rm -f LICENSE

        # Move the copying and license files to the jar root
        mv COPYING.adoc "$ROOT/${SOURCES_JAR_ROOT}"
        mv LICENSES "$ROOT/${SOURCES_JAR_ROOT}"
    )
    (
        cd ${SOURCES_JAR_ROOT}
        jar --create --file="${SOURCESFILE}" "--manifest=${EXTRA_MANIFEST_ENTRIES}" ./*
        logMsg "Sources JAR file created: ${SOURCESFILE}"
    )
fi
