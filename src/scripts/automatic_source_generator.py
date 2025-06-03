#!/usr/bin/env python3 -i
#
# Copyright (c) 2017-2025 The Khronos Group Inc.
# Copyright (c) 2017-2019 Valve Corporation
# Copyright (c) 2017-2019 LunarG, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Author(s):    Mark Young <marky@lunarg.com>
#
# Purpose:      This file is the base python script for parsing the
#               registry and forming the information in an easy to use
#               way for the rest of the automatic source generation scripts.

import re
from dataclasses import dataclass
from inspect import currentframe, getframeinfo
from typing import List, Optional, Tuple, Union
from xml.etree import ElementTree as et

from generator import (GeneratorOptions, MissingRegistryError, OutputGenerator,
                       noneStr, regSortFeatures, write)
from spec_tools.attributes import (LengthEntry, has_any_optional_in_param,
                                   parse_optional_from_param)
from spec_tools.util import getElemName


EXTNAME_RE = re.compile("^(?P<api>XR|VK)_(?P<tag>(?P<base_tag>[A-Z]+?)(?P<experimental_suffix>X*[0-9]*))_(?P<ext_name>.*)$")


def undecorate(name):
    """Undecorate a name by removing the leading Xr and making it lowercase."""
    lower = name.lower()
    assert lower.startswith('xr')
    return lower[2:]


class AutomaticSourceGeneratorOptions(GeneratorOptions):
    """AutomaticSourceGeneratorOptions - subclass of GeneratorOptions."""

    def __init__(self,
                 conventions=None,
                 filename=None,
                 directory='.',
                 apiname=None,
                 profile=None,
                 versions='.*',
                 emitversions='.*',
                 defaultExtensions=None,
                 addExtensions=None,
                 removeExtensions=None,
                 emitExtensions=None,
                 sortProcedure=regSortFeatures,
                 prefixText="",
                 genFuncPointers=True,
                 protectFile=True,
                 protectFeature=True,
                 protectProto=None,
                 protectProtoStr=None,
                 apicall='',
                 apientry='',
                 apientryp='',
                 indentFuncProto=True,
                 indentFuncPointer=False,
                 alignFuncParam=0,
                 genEnumBeginEndRange=False):
        GeneratorOptions.__init__(self,
                                  conventions=conventions,
                                  filename=filename,
                                  directory=directory,
                                  apiname=apiname,
                                  profile=profile,
                                  versions=versions,
                                  emitversions=emitversions,
                                  defaultExtensions=defaultExtensions,
                                  addExtensions=addExtensions,
                                  removeExtensions=removeExtensions,
                                  emitExtensions=emitExtensions,
                                  sortProcedure=sortProcedure)
        # Instead of using prefixText, we write our own
        self.prefixText = None
        self.genFuncPointers = genFuncPointers
        self.protectFile = protectFile
        self.protectFeature = protectFeature
        self.protectProto = protectProto
        self.protectProtoStr = protectProtoStr
        self.apicall = apicall
        self.apientry = apientry
        self.apientryp = apientryp
        self.indentFuncProto = indentFuncProto
        self.indentFuncPointer = indentFuncPointer
        self.alignFuncParam = alignFuncParam
        self.genEnumBeginEndRange = genEnumBeginEndRange


@dataclass
class LengthMember:
    """Length Member data"""

    array_name: str
    """The name of the array"""

    length_name: str
    """The name of the length parameter"""


@dataclass
class HandleData:
    """Handle data"""

    name: str
    """The name of the handle"""

    parent: Optional[str]
    """The name of the handle's direct parent"""

    ancestors: List[str]
    """A list of all the handle's ancestors"""

    protect_value: Optional[str]
    """None or a comma-delimited list indicating #define values to use around this handle"""

    protect_string: str
    """Empty string or string to use after #if to protect this handle"""

    ext_name: Optional[str]
    """Name of extension this handle is associated with (or None)"""


@dataclass
class FlagBits:
    """Flag data"""

    name: str
    """The name of the flag"""

    type: str
    """The base type of the flag (for example uint64_t)"""

    valid_flags: str
    """The list of valid flag values"""

    protect_value: Optional[str]
    """None or a comma-delimited list indicating #define values to use around this flag"""

    protect_string: str
    """Empty string or string to use after #if to protect this flag"""

    ext_name: Optional[str]
    """Name of extension this command is associated with (or None)"""


@dataclass
class EnumBitValue:
    """Individual Enum bit value"""

    name: str
    """Name of an individual enum bit"""

    protect_value: Optional[str]
    """None or a comma-delimited list indicating #define values to use around this value"""

    protect_string: str
    """Empty string or string to use after #if to protect this value"""

    ext_name: Optional[str]
    """Name of extension this command is associated with (or None)"""

    alias: Optional[str]
    """None or the name of the type this is an alias for"""


@dataclass
class EnumData:
    """Enum type data"""

    name: str
    """The name of the enum"""

    values: List[EnumBitValue]
    """List of possible EnumBitValue for this enum."""

    protect_value: Optional[str]
    """None or a comma-delimited list indicating #define values to use around this enum"""

    protect_string: str
    """Empty string or string to use after #if to protect this enum"""

    ext_name: Optional[str]
    """Name of extension this command is associated with (or None)"""


@dataclass
class MemberOrParam:
    """Struct/Union member or Command parameter data"""

    type: str
    """The type of this parameter"""

    is_handle: bool
    """Boolean indicating if this type is a handle"""

    is_const: bool
    """Boolean indicating if this has a const identifier"""

    is_bool: bool
    """Boolean indicating if this is a boolean type"""

    is_optional: bool
    """Boolean indicating if this is optional (pointers may be NULL, etc)"""

    is_array: bool
    """Boolean indicating if this is an array"""

    array_dimen: int
    """Number of dimensions for this array"""

    is_static_array: bool
    """Boolean indicating if this is a statically sized array"""

    static_array_sizes: Optional[List[int]]
    """List of static array sizes for each dimension"""

    array_count_var: str
    """Name of array count if this is a value, and not a number"""

    array_length_for: str
    """Indicates the array parameter that this is a length for"""

    pointer_count: int
    """Depth of pointers for this type (* = 1, ** == 2)"""

    pointer_count_var: str
    """If this is a pointer, and an array, it's the max size of that array"""

    is_null_terminated: bool
    """Is this parameter null-terminated"""

    no_auto_validity: bool
    """Boolean indicating if this should not have any automatic validation performed"""

    name: str
    """The parameter name"""

    valid_extension_structs: Optional[str]
    """Name of valid extension structs for this param (usually for 'next')"""

    cdecl: str
    """The complete C-declaration for this parameter."""

    values: Optional[str]
    """None or comma-separated list of valid values"""


@dataclass
class StructUnionData:
    """Information regarding a structure or union"""

    name: str
    """Name of the structure or union"""

    ext_name: Optional[str]
    """Name of extension this struct/union is associated with (or None)"""

    required_exts: List[str]
    """Additional extensions required for this struct/union to be valid"""

    returned_only: bool
    """Boolean indicating that this struct/union is only for returning information"""

    members: List[MemberOrParam]
    """List of MemberOrParam for each member in this struct/union"""

    protect_value: Optional[str]
    """None or a comma-delimited list indicating #define values to use around this structure/union"""

    protect_string: str
    """Empty string or string to use after #if to protect this structure/union"""


@dataclass
class CommandData:
    """Command data"""

    name: str
    """Name of command"""

    is_create_connect: bool
    """Boolean indicating this is a create/connect command"""

    is_destroy_disconnect: bool
    """Boolean indicating this is a destroy/disconnect command"""

    ext_name: Optional[str]
    """Name of extension this command is associated with (or None)"""

    ext_type: str
    """Type of extension (instance, device)"""

    required_exts: List[str]
    """Additional extensions required for this command to be valid"""

    handle_type: str
    """Type of handle used as the primary type for this command"""

    handle: str
    """Base handle for this command"""

    has_instance: bool
    """True if an instance is used somewhere in the command"""

    return_type: Optional[et.Element]
    """Type of return (or None)"""

    return_values: str
    """Documented return values (core only)"""

    params: List[MemberOrParam]
    """List of MemberOrParam for each parameter in this command"""

    protect_value: Optional[str]
    """None or a comma-delimited list indicating #define values to use around this command"""

    protect_string: str
    """Empty string or string to use after #if to protect this command"""

    begins_state: bool
    """Boolean indicating this command begins some kind of state"""

    ends_state: bool
    """Boolean indicating this command ends some kind of state"""

    checks_state: bool
    """Boolean indicating this command checks for some kind of state"""

    cdecl: str
    """The complete C-declaration for this command"""


