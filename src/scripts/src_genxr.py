#!/usr/bin/python3
#
# Copyright (c) 2013-2017 The Khronos Group Inc.
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

import argparse, cProfile, os, pdb, string, sys, time

base_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.append(os.path.join(base_dir, 'src', 'scripts'))
sys.path.append(os.path.join(base_dir, 'specification', 'scripts'))

from reg import *
from generator import write
from cgenerator import CGeneratorOptions, COutputGenerator
from xrconventions import OpenXRConventions
from automatic_source_generator import AutomaticSourceGeneratorOptions, AutomaticSourceOutputGenerator
from loader_source_generator import LoaderSourceGeneratorOptions, LoaderSourceOutputGenerator
from utility_source_generator import UtilitySourceGeneratorOptions, UtilitySourceOutputGenerator
from api_dump_generator import ApiDumpGeneratorOptions, ApiDumpOutputGenerator
from validation_layer_generator import ValidationSourceGeneratorOptions, ValidationSourceOutputGenerator

# Simple timer functions
startTime = None

def startTimer(timeit):
    global startTime
    startTime = time.process_time()

def endTimer(timeit, msg):
    global startTime
    endTime = time.process_time()
    if timeit:
        write(msg, endTime - startTime, file=sys.stderr)
        startTime = None

# Turn a list of strings into a regexp string matching exactly those strings
def makeREstring(list, default = None):
    if len(list) > 0 or default == None:
        return '^(' + '|'.join(list) + ')$'
    else:
        return default

