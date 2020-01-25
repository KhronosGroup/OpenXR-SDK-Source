#ifndef OPENXR_REFLECTION_H_
#define OPENXR_REFLECTION_H_ 1

/*
** Copyright (c) 2017-2020 The Khronos Group Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
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

#define XR_LIST_BITS_/*{bitmask.typeName}*/(_)/*{" \\" if bitmask.maskTuples else ""}*/
//# for member in bitmask.maskTuples
    _(/*{", ".join(member)}*/)/*{ " \\" if member != bitmask.maskTuples[-1] else ""}*/
//# endfor
//## Intentionally left blank.
//# endfor

//# for struct in structs

#define XR_LIST_STRUCT_/*{struct.typeName}*/(_) \
//# for member in struct.members
    _(/*{member}*/)/*{" \\" if member != struct.members[-1] else ""}*/
//# endfor

//## Intentionally left blank.
//# endfor


//## XR_LIST_STRUCTURE_TYPES is excluded because it is not ready to include.
//## It must make sure to not include structures which are disabled due to undefined "protected"
{#
#define XR_LIST_STRUCTURE_TYPES(_) \
//# for struct in structs
//#     if struct.structTypeName is not none
    _(/*{struct.structTypeName}*/, /*{struct.typeName}*/)/*{" \\" if struct != structs[:-1] else ""}*/
//#     endif
//# endfor
#}

#endif
