# Copyright (c) 2013-2021, The Khronos Group Inc.
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

SHELL = /usr/bin/env bash

QUIET      ?= @
PYTHON     ?= python3
ASCIIDOC   ?= asciidoctor
RM          = rm -f
RMRF        = rm -rf
MKDIR       = mkdir -p
CP          = cp -p
MV          = mv
ECHO        = @echo

# Use GENXR_OPTIONS to add arguments like -time to scripts/genxr.py invocations
GENXR_ARGS += $(GENXR_OPTIONS) -registry $(REGISTRY)

ifneq (,$(strip $(VERBOSE)))
ASCIIDOC := $(ASCIIDOC) --verbose
else
GENXR_ARGS += -q
endif

ifneq (,$(strip $(STRICT)))
ASCIIDOC := $(ASCIIDOC) --failure-level ERROR
GENXR_ARGS += -validate
endif

ifneq (,$(strip $(VERY_STRICT)))
ASCIIDOC := $(ASCIIDOC) --failure-level WARN
endif

SPECREVISION = 1.0.20
REVISION_COMPONENTS = $(subst ., ,$(SPECREVISION))
MAJORMINORVER = $(word 1,$(REVISION_COMPONENTS)).$(word 2,$(REVISION_COMPONENTS))

# Target directory for output files.
OUTDIR	 ?= out/$(MAJORMINORVER)

# Target directory for generated files.
# This is shorthand; can't be moved because the spec source
# files expect to find these here.
GENDIR    = generated

# Generator scripts and options
# GENOPTS can be e.g. '-noprotect'
GENOPTS =

SCRIPTS       := ./scripts
SPECTOOLS     := $(SCRIPTS)/spec_tools
XRAPI         := scripts/xrapi.py
METADIR       := sources/chapters/extensions/meta

VK_REF_PAGE_ROOT := https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html

# Gets built automatically
ASCIIDOCTOR_TARGETS :=

default:
	$(ECHO) "Makefile help - targets:"
	$(ECHO) "header:           build OpenXR header"
	@if [ -d sources/chapters ]; then \
	    echo "html:             HTML version of OpenXR spec"; \
	    echo "pdf:              PDF version of OpenXR spec (Letter [8.5 x 11] paper size)"; \
	    echo "pdfA4:            PDF version of OpenXR spec (A4 paper size)"; \
	    echo "manhtmlpages:     HTML separate per-feature reference pages"; \
	    echo "build-examples:   try compiling all examples"; \
	    echo "check-spec-links: run the checkMarkup and checkSpecLinks scripts"; \
	fi

	@if [ -d sources/extprocess ]; then \
	    echo "extprocess:       HTML version of OpenXR extension process"; \
	fi

	@if [ -f sources/test.c ]; then \
	    echo "header-test:      test-compile the OpenXR header"; fi

	@if [ -d sources/styleguide ]; then \
	    echo "styleguide:       HTML version of styleguide"; fi

	@if [ -d loader ]; then \
	    echo "loader:           HTML version of the loader doc"; fi

	$(ECHO) "all:              build as many of these as possible: header header-test html pdf styleguide loader build-examples check-spec-links"

	@if [ -d sources/chapters ] && [ -d loader ]; then \
	    echo "release-htmlpdf:  build html pdf and copy output to an OpenXR-Registry-like layout"; \
	    echo "release:          build html pdf manhtmlpages loader styleguide extprocess and copy output to an OpenXR-Registry-like layout"; \
	fi

	$(ECHO) "reflow:           reflow AsciiDoctor source files in-place"
	$(ECHO) "clean_dirt:       remove intermediate files"
	$(ECHO) "clean:            remove targets and intermediate files"
	$(ECHO) ""
	$(ECHO) "Variables controlling options:"
	$(ECHO) "Pass QUIET= to disable quiet mode (echos all commands)"
	$(ECHO) "Pass VERBOSE=1 to invoke asciidoctor with the --verbose option and genxr.py without the -q option"
	$(ECHO) "Pass STRICT=1 to invoke asciidoctor with the --failure-level ERROR option"
	$(ECHO) "Pass VERY_STRICT=1 to invoke asciidoctor with the --failure-level WARN option"
	$(ECHO) "Pass RELEASE=1 to turn off all asciidoctor date/timestamps for a reproducible release build"
	@if [ -d sources/chapters ] && [ -d loader ]; then \
	    echo "Pass some string for RELEASE_SPECS_SUFFIX to add a suffix to the version-numbered directory for the release targets"; fi

