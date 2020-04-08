#!/usr/bin/env python3
# Copyright (c) 2019 The Khronos Group Inc.

from itertools import product
from pathlib import Path
import sys

from shared import PLATFORMS, TRUE_FALSE, VS_VERSION, make_win_artifact_name

CWD = Path.cwd()


def move(src, dest):

    print(str(src), '->', str(dest))
    src.replace(dest)


if __name__ == "__main__":

    configs = {}
    workspace = Path(sys.argv[1])
    outbase = Path(sys.argv[2])


    include_copied = False

    for platform, uwp in product(PLATFORMS, TRUE_FALSE):
        base = outbase / '{}{}'.format(platform,
                                              '_uwp' if uwp else '')
        base.mkdir(parents=True, exist_ok=True)
        name = make_win_artifact_name(platform, uwp)

        artifact = workspace / name

        if not include_copied:
            # Move over one set of includes to the base
            move(artifact / 'include', outbase / 'include')
            include_copied = True            

        # lib files
        move(artifact / 'lib', base / 'lib')

        # dll files
        move(artifact / 'bin', base / 'bin')