@dataclass
class ExtensionData:
    """Information on a given extension"""

    name: str
    """Name of this extension (ex. XR_EXT_foo)"""

    vendor_tag: str
    """Vendor tag associated with this extension"""

    type: str
    """Type of extension (instance, device, ...)"""

    define: str
    """Define containing a string version of the extension name"""

    num_commands: int
    """Number of commands in the extension"""

    required_exts: List[str]
    """List of required extensions for using this extension's functionality"""

    protect_value: Optional[str]
    """None or a comma-delimited list indicating #define values to use around this extension"""

    protect_string: str
    """Empty string or string to use after #if to protect this extension"""


@dataclass
class BaseTypeData:
    """Base type listing (converts from a type into a name which is used as a type somewhere else)."""

    type: str
    """The base type"""

    name: str
    """The name of the derived type"""


@dataclass
class TypeData:
    """Type listing"""

    name: str
    """The name of the type"""

    protect_value: Optional[str]
    """None or a comma-delimited list indicating #define values to use around this type"""

    protect_string: str
    """Empty string or string to use after #if to protect this type"""

    alias: Optional[str]
    """None or the name of the type this is an alias for"""


@dataclass
class StructRelationGroup:
    """Structure relation group data"""

    generic_struct_name: str
    """The name of the generic structure defining the common elements"""

    child_struct_names: List[str]
    """Children of the base structure"""


@dataclass
class ApiState:
    """API state listing"""

    state: str
    """The name of the state"""

    type: str
    """The type that is associated with the state (handle/struct/...)"""

    variable: str
    """State variable"""

    begin_commands: List[CommandData]
    """List of commands that begin the current state"""

    end_commands: List[CommandData]
    """List of commands that end the current state"""

    check_commands: List[CommandData]
    """List of commands that check the current state"""


