#ifndef OPENXR_REFLECTION_STRUCTS_H_
#define OPENXR_REFLECTION_STRUCTS_H_ 1

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
This file contains expansion macros (X Macros) for OpenXR structures.
*/

/*% from "template_openxr_reflection.h" import makeStructTypes %*/

/*% macro makeStructureTypesMacros(name, unprotectedStructs, protectedStructs) -%*/
#define /*{name}*/(_avail, _unavail) \
    _impl_/*{name}*/_CORE(_avail, _unavail) \
//# for protect, structTypes in protectedStructs
    _impl_/*{name}*/_/*{protect | join("_")}*/(_avail, _unavail) \
//# endfor


//## Preceding line intentionally left blank to absorb the trailing backslash

// Implementation detail of /*{name}*/()
#define _impl_/*{name}*/_CORE(_avail, _unavail) \
/*{ makeStructTypes(unprotectedStructs, "_avail") }*/

//# for protect, structTypes in protectedStructs
/*{ protect_begin(structTypes[0]) }*/
#define _impl_/*{name}*/_/*{protect | join("_")}*/(_avail, _unavail) \
/*{ makeStructTypes(structTypes, "_avail") }*/
#else
#define _impl_/*{name}*/_/*{protect | join("_")}*/(_avail, _unavail) \
/*{ makeStructTypes(structTypes, "_unavail") }*/
#endif

//# endfor


//## Preceding line intentionally left blank to absorb the trailing backslash
/*% endmacro %*/

/// Calls one of your macros with the structure type name and the XrStructureType constant for
/// each known structure type. The first macro (_avail) is called for those that are available,
/// while the second macro (_unavail) is called for those unavailable due to preprocessor definitions.
/*{ makeStructureTypesMacros("XR_LIST_ALL_STRUCTURE_TYPES", unprotectedStructs, protectedStructs) }*/

#endif
