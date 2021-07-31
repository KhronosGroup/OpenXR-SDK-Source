#!/usr/bin/python3
#
# Copyright (c) 2019 Collabora, Ltd.
#
# SPDX-License-Identifier: Apache-2.0
#
# Author(s):    Ryan Pavlik <ryan.pavlik@collabora.com>
#
# Purpose:      This script checks some "business logic" in the XML registry.

import re
import sys
from pathlib import Path
from typing import Set

from check_spec_links import XREntityDatabase as OrigEntityDatabase
from reg import Registry
from spec_tools.algo import longest_common_token_prefix
from spec_tools.attributes import LengthEntry
from spec_tools.consistency_tools import XMLChecker
from spec_tools.util import findNamedElem, getElemName, getElemType
from xrconventions import OpenXRConventions as APIConventions

INVALID_HANDLE = "XR_ERROR_HANDLE_INVALID"
UNSUPPORTED = "XR_ERROR_FUNCTION_UNSUPPORTED"
TWO_CALL_STRING_NAME = "buffer"

CAPACITY_INPUT_RE = re.compile(r'(?P<itemname>[a-z]*)CapacityInput')
COUNT_OUTPUT_RE = re.compile(r'(?P<itemname>[a-z]*)CountOutput')

_CREATE_PREFIX = "xrCreate"
_DESTROY_PREFIX = "xrDestroy"
_TYPEENUM = "XrStructureType"

_CREATE_REQUIRED_CODES = {
    "XR_ERROR_LIMIT_REACHED",
    "XR_ERROR_OUT_OF_MEMORY",
}
_DESTROY_FORBIDDEN_CODES = {
    "XR_ERROR_INSTANCE_LOST",
    "XR_ERROR_SESSION_LOST",
    "XR_SESSION_LOSS_PENDING",
    "XR_ERROR_VALIDATION_FAILURE",
}


def get_extension_commands(reg):
    extension_cmds = set()
    for ext in reg.extensions:
        for cmd in ext.findall("./require/command[@name]"):
            extension_cmds.add(cmd.get("name"))
    return extension_cmds


def get_enum_value_names(reg, enum_type):
    names = set()
    result_elem = reg.groupdict[enum_type].elem
    for val in result_elem.findall("./enum[@name]"):
        names.add(val.get("name"))
    return names


# Enum type "names" whose value names don't fit the standard pattern.
ENUM_NAMING_EXCEPTIONS = set((
    # Intentional exceptions:
    # shortened
    _TYPEENUM,
    # shortened/more useful
    "XrResult",
    # not actually a single cohesive enum, just a collection of defines
    "API Constants",

    # Legacy mistake (shortened and not caught before release)
    # See https://gitlab.khronos.org/openxr/openxr/issues/1317
    "XrPerfSettingsNotificationLevelEXT",
))

SPECIFICATION_DIR = Path(__file__).parent.parent

EXT_DECOMPOSE_RE = re.compile(r'XR_(?P<tag>[A-Z]+)_(?P<name>[\w_]+)')
REVISION_RE = re.compile(r' *[*] Revision (?P<num>[1-9][0-9]*),.*')


def get_extension_source(extname):
    match = EXT_DECOMPOSE_RE.match(extname)
    if not match:
        raise RuntimeError("Could not decompose " + extname)

    lower_tag = match.group('tag').lower()
    lower_name = match.group('name').lower()
    fn = '{}_{}.adoc'.format(lower_tag, lower_name)
    return str(SPECIFICATION_DIR / 'sources' / 'chapters' / 'extensions' / lower_tag / fn)


def pluralize(s):
    if s.endswith('y'):
        return s[:-1] + 'ies'
    return s + 's'


class EntityDatabase(OrigEntityDatabase):

    def makeRegistry(self):
        # This tries to override and use lxml instead of the built-in etree.
        # lxml isn't suitable for generation, but it's fine for this checking,
        # and it provides file line info which is useful in messages.
        try:
            import lxml.etree as etree
            HAS_LXML = True
        except ImportError:
            HAS_LXML = False
        if not HAS_LXML:
            return super().makeRegistry()

        registryFile = str(SPECIFICATION_DIR / 'registry/xr.xml')
        registry = Registry()
        registry.filename = registryFile
        registry.loadElementTree(etree.parse(registryFile))
        return registry


