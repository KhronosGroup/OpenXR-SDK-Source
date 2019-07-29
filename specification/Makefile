# Copyright (c) 2013-2019 The Khronos Group Inc.
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

SHELL = /bin/bash

QUIET      ?= @
PYTHON     ?= python3
ASCIIDOC   ?= asciidoctor
RM          = rm -f
RMRF        = rm -rf
MKDIR       = mkdir -p
CP          = cp -p
MV          = mv
ECHO        = @echo
KATEX      := katex

# Use GENXR_OPTIONS to add arguments like -time to scripts/genxr.py invocations
GENXR_ARGS += $(GENXR_OPTIONS) -registry $(REGISTRY)

ifneq (,$(strip $(VERBOSE)))
ASCIIDOC := $(ASCIIDOC) --verbose
endif

ifneq (,$(strip $(STRICT)))
ASCIIDOC := $(ASCIIDOC) --failure-level ERROR
GENXR_ARGS += -validate
endif

ifneq (,$(strip $(VERY_STRICT)))
ASCIIDOC := $(ASCIIDOC) --failure-level WARN
endif

SPECREVISION = 1.0.0
REVISION_COMPONENTS = $(subst ., ,$(SPECREVISION))
MAJORMINORVER = $(word 1,$(REVISION_COMPONENTS)).$(word 2,$(REVISION_COMPONENTS))

# Target directory for output files.
OUTDIR	 ?= $(CURDIR)/out/$(MAJORMINORVER)

# Target directory for generated files.
# This is shorthand; can't be moved because the spec source
# files expect to find these here.
GENDIR    = $(CURDIR)/generated

# Generator scripts and options
# GENOPTS can be e.g. '-noprotect'
GENOPTS =

HEADER_DIR  = $(OUTDIR)/openxr
HEADER      = $(HEADER_DIR)/openxr.h
PLATHEAD    = $(HEADER_DIR)/openxr_platform.h
REFLECTHEAD = $(HEADER_DIR)/openxr_reflection.h
HTMLSPEC    = $(OUTDIR)/openxr.html
PDFSPEC     = $(OUTDIR)/openxr.pdf
REGISTRYOUTDIR = $(OUTDIR)/../registry-release/specs/$(MAJORMINORVER)
METADIR     = sources/chapters/extensions/meta
MANDIR     := man

default:
	$(ECHO) "Makefile help - targets:"
	$(ECHO) "header:           build OpenXR header"
	$(ECHO) "header-test:      test-compile the OpenXR header"
	$(ECHO) "html:             HTML version of OpenXR spec"
	$(ECHO) "pdf:              PDF version of OpenXR spec (Letter [8.5 x 11] paper size)"
	$(ECHO) "pdfA4:            PDF version of OpenXR spec (A4 paper size)"
#	$(ECHO) "manhtml:          HTML version of OpenXR man pages"
#	$(ECHO) "manpdf:           PDF version of OpenXR man pages"
	$(ECHO) "manhtmlpages:     HTML separate per-feature reference pages"
	$(ECHO) "styleguide:       HTML version of styleguide"
	$(ECHO) "loader:           HTML version of the loader doc"
	$(ECHO) "build-examples:   try compiling all examples"
	$(ECHO) "check-spec-links: run the checkMarkup and checkSpecLinks scripts"
	$(ECHO) "all:              build header header-test html pdf styleguide loader build-examples check-spec-links"
	$(ECHO) "release:          build header html pdf manhtmlpages and copy output to an OpenXR-Registry-like layout"
	$(ECHO) "clean_dirt:       remove intermediate files"
	$(ECHO) "clean:            remove targets and intermediate files"
	$(ECHO) ""
	$(ECHO) "Variables controlling options:"
	$(ECHO) "Pass QUIET= to disable quiet mode (echos all commands)"
	$(ECHO) "Pass VERBOSE=1 to invoke asciidoctor with the --verbose option"
	$(ECHO) "Pass STRICT=1 to invoke asciidoctor with the --failure-level ERROR option"
	$(ECHO) "Pass VERY_STRICT=1 to invoke asciidoctor with the --failure-level WARN option"

################################################
## OpenXR header file targets
# Autogenerate openxr header from XML API description
# Python and XML files on which openxr.h depends
header: $(HEADER)

# Directory to build headers into
$(OUTDIR)/openxr:
	$(QUIET)$(MKDIR) $@

