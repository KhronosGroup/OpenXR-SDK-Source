#!/usr/bin/env python3 -i
#
# Copyright 2013-2025 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

from collections import OrderedDict, namedtuple
from enum import Enum
from functools import reduce
from pathlib import Path

from generator import OutputGenerator, write
from spec_tools.attributes import (ExternSyncEntry, LengthEntry,
                                   has_any_optional_in_param,
                                   parse_optional_from_param)
from spec_tools.conventions import ProseListFormats as plf
from spec_tools.data_structures import DictOfStringSets
from spec_tools.util import (findNamedElem, findTypedElem, getElemName,
                             getElemType)
from spec_tools.validity import ValidityCollection, ValidityEntry


class UnhandledCaseError(NotImplementedError):
    def __init__(self, msg=None):
        if msg:
            super().__init__(f"Got a case in the validity generator that we have not explicitly handled: {msg}")
        else:
            super().__init__('Got a case in the validity generator that we have not explicitly handled.')


def _genericIterateIntersection(a, b):
    """Iterate through all elements in a that are also in b.

    Somewhat like a set's intersection(),
    but not type-specific so it can work with OrderedDicts, etc.
    It also returns a generator instead of a set,
    so you can pick a preferred container type,
    if any.
    """
    return (x for x in a if x in b)


def _make_ordered_dict(gen):
    """Make an ordered dict (with None as the values) from a generator."""
    return OrderedDict(((x, None) for x in gen))


def _orderedDictIntersection(a, b):
    return _make_ordered_dict(_genericIterateIntersection(a, b))


def _genericIsDisjoint(a, b):
    """Return true if nothing in a is also in b.

    Like a set's is_disjoint(),
    but not type-specific so it can work with OrderedDicts, etc.
    """
    for _ in _genericIterateIntersection(a, b):
        return False
    # if we never enter the loop...
    return True


_WCHAR = "wchar_t"
_CHAR = "char"
_CHARACTER_TYPES = {_CHAR, _WCHAR}


class StateRelationship(Enum):
    # For commands that begin a state
    BEGIN = 0
    # For commands that end a state
    END = 1
    # For commands that require being in a state without changing state
    CHECK = 2


STATE_ATTRIBUTES = {
    StateRelationship.BEGIN: 'beginvalidstate',
    StateRelationship.END: 'endvalidstate',
    StateRelationship.CHECK: 'checkvalidstate',
}


class StateCollection:
    """Stores associations between states and commands."""

    def __init__(self):
        # Mapping from states to commands
        self.states_to_commands = [DictOfStringSets(), DictOfStringSets(), DictOfStringSets()]

        # Mapping from commands to states
        self.commands_to_states = [DictOfStringSets(), DictOfStringSets(), DictOfStringSets()]

    def get_commands(self, state_name, relationship: StateRelationship):
        return self.states_to_commands[relationship.value].get(state_name)

    def get_states(self, command_name, relationship: StateRelationship):
        return self.commands_to_states[relationship.value].get(command_name)

    def add_command(self, command_name, state_name, relationship: StateRelationship):
        self.states_to_commands[relationship.value].add(state_name, command_name)
        self.commands_to_states[relationship.value].add(command_name, state_name)


