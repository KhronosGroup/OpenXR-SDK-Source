#!/usr/bin/env python3 -i
#
# Copyright (c) 2017-2025 The Khronos Group Inc.
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
        return f"({'+'.join(sorted(and_terms))})"
    assert and_terms
    return tuple(and_terms)[0]


def _repr_conjunction(and_terms):
    terms = ", ".join(f"'{t}'" for t in sorted(and_terms))
    return "".join(("{", terms, "}"))


FrozenAvailability = Tuple[Tuple[str, ...], ...]


class Availability:
    """
    Information on when something is available.

    In 'disjunctive normal form' - an OR of ANDs.

    >>> Availability([{'a'}, {'b'}])
    [{'a'}, {'b'}]

    >>> Availability([{'a'}, {'a', 'b'}])
    [{'a'}]

    >>> Availability([{'a', 'b'}, {'a'}])
    [{'a'}]
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

        >>> Availability([{'a'}]).add({'a', 'b'})
        True

        >>> Availability([{'a'}]).add({'b'})
        False

        >>> a = Availability([{'a'}])
        >>> a.add({'b'})
        False
        >>> a
        [{'a'}, {'b'}]

        >>> a = Availability([{'a'}])
        >>> a.add({'a', 'b'})
        True
        >>> a
        [{'a'}]

        >>> a = Availability([{'a', 'b'}])
        >>> a.add({'a'})
        False
        >>> a
        [{'a'}]
        """
        return self._add_impl(frozenset(features))

    def merge(self, other: 'Availability') -> bool:
        """Merge two availabilities (by OR)."""
        redundant = [self._add_impl(condition) for condition in other.conjunctions]
        return all(redundant)

    def merged(self, other: 'Availability') -> 'Availability':
        """
        Return the results of merging without changing this object.


        >>> Availability([{'a'}]).merged(Availability([{'b', 'c'}]))
        [{'a'}, {'b', 'c'}]

        >>> Availability([{'a'}]).merged(Availability([{'a', 'b'}, {'b', 'c'}]))
        [{'a'}, {'b', 'c'}]

        >>> Availability([{'a'}, {'b', 'c'}]).merged(Availability([{'a', 'b'}]))
        [{'a'}, {'b', 'c'}]
        """
        clone = deepcopy(self)
        clone.merge(other)
        return clone

    def anded(self, other: 'Availability') -> 'Availability':
        """
        Return the boolean AND of this and another availability.

        >>> Availability([{'a'}]).anded(Availability([{'b', 'c'}]))
        [{'a', 'b', 'c'}]


        >>> Availability([{'a'}]).anded(Availability([{'a', 'b'}]))
        [{'a', 'b'}]

        >>> # a+b+c drops out because a+b is sufficient
        >>> Availability([{'a'}]).anded(Availability([{'a', 'b'}, {'b', 'c'}]))
        [{'a', 'b'}]

        >>> Availability([{'a'}]).anded(Availability([{'b'}, {'c'}]))
        [{'a', 'b'}, {'a', 'c'}]

        >>> Availability([{'a'}]).anded(Availability([{'b', 'c'}, {'d', 'e'}]))
        [{'a', 'b', 'c'}, {'a', 'd', 'e'}]

        >>> Availability([{'a', 'b'}]).anded(Availability([{'c'}, {'d'}]))
        [{'a', 'b', 'c'}, {'a', 'b', 'd'}]

        >>> Availability([{'a'}]).anded(Availability([]))
        []

        >>> Availability([]).anded(Availability([{'a'}]))
        []
        """
        ret = Availability([])

        for self_term in self.conjunctions:
            for other_term in other.conjunctions:
                ret._add_impl(self_term.union(other_term))

        return ret

    def test(self, present_features: Set[str]) -> bool:
        """
        See if the provided features satisfy any of the conjunctions.

        >>> Availability([{'a'}]).test({'b'})
        False

        >>> Availability([{'a'}]).test({'a'})
        True

        >>> Availability([]).test({'a'})
        False
        """
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
        for a, b in itertools.permutations(self.conjunctions, 2):
            if a.issubset(b):
                # One condition is a subset of another
                return False
        return True

    def make_frozen(self) -> FrozenAvailability:
        """Convert to a tuple of term tuples."""
        terms = []
        for condition in self.conjunctions:
            terms.append(tuple(sorted(condition)))
        return tuple(sorted(terms))

    def as_normalized_symbol(self) -> str:
        conjs = [_format_conjunction(sorted(c)).replace("+", "_and_").replace("(", "").replace(")", "")
                 for c in sorted(self.conjunctions)]
        return "_or_".join(conjs)

    def cleaned(self, cleaner) -> 'Availability':
        """Return an availability where each term has been filtered by your function."""
        ret = Availability([])
        for term in self.conjunctions:
            ret.add(set(cleaner(term)))
        return ret

    def _add_impl(self, features: FrozenSet[str]) -> bool:
        if features in self.conjunctions:
            return True

        # Do not add this term if we are a superset of any existing term
        # e.g. A, A+B -> A
        if any(features.issuperset(c) for c in self.conjunctions):
            return True

        # Drop any terms that are a superset of the new term
        remaining = [term for term in self.conjunctions if not term.issuperset(features)]
        remaining.append(features)
        self.conjunctions = remaining
        return False

    def __str__(self):
        conjs = [_format_conjunction(c) for c in sorted(self.conjunctions)]
        return f'({" OR ".join(conjs)})'

    def __repr__(self) -> str:
        conjs = [_repr_conjunction(c) for c in sorted(self.conjunctions)]
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