REGISTRY    = ./registry/xr.xml
GENXR       = ./scripts/genxr.py
XRH_DEPENDS = $(REGISTRY) $(GENXR) ./scripts/reg.py ./scripts/generator.py $(OUTDIR)/openxr


# OpenXR header target
$(HEADER): $(XRH_DEPENDS) $(PLATHEAD)
	$(QUIET)$(PYTHON) $(GENXR) $(GENXR_ARGS) -o $(HEADER_DIR) openxr.h
	$(QUIET)$(PYTHON) $(GENXR) $(GENXR_ARGS) -o $(HEADER_DIR) openxr_platform.h
	$(QUIET)$(PYTHON) $(GENXR) $(GENXR_ARGS) -o $(HEADER_DIR) openxr_reflection.h

# Platform header (just copy)
$(PLATHEAD): $(OUTDIR)/openxr
	$(QUIET)$(CP) ../include/openxr/openxr_platform_defines.h $(HEADER_DIR)

# Test that openxr.h compiles
header-test: $(HEADER)
	$(CC) -Wall -pedantic -std=c99 -c -I$(OUTDIR) sources/test.c
	$(CXX) -Wall -c -std=c++98 -I$(OUTDIR) sources/test.c
	$(CXX) -Wall -c -std=c++11 -I$(OUTDIR) sources/test.c
	rm test.o

################################################
## Specification targets

html: $(HTMLSPEC)
pdf pdfLetter:
	$(MAKE) PAGESIZE=LETTER $(PDFSPEC)
pdfA4:
	$(MAKE) PAGESIZE=A4 $(PDFSPEC)
manhtml: $(HTMLMAN)
manpdf: $(PDFMAN)

SPECSRC = sources/openxr.adoc
SPECFILES = $(wildcard sources/chapters/[A-Za-z]*.adoc)

