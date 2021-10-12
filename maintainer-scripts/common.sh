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

COMMON_FILES=".gitignore .gitattributes .git-blame-ignore-revs CODE_OF_CONDUCT.md LICENSES .reuse .editorconfig"
export COMMON_FILES
COMMON_EXCLUDE_PATTERN="KhronosExperimental"
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
        | grep -v "${COMMON_EXCLUDE_PATTERN}" \
        | grep -v "specification/loader" \
        | grep -v "vuid[.]adoc" \
        | grep -v "CMakeLists.txt" \
        | grep -v "ubmitted" \
        | grep -v "experimental" \
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
        .proclamation.json \
        .proclamation.json.license \
        BUILDING.md \
        CHANGELOG.SDK.md \
        checkCodespell \
        CMakeLists.txt \
        format_file.sh \
        LICENSE \
        openxr-codespell.exclude \
        runClangFormat.sh \
        tox.ini \
        .azure-pipelines/shared \
        .azure-pipelines/nuget \
        .azure-pipelines/openxr-sdk.yml \
        .azure-pipelines/openxr-sdk-source.yml \
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
        specification/registry/*.xml \
        specification/scripts \
        specification/loader \
        specification/Makefile \
        specification/README.md \
        specification/requirements.txt \
        src/ \
        | grep -v "${COMMON_EXCLUDE_PATTERN}" \
        | grep -v "conformance" \
        | grep -v "template_gen_dispatch" \
        | grep -v "catch2" \
        | grep -v "function_info" \
        | grep -v "stb" \
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
        .azure-pipelines/shared \
        .azure-pipelines/nuget \
        .azure-pipelines/openxr-sdk.yml \
        specification/registry/*.xml \
        include/ \
        src/CMakeLists.txt \
        src/version.cmake \
        src/common_config.h.in \
        src/common \
        src/cmake \
        src/loader \
        src/external/CMakeLists.txt \
        src/external/jsoncpp \
        src/external/jnipp \
        src/external/android-jni-wrappers \
        src/.clang-format \
        | grep -v "${COMMON_EXCLUDE_PATTERN}" \
        | grep -v "gfxwrapper" \
        | grep -v "include/.gitignore" \
        | grep -v "images"
}

getConformanceFilenames() {
    set -e
    # The src/conformance directory, plus the minimum subset of the other files required.
    # TODO need a mention in the spec folder about how it's not under the same license (?)
    git ls-files \
        $COMMON_FILES \
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
        .azure-pipelines/shared \
        .azure-pipelines/openxr-cts.yml \
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
        specification/scripts \
        specification/Makefile \
        specification/README.md \
        specification/requirements.txt \
        src/cmake \
        src/common \
        src/conformance \
        src/external \
        src/loader \
        src/scripts \
        src/.clang-format \
        src/.gitignore \
        src/CMakeLists.txt \
        src/common_config.h.in \
        src/version.cmake \
        | grep -v "${COMMON_EXCLUDE_PATTERN}" \
        | grep -v "htmldiff" \
        | grep -v "katex"
}
