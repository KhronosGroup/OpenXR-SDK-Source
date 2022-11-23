#!/usr/bin/python3 -i
#
# Copyright (c) 2013-2022, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

from generator import OutputGenerator, write


class DocIndexOutputGenerator(OutputGenerator):
    """DocIndexOutputGenerator - subclass of OutputGenerator.
    Generates an Index chapter for the spec.
    Similar to DocOutputGenerator, but writes a single file."""

    def apiName(self, name):
        """Return True if name is in the reserved API namespace.

        Delegates to the conventions object. """
        return self.genOpts.conventions.is_api_name(name)

    def beginFile(self, genOpts):
        OutputGenerator.beginFile(self, genOpts)
        #
        # Dictionaries are keyed by the name of the entity (e.g.
        # self.structs is keyed by structure names). Values are
        # text that will go after an index entry (e.g. to point to the FlagBits for a Flags type)
        # or None if there's no such data.
        self.basetypes = {}
        self.enums = {}
        self.flags = {}
        self.funcpointers = {}
        self.protos = {}
        self.structs = {}
        self.handles = {}
        self.defines = {}
        self.consts = {}

    def record_name(self, name_dict, name, extra_data=None):
        name_dict[name] = extra_data

    def output_name_dict(self, name_dict, title, prefix):

        anchor = title.lower().replace(' ', '-')
        write('[[index-{}]]'.format(anchor), file=self.outFile)

        write('### ' + title, file=self.outFile)
        write('', file=self.outFile)

        for name in sorted(name_dict.keys()):
            text = '* ' + prefix + name
            extra_data = name_dict[name]
            if extra_data:
                text += ' -- ' + extra_data
            write(text, file=self.outFile)

        write('', file=self.outFile)

    def endFile(self):
        # Remove things from enums if they're a flag bits.
        flag_bits = set(self.enums.keys()).intersection(self.flags.values())
        for f in flag_bits:
            self.enums.pop(f)

        # This sets up the sub-chapter order
        pieces = (
            ('Base Types and Atoms', self.basetypes, 'basetype:'),
            ('Defines', self.defines, 'dlink:'),
            ('Enumerations', self.enums, 'elink:'),
            ('Flags and Flag Bits', self.flags, 'elink:'),
            ('Function Pointer Types', self.funcpointers, 'tlink:'),
            ('Functions', self.protos, 'flink:'),
            ('Handles', self.handles, 'slink:'),
            ('Structures', self.structs, 'slink:'),
        )
        write('[[index]]', file=self.outFile)
        write('## Index', file=self.outFile)
        write('', file=self.outFile)

        for title, name_dict, prefix in pieces:
            self.output_name_dict(name_dict, title, prefix)

        OutputGenerator.endFile(self)

    def genType(self, typeinfo, name, alias):
        """Generate type.

        Just adds to the right dict. In the case of bitmasks,
        tries to figure out the bit value type and store that in a dictionary instead."""
        OutputGenerator.genType(self, typeinfo, name, alias)
        typeElem = typeinfo.elem
        # If the type is a struct type, traverse the embedded <member> tags
        # generating a structure. Otherwise, emit the tag text.
        category = typeElem.get('category')
        if category in ('struct', 'union'):
            self.record_name(self.structs, name)
        elif category == 'bitmask':
            requiredEnum = typeElem.get('bitvalues')
            if requiredEnum is not None:
                self.record_name(self.flags, name, "See also elink:{}".format(requiredEnum))
        elif category == 'enum':
            self.record_name(self.enums, name)
        elif category == 'funcpointer':
            self.record_name(self.funcpointers, name)
        elif category == 'handle':
            self.record_name(self.handles, name)
        elif category == 'define':
            self.record_name(self.defines, name)
        elif category == 'basetype':
            # Don't add an entry for base types that are not API types
            # e.g. an API Bool type gets an entry, uint32_t does not
            if self.apiName(name):
                self.record_name(self.basetypes, name)

    def genGroup(self, groupinfo, groupName, alias):
        """Generate group (e.g. C "enum" type)."""
        OutputGenerator.genGroup(self, groupinfo, groupName, alias)

        if alias:
            return

        self.record_name(self.enums, groupName)

    def genEnum(self, enuminfo, name, alias):
        """Generate enumerant (compile time constants)."""
        OutputGenerator.genEnum(self, enuminfo, name, alias)

        if alias:
            return
        self.record_name(self.consts, name)

    def genCmd(self, cmdinfo, name, alias):
        """Generate command."""
        OutputGenerator.genCmd(self, cmdinfo, name, alias)

        if alias:
            return
        self.record_name(self.protos, name)
