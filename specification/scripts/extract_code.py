#!/usr/bin/python3
#
# Copyright 2018-2025 The Khronos Group Inc.
# Copyright (c) 2018 Collabora, Ltd.
#
# SPDX-License-Identifier: Apache-2.0
#
# Author(s):    Rylie Pavlik <rylie.pavlik@collabora.com>
#
# Purpose:      This script searches for and extracts embedded source code
#               from specification chapters.

import argparse
import errno
import re
from enum import Enum, unique
from pathlib import Path

from spec_tools.file_process import LinewiseFileProcessor

ROOT = Path(__file__).resolve().parent.parent.parent
ALL_DOCS = sorted((ROOT / 'specification/sources/').glob('**/*.adoc'))

CODEDIR = ROOT / 'specification/example-builds'
GENCODEDIR = CODEDIR / 'generated'


@unique
class Language(Enum):
    C = 'C'
    CPP = 'C++'
    XML = 'XML'
    ASCIIDOC = 'asciidoc'
    JSON = 'JSON'
    SH = 'sh'

    UNKNOWN = 'UNKNOWN'

    def __str__(self):
        return self.value

    @property
    def extension(self):
        if self == Language.UNKNOWN:
            raise RuntimeError(
                "Can't get extension for UNKNOWN language")
        if self == Language.ASCIIDOC:
            return 'adoc'
        return str(self).lower().replace('+', 'p')

    @classmethod
    def from_string(cls, s):
        s = s.casefold()
        for val in Language:
            if s == val.value.casefold():
                return val
        return Language.UNKNOWN


class CodeExtractor(LinewiseFileProcessor):
    def __init__(self, output_line_numbers=False, quiet=False):
        super().__init__()
        self.MIN_LINES = 5

        self.output_line_numbers = output_line_numbers
        self.quiet = quiet

        self.next_snippet_id = 0
        self.in_code_block = False
        self.block_pattern = re.compile(r'\[source(,?)(?P<tags>.*)\]')
        self.languages_to_extract = set((Language.CPP, Language.C))
        self.code_lines = None

        self.generated_files = []

        # list of (generated file path, include path) pairs
        self.deps = []

        # key: generated file path
        # value: line number where a code snippet starts
        self.origins = {}

    def get_unique_id(self):
        ret = self.next_snippet_id
        self.next_snippet_id += 1
        return ret

    def make_numbered_filename(self, language):
        name = self.filename.with_suffix('.{num}.{ext}'.format(
            num=self.get_unique_id(), ext=language.extension)).name
        return GENCODEDIR / name

    def print_message(self, s):
        if not self.quiet:
            print(f'{self.filename}:{self.line_number}: {s}')

    def process_start_of_code_block(self):
        prev_line = self.get_preceding_line()
        if not prev_line:
            # No previous line to find language.
            return

        code_block_tag = self.block_pattern.match(prev_line.rstrip())
        if not code_block_tag:
            # Not going to handle this.
            return

        tags = set(t.strip() for t in code_block_tag.group('tags').casefold().split(','))

        lang = Language.UNKNOWN
        for tag in tags:
            lang = Language.from_string(tag)
            if lang != Language.UNKNOWN:
                break

        self.language = lang

        if self.language == Language.UNKNOWN:
            self.print_message('Not extracting code snippet introduced with {} (tags = {})'.format(
                code_block_tag.group(), tags))
            return

        if self.language not in self.languages_to_extract:
            self.print_message('Not extracting code snippet identified as {}'.format(
                self.language))
            return
        if 'suppress-build' in tags:
            self.print_message(
                'Suppressing extraction of code snippet because we saw "suppress-build"')
            return

        self.code_lines = []
        self.start_of_code_block = self.line_number

    def process_end_of_code_block(self):
        if self.code_lines is None:
            return

        code_lines = self.code_lines
        self.code_lines = None

        if len(code_lines) < self.MIN_LINES:
            self.print_message(
                f'Not extracting code snippet - only {len(code_lines)} lines.')
            return

        out_filename = self.make_numbered_filename(self.language)
        self.print_message('Writing {} extracted lines to file {}\n'.format(
            len(code_lines), out_filename.relative_to(Path('.').resolve())))
        self.generated_files.append(out_filename)

        self.origins[out_filename] = self.start_of_code_block

        include_file = CODEDIR / out_filename.with_suffix('.h').name

        with out_filename.open('w', encoding='utf-8') as f:
            f.write('#include "common_include.h"\n')
            if include_file.exists():
                f.write(f'#include "{include_file.name}"\n\n')
                self.deps.append((out_filename, include_file))
            f.write('void func() {\n')
            f.write(''.join(code_lines))
            f.write('\n}\n')

    def process_code_block_line(self):
        if self.code_lines is not None:
            if self.output_line_numbers:
                self.code_lines.append(f'# {self.line_number} "{self.filename}\"\n')
            self.code_lines.append(self.line)

    def process_line(self, line_num, line):
        if line.startswith('---'):
            # Toggle code block status.
            self.in_code_block = not self.in_code_block

            if self.in_code_block:
                # We just started a code block
                self.process_start_of_code_block()
            else:
                # We just ended one.
                self.process_end_of_code_block()

        elif self.in_code_block:
            self.process_code_block_line()


