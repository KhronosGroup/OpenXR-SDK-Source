#!/usr/bin/env python3 -i
#
# Copyright 2018-2025 The Khronos Group Inc.
# Copyright (c) 2018-2019 Collabora, Ltd.
#
# SPDX-License-Identifier: Apache-2.0
#
# Author(s):    Rylie Pavlik <rylie.pavlik@collabora.com>


import pytest

from check_spec_links import XREntityDatabase


@pytest.fixture
def db():
    ret = XREntityDatabase()
    # print(ret.getEntityJson())
    return ret


def test_likely_recognized(db):
    assert db.likelyRecognizedEntity('xrBla')
    assert db.likelyRecognizedEntity('XrBla')
    assert db.likelyRecognizedEntity('XR_BLA')


def test_db(db):
    assert db.findEntity('xrCreateInstance')
    assert db.findEntity('XRAPI_CALL')
