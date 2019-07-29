# OpenXR™ Software Development Kit (SDK) Sources Project

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

Currently the best sample code is in [src/tests/hello_xr/](https://github.com/KhronosGroup/OpenXR-SDK-Source/tree/master/src/tests/hello_xr).  More will be added in the future.

## Building

See [BUILDING.md](https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/master/BUILDING.md)
