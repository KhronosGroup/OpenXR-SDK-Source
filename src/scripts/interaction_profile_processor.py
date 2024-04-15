#!/usr/bin/python3 -i
#
# Copyright (c) 2017-2024, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0
#
# Original author: Rylie Pavlik <rylie.pavlik@collabora.com>

from collections import defaultdict
import itertools
from typing import Dict, FrozenSet, Iterable, List, Optional, Set, Tuple
from xml.etree import ElementTree as et
from dataclasses import dataclass, field

from copy import deepcopy

_VERBOSE_INTERACTION_PROFILE_PROCESSING = False


def _format_conjunction(and_terms):
    if len(and_terms) > 1:
        return f"({'+'.join(and_terms)})"
    assert and_terms
    return tuple(and_terms)[0]


def _repr_conjunction(and_terms):
    terms = ", ".join(f"'{t}'" for t in and_terms)
    return "".join(("{", terms, "}"))


FrozenAvailability = Tuple[Tuple[str, ...], ...]

class Availability:
    """
    Information on when something is available.

    In 'disjunctive normal form' - an OR of ANDs.
    """

    def __init__(self, conjunctions: Iterable[Set[str]]):
        self.conjunctions: List[FrozenSet[str]] = list()
        """
        A list of possible ways to make something available.

        Satisfy all features in any of the sets.
        """

        for c in conjunctions:
            self.add(c)

    def add(self, features: Set[str]) -> bool:
        """
        Add a new set of features that would make this available.

        Return true if this feature set is redundant given what already exists.
        """
        return self._add_impl(frozenset(features))

    def merge(self, other: 'Availability') -> bool:
        """Merge two availabilities."""
        redundant = [self._add_impl(condition) for condition in other.conjunctions]
        return all(redundant)

    def test(self, present_features: Set[str]) -> bool:
        """See if the provided features satisfy any of the conjunctions."""
        for condition in self.conjunctions:
            if condition.issubset(present_features):
                return True
        return False

    def is_consistent(self) -> bool:
        if any(not c for c in self.conjunctions):
            # got an empty one
            return False
        if len(set(self.conjunctions)) < len(self.conjunctions):
            # got a dupe
            return False
        for a, b in itertools.permutations(self.conjunctions):
            if a.issubset(b):
                # One condition is a subset of another
                return False
        return True

    def issubset(self, potential_superset: "Availability") -> bool:
        for condition in self.conjunctions:
            for other_condition in potential_superset.conjunctions:
                if condition.issubset(other_condition):
                    return True
        return False

    def issuperset(self, other: "Availability") -> bool:
        for condition in self.conjunctions:
            for other_condition in other.conjunctions:
                if condition.issuperset(other_condition):
                    return True
        return False

    def make_frozen(self) -> FrozenAvailability:
        terms = []
        for condition in self.conjunctions:
            terms.append(tuple(sorted(condition)))
        return tuple(sorted(terms))

    def as_normalized_symbol(self) -> str:
        conjs = [_format_conjunction(sorted(c)).replace("+", "_and_").replace("(", "").replace(")", "")
                 for c in sorted(self.conjunctions)]
        return "_or_".join(conjs)

    def _add_impl(self, features: FrozenSet[str]) -> bool:
        if features in self.conjunctions:
            return True

        # these we can't be sure which is right
        redundant = any(features.issubset(c) or features.issuperset(c) for c in self.conjunctions)
        self.conjunctions.append(features)
        return redundant

    def __str__(self):
        conjs = [_format_conjunction(c) for c in self.conjunctions]
        return f'({" OR ".join(conjs)})'

    def __repr__(self) -> str:
        conjs = [_repr_conjunction(c) for c in self.conjunctions]
        return f"[{ ', '.join(conjs)}]"

    @classmethod
    def create(cls, features: Set[str]):
        """Create an "availability" from a single set of required features."""
        return cls(conjunctions=[features, ])