################################################
# Store our build configuration in a file, and force re-generation if it changes.
# Most common thing that changes is the extension list.

APITITLE ?= OpenXR
EXTS := $(sort $(EXTENSIONS))

CONFIG_STAMP_FN := $(OUTDIR)/config-stamp

# This is the stuff stored/checked
STAMP_DATA := $(EXTS) "$(APITITLE)"

# Depend on this target to force checking the config for changes.
config_stamp:
	$(QUIET)$(MKDIR) $(dir $(CONFIG_STAMP_FN))
	$(QUIET)if ! $(PYTHON) $(SCRIPTS)/check_stamp.py $(CONFIG_STAMP_FN) $(STAMP_DATA); then $(MAKE) clean_generated; fi
.PHONY: config_stamp

################################################
## OpenXR header file targets

HEADER_DIR    := $(OUTDIR)/openxr
HEADER        := $(HEADER_DIR)/openxr.h
PLATHEAD      := $(HEADER_DIR)/openxr_platform.h
REFLECTHEAD   := $(HEADER_DIR)/openxr_reflection.h
GENHEADERS    := $(HEADER) $(PLATHEAD) $(REFLECTHEAD)
DEFINESHEAD   := $(HEADER_DIR)/openxr_platform_defines.h

# Platform header (just copy)
$(DEFINESHEAD): ../include/openxr/openxr_platform_defines.h
	$(QUIET)$(MKDIR) $(@D)
	$(QUIET)$(CP) $< $@

# Named target to generate all headers
header: $(GENHEADERS) $(DEFINESHEAD)
.PHONY: header

TESTSRC := sources/test.c
# Test that openxr.h compiles
header-test: header $(TESTSRC)
	$(CC) -Wall -pedantic -std=c99 -c -I$(OUTDIR) $(TESTSRC)
	$(CXX) -Wall -c -std=c++98 -I$(OUTDIR) $(TESTSRC)
	$(CXX) -Wall -c -std=c++11 -I$(OUTDIR) $(TESTSRC)
	rm test.o
.PHONY: header-test

################################################
# Generated files: headers or includes referenced in spec.
# Some nominal targets are just "stamp" files generated.

REGISTRY    := registry/xr.xml
GENXR       := $(SCRIPTS)/genxr.py
BASIC_GENERATED_DEPENDS := \
  $(REGISTRY) \
  $(GENXR) \
  $(SCRIPTS)/reg.py \
  $(SCRIPTS)/generator.py \
  $(SPECTOOLS)/util.py \
  $(SPECTOOLS)/conventions.py \
  $(SCRIPTS)/xrconventions.py \
  config_stamp \
  Makefile

# Stamp files for the generated includes
GENSTAMPS := \
  $(GENDIR)/api/apiinc \
  $(GENDIR)/validity/validinc \
  $(GENDIR)/hostsynctable/hostsyncinc \
  $(METADIR)/extinc \

# The actual generated index
GENDEPENDS := $(GENSTAMPS) $(GENDIR)/index.adoc

# The rule for every genxr-generated file
$(GENDEPENDS) $(GENHEADERS) $(XRAPI): $(BASIC_GENERATED_DEPENDS)
	$(ECHO) "[genxr]       $(REGISTRY) -> $@"
	@if [ "x$(STAMP_NOTE)" != "x" ]; then echo "                                 $(STAMP_NOTE)"; fi
	$(QUIET)$(MKDIR) $(@D)
	$(QUIET)$(PYTHON) $(GENXR) $(GENXR_ARGS) $(EXTOPTIONS) -o $(@D) $(@F)

# Print an extra note for stamp files
$(GENSTAMPS): STAMP_NOTE = (and additional files in $(@D))

