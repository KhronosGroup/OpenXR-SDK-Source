#!/usr/bin/python3
#
# Copyright (c) 2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

import tempfile
from pathlib import Path

import pytest
from lxml import etree as ET

from xml_merger import _build_stub_extension, merge_registry


def _minimal_fragmented_xml(extension_line: str) -> str:
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE xml>
<registry>
    <extensions>
{extension_line}
    </extensions>
</registry>
"""


def test_stub_placeholder_expands_to_reserved_extension():
    xml = _minimal_fragmented_xml(
        '        <extension name="XR_NV_extension_524" number="524" type="instance" stubs="true"/>'
    )
    with tempfile.TemporaryDirectory() as tmpdir:
        fragmented = Path(tmpdir) / "xr.fragmented.xml"
        fragmented.write_text(xml, encoding="utf-8")
        merged = merge_registry(str(fragmented), [])

    extension = merged.getroot().find("./extensions/extension")
    assert extension is not None
    assert extension.get("name") == "XR_NV_extension_524"
    assert extension.get("number") == "524"
    assert extension.get("type") == "instance"
    assert extension.get("supported") == "disabled"
    assert extension.get("stubs") is None

    require = extension.find("require")
    assert require is not None
    enums = require.findall("enum")
    assert len(enums) == 2
    assert enums[0].get("name") == "XR_NV_extension_524_SPEC_VERSION"
    assert enums[0].get("value") == "1"
    assert enums[1].get("name") == "XR_NV_EXTENSION_524_EXTENSION_NAME"
    assert enums[1].get("value") == '"XR_NV_extension_524"'


def test_stub_placeholder_rejects_children():
    extension = ET.fromstring(
        '<extension name="XR_NV_extension_524" number="524" type="instance" stubs="true">'
        '<require><enum value="1" name="XR_NV_extension_524_SPEC_VERSION"/></require>'
        "</extension>"
    )
    with pytest.raises(ValueError, match="must not have child elements"):
        _build_stub_extension(extension)


def test_stub_placeholder_rejects_fragment_attribute():
    extension = ET.fromstring(
        '<extension name="XR_NV_extension_524" number="524" type="instance" '
        'stubs="true" fragment="true"/>'
    )
    with pytest.raises(ValueError, match="cannot have both stubs='true' and fragment='true'"):
        _build_stub_extension(extension)


def test_fragment_placeholder_still_requires_matching_fragment_file():
    fragmented_xml = """<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE xml>
<registry>
    <extensions>
        <extension name="XR_TEST_extension_1" number="1" type="instance" fragment="true"/>
    </extensions>
</registry>
"""
    fragment_xml = """<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE xml>
<fragment>
    <extensions>
        <extension name="XR_TEST_extension_1" number="1" type="instance" supported="disabled">
            <require>
                <enum value="1" name="XR_TEST_extension_1_SPEC_VERSION"/>
                <enum value="&quot;XR_TEST_extension_1&quot;" name="XR_TEST_EXTENSION_1_EXTENSION_NAME"/>
            </require>
        </extension>
    </extensions>
</fragment>
"""
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_path = Path(tmpdir)
        fragmented = tmp_path / "xr.fragmented.xml"
        fragmented.write_text(fragmented_xml, encoding="utf-8")
        fragment_dir = tmp_path / "fragments"
        fragment_dir.mkdir()
        fragment_file = fragment_dir / "XR_TEST_extension_1.xml"
        fragment_file.write_text(fragment_xml, encoding="utf-8")

        merged = merge_registry(str(fragmented), [fragment_file])

    extension = merged.getroot().find("./extensions/extension")
    assert extension is not None
    assert extension.get("fragment") is None
    assert extension.find("require") is not None
