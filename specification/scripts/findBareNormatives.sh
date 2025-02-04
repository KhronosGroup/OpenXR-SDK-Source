#!/usr/bin/env bash
#
# Copyright (c) 2018-2025 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

# findBareNormatives.sh - search usage of words that are or seem like normatives,
# but lack normative markup/highlighting.
#
# Usage: scripts/findBareNormatives.sh [sources/chapters/...]...
#
# If one or more adoc file paths (relative to specification dir) are provided,
# only those paths are checked; otherwise all chapters are checked.

set -e

list_sources() {
    (
        cd "$(dirname "$0")/.."
        find sources/chapters -name "*.adoc" | grep -v appendix
    )
}

SOURCES=()
if [ $# -gt 0 ]; then
    echo "Only checking $*"
    echo ""
    SOURCES+=("$@")
else
    while IFS='' read -r line; do SOURCES+=("$line"); done < <(list_sources)
fi

# Base function for grepping through sources for errors
# Params:
# 1. Description of issue being checked for
# All remaining parameters: passed to grep, typically a regex or something like -e regex -e regex
# May set GREPCMD before call to do something other than egrep
grep_sources() {
    DESC="$1"
    shift
    if ${GREPCMD:-grep -P} -n "$@" "${SOURCES[@]}"; then
        echo ""
        echo "*** Warning: Above lines matched grep with  $*"
        echo "*** $DESC"
        echo ""
        echo ""
        FAILED="yes"
    fi
}


# Common Params for the various check markup functions:
# 1. macro first letter (so s in slink)
# 2. description of category
# 3. expectation, completing the sentence "Things like this should..."
# 4. partial regex, which will be appended to the end of markup macro (eg slink:yourregex)

###
# Actual Checks
###
(
    export FAILED=

    cd "$(dirname "$0")/.."
    grep_sources \
        "found normative without trailing colon - add colon or rephrase" \
        "\b(cannot|can|may|must|optional|should)\b(?!:)"

    # not looking for "required" because there are too many places we don't want the markup.

    grep_sources \
        "these suggest a normative, but are not normatives used in the OpenXR spec - rephrase" \
        "\b(shall|might|could|would)\b(?!:)"


    if [ "$FAILED" != "" ]; then
        echo "Failures detected"
        exit 1
    fi

)

