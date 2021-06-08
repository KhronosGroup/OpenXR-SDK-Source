#!/usr/bin/env python3
# Copyright 2021, Collabora, Ltd.
# SPDX-License-Identifier: Apache-2.0


# import xml.etree.ElementTree as ET
from lxml import etree as ET
from pathlib import Path
from typing import List, Tuple


_REG_FILE = Path(__file__).resolve().parent.parent / "registry" / "xr.xml"


def get_codes(cmd: ET.ElementBase, attr_name: str) -> List[str]:
    # the list comprehension here drops empty elements
    return [x for x in cmd.get(attr_name, "").split(",") if x]


# These codes will be placed first in the list, in this order, if found.
# Remaining codes will be alphabetical
popular_presort = [
    "XR_SUCCESS",
    "XR_SESSION_LOSS_PENDING",
    "XR_ERROR_FUNCTION_UNSUPPORTED",
    "XR_ERROR_VALIDATION_FAILURE",
    "XR_ERROR_RUNTIME_FAILURE",
    "XR_ERROR_HANDLE_INVALID",
    "XR_ERROR_INSTANCE_LOST",
    "XR_ERROR_SESSION_LOST",
    "XR_ERROR_OUT_OF_MEMORY",
    "XR_ERROR_LIMIT_REACHED",
    "XR_ERROR_SIZE_INSUFFICIENT",
]
popular = {code: val for val, code in enumerate(reversed(popular_presort), start=100)}


def presort_key(code: str) -> Tuple[int, str]:
    return popular.get(code, 1), code


def sort_attribute(cmd: ET.ElementBase, attr_name: str):
    codes = get_codes(cmd, attr_name)
    if not codes:
        # Probably an alias or something.
        return
    cmd.set(attr_name, ",".join(sorted(codes, reverse=True, key=presort_key)))


def main():

    xml = ET.parse(str(_REG_FILE), parser=None)
    cmds = xml.findall('commands/command')

    for cmd in cmds:
        sort_attribute(cmd, "successcodes")
        sort_attribute(cmd, "errorcodes")

    xml.write(str(_REG_FILE), encoding="utf-8", xml_declaration=True)


if __name__ == "__main__":
    main()