class CodeExtractorGroup(object):
    def __init__(self, output_line_numbers=False, quiet=False):
        self.output_line_numbers = output_line_numbers
        self.quiet = quiet

        # key: adoc file path. value: list of generated source files.
        self.generated_files = {}

        # all generated sources files
        self.all_generated = []

        # list of (generated file path, include path) pairs
        self.deps = []

        # key: generated file path
        # value: (adoc file path, line number where a code snippet starts) pair
        self.origins = {}

    def process(self, files):
        for fn in files:
            extractor = CodeExtractor(output_line_numbers=self.output_line_numbers,
                                      quiet=self.quiet)
            extractor.process_file(fn)

            if extractor.generated_files:
                self.generated_files[extractor.filename] = extractor.generated_files
                self.all_generated.extend(extractor.generated_files)
                self.deps.extend(extractor.deps)
                self.origins.update({fn: (extractor.filename, line_num)
                                     for fn, line_num in extractor.origins.items()})

    def output_makefile(self, makefile):
        with open(makefile, 'w', encoding='utf-8') as f:

            generated_c_string = ' \\\n'.join(str(fn)
                                              for fn in self.all_generated if fn.suffix == '.c')
            generated_cpp_string = ' \\\n'.join(str(fn)
                                                for fn in self.all_generated if fn.suffix == '.cpp')
            deps_string = '\n'.join(f"{fn.with_suffix('.o')}: {dep} $(CODEDIR)/common_include.h"
                                    for fn, dep in self.deps)
            extra_arg = ''
            if self.output_line_numbers:
                extra_arg = '--line_numbers'
            f.write("""
OUTDIR  ?= $(CURDIR)/{out}
CODEDIR ?= $(CURDIR)/{codedir}
PYTHON   ?= python3
QUIET    ?= @

GENERATED_C := {c}
C_OBJECTS := $(patsubst %.c,%.o,$(GENERATED_C))

GENERATED_CPP := {cpp}
CPP_OBJECTS := $(patsubst %.cpp,%.o,$(GENERATED_CPP))

build-examples: $(C_OBJECTS) $(CPP_OBJECTS)
.PHONY: build-examples

clean-examples:
\trm -f $(C_OBJECTS) $(CPP_OBJECTS)
.PHONY: clean-examples

$(C_OBJECTS) : %.o : %.c $(OUTDIR)/openxr/openxr.h
\t@echo '$(ORIGIN)'
\t$(QUIET)gcc -std=gnu99 -c -I$(OUTDIR) -I$(CODEDIR) $< -o $@

$(CPP_OBJECTS) : %.o : %.cpp $(OUTDIR)/openxr/openxr.h
\t@echo '$(ORIGIN)'
\t$(QUIET)g++ -std=gnu++11 -c -I$(OUTDIR) -I$(CODEDIR) $< -o $@

ifeq ($(strip $(QUIET)),@)
EXTRACT_QUIET := --quiet
endif

$(GENERATED_C) $(GENERATED_CPP) {makefile}: {script} {inputs}
\t$(QUIET)$(PYTHON) $< {extra} --makefile={makefile} $(EXTRACT_QUIET)

gen: {script}
\t$(QUIET)$(PYTHON) $< {extra} --makefile={makefile} $(EXTRACT_QUIET)
.PHONY: gen

{deps}
""".format(out=(ROOT / 'specification' / 'generated' / 'out' / '1.1').relative_to(Path('.').resolve()),
                codedir=CODEDIR.relative_to(Path('.').resolve()),
                c=generated_c_string,
                cpp=generated_cpp_string,
                makefile=makefile,
                script=Path(__file__),
                extra=extra_arg,
                inputs=' '.join(str(infile)
                                for infile in self.generated_files),
                deps=deps_string))
            for fn, gen in self.generated_files.items():
                f.write('{stem}: {files}\n.PHONY: {stem}\n'.format(
                    stem=fn.stem, files=' '.join(str(g.with_suffix('.o')) for g in gen)))
            if self.origins:
                width = max(len(generated.name) for generated in self.origins)

                for generated, origin in self.origins.items():
                    origin_file, origin_line = origin
                    if generated.suffix == '.cpp':
                        compiler = '[c++] '
                    else:
                        compiler = '[cc]  '
                    origin_str = '{} {} extracted from {}:{}'.format(compiler, generated.name.ljust(width),
                                                                     origin_file, origin_line)
                    f.write(f"{generated.with_suffix('.o')}: ORIGIN := {origin_str}\n")


if __name__ == "__main__":

    # if it already exists, that's OK
    try:
        GENCODEDIR.mkdir(parents=True)
    except OSError as e:
        if e.errno != errno.EEXIST:
            raise
    parser = argparse.ArgumentParser()
    parser.add_argument("file",
                        help="Only extract from the indicated file(s). By default, all chapters and extensions are examined.",
                        nargs="*")
    parser.add_argument("--line_numbers", "--linenumbers",
                        help='Add lines of the form "# line_number filename" to the output, for build errors/warnings that point to the adoc files.',
                        action='store_true')
    parser.add_argument('--makefile',
                        help='Output a makefile with a build-examples target (and matching clean-examples target).',
                        type=str)
    parser.add_argument('--quiet', '-q',
                        help="Don't output debug information about what we are extracting and not extracting.",
                        action='store_true')
    # type=argparse.FileType('w', encoding='UTF-8'))
    args = parser.parse_args()

    if args.file:
        files = [Path(f).resolve() for f in args.file]
    else:
        files = ALL_DOCS

    extractors = CodeExtractorGroup(output_line_numbers=args.line_numbers,
                                    quiet=args.quiet)
    extractors.process(files)

    if args.makefile:
        extractors.output_makefile(args.makefile)
