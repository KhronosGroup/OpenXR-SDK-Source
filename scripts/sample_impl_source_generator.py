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

import os,re,sys
from automatic_source_generator import *
from collections import namedtuple

# SampleImplementationSourceGeneratorOptions - subclass of AutomaticSourceGeneratorOptions.
class SampleImplementationSourceGeneratorOptions(AutomaticSourceGeneratorOptions):
    def __init__(self,
                 filename = None,
                 directory = '.',
                 apiname = None,
                 profile = None,
                 versions = '.*',
                 emitversions = '.*',
                 defaultExtensions = None,
                 addExtensions = None,
                 removeExtensions = None,
                 emitExtensions = None,
                 sortProcedure = regSortFeatures,
                 prefixText = "",
                 genFuncPointers = True,
                 protectFile = True,
                 protectFeature = True,
                 protectProto = None,
                 protectProtoStr = None,
                 apicall = '',
                 apientry = '',
                 apientryp = '',
                 indentFuncProto = True,
                 indentFuncPointer = False,
                 alignFuncParam = 0,
                 genEnumBeginEndRange = False):
        AutomaticSourceGeneratorOptions.__init__(self, filename, directory, apiname, profile,
                                                 versions, emitversions, defaultExtensions,
                                                 addExtensions, removeExtensions,
                                                 emitExtensions, sortProcedure)
        # Instead of using prefixText, we write our own
        self.prefixText      = None
        self.genFuncPointers = genFuncPointers
        self.protectFile     = protectFile
        self.protectFeature  = protectFeature
        self.protectProto    = protectProto
        self.protectProtoStr = protectProtoStr
        self.apicall         = apicall
        self.apientry        = apientry
        self.apientryp       = apientryp
        self.indentFuncProto = indentFuncProto
        self.indentFuncPointer = indentFuncPointer
        self.alignFuncParam  = alignFuncParam
        self.genEnumBeginEndRange = genEnumBeginEndRange

# SampleImplementationSourceOutputGenerator - subclass of AutomaticSourceOutputGenerator.
class SampleImplementationSourceOutputGenerator(AutomaticSourceOutputGenerator):
    """Generate loader source using XML element attributes from registry"""
    def __init__(self,
                 errFile = sys.stderr,
                 warnFile = sys.stderr,
                 diagFile = sys.stdout):
        AutomaticSourceOutputGenerator.__init__(self, errFile, warnFile, diagFile)

    # Override the base class header warning so the comment indicates this file.
    #   self            the SampleImplementationSourceOutputGenerator object
    def outputGeneratedHeaderWarning(self):
        generated_warning  = '// *********** THIS FILE IS GENERATED - DO NOT EDIT ***********\n'
        generated_warning += '//     See sample_impl_source_generator.py for modifications\n'
        generated_warning += '// ************************************************************\n'
        write(generated_warning, file=self.outFile)

    # Call the base class to properly begin the file, and then add
    # the file-specific header information.
    #   self            the SampleImplementationSourceOutputGenerator object
    #   gen_opts        the SampleImplementationSourceGeneratorOptions object
    def beginFile(self, genOpts):
        AutomaticSourceOutputGenerator.beginFile(self, genOpts)
        preamble = ''

        if self.genOpts.filename == 'xr_generated_sample_impl.cpp':
            preamble += '#include <string>\n\n'
            preamble += '#include "xr_dependencies.h"\n'
            preamble += '#include <openxr/openxr.h>\n'
            preamble += '#include <openxr/openxr_platform.h>\n\n'
            preamble += '#include "xr_generated_dispatch_table.h"\n'

        write(preamble, file=self.outFile)

    # Write out all the information for the appropriate file,
    # and then call down to the base class to wrap everything up.
    #   self            the SampleImplementationSourceOutputGenerator object
    def endFile(self):
        file_data = ''
        file_data += self.outputSampleImplGIPA()
        write(file_data, file=self.outFile)
        # Finish processing in superclass
        AutomaticSourceOutputGenerator.endFile(self)

    # Output the reference implementation's generated xrGetInstanceProcAddr with
    # all the commands we know about.
    #   self            the SampleImplementationSourceOutputGenerator object
    def outputSampleImplGIPA(self):
        cur_extension_name = ''

        gipa  = '\n#ifdef USE_OPENXR_LOADER\n'
        gipa += 'extern "C" { // Extern to remove mangling\n\n'

        gipa += '\n// Implementation GetInstanceProcAddr function\n'
        gipa += '\nextern struct XrGeneratedDispatchTable ipa_table;\n'
        gipa += 'XRAPI_ATTR XrResult XRAPI_CALL ImplxrGetInstanceProcAddr(XrInstance instance, const char *name,\n'
        gipa += '                                                         PFN_xrVoidFunction *function) {\n'
        gipa += '    if (name[0] == \'x\' && name[1] == \'r\') {\n'
        gipa += '        std::string func_name = &name[2];\n'
        gipa += '        *function = nullptr;\n'

        count = 0
        for x in range(0, 2):
            if x == 0:
                commands = self.core_commands
            else:
                commands = self.ext_commands

            for cur_cmd in commands:
                if cur_cmd.ext_name != cur_extension_name:
                    if self.isCoreExtensionName(cur_cmd.ext_name):
                        gipa += '\n        // ---- Core %s commands\n' % cur_cmd.ext_name[11:]
                    else:
                        gipa += '\n        // ---- %s extension commands\n' % cur_cmd.ext_name
                    cur_extension_name = cur_cmd.ext_name

                # Remove 'xr' from proto name
                base_name = cur_cmd.name[2:]

                if cur_cmd.protect_value:
                    gipa += '#if %s\n' % cur_cmd.protect_string

                if count == 0:
                    gipa += '        if (func_name == "%s") {\n' % (base_name)
                else:
                    gipa += '        } else if (func_name == "%s") {\n' % (base_name)
                count = count + 1
                gipa += '            *function = reinterpret_cast<PFN_xrVoidFunction>(ipa_table.%s);\n' % (base_name)

                if cur_cmd.protect_value:
                    gipa += '#endif // %s\n' % cur_cmd.protect_string

        gipa += '        }\n'
        gipa += '    }\n'
        gipa += '    return *function ? XR_SUCCESS : XR_ERROR_FUNCTION_UNSUPPORTED;\n'
        gipa += '}\n'
        gipa += '} // extern "C"\n'
        gipa += '#endif\n'

        return gipa
