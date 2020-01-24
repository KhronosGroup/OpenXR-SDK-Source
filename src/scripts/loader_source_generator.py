#!/usr/bin/python3 -i
#
# Copyright (c) 2017-2019 The Khronos Group Inc.
# Copyright (c) 2017-2019 Valve Corporation
# Copyright (c) 2017-2019 LunarG, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Author(s):    Mark Young <marky@lunarg.com>
#
# Purpose:      This file utilizes the content formatted in the
#               automatic_source_generator.py class to produce the
#               generated source code for the loader.

from automatic_source_generator import (AutomaticSourceOutputGenerator,
                                        undecorate)
from generator import write

# The following commands are manually implemented in the loader.
MANUAL_LOADER_FUNCS = set((
    'xrGetInstanceProcAddr',
    'xrEnumerateApiLayerProperties',
    'xrEnumerateInstanceExtensionProperties',
    'xrCreateInstance',
    'xrDestroyInstance',

    # For XR_EXT_debug_utils:
    'xrCreateDebugUtilsMessengerEXT',
    'xrDestroyDebugUtilsMessengerEXT',
    'xrSessionBeginDebugUtilsLabelRegionEXT',
    'xrSessionEndDebugUtilsLabelRegionEXT',
    'xrSessionInsertDebugUtilsLabelEXT',
))

# This is a list of extensions that the loader implements.  This means that
# the runtime underneath may not support these extensions and the terminators
# need to check before they call
EXTENSIONS_LOADER_IMPLEMENTS = [
    'XR_EXT_debug_utils'
]


def generateErrorMessage(indent_level, vuid, cur_cmd, message, object_info):
    lines = []
    lines.append('LoaderLogger::LogValidationErrorMessage(')
    lines.append('    "VUID-{}",'.format('-'.join(vuid)))
    lines.append('    "{}",'.format(cur_cmd.name))
    lines.append('    {},'.format(message))

    object_info_constructors = ['XrSdkLogObjectInfo{%s, %s}' % p for p in object_info]
    if len(object_info_constructors) <= 1:
        lines.append('    {%s});' % (', '.join(object_info_constructors)))
    else:
        lines.append('    {')
        lines.append(',\n'.join('    %s' % x for x in object_info_constructors))
        lines.append('    });')
    if isinstance(indent_level, str):
        base_indent = indent_level
    else:
        base_indent = (4 * indent_level) * ' '
    indented_lines_with_lf = (''.join((base_indent, line, '\n')) for line in lines)
    return ''.join(indented_lines_with_lf)