# Returns a directory of [ generator function, generator options ] indexed
# by specified short names. The generator options incorporate the following
# parameters:
#
# args is an parsed argument object; see below for the fields that are used.
def makeGenOpts(args):
    global genOpts
    genOpts = {}

    # Extensions to emit (list of extensions)
    emitExtensions = args.emitExtensions

    # Features to include (list of features)
    features = args.feature

    # Output target directory
    directory = args.directory

    # Descriptive names for various regexp patterns used to select
    # versions and extensions
    allFeatures     = allExtensions = '.*'

    # Turn lists of names/patterns into matching regular expressions
    emitExtensionsPat    = makeREstring(emitExtensions, allExtensions)
    featuresPat          = makeREstring(features, allFeatures)

    # Copyright text prefixing all headers (list of strings).
    prefixStrings = [
        '/*',
        '** Copyright (c) 2017-2019 The Khronos Group Inc.',
        '**',
        '** Licensed under the Apache License, Version 2.0 (the "License");',
        '** you may not use this file except in compliance with the License.',
        '** You may obtain a copy of the License at',
        '**',
        '**     http://www.apache.org/licenses/LICENSE-2.0',
        '**',
        '** Unless required by applicable law or agreed to in writing, software',
        '** distributed under the License is distributed on an "AS IS" BASIS,',
        '** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.',
        '** See the License for the specific language governing permissions and',
        '** limitations under the License.',
        '*/',
        ''
    ]

    # Text specific to OpenXR headers
    xrPrefixStrings = [
        '/*',
        '** This header is generated from the Khronos OpenXR XML API Registry.',
        '**',
        '*/',
        ''
    ]

    # An API style conventions object
    conventions = OpenXRConventions()

    genOpts['xr_generated_dispatch_table.h'] = [
          UtilitySourceOutputGenerator,
          UtilitySourceGeneratorOptions(
            conventions       = conventions,
            filename          = 'xr_generated_dispatch_table.h',
            directory         = directory,
            apiname           = 'openxr',
            profile           = None,
            versions          = featuresPat,
            emitversions      = featuresPat,
            defaultExtensions = 'openxr',
            addExtensions     = None,
            removeExtensions  = None,
            emitExtensions    = emitExtensionsPat,
            prefixText        = prefixStrings + xrPrefixStrings,
            protectFeature    = False,
            protectProto      = '#ifndef',
            protectProtoStr   = 'XR_NO_PROTOTYPES',
            apicall           = 'XRAPI_ATTR ',
            apientry          = 'XRAPI_CALL ',
            apientryp         = 'XRAPI_PTR *',
            alignFuncParam    = 48)
        ]

    genOpts['xr_generated_dispatch_table.c'] = [
          UtilitySourceOutputGenerator,
          UtilitySourceGeneratorOptions(
            conventions       = conventions,
            filename          = 'xr_generated_dispatch_table.c',
            directory         = directory,
            apiname           = 'openxr',
            profile           = None,
            versions          = featuresPat,
            emitversions      = featuresPat,
            defaultExtensions = 'openxr',
            addExtensions     = None,
            removeExtensions  = None,
            emitExtensions    = emitExtensionsPat,
            prefixText        = prefixStrings + xrPrefixStrings,
            protectFeature    = False,
            protectProto      = '#ifndef',
            protectProtoStr   = 'XR_NO_PROTOTYPES',
            apicall           = 'XRAPI_ATTR ',
            apientry          = 'XRAPI_CALL ',
            apientryp         = 'XRAPI_PTR *',
            alignFuncParam    = 48)
        ]

    genOpts['xr_generated_utilities.h'] = [
          UtilitySourceOutputGenerator,
          UtilitySourceGeneratorOptions(
            conventions       = conventions,
            filename          = 'xr_generated_utilities.h',
            directory         = directory,
            apiname           = 'openxr',
            profile           = None,
            versions          = featuresPat,
            emitversions      = featuresPat,
            defaultExtensions = 'openxr',
            addExtensions     = None,
            removeExtensions  = None,
            emitExtensions    = emitExtensionsPat,
            prefixText        = prefixStrings + xrPrefixStrings,
            protectFeature    = False,
            protectProto      = '#ifndef',
            protectProtoStr   = 'XR_NO_PROTOTYPES',
            apicall           = 'XRAPI_ATTR ',
            apientry          = 'XRAPI_CALL ',
            apientryp         = 'XRAPI_PTR *',
            alignFuncParam    = 48)
        ]

    genOpts['xr_generated_utilities.c'] = [
          UtilitySourceOutputGenerator,
          UtilitySourceGeneratorOptions(
            conventions       = conventions,
            filename          = 'xr_generated_utilities.c',
            directory         = directory,
            apiname           = 'openxr',
            profile           = None,
            versions          = featuresPat,
            emitversions      = featuresPat,
            defaultExtensions = 'openxr',
            addExtensions     = None,
            removeExtensions  = None,
            emitExtensions    = emitExtensionsPat,
            prefixText        = prefixStrings + xrPrefixStrings,
            protectFeature    = False,
            protectProto      = '#ifndef',
            protectProtoStr   = 'XR_NO_PROTOTYPES',
            apicall           = 'XRAPI_ATTR ',
            apientry          = 'XRAPI_CALL ',
            apientryp         = 'XRAPI_PTR *',
            alignFuncParam    = 48)
        ]

    genOpts['xr_generated_loader.hpp'] = [
          LoaderSourceOutputGenerator,
          LoaderSourceGeneratorOptions(
            conventions       = conventions,
            filename          = 'xr_generated_loader.hpp',
            directory         = directory,
            apiname           = 'openxr',
            profile           = None,
            versions          = featuresPat,
            emitversions      = featuresPat,
            defaultExtensions = 'openxr',
            addExtensions     = None,
            removeExtensions  = None,
            emitExtensions    = emitExtensionsPat,
            prefixText        = prefixStrings + xrPrefixStrings,
            protectFeature    = False,
            protectProto      = '#ifndef',
            protectProtoStr   = 'XR_NO_PROTOTYPES',
            apicall           = 'XRAPI_ATTR ',
            apientry          = 'XRAPI_CALL ',
            apientryp         = 'XRAPI_PTR *',
            alignFuncParam    = 48)
        ]

    genOpts['xr_generated_loader.cpp'] = [
          LoaderSourceOutputGenerator,
          LoaderSourceGeneratorOptions(
            conventions       = conventions,
            filename          = 'xr_generated_loader.cpp',
            directory         = directory,
            apiname           = 'openxr',
            profile           = None,
            versions          = featuresPat,
            emitversions      = featuresPat,
            defaultExtensions = 'openxr',
            addExtensions     = None,
            removeExtensions  = None,
            emitExtensions    = emitExtensionsPat,
            prefixText        = prefixStrings + xrPrefixStrings,
            protectFeature    = False,
            protectProto      = '#ifndef',
            protectProtoStr   = 'XR_NO_PROTOTYPES',
            apicall           = 'XRAPI_ATTR ',
            apientry          = 'XRAPI_CALL ',
            apientryp         = 'XRAPI_PTR *',
            alignFuncParam    = 48)
        ]

    # Source files generated for the api_dump layer
    genOpts['xr_generated_api_dump.cpp'] = [
          ApiDumpOutputGenerator,
          ApiDumpGeneratorOptions(
            conventions       = conventions,
            filename          = 'xr_generated_api_dump.cpp',
            directory         = directory,
            apiname           = 'openxr',
            profile           = None,
            versions          = featuresPat,
            emitversions      = featuresPat,
            defaultExtensions = 'openxr',
            addExtensions     = None,
            removeExtensions  = None,
            emitExtensions    = emitExtensionsPat,
            prefixText        = prefixStrings + xrPrefixStrings,
            protectFeature    = False,
            protectProto      = '#ifndef',
            protectProtoStr   = 'XR_NO_PROTOTYPES',
            apicall           = 'XRAPI_ATTR ',
            apientry          = 'XRAPI_CALL ',
            apientryp         = 'XRAPI_PTR *',
            alignFuncParam    = 48)
        ]

    genOpts['xr_generated_api_dump.hpp'] = [
          ApiDumpOutputGenerator,
          ApiDumpGeneratorOptions(
            conventions       = conventions,
            filename          = 'xr_generated_api_dump.hpp',
            directory         = directory,
            apiname           = 'openxr',
            profile           = None,
            versions          = featuresPat,
            emitversions      = featuresPat,
            defaultExtensions = 'openxr',
            addExtensions     = None,
            removeExtensions  = None,
            emitExtensions    = emitExtensionsPat,
            prefixText        = prefixStrings + xrPrefixStrings,
            protectFeature    = False,
            protectProto      = '#ifndef',
            protectProtoStr   = 'XR_NO_PROTOTYPES',
            apicall           = 'XRAPI_ATTR ',
            apientry          = 'XRAPI_CALL ',
            apientryp         = 'XRAPI_PTR *',
            alignFuncParam    = 48)
        ]

    # Source files generated for the core validation layer
    genOpts['xr_generated_core_validation.hpp'] = [
          ValidationSourceOutputGenerator,
          ValidationSourceGeneratorOptions(
            conventions       = conventions,
            filename          = 'xr_generated_core_validation.hpp',
            directory         = directory,
            apiname           = 'openxr',
            profile           = None,
            versions          = featuresPat,
            emitversions      = featuresPat,
            defaultExtensions = 'openxr',
            addExtensions     = None,
            removeExtensions  = None,
            emitExtensions    = emitExtensionsPat,
            prefixText        = prefixStrings + xrPrefixStrings,
            protectFeature    = False,
            protectProto      = '#ifndef',
            protectProtoStr   = 'XR_NO_PROTOTYPES',
            apicall           = 'XRAPI_ATTR ',
            apientry          = 'XRAPI_CALL ',
            apientryp         = 'XRAPI_PTR *',
            alignFuncParam    = 48)
        ]

    genOpts['xr_generated_core_validation.cpp'] = [
          ValidationSourceOutputGenerator,
          ValidationSourceGeneratorOptions(
            conventions       = conventions,
            filename          = 'xr_generated_core_validation.cpp',
            directory         = directory,
            apiname           = 'openxr',
            profile           = None,
            versions          = featuresPat,
            emitversions      = featuresPat,
            defaultExtensions = 'openxr',
            addExtensions     = None,
            removeExtensions  = None,
            emitExtensions    = emitExtensionsPat,
            prefixText        = prefixStrings + xrPrefixStrings,
            protectFeature    = False,
            protectProto      = '#ifndef',
            protectProtoStr   = 'XR_NO_PROTOTYPES',
            apicall           = 'XRAPI_ATTR ',
            apientry          = 'XRAPI_CALL ',
            apientryp         = 'XRAPI_PTR *',
            alignFuncParam    = 48)
        ]