def _version_cleaner(term: FrozenSet[str]) -> FrozenSet[str]:
    versions = list(sorted((s for s in term if s.startswith("XR_VERSION_"))))
    if not versions:
        return term

    mutable_term = set(term)
    # Saying the default version is redundant if we have more interesting dependencies.
    if len(versions) == 1 and versions[0] == _DEFAULT_VER and len(term) > 1:
        mutable_term.discard(_DEFAULT_VER)

    # keep the last one
    redundant_versions = versions[:-1]
    if redundant_versions:
        mutable_term.difference_update(redundant_versions)

    return frozenset(mutable_term)


_DEFAULT_VER = 'XR_VERSION_1_0'


def _process_depends_string(s: Optional[str]) -> Availability:
    if not s:
        return Availability.create({_DEFAULT_VER})

    terms = [{feat.strip() for feat in t.split("+")} for t in s.split(',')]
    for t in terms:
        if not any(feat.startswith('XR_VERSION_') for feat in t):
            t.add(_DEFAULT_VER)
    return Availability(terms)


@dataclass
class InteractionProfileComponent:
    """A component of an interaction profile"""

    valid_user_paths: Dict[str, Availability]
    """
    The set of user paths (with their corresponding availability) where this component is available.

    For most components, this is a reference to the interaction profile's own valid_user_paths member.
    It is an independent dict when a component is limited to just one user path.
    """

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

    valid_user_paths: Dict[str, Availability]
    """The set of user paths (with their corresponding availability) where this interaction profile is available."""

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

        if limit_to_user_path:
            user_paths = {limit_to_user_path: avail}
        else:
            user_paths = self.valid_user_paths
        component = self.components.get(subpath)
        if component:
            if action_type != component.action_type:
                raise RuntimeError(f"{self.name}: Add {subpath} again: Action type mismatch")
            if system != component.system:
                raise RuntimeError(f"{self.name}: Add {subpath} again from {avail}: System mismatch, {system} vs {component.system}")

            # Just extend the availability. Not caring about return value (whether the avail is redundant)
            component.availability.merge(avail)

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

    def add_user_path(self,
                      path: str,
                      avail: Availability,
                      inherit: Optional[str] = None):
        combined_avail = avail.anded(self.availability)
        if path in self.valid_user_paths.keys():
            redundant = self.valid_user_paths[path].merge(combined_avail)
            if redundant:
                raise RuntimeError(f"{self.name}: Redundant addition of user path {path}!")
        else:
            self.valid_user_paths[path] = combined_avail

        if not inherit:
            return

        for component in self.components.values():
            inherited_avail = component.valid_user_paths.get(inherit)
            if inherited_avail is None:
                # The path we inherit is never valid for this component
                continue

            if path in component.valid_user_paths:
                # If the thing we inherit from is valid for this component,
                # and **we** are already valid for this component somehow, merge our availability.
                component.valid_user_paths[path].merge(combined_avail.anded(inherited_avail))
            else:

                # If the thing we inherit from is valid for this component,
                # and if **we** are not already valid for this component, assign our availability.
                component.valid_user_paths[path] = combined_avail.anded(inherited_avail)

    def yield_user_path_and_component_pairs(self):
        """
        Yield a user path and a component object.

        Outer iteration is over user paths.
        """
        for user_path in sorted(self.valid_user_paths.keys()):
            for subpath in sorted(self.components.keys()):
                component = self.components[subpath]
                if user_path in component.valid_user_paths:
                    yield user_path, component

    def generate_binding_paths(self):
        """
        Yield a full binding path, availability, and the component object.

        Outer iteration is over user paths.
        """
        for user_path, component in self.yield_user_path_and_component_pairs():
            yield (f"{user_path}{component.subpath}",
                   self.compute_component_availability(user_path, component),
                   component)

    def compute_component_availability(self, user_path: str, component: InteractionProfileComponent) -> Availability:
        return self.availability.anded(self.valid_user_paths[user_path]).anded(component.availability).cleaned(_version_cleaner)


