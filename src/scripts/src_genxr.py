#!/usr/bin/python3
#
# Copyright 2013-2025 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0
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

import argparse
import os
import re
import sys
import time
import xml.etree.ElementTree as etree

base_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.append(os.path.join(base_dir, 'src', 'scripts'))
sys.path.append(os.path.join(base_dir, 'specification', 'scripts'))

from api_dump_generator import ApiDumpOutputGenerator
from automatic_source_generator import AutomaticSourceGeneratorOptions
from generator import write
from loader_source_generator import LoaderSourceOutputGenerator
from reflib import logDiag, logWarn, logErr, setLogFile
from reg import Registry
from utility_source_generator import UtilitySourceOutputGenerator
from validation_layer_generator import ValidationSourceOutputGenerator
from apiconventions import APIConventions
try:
    from conformance_generator import ConformanceGenerator
    from conformance_layer_generator import ConformanceLayerGenerator
    HAVE_CONFORMANCE = True
except ImportError:
    HAVE_CONFORMANCE = False

# Simple timer functions
startTime = None


def startTimer(timeit):
    global startTime
    if timeit:
        startTime = time.process_time()


def endTimer(timeit, msg):
    global startTime
    if timeit and startTime is not None:
        endTime = time.process_time()
        logDiag(msg, endTime - startTime)
        startTime = None


def makeREstring(strings, default=None, strings_are_regex=False):
    """Turn a list of strings into a regexp string matching exactly those strings."""
    if strings or default is None:
        if not strings_are_regex:
            strings = (re.escape(s) for s in strings)
        return f"^({'|'.join(strings)})$"
    return default