# Extra deps
$(GENDIR)/api/apiinc: $(SCRIPTS)/docgenerator.py
$(GENDIR)/validity/validinc: $(SCRIPTS)/validitygenerator.py $(SPECTOOLS)/validity.py $(SPECTOOLS)/attributes.py $(SPECTOOLS)/data_structures.py
$(XRAPI): $(SCRIPTS)/pygenerator.py $(SCRIPTS)/docgenerator.py
$(GENHEADERS): $(SCRIPTS)/cgenerator.py
$(REFLECTHEAD): $(SCRIPTS)/creflectiongenerator.py $(SCRIPTS)/jinja_helpers.py

# The actual generated files depend on their stamp file.
GENAPI = $(wildcard $(GENDIR)/api/*/[A-Za-z]*.txt)
$(GENAPI): $(GENDIR)/api/apiinc

GENVALIDITY = $(wildcard $(GENDIR)/validity/*/[A-Za-z]*.txt)
$(GENVALIDITY): $(GENDIR)/validity/validinc

GENSYNC := \
  $(GENDIR)/hostsynctable/implicit.txt \
  $(GENDIR)/hostsynctable/parameterlists.txt \
  $(GENDIR)/hostsynctable/parameters.txt
$(GENSYNC): $(GENDIR)/hostsynctable/hostsyncinc

GENMETA = $(wildcard $(METADIR)/extinc/[A-Za-z]*.adoc)
$(GENMETA): $(METADIR)/extinc

# The actual generated include files
GENINCLUDE = $(GENAPI) $(GENVALIDITY) $(GENSYNC) $(GENMETA)
.PHONY: generated
generated: $(GENDEPENDS)

################################################
# OpenXR Style Guide

SPECSRC = sources/openxr.adoc
SPECFILES = $(wildcard sources/chapters/[A-Za-z]*.adoc)
STYLEGUIDE = $(OUTDIR)/styleguide.html
STYLESRC = sources/styleguide/styleguide.adoc
STYLEFILES = $(wildcard sources/styleguide/[A-Za-z]*.adoc)

styleguide: $(STYLEGUIDE)

# Use the AsciiDoctor rule
ASCIIDOCTOR_TARGETS += $(STYLEGUIDE)

# Target-specific variables and deps customizing the AsciiDoctor rule
$(STYLEGUIDE): SPECSRC=$(STYLESRC)
$(STYLEGUIDE): LOGFILE=$(OUTDIR)/adoc_styleguide_stderr.txt
$(STYLEGUIDE): $(STYLESRC) $(STYLEFILES) $(GENDIR)/validity/validinc $(GENDIR)/api/apiinc


################################################
# OpenXR Loader Guide

LOADERGUIDE = $(OUTDIR)/loader.html
LOADERSRC = loader/loader.adoc
LOADERFILES = $(wildcard loader/[A-Za-z]*.adoc)

loader: $(LOADERGUIDE)

# Use the AsciiDoctor rule
ASCIIDOCTOR_TARGETS += $(LOADERGUIDE)

# Target-specific variables and deps customizing the AsciiDoctor rule
$(LOADERGUIDE): SPECSRC=$(LOADERSRC)
$(LOADERGUIDE): LOGFILE=$(OUTDIR)/adoc_loader_stderr.txt
$(LOADERGUIDE): $(LOADERSRC) $(LOADERFILES)


################################################
# OpenXR Extension Process

EXTPROCESSGUIDE = $(OUTDIR)/extprocess.html
EXTPROCESSSRC = sources/extprocess/extension_process.adoc

extprocess: $(EXTPROCESSGUIDE)

# Use the AsciiDoctor rule
ASCIIDOCTOR_TARGETS += $(EXTPROCESSGUIDE)

$(EXTPROCESSGUIDE): SPECSRC=$(EXTPROCESSSRC)
$(EXTPROCESSGUIDE): LOGFILE=$(OUTDIR)/adoc_extprocess_stderr.txt
$(EXTPROCESSGUIDE): $(EXTPROCESSSRC)

################################################
## Specification targets

COMMONDOCS = $(SPECSRC) $(SPECFILES) $(GENINCLUDE) $(GENDEPENDS)

SPEC_FILENAME_STEM := openxr

## HTML
HTMLSPEC      := $(OUTDIR)/$(SPEC_FILENAME_STEM).html
html: $(HTMLSPEC)

# Use the AsciiDoctor rule
ASCIIDOCTOR_TARGETS += $(HTMLSPEC)