class ValidityOutputGenerator(OutputGenerator):
    """ValidityOutputGenerator - subclass of OutputGenerator.

    Generates AsciiDoc includes of valid usage information, for reference
    pages and the specification. Similar to DocOutputGenerator.

    ---- methods ----
    ValidityOutputGenerator(errFile, warnFile, diagFile) - args as for
    OutputGenerator. Defines additional internal state.
    ---- methods overriding base class ----
    beginFile(genOpts)
    endFile()
    beginFeature(interface, emit)
    endFeature()
    genCmd(cmdinfo)
    """

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.currentExtension = ''

        # Tracks whether we are tracing operations
        self.trace = False

    @property
    def null(self):
        """Preferred spelling of NULL.

        Delegates to the object implementing ConventionsBase.
        """
        return self.conventions.null

    @property
    def structtype_member_name(self):
        """Return name of the structure type member.

        Delegates to the object implementing ConventionsBase.
        """
        return self.conventions.structtype_member_name

    @property
    def nextpointer_member_name(self):
        """Return name of the structure pointer chain member.

        Delegates to the object implementing ConventionsBase.
        """
        return self.conventions.nextpointer_member_name

    def makeProseList(self, elements, fmt=plf.AND,
                      comma_for_two_elts=False, *args, **kwargs):
        """Make a (comma-separated) list for use in prose.

        Adds a connective (by default, 'and')
        before the last element if there are more than 1.

        Optionally adds a quantifier (like 'any') before a list of 2 or more,
        if specified by fmt.

        Delegates to the object implementing ConventionsBase.
        """
        if not elements:
            raise ValueError(
                'Cannot pass an empty list if you are trying to make a prose list.')
        return self.conventions.makeProseList(elements,
                                              fmt,
                                              with_verb=False,
                                              comma_for_two_elts=comma_for_two_elts,
                                              *args, **kwargs)

    def makeProseListIs(self, elements, fmt=plf.AND,
                        comma_for_two_elts=False, *args, **kwargs):
        """Make a (comma-separated) list for use in prose, followed by either 'is' or 'are' as appropriate.

        Adds a connective (by default, 'and')
        before the last element if there are more than 1.

        Optionally adds a quantifier (like 'any') before a list of 2 or more,
        if specified by fmt.

        Delegates to the object implementing ConventionsBase.
        """
        if not elements:
            raise ValueError(
                'Cannot pass an empty list if you are trying to make a prose list.')
        return self.conventions.makeProseList(elements,
                                              fmt,
                                              with_verb=True,
                                              comma_for_two_elts=comma_for_two_elts,
                                              *args, **kwargs)

    def makeValidityCollection(self, entity_name):
        """Create a ValidityCollection object, passing along our Conventions."""
        return ValidityCollection(entity_name, self.conventions)

    def populateStructChildren(self):
        """Populate self.struct_children."""
        assert self.registry
        self.struct_children = DictOfStringSets()
        d = self.struct_children
        for name, t in self.registry.typedict.items():
            d.add_key(name)
            parentstruct = t.elem.get('parentstruct')
            if parentstruct is not None:
                d.add(parentstruct, name)

    def populateStates(self):
        """Populate self.states."""
        assert self.registry
        # OpenXR-specific
        self.states = StateCollection()
        for name, cmdinfo in self.registry.cmddict.items():
            if not cmdinfo.required:
                continue
            self.recordStateRelationship(cmdinfo.elem, name, StateRelationship.BEGIN)
            self.recordStateRelationship(cmdinfo.elem, name, StateRelationship.END)
            self.recordStateRelationship(cmdinfo.elem, name, StateRelationship.CHECK)

    def beginFile(self, genOpts):
        if not genOpts.conventions:
            raise RuntimeError(
                'Must specify conventions object to generator options')
        self.conventions = genOpts.conventions
        # Vulkan says 'must: be a valid pointer' a lot, OpenXR just says
        # 'must: be a pointer'.
        self.valid_pointer_text = ' '.join(
            (x for x in (self.conventions.valid_pointer_prefix, 'pointer') if x))

        # OpenXR-only
        self.populateStructChildren()
        self.populateStates()

        OutputGenerator.beginFile(self, genOpts)

    def generateStateValidity(self, validity, command_name):
        begins_states = self.states.get_states(command_name, StateRelationship.BEGIN)
        if begins_states:
            for state_name in sorted(begins_states):
                end_commands = self.states.get_commands(state_name, StateRelationship.END)
                assert end_commands

                entry = ValidityEntry(anchor=(state_name, 'beginstate'))
                entry += f'flink:{command_name} must: not be called more than once without first successfully calling '
                entry += self.makeProseList(sorted(end_commands), fmt=plf.OR)
                validity += entry

        ends_states = self.states.get_states(command_name, StateRelationship.END)
        if ends_states:
            for state_name in sorted(ends_states):
                begin_commands = self.states.get_commands(state_name, StateRelationship.BEGIN)
                assert begin_commands

                entry = ValidityEntry(anchor=(state_name, 'beginstate'))
                entry += f'flink:{command_name} must: only be called after a successful call to '
                entry += self.makeProseList(sorted(begin_commands), fmt=plf.OR)
                validity += entry

        checks_states = self.states.get_states(command_name, StateRelationship.CHECK)
        if checks_states:
            for state_name in sorted(checks_states):
                begin_commands = self.states.get_commands(state_name, StateRelationship.BEGIN)
                end_commands = self.states.get_commands(state_name, StateRelationship.END)
                assert begin_commands
                assert end_commands

                begin_command_list = '/'.join(f'flink:{command}'
                                              for command in begin_commands)
                end_command_list = '/'.join(f'flink:{command}'
                                            for command in end_commands)

                entry = ValidityEntry(anchor=(state_name, 'checkstate'))
                entry += 'flink:{} must: only be called between successful calls to '.format(
                    command_name)
                entry += begin_command_list
                entry += ' and '
                entry += end_command_list
                validity += entry

    def endFile(self):
        OutputGenerator.endFile(self)

    def beginFeature(self, interface, emit):
        # Start processing in superclass
        OutputGenerator.beginFeature(self, interface, emit)
        self.currentExtension = interface.get('name')

    def endFeature(self):
        # Finish processing in superclass
        OutputGenerator.endFeature(self)

    @property
    def struct_macro(self):
        """Get the appropriate format macro for a structure."""
        # delegate to conventions
        return self.conventions.struct_macro

    def makeStructName(self, name):
        """Prepend the appropriate format macro for a structure to a structure type name."""
        # delegate to conventions
        return self.conventions.makeStructName(name)

    def makeParameterName(self, name):
        """Prepend the appropriate format macro for a parameter/member to a parameter name."""
        return f"pname:{name}"

    def makeBaseTypeName(self, name):
        """Prepend the appropriate format macro for a 'base type' to a type name."""
        return f"basetype:{name}"

    def makeEnumerationName(self, name):
        """Prepend the appropriate format macro for an enumeration type to a enum type name."""
        return f"elink:{name}"

    def makeFlagsName(self, name):
        """Prepend the appropriate format macro for a flags type to a flags type name."""
        return 'elink:' + name

    def makeFuncPointerName(self, name):
        """Prepend the appropriate format macro for a function pointer type to a type name."""
        return f"tlink:{name}"

    def makeExternalTypeName(self, name):
        """Prepend the appropriate format macro for an external type like uint32_t to a type name."""
        # delegate to conventions
        return self.conventions.makeExternalTypeName(name)

    def makeEnumerantName(self, name):
        """Prepend the appropriate format macro for an enumerate (value) to a enum value name."""
        return f"ename:{name}"

    def writeInclude(self, directory, basename, validity: ValidityCollection,
                     threadsafety, commandpropertiesentry=None,
                     successcodes=None, errorcodes=None):
        """Generate an include file.

        directory - subdirectory to put file in (absolute or relative pathname)
        basename - base name of the file
        validity - ValidityCollection to write.
        threadsafety - List (may be empty) of thread safety statements to write.
        successcodes - Optional success codes to document.
        errorcodes - Optional error codes to document.
        """
        assert self.genOpts
        # Create subdirectory, if needed
        directory = Path(directory)
        if not directory.is_absolute():
            directory = Path(self.genOpts.directory) / directory
        self.makeDir(directory)

        # Create validity file
        filename = directory / (f"{basename}{self.conventions.file_suffix}")
        self.logMsg('diag', '# Generating include file:', str(filename))

        with open(filename, 'w', encoding='utf-8') as fp:
            write(self.conventions.warning_comment, file=fp)

            # Valid Usage
            if validity:
                write('.Valid Usage (Implicit)', file=fp)
                write('****', file=fp)
                write(validity, file=fp, end='')
                write('****', file=fp)
                write('', file=fp)

            # Host Synchronization
            if threadsafety:
                # The heading of this block differs between projects, so an Asciidoc attribute is used.
                write('.{externsynctitle}', file=fp)
                write('****', file=fp)
                write(threadsafety, file=fp, end='')
                write('****', file=fp)
                write('', file=fp)

            # Command Properties - contained within a block, to avoid table numbering
            if commandpropertiesentry:
                write('.Command Properties', file=fp)
                write('****', file=fp)
                write('[options="header", width="100%"]', file=fp)
                write('|====', file=fp)
                write(self.makeCommandPropertiesTableHeader(), file=fp)
                write(commandpropertiesentry, file=fp)
                write('|====', file=fp)
                write('****', file=fp)
                write('', file=fp)

            # Success Codes - contained within a block, to avoid table numbering
            if successcodes or errorcodes:
                write('.Return Codes', file=fp)
                write('****', file=fp)
                if successcodes:
                    write('ifndef::doctype-manpage[]', file=fp)
                    write('<<fundamentals-successcodes,Success>>::', file=fp)
                    write('endif::doctype-manpage[]', file=fp)
                    write('ifdef::doctype-manpage[]', file=fp)
                    write('On success, this command returns::', file=fp)
                    write('endif::doctype-manpage[]', file=fp)
                    write(successcodes, file=fp)
                if errorcodes:
                    write('ifndef::doctype-manpage[]', file=fp)
                    write('<<fundamentals-errorcodes,Failure>>::', file=fp)
                    write('endif::doctype-manpage[]', file=fp)
                    write('ifdef::doctype-manpage[]', file=fp)
                    write('On failure, this command returns::', file=fp)
                    write('endif::doctype-manpage[]', file=fp)
                    write(errorcodes, file=fp)
                else:  # no errorcodes
                    write('ifndef::doctype-manpage[]', file=fp)
                    write('<<fundamentals-errorcodes,Failure>>::', file=fp)
                    write('None', file=fp)
                    write('endif::doctype-manpage[]', file=fp)
                    write('ifdef::doctype-manpage[]', file=fp)
                    write('This command does not return any failure codes::', file=fp)
                    write('endif::doctype-manpage[]', file=fp)
                write('****', file=fp)
                write('', file=fp)

    def paramIsStaticArray(self, param):
        """Check if the parameter passed in is a static array."""
        tail = param.find('name').tail
        return tail and tail[0] == '['

    def paramIsConst(self, param):
        """Check if the parameter passed in has a type that mentions const."""
        return param.text is not None and 'const' in param.text

    def staticArrayLength(self, param):
        """Get the length of a parameter that has been identified as a static array."""
        paramenumsize = param.find('enum')
        if paramenumsize is not None:
            return self.makeEnumerantName(paramenumsize.text)

        return param.find('name').tail[1:-1]

    def getHandleDispatchableAncestors(self, typename):
        """Get the ancestors of a handle object."""
        ancestors = []
        current = typename
        while True:
            current = self.getHandleParent(current)
            if current is None:
                return ancestors
            if self.isHandleTypeDispatchable(current):
                ancestors.append(current)

    def isHandleTypeDispatchable(self, handlename):
        """Check if a parent object is dispatchable or not."""
        assert self.registry
        handle = self.registry.tree.find(
            f"types/type/[name='{handlename}'][@category='handle']")
        if handle is not None and getElemType(handle) == 'VK_DEFINE_HANDLE':
            return True
        else:
            return False

    def isHandleOptional(self, param, params):
        # Simple, if it is optional, return true
        if has_any_optional_in_param(param):
            return True

        # If no validity is being generated, it usually means that validity is complex and not absolute, so say yes.
        if param.get('noautovalidity') is not None:
            return True

        # If the parameter is an array and we have not already returned, find out if any of the len parameters are optional
        if self.paramIsArray(param):
            lengths = LengthEntry.parse_len_from_param(param)
            assert lengths is not None
            for length in lengths:
                if not length.other_param_name:
                    # do not care about constants or "null-terminated"
                    continue

                other_param = findNamedElem(params, length.other_param_name)
                if other_param is None:
                    self.logMsg('warn', length.other_param_name,
                                'is listed as a length for parameter', param, 'but no such parameter exists')
                if other_param and has_any_optional_in_param(other_param):
                    return True

        return False

    def makeOptionalPre(self, param):
        # Do not generate this stub for bitflags
        param_name = getElemName(param)
        paramtype = getElemType(param)
        type_category = self.getTypeCategory(paramtype)
        is_optional = param.get('optional').split(',')[0] == 'true'
        if type_category != 'bitmask' and is_optional:
            if self.paramIsArray(param) or self.paramIsPointer(param):
                optional_val = self.null
            elif type_category == 'handle':
                if self.isHandleTypeDispatchable(paramtype):
                    optional_val = self.null
                else:
                    optional_val = 'dlink:' + self.conventions.api_prefix + 'NULL_HANDLE'
            else:
                optional_val = self.conventions.zero
            return 'If {} is not {}, '.format(
                self.makeParameterName(param_name),
                optional_val)

        return ""

    def makeParamValidityPre(self, param, params, selector=None):
        """Make the start of an entry for a parameter's validity, including a chunk of text if it is an array."""
        param_name = getElemName(param)
        paramtype = getElemType(param)

        # General pre-amble. Check optionality and add stuff.
        entry = ValidityEntry(anchor=(param_name, 'parameter'))

        optional = parse_optional_from_param(param)
        is_optional = optional[0]

        # This is for a union member, and the valid member is chosen by an enum selection
        if selector:
            selection = param.get('selection')

            entry += 'If {} is {}, '.format(
                self.makeParameterName(selector),
                self.makeEnumerantName(selection))

            if is_optional:
                entry += "and "
                optionalpre = self.makeOptionalPre(param)
                entry += optionalpre[0].lower() + optionalpre[1:]

            return entry

        if self.paramIsStaticArray(param):
            if paramtype not in _CHARACTER_TYPES:
                entry += 'Any given element of '
            return entry

        if self.paramIsArray(param) and param.get('len') != LengthEntry.NULL_TERMINATED_STRING:
            # Find all the parameters that are called out as optional,
            # so we can document that they might be zero, and the array may be ignored
            optionallengths = []
            lengths = LengthEntry.parse_len_from_param(param)
            assert lengths is not None
            for length in lengths:
                if not length.other_param_name:
                    # Only care about length entries that are parameter names
                    continue

                other_param = findNamedElem(params, length.other_param_name)
                other_param_optional = has_any_optional_in_param(other_param)

                if other_param is None or not other_param_optional:
                    # Do not care about not-found params or non-optional params
                    continue

                if self.paramIsPointer(other_param):
                    optionallengths.append(
                        f"the value referenced by {self.makeParameterName(length.other_param_name)}")
                else:
                    optionallengths.append(
                        self.makeParameterName(length.other_param_name))

            # Document that these arrays may be ignored if any of the length values are 0
            if optionallengths or is_optional:
                entry += 'If '
            if optionallengths:
                entry += self.makeProseListIs(optionallengths, fmt=plf.OR)
                entry += f' not {self.conventions.zero}, '
            # TODO enabling this in OpenXR, as used in Vulkan, causes nonsensical things like
            # "If pname:propertyCapacityInput is not `0`, and pname:properties is not `NULL`, pname:properties must: be a pointer to an array of pname:propertyCapacityInput slink:XrApiLayerProperties structures"
            # if optionallengths and is_optional:
            #     entry += 'and '
            # if is_optional:
            #     entry += self.makeParameterName(param_name)
            #     # TODO switch when cosmetic changes OK
            #     # entry += ' is not {}, '.format(self.null)
            #     entry += ' is not `NULL`, '

            return entry

        if any(optional):
            entry += self.makeOptionalPre(param)
            return entry

        # If none of the early returns happened, we at least return an empty
        # entry with an anchor.
        return entry

    def createValidationLineForParameterImpl(self, blockname, param, params, typetext,
                                             see_also=None, selector=None, parentname=None):
        """Make the generic validity portion used for all parameters.

        May return None if nothing to validate.
        """
        if param.get('noautovalidity') is not None:
            return None

        validity = self.makeValidityCollection(blockname)
        param_name = getElemName(param)
        paramtype = getElemType(param)

        entry = self.makeParamValidityPre(param, params, selector)

        # pAllocator is not supported in VulkanSC and must always be NULL
        if self.conventions.xml_api_name == "vulkansc" and param_name == 'pAllocator' and paramtype == 'VkAllocationCallbacks':
            entry = ValidityEntry(anchor=(param_name, 'null'))
            entry += 'pname:pAllocator must: be `NULL`'
            return entry

        # This is for a child member of a union
        if selector:
            entry += 'the {} member of {} must: be '.format(self.makeParameterName(param_name), self.makeParameterName(parentname))
        else:
            entry += f'{self.makeParameterName(param_name)} must: be '

        def add_see_also(entry):
            if see_also:
                entry += '. '
                entry += see_also

        optional = parse_optional_from_param(param)
        if self.paramIsStaticArray(param) and paramtype in _CHARACTER_TYPES:
            # TODO this is a minor hack to determine if this is a command parameter or a struct member
            if self.paramIsConst(param) or blockname.startswith(self.conventions.type_prefix):
                if paramtype != _CHAR:
                    raise UnhandledCaseError("input arrays of wchar_t are not yet handled")
                entry += 'a null-terminated UTF-8 string whose length is less than or equal to '
                entry += self.staticArrayLength(param)
            else:
                # This is a command's output parameter
                entry += 'a '
                if paramtype == _WCHAR:
                    entry += "wide "
                entry += f'character array of length {self.staticArrayLength(param)} '
            add_see_also(entry)
            validity += entry
            return validity

        elif self.paramIsArray(param):
            # Arrays. These are hard to get right, apparently

            lengths = LengthEntry.parse_len_from_param(param)
            if lengths is None:
                raise RuntimeError("Logic error: decided this was an array but there is no len attribute")

            for i, length in enumerate(lengths):
                if i == 0:
                    # If the first index, make it singular.
                    entry += 'a '
                    array_text = 'an array'
                    pointer_text = self.valid_pointer_text
                else:
                    array_text = 'arrays'
                    pointer_text = f"{self.valid_pointer_text}s"

                if length.null_terminated:
                    # This should always be the last thing.
                    # If it ever is not for some bizarre reason, then this
                    # will need some massaging.
                    entry += 'null-terminated '
                elif length.number == 1:
                    entry += pointer_text
                    entry += ' to '
                else:
                    entry += pointer_text
                    entry += ' to '
                    entry += array_text
                    entry += ' of '
                    # Handle equations, which are currently denoted with latex
                    if length.math:
                        # Handle equations, which are currently denoted with latex
                        entry += str(length)
                    else:
                        entry += self.makeParameterName(str(length))
                    entry += ' '

            # Void pointers do not actually point at anything - remove the word "to"
            if paramtype == 'void':
                if lengths[-1].number == 1:
                    if len(lengths) > 1:
                        # Take care of the extra s added by the post array chunk function. #HACK#
                        entry.drop_end(5)
                    else:
                        entry.drop_end(4)

                    # This has not been hit, so this has not been tested recently.
                    raise UnhandledCaseError("Got void pointer param/member with last length 1")
                else:
                    # An array of void values is a byte array.
                    entry += 'byte'

            elif paramtype == _CHAR:
                # A null terminated array of chars is a string
                if lengths[-1].null_terminated:
                    entry += 'UTF-8 string'
                else:
                    # Else it is just a bunch of chars
                    entry += 'char value'

            elif self.paramIsConst(param):
                # If a value is "const" that means it will not get modified,
                # so it must be valid going into the function.
                if 'const' in param.text:

                    if not self.isStructAlwaysValid(paramtype):
                        entry += 'valid '

            # Check if the array elements are optional
            array_element_optional = len(optional) == len(lengths) + 1 \
                and optional[-1]
            if array_element_optional and self.getTypeCategory(paramtype) != 'bitmask':  # bitmask is handled later
                entry += f"or dlink:{self.conventions.api_prefix}NULL_HANDLE "

            entry += typetext

            # pluralize
            if len(lengths) > 1 or (lengths[0] != 1 and not lengths[0].null_terminated):
                entry += 's'
            add_see_also(entry)

            return self.handleRequiredBitmask(blockname, param, paramtype, entry)

        if self.paramIsPointer(param):
            # Handle pointers - which are really special case arrays (i.e. they do not have a length)
            # TODO  should do something here if someone ever uses some intricate comma-separated `optional`
            pointercount = param.find('type').tail.count('*')

            # Treat void* as an int
            if paramtype == 'void':
                optional = parse_optional_from_param(param)
                # If there is only void*, it is just optional int - we do not need any language.
                if pointercount == 1 and optional[0]:
                    return None  # early return
                # Treat the inner-most void* as an int
                pointercount -= 1

            # Could be multi-level pointers (e.g. ppData - pointer to a pointer). Handle that.
            entry += 'a '
            entry += f"{self.valid_pointer_text} to a " * pointercount

            # Handle void* and pointers to it
            if paramtype == 'void':
                if not optional[pointercount]:
                    typetext = 'pointer value'
                else:
                    # The old conditional added "pointer value" if optional was not specified (which implies "false"),
                    # or if optional[pointercount] was truthy, which just means non-empty string.
                    # old comment:
                    # The last void* is just optional int (e.g. to be filled by the impl.)
                    raise UnhandledCaseError("Conditional previously was mixed")

            # If a value is "const" that means it will not get modified, so
            # it must be valid going into the function.
            elif self.paramIsConst(param) and paramtype != 'void':
                entry += 'valid '

            entry += typetext
            add_see_also(entry)
            return self.handleRequiredBitmask(blockname, param, paramtype, entry)

        # Add additional line for non-optional bitmasks
        if self.getTypeCategory(paramtype) == 'bitmask':
            # TODO does not really handle if someone tries something like optional="true,false"
            # TODO OpenXR has 0 or a valid combination of flags, for optional things.
            # Vulkan does not...
            mandatory = not optional[0]
            if not mandatory:
                entry += f'{self.conventions.zero} or '
            # Non-pointer, non-optional things must be valid
            entry += f'a valid {typetext}'

            return self.handleRequiredBitmask(blockname, param, paramtype, entry)

        # Non-pointer, non-optional things must be valid
        entry += f'a valid {typetext}'
        add_see_also(entry)
        return entry

    def handleRequiredBitmask(self, blockname, param, paramtype, entry):
        # TODO does not really handle if someone tries something like optional="true,false"
        optional = parse_optional_from_param(param)
        if self.getTypeCategory(paramtype) != 'bitmask' or \
                any(optional):
            return entry
        if self.paramIsPointer(param) and not self.paramIsArray(param):
            # This is presumably an output parameter
            return entry

        param_name = getElemName(param)
        # If mandatory, then we need two entries instead of just one.
        validity = self.makeValidityCollection(blockname)
        validity += entry

        entry2 = ValidityEntry(anchor=(param_name, 'requiredbitmask'))
        if self.paramIsArray(param):
            entry2 += 'Each element of '
        entry2 += f'{self.makeParameterName(param_name)} must: not be {self.conventions.zero}'
        validity += entry2
        return validity

    def isBaseHeaderType(self, typename):
        """Returns true if the type is a struct that is a "base header" type."""
        assert self.registry
        info = self.registry.typedict.get(typename)
        if not info:
            return False
        members = info.getMembers()
        type_member = findNamedElem(members, self.structtype_member_name)
        if type_member is None:
            return False

        # If we have a type member without specified values, it is a base header.
        return type_member.get('values') is None

    def createValidationLineForParameter(self, blockname, param, params, typecategory,
                                         selector=None, parentname=None):
        """Make an entire validation entry for a given parameter."""
        assert self.registry
        param_name = getElemName(param)
        paramtype = getElemType(param)
        see_also = None
        is_array = self.paramIsArray(param)
        is_pointer = self.paramIsPointer(param)
        needs_recursive_validity = (is_array
                                    or is_pointer
                                    or not self.isStructAlwaysValid(paramtype))
        typetext = None
        if paramtype in ('void', _CHAR):
            # Chars and void are special cases - we call the impl function,
            # but do not use the typetext.
            # A null-terminated char array is a string, else it is chars.
            # An array of void values is a byte array, a void pointer is just a pointer to nothing in particular
            typetext = ''

        elif typecategory == 'bitmask':
            bitsname = paramtype.replace('Flags', 'FlagBits')
            bitselem = self.registry.tree.find(f"enums[@name='{bitsname}']")

            # If bitsname is an alias, then use the alias to get bitselem.
            typeElem = self.registry.lookupElementInfo(bitsname, self.registry.typedict)
            if typeElem is not None:
                alias = self.registry.getAlias(typeElem.elem, self.registry.typedict)
                if alias is not None:
                    bitselem = self.registry.tree.find("enums[@name='" + alias + "']")

            if bitselem is None or len(bitselem.findall('enum[@required="true"]')) == 0:
                # Empty bit mask: presumably just a placeholder (or only in
                # an extension not enabled for this build)

                entry = ValidityEntry(
                    anchor=(param_name, 'zerobitmask'))
                entry += self.makeParameterName(param_name)
                entry += ' must: be '
                entry += self.conventions.zero
                # Early return
                return entry

            is_const = self.paramIsConst(param)

            if is_array:
                if is_const:
                    # input an array of bitmask values
                    template = 'combinations of {bitsname} value'
                else:
                    template = '{paramtype} value'
            elif is_pointer:
                if is_const:
                    template = 'combination of {bitsname} values'
                else:
                    template = '{paramtype} value'
            else:
                template = 'combination of {bitsname} values'

            # The above few cases all use makeEnumerationName, just with different context.
            typetext = template.format(
                bitsname=self.makeEnumerationName(bitsname),
                paramtype=self.makeEnumerationName(paramtype))

        elif typecategory == 'handle':
            typetext = f'{self.makeStructName(paramtype)} handle'

        elif typecategory == 'enum':
            typetext = f'{self.makeEnumerationName(paramtype)} value'

        elif typecategory == 'funcpointer':
            typetext = f'{self.makeFuncPointerName(paramtype)} value'

        elif typecategory == 'struct':
            if needs_recursive_validity:
                if self.isBaseHeaderType(paramtype):
                    typetext = f'{self.makeStructName(paramtype)}-based structure'
                    child_structs = self.keepOnlyRequired(self.struct_children.get(paramtype, []),
                                                          self.registry.typedict)
                    if child_structs:
                        see_also = f"See also: {', '.join(self.makeStructName(t) for t in sorted(child_structs))}"
                else:
                    typetext = f'{self.makeStructName(paramtype)} structure'

        elif typecategory == 'union':
            if needs_recursive_validity:
                typetext = f'{self.makeStructName(paramtype)} union'

        elif self.paramIsArray(param) or self.paramIsPointer(param):
            if typecategory == 'basetype':
                typetext = f'{self.makeBaseTypeName(paramtype)} value'
            elif typecategory is None:
                typetext = f'{self.makeExternalTypeName(paramtype)} value'
            else:
                raise UnhandledCaseError()

        elif typecategory is None:
            if not self.isStructAlwaysValid(paramtype):
                typetext = f'{self.makeExternalTypeName(paramtype)} value'

            # "a valid uint32_t value" does not make much sense.
            pass

        # If any of the above conditions matched and set typetext,
        # we call using it.
        if typetext is not None:
            return self.createValidationLineForParameterImpl(
                blockname, param, params, typetext,
                see_also=see_also, selector=selector, parentname=parentname)
        return None

    def makeHandleValidityParent(self, param, params):
        """Make a validity entry for a handle's parent object.

        Creates 'parent' VUID.
        """
        param_name = getElemName(param)
        paramtype = getElemType(param)

        # Iterate up the handle parent hierarchy for the first parameter of
        # a parent type.
        # This enables cases where a more distant ancestor is present, such
        # as VkDevice and VkCommandBuffer (but no direct parent
        # VkCommandPool).

        while True:
            # If we run out of ancestors, give up
            handleparent = self.getHandleParent(paramtype)
            if handleparent is None:
                if self.trace:
                    print(f'makeHandleValidityParent:{param_name} has no handle parent, skipping')
                return None

            # Look for a parameter of the ancestor type
            otherparam = findTypedElem(params, handleparent)
            if otherparam is not None:
                break

            # Continue up the hierarchy
            paramtype = handleparent

        parent_name = getElemName(otherparam)
        entry = ValidityEntry(anchor=(param_name, 'parent'))

        is_optional = self.isHandleOptional(param, params)

        if self.paramIsArray(param):
            template = 'Each element of {}'
            if is_optional:
                template += ' that is a valid handle'
        elif is_optional:
            template = 'If {} is a valid handle, it'
        else:
            # not optional, not an array. Just say the parameter name.
            template = '{}'

        entry += template.format(self.makeParameterName(param_name))

        entry += ' must: have been created, allocated, or retrieved from {}'.format(
            self.makeParameterName(parent_name))

        return entry

    def getUnnamedCommonParentOfParams(self, input_handles):
        """Return the nearest common ancestor handle type for all handle parameters given."""
        if not input_handles or len(input_handles) < 2:
            return None

        # Get a list (well, an OrderedDict pretending to be a set)
        # of the full ancestor chain for each input handle.
        ancestor_map = {
            h: _make_ordered_dict(self.iterateHandleAncestors(getElemType(h)))
            for h in input_handles
        }

        # all handle types supplied to this function
        handle_types = set(getElemType(h) for h in input_handles)

        # Only consider the handles without any ancestors also being
        # supplied to this function.
        top_level_handles = set(h for h in input_handles
                                if _genericIsDisjoint(ancestor_map[h], handle_types))

        # Remove all non-top-level-handles from further consideration
        for removal in set(ancestor_map.keys()).difference(top_level_handles):
            del (ancestor_map[removal])

        if len(ancestor_map) <= 1:
            # If only one top level,
            # it has no other top level to share an unnamed parent with.
            return None

        # Find intersection (ordered!) of all ancestor lists.
        # all_ancestor_lists = ancestor_map.values()
        common_ancestors = reduce(_orderedDictIntersection,
                                  ancestor_map.values())

        # The first common ancestor should be the nearest one.
        # We already ensured we have at least one.
        common_ancestor = next(iter(common_ancestors), None)
        return common_ancestor

    def makeHandlesValidityCommonAncestor(self, handles, params):
        """Make an validity entry for a common ancestor between handles, not explicitly passed as a param.

        Only handles parent validity for signatures taking multiple handles
        without any ancestors also being supplied to the function.
        (e.g. "Each of x, y, and z must: come from the same slink:ParentHandle")
        See self.makeHandleValidityParent() for instances where the parent
        handle is named and also passed.

        Creates 'commonparent' VUID.
        """
        if not handles or len(handles) < 2:
            return None

        # Get just the inputs: pointers to handles are outputs,
        # unless they're const (then they're probably an input array).
        # Inputs are the only ones we generate this sort of validity for.

        def is_input_handle(h):
            # NOTE: having paramIsPointer before the "or" messes up formatters...
            return (self.paramIsConst(h)
                    or not self.paramIsPointer(h))

        input_handles = [h
                         for h in handles
                         if is_input_handle(h)]
        common_ancestor = self.getUnnamedCommonParentOfParams(input_handles)
        if common_ancestor is None:
            # No common ancestor to validate
            return None

        parameter_texts = []
        for param in input_handles:
            pname = self.makeParameterName(getElemName(param))
            if self.paramIsArray(param):
                parameter_texts.append(f'the elements of {pname}')
            else:
                parameter_texts.append(pname)

        entry = ValidityEntry(anchor=('commonparent',))
        entry += self.makeProseList(sorted(parameter_texts), fmt=plf.EACH_AND)

        if any((self.isHandleOptional(h, params) for h in input_handles)):
            entry += ' that are valid handles'

        entry += ' must: have been created, allocated, or retrieved from the same '
        entry += self.makeStructName(common_ancestor)

        return entry

    def makeStructureTypeFromName(self, structname):
        """Create text for a structure type name, like ename:XR_TYPE_CREATE_INSTANCE_INFO"""
        return self.makeEnumerantName(self.conventions.generate_structure_type_from_name(structname))

    def makeStructureTypeValidity(self, structname):
        """Generate a validity line for the type value of a struct.

        Creates VUID named like the member name.
        """
        assert self.registry
        info = self.registry.typedict.get(structname)
        assert info is not None

        # If this fails (meaning we have something other than a struct in here),
        # then the caller is wrong:
        # probably passing the wrong value for structname.
        members = info.getMembers()
        assert members

        # If this fails, see caller: this should only get called for a struct type with a type value.
        param = findNamedElem(members, self.structtype_member_name)
        # OpenXR gets some structs without a type field in here, so cannot assert
        # assert param is not None
        if param is None:
            return None

        entry = ValidityEntry(
            anchor=(self.structtype_member_name, self.structtype_member_name))
        entry += self.makeParameterName(self.structtype_member_name)
        entry += ' must: be '

        values = param.get('values', '').split(',')
        if values:
            values = self.findRequiredEnums(values)

        child_structs = self.keepOnlyRequired(self.struct_children.get(structname, []),
                                              self.registry.typedict)
        if child_structs:
            if values:
                print(f'The struct: {structname} has children, it may not have a "values" attribute itself.')
            assert not values
            if len(child_structs) > 1:
                entry += 'one of the following XrStructureType values: '
            entry += ', '.join(self.makeStructureTypeFromName(child)
                               for child in sorted(child_structs))
            return entry

        if values:
            # Extract each enumerant value. They could be validated in the
            # same fashion as validextensionstructs in
            # makeStructureExtensionPointer, although that is not relevant in
            # the current extension struct model.
            entry += self.makeProseList((self.makeEnumerantName(v)
                                         for v in values), plf.OR)
            return entry

        if 'Base' in structname:
            # This type does not even have any values for its type, and it
            # seems like it might be a base struct that we would expect to
            # lack its own type, so omit the entire statement
            return None

        self.logMsg('warn', 'No values were marked-up for the structure type member of',
                    structname, 'so making one up!')
        entry += self.makeStructureTypeFromName(structname)

        return entry

    def makeStructureExtensionPointer(self, blockname, param):
        """Generate a validity line for the pointer chain member value of a struct."""
        assert self.registry
        param_name = getElemName(param)

        if param.get('validextensionstructs') is not None:
            self.logMsg('warn', blockname,
                        'validextensionstructs is deprecated/removed', '\n')

        entry = ValidityEntry(
            anchor=(param_name, self.nextpointer_member_name))
        validextensionstructs = self.registry.validextensionstructs.get(
            blockname)
        extensionstructs = []

        if validextensionstructs is not None:
            # Check each structure name and skip it if not required by the
            # generator. This allows tagging extension structs in the XML
            # that are only included in validity when needed for the spec
            # being targeted.
            for struct in validextensionstructs:
                # Unpleasantly breaks encapsulation. Should be a method in the registry class
                t = self.registry.lookupElementInfo(
                    struct, self.registry.typedict)
                if t is None:
                    self.logMsg('warn', 'makeStructureExtensionPointer: struct', struct,
                                'is in a validextensionstructs= attribute but is not in the registry')
                elif t.required:
                    extensionstructs.append(f"slink:{struct}")
                else:
                    self.logMsg(
                        'diag', 'makeStructureExtensionPointer: struct', struct, 'IS NOT required')

        link = "link:{uri-next-chain}[next structure in a structure chain]"
        entry += f'{self.makeParameterName(param_name)} must: be {self.null} or a valid pointer to the {link}'
        if not extensionstructs:
            return entry
        entry += '. See also: '

        entry += ', '.join(extensionstructs)
        return entry

    def addSharedStructMemberValidity(self, struct, blockname, param, validity):
        """Generate language to independently validate a parameter, for those validated even in output.

        Return value indicates whether it was handled internally (True) or if it may need more validity (False)."""
        param_name = getElemName(param)
        paramtype = getElemType(param)
        if param.get('noautovalidity') is None:

            if self.conventions.is_structure_type_member(paramtype, param_name):
                validity += self.makeStructureTypeValidity(blockname)
                return True

            if self.conventions.is_nextpointer_member(paramtype, param_name):
                # Vulkan: make the addition of validity here conditional.
                # if struct.get('structextends') is None:
                #     validity += self.makeStructureExtensionPointer(blockname, param)
                validity += self.makeStructureExtensionPointer(blockname, param)
                return True
        return False

    def makeOutputOnlyStructValidity(self, cmd, blockname, params):
        """Generate all the valid usage information for a struct that is entirely output.

        That is, it is only ever filled out by the implementation other than
        the structure type and pointer chain members.
        Thus, we only create validity for the pointer chain member.
        """
        # Start the validity collection for this struct
        validity = self.makeValidityCollection(blockname)

        for param in params:
            self.addSharedStructMemberValidity(
                cmd, blockname, param, validity)

        return validity

    def makeStructOrCommandValidity(self, cmd, blockname, params):
        """Generate all the valid usage information for a given struct or command."""
        assert self.registry
        validity = self.makeValidityCollection(blockname)
        handles = []
        arraylengths = dict()
        for param in params:
            param_name = getElemName(param)
            paramtype = getElemType(param)

            # Valid usage ID tags (VUID) are generated for various
            # conditions based on the name of the block (structure or
            # command), name of the element (member or parameter), and type
            # of VU statement.

            # Get the type's category
            typecategory = self.getTypeCategory(paramtype)

            if not self.addSharedStructMemberValidity(
                    cmd, blockname, param, validity):
                selector = param.get('selector')
                if not selector:
                    validity += self.createValidationLineForParameter(
                        blockname, param, params, typecategory)
                else:
                    if typecategory != 'union':
                        self.logMsg('warn', 'selector attribute set on non-union parameter', param_name, 'in', blockname)

                    paraminfo = self.registry.lookupElementInfo(paramtype, self.registry.typedict)

                    for member in paraminfo.getMembers():
                        membertype = getElemType(member)
                        membertypecategory = self.getTypeCategory(membertype)

                        validity += self.createValidationLineForParameter(
                            blockname, member, paraminfo.getMembers(), membertypecategory,
                            selector=selector, parentname=param_name)

            # Ensure that any parenting is properly validated, and list that a handle was found
            if typecategory == 'handle':
                handles.append(param)

            # Get the array length for this parameter
            lengths = LengthEntry.parse_len_from_param(param)
            if lengths:
                arraylengths.update({length.other_param_name: length
                                     for length in lengths
                                     if length.other_param_name})

        # Any non-optional arraylengths should specify they must be greater than 0
        array_length_params = ((param, getElemName(param))
                               for param in params
                               if getElemName(param) in arraylengths)

        for param, param_name in array_length_params:
            if has_any_optional_in_param(param):
                continue

            length = arraylengths[param_name]
            full_length = length.full_reference

            # Is this just a name of a param? If false, then it is some kind
            # of qualified name (a member of a param for instance)
            simple_param_reference = (len(length.param_ref_parts) == 1)
            if not simple_param_reference:
                # Loop through to see if any parameters in the chain are optional
                array_length_parent = cmd
                array_length_optional = False
                for part in length.param_ref_parts:
                    # Overwrite the param so it ends up as the bottom level parameter for later checks
                    param = array_length_parent.find("*/[name='{}']".format(part))

                    # If any parameter in the chain is optional, skip the implicit length requirement
                    array_length_optional |= (param.get('optional') is not None)

                    # Lookup the type of the parameter for the next loop iteration
                    type = param.findtext('type')
                    array_length_parent = self.registry.tree.find("./types/type/[@name='{}']".format(type))

                if array_length_optional:
                    continue

            # Get all the array dependencies
            arrays = cmd.findall(
                f"param/[@len='{full_length}'][@optional='true']")

            # Get all the optional array dependencies, including those not generating validity for some reason
            optionalarrays = arrays + \
                cmd.findall(
                    f"param/[@len='{full_length}'][@noautovalidity='true']")

            entry = ValidityEntry(anchor=(full_length, 'arraylength'))
            # Allow lengths to be arbitrary if all their dependents are optional
            if optionalarrays and len(optionalarrays) == len(arrays):
                entry += 'If '
                optional_array_names = (self.makeParameterName(getElemName(array))
                                        for array in optionalarrays)
                entry += self.makeProseListIs(optional_array_names,
                                              fmt=plf.ANY_OR)

                entry += f' not {self.null}, '

            if self.paramIsPointer(param):
                entry += 'the value referenced by '

            # Split and re-join here to insert pname: around ::
            entry += length.get_human_readable(make_param_name=self.makeParameterName)
            entry += ' must: be greater than '
            entry += self.conventions.zero
            validity += entry

        # Find the parents of all objects referenced in this command
        for param in handles:
            # Do not detect a parent for return values!
            if not self.paramIsPointer(param) or self.paramIsConst(param):
                validity += self.makeHandleValidityParent(param, params)

        # Find the common ancestor of all objects referenced in this command
        validity += self.makeHandlesValidityCommonAncestor(handles, params)

        return validity

    def makeThreadSafetyBlock(self, cmd, paramtext):
        """Generate thread-safety validity entries for cmd/structure"""
        # See also makeThreadSafetyBlock in validitygenerator.py
        validity = self.makeValidityCollection(getElemName(cmd))

        # This text varies between projects, so an Asciidoctor attribute is used.
        extsync_prefix = "{externsyncprefix} "

        # Find and add any parameters that are thread unsafe
        explicitexternsyncparams = cmd.findall(f"{paramtext}[@externsync]")
        if explicitexternsyncparams is not None:
            for param in explicitexternsyncparams:
                externsyncattribs = ExternSyncEntry.parse_externsync_from_param(param)
                assert externsyncattribs
                param_name = getElemName(param)

                for attrib in externsyncattribs:
                    entry = ValidityEntry()
                    entry += extsync_prefix
                    if attrib.entirely_extern_sync:
                        if self.paramIsArray(param):
                            entry += 'each member of '
                        elif self.paramIsPointer(param):
                            entry += 'the object referenced by '

                        entry += self.makeParameterName(param_name)

                        if attrib.children_extern_sync:
                            entry += ', and any child handles,'

                    else:
                        entry += attrib.get_human_readable(make_param_name=self.makeParameterName)
                    entry += ' must: be externally synchronized'
                    validity += entry

        # Vulkan-specific
        # For any vkCmd* functions, the command pool is externally synchronized
        if cmd.find('proto/name') is not None and 'vkCmd' in cmd.find('proto/name').text:
            entry = ValidityEntry()
            entry += extsync_prefix
            entry += 'the sname:VkCommandPool that pname:commandBuffer was allocated from must: be externally synchronized'
            validity += entry

        # Find and add any "implicit" parameters that are thread unsafe
        implicitexternsyncparams = cmd.find('implicitexternsyncparams')
        if implicitexternsyncparams is not None:
            for elem in implicitexternsyncparams:
                entry = ValidityEntry()
                entry += extsync_prefix
                entry += elem.text
                entry += ' must: be externally synchronized'
                validity += entry

        return validity

    def makeCommandPropertiesTableHeader(self):
        header  = '|<<VkCommandBufferLevel,Command Buffer Levels>>'
        header += '|<<vkCmdBeginRenderPass,Render Pass Scope>>'
        if self.videocodingRequired():
            header += '|<<vkCmdBeginVideoCodingKHR,Video Coding Scope>>'
        header += '|<<VkQueueFlagBits,Supported Queue Types>>'
        header += '|<<fundamentals-queueoperation-command-types,Command Type>>'
        return header

    def makeCommandPropertiesTableEntry(self, cmd, name):
        cmdbufferlevel, renderpass, videocoding, queues, tasks = None, None, None, None, None

        if 'vkCmd' in name:
            # Must be called in primary/secondary command buffers appropriately
            cmdbufferlevel = cmd.get('cmdbufferlevel')
            cmdbufferlevel = (' + \n').join(cmdbufferlevel.title().split(','))

            # Must be called inside/outside a render pass appropriately
            renderpass = cmd.get('renderpass')
            renderpass = renderpass.capitalize()

            # Must be called inside/outside a video coding scope appropriately
            if self.videocodingRequired():
                videocoding = self.getVideocoding(cmd).capitalize()

            #
            # This test for vkCmdFillBuffer is a hack, since we have no path
            # to conditionally have queues enabled or disabled by an extension.
            # As the VU stuff is all moving out (hopefully soon), this hack solves the issue for now
            if name == 'vkCmdFillBuffer':
                if self.isVKVersion11() or 'VK_KHR_maintenance1' in self.registry.requiredextensions:
                    queues = [ 'transfer', 'graphics', 'compute' ]
                else:
                    queues = [ 'graphics', 'compute' ]
            else:
                queues = self.getQueueList(cmd)
            queues = (' + \n').join([queue.title() for queue in queues])

            tasks = cmd.get('tasks')
            tasks = (' + \n').join(tasks.title().split(','))
        elif 'vkQueue' in name:
            # For queue commands there are no command buffer level, render
            # pass, or video coding scope specific restrictions,
            # or command type, but the queue types are considered
            cmdbufferlevel = '-'
            renderpass = '-'
            if self.videocodingRequired():
                videocoding = '-'

            queues = self.getQueueList(cmd)
            if queues is None:
                queues = 'Any'
            else:
                queues = (' + \n').join([queue.upper() for queue in queues])

            tasks = '-'

        table_items = (cmdbufferlevel, renderpass, videocoding, queues, tasks)
        entry = '|'.join(filter(None, table_items))

        return ('|' + entry) if entry else None


    def findRequiredEnums(self, enums):
        """Check each enumerant name in the enums list and remove it if not
        required by the generator. This allows specifying success and error
        codes for extensions that are only included in validity when needed
        for the spec being targeted."""
        assert self.registry
        return self.keepOnlyRequired(enums, self.registry.enumdict)

    def findRequiredCommands(self, commands):
        """Check each command name in the commands list and remove it if not
        required by the generator.

        This will allow some state operations to take place before endFile."""
        assert self.registry
        return self.keepOnlyRequired(commands, self.registry.cmddict)

    def keepOnlyRequired(self, names, info_dict):
        """Check each element name in the supplied dictionary and remove it if not
        required by the generator.

        This will allow some operations to take place before endFile no matter the order of generation."""
        # TODO Unpleasantly breaks encapsulation. Should be a method in the registry class

        def is_required(name):
            assert self.registry
            info = self.registry.lookupElementInfo(name, info_dict)
            if info is None:
                return False
            if not info.required:
                self.logMsg('diag', 'keepOnlyRequired: element',
                            name, 'IS NOT required, skipping')
            return info.required

        return [name
                for name in names
                if is_required(name)]

    def makeReturnCodeList(self, attrib, cmd, name):
        """Return a list of possible return codes for a function.

        attrib is either 'successcodes' or 'errorcodes'.
        """
        assert self.registry
        return_lines = []
        RETURN_CODE_FORMAT = '* ename:{}'

        codes_attr = cmd.get(attrib)
        if codes_attr:
            codes = self.findRequiredEnums(codes_attr.split(','))
            if codes:
                return_lines.extend((RETURN_CODE_FORMAT.format(code)
                                     for code in codes))

        applicable_ext_codes = (ext_code
                                for ext_code in self.registry.commandextensionsuccesses
                                if ext_code.command == name)
        for ext_code in applicable_ext_codes:
            line = RETURN_CODE_FORMAT.format(ext_code.value)
            if ext_code.extension:
                line += f' [only if {self.conventions.formatExtension(ext_code.extension)} is enabled]'

            return_lines.append(line)
        if return_lines:
            return '\n'.join(return_lines)

        return None

    def makeSuccessCodes(self, cmd, name):
        return self.makeReturnCodeList('successcodes', cmd, name)

    def makeErrorCodes(self, cmd, name):
        return self.makeReturnCodeList('errorcodes', cmd, name)

    def recordStateRelationship(self, cmd, name, relationship: StateRelationship):
        states_str = cmd.get(STATE_ATTRIBUTES[relationship])
        if not states_str:
            return None

        state_names = states_str.split(',')
        for current_state in state_names:
            self.states.add_command(name, current_state, relationship)
        return state_names

    def genCmd(self, cmdinfo, name, alias):
        """Command generation."""
        OutputGenerator.genCmd(self, cmdinfo, name, alias)

        # @@@ (Jon) something needs to be done here to handle aliases, probably

        validity = self.makeValidityCollection(name)

        # OpenXR-only: make sure extension is enabled
        validity.possiblyAddExtensionRequirement(self.currentExtension, 'calling flink:')

        validity += self.makeStructOrCommandValidity(
            cmdinfo.elem, name, cmdinfo.getParams())

        threadsafety = self.makeThreadSafetyBlock(cmdinfo.elem, 'param')
        commandpropertiesentry = None

        # Vulkan-specific
        # commandpropertiesentry = self.makeCommandPropertiesTableEntry(
        #     cmdinfo.elem, name)
        successcodes = self.makeSuccessCodes(cmdinfo.elem, name)
        errorcodes = self.makeErrorCodes(cmdinfo.elem, name)

        # OpenXR-specific
        self.generateStateValidity(validity, name)

        self.writeInclude('protos', name, validity, threadsafety,
                          commandpropertiesentry, successcodes, errorcodes)

    def genStructInternals(self, typeinfo, typeName, validity, threadsafety):
        """Internals of generating for a struct, so aliases can be handled."""

        if typeinfo.elem.get('returnedonly') is None:
            validity += self.makeStructOrCommandValidity(
                typeinfo.elem, typeName, typeinfo.getMembers())
            threadsafety = self.makeThreadSafetyBlock(typeinfo.elem, 'member')

        else:
            # Need to generate structure type and next pointer chain member validation
            validity += self.makeOutputOnlyStructValidity(
                typeinfo.elem, typeName, typeinfo.getMembers())

    def genStruct(self, typeinfo, typeName, alias):
        """Struct Generation."""
        assert self.registry
        OutputGenerator.genStruct(self, typeinfo, typeName, alias)

        # @@@ (Jon) something needs to be done here to handle aliases, probably

        # Anything that is only ever returned cannot be set by the user, so
        # should not have any validity information.
        validity = self.makeValidityCollection(typeName)
        threadsafety = []

        # OpenXR-only: make sure extension is enabled
        validity.possiblyAddExtensionRequirement(self.currentExtension, 'using slink:')

        self.genStructInternals(typeinfo, typeName, validity, threadsafety)

        if alias and self.conventions.duplicate_aliased_structs:
            validity.addValidityEntry(
                '**Note:** slink:{alias_source} is an alias for slink:{alias_target}, so the following items replicate the implicit valid usage for slink:{alias_target}'.format(
                    alias_target=alias, alias_source=typeName))
            validity_count_before = len(validity.lines)
            alias_info = self.registry.typedict[alias]
            self.genStructInternals(alias_info, alias, validity, threadsafety)
            if len(validity.lines) == validity_count_before:
                # The alias target has no implicit valid usage, so drop the note.
                # (there are no "following items")
                validity.lines.pop()

        self.writeInclude('structs', typeName, validity,
                          threadsafety, None, None, None)

    def genGroup(self, groupinfo, groupName, alias):
        """Group (e.g. C "enum" type) generation.
        For the validity generator, this just tags individual enumerants
        as required or not.
        """
        assert self.registry
        OutputGenerator.genGroup(self, groupinfo, groupName, alias)

        # @@@ (Jon) something needs to be done here to handle aliases, probably

        groupElem = groupinfo.elem

        # Loop over the nested 'enum' tags. Keep track of the minimum and
        # maximum numeric values, if they can be determined; but only for
        # core API enumerants, not extension enumerants. This is inferred
        # by looking for 'extends' attributes.
        for elem in groupElem.findall('enum'):
            name = elem.get('name')
            ei = self.registry.lookupElementInfo(name, self.registry.enumdict)

            if ei is None:
                self.logMsg('error',
                            f'genGroup({groupName}) - no element found for enum {name}')

            # Tag enumerant as required or not
            ei.required = self.isEnumRequired(elem)

    def genType(self, typeinfo, name, alias):
        """Type Generation."""
        OutputGenerator.genType(self, typeinfo, name, alias)

        # @@@ (Jon) something needs to be done here to handle aliases, probably

        category = typeinfo.elem.get('category')
        if category in ('struct', 'union'):
            self.genStruct(typeinfo, name, alias)