class AvailabilitySymbols:
    def __init__(self):
        self.syms: Dict[str, Availability] = dict()

    def add(self, avail: Availability):
        sym = avail.as_normalized_symbol()
        if sym not in self.syms:
            self.syms[sym] = avail

    def make_frozen(self) -> List[Tuple[str, FrozenAvailability]]:
        return [(key, self.syms[key].make_frozen())
                for key in sorted(self.syms.keys())]


@dataclass
class InteractionProfileComponent:
    """A component of an interaction profile"""

    valid_user_paths: Set[str]
    """The set of user paths where this component is available."""

    subpath: str
    """Starts with /, normally appended to valid user paths."""

    action_type: str
    """One of the XrActionType values"""

    system: bool
    """If true, may not be available for user applications."""

    availability: Availability
    """All conditions for this component."""

    integral: bool
    """True if this component is defined directly in the interaction_profile element."""

    def generate_binding_paths(self):
        """Generate full binding paths."""
        for user_path in self.valid_user_paths:
            yield f"{user_path}{self.subpath}"


@dataclass
class InteractionProfile:
    """A component of an interaction profile"""

    valid_user_paths: Set[str]
    """The set of user paths where this interaction profile is available."""

    name: str
    """Starts with /interaction_profiles."""

    title: str
    """Human-readable description"""

    availability: Availability
    """Base conditions for the availability of this profile."""

    components: Dict[str, InteractionProfileComponent] = field(default_factory=dict)
    """The components available for this profile."""

    def add_component(self,
                      subpath: str,
                      action_type: str,
                      avail: Availability,
                      integral: bool,
                      system: bool = False,
                      limit_to_user_path: Optional[str] = None,
                      verbose: bool = _VERBOSE_INTERACTION_PROFILE_PROCESSING
                      ) -> InteractionProfileComponent:
        # if not avail.issuperset(self.availability):
        #     # the component availability does not include all the requirements of the profile availability?
        #     print(f"{self.name}: Add {subpath}: component availability {avail} is not a superset of profile availability {self.availability}")

        if limit_to_user_path:
            user_paths = {limit_to_user_path}
        else:
            user_paths = deepcopy(self.valid_user_paths)
        if subpath in self.components:
            component = self.components[subpath]
            if action_type != component.action_type:
                raise RuntimeError(f"{self.name}: Add {subpath} again: Action type mismatch")
            if system != component.system:
                raise RuntimeError(f"{self.name}: Add {subpath} again: System mismatch")
            if user_paths != component.valid_user_paths:
                raise RuntimeError(f"{self.name}: Add {subpath} again: Valid user paths mismatch")

            assert len(avail.conjunctions) == 1
            # Just extend the availability
            redundant = component.availability.merge(avail)
            if redundant and verbose:
                print(f"{self.name}{subpath}: Redundant availability {avail}")
            return component

        ret = InteractionProfileComponent(
            valid_user_paths=user_paths,
            subpath=subpath,
            action_type=action_type,
            system=system,
            integral=integral,
            availability=avail)

        self.components[subpath] = ret
        return ret