# Target-specific variables and deps customizing the AsciiDoctor rule
$(HTMLSPEC): LOGFILE=$(OUTDIR)/adoc_html_stderr.txt
$(HTMLSPEC): ATTRIBOPTS+=-a sectanchors
$(HTMLSPEC): $(COMMONDOCS)
$(HTMLSPEC): POSTPROCESS=$(QUIET)$(PYTHON) $(SCRIPTS)/genanchorlinks.py $@ $@

## PDF
PDFSPEC       := $(OUTDIR)/$(SPEC_FILENAME_STEM).pdf
PDFA4SPEC     := $(OUTDIR)/$(SPEC_FILENAME_STEM).a4.pdf

pdf pdfLetter: $(PDFSPEC)
pdfA4: $(PDFA4SPEC)

# Use the AsciiDoctor rule
ASCIIDOCTOR_TARGETS += $(PDFSPEC) $(PDFA4SPEC)

# Target-specific variables and deps customizing the AsciiDoctor rule
$(PDFSPEC) $(PDFA4SPEC): LOGFILE=$(OUTDIR)/adoc_pdf_stderr.txt
$(PDFSPEC) $(PDFA4SPEC): BACKEND_ARGS=--backend pdf --require asciidoctor-pdf -a compress  -r ./scripts/pdf-index-customizer.rb
$(PDFSPEC): PAGESIZE=LETTER
$(PDFA4SPEC): PAGESIZE=A4
$(PDFSPEC) $(PDFA4SPEC): $(COMMONDOCS)

################################################
## Shared asciidoctor rule

EXTATTRIBS := $(foreach ext,$(EXTS),-a $(ext))
EXTOPTIONS := $(foreach ext,$(EXTS),-extension $(ext))

# Generate Asciidoc attributes for spec revision remark.


# Spell out RFC2822 format as not all date commands support -R
SPECDATE     = $(shell echo `date -u "+%a, %d %b %Y %T %z"`)

# GITBRANCH should be set by the caller in CI environments
# because CI machines often checkout the code in detached HEAD state.
ifeq ($(GITBRANCH),)
# Evaluate only once to avoid slow calls to git.
GITBRANCH  := $(shell echo `git symbolic-ref --short HEAD`)
endif
GITREMARK  ?= from git branch: $(GITBRANCH)

ATTRIBOPTS   = -a revnumber="$(SPECREVISION)" \
               -a revremark="$(SPECREMARK)" \
               -a apititle="$(APITITLE)" \
               -a stem=latexmath \
               -a config=$(CURDIR)/man \
               -a pdf-page-size=$(PAGESIZE) \
               -a pdf-stylesdir=config \
               -a pdf-style=pdf \
               $(EXTATTRIBS)

ADOCOPTS = --doctype book -a data-uri -r $(CURDIR)/scripts/openxr-macros.rb $(ATTRIBOPTS)

ifneq (,$(strip $(RELEASE)))
# No dates or internal commit hashes in release builds for reproducibility
ATTRIBOPTS   += -a revdate!
ATTRIBOPTS   += -a last-update-label!
ATTRIBOPTS   += -a reproducible
SPECREMARK   ?= $(GITREMARK)

else
ATTRIBOPTS   += -a revdate="$(SPECDATE)"
SPECREMARK   ?= $(GITREMARK) \
                commit: $(shell echo `git log -1 --format="%H"`)

endif

ifneq (,$(strip $(KHRONOS_SPEC_LICENSED)))
# Enable photo of JP in dedication: not CC-BY-4.0,
# so only used in generated docs now.
ATTRIBOPTS   += -a include-dedication-photo
endif

# Default to html5
BACKEND_ARGS=--backend html5

