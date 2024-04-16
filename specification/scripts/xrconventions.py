#!/usr/bin/python3 -i
#
# Copyright (c) 2013-2024, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

# Working-group-specific style conventions,
# used in generation.

import re
from pathlib import Path

from spec_tools.conventions import ConventionsBase

# Tokenize into "words"
# The leading named groups are for "special cases" per spec "Implicit Valid Usage" section 2.7.7
# Note that while Vulkan is listed as a special case,
# it doesn't actually break the rules, so it's not treated specially
MAIN_RE = re.compile(
    r'''(?P<d3d>D3D[0-9]*)|    # Take D3D as a group of its own
        (?P<egl>EGL)|          # EGL as a word of its own
        (?P<gl>OpenGL(ES)?)|   # OpenGL and OpenGLES as words
        (?P<dimension>[0-9]D)| # Things like 2D are words
        (?P<word>              # Normal-ish words, which are....
            ([A-Z]([a-z]+([0-9](?!D[A-Z]{1}))*)+)|  # Capital letter followed by at least one lowercase letter, possibly ending in some digits as long as the digits aren't followed by a "D" then another word
            ([A-Z][A-Z0-9]+(?![a-z]))       # Or, all-caps letter and digit mix starting with a letter, excluding the last capital before some lowercase
        )''', re.VERBOSE)