def makeGenOpts(args):
    """Returns a directory of [ generator function, generator options ] indexed
    by specified short names. The generator options incorporate the following
    parameters:

    args is an parsed argument object; see below for the fields that are used."""
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
    allFeatures = allExtensions = r'.*'

    # Turn lists of names/patterns into matching regular expressions
    emitExtensionsPat = makeREstring(emitExtensions, allExtensions)
    featuresPat = makeREstring(features, allFeatures)

    # REUSE-IgnoreStart
    # Copyright text prefixing all headers (list of strings).
    prefixStrings = [
        '/*',
        '** Copyright (c) 2017-2025 The Khronos Group Inc.',
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
    # REUSE-IgnoreEnd

    # Text specific to OpenXR headers
    xrPrefixStrings = [
        '/*',
        '** This header is generated from the Khronos OpenXR XML API Registry.',
        '**',
        '*/',
        ''
    ]

    # An API style conventions object
    conventions = APIConventions()

    if HAVE_CONFORMANCE:
        opts = {
            'conventions': conventions,
            'directory': directory,
            'apiname': 'openxr',
            'profile': None,
            'versions': featuresPat,
            'emitversions': featuresPat,
            'defaultExtensions': 'openxr',
            'addExtensions': None,
            'removeExtensions': None,
            'emitExtensions': emitExtensionsPat,
            'prefixText': prefixStrings + xrPrefixStrings,
            'protectFeature': False,
            'protectProto': '#ifndef',
            'protectProtoStr': 'XR_NO_PROTOTYPES',
            'apicall': 'XRAPI_ATTR ',
            'apientry': 'XRAPI_CALL ',
            'apientryp': 'XRAPI_PTR *',
            'alignFuncParam': 48,
        }
        genOpts['function_info.cpp'] = [
            ConformanceGenerator,
            AutomaticSourceGeneratorOptions(
                filename='function_info.cpp',
                **opts
            )
        ]
        genOpts['interaction_info_generated.cpp'] = [
            ConformanceGenerator,
            AutomaticSourceGeneratorOptions(
                filename='interaction_info_generated.cpp',
                **opts
            )
        ]
        genOpts['interaction_info_generated.h'] = [
            ConformanceGenerator,
            AutomaticSourceGeneratorOptions(
                filename='interaction_info_generated.h',
                **opts
            )
        ]

        genOpts['gen_dispatch.cpp'] = [
            ConformanceLayerGenerator,
            AutomaticSourceGeneratorOptions(
                conventions=conventions,
                filename='gen_dispatch.cpp',
                directory=directory,
                apiname='openxr',
                profile=None,
                versions=featuresPat,
                emitversions=featuresPat,
                defaultExtensions='openxr',
                addExtensions=None,
                removeExtensions=None,
                emitExtensions=emitExtensionsPat,
                apicall='XRAPI_ATTR ',
                apientry='XRAPI_CALL ',
                apientryp='XRAPI_PTR *',)
        ]

        genOpts['gen_dispatch.h'] = [
            ConformanceLayerGenerator,
            AutomaticSourceGeneratorOptions(
                conventions=conventions,
                filename='gen_dispatch.h',
                directory=directory,
                apiname='openxr',
                profile=None,
                versions=featuresPat,
                emitversions=featuresPat,
                defaultExtensions='openxr',
                addExtensions=None,
                removeExtensions=None,
                emitExtensions=emitExtensionsPat,
                apicall='XRAPI_ATTR ',
                apientry='XRAPI_CALL ',
                apientryp='XRAPI_PTR *',)
        ]

    DISPATCH_TABLE_FILES = [
        'xr_generated_dispatch_table.h',
        'xr_generated_dispatch_table.c',
        'xr_generated_dispatch_table_core.h',
        'xr_generated_dispatch_table_core.c',
    ]

    for filename in DISPATCH_TABLE_FILES:
        genOpts[filename] = [
            UtilitySourceOutputGenerator,
            AutomaticSourceGeneratorOptions(
                conventions=conventions,
                filename=filename,
                directory=directory,
                apiname='openxr',
                profile=None,
                versions=featuresPat,
                emitversions=featuresPat,
                defaultExtensions='openxr',
                addExtensions=None,
                removeExtensions=None,
                emitExtensions=emitExtensionsPat)
        ]

    genOpts['xr_generated_loader.hpp'] = [
        LoaderSourceOutputGenerator,
        AutomaticSourceGeneratorOptions(
            conventions=conventions,
            filename='xr_generated_loader.hpp',
            directory=directory,
            apiname='openxr',
            profile=None,
            versions=featuresPat,
            emitversions=featuresPat,
            defaultExtensions='openxr',
            addExtensions=None,
            removeExtensions=None,
            emitExtensions=emitExtensionsPat,
            prefixText=prefixStrings + xrPrefixStrings,
            protectFeature=False,
            protectProto='#ifndef',
            protectProtoStr='XR_NO_PROTOTYPES',
            apicall='XRAPI_ATTR ',
            apientry='XRAPI_CALL ',
            apientryp='XRAPI_PTR *',
            alignFuncParam=48)
    ]

    genOpts['xr_generated_loader.cpp'] = [
        LoaderSourceOutputGenerator,
        AutomaticSourceGeneratorOptions(
            conventions=conventions,
            filename='xr_generated_loader.cpp',
            directory=directory,
            apiname='openxr',
            profile=None,
            versions=featuresPat,
            emitversions=featuresPat,
            defaultExtensions='openxr',
            addExtensions=None,
            removeExtensions=None,
            emitExtensions=emitExtensionsPat,
            prefixText=prefixStrings + xrPrefixStrings,
            protectFeature=False,
            protectProto='#ifndef',
            protectProtoStr='XR_NO_PROTOTYPES',
            apicall='XRAPI_ATTR ',
            apientry='XRAPI_CALL ',
            apientryp='XRAPI_PTR *',
            alignFuncParam=48)
    ]

    # Source files generated for the api_dump layer
    genOpts['xr_generated_api_dump.cpp'] = [
        ApiDumpOutputGenerator,
        AutomaticSourceGeneratorOptions(
            conventions=conventions,
            filename='xr_generated_api_dump.cpp',
            directory=directory,
            apiname='openxr',
            profile=None,
            versions=featuresPat,
            emitversions=featuresPat,
            defaultExtensions='openxr',
            addExtensions=None,
            removeExtensions=None,
            emitExtensions=emitExtensionsPat,
            apicall='XRAPI_ATTR ',
            apientry='XRAPI_CALL ',
            apientryp='XRAPI_PTR *')
    ]

    genOpts['xr_generated_api_dump.hpp'] = [
        ApiDumpOutputGenerator,
        AutomaticSourceGeneratorOptions(
            conventions=conventions,
            filename='xr_generated_api_dump.hpp',
            directory=directory,
            apiname='openxr',
            profile=None,
            versions=featuresPat,
            emitversions=featuresPat,
            defaultExtensions='openxr',
            addExtensions=None,
            removeExtensions=None,
            emitExtensions=emitExtensionsPat,
            apicall='XRAPI_ATTR ',
            apientry='XRAPI_CALL ',
            apientryp='XRAPI_PTR *')
    ]

    # Source files generated for the core validation layer
    genOpts['xr_generated_core_validation.hpp'] = [
        ValidationSourceOutputGenerator,
        AutomaticSourceGeneratorOptions(
            conventions=conventions,
            filename='xr_generated_core_validation.hpp',
            directory=directory,
            apiname='openxr',
            profile=None,
            versions=featuresPat,
            emitversions=featuresPat,
            defaultExtensions='openxr',
            addExtensions=None,
            removeExtensions=None,
            emitExtensions=emitExtensionsPat,
            apicall='XRAPI_ATTR ',
            apientry='XRAPI_CALL ',
            apientryp='XRAPI_PTR *')
    ]

    genOpts['xr_generated_core_validation.cpp'] = [
        ValidationSourceOutputGenerator,
        AutomaticSourceGeneratorOptions(
            conventions=conventions,
            filename='xr_generated_core_validation.cpp',
            directory=directory,
            apiname='openxr',
            profile=None,
            versions=featuresPat,
            emitversions=featuresPat,
            defaultExtensions='openxr',
            addExtensions=None,
            removeExtensions=None,
            emitExtensions=emitExtensionsPat,
            apicall='XRAPI_ATTR ',
            apientry='XRAPI_CALL ',
            apientryp='XRAPI_PTR *')
    ]


def genTarget(args):
    """Create an API generator and corresponding generator options based on
    the requested target and command line options.

    This is encapsulated in a function so it can be profiled and/or timed.
    The args parameter is an parsed argument object containing the following
    fields that are used:

    - target - target to generate
    - directory - directory to generate it in
    - protect - True if re-inclusion wrappers should be created
    - extensions - list of additional extensions to include in generated interfaces"""

    # Create generator options with specified parameters
    makeGenOpts(args)

    # Select a generator matching the requested target
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
        return (gen, options)
    else:
        write('No generator options for unknown target:',
              args.target, file=sys.stderr)
        sys.exit(1)


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
                        default='xr.xml',
                        help='Use specified registry file instead of xr.xml')
    parser.add_argument('-time', action='store_true',
                        help='Enable timing')
    parser.add_argument('-genpath', action='store', default='gen',
                        help='Path to generated files')
    parser.add_argument('-o', action='store', dest='directory',
                        default='.',
                        help='Create target and related files in specified directory')
    parser.add_argument('target', metavar='target', nargs='?',
                        help='Specify target')
    parser.add_argument('-quiet', action='store_true', default=False,
                        help='Suppress script output during normal execution.')
    parser.add_argument('-verbose', action='store_false', dest='quiet', default=True,
                        help='Enable script output during normal execution.')

    args = parser.parse_args()

    # This splits arguments which are space-separated lists
    args.feature = [name for arg in args.feature for name in arg.split()]
    args.extension = [name for arg in args.extension for name in arg.split()]

    # create error/warning & diagnostic files
    if args.errfile:
        errWarn = open(args.errfile, 'w', encoding='utf-8')
    else:
        errWarn = sys.stderr

    if args.diagfile:
        diag = open(args.diagfile, 'w', encoding='utf-8')
    else:
        diag = None

    if args.time:
        # Log diagnostics and warnings
        setLogFile(setDiag=True, setWarn=True, filename='-')

    # Create the API generator & generator options
    (gen, options) = genTarget(args)

    # Create the registry object with the specified generator and generator
    # options. The options are set before XML loading as they may affect it.
    reg = Registry(gen, options)

    # Parse the specified registry XML into an ElementTree object
    startTimer(args.time)
    tree = etree.parse(args.registry)
    endTimer(args.time, '* Time to make ElementTree =')

    # Load the XML tree into the registry object
    startTimer(args.time)
    reg.loadElementTree(tree)
    endTimer(args.time, '* Time to parse ElementTree =')

    # Finally, use the output generator to create the requested target
    if args.debug:
        pdb.run('reg.apiGen()')
    else:
        startTimer(args.time)
        reg.apiGen()
        endTimer(args.time, f"* Time to generate {options.filename} =")

    if not args.quiet:
        logDiag('* Generated', options.filename)