# AsciiDoctor rule - customized by the places where these are described
$(ASCIIDOCTOR_TARGETS):
	$(ECHO) "[asciidoctor] $(SPECSRC) -> $@"
	$(QUIET)$(MKDIR) $(@D)
	$(QUIET)if [ $$(uname -s | cut -c 1-6) == "CYGWIN" ]; then \
	OUTSPEC_DOS=$$(cygpath -w $@) ;\
	SPECSRC_DOS=$$(cygpath -w $(SPECSRC)) ;\
	ATTRIBOPTS_DOS='$(ATTRIBOPTS)' ;\
	ADOCOPTS_DOS="--doctype book -a data-uri -r $$(cygpath -w $(CURDIR)/scripts/openxr-macros.rb) $$ATTRIBOPTS_DOS" ;\
	BATCH_FILE=$$(cygpath -w $$(mktemp)).bat ;\
	echo $(ASCIIDOC) $$ADOCOPTS_DOS $(BACKEND_ARGS) --out-file $$OUTSPEC_DOS $$SPECSRC_DOS > $$BATCH_FILE ;\
	CMD /C $$BATCH_FILE ;\
	rm -f $$BATCH_FILE ;\
	else \
	$(ASCIIDOC) $(ADOCOPTS) $(BACKEND_ARGS) --out-file $@ $(SPECSRC) 2>&1 | tee $(LOGFILE) ;\
	if [ -s $(LOGFILE) ]; then \
		false; \
	else \
		rm $(LOGFILE); \
	fi; \
	fi
	$(POSTPROCESS)
# TODO: Postprocess step(s) may have broken the Cygwin build. Not sure this matters anymore with WSL, though.


################################################
# Reference "man" pages extracted from spec

