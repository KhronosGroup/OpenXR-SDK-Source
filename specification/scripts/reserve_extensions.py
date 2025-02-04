#!/usr/bin/python3
#
# Copyright 2020-2025 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0
"""
Simple script to generate XML to reserve extensions.

The generated XML still needs to be added to xr.xml and have
PrettyRegistryXML.OpenXR run on it.
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
    <extension name="XR_{vendor}_extension_{num}" number="{num}" type="instance" supported="disabled">
        <require>
            <enum value="1"                                 name="XR_{vendor}_extension_{num}_SPEC_VERSION"/>
            <enum value="&quot;XR_{vendor}_extension_{num}&quot;" name="XR_{vendor}_EXTENSION_{num}_EXTENSION_NAME"/>
        </require>
    </extension>
        """.strip())
        print()

if __name__ == "__main__":
    generate_reserved_extensions()
