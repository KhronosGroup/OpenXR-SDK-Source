# Copyright (c) 2023-2025 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

add_library(openxr-glad-loader STATIC src/gl.c)
target_include_directories(openxr-glad-loader PUBLIC include)
target_link_libraries(openxr-glad-loader PRIVATE ${CMAKE_DL_LIBS})

# cause xr_dependencies.h to include glad versions of headers instead of standard ones
target_compile_definitions(openxr-glad-loader PUBLIC XRDEPENDENCIES_USE_GLAD)

if(WIN32)
    target_sources(
        openxr-glad-loader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/wgl.c
    )
else()
    target_sources(
        openxr-glad-loader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/egl.c
    )
endif()

if(WIN32 AND OPENGL_FOUND)
    if(TARGET OpenGL::OpenGL)
        target_link_libraries(openxr-glad-loader PUBLIC OpenGL::OpenGL)
    elseif(TARGET OpenGL::GL)
        target_link_libraries(openxr-glad-loader PUBLIC OpenGL::GL)
    else()
        target_link_libraries(openxr-glad-loader PUBLIC ${OPENGL_LIBRARIES})
    endif()
endif()

if(ANDROID)
    target_compile_definitions(openxr-glad-loader PUBLIC GLAD_GLES2)
endif()

if(NOT WIN32
   AND NOT ANDROID
   AND NOT APPLE
)
    target_sources(
        openxr-glad-loader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/glx.c
    )
endif()
