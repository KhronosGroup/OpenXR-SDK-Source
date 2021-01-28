#!/usr/bin/python3
#
# Copyright (c) 2018-2021, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

# Used to update the version in the appropriate place.  Uses the
# 'current_version.ini' file in the the openxr/specification folder.
# To execute, run this script from the specification folder with a clean
# tree.

# Usage: python3 ./scripts/update_version.py

import fileinput
import configparser

if __name__ == "__main__":

    # Get the current version from the 'current_version.ini' file.
    with open("current_version.ini", "r") as fp:
        config = configparser.ConfigParser()
        config.read_file(fp)
        versions = config['Version']
        major_version = versions['MAJOR']
        minor_version = versions['MINOR']
        spec_version = (versions['MAJOR'], versions['MINOR'], versions['PATCH'])

    # Now update the version in the appropriate places in the
    # registry file (registry/xr.xml).
    #
    print('Replacing version lines in the registry')
    for line in fileinput.input('registry/xr.xml', inplace=True):
        printed = False
        if 'XR_CURRENT_API_VERSION' in line:
            if 'XR_MAKE_VERSION' in line:
                printed = True
                print('#define <name>XR_CURRENT_API_VERSION</name> <type>XR_MAKE_VERSION</type>(%s, %s, %s)</type>' % spec_version)
            if 'type name' in line:
                printed = True
                print('            <type name="XR_CURRENT_API_VERSION"/>')
        elif 'XR_VERSION_' in line and 'feature' in line and 'number' in line and 'openxr' in line:
            printed = True
            print('    <feature api="openxr" name="XR_VERSION_%s_%s" number="%s.%s">' % (major_version, minor_version, major_version, minor_version))
        if not printed:
            print("%s" % (line), end='')

    # Now update the version in the appropriate places in the
    # specification make file (Makefile).
    #
    print('Replacing version lines in the specification Makefile')
    for line in fileinput.input('Makefile', inplace=True):
        printed = False
        if 'SPECREVISION = ' in line:
            printed = True
            print('SPECREVISION = %s.%s.%s' % spec_version)
        if not printed:
            print("%s" % (line), end='')
