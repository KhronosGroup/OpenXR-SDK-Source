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

# common.sh - Utilities shared by multiple shell scripts

SPEC_SCRIPTS=$ROOT/specification/scripts
SRC_SCRIPTS=$ROOT/src/scripts
REGISTRY=$ROOT/specification/registry/xr.xml

# Call with an archive/repo name and the list of all files you want to include.
makeSubset() {
    set -e
    NAME=$1
    shift
    FN=$NAME.tar
    echo
    echo "[$FN] Creating from a subset of the Git repo..."
    rm -f "$ROOT/$FN" "$ROOT/$FN.gz"
    git archive --format tar HEAD "$@" > "$ROOT/$FN"

}

COMMON_FILES=".gitignore .gitattributes .git-blame-ignore-revs CODE_OF_CONDUCT.md LICENSES .reuse .editorconfig HOTFIX"
export COMMON_FILES
COMMON_EXCLUDE_PATTERN="(KhronosExperimental|KhronosConfidential)"
export COMMON_EXCLUDE_PATTERN

add_to_tar() {
    set -e
    TARNAME=$1
    shift
    echo "[$TARNAME.tar] Adding $*"
    tar --owner=0 --group=0 -rf "$ROOT/$TARNAME.tar" "$@"
}

# Generate a file using the scripts in specification/scripts/,
# and add it to the specified tarball.
generate_spec() {
    set -e
    DIR=$1
    FN=$2
    TAR=$3
    echo
    echo "Generating $DIR/$FN"
    WORKDIR=$(mktemp -d)
    if [ ! -d "${WORKDIR}" ]; then
        echo "Could not create temporary directory!"
        exit 1
    fi
    trap 'rm -rf $WORKDIR' EXIT
    (
        cd "$WORKDIR" || return
        mkdir -p "$DIR"
        PYTHONPATH=$SPEC_SCRIPTS python3 "$SPEC_SCRIPTS/genxr.py" -registry "$REGISTRY" -quiet -o "$DIR" "$FN"
        add_to_tar "$TAR" "$DIR/$FN"
        rm "$DIR/$FN"
    )
}

# Generate a file using the scripts in src/scripts/,
# and add it to the specified tarball.
generate_src() {
    set -e
    DIR=$1
    FN=$2
    echo
    echo "Generating $DIR/$FN"
    WORKDIR=$(mktemp -d)
    if [ ! -d "${WORKDIR}" ]; then
        echo "Could not create temporary directory!"
        exit 1
    fi
    trap 'rm -rf $WORKDIR' EXIT
    (
        cd "$WORKDIR" || exit 1
        mkdir -p "$DIR"
        PYTHONPATH=$SPEC_SCRIPTS:$SRC_SCRIPTS python3 "$SRC_SCRIPTS/src_genxr.py" -registry "$REGISTRY" -quiet -o "$DIR" "$FN"
        add_to_tar "$TAR" "$DIR/$FN"
        rm "$DIR/$FN"
    )
}


gzip_a_tar() {
    set -e
    echo "[$1.tar] Compressing to $1.tar.gz"
    gzip > "$ROOT/$1.tar.gz" < "$ROOT/$1.tar"
    rm "$ROOT/$1.tar"
}


getDocsFilenames() {
    set -e
    # Just the things required to build/maintain the spec text and registry.
    # Omitting the old version in submitted and its script
    # TODO omitting style guide VUID chapter for now
    git ls-files \
        $COMMON_FILES \
        .env \
        .proclamation.json \
        .proclamation.json.license \
        CHANGELOG.Docs.md \
        checkCodespell \
        format_file.sh \
        open-in-docker.sh \
        openxr-codespell.exclude \
        tox.ini \
        changes/README.md \
        changes/template.md \
        changes/.markdownlint.yaml \
        changes/specification \
        changes/registry \
        external/python \
        maintainer-scripts/check-changelog-fragments.sh \
        specification/sources/extprocess/ \
        include/ \
        specification/ \
        | grep -E -v "${COMMON_EXCLUDE_PATTERN}" \
        | grep -v "specification/loader" \
        | grep -v "vuid[.]adoc" \
        | grep -v "CMakeLists.txt" \
        | grep -v "ubmitted" \
        | grep -v "experimental" \
        | grep -v "xml\.reservations" \
        | grep -v "compare" \
        | grep -v "JP\.jpg"
}

