# OpenXR™ Software Development Kit (SDK) Sources Project

<!--
Copyright (c) 2017-2025 The Khronos Group Inc.

SPDX-License-Identifier: CC-BY-4.0
-->

This repository contains source code and build scripts for implementations
of the OpenXR loader, validation layers, and code samples.

The authoritative public repository is located at
<https://github.com/KhronosGroup/OpenXR-SDK-Source/>.
It hosts the public Issue tracker, and accepts patches (Pull Requests) from the
general public.

If you want to simply write an application using OpenXR (the headers and loader),
with minimum dependencies,
see <https://github.com/KhronosGroup/OpenXR-SDK/>.
That project is based on this one, but contains all generated files pre-generated,
removing the requirement for Python or build-time file generation,
and omits the samples, tests, and API layers, as they are not typically built as a part of an application.

## Directory Structure

<!-- REUSE-IgnoreStart -->
- `BUILDING.md` - Instructions for building the projects
- `README.md` - This file
- `COPYING.md` - Copyright and licensing information
- `CODE_OF_CONDUCT.md` - Code of Conduct
- `external/` - External code for projects in the repo
- `include/` - OpenXR platform include file
- `specification/` - xr.xml file
- `src/` - Source code for various projects
- `src/api_layer` - Sample code for developing API layers
- `src/loader` - OpenXR loader code
- `src/tests` - various test code (if looking for sample code start with `hello_xr/`)
<!-- REUSE-IgnoreEnd -->

Currently the best sample code is in [src/tests/hello_xr/](src/tests/hello_xr).  More will be added in the future.

## Building

See [BUILDING.md](BUILDING.md)

## Note about `git blame`

We are tracking "bulk commits" in the `.git-blame-ignore-revs` file, for better
git blame output. Sadly it appears that web interfaces do not yet handle this
file, but you can if using the command line. See
[--ignore-revs-file docs](https://git-scm.com/docs/git-blame#Documentation/git-blame.txt---ignore-revs-fileltfilegt)
for details, and
[this blog post about ignore-revs](https://www.moxio.com/blog/43/ignoring-bulk-change-commits-with-git-blame)
for some useful usage details.
