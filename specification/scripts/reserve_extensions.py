#!/usr/bin/python3
#
# Copyright 2020-2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0
"""
Simple script to generate XML to reserve extensions.

The generated XML still needs to be added to xr.fragmented.xml and have
PrettyRegistryXML.OpenXR run on it after merging fragments into xr.xml.
"""

import click


@click.command()
@click.option('--extensions', default=1, help='number of extensions to reserve')
@click.argument('vendor')
@click.argument('first_extension', type=int)
def generate_reserved_extensions(vendor: str, first_extension: int, extensions: int):
    """Prints extension reservation XML."""
    vendor = vendor.upper()
    for num in range(first_extension, first_extension + extensions):
        print("    " + f"""
    <extension name="XR_{vendor}_extension_{num}" number="{num}" type="instance" stubs="true"/>
        """.strip())
        print()

if __name__ == "__main__":
    generate_reserved_extensions()
