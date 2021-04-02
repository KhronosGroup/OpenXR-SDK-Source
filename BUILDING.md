# How to Build

<!--
Copyright (c) 2014-2021, The Khronos Group Inc.

SPDX-License-Identifier: CC-BY-4.0
-->

## Python v3.6+ required

Certain source files are generated at build time from the `xr.xml` file, utilizing
python scripts. The scripts make use of the python `pathlib` module, which is
fully supported in python version 3.6 or later.

You will also need the python `jinja2` package, available from your package
manager or with something like `pip3 install jinja2`.

## Windows

Building the OpenXR components in this tree on Windows is supported using Visual
Studio 2013 and newer. If you are using Visual Studio 2019, you can simply "Open
folder" and use the built-in CMake support. Other environments, such as VS Code,
that can have CMake support installed and use the Visual Studio toolchains,
should also work: the project is a relatively standard CMake-based project. You
might consider
[the official CMake User Interaction Guide][cmake-user-interaction] if you're
new to building CMake-based projects.

[cmake-user-interaction]:https://cmake.org/cmake/help/latest/guide/user-interaction/

To easily build from the command line using the following steps, make sure the
appropriate `msbuild.exe` is in your PATH. Also, when generating the
solutions/projects using CMake, be sure to use the correct compiler version
number. The following table is provided to help you:

| Visual Studio        | Version Number |
| -------------------- |:--------------:|
| Visual Studio 2013   |       12       |
| Visual Studio 2015   |       14       |
| Visual Studio 2017   |       15       |
| Visual Studio 2019   |       16       |

### Windows 64-bit

First, generate the 64-bit solution and project files using CMake:

```
mkdir build\win64
cd build\win64
cmake -G "Visual Studio [Version Number] Win64" ../..
```

Finally, open the build\win64\OPENXR.sln in the Visual Studio to build the samples.

#### VS2019

For VS2019 the above may complain and say to split out the arch into `-A '[arch]'`.  This `-A` parameter must be set to `x64`, *not* `Win64`:

```cmd
mkdir build\win64
cd build\win64
cmake -G "Visual Studio [Version Number]" -A x64 ../..
```

VS2019 includes CMake tools, which can be installed through the Visual Studio
Installer -> Modify -> Individual Components -> C++ CMake tools for Windows. To
get the right paths for `msbuild.exe` and `cmake.exe`, you can launch through
`Start -> Visual Studio 2019 -> x64 Native Tools Command Prompt For VS 2019`.

### Windows 32-bit

First, generate the 32-bit solution and project files using CMake:

```
mkdir build\win32
cd build\win32
cmake -G "Visual Studio [Version Number]" ../..
```

Open the build\win32\OPENXR.sln in the Visual Studio to build the samples.

## Linux

The following set of packages provides all required libs for building for xlib or xcb with OpenGL and Vulkan support.
 - build-essential
 - cmake (of _somewhat_ recent vintage, 3.10+ known working)
 - libgl1-mesa-dev
 - libvulkan-dev
 - libx11-xcb-dev
 - libxcb-dri2-0-dev
 - libxcb-glx0-dev
 - libxcb-icccm4-dev
 - libxcb-keysyms1-dev
 - libxcb-randr0-dev
 - libxrandr-dev
 - libxxf86vm-dev
 - mesa-common-dev

### Linux Debug

```
mkdir -p build/linux_debug
cd build/linux_debug
cmake -DCMAKE_BUILD_TYPE=Debug ../..
make
```

### Linux Release

```
mkdir -p build/linux_release
cd build/linux_release
cmake -DCMAKE_BUILD_TYPE=Release ../..
make
```

## (Optional) Building the OpenXR Loader as a DLL

The OpenXR loader is built as a static library by default. To instead build as a dynamic link library, define
the cmake option `DYNAMIC_LOADER=ON`.  e.g. for Win64, replace the cmake line shown above with:

```
cmake -DDYNAMIC_LOADER=ON -G "Visual Studio [Version Number] Win64" ../..
```

# Running the HELLO_XR sample

## OpenXR runtime installation

An OpenXR _runtime_ must first be installed before running the hello_xr sample. The runtime is an
implementation of the OpenXR API, typically tailed to a specific device and distributed by the
device manufacturer. To allow experimentation with the API in the absence of a specific device runtime,
several organizations have made available prototype OpenXR runtimes, linked from the main OpenXR landing
page at https://www.khronos.org/openxr

## Configuring the OpenXR Loader
### XR\_RUNTIME\_JSON environment variable

The OpenXR loader looks in system-specific locations for the JSON file `active_runtime.json`, which describes the
default installed OpenXR runtime. To override the default selection, you may define an environment variable
`XR_RUNTIME_JSON` to select a different runtime, or a runtime which has not been installed in the default
location.

For example, you might set XR\_RUNTIME\_JSON to `<build_dir>/test/runtime/my_custom_runtime.json` to select
an OpenXR runtime described by JSON file `my_custom_runtime.json`.

## Running the Hello_XR Test

The binary for the hello_xr application is written to the `<build_dir>/src/tests/hello_xr` directory.
Set your working directory to this directory and execute the hello_xr binary.

### Hello_XR with a dynamic loader (Windows)

When building a DLL version of the loader, the Visual Studio projects generated by CMake will copy the loader
DLLs to the test application's (hello_xr) binary directory. This makes it unnecessary to copy these files to someplace like `Windows\System32`.

