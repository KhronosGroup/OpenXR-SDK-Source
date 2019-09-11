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

# The following commands are manually implemented in the loader as trampoline
# and terminator calls since they take an instance handle.
MANUAL_LOADER_INSTANCE_FUNCS = set((
    'xrGetInstanceProcAddr',
    'xrEnumerateApiLayerProperties',
    'xrEnumerateInstanceExtensionProperties',
    'xrCreateInstance',
    'xrDestroyInstance',
    # Extensions
    #  XR_EXT_debug_utils (both trampoline and terminator are manually defined)
    'xrCreateDebugUtilsMessengerEXT',
    'xrDestroyDebugUtilsMessengerEXT',
))

# The following commands are manually implemented in the loader as trampoline
# calls since they do not take an instance handle.
MANUAL_LOADER_NONINSTANCE_FUNCS = set((
    'xrSessionBeginDebugUtilsLabelRegionEXT',
    'xrSessionEndDebugUtilsLabelRegionEXT',
    'xrSessionInsertDebugUtilsLabelEXT',
))

MANUAL_LOADER_FUNCS = MANUAL_LOADER_INSTANCE_FUNCS.union(MANUAL_LOADER_NONINSTANCE_FUNCS)

# The terminators for the following commands are manually implemented in the
# loader.
MANUAL_LOADER_INSTANCE_TERMINATOR_FUNCS = set((
    'xrSubmitDebugUtilsMessageEXT',
    'xrSetDebugUtilsObjectNameEXT',
))

VALID_FOR_NULL_INSTANCE_GIPA = set((
    'xrEnumerateInstanceExtensionProperties',
    'xrEnumerateInstanceLayerProperties',
    'xrCreateInstance',
))

