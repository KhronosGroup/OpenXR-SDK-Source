# Toolchain file for 32-bit MinGW build on *nix
# Developed for use with Debian and its derivatives
#
# Copyright 2019 Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0

set(CMAKE_SYSTEM_NAME "Windows")

set(TARGET i686-w64-mingw32)
set(PREFIX ${TARGET}-)
set(SUFFIX -posix) # required for threads
set(CMAKE_C_COMPILER ${PREFIX}gcc${SUFFIX})
set(CMAKE_CXX_COMPILER ${PREFIX}g++${SUFFIX})
set(CMAKE_RC_COMPILER ${PREFIX}windres)

set(CMAKE_C_COMPILER_AR ${PREFIX}gcc-ar${SUFFIX})
set(CMAKE_CXX_COMPILER_AR ${PREFIX}gcc-ar${SUFFIX})
set(CMAKE_C_COMPILER_RANLIB ${PREFIX}gcc-ranlib${SUFFIX})
set(CMAKE_CXX_COMPILER_RANLIB ${PREFIX}gcc-ranlib${SUFFIX})
set(CMAKE_NM ${PREFIX}gcc-nm${SUFFIX})
set(CMAKE_OBJCOPY ${PREFIX}objcopy)
set(CMAKE_OBJDUMP ${PREFIX}objdump)
set(CMAKE_RANLIB ${PREFIX}ranlib)
set(CMAKE_STRIP ${PREFIX}strip)

if(NOT CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX /usr/${TARGET})
endif()

set(CMAKE_FIND_ROOT_PATH /usr/${TARGET})
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
