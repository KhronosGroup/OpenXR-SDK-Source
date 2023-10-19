#!/usr/bin/python3 -i
#
# Copyright (c) 2017-2023, The Khronos Group Inc.
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


from automatic_source_generator import AutomaticSourceOutputGenerator
from generator import write

# UtilitySourceOutputGenerator - subclass of AutomaticSourceOutputGenerator.


class UtilitySourceOutputGenerator(AutomaticSourceOutputGenerator):
    """Generate loader source using XML element attributes from registry"""

    # Override the base class header warning so the comment indicates this file.
    #   self            the UtilitySourceOutputGenerator object
    def outputGeneratedHeaderWarning(self):
        # REUSE-IgnoreStart
        generated_warning = ''
        generated_warning += '// Copyright (c) 2017-2023, The Khronos Group Inc.\n'
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
            raise RuntimeError("Unknown filename extension! " + self.genOpts.filename)

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
            raise RuntimeError("Unknown filename extension! " + self.genOpts.filename)

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
        table_helper += 'void GeneratedXrPopulateDispatchTable(struct XrGeneratedDispatchTable *table,\n'
        table_helper += '                                      XrInstance instance,\n'
        table_helper += '                                      PFN_xrGetInstanceProcAddr get_inst_proc_addr);\n'
        return table_helper

    # Write out a C-style structure used to store the Dispatch table information
    #   self            the ApiDumpOutputGenerator object
    def outputDispatchTable(self):
        assert self.genOpts
        commands = []
        table = ''
        cur_extension_name = ''

        table += '// Generated dispatch table\n'
        table += 'struct XrGeneratedDispatchTable {\n'

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

                # If we've switched to a new "feature" print out a comment on what it is.  Usually,
                # this is a group of core commands or a group of commands in an extension.
                if cur_cmd.ext_name != cur_extension_name:
                    if self.isCoreExtensionName(cur_cmd.ext_name):
                        table += '\n    // ---- Core %s commands\n' % cur_cmd.ext_name[11:].replace(
                            "_", ".")
                    else:
                        table += '\n    // ---- %s extension commands\n' % cur_cmd.ext_name
                    cur_extension_name = cur_cmd.ext_name

                # Remove 'xr' from proto name
                base_name = cur_cmd.name[2:]

                # If a protect statement exists, use it.
                if cur_cmd.protect_value:
                    table += '#if %s\n' % cur_cmd.protect_string

                # Write out each command using it's function pointer for each command
                table += '    PFN_%s %s;\n' % (cur_cmd.name, base_name)

                # If a protect statement exists, wrap it up.
                if cur_cmd.protect_value:
                    table += '#endif // %s\n' % cur_cmd.protect_string
        table += '};\n\n'
        return table

    # Write out the helper function that will populate a dispatch table using
    # an instance handle and a corresponding xrGetInstanceProcAddr command.
    #   self            the ApiDumpOutputGenerator object
    def outputDispatchTableHelper(self):
        assert self.genOpts
        commands = []
        table_helper = ''
        cur_extension_name = ''

        table_helper += '// Helper function to populate an instance dispatch table\n'
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
                if cur_cmd.ext_name != cur_extension_name:
                    if self.isCoreExtensionName(cur_cmd.ext_name):
                        table_helper += '\n    // ---- Core %s commands\n' % cur_cmd.ext_name[11:].replace(
                            "_", ".")
                    else:
                        table_helper += '\n    // ---- %s extension commands\n' % cur_cmd.ext_name
                    cur_extension_name = cur_cmd.ext_name

                # Remove 'xr' from proto name
                base_name = cur_cmd.name[2:]

                if cur_cmd.protect_value:
                    table_helper += '#if %s\n' % cur_cmd.protect_string

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
                    table_helper += '#endif // %s\n' % cur_cmd.protect_string
        table_helper += '}\n\n'
        return table_helper