class AutomaticSourceOutputGenerator(OutputGenerator):
    """Parse source based on XML element attributes from registry"""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        # Did we encounter an error
        self.encountered_error = False
        # List of strings containing all vendor tags
        self.vendor_tags: List[str] = []
        # Current vendor tag that should be used by this extension
        self.current_vendor_tag = ''
        # A define used to set the current API version in a component (in the loader, layers, etc).
        self.api_version_define = ''
        # A defined used to grab the current API Header's version
        self.header_version = ''
        # A list of all the API's core commands (CommandData).
        self.core_commands: List[CommandData] = []
        # A list of all the API's extension commands (CommandData).
        self.ext_commands: List[CommandData] = []
        # A list of all extensions (ExtensionData) for this API
        self.extensions: List[ExtensionData] = []
        # A list of all base data types (BaseTypeData) for this API
        self.api_base_types: List[BaseTypeData] = []
        # A list of all handles (HandleData) for this API
        self.api_handles: List[HandleData] = []
        # A list of all structures (StructUnionData) for this API
        self.api_structures: List[StructUnionData] = []
        # A list of all unions (StructUnionData) for this API
        self.api_unions: List[StructUnionData] = []
        # A list of all enumeration (EnumData) for this API
        self.api_enums: List[EnumData] = []
        # A list of all flags (FlagBits) for this API
        self.api_flags: List[FlagBits] = []
        # A list of all bitmasks (EnumData) for this API
        self.api_bitmasks: List[EnumData] = []
        # A list of all object types
        self.api_object_types = []
        # A list of all result types
        self.api_result_types = []
        # A list of all structure types
        self.api_structure_types = []
        # A dictionary of all base types to struct relation groups
        self._struct_relation_groups = {}
        # A list of all the API states
        self.api_states = []
        # A mapping of all aliases
        self.aliases = {}
        # Max lengths for various items
        self.max_extension_name_length = 32
        self.max_structure_type_length = 32
        self.max_result_length = 32
        self.structs_with_no_type = ['XrBaseInStructure', 'XrBaseOutStructure']
        # The following commands should only exist in the loader, and only as a trampoline
        # (i.e. Don't add it to the dispatch table)
        self.no_trampoline_or_terminator = [
            'xrEnumerateApiLayerProperties',
            'xrEnumerateInstanceExtensionProperties',
            'xrNegotiateLoaderRuntimeInterface',
            'xrCreateApiLayerInstance',
            'xrNegotiateLoaderApiLayerInterface',
            'xrInitializeLoaderKHR'
        ]

    # This is the basic warning about the source file being generated.  It can be
    # overridden by a derived class.
    #   self            the AutomaticSourceOutputGenerator object
    def outputGeneratedHeaderWarning(self):
        generated_warning = '// *********** THIS FILE IS GENERATED - DO NOT EDIT ***********\n'
        generated_warning += '//     See automatic_source_generator.py for modifications\n'
        generated_warning += '// ************************************************************\n'
        write(generated_warning, file=self.outFile)

    # The author tag can vary per python file, so let it be overridden
    #   self            the AutomaticSourceOutputGenerator object
    def outputGeneratedAuthorNote(self):
        author_note = '//\n'
        author_note += '// Author: Mark Young <marky@lunarg.com>\n'
        author_note += '//\n'
        write(author_note, file=self.outFile)

    # Print an error message that will clearly identify any problems encountered during the
    # code generation process, and return a string that can be inserted to break a compile.
    #   self            the AutomaticSourceOutputGenerator object
    #   message         the message to display
    #   file            the name of the file encountering the problem (or None, default, to autodetect)
    #   line            the line number the failure occurred on (or None, default, to autodetect)
    def printCodeGenErrorMessage(self, message, file=None, line=None):
        if file is None or line is None:
            frame = currentframe()
            if frame is not None:
                frame = frame.f_back
            if frame is not None:
                frame_info = getframeinfo(frame)
                if file is None:
                    file = frame_info.filename
                if line is None:
                    line = frame_info.lineno + 1
        # The filename we get is the full path file name.  So trim it down to only the
        # last two folders
        trimmed_file = file
        second_last_dir = file.rfind('/', 0, file.rfind('/'))
        if second_last_dir < 0:
            second_last_dir = file.rfind('\\', 0, file.rfind('\\'))
        if second_last_dir > 0:
            trimmed_file = file[second_last_dir + 1:]
        # Set the error flag so we can insert a failure into the build
        self.encountered_error = True
        # Write the message
        print('**CODEGEN_ERROR: %s[%d] %s' % (trimmed_file, line, message))
        return f'#{line} "{file}"\n#error("Bug: {message}")\n'

    # Print a warning message the will clearly identify any possible problems encountered
    # during the code generation process
    #   self            the AutomaticSourceOutputGenerator object
    #   file            the name of the file encountering the problem
    #   line            the line number the failure occurred on
    #   message         the message to display
    def printCodeGenWarningMessage(self, file, line, message):
        # The filename we get is the full path file name.  So trim it down to only the
        # last two folders
        trimmed_file = file
        second_last_dir = file.rfind('/', 0, file.rfind('/'))
        if second_last_dir < 0:
            second_last_dir = file.rfind('\\', 0, file.rfind('\\'))
        if second_last_dir > 0:
            trimmed_file = file[second_last_dir + 1:]
        # Write the message
        print('**CODEGEN_WARNING: %s[%d] %s' % (trimmed_file, line, message))

    # Insert an #error line into the source file so the build fails if a failure occurs
    # during the codegen process.  This is so that the CI system can fail when someone introduces
    # some change that breaks code generation so it is obvious and can be fixed prior to being
    # checked into the tree.
    #   self            the AutomaticSourceOutputGenerator object
    def outputErrorIfNeeded(self):
        if self.encountered_error:
            write('#error("Encountered error during generation, inserted so CI catches this failure")\n', file=self.outFile)

    # This is the basic copyright for the source file being generated.  It can be
    # overridden by a derived class.
    #   self            the AutomaticSourceOutputGenerator object
    def outputCopywriteHeader(self):
        notice = '// Copyright (c) 2017-2025 The Khronos Group Inc.\n'
        notice += '// Copyright (c) 2017-2019 Valve Corporation\n'
        notice += '// Copyright (c) 2017-2019 LunarG, Inc.\n'
        notice += '//\n'
        notice += '// SPDX-License-Identifier: Apache-2.0\n'
        notice += '//\n'
        notice += '// Licensed under the Apache License, Version 2.0 (the "License");\n'
        notice += '// you may not use this file except in compliance with the License.\n'
        notice += '// You may obtain a copy of the License at\n'
        notice += '//\n'
        notice += '//     http://www.apache.org/licenses/LICENSE-2.0\n'
        notice += '//\n'
        notice += '// Unless required by applicable law or agreed to in writing, software\n'
        notice += '// distributed under the License is distributed on an "AS IS" BASIS,\n'
        notice += '// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n'
        notice += '// See the License for the specific language governing permissions and\n'
        notice += '// limitations under the License.'
        write(notice, file=self.outFile)

    # Called once at the beginning of each run.  Starts writing to the output
    # file, including the header warning and copyright.
    #   self            the AutomaticSourceOutputGenerator object
    #   gen_opts        the AutomaticSourceGeneratorOptions object
    def beginFile(self, genOpts):
        OutputGenerator.beginFile(self, genOpts)
        if self.registry is None:
            raise MissingRegistryError()

        # Iterate over all 'tag' Elements and add the names of all the valid vendor
        # tags to the list
        root = self.registry.tree.getroot()
        for tag in root.findall('tags/tag'):
            self.vendor_tags.append(tag.get('name'))

        # User-supplied prefix text, if any (list of strings)
        if genOpts.prefixText:
            for s in genOpts.prefixText:
                write(s, file=self.outFile)
        self.outputGeneratedHeaderWarning()
        self.outputCopywriteHeader()
        self.outputGeneratedAuthorNote()

    # Write out all the information for the appropriate file,
    # and then call down to the base class to wrap everything up.
    #   self            the AutomaticSourceOutputGenerator object
    def endFile(self):
        self.outputErrorIfNeeded()
        # Finish processing in superclass
        OutputGenerator.endFile(self)

        if self.encountered_error:
            exit(-1)

    # This is called for the beginning of each API feature.  Each feature includes a set of
    # API types, commands, etc.  Each feature is identified by a name, this includes the API
    # core items (as <API>_VERSION_...) or extension items (by the extension name define
    # EXTENSION_NAME_...).
    #   self            the AutomaticSourceOutputGenerator object
    #   interface       element for the <version> / <extension> to generate
    #   emit            actually write to the header only when True (ignored in this class)
    def beginFeature(self, interface, emit):
        # Start processing in superclass
        OutputGenerator.beginFeature(self, interface, emit)

        # Reset the values for the new feature
        self.currentExtension = ''
        self.name_definition = ''
        self.type = interface.get('type')
        self.num_commands = 0
        self.currentExtension = interface.get('name')
        self.required_exts = []

        # Look through all the enums for this XML feature.  If one of them contains
        # a 'name' field, and that 'name' field contains "EXTENSION_NAME", then we're
        # dealing with the beginning of an extension, and not a core group of functionality.
        enums = interface[0].findall('enum')
        for item in enums:
            name_definition = item.get('name')
            if 'EXTENSION_NAME' in name_definition:
                self.name_definition = name_definition
                break

        # If this is not part of an XR core, it requires the extension provided in the
        # feature's name to be enabled for this functionality to be valid.
        if not self.isCoreExtensionName(self.currentExtension):
            if len(self.currentExtension) + 1 > self.max_extension_name_length:
                self.printCodeGenErrorMessage('Extension name %s length %d in XML is greater'
                                              ' than allowable space of %d when null-terminator is added' % (
                                                  self.currentExtension, len(self.currentExtension), self.max_extension_name_length))
            self.required_exts.append(self.currentExtension)
            # Make sure the extension has the proper vendor tags
            valid_extension_vendor = False
            extension_tag = EXTNAME_RE.sub("\\g<tag>", self.currentExtension)
            extension_base_tag = EXTNAME_RE.sub("\\g<base_tag>", self.currentExtension)
            if extension_base_tag in self.vendor_tags or extension_tag in self.vendor_tags:
                valid_extension_vendor = True
                self.current_vendor_tag = extension_tag

            if not valid_extension_vendor:
                self.printCodeGenErrorMessage('Extension %s does not appear to begin with a'
                                              ' valid vendor tag! (for example XR_KHR_)' % self.currentExtension)
                # Set the tag to something invalid so we can explicitly identify each type related to
                # this unknown extension
                self.current_vendor_tag = "UNKNOWN"

        # Add any defined extension dependencies to the base dependency list for this extension
        requires = interface.get('requires')
        if requires:
            self.required_exts.extend(requires.split(','))

    # Called on the completion of an XML feature
    #   self            the AutomaticSourceOutputGenerator object
    def endFeature(self):
        # TODO: Skip Android extensions for now since they don't have a protect clause.
        if 'android' not in self.currentExtension:
            (protect_value, protect_string) = self.genProtectInfo(
                self.featureExtraProtect, None)
            if not self.isCoreExtensionName(self.currentExtension):
                if self.type == 'instance' or self.type == 'system':
                    # Add the completed extension to the list of extensions
                    self.extensions.append(
                        ExtensionData(
                            name=self.currentExtension,
                            vendor_tag=self.current_vendor_tag,
                            type=self.type,
                            protect_value=protect_value,
                            protect_string=protect_string,
                            define=self.name_definition,
                            num_commands=self.num_commands,
                            required_exts=self.required_exts))
                else:
                    self.printCodeGenErrorMessage('Extension %s is neither an instance or system extension but '
                                                  ' an unknown type \"%s\"' % (
                                                      self.currentExtension, self.type))
        OutputGenerator.endFeature(self)

    # Process commands, adding to appropriate dispatch tables
    #   self            the AutomaticSourceOutputGenerator object
    #   cmd_info        the XML information for this command
    #   name            the name of the command
    def genCmd(self, cmd_info, name, alias):
        OutputGenerator.genCmd(self, cmd_info, name, alias)

        # We don't treat cmd aliases any differently
        if alias:
            self.aliases[name] = alias

        self.num_commands = self.num_commands + 1
        self.addCommandToDispatchList(
            self.currentExtension, self.type, name, cmd_info)

    # Process a group of items.  We're typically interested in
    # enumeration values or flag bitmasks.
    #   self            the AutomaticSourceOutputGenerator object
    #   cmd_info        the XML information for this group
    #   name            the name of the group
    def genGroup(self, group_info, name, alias):
        OutputGenerator.genGroup(self, group_info, name, alias)
        if alias:
            self.aliases[name] = alias

        group_type = group_info.elem.get('type')
        group_supported = group_info.elem.get('supported') != 'disabled'
        is_extension = not self.isCoreExtensionName(self.currentExtension)
        # We really only care to handle enum or bitmask values here
        if group_supported and group_type in ('enum', 'bitmask'):
            values = []
            (top_protect_value, top_protect_string) = self.genProtectInfo(
                self.featureExtraProtect, group_info.elem.get('protect'))
            # Both are sorted under "enum" tags.  So grab all the valid
            # field values.
            for elem in group_info.elem.findall('enum'):
                if elem.get('supported') != 'disabled':
                    (enum_protect_value, enum_protect_string) = self.genProtectInfo(
                        self.featureExtraProtect, elem.get('protect'))
                    elem_name = elem.get('name')
                    # TODO this variable is never read
                    elem_name_base = re.sub("X[0-9]*$", "", elem_name)
                    if is_extension and not (elem_name_base.endswith(tuple(self.vendor_tags)) or elem_name.endswith(tuple(self.vendor_tags))):
                        self.printCodeGenErrorMessage('Enum value %s in XML (for extension %s) does'
                                                      ' not end with a suitable vendor tag (such as\"%s\")' % (
                                                          elem_name, self.currentExtension, self.current_vendor_tag))
                    extension_to_check = elem.get('extname', self.currentExtension)
                    alias = elem.get('alias')
                    values.append(
                        EnumBitValue(
                            name=elem_name,
                            protect_value=enum_protect_value,
                            protect_string=enum_protect_string,
                            ext_name=extension_to_check,
                            alias=alias))
            if group_type == 'enum':
                if is_extension and not name.endswith(self.current_vendor_tag):
                    self.printCodeGenErrorMessage('Enum %s in XML (for extension %s) does not end'
                                                  ' with the expected vendor tag \"%s\"' % (
                                                      name, self.currentExtension, self.current_vendor_tag))
                self.api_enums.append(
                    EnumData(
                        name=name,
                        values=values,
                        protect_value=top_protect_value,
                        protect_string=top_protect_string,
                        ext_name=self.currentExtension))
            else:
                if is_extension and not name.endswith(self.current_vendor_tag):
                    self.printCodeGenErrorMessage('Bitmask %s in XML (for extension %s) does not'
                                                  ' end with the expected vendor tag \"%s\"' % (
                                                      name, self.currentExtension, self.current_vendor_tag))
                self.api_bitmasks.append(
                    EnumData(
                        name=name,
                        values=values,
                        protect_value=top_protect_value,
                        protect_string=top_protect_string,
                        ext_name=self.currentExtension))
        # Check all the XrResult values
        if name == 'XrResult':
            for elem in group_info.elem.findall('enum'):
                if elem.get('supported') != 'disabled':
                    (protect_value, protect_string) = self.genProtectInfo(
                        self.featureExtraProtect, elem.get('protect'))
                    item_name = elem.get('name')
                    if item_name is not None:
                        if is_extension and not item_name.endswith(self.current_vendor_tag):
                            self.printCodeGenErrorMessage('XrResult %s in XML (for extension %s) does'
                                                          ' not end with the expected vendor tag \"%s\"' % (
                                                              item_name, self.currentExtension, self.current_vendor_tag))
                        if len(item_name) + 1 > self.max_result_length:
                            self.printCodeGenErrorMessage('XrResult %s length %d in XML is greater'
                                                          ' than allowable space of %d when null-terminator is added' % (
                                                              item_name, len(item_name), self.max_result_length))
                        alias = elem.get('alias')
                        self.api_result_types.append(
                            TypeData(
                                name=item_name,
                                protect_value=protect_value,
                                protect_string=protect_string,
                                alias=alias))
        # Separately save a list of all the API object types we care about
        elif name == 'XrObjectType':
            for elem in group_info.elem.findall('enum'):
                if elem.get('supported') != 'disabled':
                    (protect_value, protect_string) = self.genProtectInfo(
                        self.featureExtraProtect, elem.get('protect'))
                    item_name = elem.get('name')
                    if item_name is not None:
                        alias = elem.get('alias')
                        if is_extension and not item_name.endswith(self.current_vendor_tag):
                            self.printCodeGenErrorMessage('ObjectType %s in XML (for extension %s) does'
                                                          ' not end with the expected vendor tag \"%s\"' % (
                                                              item_name, self.currentExtension, self.current_vendor_tag))
                        self.api_object_types.append(
                            TypeData(
                                name=item_name,
                                protect_value=protect_value,
                                protect_string=protect_string,
                                alias=alias))
        # Separately save a list of all the API structure types we care about
        elif name == 'XrStructureType':
            for elem in group_info.elem.findall('enum'):
                item_name = elem.get('name')
                if elem.get('supported') != 'disabled':
                    (protect_value, protect_string) = self.genProtectInfo(
                        self.featureExtraProtect, elem.get('protect'))
                    if item_name is not None:
                        if is_extension and not item_name.endswith(self.current_vendor_tag):
                            self.printCodeGenErrorMessage('StructureType %s in XML (for extension'
                                                          ' %s) does not end with the expected'
                                                          ' vendor tag \"%s\"' % (
                                                              item_name, self.currentExtension, self.current_vendor_tag))
                        alias = elem.get('alias')
                        if alias:
                            self.aliases[item_name] = alias
                        self.api_structure_types.append(
                            TypeData(
                                name=item_name,
                                protect_value=protect_value,
                                protect_string=protect_string,
                                alias=alias))

    # Retrieve the type and name for a parameter
    #   self            the AutomaticSourceOutputGenerator object
    #   param           the XML parameter information to access
    def getTypeNameTuple(self, param):
        typename = ''
        name = ''
        for elem in param:
            if elem.tag == 'type':
                typename = noneStr(elem.text)
            elif elem.tag == 'name':
                name = noneStr(elem.text)
        return (typename, name)

    # Retrieve the type, name, and enum for a parameter
    #   self            the AutomaticSourceOutputGenerator object
    #   param           the XML parameter information to access
    def getTypeNameEnumTuple(self, param):
        typename = ''
        name = ''
        enum = ''
        for elem in param:
            if elem.tag == 'type':
                typename = noneStr(elem.text)
            elif elem.tag == 'name':
                name = noneStr(elem.text)
            elif elem.tag == 'enum':
                enum = noneStr(elem.text)
        return (typename, name, enum)

    # Override base class genType command so we can grab more information about the
    # necessary types.
    #   self            the AutomaticSourceOutputGenerator object
    #   type_info       the XML information for this type
    #   type_name       the name of this type
    def genType(self, type_info, type_name, alias):
        OutputGenerator.genType(self, type_info, type_name, alias)

        # Skip aliases
        if alias:
            self.aliases[type_name] = alias
            return

        type_elem = type_info.elem
        type_category = type_elem.get('category')
        protect_value, protect_string = self.genProtectInfo(
            self.featureExtraProtect, type_elem.get('protect'))
        extension_to_check = self.currentExtension
        if type_elem.get('extname') is not None:
            extension_to_check = type_elem.get('extname')
        has_proper_ending = True
        if not self.isCoreExtensionName(self.currentExtension) and not type_name.endswith(self.current_vendor_tag):
            has_proper_ending = False
        if type_category in ('struct', 'union'):
            self.genStructUnion(type_info, type_category, type_name, alias)
        elif type_category == 'handle':
            if not has_proper_ending:
                self.printCodeGenErrorMessage('Handle %s in XML (for extension %s) does not end with the expected vendor tag \"%s\"' % (
                    type_name, self.currentExtension, self.current_vendor_tag))
            # Save this if it's a handle so that we can treat it differently.  Things we
            # need handle for are:
            #  - We need to know when it's created and destroyed,
            #  - We need to setup unordered_maps and mutexes to track dispatch tables for each handle
            self.api_handles.append(
                HandleData(
                    name=type_name,
                    parent=self.getHandleParent(type_name),
                    ancestors=self.getHandleAncestors(type_name),
                    protect_value=protect_value,
                    protect_string=protect_string,
                    ext_name=extension_to_check))
        elif type_category == 'basetype':
            # Save the base type information just so we can convert to the base type when
            # outputting to a file.
            basetype_info = self.getTypeNameTuple(type_info.elem)
            base_type = basetype_info[0]
            base_name = basetype_info[1]
            self.api_base_types.append(
                BaseTypeData(
                    type=base_type,
                    name=base_name))
        elif type_category == 'define':
            if "XR_CURRENT_API_VERSION" in type_name:
                # The API Version (most importantly, the API Major and Minor version)
                self.api_version_define = type_name
            elif type_name == 'XR_HEADER_VERSION':
                nameElem = type_elem.find('name')
                # The API Header Version (typically used as the patch or build version)
                self.header_version = noneStr(nameElem.tail).strip()
        elif type_category == 'bitmask':
            mask_info = self.getTypeNameTuple(type_info.elem)
            mask_type = mask_info[0]
            mask_name = mask_info[1]
            bitvalues = type_elem.get('bitvalues')
            # Record a bitmask and all it's valid flag bit values
            self.api_flags.append(
                FlagBits(
                    name=mask_name,
                    type=mask_type,
                    valid_flags=bitvalues,
                    protect_value=protect_value,
                    protect_string=protect_string,
                    ext_name=extension_to_check))

    # Enumerant generation
    #   self            the AutomaticSourceOutputGenerator object
    #   enum_info       the XML information for this enum
    #   name            the name of this enum
    def genEnum(self, enum_info, name, alias):
        OutputGenerator.genEnum(self, enum_info, name, alias)
        if alias:
            self.aliases[name] = alias

        if name == 'XR_MAX_EXTENSION_NAME_SIZE':
            self.max_extension_name_length = int(enum_info.elem.get('value'))
        if name == 'XR_MAX_STRUCTURE_NAME_SIZE':
            self.max_structure_type_length = int(enum_info.elem.get('value'))
        if name == 'XR_MAX_RESULT_STRING_SIZE':
            self.max_result_length = int(enum_info.elem.get('value'))

    # Generate the protection information based on the feature-level and command/type-level
    # protection statements.
    #   self            the AutomaticSourceOutputGenerator object
    #   feature_protect None or the feature's protection statement
    #   local_protect   None or the local type/command's protection statement
    def genProtectInfo(self, feature_protect: Optional[str], local_protect: Optional[str]) -> Tuple[Optional[str], str]:
        protect_type = None
        protect_str = ''
        protect_list = []
        # If the feature adding this struct/union contains a protect message, add it.
        if self.featureExtraProtect:
            protect_type = self.featureExtraProtect
        # If the struct/union itself has a protect message, add it.
        if local_protect:
            if protect_type:
                protect_type += ','
                protect_type += local_protect
            else:
                protect_type = local_protect
        # If any protection was found, let's generate a string to use for the #if statements
        if protect_type:
            if ',' in protect_type:
                protect_list.extend(protect_type.split(","))
                count = 0
                for protect_define in protect_list:
                    if count > 0:
                        protect_str += ' && '
                    protect_str += f'defined({protect_define})'
                    count = count + 1
            else:
                protect_str += f'defined({protect_type})'
        return (protect_type, protect_str)

    def getRelationGroupForBaseStruct(self, type_name):
        """Get the relation group that has the given type_name as the base type.

        Returns None if there is no relation group based on this type."""
        return self._struct_relation_groups.get(type_name)

    def updateArrayLengthsForMember(self, arraylengthsdict, member):
        membername = getElemName(member)
        # Get the array length for this parameter
        lengths = LengthEntry.parse_len_from_param(member)
        if lengths:
            arraylengthsdict.update({length.other_param_name: membername
                                     for length in lengths
                                     if length.other_param_name})
        return lengths

    # Struct/Union member generation.
    # This is a special case of the <type> tag where the contents are interpreted as a set of <member> tags instead of freeform C
    # type declarations. The <member> tags are just like <param> tags - they are a declaration of a struct or union member.
    # Only simple member declarations are supported (no nested structs etc.)
    #   self            the AutomaticSourceOutputGenerator object
    #   type_info       the XML information for this type
    #   type_category   the category of this type.  In this case 'union' or 'struct'.
    #   type_name       the name of this type
    #
    def genStructUnion(self, type_info, type_category, type_name, alias):
        OutputGenerator.genStruct(self, type_info, type_name, alias)
        if self.registry is None:
            raise MissingRegistryError()
        is_union = type_category == 'union'
        (protect_value, protect_string) = self.genProtectInfo(
            self.featureExtraProtect, type_info.elem.get('protect'))
        members = type_info.elem.findall('.//member')
        required_exts = self.required_exts
        if type_info.elem.get('extname'):
            ext_names = type_info.elem.get('extname')
            required_exts.extend(ext_names.split(','))
        returned_only = (type_info.elem.get('returnedonly') == "true")

        # Check if this is a base header.
        if type_name.endswith(f"BaseHeader{self.current_vendor_tag}"):
            # Check if the relation group already existed
            existing_relation_group = self.getRelationGroupForBaseStruct(type_name)
            if existing_relation_group is None:
                # Create with an empty child list
                relation_group = StructRelationGroup(generic_struct_name=type_name,
                                                            child_struct_names=[])
                self._struct_relation_groups[type_name] = relation_group

        # Search through the members to determine all the array lengths
        arraylengths = dict()
        for member in members:
            self.updateArrayLengthsForMember(arraylengths, member)

        # Generate member info
        members_info = []
        for member in members:
            # Get the member's type, enum and name
            member_type, member_name, member_enum = self.getTypeNameEnumTuple(member)

            # Initialize some flags about this member
            static_array_sizes = []
            no_auto_validity = True if member.get(
                'noautovalidity') else False
            is_optional = (member_name == 'next') or (has_any_optional_in_param(member))
            is_handle = self.isHandle(member_type)
            static_array_dim = self.paramIsStaticArray(member)
            is_array = static_array_dim > 0
            array_count_var = ''
            # Determine if this is an array length member
            array_name_for_length = arraylengths.get(member_name, '')
            pointer_count_var = ''
            is_null_terminated = False
            member_values = member.get('values')

            cdecl = self.makeCParamDecl(member, 0)
            is_const = ('const' in cdecl)
            pointer_count = self.paramPointerCount(
                cdecl, member_type, member_name)
            array_dimen = self.paramArrayDimension(
                cdecl, member_type, member_name)

            # If this is a static array, grab the sizes
            if static_array_dim:
                static_array_sizes = self.paramStaticArraySizes(member)

            # If the enum field is there, then this is an array with an enum
            # static size.
            if member_enum:
                is_array = True
                array_count_var = member_enum

            # If this member has a "len" tag, then it is a pointer to an array
            # with a restricted length.

            lengths = LengthEntry.parse_len_from_param(member)
            if lengths is not None:
                is_array = True
                is_null_terminated = any(elt.null_terminated for elt in lengths)
                assert ('null-terminated' in member.get('len')) == is_null_terminated

                # Get the name of the (first) variable to use for the count.
                for length in lengths:
                    if not length.other_param_name:
                        # don't care about constants or "null-terminated"
                        continue
                    pointer_count_var = length.other_param_name
                    break

            # Append this member to the list of current members
            members_info.append(
                MemberOrParam(type=member_type,
                              name=member_name,
                              is_const=is_const,
                              is_handle=is_handle,
                              is_bool=True if 'XrBool' in member_type else False,
                              is_optional=is_optional,
                              no_auto_validity=no_auto_validity,
                              valid_extension_structs=self.registry.validextensionstructs[
                                  type_name] if member_name == 'next' else None,
                              is_array=is_array,
                              is_static_array=(static_array_dim > 0),
                              static_array_sizes=static_array_sizes,
                              array_dimen=array_dimen,
                              array_count_var=array_count_var,
                              array_length_for=array_name_for_length,
                              pointer_count=pointer_count,
                              pointer_count_var=pointer_count_var,
                              is_null_terminated=is_null_terminated,
                              cdecl=cdecl,
                              values=member_values))

        # If this structure/union expands a generic one, save the information and validate that
        # it contains at least the same elements as the generic one.
        if type_info.elem.get('parentstruct'):
            generic_struct_name = type_info.elem.get('parentstruct')
            if self.isStruct(generic_struct_name):
                # First, determine if it is or is not already a relation group
                existing_relation_group = self.getRelationGroupForBaseStruct(generic_struct_name)
                if existing_relation_group is not None:
                    relation_group = existing_relation_group
                else:
                    # Create with an empty child list for now
                    child_list = []
                    relation_group = StructRelationGroup(generic_struct_name=generic_struct_name,
                                                         child_struct_names=child_list)
                    self._struct_relation_groups[generic_struct_name] = relation_group

                # Get the structure information for the group's generic structure
                generic_struct = self.getStruct(generic_struct_name)
                if generic_struct is None:
                    raise RuntimeError(f"Could not find struct: {generic_struct_name}")
                base_member_count = len(generic_struct.members)

                # Second, it must have at least the same number of members
                if len(members) >= base_member_count:
                    # Third, the first 'n' elements must match in type
                    members_match = all(generic_member.type == member.type
                                        for generic_member, member
                                        in zip(generic_struct.members, members_info))
                    if members_match:
                        relation_group.child_struct_names.append(type_name)

        if is_union:
            self.api_unions.append(
                StructUnionData(name=type_name,
                                ext_name=self.currentExtension,
                                required_exts=required_exts,
                                protect_value=protect_value,
                                protect_string=protect_string,
                                returned_only=returned_only,
                                members=members_info))
        else:
            self.api_structures.append(
                StructUnionData(name=type_name,
                                ext_name=self.currentExtension,
                                required_exts=required_exts,
                                protect_value=protect_value,
                                protect_string=protect_string,
                                returned_only=returned_only,
                                members=members_info))

    # Add a command to the appropriate list of commands (core or extension)
    #   self            the AutomaticSourceOutputGenerator object
    #   ext_name        the name of the extension this command is part of
    #   ext_type        the type of the extension (instance/device)
    #   name            the name of the command
    #   cmd_info        the XML information for this command
    def addCommandToDispatchList(self, ext_name, ext_type, name, cmd_info):
        if self.registry is None:
            raise MissingRegistryError()
        cmd_params = []
        required_exts = []
        is_core = True if self.isCoreExtensionName(ext_name) else False
        cmd_has_instance = False
        is_create_connect = False
        is_destroy_disconnect = False
        return_values = []

        # Generate the protection information
        (protect_value, protect_string) = self.genProtectInfo(
            self.featureExtraProtect, cmd_info.elem.find('protect'))

        # Get parameters for this command
        params = cmd_info.elem.findall('param')
        handle_type = self.getTypeNameTuple(params[0])[0]
        handle = self.registry.tree.find(
            f"types/type/[name='{handle_type}'][@category='handle']")
        return_type = cmd_info.elem.find('proto/type')

        # If the return type is void, we really don't have a return type so set it to None
        if return_type is not None and return_type.text == 'void':
            return_type = None

        if not is_core:
            required_exts.append(ext_name)

        # Identify this command as either a create or destroy command for later use
        if any(keyword in name for keyword in ('xrCreate', 'Connect', 'xrTryCreate')):
            is_create_connect = True
        elif 'xrDestroy' in name or 'Disconnect' in name:
            is_destroy_disconnect = True

        # Search through the parameters to determine all the array lengths
        arraylengthparams = dict()
        for param in params:
            self.updateArrayLengthsForMember(arraylengthparams, param)

        # See if this command adjusts any state
        begin_valid_state = cmd_info.elem.get('beginvalidstate')
        begins_state = (begin_valid_state is not None)
        end_valid_state = cmd_info.elem.get('endvalidstate')
        ends_state = (end_valid_state is not None)
        check_valid_state = cmd_info.elem.get('checkvalidstate')
        checks_state = (check_valid_state is not None)

        # This will capture the core return values, but not any added by extension.
        return_values = cmd_info.elem.get('successcodes').split(',')
        return_values += cmd_info.elem.get('errorcodes').split(',')

        # If a beginvalidstate string exists, add it to the list of tracked states
        if begins_state:
            self.addCommandToBeginStates(begin_valid_state, name)
        # If a endvalidstate string exists, add it to the list of tracked states
        if ends_state:
            self.addCommandToEndStates(end_valid_state, name)
        # If a checkvalidstate string exists, add it to the list of tracked states
        if checks_state:
            self.addCommandToCheckStates(check_valid_state, name)

        # Generate a list of commands
        for param in params:
            is_array = self.paramIsStaticArray(param) > 0
            param_cdecl = self.makeCParamDecl(param, 0)
            array_count_var = ''
            pointer_count_var = ''
            is_null_terminated = False
            # Get the basics of the parameter that we need (type and name) and
            # any info about pointers and arrays we can grab.
            paramInfo = self.getTypeNameTuple(param)
            param_type = paramInfo[0]
            param_name = paramInfo[1]
            pointer_count = self.paramPointerCount(
                param_cdecl, param_type, param_name)
            array_dimen = self.paramArrayDimension(
                param_cdecl, param_type, param_name)

            # Determine if this is an array length parameter
            array_name_for_length = arraylengthparams.get(param_name, '')

            # If this is an instance, remember it since we have to treat instance cases
            # special
            if param_type == 'XrInstance':
                cmd_has_instance = True

            # Determine if this is a pointer array with a length variable
            lengths = LengthEntry.parse_len_from_param(param)
            if lengths:
                is_array = any(not elt.null_terminated for elt in lengths)
                is_null_terminated = any(elt.null_terminated for elt in lengths)
                # assert is_null_terminated == new_is_null_terminated
                # Get the name of the (first) variable to use for the count.
                length_params = tuple(length.other_param_name
                                      for length in lengths
                                      # don't care about constants or "null-terminated"
                                      if length.other_param_name)
                if length_params:
                    pointer_count_var = ','.join(length_params).replace('::', '->')

            # If this is a handle, and it is a pointer, it really must also be an array unless it is a create command
            if self.isHandle(param_type) and pointer_count > 0 and not (
                    is_create_connect or is_array or self.paramIsStaticArray(param) or array_count_var or pointer_count_var):
                self.printCodeGenErrorMessage('OpenXR command %s has parameter %s which is a non-array pointer to a handle and is not a create command' % (
                    name, param_name))

            # Add the command parameter to the list
            cmd_params.append(
                MemberOrParam(
                    type=param_type,
                    name=param_name,
                    is_const=('const' in param_cdecl.strip().lower()),
                    is_handle=self.isHandle(param_type),
                    is_bool=('XrBool' in paramInfo[0]),
                    is_optional=has_any_optional_in_param(param),
                    no_auto_validity=(param.get('noautovalidity') is not None),
                    is_array=is_array,
                    is_static_array=self.paramIsStaticArray(param) > 0,
                    static_array_sizes=self.paramStaticArraySizes(
                        param) if self.paramIsStaticArray(param) else None,
                    array_dimen=array_dimen,
                    array_count_var=array_count_var,
                    array_length_for=array_name_for_length,
                    pointer_count=pointer_count,
                    pointer_count_var=pointer_count_var,
                    is_null_terminated=is_null_terminated,
                    valid_extension_structs=None,
                    cdecl=param_cdecl,
                    values=None))

        # If this is a create or destroy that returns a handle, it must have a return type.
        is_create_or_destroy = (is_create_connect or is_destroy_disconnect)
        is_last_param_handle = cmd_params[-1].is_handle
        returns_result = (return_type is not None) and (return_type.text == 'XrResult')
        if is_create_or_destroy and is_last_param_handle and not returns_result:
            self.printCodeGenErrorMessage(
                f'OpenXR create/destroy command {name} requires an XrResult return value')

        # The Core OpenXR code will be wrapped in a feature called XR_VERSION_#_#
        # For example: XR_VERSION_1_0 wraps the core 1.0 OpenXR functionality
        if is_core:
            core_command_type = 'instance'
            if handle is not None and handle_type != 'XrInstance':
                core_command_type = 'device'
            self.core_commands.append(
                CommandData(name=name,
                            is_create_connect=is_create_connect,
                            is_destroy_disconnect=is_destroy_disconnect,
                            ext_name=ext_name,
                            ext_type=core_command_type,
                            required_exts=required_exts,
                            protect_value=protect_value,
                            protect_string=protect_string,
                            return_type=return_type,
                            return_values=return_values,
                            handle=handle,
                            handle_type=handle_type,
                            has_instance=cmd_has_instance,
                            params=cmd_params,
                            begins_state=begins_state,
                            ends_state=ends_state,
                            checks_state=checks_state,
                            cdecl=self.makeCDecls(cmd_info.elem)[0]))
        else:
            self.ext_commands.append(
                CommandData(name=name,
                            is_create_connect=is_create_connect,
                            is_destroy_disconnect=is_destroy_disconnect,
                            ext_name=ext_name,
                            ext_type=ext_type,
                            required_exts=required_exts,
                            protect_value=protect_value,
                            protect_string=protect_string,
                            return_type=return_type,
                            return_values=return_values,
                            handle=handle,
                            handle_type=handle_type,
                            has_instance=cmd_has_instance,
                            params=cmd_params,
                            begins_state=begins_state,
                            ends_state=ends_state,
                            checks_state=checks_state,
                            cdecl=self.makeCDecls(cmd_info.elem)[0]))

    def findState(self, state):
        for api_state in self.api_states:
            if api_state.state == state:
                return api_state
        return None

    # Add this command to any begin state checks (used for validation)
    #   self              the AutomaticSourceOutputGenerator object
    #   comma_list_states a comma-delimited string containing the list
    #                     of states that need to be begun for the
    #                     current command
    #   command           the name of the command
    def addCommandToBeginStates(self, comma_list_states, command):
        state_list = comma_list_states.split(",")
        # Loop through each state individually
        for cur_state in state_list:
            found_state = self.findState(cur_state)
            # If not found, create a new one
            if found_state is None:
                # Split the state name into the "type" and the "variable"
                # after the first underscore ('_')
                split_state_name = cur_state.split('_', 1)
                state_type = split_state_name[0]
                state_variable = split_state_name[1]
                # Create a new begin state command list with this command
                # and create empty lists for the rest
                begin_list = []
                end_list = []
                check_list = []
                begin_list.append(command)
                # Create and append the new state
                self.api_states.append(
                    ApiState(state=cur_state,
                             type=state_type,
                             variable=state_variable,
                             begin_commands=begin_list,
                             end_commands=end_list,
                             check_commands=check_list))
            else:
                # Found, so just add a new begin command
                found_state.begin_commands.append(command)

    # Add this command to any end state checks (used for validation)
    #   self              the AutomaticSourceOutputGenerator object
    #   comma_list_states a comma-delimited string containing the list
    #                     of states that need to end for the
    #                     current command
    #   command           the name of the command
    def addCommandToEndStates(self, comma_list_states, command):
        state_list = comma_list_states.split(",")
        # Loop through each state individually
        for cur_state in state_list:
            found_state = self.findState(cur_state)
            # If not found, create a new one
            if found_state is None:
                # Split the state name into the "type" and the "variable"
                # after the first underscore ('_')
                split_state_name = cur_state.split('_', 1)
                state_type = split_state_name[0]
                state_variable = split_state_name[1]
                # Create a new end state command list with this command
                # and create empty lists for the rest
                begin_list = []
                end_list = []
                check_list = []
                end_list.append(command)
                # Create and append the new state
                self.api_states.append(
                    ApiState(state=cur_state,
                             type=state_type,
                             variable=state_variable,
                             begin_commands=begin_list,
                             end_commands=end_list,
                             check_commands=check_list))
            else:
                # Found, so just add a new end command
                found_state.end_commands.append(command)

    # Add this command to any state checks (used for validation)
    #   self              the AutomaticSourceOutputGenerator object
    #   comma_list_states a comma-delimited string containing the list
    #                     of states that need to be checked for the
    #                     current command
    #   command           the name of the command
    def addCommandToCheckStates(self, comma_list_states, command):
        state_list = comma_list_states.split(",")
        # Loop through each state individually
        for cur_state in state_list:
            found_state = self.findState(cur_state)
            # If not found, create a new one
            if found_state is None:
                # Split the state name into the "type" and the "variable"
                # after the first underscore ('_')
                split_state_name = cur_state.split('_', 1)
                state_type = split_state_name[0]
                state_variable = split_state_name[1]
                # Create a new check state command list with this command
                # and create empty lists for the rest
                begin_list = []
                end_list = []
                check_list = []
                check_list.append(command)
                # Create and append the new state
                self.api_states.append(
                    ApiState(state=cur_state,
                             type=state_type,
                             variable=state_variable,
                             begin_commands=begin_list,
                             end_commands=end_list,
                             check_commands=check_list))
            else:
                # Found, so just add a new check command
                found_state.check_commands.append(command)

    # Check if the parameter passed in is a static array
    #   self            the AutomaticSourceOutputGenerator object
    #   param           the XML information for the param
    def paramIsStaticArray(self, param) -> int:
        is_static_array = 0
        param_name = param.find('name')
        if param_name.tail and ('[' in param_name.tail):
            is_static_array = param_name.tail.count('[')
        return is_static_array

    # Determine the array dimension and the size of each.  Only support
    # up to two dimensions for now
    #   self            the AutomaticSourceOutputGenerator object
    #   param           the XML information for the param
    def paramStaticArraySizes(self, param):
        static_array_sizes = []
        param_name = param.find('name')
        param_enum = param.find('enum')
        if not param_name.tail or '[' not in param_name.tail:
            return static_array_sizes

        static_array_dimen = param_name.tail.count('[')
        if static_array_dimen == 0:
            return static_array_sizes
        if static_array_dimen == 1 and param_enum is not None:
            static_array_sizes.append(param_enum.text)
        else:
            tail_str = param_name.tail
            while (tail_str.count('[') > 0):
                tail_str = tail_str.replace('[', '', 1)
                tail_str = tail_str.replace(']', '', 1)

                if tail_str:
                    cur_size_str = ''
                    if tail_str.count('[') > 0:
                        cur_size_str = tail_str[0:tail_str.find('[')]
                    else:
                        cur_size_str = tail_str
                    static_array_sizes.append(cur_size_str)
        return static_array_sizes

    # Check if the parameter passed in is optional
    # Returns True, False, or a list of Boolean values for comma separated len attributes (len='false,true')
    #   self            the AutomaticSourceOutputGenerator object
    #   param           the XML information for the param
    def paramIsOptional(self, param) -> Union[bool, List[bool]]:
        optional = parse_optional_from_param(param)
        if len(optional) == 1:
            return optional[0]
        return optional

    # Check if the parameter passed in is a pointer
    #   self            the AutomaticSourceOutputGenerator object
    #   param_cdecl     the entire cdecl for the parameter
    #   param_type      the API type of the parameter
    #   param_name      the name of the parameter
    def paramPointerCount(self, param_cdecl, param_type, param_name):
        pointer_count = 0
        type_start = param_cdecl.find(param_type)
        name_start = param_cdecl.find(param_name)
        past_type_string = param_cdecl[type_start + len(param_type):name_start]
        pointer_count = past_type_string.count('*')
        if param_type[:4] == 'PFN_':
            pointer_count = pointer_count + 1
        return pointer_count

    # Check if the parameter passed in is an array
    #   self            the AutomaticSourceOutputGenerator object
    #   param_cdecl     the entire cdecl for the parameter
    #   param_type      the API type of the parameter
    #   param_name      the name of the parameter
    def paramArrayDimension(self, param_cdecl, param_type, param_name):
        array_dimen = 0
        type_start = param_cdecl.find(param_type)
        name_start = param_cdecl.find(param_name)
        past_type_string = param_cdecl[type_start + len(param_type):name_start]
        array_dimen = past_type_string.count('[')
        return array_dimen

    # Determine if the provided name is really part of the API core
    #   self            the AutomaticSourceOutputGenerator object
    #   ext_name        the name of the "extension" to determine if it really is core
    def isCoreExtensionName(self, ext_name):
        if "XR_VERSION_" in ext_name:
            return True
        if "XR_LOADER_VERSION_" in ext_name:
            return True
        return False

    # Determine if all the characters in a string are upper-case
    #   self            the AutomaticSourceOutputGenerator object
    #   check_str       string to check for all uppercase letters
    def isAllUpperCase(self, check_str):
        for c in check_str:
            if c.isalpha() and c.islower():
                return False
        return True

    # Determine if all the characters in a string are numbers
    #   self            the AutomaticSourceOutputGenerator object
    #   check_str       string to check for all numbers
    def isAllNumbers(self, check_str):
        for c in check_str:
            if not c.isdigit():
                return False
        return True

    # Is this an enum type?
    #   self            the AutomaticSourceOutputGenerator object
    #   type_name       the name of the type to check
    def isEnumType(self, type_name):
        type_name = self.resolve_type_name_alias(type_name)
        for enum_tuple in self.api_enums:
            if type_name == enum_tuple.name:
                return True
        return False

    # Is this type a flag?
    #   self            the AutomaticSourceOutputGenerator object
    #   type_name       the name of the type to check
    def isFlagType(self, type_name):
        type_name = self.resolve_type_name_alias(type_name)
        for flag_tuple in self.api_flags:
            if type_name == flag_tuple.name:
                return True
        return False

    # This flag is defined to a particular set of bitfields, so can be non-zero.
    #   self            the AutomaticSourceOutputGenerator object
    #   type_name       the name of the type to check
    def flagHasValidValues(self, type_name):
        for flag_tuple in self.api_flags:
            if type_name == flag_tuple.name:
                if flag_tuple.valid_flags:
                    return True
                break
        return False

    # Is this an OpenXR handle?
    #   self            the AutomaticSourceOutputGenerator object
    #   type_name       the name of the type to check
    def isHandle(self, type_name):
        type_name = self.resolve_type_name_alias(type_name)
        for handle_tuple in self.api_handles:
            if type_name == handle_tuple.name:
                return True
        return False

    # Return the handle if it is one, None otherwise
    #   self            the AutomaticSourceOutputGenerator object
    #   type_name       the name of the type to check
    def getHandle(self, type_name):
        type_name = self.resolve_type_name_alias(type_name)
        for handle_tuple in self.api_handles:
            if type_name == handle_tuple.name:
                return handle_tuple
        return None

    # Is this an OpenXR type defined by XR_DEFINE_OPAQUE_64?
    #   self            the AutomaticSourceOutputGenerator object
    #   type_name       the name of the type to check
    def isOpaque64(self, type_name):
        return type_name == "XR_DEFINE_OPAQUE_64"

    # Gets the name of a handle parameter (dereferenced if a pointer,
    # the first element thereof if an array),
    # or none if not a handle.
    #   self            the ValidationSourceOutputGenerator object
    #   param           a MemberParam object
    def getFirstHandleName(self, param):
        if not self.isHandle(param.type):
            return None
        name = param.name
        assert param.pointer_count <= 1
        if param.pointer_count == 1:
            if param.pointer_count_var is None:
                # Just a pointer
                name = f"(*{name})"
            else:
                name += '[0]'
        return name

    # Get the type without any pointer/const modifiers
    #   self            the AutomaticSourceOutputGenerator object
    #   type_cdecl      the type cdecl to extract the basic type from
    def getRawType(self, type_cdecl):
        basic_type = type_cdecl
        # For now, get rid of any constant qualifiers or extra spaces or stars
        # so we can just get the parameter name.
        basic_type.replace("const", "")
        basic_type.replace("*", "")
        basic_type.replace("&", "")
        basic_type.replace(" ", "")

        # See if the type returned is actually another base type and replace it
        # with what the base type is.
        base_type = self.getBaseType(basic_type)
        if base_type:
            basic_type = self.getRawType(base_type.type)
        return basic_type

    # Utility function to determine if a type is a structure
    #   self        the AutomaticSourceOutputGenerator object
    #   type_name   the name of the type to check
    def isStruct(self, type_name):
        is_struct = False
        type_name = self.resolve_type_name_alias(type_name)
        for cur_struct in self.api_structures:
            if cur_struct.name == type_name:
                is_struct = True
                break
        return is_struct

    # Utility function to determine if a type is a structure with handles
    #   self            the AutomaticSourceOutputGenerator object
    #   type_name       the name of the type to check
    def isStructWithHandles(self, type_name):
        struct = None
        type_name = self.resolve_type_name_alias(type_name)
        for cur_struct in self.api_structures:
            if cur_struct.name == type_name:
                for member in cur_struct.members:
                    if member.is_handle:
                        struct = cur_struct
                        break
                break
        return struct

    # Utility function to determine if a type is a structure
    #   self            the AutomaticSourceOutputGenerator object
    #   type_name       the name of the type to get the structure for
    def getStruct(self, type_name):
        ret_struct = None
        type_name = self.resolve_type_name_alias(type_name)
        for cur_struct in self.api_structures:
            if cur_struct.name == type_name:
                ret_struct = cur_struct
                break
        return ret_struct

    # Utility function to determine if a type is a union
    #   self            the AutomaticSourceOutputGenerator object
    #   type_name       the name of the type to check
    def isUnion(self, type_name):
        is_union = False
        type_name = self.resolve_type_name_alias(type_name)
        for cur_union in self.api_unions:
            if cur_union.name == type_name:
                is_union = True
                break
        return is_union

    # Utility function to determine if a type is a union
    #   self            the AutomaticSourceOutputGenerator object
    #   type_name       the name of the type to retrieve the union for
    def getUnion(self, type_name):
        ret_union = None
        type_name = self.resolve_type_name_alias(type_name)
        for cur_union in self.api_unions:
            if cur_union.name == type_name:
                ret_union = cur_union
                break
        return ret_union

    # Utility function to determine if a type is a basetype
    def getBaseType(self, name):
        ret_base = None
        for base_type in self.api_base_types:
            if base_type.name == name:
                ret_base = base_type
                break
        return ret_base

    # Generate a XrObjectType based on a handle name
    #   self            the AutomaticSourceOutputGenerator object
    #   handle_name     the name of the type to convert to the XrObjectType enum
    def genXrObjectType(self, handle_name):
        # Find any place where a lowercase letter is followed by an uppercase
        # letter, and insert an underscore between them
        value = re.sub('([a-z0-9])([A-Z])', r'\1_\2', handle_name)
        # Change the whole string to uppercase
        value = value.upper()
        # Add "OBJECT_TYPE_" after the XR_ prefix
        object_type_name = re.sub('XR_', 'XR_OBJECT_TYPE_', value, 1)
        invalid_type = True
        for object_type in self.api_object_types:
            if object_type.name == object_type_name:
                invalid_type = False
                break
        if invalid_type:
            self.printCodeGenErrorMessage('Generated XrObjectType %s for handle %s does not exist!' % (
                object_type_name, handle_name))
        return object_type_name

    # Generate a handle name based on a XrObjectType
    #   self            the AutomaticSourceOutputGenerator object
    #   object_type     the XrObjectType enum to convert to a handle name
    def genXrHandleName(self, object_type):
        if 'XR_OBJECT_TYPE_' not in object_type:
            return ''
        cur_string = object_type[15:].lower()
        while '_' in cur_string:
            value_index = cur_string.index('_')
            new_string = ''
            if value_index > 0:
                new_string = cur_string[:value_index]
            new_char = cur_string[value_index + 1].upper()
            new_string += new_char
            new_string += cur_string[value_index + 2:]
            cur_string = new_string
        handle_name = 'Xr'
        handle_name += cur_string
        # Now loop through and find real string.  Case of extension
        # on end may be different, so we'll use the real structure
        # name
        for handle in self.api_handles:
            if handle.name.lower() == handle_name.lower():
                return handle.name
        self.printCodeGenErrorMessage(
            f'Generated handle {handle_name} for XrObjectType {object_type} does not exist!')
        return ''

    # Generate a XrStructureType based on a structure typename
    #   self            the AutomaticSourceOutputGenerator object
    #   type_name       the name of the type to convert to the XrStructureType enum
    def genXrStructureType(self, type_name):
        return self.conventions.generate_structure_type_from_name(type_name)

    # Generate a structure typename based on a XrStructureType
    #   self            the AutomaticSourceOutputGenerator object
    #   type_name       the XrStructureType enum to convert to a structure name
    def genXrStructureName(self, type_name):
        if 'XR_TYPE' not in type_name:
            return ''
        if type_name == 'XR_TYPE_UNKNOWN':
            return ''
        # Alias substitution, if possible.
        type_name = self.aliases.get(type_name, type_name)
        cur_string = type_name[7:].lower()
        while '_' in cur_string:
            value_index = cur_string.index('_')
            new_string = ''
            if value_index > 0:
                new_string = cur_string[:value_index]
            new_char = cur_string[value_index + 1].upper()
            new_string += new_char
            new_string += cur_string[value_index + 2:]
            cur_string = new_string
        struct_name = 'Xr'
        struct_name += cur_string
        # Now loop through and find real string.  Case of extension
        # on end may be different, so we'll use the real structure
        # name
        for cur_struct in self.api_structures:
            if cur_struct.name.lower() == struct_name.lower():
                return cur_struct.name
        self.printCodeGenErrorMessage('Generated structure name %s for XrStructureType %s does not exist!' % (
            struct_name, type_name))
        return ''

    # Write the indent for the appropriate number of tab spaces
    #   self            the AutomaticSourceOutputGenerator object
    #   indent_cnt      the number of indents to return a string of
    def writeIndent(self, indent_cnt):
        return '    ' * indent_cnt

    def resolve_type_name_alias(self, type_name):
        """
        Recursively find the root type name using aliases.

        Will return type_name itself if no alias exists.
        Not actually a recursive call. Will raise an error if a cycle is detected.
        """
        result = type_name

        # Keep a set of all type names visited to detect cycles.
        seen = set()
        while True:
            seen.add(result)
            alias = self.aliases.get(result)
            if alias in seen:
                raise RuntimeError(f"Cycle in type name aliases starting at {type_name}")
            if not alias:
                # no more to resolve
                break
            result = alias

        return result


class CurrentExtensionTracker:
    """Helps insert comments between extensions/core versions."""

    def __init__(self, api_version_prefix: str):
        """Initialize state."""
        self.api_version_prefix = api_version_prefix
        self.current_extension = ''

    def format_if_extension_changed(self,
                                    ext_name: str,
                                    fmt_str: str,
                                    no_change_str: str = "") -> str:
        """
        Format a string with version or extension info if the extension changed.

        Supply a format string with a single {} placeholder in it.

        Return your choice of string, empty string by default, if the extension
        did not change.
        """
        if ext_name == self.current_extension:
            # no change - empty string.
            return no_change_str

        # Update current extension
        self.current_extension = ext_name

        if ext_name.startswith(self.api_version_prefix):
            # core!
            ver = ext_name[len(self.api_version_prefix):].replace("_", ".")
            desc = f"Core {ver}"
        else:
            desc = f"{ext_name} extension"
        return fmt_str.format(desc)