# LoaderSourceOutputGenerator - subclass of AutomaticSourceOutputGenerator.
class LoaderSourceOutputGenerator(AutomaticSourceOutputGenerator):
    """Generate loader source using XML element attributes from registry"""

    def getProto(self, cur_cmd, allow_export=True):
        # Start by making it a C calling convention
        func_proto = cur_cmd.cdecl.replace(
            "XRAPI_ATTR", 'extern "C" XRAPI_ATTR')
        if allow_export and self.isCoreExtensionName(cur_cmd.ext_name):
            # Export core functions, if permitted.
            func_proto = func_proto.replace(
                "XRAPI_ATTR", 'LOADER_EXPORT XRAPI_ATTR')
        return func_proto

    # Override the base class header warning so the comment indicates this file.
    #   self            the LoaderSourceOutputGenerator object

    def outputGeneratedHeaderWarning(self):
        generated_warning = '// *********** THIS FILE IS GENERATED - DO NOT EDIT ***********\n'
        generated_warning += '//     See loader_source_generator.py for modifications\n'
        generated_warning += '// ************************************************************\n'
        write(generated_warning, file=self.outFile)

    # Call the base class to properly begin the file, and then add
    # the file-specific header information.
    #   self            the LoaderSourceOutputGenerator object
    #   gen_opts        the LoaderSourceGeneratorOptions object
    def beginFile(self, genOpts):
        AutomaticSourceOutputGenerator.beginFile(self, genOpts)
        preamble = ''

        if self.genOpts.filename == 'xr_generated_loader.hpp':
            preamble += '#pragma once\n'
            preamble += '#include <unordered_map>\n'
            preamble += '#include <thread>\n'
            preamble += '#include <mutex>\n\n'
            preamble += '#include "xr_dependencies.h"\n'
            preamble += '#include "openxr/openxr.h"\n'
            preamble += '#include "openxr/openxr_platform.h"\n\n'
            preamble += '#include "loader_interfaces.h"\n\n'
            preamble += '#include "loader_instance.hpp"\n\n'
            preamble += '#include "loader_platform.hpp"\n\n'

        elif self.genOpts.filename == 'xr_generated_loader.cpp':
            preamble += '#include "xr_generated_loader.hpp"\n\n'
            preamble += '#include "api_layer_interface.hpp"\n'
            preamble += '#include "exception_handling.hpp"\n'
            preamble += '#include "hex_and_handles.h"\n'
            preamble += '#include "loader_instance.hpp"\n'
            preamble += '#include "loader_logger.hpp"\n'
            preamble += '#include "loader_platform.hpp"\n'
            preamble += '#include "runtime_interface.hpp"\n'
            preamble += '#include "xr_generated_dispatch_table.h"\n\n'

            preamble += '#include "xr_dependencies.h"\n'
            preamble += '#include <openxr/openxr.h>\n'
            preamble += '#include <openxr/openxr_platform.h>\n\n'

            preamble += '#include <cstring>\n'
            preamble += '#include <memory>\n'
            preamble += '#include <new>\n'
            preamble += '#include <string>\n'
            preamble += '#include <unordered_map>\n'

        write(preamble, file=self.outFile)

    # Write out all the information for the appropriate file,
    # and then call down to the base class to wrap everything up.
    #   self            the LoaderSourceOutputGenerator object
    def endFile(self):
        file_data = ''

        if self.genOpts.filename == 'xr_generated_loader.hpp':
            file_data += '#ifdef __cplusplus\n'
            file_data += 'extern "C" { \n'
            file_data += '#endif\n'
            file_data += self.outputLoaderManualFuncs()
            file_data += '#ifdef __cplusplus\n'
            file_data += '} // extern "C"\n'
            file_data += '#endif\n'

        elif self.genOpts.filename == 'xr_generated_loader.cpp':
            file_data += self.outputLoaderGeneratedFuncs()

        write(file_data, file=self.outFile)

        # Finish processing in superclass
        AutomaticSourceOutputGenerator.endFile(self)

    # Create prototypes for the loader's manually generated functions
    # so the generated code can call them.
    #   self            the LoaderSourceOutputGenerator object
    def outputLoaderManualFuncs(self):
        commands = []
        cur_extension_name = ''
        manual_funcs = '\n// Loader manually generated function prototypes\n\n'
        for x in range(0, 2):
            if x == 0:
                commands = self.core_commands
            else:
                commands = self.ext_commands

            for cur_cmd in commands:
                if not cur_cmd.name in MANUAL_LOADER_FUNCS:
                    if cur_cmd.ext_name != cur_extension_name:
                        if self.isCoreExtensionName(cur_cmd.ext_name):
                            manual_funcs += '\n// ---- Core %s loader manual functions\n' % cur_cmd.ext_name[11:].replace(
                                "_", ".")
                        else:
                            manual_funcs += '\n// ---- %s loader manual functions\n' % cur_cmd.ext_name
                        cur_extension_name = cur_cmd.ext_name

                    if cur_cmd.protect_value:
                        manual_funcs += '#if %s\n' % cur_cmd.protect_string

                    func_proto = self.getProto(cur_cmd)

                    # Output the standard API form of the command
                    manual_funcs += func_proto
                    manual_funcs += '\n'

                    if cur_cmd.protect_value:
                        manual_funcs += '#endif // %s\n' % cur_cmd.protect_string

        return manual_funcs

   # Output loader generated functions.  This has special cases for create and destroy commands
    # since we have to associate the created objects with the original instance during the create,
    # and then remove that association in the delete.
    #   self            the LoaderSourceOutputGenerator object
    def outputLoaderGeneratedFuncs(self):
        cur_extension_name = ''
        generated_funcs = '\n// Automatically generated instance trampolines and terminators\n'
        count = 0
        for x in range(0, 2):
            if x == 0:
                commands = self.core_commands
            else:
                commands = self.ext_commands

            for cur_cmd in commands:
                if cur_cmd.ext_name != cur_extension_name:
                    if self.isCoreExtensionName(cur_cmd.ext_name):
                        generated_funcs += '\n// ---- Core %s commands\n' % cur_cmd.ext_name[11:].replace(
                            "_", ".")
                    else:
                        generated_funcs += '\n// ---- %s extension commands\n' % cur_cmd.ext_name
                    cur_extension_name = cur_cmd.ext_name

                if cur_cmd.name in MANUAL_LOADER_FUNCS:
                    continue

                # Remove 'xr' from proto name
                base_name = cur_cmd.name[2:]

                has_return = False

                if cur_cmd.is_create_connect or cur_cmd.is_destroy_disconnect:
                    has_return = True
                elif cur_cmd.return_type is not None:
                    has_return = True

                tramp_variable_defines = ''
                tramp_param_replace = []
                base_handle_name = ''

                for count, param in enumerate(cur_cmd.params):
                    param_cdecl = param.cdecl
                    is_const = False
                    const_check = param_cdecl.strip()
                    if const_check[:5].lower() == "const":
                        is_const = True
                    pointer_count = self.paramPointerCount(
                        param.cdecl, param.type, param.name)
                    array_dimen = self.paramArrayDimension(
                        param.cdecl, param.type, param.name)

                    static_array_sizes = []
                    if param.is_static_array:
                        static_array_sizes = param.static_array_sizes

                    cmd_tramp_param_name = param.name
                    cmd_tramp_is_handle = param.is_handle
                    if count == 0:
                        if param.is_handle:
                            base_handle_name = undecorate(param.type)
                            first_handle_name = self.getFirstHandleName(param)

                            tramp_variable_defines += '    LoaderInstance* loader_instance;\n'
                            tramp_variable_defines += '    XrResult result = ActiveLoaderInstance::Get(&loader_instance, "%s");\n' % (cur_cmd.name)
                            tramp_variable_defines += '    if (XR_SUCCEEDED(result)) {\n'

                            # These should be mutually exclusive - verify it.
                            assert((not cur_cmd.is_destroy_disconnect) or
                                   (pointer_count == 0))
                        else:
                            tramp_variable_defines += self.printCodeGenErrorMessage(
                                'Command %s does not have an OpenXR Object handle as the first parameter.' % cur_cmd.name)

                    tramp_param_replace.append(
                        self.MemberOrParam(type=param.type,
                                           name=cmd_tramp_param_name,
                                           is_const=is_const,
                                           is_handle=cmd_tramp_is_handle,
                                           is_bool=param.is_bool,
                                           is_optional=param.is_optional,
                                           no_auto_validity=param.no_auto_validity,
                                           is_array=param.is_array,
                                           is_static_array=param.is_static_array,
                                           static_array_sizes=static_array_sizes,
                                           array_dimen=array_dimen,
                                           array_count_var=param.array_count_var,
                                           array_length_for=param.array_length_for,
                                           pointer_count=pointer_count,
                                           pointer_count_var=param.pointer_count_var,
                                           is_null_terminated=param.is_null_terminated,
                                           valid_extension_structs=None,
                                           cdecl=param.cdecl,
                                           values=param.values))
                    count = count + 1

                if cur_cmd.protect_value:
                    generated_funcs += '#if %s\n' % cur_cmd.protect_string
                decl = self.getProto(cur_cmd).replace(";", " XRLOADER_ABI_TRY {\n")

                generated_funcs += decl
                generated_funcs += tramp_variable_defines

                # If this is not core, but an extension, check to make sure the extension is enabled.
                if x == 1:
                    generated_funcs += '        if (!loader_instance->ExtensionIsEnabled("%s")) {\n' % (
                        cur_cmd.ext_name)
                    generated_funcs += '            LoaderLogger::LogValidationErrorMessage("VUID-%s-extension-notenabled",\n' % cur_cmd.name
                    generated_funcs += '                                                    "%s",\n' % cur_cmd.name
                    generated_funcs += '                                                    "The %s extension has not been enabled prior to calling %s");\n' % (
                        cur_cmd.ext_name, cur_cmd.name)
                    if has_return:
                        generated_funcs += '            return XR_ERROR_FUNCTION_UNSUPPORTED;\n'
                    else:
                        generated_funcs += '            return;\n'
                    generated_funcs += '        }\n\n'

                if has_return:
                    generated_funcs += '        result = '
                else:
                    generated_funcs += '        '

                generated_funcs += 'loader_instance->DispatchTable()->'
                generated_funcs += base_name
                generated_funcs += '('
                count = 0
                for param in tramp_param_replace:
                    if count > 0:
                        generated_funcs += ', '
                    generated_funcs += param.name
                    count = count + 1
                generated_funcs += ');\n'

                generated_funcs += '    }\n'


                if has_return:
                    generated_funcs += '    return result;\n'

                generated_funcs += '}\nXRLOADER_ABI_CATCH_FALLBACK\n'

                if cur_cmd.protect_value:
                    generated_funcs += '#endif // %s\n' % cur_cmd.protect_string
                generated_funcs += '\n'
        return generated_funcs