MANDIR      := man
MANHTMLDIR   = $(OUTDIR)/man/html
KHRSOURCES   = $(wildcard $(MANDIR)/*KHR.txt)
MACROSOURCES = $(wildcard $(MANDIR)/XR_*[A-Z][A-Z].txt)
VENSOURCES   = $(filter-out $(KHRSOURCES) $(MACROSOURCES),$(wildcard $(MANDIR)/*[A-Z][A-Z].txt))
CORESOURCES  = $(filter-out $(KHRSOURCES) $(VENSOURCES),$(wildcard $(MANDIR)/[Xx][Rr]*.txt $(MANDIR)/PFN*.txt))
MANSOURCES   = $(CORESOURCES) $(VENSOURCES) $(KHRSOURCES)
MANCOPYRIGHT = $(MANDIR)/copyright-ccby.txt $(MANDIR)/footer.txt
MANGENERATED = $(filter-out $(MANCOPYRIGHT),$(wildcard $(MANDIR)/*))
MANHTML      = $(MANSOURCES:$(MANDIR)/%.txt=$(MANHTMLDIR)/%.html)
MANDEPS      = $(MANCOPYRIGHT) $(GENINCLUDE) $(GENDEPENDS)
HTML_SPEC_RELATIVE ?= ../../$(SPEC_FILENAME_STEM).html
MANATTRIBOPTS := -a stylesheet=khronos.css \
				 -a stylesdir=$(CURDIR)/config \
				 -a html_spec_relative='$(HTML_SPEC_RELATIVE)' \
				 -a imagesdir=$(CURDIR)/sources \
				 -a vkRefPageRoot='$(VK_REF_PAGE_ROOT)'


# Pure makefile lowercase function, generated by a script.
make_lower = $(subst A,a,$(subst B,b,$(subst C,c,$(subst D,d,$(subst E,e,$(subst F,f,$(subst G,g,$(subst H,h,$(subst I,i,$(subst J,j,$(subst K,k,$(subst L,l,$(subst M,m,$(subst N,n,$(subst O,o,$(subst P,p,$(subst Q,q,$(subst R,r,$(subst S,s,$(subst T,t,$(subst U,u,$(subst V,v,$(subst W,w,$(subst X,x,$(subst Y,y,$(subst Z,z,$(1)))))))))))))))))))))))))))
EXTENSIONS_LOWER := $(call make_lower,$(EXTENSIONS))

# Function that turns khr_vulkan_enable into sources/chapters/extensions/khr/khr_vulkan_enable.adoc
make_extension_source = sources/chapters/extensions/$(word 1,$(subst _, ,$(1)))/$(1).adoc

# Call make_extension_source on every enabled extension, after lowercasing and stripping the leading XR prefix.
EXTENSION_SOURCES := $(foreach ext,$(patsubst xr_%,%,$(EXTENSIONS_LOWER)),$(call make_extension_source,$(ext)))

# Generation of ref page asciidoctor sources by extraction from the
# specification.
#
# Treating the all-in-one ref page source apispec.txt as the "stamp" for genRef.
LOGFILE := man/logfile
man/apispec.txt: $(SPECFILES) $(EXTENSION_SOURCES) $(SCRIPTS)/genRef.py $(SCRIPTS)/reflib.py $(SCRIPTS)/xrapi.py
	$(ECHO) "[genRef.py]   $(REGISTRY) and spec -> $@"
	$(ECHO) "                                          (and additional files in $(@D))"
	$(QUIET)$(PYTHON) $(SCRIPTS)/genRef.py -log $(LOGFILE) -registry $(REGISTRY) $(EXTOPTIONS) $(SPECFILES) $(EXTENSION_SOURCES)
	$(QUIET)grep "ERROR:" $(LOGFILE)

# The recursive $(MAKE) is an apparently unavoidable hack, since the
# actual list of man page sources isn't known until after
# man/apispec.txt is generated.
manhtmlpages: man/apispec.txt
	$(QUIET)$(MAKE) buildmanpages

buildmanpages: $(MANHTML) $(MANHTMLDIR)/openxr.html

# This is the single-page ref page.
# Manually defining doctype-manpage so that we can use "book" style but still enable the refpage-only portions.
$(MANHTMLDIR)/openxr.html: man/apispec.txt $(MANDEPS)
	$(ECHO) "[asciidoctor] $< -> $@"
	$(QUIET)$(MKDIR) $(@D)
	$(QUIET)$(ASCIIDOC) -b html5 $(MANATTRIBOPTS) $(ADOCOPTS) -a doctype-manpage -d book -o $@ $<

# This is all the individual-page ref pages.
$(MANHTML): $(MANHTMLDIR)/%.html: $(MANDIR)/%.txt $(MANDEPS)
	$(ECHO) "[asciidoctor] $< -> $@"
	$(QUIET)$(MKDIR) $(@D)
	$(QUIET)$(ASCIIDOC) -b html5 $(MANATTRIBOPTS) $(ADOCOPTS) -a cross-file-links -d manpage -o $@ $<

################################################
# Extension dependency script

DEPSCRIPT = $(SCRIPTS)/make_ext_dependency.py
DEPSCRIPTOUT := out/extDependency.sh
$(DEPSCRIPTOUT): $(REGISTRY) $(DEPSCRIPT)
	$(ECHO) "[mk_ext_dep]  $(REGISTRY) -> $@"
	$(QUIET)$(MKDIR) $(@D)
	$(QUIET)$(PYTHON) $(DEPSCRIPT) -registry $< -outscript $@

################################################
# Embedded example code

# Script that extracts code to .cpp and .c files, and
# generates an associated makefile
EXAMPLES_SCRIPT = $(SCRIPTS)/extract_code.py

# The makefile generated by extract_code.py
EXAMPLES_MAKEFILE = examples.mk

ifeq ($(strip $(QUIET)),@)
EXTRACT_QUIET := --quiet
endif

$(EXAMPLES_MAKEFILE): $(EXAMPLES_SCRIPT)
	$(QUIET)$(PYTHON) $< --makefile=$@ --line_numbers $(EXTRACT_QUIET)

build-examples: $(EXAMPLES_MAKEFILE) header
	$(QUIET)$(MAKE) -f $(EXAMPLES_MAKEFILE)
.PHONY: build-examples

################################################
# Check Markup, Check Spec Links, check XML schema
CHECK_MARKUP_SCRIPT = $(CURDIR)/checkMarkup

# Edit the following line's "ignore_count" when the number of checkSpecLinks errors changes,
# either by reducing (ensuring we keep the improvement)
# or by increasing (if there's an error we can't resolve right now and don't want to break CI)
CHECK_SPEC_LINKS_SCRIPT = $(CURDIR)/scripts/check_spec_links.py --ignore_count=0 -Wall --include_warn
# -Werror disabled for now because there are ~14 -Wrefpage_missing messages.
# Would like those in the logs, but not in the count for build-breaking.

check-spec-links:
	$(QUIET)if [ -f $(CHECK_MARKUP_SCRIPT) ] && [ -f $(SPECSRC) ]; then $(CHECK_MARKUP_SCRIPT) -Werror; fi
	$(QUIET)if [ -f $(SPECSRC) ]; then $(PYTHON) $(CHECK_SPEC_LINKS_SCRIPT); fi
	$(QUIET)if [ -f checkXml.sh ] && [ -f registry/registry.rnc ]; then env FAIL_IF_COULD_NOT_VALIDATE=false ./checkXml.sh; fi
	$(QUIET)if [ -f checkSchematron.sh ] && [ -f registry/registry.sch ]; then ./checkSchematron.sh; fi
	$(QUIET)$(PYTHON) $(SCRIPTS)/xml_consistency.py

.PHONY: check-spec-links

################################################
# Reflow all specification source text

reflow:
	$(QUIET)$(PYTHON) $(CURDIR)/scripts/reflow.py -overwrite

.PHONY: reflow

################################################
# Meta "build approximately everything spec-specific required to pass CI" target

# recursive to cause csl to run first for better errors messages, without adding bogus depends.
all:
	$(QUIET)if [ -f $(SPECSRC) ]; then $(MAKE) check-spec-links; fi
	$(QUIET)$(MAKE) header
	$(QUIET)if [ -f $(TESTSRC) ]; then $(MAKE) header-test; fi
	$(QUIET)if [ -f $(SPECSRC) ]; then $(MAKE) html pdf build-examples; fi
	$(QUIET)if [ -f $(STYLESRC) ]; then $(MAKE) styleguide extprocess; fi
	$(QUIET)if [ -f $(LOADERSRC) ]; then $(MAKE) loader; fi
	$(ECHO) ""
	$(ECHO) "Target 'all': Completed specification build and basic checks successfully."
.PHONY: all


################################################
# Meta build for releases. Also copies into approximately the right layout for the OpenXR-Registry repo.
REGISTRYOUTDIR = $(OUTDIR)/../registry-release/specs/$(MAJORMINORVER)$(RELEASE_SPECS_SUFFIX)

$(REGISTRYOUTDIR):
	$(QUIET)$(MKDIR) $@

$(REGISTRYOUTDIR)/pdf $(REGISTRYOUTDIR)/html $(REGISTRYOUTDIR)/man: $(REGISTRYOUTDIR)
	$(QUIET)$(MKDIR) $@

release-htmlpdf: html pdf $(REGISTRYOUTDIR)/pdf $(REGISTRYOUTDIR)/html
	-asciidoctor-pdf-optimize $(PDFSPEC)
	$(QUIET)$(CP) $(PDFSPEC) $(REGISTRYOUTDIR)/pdf/xrspec.pdf
	$(QUIET)$(CP) $(HTMLSPEC) $(REGISTRYOUTDIR)/html/xrspec.html
.PHONY: release-htmlpdf
release: release-htmlpdf manhtmlpages loader styleguide extprocess $(REGISTRYOUTDIR) $(REGISTRYOUTDIR)/man
	$(QUIET)$(CP) $(OUTDIR)/styleguide.html $(OUTDIR)/extprocess.html $(OUTDIR)/loader.html $(REGISTRYOUTDIR)
	$(QUIET)$(CP) -R $(MANHTMLDIR) $(REGISTRYOUTDIR)/man/html
.PHONY: release

################################################
## Clean targets

# Files to clean up
PYDIRT = diag.txt dumpReg.txt errwarn.txt *.pyc regdump.txt
MANDIRT = $(MANGENERATED)
DIRT = $(PYDIRT) $(MANDIRT) ERRS \#*

# Clean intermediate files
clean_dirt:
	$(RM) $(DIRT)

# Clean generated targets.
clean_generated:
	$(RMRF) $(GENDIR) $(METADIR) man/apispec.txt

# Clean generated targets as well as intermediates.
clean clobber: clean_dirt clean_generated
	$(RM) $(HEADER) $(PLATHEAD) $(REFLECTHEAD) $(DEFINESHEAD) $(HTMLSPEC) $(PDFSPEC) $(PDFA4SPEC) $(STYLEGUIDE) $(LOADERGUIDE) $(DEPSCRIPTOUT) config_stamp
	# Clean up extracted code
	if [ -f $(EXAMPLES_MAKEFILE) ]; then $(MAKE) -f $(EXAMPLES_MAKEFILE) clean-examples; fi
	$(RM) $(EXAMPLES_MAKEFILE)
	# Clean up man pages
	$(RMRF) $(MANHTMLDIR)
