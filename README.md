# OpenXR™ Software Development Kit (SDK) Sources Project

<!--
Copyright (c) 2017-2022, The Khronos Group Inc.

SPDX-License-Identifier: CC-BY-4.0
-->

BRANCH: QUEST CLIENT USING OCULUS INSTRUCTIONS FROM HERE:

https://developer.oculus.com/documentation/native/android/mobile-build-run-hello-xr-app/

QUEST PRO EYE TRACKING sample on PC VR via Link / AirLink (enable Dev mode + eye-tracking sliders in Oculus PC app):

https://twitter.com/BattleAxeVR/status/1588640727197954048/video/1

![image](https://user-images.githubusercontent.com/11604039/200199048-adeef6c8-d349-436f-80ff-d237c35244c4.png)


NB This same exact code should work on Quest Pro standalone mode, but I didn't add the request permissions Java code for eye-tracking (which needs to be accepted for it to work). There is no java code in this project so it will probably have to be done via JNI / Android Native somehow. I'll try to add that soon.

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
