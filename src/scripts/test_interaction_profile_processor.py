#!/usr/bin/env python3
#
# Copyright (c) 2017-2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

import pytest

from interaction_profile_processor import Availability, InteractionProfile


def test_extending_integral_component_does_not_leak_into_1_1_profile_availability():
    """
    Ensure extending an integral component (such as grip_surface/pose) with XR_KHR_maintenance1
    does not make a 1.1-only profile (such as touch_pro_controller) accessible via XR_KHR_maintenance1.
    """
    # 1.1-only profile is initialized with XR_VERSION_1_1
    profile_avail = Availability([{"XR_VERSION_1_1"}])
    profile = InteractionProfile(
        valid_user_paths={},
        name="/interaction_profiles/meta/touch_pro_controller",
        title="Meta Quest Touch Pro Controller",
        availability=profile_avail,
    )

    # grip_surface/pose is an integral component in 1.1, added with the profile's availability
    profile.add_component(
        "/input/grip_surface/pose",
        "XR_ACTION_TYPE_POSE_INPUT",
        avail=profile_avail,
        integral=True,
    )

    # XR_KHR_maintenance1 adds grip_surface/pose to matching interaction profiles
    profile.add_component(
        "/input/grip_surface/pose",
        "XR_ACTION_TYPE_POSE_INPUT",
        avail=Availability([{"XR_KHR_maintenance1"}]),
        integral=False,
    )

    # The component itself is available in 1.1 OR XR_KHR_maintenance1
    assert profile.components["/input/grip_surface/pose"].availability.conjunctions == [
        frozenset(["XR_VERSION_1_1"]),
        frozenset(["XR_KHR_maintenance1"]),
    ]

    # BUT the 1.1-only profile itself must NOT leak XR_KHR_maintenance1 into its availability
    assert profile.availability.conjunctions == [frozenset(["XR_VERSION_1_1"])]
