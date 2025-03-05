#!/usr/bin/env python3 -i
#
# Copyright (c) 2017-2025 The Khronos Group Inc.
# Copyright (c) 2017-2019 Valve Corporation
# Copyright (c) 2017-2019 LunarG, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#
# NOTE: Generated file is dual-licensed (Apache-2.0 OR MIT),
# to allow downstream projects to avoid license incompatibility with
# the loader
#
# Author(s):    Mark Young <marky@lunarg.com>
#
# Purpose:      This file utilizes the content formatted in the
#               automatic_source_generator.py class to produce the
#               generated source code for the loader.


from automatic_source_generator import AutomaticSourceOutputGenerator, CurrentExtensionTracker
from generator import write

# UtilitySourceOutputGenerator - subclass of AutomaticSourceOutputGenerator.


class UtilitySourceOutputGenerator(AutomaticSourceOutputGenerator):
    """Generate loader source using XML element attributes from registry"""

    # Override the base class header warning so the comment indicates this file.
    #   self            the UtilitySourceOutputGenerator object
    def outputGeneratedHeaderWarning(self):
        # REUSE-IgnoreStart
        generated_warning = ''
        generated_warning += '// Copyright (c) 2017-2025 The Khronos Group Inc.\n'
        generated_warning += '// Copyright (c) 2017-2019, Valve Corporation\n'
        generated_warning += '// Copyright (c) 2017-2019, LunarG, Inc.\n\n'
        # Broken string is to avoid confusing the REUSE tool here.
        generated_warning += '// SPDX-License-' + 'Identifier: Apache-2.0 OR MIT\n\n'
        generated_warning += '// *********** THIS FILE IS GENERATED - DO NOT EDIT ***********\n'
        generated_warning += '//     See utility_source_generator.py for modifications\n'
        generated_warning += '// ************************************************************\n'
        # REUSE-IgnoreEnd
        write(generated_warning, file=self.outFile)

    # Call the base class to properly begin the file, and then add
    # the file-specific header information.
    #   self            the UtilitySourceOutputGenerator object
    #   gen_opts        the UtilitySourceGeneratorOptions object
    def beginFile(self, genOpts):
        AutomaticSourceOutputGenerator.beginFile(self, genOpts)
        assert self.genOpts
        assert self.genOpts.filename

        preamble = ''

        if self.genOpts.filename.endswith('.h'):
            # All .h start the same
            preamble += '#pragma once\n\n'

        elif self.genOpts.filename.endswith('.c'):
            # All .c files start the same
            header = self.genOpts.filename.replace('.c', '.h')
            preamble += f'#include "{header}"\n\n'
        else:
            raise RuntimeError(f"Unknown filename extension! {self.genOpts.filename}")

        # The different .h files have different includes
        if self.genOpts.filename == 'xr_generated_dispatch_table_core.h':
            preamble += '#include <openxr/openxr.h>\n'

        elif self.genOpts.filename == 'xr_generated_dispatch_table.h':
            preamble += '#include "xr_dependencies.h"\n'
            preamble += '#include <openxr/openxr.h>\n'
            preamble += '#include <openxr/openxr_platform.h>\n'

        preamble += '\n'

        write(preamble, file=self.outFile)

    # Write out all the information for the appropriate file,
    # and then call down to the base class to wrap everything up.
    #   self            the UtilitySourceOutputGenerator object
    def endFile(self):
        assert self.genOpts
        assert self.genOpts.filename

        file_data = ''

        file_data += '#ifdef __cplusplus\n'
        file_data += 'extern "C" { \n'
        file_data += '#endif\n'

        if self.genOpts.filename.endswith('.h'):
            file_data += self.outputDispatchTable()
            file_data += self.outputDispatchPrototypes()

        elif self.genOpts.filename.endswith('.c'):
            file_data += self.outputDispatchTableHelper()

        else:
            raise RuntimeError(f"Unknown filename extension! {self.genOpts.filename}")

        file_data += '\n'
        file_data += '#ifdef __cplusplus\n'
        file_data += '} // extern "C"\n'
        file_data += '#endif\n'

        write(file_data, file=self.outFile)

        # Finish processing in superclass
        AutomaticSourceOutputGenerator.endFile(self)

    # Write out a prototype for a C-style command to populate a Dispatch table
    #   self            the ApiDumpOutputGenerator object
    def outputDispatchPrototypes(self):
        table_helper = '\n'
        table_helper += '// Prototype for dispatch table helper function\n'

        if self.genOpts.filename == 'xr_generated_dispatch_table_core.h':
            table_helper += 'void GeneratedXrPopulateDispatchTableCore(struct XrGeneratedDispatchTableCore *table,\n'
        else:
            table_helper += 'void GeneratedXrPopulateDispatchTable(struct XrGeneratedDispatchTable *table,\n'

        table_helper += '                                      XrInstance instance,\n'
        table_helper += '                                      PFN_xrGetInstanceProcAddr get_inst_proc_addr);\n'
        return table_helper

    def _feature_name_to_core_version(self, ext_name: str):
        return ext_name[len(self.conventions.api_version_prefix):].replace("_", ".")

    # Write out a C-style structure used to store the Dispatch table information
    #   self            the ApiDumpOutputGenerator object
    def outputDispatchTable(self):
        assert self.genOpts
        commands = []
        table = ''
        cur_extension = CurrentExtensionTracker(self.conventions.api_version_prefix)

        table += '// Generated dispatch table\n'
        if self.genOpts.filename == 'xr_generated_dispatch_table_core.h':
            table += 'struct XrGeneratedDispatchTableCore {\n'
        else:
            table += 'struct XrGeneratedDispatchTable {\n'

        # functions implemented for the loader are different
        LOADER_FUNCTIONS = [
            'xrCreateApiLayerInstance',
            'xrNegotiateLoaderRuntimeInterface',
            'xrNegotiateLoaderApiLayerInterface',
        ]
        # Loop through both core commands, and extension commands
        # Outputting the core commands first, and then the extension commands.
        for x in range(0, 2):
            if x == 0:
                commands = self.core_commands
            else:
                commands = self.ext_commands

            for cur_cmd in commands:
                if self.genOpts.filename == 'xr_generated_dispatch_table_core.h':
                    if self.isCoreExtensionName(cur_cmd.ext_name):
                        pass
                    # Loader implements XR_EXT_debug_utils
                    elif cur_cmd.ext_name == 'XR_EXT_debug_utils':
                        pass
                    else:
                        # Skip anything that is not core or XR_EXT_debug_utils in the loader dispatch table
                        continue

                # Skip loader-use-only functions in dispatch tables.
                if cur_cmd.name in LOADER_FUNCTIONS:
                    continue

                # If we've switched to a new "feature" print out a comment on what it is.  Usually,
                # this is a group of core commands or a group of commands in an extension.
                assert cur_cmd.ext_name
                table += cur_extension.format_if_extension_changed(cur_cmd.ext_name, "\n    // ---- {} commands\n")

                # Remove 'xr' from proto name
                base_name = cur_cmd.name[2:]

                # If a protect statement exists, use it.
                if cur_cmd.protect_value:
                    table += f'#if {cur_cmd.protect_string}\n'

                # Write out each command using it's function pointer for each command
                table += f'    PFN_{cur_cmd.name} {base_name};\n'

                # If a protect statement exists, wrap it up.
                if cur_cmd.protect_value:
                    table += f'#endif // {cur_cmd.protect_string}\n'
        table += '};\n\n'
        return table

    # Write out the helper function that will populate a dispatch table using
    # an instance handle and a corresponding xrGetInstanceProcAddr command.
    #   self            the ApiDumpOutputGenerator object
    def outputDispatchTableHelper(self):
        assert self.genOpts
        commands = []
        table_helper = ''
        cur_extension = CurrentExtensionTracker(self.conventions.api_version_prefix)

        table_helper += '// Helper function to populate an instance dispatch table\n'
        if self.genOpts.filename == 'xr_generated_dispatch_table_core.c':
            table_helper += 'void GeneratedXrPopulateDispatchTableCore(struct XrGeneratedDispatchTableCore *table,\n'
        else:
            table_helper += 'void GeneratedXrPopulateDispatchTable(struct XrGeneratedDispatchTable *table,\n'
        table_helper += '                                      XrInstance instance,\n'
        table_helper += '                                      PFN_xrGetInstanceProcAddr get_inst_proc_addr) {\n'

        # Loop through both core commands, and extension commands
        # Outputting the core commands first, and then the extension commands.
        for x in range(0, 2):
            if x == 0:
                commands = self.core_commands
            else:
                commands = self.ext_commands

            for cur_cmd in commands:
                # If the command is only manually implemented in the loader,
                # it is not needed anywhere else, so skip it.
                if cur_cmd.name in self.no_trampoline_or_terminator:
                    continue

                if self.genOpts.filename == 'xr_generated_dispatch_table_core.c':
                    if self.isCoreExtensionName(cur_cmd.ext_name):
                        pass
                    # Loader implements XR_EXT_debug_utils
                    elif cur_cmd.ext_name == 'XR_EXT_debug_utils':
                        pass
                    else:
                        # Skip anything that is not core or XR_EXT_debug_utils in the loader dispatch table
                        continue

                # If we've switched to a new "feature" print out a comment on what it is.  Usually,
                # this is a group of core commands or a group of commands in an extension.
                assert cur_cmd.ext_name
                table_helper += cur_extension.format_if_extension_changed(cur_cmd.ext_name, "\n    // ---- {} commands\n")

                # Remove 'xr' from proto name
                base_name = cur_cmd.name[2:]

                if cur_cmd.protect_value:
                    table_helper += f'#if {cur_cmd.protect_string}\n'

                if cur_cmd.name == 'xrGetInstanceProcAddr':
                    # If the command we're filling in is the xrGetInstanceProcAddr command, use
                    # the one passed into this helper function.
                    table_helper += '    table->GetInstanceProcAddr = get_inst_proc_addr;\n'
                else:
                    # Otherwise, fill in the dispatch table with an xrGetInstanceProcAddr call
                    # to the appropriate command.
                    table_helper += '    (get_inst_proc_addr(instance, "%s", (PFN_xrVoidFunction*)&table->%s));\n' % (
                        cur_cmd.name, base_name)

                if cur_cmd.protect_value:
                    table_helper += f'#endif // {cur_cmd.protect_string}\n'
        table_helper += '}\n\n'
        return table_helper
