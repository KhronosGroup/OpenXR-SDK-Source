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
"""Handle registry XML fragments and merging into xr.xml."""

import argparse
from collections.abc import Iterable
from pathlib import Path
from typing import List, Optional

from lxml import etree as ET


def _merge_fragment(tree: ET.ElementTree, fragment_tree: ET.ElementTree, fragment_filepath: Path) -> None:
    """Merge a fragment tree into the main tree."""
    main_root: ET.Element = tree.getroot()
    fragment_root: ET.Element = fragment_tree.getroot()

    fragment_name = fragment_filepath.stem
    comment_text = f" {fragment_name} "

    # types, commands, interaction profiles, are simply appended at the end of their block
    additions = [
        "./types",
        "./commands",
        "./interaction_profiles",
    ]
    for anchor_xpath in additions:

        elements_to_add = []
        for block in fragment_root.findall(anchor_xpath):
            elements_to_add.extend(block[:])

        if elements_to_add:
            anchor = main_root.findall(anchor_xpath)[-1]

            comment = ET.Comment(comment_text)
            comment.tail = "\n        "
            anchor.append(comment)

            elements_to_add[-1].tail = "\n\n        "
            anchor.extend(elements_to_add)

    # enums don't have a parent element, so just append after the last enum in the root
    enums_to_add: List[ET.Element] = fragment_root.findall("./enums")
    if enums_to_add:
        _merge_fragment_enums(main_root, comment_text, enums_to_add)

    # extensions are merged in to their numbered element
    extensions_anchor: Optional[ET.Element] = main_root.find("extensions")
    if extensions_anchor is None:
        msg = "Could not find extensions tag"
        raise ValueError(msg)

    for fragment_extension in fragment_root.findall("./extensions/extension"):
        _merge_fragment_extension(extensions_anchor, fragment_extension)


def _merge_fragment_extension(extensions_anchor: ET.Element,
                              fragment_extension: ET.Element) -> None:
    ext_name = fragment_extension.get("name")
    ext_num = fragment_extension.get("number")
    # extensions_anchor is the <extensions> tag in the main tree.
    # fragment_extension is the <extension> tag from the fragment,
    # which will replace one that it matches in the main tree.
    for i, existing_ext in enumerate(extensions_anchor):
        if existing_ext.get("name") != ext_name:
            continue
        if existing_ext.get("fragment") != "true":
            continue
        existing_num = existing_ext.get("number")
        if existing_num != ext_num:
            msg = (f"Extension number mismatch: main file says {existing_num}, "
                   f"fragment says {ext_num}")
            raise ValueError(msg)
        tail = existing_ext.tail
        extensions_anchor[i] = fragment_extension
        extensions_anchor[i].tail = tail
        return

    msg = (f"Extension '{ext_name}' (num {ext_num}) placeholder with "
           "fragment='true' not found in input .fragmented.xml.")
    raise ValueError(msg)


def _merge_fragment_enums(main_root: ET.Element,
                          comment_text: str,
                          enums_to_add: List[ET.Element]) -> None:
    last_enum = main_root.findall("enums")[-1]
    insert_idx = list(main_root).index(last_enum) + 1

    for idx, enum in enumerate(enums_to_add, insert_idx):
        main_root.insert(idx, enum)
        enum.tail = "\n    "

    # Extra newline after last enum
    enums_to_add[-1].tail = "\n\n    "

    # Insert the comment before the enums
    comment = ET.Comment(comment_text)
    comment.tail = "\n    "
    main_root.insert(insert_idx, comment)


def merge_registry(input_registry_path: str, fragment_files: Iterable[Path]) -> ET.ElementTree:
    """Merge all fragment files into an input registry file and return a merged tree."""
    xml_parser = ET.XMLParser(remove_comments=False, remove_blank_text=False, resolve_entities=False)
    tree = ET.parse(input_registry_path, xml_parser)

    input_path = Path(input_registry_path).resolve()
    registry_dir = input_path.parent.resolve()

    comment_text = [
        "",
        f"      DO NOT EDIT DIRECTLY - generated from {input_path.name} and the following fragments:",
        "",
    ]

    for filepath in fragment_files:
        with filepath.open("r", encoding="utf-8") as f:
            fragment_tree = ET.parse(f, xml_parser)
            _merge_fragment(tree, fragment_tree, filepath)
        relative_path = filepath.resolve().relative_to(registry_dir)
        comment_text.append(f"      - {relative_path}")

    comment_text.append("")

    # Insert the comment about being generated
    comment = ET.Comment("\n".join(comment_text))
    comment.tail = "\n"  # newline after comment
    tree.getroot().insert(0, comment)
    return tree


def main() -> None:
    parser = argparse.ArgumentParser(description="Merge XML fragments into xr.xml.")
    parser.add_argument("--fragment-dir", required=True, type=Path,
                        help="Directory where XML fragments are located.")
    parser.add_argument("--input", required=True, type=Path,
                        help="Input file path for the main xr.fragmented.xml.")
    parser.add_argument("--output", required=True, type=Path,
                        help="Output file path for merged xr.xml.")
    args = parser.parse_args()

    merged_tree = merge_registry(str(args.input), sorted(args.fragment_dir.rglob("*.xml")))
    args.output.parent.mkdir(parents=True, exist_ok=True)

    # lxml handles the doc level comment serialization in a funny way, so do that manually.
    root = merged_tree.getroot()

    doctype = merged_tree.docinfo.doctype

    pis_before = []
    curr = root.getprevious()
    while curr is not None:
        node_type = type(curr).__name__
        if node_type in ("_ProcessingInstruction", "_Comment"):
            pis_before.append(curr)
        curr = curr.getprevious()
    pis_before.reverse()

    with args.output.open("wb") as f:
        f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n')

        if doctype:
            f.write(doctype.encode("utf-8") + b"\n")

        for pi in pis_before:
            f.write(ET.tostring(pi, encoding="utf-8") + b"\n")

        f.write(ET.tostring(root, encoding="utf-8"))

        f.write(b"\n")


if __name__ == "__main__":
    main()