# Generate a target based on the options in the matching genOpts{} object.
# This is encapsulated in a function so it can be profiled and/or timed.
# The args parameter is an parsed argument object containing the following
# fields that are used:
#   target - target to generate
#   directory - directory to generate it in
#   protect - True if re-inclusion wrappers should be created
#   extensions - list of additional extensions to include in generated
#   interfaces
def genTarget(args):
    # Create generator options with specified parameters
    makeGenOpts(args)

    if args.target in genOpts.keys():
        createGenerator = genOpts[args.target][0]
        options = genOpts[args.target][1]

        if not args.quiet:
            write('* Building', options.filename, file=sys.stderr)
            write('* options.versions          =', options.versions, file=sys.stderr)
            write('* options.emitversions      =', options.emitversions, file=sys.stderr)
            write('* options.defaultExtensions =', options.defaultExtensions, file=sys.stderr)
            write('* options.addExtensions     =', options.addExtensions, file=sys.stderr)
            write('* options.removeExtensions  =', options.removeExtensions, file=sys.stderr)
            write('* options.emitExtensions    =', options.emitExtensions, file=sys.stderr)

        startTimer(args.time)
        gen = createGenerator(errFile=errWarn,
                              warnFile=errWarn,
                              diagFile=diag)
        reg.setGenerator(gen)
        reg.apiGen(options)

        if not args.quiet:
            write('* Generated', options.filename, file=sys.stderr)
        endTimer(args.time, '* Time to generate ' + options.filename + ' =')
    else:
        write('No generator options for unknown target:',
              args.target, file=sys.stderr)

