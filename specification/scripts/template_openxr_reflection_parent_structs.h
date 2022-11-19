#ifndef OPENXR_REFLECTION_PARENT_STRUCTS_H_
#define OPENXR_REFLECTION_PARENT_STRUCTS_H_ 1

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
This file contains expansion macros (X Macros) for OpenXR structures that have a parent type.
*/

/*% from "template_openxr_reflection_structs.h" import makeStructureTypesMacros %*/

//# for family in polymorphic_struct_families
/// Like XR_LIST_ALL_STRUCTURE_TYPES, but only includes types whose parent struct type is /*{family.parent_type_name}*/
/*{ makeStructureTypesMacros("XR_LIST_ALL_CHILD_STRUCTURE_TYPES_" + family.parent_type_name, family.unprotected_structs, family.protect_sets_and_protected_structs) }*/

//# endfor

#endif