class OpenXRConventions(ConventionsBase):
    """The specifics of how OpenXR writes a spec."""

    @property
    def null(self):
        """Preferred spelling of NULL."""
        return 'code:NULL'

    @property
    def constFlagBits(self):
        """Return True if static const flag bits should be generated, False if an enumerated type should be generated."""
        return True

    @property
    def struct_macro(self):
        """Get the appropriate format macro for a structure.

        Primarily affects generated valid usage statements.
        """

        return 'slink:'

    @property
    def structtype_member_name(self):
        """Return name of the structure type member"""
        return 'type'

    @property
    def nextpointer_member_name(self):
        """Return name of the structure pointer chain member"""
        return 'next'

    @property
    def valid_pointer_prefix(self):
        """Return prefix to pointers which must themselves be valid.

           OpenXR doesn't currently do this.
        """
        return None

    def is_structure_type_member(self, paramtype, paramname):
        """Determine if member type and name match the structure type member."""
        return paramtype == 'XrStructureType' and paramname == self.structtype_member_name

    def is_nextpointer_member(self, paramtype, paramname):
        """Determine if member type and name match the next pointer chain member."""
        return paramtype == 'void' and paramname == self.nextpointer_member_name

    def generate_structure_type_from_name(self, structname):
        """Generate a structure type name, like XR_TYPE_CREATE_INSTANCE_INFO"""
        structure_type_parts = []
        # Tokenize into "words"
        for elem in MAIN_RE.finditer(structname):
            if elem.group('d3d'):
                # D3D ⇒ _D3D
                structure_type_parts.append(elem.group())
            elif elem.group('egl'):
                # EGL ⇒ _EGL
                structure_type_parts.append(elem.group())
            elif elem.group('gl'):
                # OpenGL ⇒ _OPENGL
                # OpenGLES ⇒ _OPENGL_ES
                structure_type_parts.append(
                    elem.group().upper().replace('ES', '_ES'))
            elif elem.group('dimension'):
                # 2D ⇒ _2D
                structure_type_parts.append(
                    elem.group())
            else:
                word = elem.group('word')
                if word == 'Xr':
                    structure_type_parts.append('XR_TYPE')
                else:
                    structure_type_parts.append(word.upper())
        return '_'.join(structure_type_parts)

    @property
    def warning_comment(self):
        """Return warning comment to be placed in header of generated Asciidoctor files"""
        return '// WARNING: DO NOT MODIFY! This file is automatically generated from the xr.xml registry'

    @property
    def file_suffix(self):
        """Return suffix of generated Asciidoctor files"""
        return '.adoc'

    def api_name(self, spectype='api'):
        """Return API or specification name for citations in ref pages.

        spectype is the spec this refpage is for.
        'api' (the default value) is the main OpenXR API Specification.
        If an unrecognized spectype is given, returns None.

        """
        if spectype == 'api' or spectype is None:
            return 'OpenXR'
        return None

    @property
    def xml_supported_name_of_api(self):
        """Return the supported= attribute used in API XML"""
        return 'openxr'

    @property
    def api_prefix(self):
        """Return API token prefix"""
        return 'XR_'

    @property
    def write_contacts(self):
        """Return whether contact list should be written to extension appendices"""
        return False

    @property
    def write_refpage_include(self):
        """Return whether refpage include should be written to extension appendices"""
        return True

    @property
    def allows_x_number_suffix(self):
        """Whether vendor tags can be suffixed with X and a number to mark experimental extensions."""
        return True

    def formatVersion(self, name, apivariant, major, minor):
        """Mark up an API version name as a link in the spec."""
        version = f'{major}.{minor}'
        return f'<<versions-{version}, OpenXR {version}>>'

    def formatExtension(self, name):
        """Mark up an extension name as a link in the spec."""
        return f'apiext:{name}'

    def writeFeature(self, featureName, featureExtraProtect, filename):
        """Returns True if OutputGenerator.endFeature should write this feature.

        Used in COutputGenerator.

        In OpenXR, a feature is written to the openxr_platform header
        if and only if it has a defined "protect" attribute.
        """
        if filename == 'openxr_reflection.h':
            # Write all features to the reflection header
            return True
        is_loader = featureName == 'XR_LOADER_VERSION_1_0'
        is_protected = featureExtraProtect is not None
        is_platform_header = (filename == 'openxr_platform.h')

        # Only write loader spec to loader file.
        if is_loader:
            return filename == 'openxr_loader_negotiation.h'

        if filename == 'openxr_loader_negotiation.h':
            return False

        # non-protected goes in non-platform header,
        # protected goes in platform header.
        return is_protected == is_platform_header

    def requires_error_validation(self, return_type):
        """Returns True if the return_type element is an API result code
           requiring error validation.
        """
        return return_type is not None and return_type.text == 'XrResult'

    @property
    def member_used_for_unique_vuid(self):
        """Return the member name used in the VUID-...-...-unique ID."""
        return self.nextpointer_member_name

    def is_externsync_command(self, protoname):
        """Returns True if the protoname element is an API command requiring
           external synchronization.

           OpenXR does not do this at present.
        """
        return False

    def is_api_name(self, name):
        """Returns True if name is in the reserved API namespace.
        For OpenXR, these are names with a case-insensitive 'xr' prefix, or
        a 'PFN_xr' function pointer type prefix.
        """
        return name[0:2].lower() == 'xr' or name[0:6] == 'PFN_xr'

    def should_insert_may_alias_macro(self, genOpts):
        """Return true if we should insert a "may alias" macro in this file."""

        # Don't insert may alias macro in things included in the spec.
        return "inc" not in genOpts.filename

    def make_voidpointer_alias(self, tail):
        """Reformat a void * declaration to include the API alias macro"""
        return f'* XR_MAY_ALIAS{tail[1:]}'

    def specURL(self, spectype='api'):
        """Return public registry URL which ref pages should link to for the
           current all-extensions HTML specification, so xrefs in the
           asciidoc source that aren't to ref pages can link into it
           instead. N.b. this may need to change on a per-refpage basis if
           there are multiple documents involved.
        """
        return 'https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html'

    @property
    def xml_api_name(self):
        """Return the name used in the default API XML registry for the default API"""
        return 'openxr'

    @property
    def registry_path(self):
        """Return relpath to the default API XML registry in this project."""
        return 'registry/xr.xml'

    @property
    def specification_path(self):
        """Return relpath to the Asciidoctor specification sources in this project."""
        return '{appendices}'

    @property
    def extra_refpage_headers(self):
        """Return any extra text to add to refpage headers."""
        return 'include::{config}/attribs.adoc[]'

    @property
    def extension_index_prefixes(self):
        """Return a set of extension prefixes used to group extension refpages."""
        return ('XR_KHR', 'XR_KHX', 'XR_EXT', 'XR')

    @property
    def unified_flag_refpages(self):
        """Return True if Flags/FlagBits refpages are unified, False if
           they are separate.
        """
        return False

    @property
    def spec_reflow_path(self):
        """Return the path to the spec source folder to reflow"""
        return str(Path(__file__).resolve().parent.parent / 'sources')

    @property
    def spec_no_reflow_dirs(self):
        """Return a set of directories not to automatically descend into
           when reflowing spec text
        """
        return tuple()

    @property
    def zero(self):
        """Return the preferred way of formatting a literal 0."""
        return "`0`"

    @property
    def generate_index_terms(self):
        """Return True if asiidoctor index terms should be generated as part
           of an API interface from the docgenerator."""

        return True

    @property
    def generate_enum_table(self):
        """Return True if asciidoctor tables describing enumerants in a
           group should be generated as part of group generation."""
        return True

    def generate_max_enum_in_docs(self):
        """Return True if MAX_ENUM tokens should be generated in
           documentation includes."""
        return False

    def extension_file_path(self, name):
        """Return file path to an extension appendix relative to a directory
           containing all such appendices.
           - name - extension name"""

        (vendor, bare_name) = self.extension_name_split(name)
        vendor = vendor.lower()

        return f'{vendor}/{vendor}_{bare_name.lower()}{self.file_suffix}'

    def extension_include_string(self, name):
        """Return format string for include:: line for an extension appendix
           file.
            - name - extension name"""

        return f'include::{{appendices}}/{self.extension_file_path(name)}[]'

    @property
    def provisional_extension_warning(self):
        """Return True if a warning should be included in extension
           appendices for provisional extensions."""
        return False

    @property
    def include_extension_appendix_in_refpage(self):
        """Return True if generating extension refpages by embedding
           extension appendix content (default), False otherwise
           (OpenXR)."""

        return False

    @property
    def duplicate_aliased_structs(self):
        """
        Should aliased structs have the original struct definition listed in the
        generated docs snippet?
        """
        return True

    @property
    def protectProtoComment(self):
        """Return True if generated #endif should have a comment matching
           the protection symbol used in the opening #ifdef/#ifndef."""
        return True
