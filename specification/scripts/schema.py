#!/usr/bin/env python3 -i
#
# Copyright 2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

"""String constants to use when parsing the XML according to our schema."""


class CommandAttrs:
    """Constant attribute names for <command> tags."""
    SUCCESSCODES = "successcodes"
    ERRORCODES = "errorcodes"


class ExtendTag:
    """Constant strings related to <extend> tags."""
    NAME = "extend"

    TYPE_ATTR = "type"

    TYPE_VAL_COMMAND = "command"
    """Value for the "type" attribute when extending return codes for a command."""

    INTERACTION_PROFILE_PATH_ATTR = "interaction_profile_path"
    INTERACTION_PROFILE_PREDICATE_ATTR = "interaction_profile_predicate"

    # for type=command
    NAME_ATTR = "name"
    SUCCESSCODES_ATTR = "successcodes"
    ERRORCODES_ATTR = "errorcodes"
