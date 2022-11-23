#!/usr/bin/python3
#
# Copyright 2013-2022 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import pdb
import re
import sys
import time
import xml.etree.ElementTree as etree

sys.path.append(os.path.abspath(os.path.dirname(__file__)))

from cgenerator import CGeneratorOptions, COutputGenerator
from creflectiongenerator import CReflectionOutputGenerator
from docgenerator import DocGeneratorOptions, DocOutputGenerator
from extensionmetadocgenerator import (ExtensionMetaDocGeneratorOptions,
                                       ExtensionMetaDocOutputGenerator)
from generator import write
from hostsyncgenerator import HostSynchronizationOutputGenerator
from indexgenerator import DocIndexOutputGenerator
from pygenerator import PyOutputGenerator
from rubygenerator import RubyOutputGenerator
from reflib import logDiag, logWarn, logErr, setLogFile
from reg import Registry
from validitygenerator import ValidityOutputGenerator
from apiconventions import APIConventions

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
        return '^(' + '|'.join(strings) + ')$'
    return default


def makeGenOpts(args):
    """Returns a directory of [ generator function, generator options ] indexed
    by specified short names. The generator options incorporate the following
    parameters:

    args is an parsed argument object; see below for the fields that are used."""
    global genOpts
    genOpts = {}

    # Default class of extensions to include, or None
    defaultExtensions = args.defaultExtensions

    # Additional extensions to include (list of extensions)
    extensions = args.extension

    # Extensions to remove (list of extensions)
    removeExtensions = args.removeExtensions

    # Extensions to emit (list of extensions)
    emitExtensions = args.emitExtensions

    # Extension to emit for standalone_header (single extension)
    standaloneExtension = args.standaloneExtension

    # Header prefix override for standalone headers (list of strings)
    standalonePrefixOverride = args.standalonePrefixOverride

    # Features to include (list of features)
    features = args.feature

    # Whether to disable inclusion protect in headers
    protect = args.protect

    # Output target directory
    directory = args.directory

    # Path to generated files, particularly apimap.py
    genpath = args.genpath

    # Descriptive names for various regexp patterns used to select
    # versions and extensions
    allFeatures = allExtensions = r'.*'

    # Turn lists of names/patterns into matching regular expressions
    addExtensionsPat     = makeREstring(extensions, None)
    removeExtensionsPat  = makeREstring(removeExtensions, None)
    emitExtensionsPat    = makeREstring(emitExtensions, allExtensions)
    featuresPat          = makeREstring(features, allFeatures)

    # Copyright text prefixing all headers (list of strings).
    prefixStrings = [
        '/*',
        '** Copyright 2017-2022 The Khronos Group Inc.',
        '**',
        # The following split string is to avoid confusing the "REUSE" tool
        '** SPDX-License-Identifier' + ': Apache-2.0 OR MIT',
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

    # Include the non-platform openxr header in the platform header.
    platformPrefixStrings = [
        '#include "openxr.h"'
    ]

    # Defaults for generating re-inclusion protection wrappers (or not)
    protectFile = protect

    # An API style conventions object
    conventions = APIConventions()

    # OpenXR 1.0 - header for core API + extensions.
    # To generate just the core API,
    # change to 'defaultExtensions = None' below.
    genOpts['openxr.h'] = [
          COutputGenerator,
          CGeneratorOptions(
            conventions       = conventions,
            filename          = 'openxr.h',
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
            genFuncPointers   = True,
            protectFile       = protectFile,
            protectFeature    = False,
            protectProto      = '#ifndef',
            protectProtoStr   = 'XR_NO_PROTOTYPES',
            protectExtensionProto      = '#ifdef',
            protectExtensionProtoStr   = 'XR_EXTENSION_PROTOTYPES',
            apicall           = 'XRAPI_ATTR ',
            apientry          = 'XRAPI_CALL ',
            apientryp         = 'XRAPI_PTR *',
            alignFuncParam    = 48,
            genAliasMacro     = True,
            genStructExtendsComment = True,
            aliasMacro        = 'XR_MAY_ALIAS')
        ]

    standalonePrefixString = prefixStrings + xrPrefixStrings
    if len(standalonePrefixOverride) > 0:
        standalonePrefixString = standalonePrefixOverride

    # "XR_EXT_some_extension" becomes "ext_some_extension.h"
    standaloneExtensionFileName = standaloneExtension[len('XR_'):].lower() + '.h'

    genOpts['standalone_header'] = [
          COutputGenerator,
          CGeneratorOptions(
            conventions       = conventions,
            filename          = standaloneExtensionFileName,
            directory         = directory,
            apiname           = 'openxr',
            profile           = None,
            versions          = None,
            emitversions      = None,
            defaultExtensions = 'openxr',
            addExtensions     = None,
            removeExtensions  = None,
            emitExtensions    = standaloneExtension,
            prefixText        = standalonePrefixString,
            genFuncPointers   = True,
            protectFile       = protectFile,
            protectFeature    = True,
            protectProto      = '#ifndef',
            protectProtoStr   = 'XR_NO_PROTOTYPES',
            protectExtensionProto      = '#ifdef',
            protectExtensionProtoStr   = 'XR_EXTENSION_PROTOTYPES',
            apicall           = 'XRAPI_ATTR ',
            apientry          = 'XRAPI_CALL ',
            apientryp         = 'XRAPI_PTR *',
            alignFuncParam    = 48,
            genAliasMacro     = True,
            genStructExtendsComment = True,
            aliasMacro        = 'XR_MAY_ALIAS',
            misracppstyle     = False,
            reparentEnums     = False,
            # Crucial part of making standalone headers is adding struct types
            # that would otherwise be added to openxr.h
            redefineEnumExtends = True,
            # Ensure we only include the directly
            # required types, and not recursively
            # required ones. Otherwise it would
            # create collisions with types defined
            # in main openxr.h
            emitRecursiveRequirements = False,
            )
        ]


    # OpenXR platform header for Graphics API and Platform extensions.
    genOpts['openxr_platform.h'] = [
          COutputGenerator,
          CGeneratorOptions(
            conventions       = conventions,
            filename          = 'openxr_platform.h',
            directory         = directory,
            apiname           = 'openxr',
            profile           = None,
            versions          = featuresPat,
            emitversions      = featuresPat,
            defaultExtensions = 'openxr',
            addExtensions     = None,
            removeExtensions  = None,
            emitExtensions    = emitExtensionsPat,
            prefixText        = prefixStrings + xrPrefixStrings + platformPrefixStrings,
            genFuncPointers   = True,
            protectFile       = protectFile,
            protectFeature    = False,
            protectProto      = '#ifndef',
            protectProtoStr   = 'XR_NO_PROTOTYPES',
            protectExtensionProto      = '#ifdef',
            protectExtensionProtoStr   = 'XR_EXTENSION_PROTOTYPES',
            apicall           = 'XRAPI_ATTR ',
            apientry          = 'XRAPI_CALL ',
            apientryp         = 'XRAPI_PTR *',
            alignFuncParam    = 48,
            genAliasMacro     = True,
            genStructExtendsComment = True,
            aliasMacro        = 'XR_MAY_ALIAS')
    ]

    def make_reflection_options(fn):
        return CGeneratorOptions(
            conventions=conventions,
            filename=fn,
            directory=directory,
            apiname='openxr',
            profile=None,
            versions=featuresPat,
            emitversions=featuresPat,
            defaultExtensions='openxr',
            addExtensions=None,
            removeExtensions=None,
            emitExtensions=emitExtensionsPat,
            prefixText=prefixStrings + xrPrefixStrings + platformPrefixStrings,
            genFuncPointers=True,
            protectFile=protectFile,
            protectFeature=False,
            protectProto='#ifndef',
            protectProtoStr='XR_NO_PROTOTYPES',
            apicall='XRAPI_ATTR ',
            apientry='XRAPI_CALL ',
            apientryp='XRAPI_PTR *',
            alignFuncParam=48,
            genAliasMacro=True,
            genStructExtendsComment=True,
            aliasMacro='XR_MAY_ALIAS')

    # OpenXR generic reflection header
    genOpts['openxr_reflection.h'] = [
        CReflectionOutputGenerator,
        make_reflection_options('openxr_reflection.h'),
    ]

    genOpts['openxr_reflection_structs.h'] = [
        CReflectionOutputGenerator,
        make_reflection_options('openxr_reflection_structs.h'),
    ]

    genOpts['openxr_reflection_parent_structs.h'] = [
        CReflectionOutputGenerator,
        make_reflection_options('openxr_reflection_parent_structs.h'),
    ]

    # OpenXR 1.0 - API include files for spec and ref pages
    # Overwrites include subdirectories in spec source tree
    # The generated include files do not include the calling convention
    # macros (apientry etc.), unlike the header files.
    # Because the 1.0 core branch includes ref pages for extensions,
    # all the extension interfaces need to be generated, even though
    # none are used by the core spec itself.
    genOpts['apiinc'] = [
          DocOutputGenerator,
          DocGeneratorOptions(
            conventions       = conventions,
            filename          = 'apiinc',
            directory         = directory,
            genpath           = genpath,
            apiname           = 'openxr',
            profile           = None,
            versions          = featuresPat,
            emitversions      = featuresPat,
            defaultExtensions = None,
            addExtensions     = addExtensionsPat,
            removeExtensions  = removeExtensionsPat,
            emitExtensions    = emitExtensionsPat,
            prefixText        = prefixStrings + xrPrefixStrings,
            apicall           = '',
            apientry          = '',
            apientryp         = '*',
            alignFuncParam    = 48,
            secondaryInclude  = True,
            expandEnumerants  = False,
            extEnumerantAdditions = True)
        ]

    # Python and Ruby representations of API information, used by scripts
    # that do not need to load the full XML.
    genOpts['apimap.py'] = [
          PyOutputGenerator,
          DocGeneratorOptions(
            conventions       = conventions,
            filename          = 'apimap.py',
            directory         = directory,
            apiname           = 'openxr',
            profile           = None,
            versions          = featuresPat,
            emitversions      = featuresPat,
            defaultExtensions = None,
            addExtensions     = addExtensionsPat,
            removeExtensions  = removeExtensionsPat,
            emitExtensions    = emitExtensionsPat)
        ]

    # Python and Ruby representations of API information, used by scripts
    # that do not need to load the full XML.
    genOpts['apimap.rb'] = [
          RubyOutputGenerator,
          DocGeneratorOptions(
            conventions       = conventions,
            filename          = 'apimap.rb',
            directory         = directory,
            apiname           = 'openxr',
            profile           = None,
            versions          = featuresPat,
            emitversions      = featuresPat,
            defaultExtensions = None,
            addExtensions     = addExtensionsPat,
            removeExtensions  = removeExtensionsPat,
            emitExtensions    = emitExtensionsPat)
        ]

    # Index chapter
    genOpts['index.adoc'] = [
          DocIndexOutputGenerator,
          DocGeneratorOptions(
            conventions       = conventions,
            filename          = 'index.adoc',
            directory         = directory,
            apiname           = 'openxr',
            profile           = None,
            versions          = featuresPat,
            emitversions      = featuresPat,
            defaultExtensions = None,
            addExtensions     = addExtensionsPat,
            removeExtensions  = removeExtensionsPat,
            emitExtensions    = emitExtensionsPat)
        ]

    # Core API validity files for spec
    genOpts['validinc'] = [
          ValidityOutputGenerator,
          DocGeneratorOptions(
            conventions       = conventions,
            filename          = 'validinc',
            directory         = directory,
            apiname           = 'openxr',
            profile           = None,
            versions          = featuresPat,
            emitversions      = featuresPat,
            defaultExtensions = None,
            addExtensions     = addExtensionsPat,
            removeExtensions  = removeExtensionsPat,
            emitExtensions    = emitExtensionsPat)
        ]

    # Core API host sync table files for spec
    genOpts['hostsyncinc'] = [
          HostSynchronizationOutputGenerator,
          DocGeneratorOptions(
            conventions       = conventions,
            filename          = 'hostsyncinc',
            directory         = directory,
            apiname           = 'openxr',
            profile           = None,
            versions          = featuresPat,
            emitversions      = featuresPat,
            defaultExtensions = None,
            addExtensions     = addExtensionsPat,
            removeExtensions  = removeExtensionsPat,
            emitExtensions    = emitExtensionsPat)
        ]

    # Extension metainformation for spec extension appendices
    genOpts['extinc'] = [
          ExtensionMetaDocOutputGenerator,
          ExtensionMetaDocGeneratorOptions(
            conventions       = conventions,
            filename          = 'extinc',
            directory         = directory,
            apiname           = 'openxr',
            profile           = None,
            versions          = featuresPat,
            emitversions      = None,
            defaultExtensions = defaultExtensions,
            addExtensions     = None,
            removeExtensions  = None,
            emitExtensions    = emitExtensionsPat)
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
    if args.target in genOpts:
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
        return None


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
    parser.add_argument('-standaloneExtension', action='store',
                        default='',
                        help='Specify an extension to emit during standalone_header generation')
    parser.add_argument('-standalonePrefixOverride', action='append',
                        default=[],
                        help='Override the header prefix of headers generated with "standalone_header"')

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
                        default='xr.xml',
                        help='Use specified registry file instead of xr.xml')
    parser.add_argument('-time', action='store_true',
                        help='Enable timing')
    parser.add_argument('-genpath', action='store', default='generated',
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
        setLogFile(setDiag = True, setWarn = True, filename = '-')

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

    if args.dump:
        write('* Dumping registry to regdump.txt', file=sys.stderr)
        reg.dumpReg(filehandle=open('regdump.txt', 'w', encoding='utf-8'))

    # Finally, use the output generator to create the requested target
    if args.debug:
        pdb.run('reg.apiGen()')
    else:
        startTimer(args.time)
        reg.apiGen()
        endTimer(args.time, '* Time to generate ' + options.filename + ' =')

    if not args.quiet:
        logDiag('* Generated', options.filename)
