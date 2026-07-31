#!/bin/sh
# Copyright 2026, The Khronos Group Inc.
# SPDX-License-Identifier: BSL-1.0
# Author: Kenny Vercaemer <vercaemer@google.com>

# Merges all fragments into xr.xml.

set -e


SPEC_ROOT=$(cd "$(dirname "$0")" && cd .. && pwd)

PRETTY_XML=PrettyRegistryXml.OpenXR
REGISTRY=$SPEC_ROOT/registry/xr.xml
REGISTRY_FRAGMENTED=$SPEC_ROOT/registry/xr.fragmented.xml
FRAGMENT_DIR=$SPEC_ROOT/registry/fragments

echo "Merging"
python3 "$SPEC_ROOT/scripts/xml_merger.py" \
  --fragment-dir "$FRAGMENT_DIR" \
  --input  "$REGISTRY_FRAGMENTED" \
  --output "$REGISTRY"

if command -v $PRETTY_XML > /dev/null; then
    echo "Performing auto pretty of merged registry"
    $PRETTY_XML "$REGISTRY"
else
    echo "Warning: $PRETTY_XML not found!"
fi
