#!/usr/bin/python3
#
# Copyright (c) 2026 The Khronos Group Inc.
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
"""Compares a generated registry and a checked-in registry."""

import argparse
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from lxml import etree as ET
from xml_merger import merge_registry

_RE_TRAILING_WHITESPACE = re.compile(r"\n[ ]+", re.MULTILINE)


def _drop_trailing_whitespace_in_tail(tree: ET.ElementTree):
    for elem in tree.iter():
        if elem.tail:
            elem.tail = _RE_TRAILING_WHITESPACE.sub("\n", elem.tail)


if __name__ == "__main__":

    parser = argparse.ArgumentParser(description="Compares a generated registry and a checked-in registry.")
    parser.add_argument("--fragment-dir", required=True, type=Path,
                        help="Directory where XML fragments are located.")
    parser.add_argument("--input", required=True, type=Path,
                        help="Input file path for the main xr.fragmented.xml.")
    parser.add_argument("--checked-in", required=True, type=Path,
                        help="Input file path for the checked-in registry (i.e. pre-merged).")

    args = parser.parse_args()

    merged_tree = merge_registry(str(args.input), sorted(args.fragment_dir.rglob("*.xml")))
    _drop_trailing_whitespace_in_tail(merged_tree)

    xml_parser = ET.XMLParser(remove_comments=False, remove_blank_text=False, resolve_entities=False)
    checked_in_tree = ET.parse(str(args.checked_in), xml_parser)
    _drop_trailing_whitespace_in_tail(checked_in_tree)

    with tempfile.NamedTemporaryFile(mode="wb", delete=False, suffix=".xml") as temp_merged, \
            tempfile.NamedTemporaryFile(mode="wb", delete=False, suffix=".xml") as temp_checked_in:
        temp_merged_path = temp_merged.name
        temp_checked_in_path = temp_checked_in.name

        temp_merged.write(ET.tostring(merged_tree, pretty_print=True, encoding="utf-8"))
        temp_checked_in.write(ET.tostring(checked_in_tree, pretty_print=True, encoding="utf-8"))

    try:
        result = subprocess.run(["diff", "-u", temp_merged_path, temp_checked_in_path], capture_output=True, text=True)
        if result.returncode != 0:
            print(f"ERROR: {args.input} and {args.checked_in} are not semantically equivalent.", file=sys.stderr)
            print("Differences found:\n", file=sys.stderr)
            print(result.stdout, file=sys.stderr)
            sys.exit(1)
    finally:
        os.remove(temp_merged_path)
        os.remove(temp_checked_in_path)
