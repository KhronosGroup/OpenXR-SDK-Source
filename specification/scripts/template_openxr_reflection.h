#ifndef OPENXR_REFLECTION_H_
#define OPENXR_REFLECTION_H_ 1

/*
** Copyright (c) 2017-2022, The Khronos Group Inc.
**
** SPDX-License-Identifier: Apache-2.0 OR MIT
*/

/*
** This header is generated from the Khronos OpenXR XML API Registry.
**
*/

#include "openxr.h"

/*
This file contains expansion macros (X Macros) for OpenXR enumerations and structures.
Example of how to use expansion macros to make an enum-to-string function:

#define XR_ENUM_CASE_STR(name, val) case name: return #name;
#define XR_ENUM_STR(enumType)                         \
    constexpr const char* XrEnumStr(enumType e) {     \
        switch (e) {                                  \
            XR_LIST_ENUM_##enumType(XR_ENUM_CASE_STR) \
            default: return "Unknown";                \
        }                                             \
    }                                                 \

XR_ENUM_STR(XrResult);
*/

//# for enum in enums

#define XR_LIST_ENUM_/*{enum.typeName}*/(_) \
//# for member in enum.enumTuples:
    _(/*{", ".join(member)}*/) \
//# endfor
    _(/*{enum.typeNamePrefix}*/_MAX_ENUM/*{enum.typeNameSuffix}*/, 0x7FFFFFFF)

//# endfor

//# for bitmask in bitmasks

#define XR_LIST_BITS_/*{bitmask.typeName}*/(_)/*{" \\" if bitmask.maskTuples}*/
//# for member in bitmask.maskTuples
    _(/*{", ".join(member)}*/) \
//# endfor

//## Preceding line intentionally left blank to absorb the trailing backslash
//# endfor

//# for struct in structs

/// Calls your macro with the name of each member of /*{struct.typeName}*/, in order.
#define XR_LIST_STRUCT_/*{struct.typeName}*/(_) \
//# for member in struct.members
    _(/*{member}*/) \
//# endfor

//## Preceding line intentionally left blank to absorb the trailing backslash
//# endfor

//## Used when making structure type macros
/*% macro makeStructTypes(typedStructs, funcName="_") -%*/
//# for struct in typedStructs
    /*{funcName}*/(/*{struct.typeName}*/, /*{struct.structTypeName}*/) \
//# endfor

//## Preceding line intentionally left blank to absorb the trailing backslash
/*%- endmacro %*/

/// Calls your macro with the structure type name and the XrStructureType constant for
/// each known/available structure type, excluding those unavailable due to preprocessor definitions.
#define XR_LIST_STRUCTURE_TYPES(_) \
    XR_LIST_STRUCTURE_TYPES_CORE(_) \
//# for protect, structTypes in protectedStructs
    XR_LIST_STRUCTURE_TYPES_/*{protect | join("_")}*/(_) \
//# endfor


//## Preceding line intentionally left blank to absorb the trailing backslash

/// Implementation detail of XR_LIST_STRUCTURE_TYPES() - structure types available without any preprocessor definitions
#define XR_LIST_STRUCTURE_TYPES_CORE(_) \
/*{ makeStructTypes(unprotectedStructs) }*/

//# for protect, structTypes in protectedStructs
/*{ protect_begin(structTypes[0]) }*/
/// Implementation detail of XR_LIST_STRUCTURE_TYPES()
/// Structure types available only when /*{ protect | join(" and ") }*/ /*{ "is" if protect | length == 1 else "are" }*/ defined
#define XR_LIST_STRUCTURE_TYPES_/*{protect | join("_")}*/(_) \
/*{ makeStructTypes(structTypes) }*/
#else
#define XR_LIST_STRUCTURE_TYPES_/*{protect | join("_")}*/(_)
#endif

//# endfor


//## Preceding line intentionally left blank to absorb the trailing backslash

/// Calls your macro with the name and extension number of all known
/// extensions in this version of the spec.
#define XR_LIST_EXTENSIONS(_) \
//# for extname, extdata in extensions
    _(/*{extname}*/, /*{extdata.number}*/) \
//# endfor

//## Preceding line intentionally left blank to absorb the trailing backslash

#endif