getSDKSourceFilenames() {
    set -e
    # The src directory, plus the minimum subset of the specification directory
    # required to build those tools
    git ls-files \
        $COMMON_FILES \
        .appveyor.yml \
        .env \
        .proclamation.json \
        .proclamation.json.license \
        BUILDING.md \
        CHANGELOG.SDK.md \
        checkCodespell \
        CMakeLists.txt \
        format_file.sh \
        open-in-docker.sh \
        LICENSE \
        openxr-codespell.exclude \
        runClangFormat.sh \
        tox.ini \
        .appveyor.yml \
        .azure-pipelines/shared \
        .azure-pipelines/nuget \
        .azure-pipelines/openxr-sdk.yml \
        .azure-pipelines/openxr-sdk-source.yml \
        .github/dependabot.yml \
        .github/scripts \
        .github/.java-version* \
        .github/workflows/android.yml \
        .github/workflows/check_clang_format_and_codespell.yml \
        .github/workflows/gradle-wrapper-validation.yml \
        .github/workflows/macos-build.yml \
        .github/workflows/macOS.yml \
        .github/workflows/msvc-build-preset.yml \
        .github/workflows/pr.yml \
        .github/workflows/release.yml \
        .github/workflows/snapshot.yml \
        .github/workflows/windows-matrix.yml \
        changes/README.md \
        changes/template.md \
        changes/.markdownlint.yaml \
        changes/registry \
        changes/sdk \
        external/ \
        github/sdk/ \
        include/ \
        maintainer-scripts/common.sh \
        maintainer-scripts/archive-sdk.sh \
        maintainer-scripts/check-changelog-fragments.sh \
        maintainer-scripts/build-aar.sh \
        maintainer-scripts/build-and-publish-aar-mavencentral.sh \
        maintainer-scripts/build-and-publish-aar-snapshot.sh \
        maintainer-scripts/publish-aar \
        specification/.gitignore \
        specification/config/attribs.adoc \
        specification/registry/*.xml \
        specification/scripts \
        specification/loader \
        specification/Makefile \
        specification/README.md \
        specification/requirements.txt \
        src/.clang-format \
        src/.gitignore \
        src/CMakeLists.txt \
        src/api_layers \
        src/cmake \
        src/common \
        src/common_config.h.in \
        src/external/CMakeLists.txt \
        src/external/android-jni-wrappers \
        src/external/glad2 \
        src/external/jnipp \
        src/external/jsoncpp \
        src/external/catch2 \
        src/external/metal-cpp \
        src/loader \
        src/scripts \
        src/tests \
        src/version.cmake \
        src/version.gradle \
        | grep -E -v "${COMMON_EXCLUDE_PATTERN}" \
        | grep -v "conformance" \
        | grep -v "template_gen_dispatch" \
        | grep -v "function_info" \
        | grep -v "htmldiff" \
        | grep -v "katex"
}


getSDKFilenames() {
    set -e
    # Parts of the src directory, plus pre-generated files.
    git ls-files \
        $COMMON_FILES \
        CHANGELOG.SDK.md \
        CMakeLists.txt \
        LICENSE \
        .appveyor.yml \
        .azure-pipelines/shared \
        .azure-pipelines/nuget \
        .azure-pipelines/openxr-sdk.yml \
        .github/scripts \
        .github/.java-version* \
        .github/workflows/android.yml \
        .github/workflows/check_clang_format_and_codespell.yml \
        .github/workflows/msvc-build-preset.yml \
        .github/workflows/windows-matrix.yml \
        specification/registry/*.xml \
        include/ \
        src/.clang-format \
        src/CMakeLists.txt \
        src/cmake \
        src/common \
        src/common_config.h.in \
        src/external/CMakeLists.txt \
        src/external/android-jni-wrappers \
        src/external/jnipp \
        src/external/jsoncpp \
        src/loader \
        src/version.cmake \
        | grep -E -v "${COMMON_EXCLUDE_PATTERN}" \
        | grep -v "gfxwrapper" \
        | grep -v "include/.gitignore" \
        | grep -v "images"
}

getConformanceFilenames() {
    set -e
    # The src/conformance directory, plus the minimum subset of the other files required.
    git ls-files \
        $COMMON_FILES \
        .env \
        .proclamation.json \
        .proclamation.json.license \
        BUILDING.md \
        CHANGELOG.CTS.md \
        checkCodespell \
        CMakeLists.txt \
        format_file.sh \
        LICENSE \
        openxr-codespell.exclude \
        runClangFormat.sh \
        tox.ini \
        .github/dependabot.yml \
        .github/.java-version* \
        .github/scripts \
        .github/workflows/android-cts-build.yml \
        .github/workflows/android-cts-pr.yml \
        .github/workflows/android-cts-release.yml \
        .github/workflows/check_clang_format_and_codespell.yml \
        .github/workflows/gradle-wrapper-validation.yml \
        .github/workflows/msvc-build-preset.yml \
        .github/workflows/windows-cts-pr.yml \
        .github/workflows/windows-cts-release.yml \
        changes/README.md \
        changes/template.md \
        changes/.markdownlint.yaml \
        changes/registry \
        changes/conformance \
        external/ \
        github/conformance/ \
        include/ \
        maintainer-scripts/common.sh \
        maintainer-scripts/archive-conformance.sh \
        maintainer-scripts/check-changelog-fragments.sh \
        specification/.gitignore \
        specification/registry/*.xml \
        specification/config \
        specification/scripts \
        specification/Makefile \
        specification/README.md \
        specification/requirements.txt \
        src/.clang-format \
        src/.gitignore \
        src/CMakeLists.txt \
        src/cmake \
        src/common \
        src/common_config.h.in \
        src/conformance \
        src/external \
        src/loader \
        src/scripts \
        src/version.cmake \
        src/version.gradle \
        | grep -E -v "${COMMON_EXCLUDE_PATTERN}" \
        | grep -v "htmldiff" \
        | grep -v "katex"
}
