#!/usr/bin/env bash
# Copyright 2020, Collabora, Ltd.
# SPDX-License-Identifier: Apache-2.0

set -e

if [ $# -eq 0 ]; then
    TOPIC=HEAD
    MASTER=origin/master
elif [ $# -eq 1 ]; then
    TOPIC=$1
    MASTER=origin/master
elif [ $# -eq 2 ]; then
    TOPIC=$1
    MASTER=$2
else
    echo "$0 [<topic_branch> [<master_branch>]]" >&2
    exit 1
fi

BASE=$(git merge-base "${MASTER}" "${TOPIC}")
export TOPIC
export BASE

echo "Topic branch: $TOPIC"
echo "Compared against target branch: $MASTER"
echo "Merge base: $BASE"
echo ""
echo ""

changes_in_tree() {
    if ! git diff --exit-code "${BASE}..${TOPIC}" -- "$@" > /dev/null; then
        true
    else
        false
    fi
}

check_tree_and_changelog() {
    changelog_dir=$1
    shift
    if changes_in_tree "$@"; then
        if changes_in_tree $changelog_dir; then
            echo "OK: Found changes in $@ and changelog fragment in $changelog_dir"
        else
            echo "Error: Found changes in $@ but no changelog fragment in $changelog_dir"
            RESULT=false
            export RESULT
        fi
    fi
}

(
    cd "$(dirname $0)/.."
    RESULT=true
    check_tree_and_changelog changes/sdk src/api_layers src/cmake src/common src/loader src/scripts src/tests specification/loader
    check_tree_and_changelog changes/specification specification/sources
    check_tree_and_changelog changes/registry specification/registry

    if [ "x$CI_MERGE_REQUEST_ID" != "x" ]; then
        # This is a CI build of a merge request.
        echo "Merge request $CI_MERGE_REQUEST_IID"
        if ! find changes -name "mr.$CI_MERGE_REQUEST_IID.gl.md" | grep -q "."; then
            echo "Warning: Could not find a changelog fragment named mr.$CI_MERGE_REQUEST_IID.gl.md"
            # RESULT=false
        fi
    fi
    $RESULT
)
