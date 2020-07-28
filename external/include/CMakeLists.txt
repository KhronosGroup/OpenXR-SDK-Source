# Copyright (c) 2017-2020 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

set( INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" )

set( GL_HEADERS
    ${INCLUDE_DIR}/GL/gl_format.h )

# These can be dropped by some subsetted source distributions since
# they ought to exist, if required, in your OpenGL SDK package.
foreach(FN ${INCLUDE_DIR}/GL/glext.h ${INCLUDE_DIR}/GL/wglext.h)
    if(EXISTS "${FN}")
        list(APPEND GL_HEADERS "${FN}")
    endif()
endforeach()

set( UTILS_HEADERS
    ${INCLUDE_DIR}/utils/algebra.h
    ${INCLUDE_DIR}/utils/sysinfo.h
    ${INCLUDE_DIR}/utils/nanoseconds.h
    ${INCLUDE_DIR}/utils/threading.h )
source_group( utils FILES ${UTILS_HEADERS} )
source_group( GL FILES ${GL_HEADERS} )
add_library( include INTERFACE ${UTILS_HEADERS} ${GL_HEADERS})
set_property( TARGET include PROPERTY FOLDER external/include )