@dataclass
class InteractionProfileCondition:
    user_paths: List[str]
    components: List[Dict[str, str]]

    def match(self, profile: InteractionProfile) -> bool:
        for user_path in self.user_paths:
            if user_path not in profile.valid_user_paths:
                return False

        for component in self.components:
            subpath = component['subpath']
            c = profile.components.get(subpath)
            if not c:
                return False
            if c.action_type != component['type']:
                return False

        return True

    @classmethod
    def parse_xml(cls, condition_elt: et.Element) -> 'InteractionProfileCondition':
        user_paths = []
        components = []
        for user_path_elt in condition_elt.findall("./user_path"):
            user_paths.append(user_path_elt.get("path"))

        for component_elt in condition_elt.findall("./component"):
            subpath = component_elt.get("subpath")
            component_type = component_elt.get("type")
            assert subpath
            assert component_type
            components.append({"subpath": subpath, "type": component_type})
        return cls(user_paths=user_paths, components=components)


class InteractionProfileProcessor:
    """Encapsulates the logic to process interaction profiles in the XML."""

    def __init__(self):
        self.interaction_profiles: Dict[str, InteractionProfile] = dict()
        self.extension_dependencies: Dict[str, Availability] = dict()
        self.processed_features: List[et.Element] = []
        self.finished = False
        self.verbose = _VERBOSE_INTERACTION_PROFILE_PROCESSING

    def process_feature(self, root: et.Element, interface: et.Element, emit: bool):
        """
        Handle interaction profiles, and additions to them, in a given interface.

        Call from beginFeature() if you are using the common Generator classes.

        root is the root of the registry XML tree.
        interface is either a `<feature>` tag or `<extension>` tag.

        This logs the features processed, in order, to perform additional passes
        when finishing processing. Call `finish_processing()` once all features
        are processed to perform these subsequent processing passes.
        """
        if self.finished:
            raise ValueError("Cannot process more features once we are finished!")

        interface_name = interface.get('name')
        assert interface_name

        if self.verbose:
            print(f"Handling {interface.tag}: {interface_name} {emit}")

        # Only grab interaction profiles and extension dependencies in this first pass
        for require in interface.findall('./require[interaction_profile]'):
            avail = self._compute_deps_for_interaction_profile(interface, require)

            if self.verbose:
                print(interface.tag, interface_name, avail)

            for include_ipp in require.findall('./interaction_profile'):
                # Path
                name = include_ipp.get("name")
                assert name

                if self.verbose:
                    print(interface_name, name)

                self._include_interaction_profile(root, name, avail)

        if interface.tag == "extension" and "depends" in interface.attrib:
            extension_depends = interface.get("depends")
            assert extension_depends
            self.extension_dependencies[interface_name] = _process_depends_string(extension_depends)

        self.processed_features.append(interface)

    def _process_extending_bindings(self, interface: et.Element):

        interface_name = interface.get('name')
        assert interface_name

        if self.verbose:
            print(f"Handling {interface.tag}: {interface_name}: Pass 2, additional binding paths")
        for require in interface.findall('./require[extend]'):
            avail = self._compute_deps_for_interaction_profile(interface, require)

            for extend in require.findall('./extend[@interaction_profile_path]'):
                profile_name = extend.get("interaction_profile_path")
                assert profile_name
                profile = self.interaction_profiles[profile_name]
                self._add_interaction_profile_components(profile, extend, avail)

    def _process_conditional_extends(self, interface: et.Element, desc: str, profile_extender):

        interface_name = interface.get('name')
        assert interface_name

        if self.verbose:
            print(f"Handling {interface.tag}: {interface_name}: Pass 3, subpass {desc}")

        for require in interface.findall('./require[extend]'):
            avail = self._compute_deps_for_interaction_profile(interface, require)

            for extend in require.findall('./extend[@interaction_profile_predicate]'):
                conditions: List[InteractionProfileCondition] = [
                    InteractionProfileCondition.parse_xml(condition)
                    for condition in extend.findall('./condition')]

                assert conditions
                for profile in self.interaction_profiles.values():
                    if any(condition.match(profile) for condition in conditions):
                        profile_extender(profile, extend, avail)

    def finish_processing(self):
        """
        Perform the second pass over features and other finish-up work.

        To guarantee correct processing, only call this once, after all
        calls to `process_feature()` are complete.
        Call it once from `endFile()` if you are using the common
        `Generator` classes.
        """
        # Pass 2: extend binding paths (components)
        for feature in self.processed_features:
            self._process_extending_bindings(feature)
        # Pass 3: conditionally extend stuff
        for feature in self.processed_features:
            self._process_conditional_extends(feature, "components", self._add_interaction_profile_components)
        for feature in self.processed_features:
            self._process_conditional_extends(feature, "top level /user paths", self._add_interaction_profile_user_paths)

        self.processed_features.clear()
        self.extension_dependencies.clear()

        self.finished = True

    def _include_interaction_profile(self, root: et.Element, name: str, avail: Availability):

        if name in self.interaction_profiles:
            profile = self.interaction_profiles[name]
            redundant = profile.availability.merge(avail)
            if redundant and self.verbose:
                print(f"{name}: Adding redundant availability! {avail}")

            # Add our new route of availability to all components defined inline, without
            # going through the XML again.
            for component in profile.components.values():
                if component.integral:
                    redundant = component.availability.merge(avail)
                    if redundant and self.verbose:
                        print(f"{name}{component.subpath}: Adding redundant availability!")
                        print(f"                new: {avail} existing availability: {component.availability}")

            return

        # Find the profile this refers to.
        profile_elt = root.findall(f".//interaction_profiles/interaction_profile[@name='{name}']")[0]

        raw_user_paths = {user_path.get("path") for user_path in profile_elt.findall("./user_path")}
        user_paths = {path: avail for path in raw_user_paths if path is not None}

        title = profile_elt.get("title")
        assert title
        profile = InteractionProfile(valid_user_paths=user_paths,
                                     name=name,
                                     title=title,
                                     availability=avail)
        self.interaction_profiles[name] = profile

        self._add_interaction_profile_components(profile, profile_elt, avail, integral=True)

    def _compute_deps_for_interaction_profile(self, feature_elt: et.Element, require_elt: et.Element) -> Availability:
        feature_name = feature_elt.get("name")
        assert feature_name

        deps = Availability.create({feature_name})

        require_depends = require_elt.get('depends')
        if require_depends:
            deps = deps.anded(_process_depends_string(require_depends))

        return self._get_chained_requirements(deps.cleaned(_version_cleaner))

    def _get_chained_requirements(self, requirements: Availability) -> Availability:
            updated_requirements = Availability([])
            for conj in requirements.conjunctions:
                term = Availability([conj])
                exts = [feat for feat in conj if not feat.startswith('XR_VERSION_')]
                for ext in exts:
                    ext_deps = self.extension_dependencies.get(ext)
                    if ext_deps is not None:
                        term = term.anded(self._get_chained_requirements(ext_deps))
                updated_requirements.merge(term)

            return updated_requirements

    def _add_interaction_profile_components(self, profile: InteractionProfile, component_parent, avail: Availability, integral: bool = False):
        for component in component_parent.findall("./component"):
            system = False
            if component.get("system") is not None:
                system = True
            profile.add_component(component.get("subpath"),
                                  action_type=component.get("type"),
                                  limit_to_user_path=component.get("user_path"),
                                  system=system, integral=integral, avail=avail)

    def _add_interaction_profile_user_paths(self, profile: InteractionProfile, user_path_parent, avail: Availability):
        for user_path in user_path_parent.findall("./user_path"):
            path = user_path.get("path")
            inherit = user_path.get("inherit")
            profile.add_user_path(path, avail=avail, inherit=inherit)