# Nothing currently needs a special-cased terminator
NEEDS_TERMINATOR = set()

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
            file_data += self.outputLoaderGeneratedPrototypes()
            file_data += '#ifdef __cplusplus\n'
            file_data += '} // extern "C"\n'
            file_data += '#endif\n'
            file_data += self.outputLoaderMapExterns()

        elif self.genOpts.filename == 'xr_generated_loader.cpp':
            file_data += self.outputLoaderMapDefines()
            file_data += self.outputLoaderGeneratedFuncs()
            file_data += self.outputLoaderExportFuncs()

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
                if cur_cmd.name in MANUAL_LOADER_FUNCS or \
                    cur_cmd.name in MANUAL_LOADER_INSTANCE_TERMINATOR_FUNCS or \
                        cur_cmd.has_instance:
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

                    # If this is a command is implemented in the loader manually, but not only for
                    # loader, add a prototype for the terminator (unless it doesn't have a terminator)
                    if ((cur_cmd.name in MANUAL_LOADER_INSTANCE_FUNCS or cur_cmd.name in MANUAL_LOADER_INSTANCE_TERMINATOR_FUNCS)
                            and cur_cmd.name not in self.no_trampoline_or_terminator):
                        manual_funcs += self.getProto(cur_cmd, allow_export=False).replace(
                            "XRAPI_CALL xr", "XRAPI_CALL LoaderXrTerm")
                        manual_funcs += '\n'

                    if cur_cmd.protect_value:
                        manual_funcs += '#endif // %s\n' % cur_cmd.protect_string

        # Add one that we need for layer termination
        manual_funcs += '\n// Special use function to handle creating API Layer information during xrCreateInstance\n'
        manual_funcs += 'XRAPI_ATTR XrResult XRAPI_CALL LoaderXrTermCreateApiLayerInstance(const XrInstanceCreateInfo* info,\n'
        manual_funcs += '                                                                  const struct XrApiLayerCreateInfo* apiLayerInfo,\n'
        manual_funcs += '                                                                  XrInstance* instance);\n'
        manual_funcs += '\n'
        return manual_funcs

    # Create a prototype for initializing the instance dispatch table for the loader.
    #   self            the LoaderSourceOutputGenerator object
    def outputLoaderGeneratedPrototypes(self):
        generated_protos = '\n// Generated loader terminator prototypes\n'
        for x in range(0, 2):
            if x == 0:
                commands = self.core_commands
            else:
                commands = self.ext_commands

            for cur_cmd in commands:
                if cur_cmd.name in NEEDS_TERMINATOR:
                    if cur_cmd.protect_value:
                        generated_protos += '#if %s\n' % cur_cmd.protect_string
                    # Output the standard API form of the command
                    raise RuntimeError
                    generated_protos += cur_cmd.cdecl.replace(
                        "XRAPI_CALL xr", "XRAPI_CALL LoaderGenTermXr")
                    generated_protos += '\n'
                    if cur_cmd.protect_value:
                        generated_protos += '#endif // %s\n' % cur_cmd.protect_string

        generated_protos += '// Instance Init Dispatch Table (put all terminators in first)\n'
        generated_protos += 'void LoaderGenInitInstanceDispatchTable(XrInstance runtime_instance,\n'
        generated_protos += '                                        std::unique_ptr<XrGeneratedDispatchTable>& table);\n\n'
        return generated_protos

    # Output global externs of unordered_maps and mutexes for each handle type.
    #   self            the LoaderSourceOutputGenerator object
    def outputLoaderMapExterns(self):
        map_externs = '\n// Unordered maps and mutexes to lookup the instance for a given object type\n'
        for handle in self.api_handles:
            if handle.protect_value:
                map_externs += '#if %s\n' % handle.protect_string
            base_handle_name = undecorate(handle.name)
            map_externs += 'extern HandleLoaderMap<%s> g_%s_map;\n' % (
                handle.name, base_handle_name)
            if handle.protect_value:
                map_externs += '#endif // %s\n' % handle.protect_string
        map_externs += '\n'
        map_externs += '// Function used to clean up any residual map values that point to an instance prior to that\n'
        map_externs += '// instance being deleted.\n'
        map_externs += 'void LoaderCleanUpMapsForInstance(LoaderInstance *instance);\n'
        map_externs += '\n'
        return map_externs

    # Instantiate the unordered_maps and mutexes for each of the object types.  Also, output a utility
    # function that can be used to clean up everything for a particular instance if we get a xrDestroyInstance
    # call.
    #   self            the LoaderSourceOutputGenerator object
    def outputLoaderMapDefines(self):
        map_defines = '// Unordered maps to lookup the instance for a given object type\n'
        for handle in self.api_handles:
            base_handle_name = undecorate(handle.name)
            if handle.protect_value:
                map_defines += '#if %s\n' % handle.protect_string
            map_defines += 'HandleLoaderMap<%s> g_%s_map;\n' % (
                handle.name, base_handle_name)
            if handle.protect_value:
                map_defines += '#endif // %s\n' % handle.protect_string
        map_defines += '\n'
        map_defines += '// Function used to clean up any residual map values that point to an instance prior to that\n'
        map_defines += '// instance being deleted.\n'
        map_defines += 'void LoaderCleanUpMapsForInstance(LoaderInstance *instance) {\n'
        for handle in self.api_handles:
            if handle.protect_value:
                map_defines += '#if %s\n' % handle.protect_string
            base_handle_name = undecorate(handle.name)
            map_defines += '    g_%s_map.RemoveHandlesForLoader(*instance);\n' % base_handle_name
            if handle.protect_value:
                map_defines += '#endif // %s\n' % handle.protect_string
        map_defines += '}\n\n'

        return map_defines

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

                just_return_call = False
                has_return = False

                if cur_cmd.is_create_connect or cur_cmd.is_destroy_disconnect:
                    has_return = True
                elif cur_cmd.return_type is not None:
                    has_return = True
                    just_return_call = True

                tramp_variable_defines = ''
                tramp_param_replace = []
                func_follow_up = ''
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
                            if pointer_count == 1 and param.pointer_count_var is not None:
                                if not param.is_optional:
                                    # Check we have at least 1 in the array.
                                    tramp_variable_defines += '    if (0 == %s) {\n' % param.pointer_count_var
                                    tramp_variable_defines += generateErrorMessage(
                                        3,
                                        [cur_cmd.name, param.pointer_count_var, 'parameter'],
                                        cur_cmd,
                                        '"%s is 0, but %s is not optional"' % (param.pointer_count_var, param.name),
                                        [(first_handle_name, self.genXrObjectType(param.type))]
                                    )
                                    tramp_variable_defines += '    }\n'
                            tramp_variable_defines += '    LoaderInstance *loader_instance = g_%s_map.Get(%s);\n' % (
                                base_handle_name, first_handle_name)
                            if cur_cmd.is_destroy_disconnect:
                                tramp_variable_defines += '    // Destroy the mapping entry for this item if it was valid.\n'
                                tramp_variable_defines += '    if (nullptr != loader_instance) {\n'
                                tramp_variable_defines += '            g_%s_map.Erase(%s);\n' % (
                                    base_handle_name, first_handle_name)
                                tramp_variable_defines += '    }\n'
                            # These should be mutually exclusive - verify it.
                            assert((not cur_cmd.is_destroy_disconnect) or
                                   (pointer_count == 0))

                            if pointer_count == 1:
                                # NOTE - @ 10-June-2019 this stanza is never exercised in loader code-gen. Consider whether necessary. DJH
                                tramp_variable_defines += '    for (uint32_t i = 1; i < %s; ++i) {\n' % param.pointer_count_var
                                tramp_variable_defines += '        LoaderInstance *elt_loader_instance = g_%s_map.Get(%s[i]);\n' % (
                                    base_handle_name, param.name)
                                tramp_variable_defines += '        if (elt_loader_instance == nullptr || elt_loader_instance != loader_instance) {\n'
                                tramp_variable_defines += '            auto elt_name = "%s[" + std::to_string(i) + "]";\n' % param.name
                                tramp_variable_defines += '            if (elt_loader_instance == nullptr) {\n'
                                tramp_variable_defines += generateErrorMessage(
                                    '                    ',
                                    [cur_cmd.name, param.name, 'parameter'],
                                    cur_cmd,
                                    'elt_name + " is not a valid %s"' % (param.name),
                                    [(first_handle_name + '[i]', self.genXrObjectType(param.type))]
                                )
                                tramp_variable_defines += '            } else {\n'
                                tramp_variable_defines += generateErrorMessage(
                                    '                    ',
                                    [cur_cmd.name, param.name, 'parameter'],
                                    cur_cmd,
                                    '"%s[" + std::to_string(i) + "] belongs to a different instance than %s[0]"' % (param.name, param.name),
                                    [(first_handle_name + '[i]', self.genXrObjectType(param.type))]
                                )
                                tramp_variable_defines += '            }\n'
                                if has_return:
                                    tramp_variable_defines += '            return XR_ERROR_HANDLE_INVALID;\n'
                                tramp_variable_defines += '        }\n'
                                tramp_variable_defines += '    }\n'
                            tramp_variable_defines += '    if (nullptr == loader_instance) {\n'
                            tramp_variable_defines += generateErrorMessage(
                                '            ',
                                [cur_cmd.name, param.name, 'parameter'],
                                cur_cmd,
                                '"%s is not a valid %s"' % (first_handle_name, param.type),
                                [(first_handle_name, self.genXrObjectType(param.type))]
                            )
                            if has_return:
                                tramp_variable_defines += '        return XR_ERROR_HANDLE_INVALID;\n'
                            tramp_variable_defines += '    }\n'
                        else:
                            tramp_variable_defines += self.printCodeGenErrorMessage(
                                'Command %s does not have an OpenXR Object handle as the first parameter.' % cur_cmd.name)

                        tramp_variable_defines += '    const std::unique_ptr<XrGeneratedDispatchTable>& dispatch_table = loader_instance->DispatchTable();\n'

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

                    if count == len(cur_cmd.params) - 1:
                        if param.is_handle:
                            base_handle_name = undecorate(param.type)
                            if cur_cmd.is_create_connect:
                                func_follow_up += '        if (XR_SUCCESS == result && nullptr != %s) {\n' % param.name
                                func_follow_up += '            XrResult insert_result = g_%s_map.Insert(*%s, *loader_instance);\n' % (
                                    base_handle_name, param.name)
                                func_follow_up += '            if (XR_FAILED(insert_result)) {\n'
                                func_follow_up += '                LoaderLogger::LogErrorMessage(\n'
                                func_follow_up += '                    "%s",\n' % cur_cmd.name
                                func_follow_up += '                    "Failed inserting new %s into map: may be null or not unique");\n' % base_handle_name
                                func_follow_up += '            }\n'
                                func_follow_up += '        }\n'
                    count = count + 1

                if has_return and not just_return_call:
                    return_prefix = '        '
                    return_prefix += cur_cmd.return_type.text
                    return_prefix += ' result'
                    if cur_cmd.return_type.text == 'XrResult':
                        return_prefix += ' = XR_SUCCESS;\n'
                    else:
                        return_prefix += ';\n'
                    tramp_variable_defines += return_prefix

                if cur_cmd.protect_value:
                    generated_funcs += '#if %s\n' % cur_cmd.protect_string
                decl = self.getProto(cur_cmd).replace(";", " XRLOADER_ABI_TRY {\n")

                generated_funcs += decl
                generated_funcs += tramp_variable_defines

                # If this is not core, but an extension, check to make sure the extension is enabled.
                if x == 1:
                    generated_funcs += '    if (!loader_instance->ExtensionIsEnabled("%s")) {\n' % (
                        cur_cmd.ext_name)
                    generated_funcs += '        LoaderLogger::LogValidationErrorMessage("VUID-%s-extension-notenabled",\n' % cur_cmd.name
                    generated_funcs += '                                                "%s",\n' % cur_cmd.name
                    generated_funcs += '                                                "The %s extension has not been enabled prior to calling %s");\n' % (
                        cur_cmd.ext_name, cur_cmd.name)
                    if has_return:
                        generated_funcs += '        return XR_ERROR_FUNCTION_UNSUPPORTED;\n'
                    else:
                        generated_funcs += '        return;\n'
                    generated_funcs += '    }\n\n'

                if has_return:
                    if just_return_call:
                        generated_funcs += '    return '
                    else:
                        generated_funcs += '    result = '
                else:
                    generated_funcs += '    '

                generated_funcs += 'dispatch_table->'
                generated_funcs += base_name
                generated_funcs += '('
                count = 0
                for param in tramp_param_replace:
                    if count > 0:
                        generated_funcs += ', '
                    generated_funcs += param.name
                    count = count + 1
                generated_funcs += ');\n'

                generated_funcs += func_follow_up

                if has_return and not just_return_call:
                    generated_funcs += '    return result;\n'

                generated_funcs += '}\nXRLOADER_ABI_CATCH_FALLBACK\n\n'

                # If this is a function that needs a terminator, provide the call to it, not the runtime.
                # Only a few items need a terminator.  Usually something we want to be able to return information
                # to the API layers and act as an interceptor prior to the runtime.
                if cur_cmd.name in NEEDS_TERMINATOR:
                    # Not currently used.
                    raise NotImplementedError
                    term_decl = cur_cmd.cdecl.replace(";", " {\n")
                    term_decl = term_decl.replace(" xr", " LoaderGenTermXr")
                    generated_funcs += term_decl

                    loader_override_func = False

                    if cur_cmd.ext_name in EXTENSIONS_LOADER_IMPLEMENTS or loader_override_func:
                        generated_funcs += '    if (nullptr != dispatch_table->%s) {\n' % base_name

                    if has_return:
                        if just_return_call:
                            generated_funcs += '    return '
                        else:
                            generated_funcs += '    result = '
                    else:
                        generated_funcs += '    '

                    generated_funcs += 'dispatch_table->'
                    generated_funcs += base_name
                    generated_funcs += '('
                    count = 0
                    for param in cur_cmd.params:
                        if count > 0:
                            generated_funcs += ', '
                        generated_funcs += param.name
                        count = count + 1
                    generated_funcs += ');\n'

                    if cur_cmd.ext_name in EXTENSIONS_LOADER_IMPLEMENTS or loader_override_func:
                        generated_funcs += '    }\n'

                    if has_return and not just_return_call:
                        generated_funcs += '    return result;\n'
                    generated_funcs += '}\n'
                if cur_cmd.protect_value:
                    generated_funcs += '#endif // %s\n' % cur_cmd.protect_string
                generated_funcs += '\n'
        return generated_funcs

    # Output the source of all the loader export functions.  This includes:
    #  - The Loader's xrGetInstanceProcAddr trampoline
    #  - The Loader's xrGetInstanceProcAddr terminator
    #  - A Utility function for initializing a dispatch table
    #  - A Utility function for updating a dispatch table
    #   self            the LoaderSourceOutputGenerator object
    def outputLoaderExportFuncs(self):
        cur_extension_name = ''

        export_funcs = '\n'
        export_funcs += 'LOADER_EXPORT XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance instance, const char* name,\n'
        export_funcs += '                                                                   PFN_xrVoidFunction* function) XRLOADER_ABI_TRY {\n'
        indent = 1
        export_funcs += self.writeIndent(indent)
        export_funcs += 'if (nullptr == function) {\n'
        export_funcs += self.writeIndent(indent + 1)
        export_funcs += 'LoaderLogger::LogValidationErrorMessage("VUID-xrGetInstanceProcAddr-function-parameter",\n'
        export_funcs += self.writeIndent(indent + 1)
        export_funcs += '                                        "xrGetInstanceProcAddr", "Invalid Function pointer");\n'
        export_funcs += self.writeIndent(indent + 1)
        export_funcs += '    return XR_ERROR_VALIDATION_FAILURE;\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += '}\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += 'if (nullptr == name) {\n'
        export_funcs += self.writeIndent(indent + 1)
        export_funcs += 'LoaderLogger::LogValidationErrorMessage("VUID-xrGetInstanceProcAddr-function-parameter",\n'
        export_funcs += self.writeIndent(indent + 1)
        export_funcs += '                                        "xrGetInstanceProcAddr", "Invalid Name pointer");\n'
        export_funcs += self.writeIndent(indent + 1)
        export_funcs += '    return XR_ERROR_VALIDATION_FAILURE;\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += '}\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += '// Initialize the function to nullptr in case it does not get caught in a known case\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += '*function = nullptr;\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += 'if (name[0] == \'x\' && name[1] == \'r\') {\n'
        indent = indent + 1
        export_funcs += self.writeIndent(indent)
        export_funcs += 'std::string func_name = &name[2];\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += 'LoaderInstance * const loader_instance = g_instance_map.Get(instance);\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += 'if (instance == XR_NULL_HANDLE) {\n'
        indent = indent + 1
        export_funcs += self.writeIndent(indent)
        export_funcs += '// Null instance is allowed for 3 specific API entry points, otherwise return error\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += 'if (!((func_name == "CreateInstance") ||\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += '      (func_name == "EnumerateApiLayerProperties") ||\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += '      (func_name == "EnumerateInstanceExtensionProperties"))) {\n'
        indent = indent + 1
        export_funcs += self.writeIndent(indent)
        export_funcs += 'std::string error_str = "XR_NULL_HANDLE for instance but query for ";\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += 'error_str += name;\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += 'error_str += " requires a valid instance";\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += 'LoaderLogger::LogValidationErrorMessage("VUID-xrGetInstanceProcAddr-instance-parameter",\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += '                                        "xrGetInstanceProcAddr", error_str);\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += 'return XR_ERROR_HANDLE_INVALID;\n'
        indent = indent - 1
        export_funcs += self.writeIndent(indent)
        export_funcs += '}\n'
        indent = indent - 1
        export_funcs += self.writeIndent(indent)
        export_funcs += '}\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += 'else if (loader_instance == nullptr) {\n'
        indent = indent + 1
        export_funcs += self.writeIndent(indent)
        export_funcs += 'std::string error_str = "Invalid handle for instance (query for ";\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += 'error_str += name;\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += 'error_str += " )";\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += 'LoaderLogger::LogValidationErrorMessage("VUID-xrGetInstanceProcAddr-instance-parameter",\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += '                                        "xrGetInstanceProcAddr", error_str);\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += 'return XR_ERROR_HANDLE_INVALID;\n'
        indent = indent - 1
        export_funcs += self.writeIndent(indent)
        export_funcs += '}\n'

        count = 0
        for x in range(0, 2):
            if x == 0:
                commands = self.core_commands
            else:
                commands = self.ext_commands

            for cur_cmd in commands:
                if cur_cmd.ext_name != cur_extension_name:
                    if self.isCoreExtensionName(cur_cmd.ext_name):
                        export_funcs += '\n'
                        export_funcs += self.writeIndent(indent)
                        export_funcs += '// ---- Core %s commands\n\n' % cur_cmd.ext_name[11:].replace(
                            "_", ".")
                    else:
                        export_funcs += '\n'
                        export_funcs += self.writeIndent(indent)
                        export_funcs += '// ---- %s extension commands\n\n' % cur_cmd.ext_name
                    cur_extension_name = cur_cmd.ext_name

                # Remove 'xr' from proto name
                base_name = cur_cmd.name[2:]

                if cur_cmd.protect_value:
                    export_funcs += '#if %s\n' % cur_cmd.protect_string

                if count == 0:
                    export_funcs += self.writeIndent(indent)
                    export_funcs += 'if (func_name == "%s") {\n' % (base_name)
                else:
                    export_funcs += self.writeIndent(indent)
                    export_funcs += '} else if (func_name == "%s") {\n' % (
                        base_name)
                indent = indent + 1
                count = count + 1

                # Instance commands always need to start with trampoline to properly de-reference instance
                if self.isCoreExtensionName(cur_cmd.ext_name):
                    if cur_cmd.has_instance or cur_cmd.name in MANUAL_LOADER_FUNCS:
                        export_funcs += self.writeIndent(indent)
                        export_funcs += '*function = reinterpret_cast<PFN_xrVoidFunction>(%s);\n' % (
                            cur_cmd.name)
                    else:
                        export_funcs += self.writeIndent(indent)
                        export_funcs += '*function = reinterpret_cast<PFN_xrVoidFunction>(loader_instance->DispatchTable()->%s);\n' % (
                            base_name)
                else:
                    export_funcs += self.writeIndent(indent)
                    export_funcs += 'if (loader_instance->ExtensionIsEnabled("%s")) {\n' % (
                        cur_cmd.ext_name)
                    export_funcs += self.writeIndent(indent + 1)
                    if cur_cmd.has_instance or cur_cmd.name in MANUAL_LOADER_FUNCS:
                        export_funcs += '*function = reinterpret_cast<PFN_xrVoidFunction>(%s);\n' % (
                            cur_cmd.name)
                    else:
                        export_funcs += '*function = reinterpret_cast<PFN_xrVoidFunction>(loader_instance->DispatchTable()->%s);\n' % (
                            base_name)
                    export_funcs += self.writeIndent(indent)
                    export_funcs += '}\n'

                if cur_cmd.protect_value:
                    export_funcs += '#endif // %s\n' % cur_cmd.protect_string

                indent = indent - 1
        export_funcs += self.writeIndent(indent)
        export_funcs += '}\n'
        indent = indent - 1
        export_funcs += self.writeIndent(indent)
        export_funcs += '}\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += 'if (*function == nullptr) {\n'
        export_funcs += self.writeIndent(indent + 1)
        export_funcs += 'return XR_ERROR_FUNCTION_UNSUPPORTED;\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += '}\n'
        export_funcs += self.writeIndent(indent)
        export_funcs += 'return XR_SUCCESS;\n'
        export_funcs += '}\n'
        export_funcs += 'XRLOADER_ABI_CATCH_FALLBACK\n'

        export_funcs += '\n// Terminator GetInstanceProcAddr function\n'
        export_funcs += 'XRAPI_ATTR XrResult XRAPI_CALL LoaderXrTermGetInstanceProcAddr(XrInstance instance, const char* name,\n'
        export_funcs += '                                                               PFN_xrVoidFunction* function) XRLOADER_ABI_TRY {\n'

        count = 0
        for x in range(0, 2):
            if x == 0:
                commands = self.core_commands
            else:
                commands = self.ext_commands

            for cur_cmd in commands:
                # Remove 'xr' from proto name
                base_name = cur_cmd.name[2:]

                # If this is a function that needs a terminator, provide the call to it, not the runtime.
                # Anything with an XrInstance requires a terminator so we can unwrap it properly for the
                # runtime.
                if ((cur_cmd.name in MANUAL_LOADER_INSTANCE_FUNCS or cur_cmd.name in MANUAL_LOADER_INSTANCE_TERMINATOR_FUNCS)
                        and cur_cmd.name not in self.no_trampoline_or_terminator) or cur_cmd.name in NEEDS_TERMINATOR:
                    if cur_cmd.protect_value:
                        export_funcs += '#if %s\n' % cur_cmd.protect_string
                    if count == 0:
                        export_funcs += '\n    // A few instance commands need to go through a loader terminator.\n'
                        export_funcs += '    // Otherwise, go directly to the runtime version of the command if it exists.\n'
                        export_funcs += '    // But first set the function pointer to NULL so that the fall-through below actually works.\n'
                        export_funcs += '    *function = nullptr;\n\n'
                        export_funcs += '    if (0 == strcmp(name, "%s")) {\n' % (
                            cur_cmd.name)
                    else:
                        export_funcs += '    } else if (0 == strcmp(name, "%s")) {\n' % (
                            cur_cmd.name)
                    # If generated, the function should start with the prefix "LoaderGenTermXr"
                    if cur_cmd.name in NEEDS_TERMINATOR:
                        export_funcs += '        *function = reinterpret_cast<PFN_xrVoidFunction>(LoaderGenTermXr%s);\n' % (
                            base_name)
                    # Otherwise, the function should start with "LoaderXrTerm"
                    else:
                        export_funcs += '        *function = reinterpret_cast<PFN_xrVoidFunction>(LoaderXrTerm%s);\n' % (
                            base_name)
                    if cur_cmd.protect_value:
                        export_funcs += '#endif // %s\n' % cur_cmd.protect_string
                    count = count + 1

        export_funcs += '    } else if (0 == strcmp(name, "xrCreateApiLayerInstance")) {\n'
        export_funcs += '        // Special layer version of xrCreateInstance terminator.  If we get called this by a layer,\n'
        export_funcs += '        // we simply re-direct the information back into the standard xrCreateInstance terminator.\n'
        export_funcs += '        *function = reinterpret_cast<PFN_xrVoidFunction>(LoaderXrTermCreateApiLayerInstance);\n'
        export_funcs += '    }\n'
        export_funcs += '    if (nullptr != *function) {\n'
        export_funcs += '        return XR_SUCCESS;\n'
        export_funcs += '    }\n'
        export_funcs += '    return RuntimeInterface::GetInstanceProcAddr(instance, name, function);\n'
        export_funcs += '}\n'
        export_funcs += 'XRLOADER_ABI_CATCH_FALLBACK\n\n'

        export_funcs += '// Instance Init Dispatch Table (put all terminators in first)\n'
        export_funcs += 'void LoaderGenInitInstanceDispatchTable(XrInstance instance, std::unique_ptr<XrGeneratedDispatchTable>& table) {\n'

        count = 0
        for x in range(0, 2):
            if x == 0:
                commands = self.core_commands
            else:
                commands = self.ext_commands

            for cur_cmd in commands:
                if cur_cmd.ext_name != cur_extension_name:
                    if self.isCoreExtensionName(cur_cmd.ext_name):
                        export_funcs += '\n    // ---- Core %s commands\n' % cur_cmd.ext_name[11:]
                    else:
                        export_funcs += '\n    // ---- %s extension commands\n' % cur_cmd.ext_name
                    cur_extension_name = cur_cmd.ext_name

                # Remove 'xr' from proto name
                base_name = cur_cmd.name[2:]

                if cur_cmd.protect_value:
                    export_funcs += '#if %s\n' % cur_cmd.protect_string

                if cur_cmd.name in self.no_trampoline_or_terminator:
                    export_funcs += '    table->%s = nullptr;\n' % base_name
                else:
                    export_funcs += '    LoaderXrTermGetInstanceProcAddr(instance, "%s", reinterpret_cast<PFN_xrVoidFunction*>(&table->%s));\n' % (
                        cur_cmd.name, base_name)

                if cur_cmd.protect_value:
                    export_funcs += '#endif // %s\n' % cur_cmd.protect_string
        export_funcs += '}\n\n'
        export_funcs += '// Instance Update Dispatch Table with an API Layer Interface\n'
        export_funcs += 'void ApiLayerInterface::GenUpdateInstanceDispatchTable(XrInstance instance, std::unique_ptr<XrGeneratedDispatchTable>& table) {\n'
        export_funcs += '    PFN_xrVoidFunction cur_func_ptr;\n'
        count = 0
        for x in range(0, 2):
            if x == 0:
                commands = self.core_commands
            else:
                commands = self.ext_commands

            for cur_cmd in commands:
                if cur_cmd.ext_name != cur_extension_name:
                    if self.isCoreExtensionName(cur_cmd.ext_name):
                        export_funcs += '\n    // ---- Core %s commands\n' % cur_cmd.ext_name[11:]
                    else:
                        export_funcs += '\n    // ---- %s extension commands\n' % cur_cmd.ext_name
                    cur_extension_name = cur_cmd.ext_name

                # Remove 'xr' from proto name
                base_name = cur_cmd.name[2:]

                if cur_cmd.protect_value:
                    export_funcs += '#if %s\n' % cur_cmd.protect_string

                if cur_cmd.name not in self.no_trampoline_or_terminator:
                    if cur_cmd.name == 'xrGetInstanceProcAddr':
                        export_funcs += '    table->GetInstanceProcAddr = _get_instance_proc_addr;\n'
                    else:
                        export_funcs += '    _get_instance_proc_addr(instance, "%s", &cur_func_ptr);\n' % cur_cmd.name
                        export_funcs += '    if (nullptr != cur_func_ptr) {\n'
                        export_funcs += '        table->%s = reinterpret_cast<PFN_%s>(cur_func_ptr);\n' % (
                            base_name, cur_cmd.name)
                        export_funcs += '    }\n'

                    if cur_cmd.protect_value:
                        export_funcs += '#endif // %s\n' % cur_cmd.protect_string
        export_funcs += '}\n'
        return export_funcs
