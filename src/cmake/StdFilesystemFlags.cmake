# Copyright 2020, Collabora, Ltd.
#
# SPDX-License-Identifier: BSL-1.0

set(_FILESYSTEM_UTILS_DIR "${PROJECT_SOURCE_DIR}/src/common")

if(MSVC AND MSVC_VERSION GREATER 1890)
    set(HAVE_FILESYSTEM_WITHOUT_LIB
        ON
        CACHE INTERNAL "" FORCE
    )
    if(MSVC_VERSION GREATER 1910)
        # Visual Studio 2017 Update 3 added new filesystem impl,
        # which only works in C++17 mode.
        set(HAVE_FILESYSTEM_NEEDS_17
            ON
            CACHE INTERNAL "" FORCE
        )
    endif()
else()
    include(CheckCXXSourceCompiles)

    # This is just example code that is known to not compile if std::filesystem isn't working right
    set(_stdfs_test_source
        "int main() {
        (void)is_regular_file(\"/\");
        return 0;
    }
    "
    )

    set(_stdfs_conditions "#include <stdfs_conditions.h>
    ")

    set(_stdfs_source
        "${_stdfs_conditions}
    #if defined(USE_FINAL_FS) && USE_FINAL_FS
    #include <filesystem>
    using namespace std::filesystem;
    #endif
    ${_stdfs_test_source}
    "
    )
    set(_stdfs_experimental_source
        "${_stdfs_conditions}
    #if defined(USE_EXPERIMENTAL_FS) && USE_EXPERIMENTAL_FS
    #include <experimental/filesystem>
    using namespace std::experimental::filesystem;
    #endif
    ${_stdfs_test_source}
    "
    )
    set(_stdfs_needlib_source
        "${_stdfs_conditions}
    #if defined(USE_FINAL_FS) && USE_FINAL_FS
    #include <filesystem>
    using namespace std::filesystem;
    #endif
    #if defined(USE_EXPERIMENTAL_FS) && USE_EXPERIMENTAL_FS
    #include <experimental/filesystem>
    using namespace std::experimental::filesystem;
    #endif
    ${_stdfs_test_source}
    "
    )

    # First, just look for the include.
    # We're checking if it compiles, not if the include exists,
    # because the source code uses the same conditionals to decide.
    # (Static libraries are just object files, they don't get linked)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    set(CMAKE_REQUIRED_INCLUDES "${_FILESYSTEM_UTILS_DIR}")
    unset(CMAKE_REQUIRED_LIBRARIES)
    unset(CMAKE_REQUIRED_FLAGS)
    check_cxx_source_compiles("${_stdfs_source}" HAVE_FILESYSTEM_IN_STD)
    check_cxx_source_compiles("${_stdfs_experimental_source}" HAVE_FILESYSTEM_IN_STDEXPERIMENTAL)

    set(CMAKE_REQUIRED_FLAGS "-DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=TRUE")
    check_cxx_source_compiles("${_stdfs_source}" HAVE_FILESYSTEM_IN_STD_17)
    unset(CMAKE_REQUIRED_FLAGS)

    if(HAVE_FILESYSTEM_IN_STD_17 AND NOT HAVE_FILESYSTEM_IN_STD)
        set(HAVE_FILESYSTEM_NEEDS_17
            ON
            CACHE INTERNAL ""
        )
        set(CMAKE_REQUIRED_FLAGS "-DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=TRUE")
    else()
        set(HAVE_FILESYSTEM_NEEDS_17
            OFF
            CACHE INTERNAL ""
        )
    endif()

    # Now, see if we need libstdc++fs
    set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
    check_cxx_source_compiles("${_stdfs_needlib_source}" HAVE_FILESYSTEM_WITHOUT_LIB)
    set(CMAKE_REQUIRED_LIBRARIES stdc++fs)
    check_cxx_source_compiles("${_stdfs_needlib_source}" HAVE_FILESYSTEM_NEEDING_LIBSTDCXXFS)
    set(CMAKE_REQUIRED_LIBRARIES c++fs)
    check_cxx_source_compiles("${_stdfs_needlib_source}" HAVE_FILESYSTEM_NEEDING_LIBCXXFS)
    unset(CMAKE_REQUIRED_LIBRARIES)
    unset(CMAKE_TRY_COMPILE_TARGET_TYPE)
    unset(CMAKE_REQUIRED_INCLUDES)
endif()

function(openxr_add_filesystem_utils TARGET_NAME)
    target_sources(${TARGET_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src/common/filesystem_utils.cpp)
    target_include_directories(${TARGET_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src/common)
    if(NOT BUILD_WITH_STD_FILESYSTEM)
        target_compile_definitions(${TARGET_NAME} PRIVATE DISABLE_STD_FILESYSTEM)
    else()
        if(HAVE_FILESYSTEM_NEEDS_17)
            set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 17)
            set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD_REQUIRED TRUE)
        endif()
        if(NOT HAVE_FILESYSTEM_WITHOUT_LIB)
            if(HAVE_FILESYSTEM_NEEDING_LIBSTDCXXFS)
                target_link_libraries(${TARGET_NAME} PRIVATE stdc++fs)
            elseif(HAVE_FILESYSTEM_NEEDING_LIBCXXFS)
                target_link_libraries(${TARGET_NAME} PRIVATE c++fs)
            endif()
        endif()
    endif()
endfunction()
