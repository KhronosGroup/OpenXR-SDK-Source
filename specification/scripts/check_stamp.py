#!/usr/bin/env python3
# Copyright (c) 2019-2025 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0
#
# Author: Rylie Pavlik <rylie.pavlik@collabora.com>
"""Pass a filename and any arguments. Will use those arguments to create a stamp file to monitor for changes."""
import sys
from pathlib import Path


def check_stamp(fn, args):
    new_contents = ",".join(str(x) for x in args).strip()
    write_stamp = True
    p = Path(fn)
    if p.exists():
        with open(fn, 'r', encoding='utf-8') as fp:
            stamp_contents = fp.read().strip()
            if stamp_contents == new_contents:
                write_stamp = False
            else:
                print("Build configuration options have changed - forcing clean_generated")

    if write_stamp:
        with open(fn, 'w', encoding='utf-8') as fp:
            fp.write(new_contents)
            fp.flush()
    return write_stamp


if __name__ == "__main__":
    stamp = sys.argv[1]
    data = sys.argv[2:]
    # print("Stamp name:", stamp)
    # print("Data:", data)
    if check_stamp(sys.argv[1], sys.argv[2:]):
        sys.exit(1)