class Checker(XMLChecker):
    def __init__(self):
        manual_types_to_codes = {
            # These are hard-coded "manual" return codes:
            # the codes of the value (string, list, or tuple)
            # are available for a command if-and-only-if
            # the key type is passed as an input.
            "XrSystemId":  "XR_ERROR_SYSTEM_INVALID",
            "XrFormFactor": ("XR_ERROR_FORM_FACTOR_UNSUPPORTED",
                             "XR_ERROR_FORM_FACTOR_UNAVAILABLE"),
            "XrInstance": ("XR_ERROR_INSTANCE_LOST"),
            "XrSession":
                ("XR_ERROR_SESSION_LOST", "XR_SESSION_LOSS_PENDING"),
            "XrPosef":
                "XR_ERROR_POSE_INVALID",
            "XrViewConfigurationType":
                "XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED",
            "XrEnvironmentBlendMode":
                "XR_ERROR_ENVIRONMENT_BLEND_MODE_UNSUPPORTED",
            "XrCompositionLayerBaseHeader": ("XR_ERROR_LAYER_INVALID"),
            "XrPath": ("XR_ERROR_PATH_INVALID", "XR_ERROR_PATH_UNSUPPORTED"),
            "XrTime": "XR_ERROR_TIME_INVALID"
        }
        forward_only = {
            # Like the above, but these are only valid in the
            # "type implies return code" direction
        }
        reverse_only = {
            # like the above, but these are only valid in the
            # "return code implies type or its descendant" direction
            "XrSession": ("XR_ERROR_SESSION_RUNNING",
                          "XR_ERROR_SESSION_NOT_RUNNING",
                          "XR_ERROR_SESSION_NOT_READY",
                          "XR_ERROR_SESSION_NOT_STOPPING",
                          "XR_SESSION_NOT_FOCUSED",
                          "XR_FRAME_DISCARDED"),
            "XrReferenceSpaceType": "XR_SPACE_BOUNDS_UNAVAILABLE",
            "XrDuration": "XR_TIMEOUT_EXPIRED",
            "uint32_t": "XR_ERROR_SIZE_INSUFFICIENT",
        }

        # Some return codes are related in that only one of a set
        # may be returned by a command
        # (eg. XR_ERROR_SESSION_RUNNING and XR_ERROR_SESSION_NOT_RUNNING)
        self.exclusive_return_code_sets = (
            set(("XR_ERROR_SESSION_NOT_RUNNING", "XR_ERROR_SESSION_RUNNING")),
        )

        # Keys are entity names, values are tuples or lists of message text to suppress.
        suppressions = {
            # path to string can take any path
            "xrPathToString": ("Missing expected return code(s) XR_ERROR_PATH_UNSUPPORTED implied because of input of type XrPath",)
        }

        conventions = APIConventions()
        db = EntityDatabase()

        self.extension_cmds = get_extension_commands(db.registry)
        self.return_codes: Set[str] = get_enum_value_names(db.registry, 'XrResult')
        self.structure_types: Set[str] = get_enum_value_names(db.registry, _TYPEENUM)

        # Initialize superclass
        super().__init__(entity_db=db, conventions=conventions,
                         manual_types_to_codes=manual_types_to_codes,
                         forward_only_types_to_codes=forward_only,
                         reverse_only_types_to_codes=reverse_only,
                         suppressions=suppressions)

    def check(self):
        # Our custom check happens before the rest of the checks.
        # This is because the end of the super-class' check()
        # does the printing of the results.
        for name in self.db.registry.groupdict:
            if name not in ENUM_NAMING_EXCEPTIONS:
                self.set_error_context(entity=name, elem=self.db.registry.groupdict[name].elem)
                self.check_enum_naming(name)
        super().check()

    def check_enum_naming(self, enum_type):
        stripped_enum_type, tag = self.strip_extension_tag(enum_type)
        end = "_{}".format(tag) if tag else ""
        bare_end = None
        if stripped_enum_type.endswith("FlagBits"):
            bare_end = "_BIT"
            end = bare_end + end
            stripped_enum_type = stripped_enum_type.replace("FlagBits", "")
        # This is how we apply the "convert type to enum value name" transform: pretend it's a structure type,
        # then replace "XR_TYPE_" with the generic prefix "XR_"
        start = self.conventions.generate_structure_type_from_name(stripped_enum_type).replace("XR_TYPE", "XR") + "_"

        value_names = get_enum_value_names(self.db.registry, enum_type)

        # Check that they do start with the right beginning.
        for name in value_names:
            stripped_name, tag = self.strip_extension_tag(name)
            if not name.startswith(start):
                self.record_error('Got an enum value whose name does not match the pattern: got', name,
                                  'but expected something that started with', start, 'due to typename being', enum_type)

            if bare_end:
                # If bare_end is set, end is always non-empty because it means it's a bitmask.
                assert(end)
                if not name.endswith(end) and not stripped_name.endswith(bare_end):
                    self.record_error('Got an enum value whose name does not match the pattern: got', name,
                                      'but expected something that ended with', end, ', or', bare_end,
                                      'plus a vendor/author tag, due to typename being', enum_type)
            elif end:
                if not name.endswith(end):
                    self.record_error('Got an enum value whose name does not match the pattern: got', name,
                                      'but expected something that ended with', end, 'due to typename being', enum_type)

        # Check that the expected beginning is the longest common prefix (meaning the type is named right)
        if len(value_names) > 1:
            prefix = longest_common_token_prefix(value_names)
            if prefix != start:
                self.record_error('Got an enum whose name does not match the pattern: the type is', enum_type,
                                  'which would cause values to start with', start, 'but got a different longest common prefix', prefix)

    def add_extra_codes(self, types_to_codes):
        """Add any desired entries to the types-to-codes DictOfStringSets
        before performing "ancestor propagation".

        Passed to compute_type_to_codes as the extra_op."""
        # All handle types can result in an invalid handle error.
        for type_name in self.handle_data.handle_types:
            types_to_codes.add(type_name, INVALID_HANDLE)

    def get_codes_for_command_and_type(self, cmd_name, type_name):
        """Return a set of return codes expected due to having
        an input argument of type type_name."""
        codes = super().get_codes_for_command_and_type(cmd_name, type_name)

        # Filter out any based on the specific command
        if cmd_name.startswith(_DESTROY_PREFIX):
            # xrDestroyX should not return XR_ERROR_anything_LOST or XR_anything_LOSS_PENDING
            codes = {x for x in codes if not x.endswith("_LOST")}
            codes = {x for x in codes if not x.endswith("_LOSS_PENDING")}

        return codes

    def get_required_codes_for_command(self, cmd_name):
        """Return a set of return codes required due to having a particular name."""
        codes = set()

        if cmd_name.startswith(_CREATE_PREFIX):
            codes.update(_CREATE_REQUIRED_CODES)

        return codes

    def get_forbidden_codes_for_command(self, cmd_name):
        """Return a set of return codes not permittted due to having a particular name."""
        if cmd_name.startswith(_DESTROY_PREFIX):
            return _DESTROY_FORBIDDEN_CODES
        return set()

    def check_command_return_codes_basic(self, name, info,
                                         successcodes, errorcodes):
        """Check a command's return codes for consistency.

        Called on every command."""
        # Check that all extension commands can return the code associated
        # with trying to use an extension that wasn't enabled.
        if name in self.extension_cmds and UNSUPPORTED not in errorcodes:
            self.record_error("Missing expected return code",
                              UNSUPPORTED,
                              "implied due to being an extension command")

        codes = successcodes.union(errorcodes)

        # Check that all return codes are recognized.
        unrecognized = codes - self.return_codes
        if unrecognized:
            self.record_error("Unrecognized return code(s):",
                              unrecognized)

        for exclusive_set in self.exclusive_return_code_sets:
            specified_exclusives = exclusive_set.intersection(codes)
            if len(specified_exclusives) > 1:
                self.record_error(
                    "Mutually-exclusive return codes specified:",
                    specified_exclusives)

    def check_two_call_capacity_input(self, param_name, param_elem, match):
        """Check a two-call-idiom command's CapacityInput parameter."""
        param_type = getElemType(param_elem)
        if param_type != 'uint32_t':
            self.record_error('Two-call-idiom call has capacity parameter', param_name,
                              'with type', param_type, 'instead of uint32_t')
        optional = param_elem.get('optional')
        if optional != 'true':
            self.record_error('Two-call-idiom call capacity parameter has incorrect "optional" attribute',
                              optional, '- expected "true"')

        prefix = match.group('itemname')
        if prefix.endswith('s'):
            self.record_error('"Item name" part of capacity input parameter name appears to be plural:', prefix)

    def check_two_call_count_output(self, param_name, param_elem, match):
        """Check a two-call-idiom command's CountOutput parameter."""
        param_type = getElemType(param_elem)
        if param_type != 'uint32_t':
            self.record_error('Two-call-idiom call has count parameter', param_name,
                              'with type', param_type, 'instead of uint32_t')
        type_elem = param_elem.find('type')
        assert(type_elem is not None)

        tail = type_elem.tail.strip()
        if '*' != tail:
            self.record_error('Two-call-idiom call has count parameter', param_name,
                              'that is not a pointer:', type_elem.text, type_elem.tail)

        optional = param_elem.get('optional')
        if optional is not None:
            self.record_error('Two-call-idiom call count parameter has "optional" attribute, when none should be present:',
                              optional)

        prefix = match.group('itemname')
        if prefix.endswith('s'):
            self.record_error('"Item name" part of count output parameter name appears to be plural:', prefix)

    def check_two_call_array(self, param_name, param_elem, match):
        """Check a two-call-idiom command's array output parameter."""
        optional = param_elem.get('optional')
        if optional != 'true':
            self.record_error('Two-call-idiom call array parameter has incorrect "optional" attribute',
                              optional, '- expected "true"')

        type_elem = param_elem.find('type')
        assert(type_elem is not None)

        tail = type_elem.tail.strip()
        if '*' not in tail:
            self.record_error('Two-call-idiom call has array parameter', param_name,
                              'that is not a pointer:', type_elem.text, type_elem.tail)

        length = LengthEntry.parse_len_from_param(param_elem)
        if not length[0].other_param_name:
            self.record_error('Two-call-idiom call has array parameter', param_name,
                              'whose first length is not another parameter:', length[0])

        # Only valid reason to have more than 1 comma-separated entry in len is for strings,
        # which are named "buffer".
        if length is None:
            self.record_error('Two-call-idiom call has array parameter', param_name,
                              'with no length attribute specified')
        else:
            if len(length) > 2:
                self.record_error('Two-call-idiom call has array parameter', param_name,
                                  'with too many lengths - should be 1 or 2 comma-separated lengths, not', len(length))
            if len(length) == 2:
                if not length[1].null_terminated:
                    self.record_error('Two-call-idiom call has two-length array parameter', param_name,
                                      'whose second length is not "null-terminated":', length[1])

                param_type = getElemType(param_elem)
                if param_type != 'char':
                    self.record_error('Two-call-idiom call has two-length array parameter', param_name,
                                      'that is not a string:', param_type)

                if param_name != TWO_CALL_STRING_NAME:
                    self.record_error('Two-call-idiom call has two-length array parameter', param_name,
                                      'that is not named "buffer"')

    def check_two_call_command(self, name, info, params):
        """Check a two-call-idiom command."""
        named_params = [(getElemName(p), p) for p in params]

        # Find the three important parameters
        capacity_input_param_name = None
        capacity_input_param_match = None
        count_output_param_name = None
        count_output_param_match = None
        array_param_name = None
        for param_name, param_elem in named_params:
            match = CAPACITY_INPUT_RE.match(param_name)
            if match:
                capacity_input_param_name = param_name
                capacity_input_param_match = match
                self.check_two_call_capacity_input(param_name, param_elem, match)
                continue
            match = COUNT_OUTPUT_RE.match(param_name)
            if match:
                count_output_param_name = param_name
                count_output_param_match = match
                self.check_two_call_count_output(param_name, param_elem, match)
                continue

            # Try detecting the output array using its length field
            if capacity_input_param_name:
                length = LengthEntry.parse_len_from_param(param_elem)
                if length and length[0].other_param_name == capacity_input_param_name:
                    array_param_name = param_name

        if not capacity_input_param_name:
            self.record_error('Apparent two-call-idiom call missing a *CapacityInput parameter')

        if not count_output_param_name:
            self.record_error('Apparent two-call-idiom call missing a *CountOutput parameter')

        if not array_param_name:
            self.record_error('Apparent two-call-idiom call missing an array output parameter')

        if not capacity_input_param_name or \
                not count_output_param_name or \
                not array_param_name:
            # If we're missing at least one, stop checking two-call stuff here.
            return

        input_prefix = capacity_input_param_match.group('itemname')
        output_prefix = count_output_param_match.group('itemname')
        if input_prefix != output_prefix:
            self.record_error('Two-call-idiom call has "item name" prefixes for input and output names that mismatch:',
                              input_prefix, output_prefix)

        # If not a "buffer", pluralize the item name for the array name.
        if input_prefix == TWO_CALL_STRING_NAME and output_prefix == TWO_CALL_STRING_NAME:
            expected_array_name = TWO_CALL_STRING_NAME
        else:
            expected_array_name = pluralize(input_prefix)

        if expected_array_name != array_param_name:
            self.record_error('Two-call-idiom call has array parameter with unexpected name: expected',
                              expected_array_name, 'got', array_param_name)
        # TODO check order: all other params, then capacity input, count output, and array

    def check_command(self, name, info):
        """Check a command's XML data for consistency.

        Called from check."""
        t = info.elem.find('proto/type')
        if t is None:
            self.record_warning("Got a command without a return type?")
        else:
            return_type = t.text
            if return_type != 'XrResult':
                self.record_error("Got a function that returned", return_type,
                                  "instead of XrResult - some scripts/software assume all functions return XrResult!")
        params = info.elem.findall('./param')
        for p in params:
            param_name = getElemName(p)
            if COUNT_OUTPUT_RE.match(param_name) or CAPACITY_INPUT_RE.match(param_name):
                self.check_two_call_command(name, info, params)
                break

        return_type = info.elem.find('proto/type')
        if self.conventions.requires_error_validation(return_type):
            # This command returns an API result code, so check that it
            # returns at least the required errors.
            required_errors = set(self.conventions.required_errors)
            errorcodes = info.elem.get('errorcodes').split(',')
            if not required_errors.issubset(set(errorcodes)):
                self.record_error('Missing required error code')

        if name.startswith(_DESTROY_PREFIX):
            if len(params) != 1:
                self.record_error('A destroy command should have exactly one parameter')
            if params:
                param = params[0]
                if param.get("externsync") is None:
                    self.record_error('A destroy command parameter should have an externsync attribute, probably externsync="true_with_children"')

        super().check_command(name, info)

    def check_type(self, name, info, category):
        """Check a type's XML data for consistency.

        Called from check."""

        elem = info.elem
        type_elts = [elt
                     for elt in elem.findall("member")
                     if getElemType(elt) == _TYPEENUM]
        if category == 'struct' and type_elts:
            if len(type_elts) > 1:
                self.record_error(
                    "Have more than one member of type", _TYPEENUM)
            else:
                type_elt = type_elts[0]
                val = type_elt.get('values')
                if val and val not in self.structure_types:
                    self.record_error("Unknown structure type constant", val)
        elif category == "bitmask":
            if 'Flags' in name:
                expected_bitvalues = name.replace('Flags', 'FlagBits')
                bitvalues = info.elem.get('bitvalues')
                if expected_bitvalues != bitvalues:
                    self.record_error("Unexpected bitvalues attribute value:",
                                      "got", bitvalues,
                                      "but expected", expected_bitvalues)
        super().check_type(name, info, category)

    def check_extension(self, name, info, supported):
        """Check an extension's XML data for consistency.

        Called from check."""
        super().check_extension(name, info, supported)
        if not supported:
            return

        elem = info.elem
        name_upper = name.upper()
        version_name = "{}_SPEC_VERSION".format(name)
        enums = elem.findall('./require/enum[@name]')
        version_elem = findNamedElem(enums, version_name)
        if version_elem is None:
            self.record_error("Missing version enum", version_name)
        else:
            fn = get_extension_source(name)
            revisions = []
            try:
                with open(fn, 'r', encoding='utf-8') as fp:
                    for line in fp:
                        line = line.rstrip()
                        match = REVISION_RE.match(line)
                        if match:
                            revisions.append(int(match.group('num')))
                ver_from_xml = version_elem.get('value')
                if revisions:
                    ver_from_text = str(max(revisions))
                    if ver_from_xml != ver_from_text:
                        self.record_error("Version enum mismatch: spec text indicates", ver_from_text,
                                          "but XML says", ver_from_xml)
                else:
                    if ver_from_xml == '1':
                        self.record_warning(
                            "Cannot find version history in spec text - make sure it has lines starting exactly like '* Revision 1, ....'",
                            filename=fn)
                    else:
                        self.record_error("Cannot find version history in spec text, but XML reports a non-1 version number", ver_from_xml,
                                          " - make sure the spec text has lines starting exactly like '* Revision 1, ....'",
                                          filename=fn)
            except FileNotFoundError:
                # This is OK: just means we can't check against the spec text.
                pass

        name_define = "{}_EXTENSION_NAME".format(name_upper)
        name_elem = findNamedElem(enums, name_define)
        if name_elem is None:
            self.record_error("Missing name enum", name_define)
        else:
            # Note: etree handles the XML entities here and turns &quot; back into "
            expected_name = '"{}"'.format(name)
            name_val = name_elem.get('value')
            if name_val != expected_name:
                self.record_error("Incorrect name enum: expected", expected_name,
                                  "got", name_val)


if __name__ == "__main__":

    ckr = Checker()
    ckr.check()

    if ckr.fail:
        sys.exit(1)