class InteractionProfileProcessor:
    """Encapsulates the logic to process interaction profiles in the XML."""

    def __init__(self):
        self.interaction_profiles: Dict[str, InteractionProfile] = dict()
        self.deferred_added_components = defaultdict(list)
        self.availabilities: Set[FrozenAvailability] = set()
        self.finished = False
        self.verbose = _VERBOSE_INTERACTION_PROFILE_PROCESSING

    def process_feature(self, root: et.Element, interface: et.Element, emit: bool):
        """
        Handle interaction profiles, and additions to them, in a given interface.

        Call from beginFeature() if you are using the common Generator classes.

        root is the root of the registry XML tree.
        interface is either a `<feature>` tag or `<extension>` tag.

        This may queue added components, if a feature requiring their profile
        is not yet processed. Call process_deferred() to resolve this once all features
        are processed.
        """
        if self.finished:
            print("Hey, we think we are already finished!")

        interface_name = interface.get('name')
        assert interface_name

        if self.verbose:
            print(f"Handling {interface.tag}: {interface_name} {emit}")

        for require in interface.findall('./require[interaction_profile]'):
            required_features = self._compute_deps_for_interaction_profile(interface, require)

            if self.verbose:
                print(interface.tag, interface_name, required_features)

            for include_ipp in require.findall('./interaction_profile'):
                # Path
                name = include_ipp.get("name")
                assert name

                if self.verbose:
                    print(interface_name, name)

                self._include_interaction_profile(root, name, required_features)

        for require in interface.findall('./require[extend]'):
            required_features = self._compute_deps_for_interaction_profile(interface, require)

            for extend in require.findall('./extend[@interaction_profile_path]'):
                profile_name = extend.get("interaction_profile_path")
                assert profile_name
                profile = self.interaction_profiles.get(profile_name)
                if profile:
                    self._add_interaction_profile_components(profile, extend, required_features)
                else:
                    self.deferred_added_components[profile_name].append((extend, required_features))

    def process_deferred(self):
        """
        Process components originating in features processed before their profile.

        This is idempotent: you can call it multiple times safely.
        However, you must have processed the profiles referred to by deferred components
        first! So, just call it once from endFile() if you are using the common
        Generator classes.
        """
        for profile_name, components in self.deferred_added_components.items():
            for extend, deps in components:
                profile = self.interaction_profiles.get(profile_name)
                assert profile
                self._add_interaction_profile_components(profile, extend, deps)
        self.deferred_added_components.clear()

        for profile in self.interaction_profiles.values():
            self.availabilities.add(profile.availability.make_frozen())
            for component in profile.components.values():
                self.availabilities.add(component.availability.make_frozen())

        self.finished = True
        # for ip in self.interaction_profiles.values():
        #     for component in ip.components:

    def _include_interaction_profile(self, root: et.Element, name: str, required_features: Set[str]):

        if name in self.interaction_profiles:
            profile = self.interaction_profiles[name]
            redundant = profile.availability.add(required_features)
            if redundant and self.verbose:
                print(f"{name}: Adding redundant set of required features! {required_features}")

            # Add our new route of availability to all components defined inline, without
            # going through the XML again.
            for component in profile.components.values():
                if component.integral:
                    redundant = component.availability.add(required_features)
                    if redundant and self.verbose:
                        print(f"{name}{component.subpath}: Adding redundant set of required features!")
                        print(f"                new: {required_features} existing availability: {component.availability}")

            return

        # Find the profile this refers to.
        profile_elt = root.findall(f".//interaction_profiles/interaction_profile[@name='{name}']")[0]

        raw_user_paths = {user_path.get("path") for user_path in profile_elt.findall("./user_path")}
        user_paths = {path for path in raw_user_paths if path is not None}

        title = profile_elt.get("title")
        assert title
        avail = Availability.create(required_features)
        profile = InteractionProfile(valid_user_paths=user_paths,
                                     name=name,
                                     title=title,
                                     availability=avail)
        self.interaction_profiles[name] = profile

        self._add_interaction_profile_components(profile, profile_elt, required_features, integral=True)

    def _compute_deps_for_interaction_profile(self, feature_elt: et.Element, require_elt: et.Element) -> Set[str]:
        feature_name = feature_elt.get("name")
        assert feature_name

        deps = {feature_name}

        # TODO finish fixing for schema update
        if feature_elt.tag == "extension":
            requiresCore = feature_elt.get("requiresCore", "1.0")
            deps.add(f"XR_VERSION_{requiresCore.replace('.', '_')}")

        additional_extension = require_elt.get('extension')
        if additional_extension is None:
            additional_extension = require_elt.get('depends')
        if additional_extension is not None:
            deps.add(additional_extension)

        return deps

    def _add_interaction_profile_components(self, profile: InteractionProfile, component_parent, required_features: Set[str], integral: bool = False):

        avail = Availability.create(required_features)
        for component in component_parent.findall("./component"):
            system = False
            if component.get("system") is not None:
                system = True
            profile.add_component(component.get("subpath"),
                                  action_type=component.get("type"),
                                  limit_to_user_path=component.get("user_path"),
                                  system=system, integral=integral, avail=avail)
