#!/usr/bin/python3
#
# Copyright (c) 2018-2019 Collabora, Ltd.
#
# SPDX-License-Identifier: Apache-2.0
#
# Author(s):    Ryan Pavlik <ryan.pavlik@collabora.com>
#
# Purpose:      This file contains tests for check_spec_links.py
#               that depend on the API being used.

import pytest
from check_spec_links import MessageId
from test_check_spec_links import CheckerWrapper, allMessages


@pytest.fixture
def ckr(capsys):
    """Fixture - add an arg named ckr to your test function to automatically get one passed to you."""
    return CheckerWrapper(capsys)


def test_openxr_refpage_mismatch(ckr):
    """Test the REFPAGE_MISMATCH message."""
    ckr.enabled([MessageId.REFPAGE_MISMATCH])
    # Should error: we undid the special case of the include for FlagBits inside of refpage for Flags
    assert ckr.check(
        """[open,refpage='XrSwapchainCreateFlags']
        --
        include::{generated}/api/enums/XrSwapchainCreateFlagBits.txt[]""").messages

    # Should not error, this is how we expect it
    assert not ckr.check(
        """[open,refpage='XrSwapchainCreateFlagBits']
        --
        include::{generated}/api/enums/XrSwapchainCreateFlagBits.txt[]""").messages

def test_openxr_refpage_type(ckr):
    """Test the REFPAGE_TYPE message, for fallout following the flags fix"""
    ckr.enabled([MessageId.REFPAGE_TYPE])
    # Should not error, this is how we expect it
    assert ckr.check(
        """[open,type='enums',refpage='XrEnvironmentBlendMode']
        --
        include::{generated}/api/enums/XrEnvironmentBlendMode.txt[]""").numDiagnostics() == 0


def test_openxr_refpage_missing(ckr):
    """OpenXR-specific tests of the REFPAGE_MISSING message."""
    ckr.enabled([MessageId.REFPAGE_MISSING])

    # Should error: all flags includes are in the right place now so we need ref pages for them
    assert ckr.check(
        "include::{generated}/api/flags/XrSwapchainCreateFlags.txt[]").messages
    assert ckr.check(
        "include::{generated}/api/flags/XrSwapchainCreateFlagBits.txt[]").messages


def test_openxr_refpage_block(ckr):
    """OpenXR-specific tests of the REFPAGE_BLOCK message."""
    ckr.enabled([MessageId.REFPAGE_BLOCK])

    # Should have 1 error: line before '--' isn't tag
    assert ckr.check(
        """--
        bla
        --""").numDiagnostics() == 1

    # Should have 3 errors:
    #  - line before '--' isn't tag (refpage gets opened anyway),
    #  - tag is inside refpage (which auto-closes the previous refpage block, then re-opens a refpage)
    #  - line after tag isn't '--'
    result = ckr.check(
        """--
        [open,]
        bla
        --""")
    assert result.numDiagnostics() == 3
    # Internally, it's as if the following were the spec source, after putting in the "fake" lines
    # (each of the added lines comes from one message):
    #
    # [open,]
    # --
    # --
    # [open,]
    # --
    # bla
    # --
    assert len([x for x in allMessages(result)
                if "but did not find, a line containing only -- following a reference page tag" in x]) == 1
    assert len([x for x in allMessages(result)
                if "containing only -- outside of a reference page block" in x]) == 1
    assert len([x for x in allMessages(result)
                if "we are already in a refpage block" in x]) == 1
