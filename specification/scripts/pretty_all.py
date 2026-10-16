#!/usr/bin/env python3
# Copyright (c) 2026 Collabora, Ltd.
#
# SPDX-License-Identifier: Apache-2.0
#
# Author(s):    Rylie Pavlik <rylie.pavlik@collabora.com>
#
# Purpose:      Find and run "PrettyRegistryXml.OpenXR" on all xml files.

import subprocess
from pathlib import Path


def main():
    spec_dir = Path(__file__).parent.parent.resolve()
    registry_dir = spec_dir / "registry"

    import argparse

    parser = argparse.ArgumentParser(description="Format multiple XML files.",
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("--tool", type=str,
                        default="PrettyRegistryXml.OpenXR",
                        help="The PrettyRegistryXml.OpenXR executable. Defaults to expecting in PATH.")
    parser.add_argument("directory", type=Path,
                        default=registry_dir,
                        nargs=argparse.OPTIONAL,
                        help="The directory to recursively scan for XML files to format."
                        " Optional."
                        " Defaults to the 'registry' sibling directory of the script-containing directory:"
                        f" {registry_dir}")

    args = parser.parse_args()

    process_root(tool=args.tool, root=args.directory)


def process_root(tool: str, root: Path):
    for xml in sorted(root.glob("**/*.xml")):
        process_file(tool, xml)


def process_file(tool: str, xml: Path):
    print(f"\n\n{xml}\n")

    args = [tool, str(xml)]
    if xml.name == "fail.command.enumorder.xml":
        # This one is testing that we catch return values in the wrong order
        # TODO uncomment once release includes this flag.
        # args.append("--no-sort-codes")
        return

    subprocess.check_call(args)


if __name__ == "__main__":
    main()
