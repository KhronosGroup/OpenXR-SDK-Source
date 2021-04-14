#!/usr/bin/python3 -i
#
# Copyright (c) 2013-2021, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0
#
# NOTE: Generated file is dual-licensed (Apache-2.0 OR MIT),
# to allow downstream projects to use the standard while
# avoiding license incompatibility

import re

from generator import OutputGenerator, write
from jinja_helpers import JinjaTemplate, make_jinja_environment
from spec_tools.util import getElemName, getElemType


class CStruct:
    """Represents a OpenXR struct type"""

    def __init__(self, typeName, structTypeName, members, protect):
        self.typeName = typeName
        self.members = members
        self.structTypeName = structTypeName
        self.protect = protect

    @property
    def protect_value(self):
        return self.protect is not None

    @property
    def protect_string(self):
        if self.protect:
            return " && ".join("defined({})".format(x) for x in self.protect)


class CBitmask:
    """Represents a OpenXR mask type"""

    def __init__(self, typeName, maskTuples):
        self.typeName = typeName
        self.maskTuples = maskTuples


class CEnum:
    """Represents a OpenXR group enum type"""

    def __init__(self, typeName, typeNamePrefix, typeNameSuffix, enumTuples):
        self.typeName = typeName
        self.typeNamePrefix = typeNamePrefix
        self.typeNameSuffix = typeNameSuffix
        self.enumTuples = enumTuples


class CReflectionOutputGenerator(OutputGenerator):
    """Generate specified API interfaces in a specific style, such as a C header"""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.env = make_jinja_environment(file_with_templates_as_sibs=__file__)
        self.structs = []
        self.enums = []
        self.bitmasks = []
        self.protects = set()

    def beginFile(self, genOpts):
        OutputGenerator.beginFile(self, genOpts)
        self.template = JinjaTemplate(self.env, "template_{}".format(genOpts.filename))

    def getStructsForProtect(self, protect=None):
        return [x for x in self.structs
                if x.protect == protect and x.structTypeName is not None]

    def endFile(self):
        file_data = ''

        unprotected_structs = self.getStructsForProtect()
        protected_structs = [(x, self.getStructsForProtect(x))
                             for x in sorted(self.protects)]

        extensions = list(
            ((name, data) for name, data in self.registry.extdict.items()
             if data.supported != 'disabled'))
        def getNumber(x):
            return int(x[1].number)
        extensions.sort(key=getNumber)
        file_data += self.template.render(
            unprotectedStructs=unprotected_structs,
            protectedStructs=protected_structs,
            structs=self.structs,
            enums=self.enums,
            bitmasks=self.bitmasks,
            extensions=extensions)
        write(file_data, file=self.outFile)

        # Finish processing in superclass
        OutputGenerator.endFile(self)

    def genType(self, typeinfo, name, alias):
        OutputGenerator.genType(self, typeinfo, name, alias)
        typeElem = typeinfo.elem

        if alias:
            return
        category = typeElem.get('category')
        if category in ('struct', 'union'):
            # If the type is a struct type, generate it using the
            # special-purpose generator.
            self.genStruct(typeinfo, name, alias)

    def genStruct(self, typeinfo, typeName, alias):
        OutputGenerator.genStruct(self, typeinfo, typeName, alias)

        if alias:
            return

        structTypeName = None
        members = []
        for member in typeinfo.getMembers():
            memberName = getElemName(member)
            memberType = getElemType(member)
            if self.conventions.is_structure_type_member(memberType, memberName):
                structTypeName = member.get("values")

            members.append(memberName)

        protect = set()
        if self.featureExtraProtect:
            protect.update(self.featureExtraProtect.split(','))
        localProtect = typeinfo.elem.get('protect')
        if localProtect:
            protect.update(localProtect.split(','))

        if protect:
            protect = tuple(sorted(protect))
        else:
            protect = None

        self.structs.append(CStruct(typeName, structTypeName, members, protect))
        if protect:
            self.protects.add(protect)

    def genGroup(self, groupinfo, groupName, alias=None):
        OutputGenerator.genGroup(self, groupinfo, groupName, alias)

        if alias:
            return

        if getElemType(groupinfo.elem) == 'bitmask':
            bitmaskTypeName = getElemName(groupinfo.flagType.elem)

            bitmaskTuples = []
            for elem in groupinfo.elem.findall('enum'):
                (numVal, strVal) = self.enumToValue(elem, True)
                bitmaskTuples.append((getElemName(elem), strVal))

            self.bitmasks.append(CBitmask(bitmaskTypeName, bitmaskTuples))
        else:
            groupElem = groupinfo.elem

            expandName = re.sub(r'([0-9a-z_])([A-Z0-9][^A-Z0-9]?)', r'\1_\2', groupName).upper()
            expandPrefix = expandName

            expandSuffix = ''
            expandSuffixMatch = re.search(r'[A-Z][A-Z]+$', groupName)
            if expandSuffixMatch:
                expandSuffix = '_' + expandSuffixMatch.group()
                # Strip off the suffix from the prefix
                expandPrefix = expandName.rsplit(expandSuffix, 1)[0]

            enumTuples = []
            for elem in groupElem.findall('enum'):
                (numVal, strVal) = self.enumToValue(elem, True)
                if numVal is None:
                    # then this is an alias or something
                    continue
                enumTuples.append((getElemName(elem), strVal))

            self.enums.append(CEnum(groupName, expandPrefix, expandSuffix, enumTuples))