# -feature name
# -extension name
# For both, "name" may be a single name, or a space-separated list
# of names, or a regular expression.
if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    parser.add_argument('-defaultExtensions', action='store',
                        default='openxr',
                        help='Specify a single class of extensions to add to targets')
    parser.add_argument('-extension', action='append',
                        default=[],
                        help='Specify an extension or extensions to add to targets')
    parser.add_argument('-removeExtensions', action='append',
                        default=[],
                        help='Specify an extension or extensions to remove from targets')
    parser.add_argument('-emitExtensions', action='append',
                        default=[],
                        help='Specify an extension or extensions to emit in targets')
    parser.add_argument('-feature', action='append',
                        default=[],
                        help='Specify a core API feature name or names to add to targets')
    parser.add_argument('-debug', action='store_true',
                        help='Enable debugging')
    parser.add_argument('-dump', action='store_true',
                        help='Enable dump to stderr')
    parser.add_argument('-diagfile', action='store',
                        default=None,
                        help='Write diagnostics to specified file')
    parser.add_argument('-errfile', action='store',
                        default=None,
                        help='Write errors and warnings to specified file instead of stderr')
    parser.add_argument('-noprotect', dest='protect', action='store_false',
                        help='Disable inclusion protection in output headers')
    parser.add_argument('-profile', action='store_true',
                        help='Enable profiling')
    parser.add_argument('-registry', action='store',
                        default='vk.xml',
                        help='Use specified registry file instead of vk.xml')
    parser.add_argument('-time', action='store_true',
                        help='Enable timing')
    parser.add_argument('-validate', action='store_true',
                        help='Enable group validation')
    parser.add_argument('-o', action='store', dest='directory',
                        default='.',
                        help='Create target and related files in specified directory')
    parser.add_argument('target', metavar='target', nargs='?',
                        help='Specify target')
    parser.add_argument('-quiet', action='store_true', default=False,
                        help='Suppress script output during normal execution.')

    args = parser.parse_args()

    # This splits arguments which are space-separated lists
    args.feature = [name for arg in args.feature for name in arg.split()]
    args.extension = [name for arg in args.extension for name in arg.split()]

    # Load & parse registry
    reg = Registry()

    startTimer(args.time)
    tree = etree.parse(args.registry)
    endTimer(args.time, '* Time to make ElementTree =')

    startTimer(args.time)
    reg.loadElementTree(tree)
    endTimer(args.time, '* Time to parse ElementTree =')

    if args.validate:
        reg.validateGroups()

    if args.dump:
        write('* Dumping registry to regdump.txt', file=sys.stderr)
        reg.dumpReg(filehandle = open('regdump.txt', 'w', encoding='utf-8'))

    # create error/warning & diagnostic files
    if args.errfile:
        errWarn = open(args.errfile, 'w', encoding='utf-8')
    else:
        errWarn = sys.stderr

    if args.diagfile:
        diag = open(args.diagfile, 'w', encoding='utf-8')
    else:
        diag = None

    if args.debug:
        pdb.run('genTarget(args)')
    elif args.profile:
        import cProfile, pstats
        cProfile.run('genTarget(args)', 'profile.txt')
        p = pstats.Stats('profile.txt')
        p.strip_dirs().sort_stats('time').print_stats(50)
    else:
        genTarget(args)
