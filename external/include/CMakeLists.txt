# Copyright (c) 2017-2025 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(GL_HEADERS ${INCLUDE_DIR}/GL/gl_format.h)

# These can be dropped by some subsetted source distributions since
# they ought to exist, if required, in your OpenGL SDK package.
foreach(FN ${INCLUDE_DIR}/GL/glext.h ${INCLUDE_DIR}/GL/wglext.h)
    if(EXISTS "${FN}")
        list(APPEND GL_HEADERS "${FN}")
    endif()
endforeach()

source_group(GL FILES ${GL_HEADERS})
set_property(TARGET include PROPERTY FOLDER external/include)
