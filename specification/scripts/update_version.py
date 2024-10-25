#!/usr/bin/python3
#
# Copyright (c) 2018-2024, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

# Used to update the version in the appropriate place.  Uses the
# 'current_version.ini' file in the the openxr/specification folder.
# To execute, run this script from the specification folder with a clean
# tree.

# Usage: python3 ./scripts/update_version.py

import fileinput
import configparser


def update_makefile(fn, spec_version):
    for line in fileinput.input(fn, inplace=True):
        if 'SPECREVISION = ' in line:
            print('SPECREVISION = %s.%s.%s' % spec_version)
        else:
            print(line, end='')


if __name__ == "__main__":

    # Get the current version from the 'current_version.ini' file.
    with open("current_version.ini", "r") as fp:
        config = configparser.ConfigParser()
        config.read_file(fp)
        versions = config['Version']
        spec_version = (versions['MAJOR'], versions['MINOR'], versions['PATCH'])

    # Now update the version in the appropriate places in the
    # registry file (registry/xr.xml).
    #
    print('Replacing version lines in the registry')
    for line in fileinput.input('registry/xr.xml', inplace=True):
        if '<name>XR_CURRENT_API_VERSION</name>' in line and 'XR_MAKE_VERSION' in line:
            print('#define <name>XR_CURRENT_API_VERSION</name> <type>XR_MAKE_VERSION</type>(%s, %s, %s)</type>' % spec_version)
        else:
            print(line, end='')

    # Now update the version in the appropriate places in the
    # specification make file (Makefile).
    #
    print('Replacing version lines in the specification Makefile')
    update_makefile('Makefile', spec_version)

    print('Replacing version lines in the CTS Usage Makefile')
    update_makefile('../src/conformance/usage/Makefile', spec_version)
