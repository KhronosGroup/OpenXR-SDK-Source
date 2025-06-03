#!/usr/bin/env python3
# Copyright (c) 2025 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0
"""
Update the internal-only extension reservation file from the XML.

Always review these changes by hand!
"""

import csv
from typing import Optional
from xml.etree import ElementTree
from pathlib import Path


def load_reservations(fn):
    reservations = dict()

    with open(fn, 'r', encoding='utf-8') as fp:
        reader = csv.reader(fp)
        for row in reader:
            if len(row) <= 2:
                continue
            try:
                num = int(row[0])
            except ValueError:
                continue
            reserved_for = row[2].strip()
            if not reserved_for:
                continue
            reservations[num] = reserved_for
    return reservations


def load_comment_lines(fn):
    lines: list[str] = []
    with open(fn, 'r', encoding='utf-8') as fp:
        for line in fp:
            clean_line = line.strip()
            if clean_line.startswith("#"):
                lines.append(clean_line)
                continue

            if lines and lines[-1] and not clean_line:
                # If we have lines, the last one is not blank,
                # but our current line is blank, append the blank line to the list
                lines.append(clean_line)
    return lines


def generate_data_lines(tree, reservations: Optional[dict[int, str]] = None):
    known: dict[int, str] = dict()
    if reservations:
        known = reservations
    lines: list[str] = []

    lines.append("ext_number,registered_name,reserved_for,released?")

    for ext in tree.iterfind('extensions/extension'):
        num = int(ext.get("number", "-1"))
        name = ext.get("name")
        supported = ext.get("supported")
        released = ",,released" if supported == "openxr" else ""
        if num in known and not released:
            lines.append(f"{num},{name},{known[num]}")
            continue

        lines.append(f"{num},{name}{released}")
    return lines


def update_reservations(registry_fn, reservation_fn):
    reservations = load_reservations(reservation_fn)
    comment_lines = load_comment_lines(reservation_fn)

    tree = ElementTree.parse(registry_fn)

    data_lines = generate_data_lines(tree, reservations)

    with open(reservation_fn, 'w', encoding='utf-8') as fp:
        fp.write("\n".join(comment_lines + data_lines))
        fp.write("\n")


if __name__ == "__main__":
    script_dir = Path(__file__).resolve().parent
    spec_dir = script_dir.parent.resolve()
    update_reservations(spec_dir / "registry" / "xr.xml", spec_dir / "registry" / "xr.xml.reservations")
