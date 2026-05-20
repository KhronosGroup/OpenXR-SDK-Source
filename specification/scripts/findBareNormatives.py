#!/usr/bin/env python3
#
# Copyright (c) Meta Platforms, LLC and its affiliates. All rights reserved.
# Copyright (c) 2018-2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

# findBareNormatives.py - search usage of words that are or seem like normatives,
# but lack normative markup/highlighting.
#
# Usage: scripts/findBareNormatives.py [sources/chapters/...]...
#
# If one or more adoc file paths (relative to specification dir) are provided,
# only those paths are checked; otherwise all chapters are checked.

import os
import re
import sys
from pathlib import Path
from typing import Callable, List, Optional, Sequence, Tuple

BARE_NORMATIVE_CHECKS: List[Tuple[str, str]] = [
    (
        r"\b(cannot|can|may|must|should)\b(?!:)",
        "found normative without trailing colon - add colon or rephrase",
    ),
    (
        r"\bshall\b(?!:)",
        "'shall' suggests a normative, but is not a normative used in the OpenXR spec - rephrase",
    ),
]

INVALID_NORMATIVE_CHECKS: List[Tuple[str, str]] = [
    (
        r"\bwill:",
        "'will:' is not a valid OpenXR normative - rephrase using must:/should:/may:/can:/cannot:",
    ),
]


def load_allowlist(path: str) -> dict:
    entries: dict = {}
    try:
        with open(path, encoding="utf-8") as f:
            for line in f:
                stripped = line.rstrip("\n")
                if not stripped:
                    continue
                parts = stripped.split("|||", 1)
                if len(parts) == 2:
                    filename, line_content = parts
                    entries.setdefault(filename, set()).add(line_content)
    except FileNotFoundError:
        pass
    return entries


def grep_sources(
    description: str,
    pattern: str,
    sources: Sequence[str],
    output: Optional[Callable[[str], None]] = None,
    allowlist: Optional[dict] = None,
) -> bool:
    if output is None:
        output = lambda msg: print(msg)  # noqa: E731
    regex = re.compile(pattern)
    matches = []
    for source in sources:
        basename = Path(source).name
        allowed_lines = (
            allowlist.get(basename, set()) if allowlist is not None else None
        )
        try:
            with open(source, encoding="utf-8") as f:
                for lineno, line in enumerate(f, start=1):
                    if regex.search(line):
                        stripped = line.rstrip()
                        if allowed_lines is not None and stripped in allowed_lines:
                            continue
                        matches.append(f"{source}:{lineno}:{stripped}")
        except FileNotFoundError:
            output(f"Warning: file not found: {source}")
    if matches:
        for m in matches:
            output(m)
        output("")
        output(f"*** Warning: Above lines matched pattern  {pattern}")
        output(f"*** {description}")
        output("")
        output("")
    return bool(matches)


def check_sources(
    sources: Sequence[str],
    checks: Optional[List[Tuple[str, str]]] = None,
    output: Optional[Callable[[str], None]] = None,
    allowlist: Optional[dict] = None,
) -> bool:
    if checks is None:
        checks = BARE_NORMATIVE_CHECKS
    failed = False
    for pattern, description in checks:
        failed |= grep_sources(
            description, pattern, sources, output=output, allowlist=allowlist
        )
    return failed


def _list_default_sources() -> List[str]:
    script_dir = Path(__file__).resolve().parent
    spec_dir = script_dir.parent
    chapters_dir = spec_dir / "sources" / "chapters"
    return sorted(
        str(p.relative_to(spec_dir))
        for p in chapters_dir.rglob("*.adoc")
        if "appendix" not in str(p)
    )


def main() -> None:
    script_dir = Path(__file__).resolve().parent
    spec_dir = script_dir.parent
    os.chdir(spec_dir)

    allowlist_path = script_dir / "bare_normatives_allowlist.txt"
    allowlist = load_allowlist(str(allowlist_path))

    if len(sys.argv) > 1:
        sources = sys.argv[1:]
        print(f"Only checking {' '.join(sources)}")
        print()
    else:
        sources = _list_default_sources()

    failed = check_sources(sources, BARE_NORMATIVE_CHECKS, allowlist=allowlist)
    failed |= check_sources(sources, INVALID_NORMATIVE_CHECKS, allowlist=allowlist)

    if failed:
        print("Failures detected")
        sys.exit(1)


if __name__ == "__main__":
    main()