HOSTSYNCPATH   = $(GENDIR)/hostsynctable
HOSTSYNCDEPEND = $(HOSTSYNCPATH)/hostsyncinc
INDEXDEPEND = $(GENDIR)
INDEXNAME = index.adoc
GENDEPENDS = $(GENDIR)/api/apiinc $(GENDIR)/validity/validinc $(METADIR)/extinc $(INDEXDEPEND)/$(INDEXNAME) $(HOSTSYNCDEPEND)
GENINCLUDE = $(wildcard $(GENDIR)/api/*/[A-Za-z]*.txt $(GENDIR)/validity/*/[A-Za-z]*.txt $(HOSTSYNCPATH)/*/[A-Za-z]*.txt)
COMMONDOCS = $(SPECSRC) $(SPECFILES) $(GENINCLUDE) $(GENDEPENDS) $(OUTDIR)

.PHONY: generated
generated: $(COMMONDOCS)

EXTS := $(sort $(EXTENSIONS))
EXTATTRIBS := $(foreach ext,$(EXTS),-a $(ext))
EXTOPTIONS := $(foreach ext,$(EXTS),-extension $(ext))


# Generate Asciidoc attributes for spec revision remark.
APITITLE ?= OpenXR

# Spell out RFC2822 format as not all date commands support -R
SPECDATE     = $(shell echo `date -u "+%a, %d %b %Y %T %z"`)

# GITBRANCH should be set by the caller in CI environments
# because CI machines often checkout the code in detached HEAD state.
ifeq ($(GITBRANCH),)
# Evaluate only once to avoid slow calls to git.
GITBRANCH  := $(shell echo `git symbolic-ref --short HEAD`)
endif
GITREMARK  ?= from git branch: $(GITBRANCH)
SPECREMARK ?= $(GITREMARK) \
             commit: $(shell echo `git log -1 --format="%H"`)

ATTRIBOPTS   = -a revnumber="$(SPECREVISION)" \
               -a revdate="$(SPECDATE)" \
               -a revremark="$(SPECREMARK)" \
               -a apititle="$(APITITLE)" \
               -a stem=latexmath \
               -a pdf-page-size=$(PAGESIZE) \
               -a pdf-stylesdir=config \
               -a pdf-style=pdf \
               $(EXTATTRIBS)

ADOCOPTS = --doctype book -a data-uri -r $(CURDIR)/scripts/openxr-macros.rb $(ATTRIBOPTS)

$(HTMLSPEC): $(COMMONDOCS)
	$(QUIET)if [ $$(uname -s | cut -c 1-6) == "CYGWIN" ]; then \
	HTMLSPEC_DOS=$$(cygpath -w $@) ;\
	SPECSRC_DOS=$$(cygpath -w $(SPECSRC)) ;\
	ATTRIBOPTS_DOS='$(ATTRIBOPTS)' ;\
	ADOCOPTS_DOS="--doctype book -a data-uri -r $$(cygpath -w $(CURDIR)/scripts/openxr-macros.rb) $$ATTRIBOPTS_DOS" ;\
	HTML_BATCH_FILE=$$(cygpath -w $$(mktemp)).bat ;\
	echo $(ASCIIDOC) $$ADOCOPTS_DOS --backend html5 --out-file $$HTMLSPEC_DOS $$SPECSRC_DOS > $$HTML_BATCH_FILE ;\
	CMD /C $$HTML_BATCH_FILE ;\
	rm -f $$HTML_BATCH_FILE ;\
	else \
	$(ASCIIDOC) $(ADOCOPTS) --backend html5 --out-file $@ $(SPECSRC) 2>&1 | tee $(OUTDIR)/adoc_html_stderr.txt ;\
	if [ -s $(OUTDIR)/adoc_html_stderr.txt ]; then \
		false; \
	else \
		rm $(OUTDIR)/adoc_html_stderr.txt; \
	fi; \
	fi

$(PDFSPEC): $(COMMONDOCS)
	$(QUIET)if [ $$(uname -s | cut -c 1-6) == "CYGWIN" ]; then \
	PDFSPEC_DOS=$$(cygpath -w $@) ;\
	SPECSRC_DOS=$$(cygpath -w $(SPECSRC)) ;\
	ATTRIBOPTS_DOS='$(ATTRIBOPTS)' ;\
	ADOCOPTS_DOS="--doctype book -a data-uri -r $$(cygpath -w $(CURDIR)/scripts/openxr-macros.rb) $$ATTRIBOPTS_DOS" ;\
	PDF_BATCH_FILE=$$(cygpath -w $$(mktemp)).bat ;\
	echo $(ASCIIDOC) $$ADOCOPTS_DOS --backend pdf --require asciidoctor-pdf --out-file $$PDFSPEC_DOS $$SPECSRC_DOS > $$PDF_BATCH_FILE ;\
	CMD /C $$PDF_BATCH_FILE ;\
	rm -f $$PDF_BATCH_FILE ;\
	else \
	$(ASCIIDOC) $(ADOCOPTS) --backend pdf --require asciidoctor-pdf --out-file $@ $(SPECSRC) 2>&1 | tee $(OUTDIR)/adoc_pdf_stderr.txt ;\
	if [ -s $(OUTDIR)/adoc_pdf_stderr.txt ]; then \
		false; \
	else \
		rm $(OUTDIR)/adoc_pdf_stderr.txt; \
	fi; \
	fi


################################################
# Reference "man" pages extracted from spec

MANHTMLDIR   = $(OUTDIR)/man/html
KHRSOURCES   = $(wildcard $(MANDIR)/*KHR.txt)
MACROSOURCES = $(wildcard $(MANDIR)/XR_*[A-Z][A-Z].txt)
VENSOURCES   = $(filter-out $(KHRSOURCES) $(MACROSOURCES),$(wildcard $(MANDIR)/*[A-Z][A-Z].txt))
CORESOURCES  = $(filter-out $(KHRSOURCES) $(VENSOURCES),$(wildcard $(MANDIR)/[Xx][Rr]*.txt $(MANDIR)/PFN*.txt))
MANSOURCES   = $(CORESOURCES) $(VENSOURCES) $(KHRSOURCES)
MANCOPYRIGHT = $(MANDIR)/copyright-ccby.txt $(MANDIR)/footer.txt
MANGENERATED = $(filter-out $(MANCOPYRIGHT),$(wildcard $(MANDIR)/*))
MANHTML     = $(MANSOURCES:$(MANDIR)/%.txt=$(MANHTMLDIR)/%.html)
MANDEPS     = $(MANCOPYRIGHT) $(GENINCLUDE) $(GENDEPENDS)
MANATTRIBOPTS = -a stylesheet=khronos.css \
				-a stylesdir=$(CURDIR)/config \
				-a html_spec_relative='../../openxr.html' \

# Pure makefile lowercase function, generated by a script.
make_lower = $(subst A,a,$(subst B,b,$(subst C,c,$(subst D,d,$(subst E,e,$(subst F,f,$(subst G,g,$(subst H,h,$(subst I,i,$(subst J,j,$(subst K,k,$(subst L,l,$(subst M,m,$(subst N,n,$(subst O,o,$(subst P,p,$(subst Q,q,$(subst R,r,$(subst S,s,$(subst T,t,$(subst U,u,$(subst V,v,$(subst W,w,$(subst X,x,$(subst Y,y,$(subst Z,z,$(1)))))))))))))))))))))))))))
EXTENSIONS_LOWER := $(call make_lower,$(EXTENSIONS))

# Function that turns khr_vulkan_enable into sources/chapters/extensions/khr/khr_vulkan_enable.adoc
make_extension_source = sources/chapters/extensions/$(word 1,$(subst _, ,$(1)))/$(1).adoc

# Call make_extension_source on every enabled extension, after lowercasing and stripping the leading XR prefix.
EXTENSION_SOURCES := $(foreach ext,$(patsubst xr_%,%,$(EXTENSIONS_LOWER)),$(call make_extension_source,$(ext)))

# This is a generated python script used by genRef
# in the process of extracting man page sources.
# So, it must be generated before we can run genRef
scripts/xrapi.py: $(XRH_DEPENDS)
	$(QUIET)$(PYTHON) $(GENXR) $(GENXR_ARGS) $(EXTOPTIONS) -o scripts xrapi.py

# Generation of ref page asciidoctor sources by extraction from the
# specification.
#
# Should have a proper dependency causing the man page sources to be
# generated by running genRef (once), but adding $(MANSOURCES) to the
# targets causes genRef to run once/target.
#
# So, we treat the apispec.txt file as the "stamp" for genRef.
#
# Should pass in $(EXTOPTIONS) to determine which pages to generate,
# which requires a matching update to genRef.
# For now, all core and extension ref pages are extracted by genRef.py.
LOGFILE = man/logfile
man/apispec.txt: $(SPECFILES) $(EXTENSION_SOURCES) ./scripts/genRef.py ./scripts/reflib.py ./scripts/xrapi.py
	$(QUIET)$(PYTHON) ./scripts/genRef.py -log $(LOGFILE) -registry $(REGISTRY) $(EXTOPTIONS) $(SPECFILES) $(EXTENSION_SOURCES)
	$(QUIET)grep "ERROR:" $(LOGFILE)

# The recursive $(MAKE) is an apparently unavoidable hack, since the
# actual list of man page sources isn't known until after
# man/apispec.txt is generated.
manhtmlpages: man/apispec.txt
	$(MAKE) buildmanpages

buildmanpages: $(MANHTML) $(MANHTMLDIR)/openxr.html

# This is the single-page ref page.
# Manually defining doctype-manpage so that we can use "book" style but still enable the refpage-only portions.
$(MANHTMLDIR)/openxr.html: man/apispec.txt $(MANDEPS)
	$(QUIET)$(ASCIIDOC) -b html5 $(MANATTRIBOPTS) $(ADOCOPTS) -a doctype-manpage -d book -o $@ $<

# This is all the individual-page ref pages.
$(MANHTML): $(MANHTMLDIR)/%.html: $(MANDIR)/%.txt $(MANDEPS)
	$(QUIET)$(MKDIR) $(MANHTMLDIR)
	$(QUIET)$(ASCIIDOC) -b html5 $(MANATTRIBOPTS) $(ADOCOPTS) -a cross-file-links -d manpage -o $@ $<


#manpdf: $(OUTDIR)/apispec.pdf
#$(OUTDIR)/apispec.pdf: $(SPECVERSION) man/apispec.txt $(MANCOPYRIGHT) $(SVGFILES) $(GENINCLUDE) $(GENDEPENDS)
#        $(QUIET)$(MKDIR) $(OUTDIR)
#        $(QUIET)$(MKDIR) $(PDFMATHDIR)
#        $(QUIET)$(ASCIIDOC) -b pdf -a html_spec_relative='html/vkspec.html' $(ADOCOPTS) $(ADOCPDFOPTS) -o $@ man/apispec.txt
#ifndef GS_EXISTS
#        $(QUIET) echo "Warning: Ghostscript not installed, skipping pdf optimization"
#else
#        $(QUIET)$(CURDIR)/config/optimize-pdf $@
#        $(QUIET)rm $@
#        $(QUIET)mv $(OUTDIR)/apispec-optimized.pdf $@
#endif

#manhtml: $(OUTDIR)/apispec.html
#$(OUTDIR)/apispec.html: KATEXDIR = katex
#$(OUTDIR)/apispec.html: $(SPECVERSION) man/apispec.txt $(MANCOPYRIGHT) $(SVGFILES) $(GENINCLUDE) $(GENDEPENDS) katexinst
#        $(QUIET)$(MKDIR) $(OUTDIR)
#        $(QUIET)$(ASCIIDOC) -b html5 -a html_spec_relative='html/vkspec.html' $(ADOCOPTS) $(ADOCHTMLOPTS) -o $@ man/apispec.txt

################################################
# Generated include files referenced in spec

# The nominal targets here are just "stamp" files generated
# at the same time as the real outputs of these commands.

# Generated files for spec
$(GENDIR)/api/apiinc: $(XRH_DEPENDS)
	$(QUIET)$(MKDIR) $(@D)
	$(QUIET)$(PYTHON) $(GENXR) $(GENXR_ARGS) $(EXTOPTIONS) -o $(GENDIR)/api apiinc

$(GENDIR)/validity/validinc: $(XRH_DEPENDS)
	$(QUIET)$(MKDIR) $(@D)
	$(QUIET)$(PYTHON) $(GENXR) $(GENXR_ARGS) $(EXTOPTIONS) -o $(GENDIR)/validity validinc

$(HOSTSYNCDEPEND): $(XRH_DEPENDS)
	$(QUIET)$(MKDIR) $(@D)
	$(QUIET)$(PYTHON) $(GENXR) $(GENXR_ARGS) $(EXTOPTIONS) -o $(HOSTSYNCPATH) hostsyncinc

$(INDEXDEPEND)/$(INDEXNAME): $(XRH_DEPENDS)
	$(QUIET)$(MKDIR) $(@D)
	$(QUIET)$(PYTHON) $(GENXR) $(GENXR_ARGS) $(EXTOPTIONS) -o $(INDEXDEPEND) $(INDEXNAME)

XRM_DEPENDS = $(REGISTRY) $(GENXR) ./scripts/reg.py ./scripts/generator.py

$(METADIR)/extinc: $(XRM_DEPENDS)
	$(QUIET)$(MKDIR) $(@D)
	$(QUIET)$(PYTHON) $(GENXR) $(GENXR_ARGS) $(EXTOPTIONS) -o $(METADIR) extinc

# Every generated include snippet depends on registry and scripts.
$(GENINCLUDE): $(XRH_DEPENDS)

DEPSCRIPT = ./scripts/make_ext_dependency.py
generated/extDependency.sh: $(REGISTRY) $(DEPSCRIPT)
	$(QUIET)$(PYTHON) $(DEPSCRIPT) -registry $(REGISTRY) \
	    -outscript $@

################################################
# OpenXR Style Guide

STYLEGUIDE = $(OUTDIR)/styleguide.html
STYLESRC = sources/styleguide/styleguide.txt
STYLEFILES = $(wildcard sources/styleguide/[A-Za-z]*.txt)

styleguide: $(STYLEGUIDE)

$(STYLEGUIDE): $(STYLESRC) $(STYLEFILES) $(GENDIR)/api/apiinc
	$(QUIET)$(MKDIR) $(OUTDIR)
	$(QUIET)if [ $$(uname -s | cut -c 1-6) == "CYGWIN" ]; then \
	STYLESPEC_DOS=$$(cygpath -w $@) ;\
	STYLESRC_DOS=$$(cygpath -w $(SPECSRC)) ;\
	ATTRIBOPTS_DOS='$(ATTRIBOPTS)' ;\
	ADOCOPTS_DOS="--doctype book -a data-uri -r $$(cygpath -w $(CURDIR)/scripts/openxr-macros.rb) $$ATTRIBOPTS_DOS" ;\
	STYLE_BATCH_FILE=$$(cygpath -w $$(mktemp)).bat ;\
	echo $(ASCIIDOC) -b html5 $$ADOCOPTS_DOS -o $$STYLESPEC_DOS $$STYLESRC_DOS > $$STYLE_BATCH_FILE ;\
	CMD /C $$STYLE_BATCH_FILE ;\
	rm -f $$STYLE_BATCH_FILE ;\
	else \
	$(ASCIIDOC) -b html5 $(ADOCOPTS) -o $@ $(STYLESRC) ;\
	fi

################################################
# OpenXR Loader Guide

LOADERGUIDE = $(OUTDIR)/loader.html
LOADERSRC = loader/loader.adoc
LOADERFILES = $(wildcard loader/[A-Za-z]*.adoc)

loader: $(LOADERGUIDE)

$(LOADERGUIDE): $(LOADERSRC) $(LOADERFILES)
	$(QUIET)$(MKDIR) $(OUTDIR)
	$(QUIET)if [ $$(uname -s | cut -c 1-6) == "CYGWIN" ]; then \
	SPEC_DOS=$$(cygpath -w $@) ;\
	SRC_DOS=$$(cygpath -w $(LOADERSRC)) ;\
	ATTRIBOPTS_DOS='$(ATTRIBOPTS)' ;\
	ADOCOPTS_DOS="--doctype book -a data-uri -r $$(cygpath -w $(CURDIR)/scripts/openxr-macros.rb) $$ATTRIBOPTS_DOS" ;\
	BATCH_FILE=$$(cygpath -w $$(mktemp)).bat ;\
	echo $(ASCIIDOC) -b html5 $$ADOCOPTS_DOS -o $$SPEC_DOS $$SRC_DOS > $$BATCH_FILE ;\
	CMD /C $$BATCH_FILE ;\
	rm -f $$BATCH_FILE ;\
	else \
	$(ASCIIDOC) -b html5 $(ADOCOPTS) -o $@ $(LOADERSRC) ;\
	fi

################################################
# Embedded example code

# Script that extracts code to .cpp and .c files, and
# generates an associated makefile
EXAMPLES_SCRIPT = $(CURDIR)/scripts/extract_code.py

# The makefile generated by extract_code.py
EXAMPLES_MAKEFILE = $(CURDIR)/examples.mk

# Only include the generated makefile if we are building neither clean nor default
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),)
-include $(EXAMPLES_MAKEFILE)
endif
endif

ifeq (,$(GENERATED_CPP))
# If we didn't include the generated makefile (on purpose or because it hasn't been generated),
# just add dummy rules to generate it.

ifeq ($(strip $(QUIET)),@)
EXTRACT_QUIET := --quiet
endif

$(EXAMPLES_MAKEFILE): $(EXAMPLES_SCRIPT)
	$(QUIET)$(PYTHON) $< --makefile=$@ --line_numbers $(EXTRACT_QUIET)

build-examples: $(EXAMPLES_MAKEFILE)
.PHONY: build-examples

endif

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
	$(QUIET)$(CHECK_MARKUP_SCRIPT) -Werror
	$(QUIET)$(PYTHON) $(CHECK_SPEC_LINKS_SCRIPT)
	$(QUIET)env FAIL_IF_COULD_NOT_VALIDATE=false ./checkXml.sh
	$(QUIET)$(PYTHON) $(CURDIR)/scripts/xml_consistency.py

.PHONY: check-spec-links

################################################
# Meta "build approximately everything spec-specific required to pass CI" target
all:
	$(QUIET)$(MAKE) check-spec-links # recursive to cause csl to run first for better errors messages, without adding bogus depends.
	$(QUIET)$(MAKE) header header-test html pdf styleguide loader build-examples
	$(ECHO) ""
	$(ECHO) "Target 'all': Completed specification build and basic checks successfully."
.PHONY: all

################################################
# Meta build for releases. Also copies into approximately the right layout for the OpenXR-Registry repo.
release: header html pdf manhtmlpages loader
	$(QUIET)$(MKDIR) $(REGISTRYOUTDIR)/pdf
	$(QUIET)$(CP) $(PDFSPEC) $(REGISTRYOUTDIR)/pdf/xrspec.pdf
	$(QUIET)$(MKDIR) $(REGISTRYOUTDIR)/html
	$(QUIET)$(CP) $(HTMLSPEC) $(REGISTRYOUTDIR)/html/xrspec.html
	$(QUIET)$(MKDIR) $(REGISTRYOUTDIR)/man
	$(QUIET)$(CP) -R $(MANHTMLDIR) $(REGISTRYOUTDIR)/man/html

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
	$(RM) $(HEADER) $(PLATHEAD) $(HTMLSPEC) $(PDFSPEC) $(STYLEGUIDE) $(LOADERGUIDE)
	# Clean up extracted code
	$(RM) $(EXAMPLES_MAKEFILE) $(GENERATED_C) $(C_OBJECTS) $(GENERATED_CPP) $(CPP_OBJECTS)
	# Clean up man pages
	$(RMRF) $(MANHTMLDIR)
	find $(CURDIR)/sources/chapters/ -name "*.cpp" -o -name "*.c" -o -name "*.o" | xargs $(RM)
