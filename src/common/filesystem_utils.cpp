// Copyright (c) 2017 The Khronos Group Inc.
// Copyright (c) 2017 Valve Corporation
// Copyright (c) 2017 LunarG, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Authors: Mark Young <marky@lunarg.com>
//          Nat Brown <natb@valvesoftware.com>
//

#include "filesystem_utils.hpp"

#include "platform_utils.hpp"

#include <cstring>

#if defined DISABLE_STD_FILESYSTEM
#define USE_EXPERIMENTAL_FS 0
#define USE_FINAL_FS 0

#else
// If the C++ macro is set to the version containing C++17, it must support
// the final C++17 package
#if __cplusplus >= 201703L
#define USE_EXPERIMENTAL_FS 0
#define USE_FINAL_FS 1

#elif defined(_MSC_VER) && _MSC_VER >= 1900

#if defined(_HAS_CXX17) && _HAS_CXX17
// When MSC supports c++17 use <filesystem> package.
#define USE_EXPERIMENTAL_FS 0
#define USE_FINAL_FS 1
#endif  // !_HAS_CXX17

// Right now, GCC still only supports the experimental filesystem items starting in GCC 6
#elif (__GNUC__ >= 6)
#define USE_EXPERIMENTAL_FS 1
#define USE_FINAL_FS 0

// If Clang, check for feature support
#elif defined(__clang__) && (__cpp_lib_filesystem || __cpp_lib_experimental_filesystem)
#if __cpp_lib_filesystem
#define USE_EXPERIMENTAL_FS 0
#define USE_FINAL_FS 1
#else
#define USE_EXPERIMENTAL_FS 1
#define USE_FINAL_FS 0
#endif

// If all above fails, fall back to standard C++ and OS-specific items
#else
#define USE_EXPERIMENTAL_FS 0
#define USE_FINAL_FS 0
#endif
#endif

#if USE_FINAL_FS == 1
#include <filesystem>
#define FS_PREFIX std::filesystem
#elif USE_EXPERIMENTAL_FS == 1
#include <experimental/filesystem>
#define FS_PREFIX std::experimental::filesystem
#elif defined(XR_USE_PLATFORM_WIN32)
// Windows fallback includes
#include <stdint.h>
#include <direct.h>
#else
// Linux/Apple fallback includes
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <dirent.h>
#endif

#if defined(XR_USE_PLATFORM_WIN32)
#define PATH_SEPARATOR ';'
#define DIRECTORY_SYMBOL '\\'
#else
#define PATH_SEPARATOR ':'
#define DIRECTORY_SYMBOL '/'
#endif

#if (USE_FINAL_FS == 1) || (USE_EXPERIMENTAL_FS == 1)
// We can use one of the C++ filesystem packages

bool FileSysUtilsIsRegularFile(const std::string& path) { return FS_PREFIX::is_regular_file(path); }

bool FileSysUtilsIsDirectory(const std::string& path) { return FS_PREFIX::is_directory(path); }

bool FileSysUtilsPathExists(const std::string& path) { return FS_PREFIX::exists(path); }

bool FileSysUtilsIsAbsolutePath(const std::string& path) {
    FS_PREFIX::path file_path(path);
    return file_path.is_absolute();
}

bool FileSysUtilsGetCurrentPath(std::string& path) {
    FS_PREFIX::path cur_path = FS_PREFIX::current_path();
    path = cur_path.string();
    return true;
}

bool FileSysUtilsGetParentPath(const std::string& file_path, std::string& parent_path) {
    FS_PREFIX::path path_var(file_path);
    parent_path = path_var.parent_path().string();
    return true;
}

bool FileSysUtilsGetAbsolutePath(const std::string& path, std::string& absolute) {
    absolute = FS_PREFIX::absolute(path).string();
    return true;
}

bool FileSysUtilsCombinePaths(const std::string& parent, const std::string& child, std::string& combined) {
    FS_PREFIX::path parent_path(parent);
    FS_PREFIX::path child_path(child);
    FS_PREFIX::path full_path = parent_path / child_path;
    combined = full_path.string();
    return true;
}

bool FileSysUtilsParsePathList(std::string& path_list, std::vector<std::string>& paths) {
    std::string::size_type start = 0;
    std::string::size_type location = path_list.find(PATH_SEPARATOR);
    while (location != std::string::npos) {
        paths.push_back(path_list.substr(start, location));
        start = location + 1;
        location = path_list.find(PATH_SEPARATOR, start);
    }
    paths.push_back(path_list.substr(start, location));
    return true;
}

bool FileSysUtilsFindFilesInPath(const std::string& path, std::vector<std::string>& files) {
    for (auto& dir_iter : FS_PREFIX::directory_iterator(path)) {
        files.push_back(dir_iter.path().filename().string());
    }
    return true;
}

#elif defined(XR_OS_WINDOWS)

// For pre C++17 compiler that doesn't support experimental filesystem
#include <shlwapi.h>

bool FileSysUtilsIsRegularFile(const std::string& path) { return (1 != PathIsDirectoryW(utf8_to_wide(path).c_str())); }

bool FileSysUtilsIsDirectory(const std::string& path) { return (1 == PathIsDirectoryW(utf8_to_wide(path).c_str())); }

bool FileSysUtilsPathExists(const std::string& path) { return (1 == PathFileExistsW(utf8_to_wide(path).c_str())); }

bool FileSysUtilsIsAbsolutePath(const std::string& path) {
    if ((path[0] == '\\') || (path[1] == ':' && (path[2] == '\\' || path[2] == '/'))) {
        return true;
    }
    return false;
}

bool FileSysUtilsGetCurrentPath(std::string& path) {
    wchar_t tmp_path[MAX_PATH];
    if (nullptr != _wgetcwd(tmp_path, MAX_PATH - 1)) {
        path = wide_to_utf8(tmp_path);
        return true;
    }
    return false;
}

bool FileSysUtilsGetParentPath(const std::string& file_path, std::string& parent_path) {
    std::string full_path;
    if (FileSysUtilsGetAbsolutePath(file_path, full_path)) {
        std::string::size_type lastSeperator = full_path.find_last_of(DIRECTORY_SYMBOL);
        parent_path = (lastSeperator == 0) ? full_path : full_path.substr(0, lastSeperator);
        return true;
    }
    return false;
}

bool FileSysUtilsGetAbsolutePath(const std::string& path, std::string& absolute) {
    wchar_t tmp_path[MAX_PATH];
    if (0 != GetFullPathNameW(utf8_to_wide(path).c_str(), MAX_PATH, tmp_path, NULL)) {
        absolute = wide_to_utf8(tmp_path);
        return true;
    }
    return false;
}

bool FileSysUtilsCombinePaths(const std::string& parent, const std::string& child, std::string& combined) {
    std::string::size_type parent_len = parent.length();
    if (0 == parent_len || "." == parent || ".\\" == parent || "./" == parent) {
        combined = child;
        return true;
    }
    char last_char = parent[parent_len - 1];
    if (last_char == DIRECTORY_SYMBOL) {
        parent_len--;
    }
    combined = parent.substr(0, parent_len) + DIRECTORY_SYMBOL + child;
    return true;
}

bool FileSysUtilsParsePathList(std::string& path_list, std::vector<std::string>& paths) {
    std::string::size_type start = 0;
    std::string::size_type location = path_list.find(PATH_SEPARATOR);
    while (location != std::string::npos) {
        paths.push_back(path_list.substr(start, location));
        start = location + 1;
        location = path_list.find(PATH_SEPARATOR, start);
    }
    paths.push_back(path_list.substr(start, location));
    return true;
}

bool FileSysUtilsFindFilesInPath(const std::string& path, std::vector<std::string>& files) {
    WIN32_FIND_DATAW file_data;
    HANDLE file_handle = FindFirstFileW(utf8_to_wide(path).c_str(), &file_data);
    if (file_handle != INVALID_HANDLE_VALUE) {
        do {
            if (!(file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                files.push_back(wide_to_utf8(file_data.cFileName));
            }
        } while (FindNextFileW(file_handle, &file_data));
        return true;
    }
    return false;
}

#else  // XR_OS_LINUX/XR_OS_APPLE fallback

// simple POSIX-compatible implementation of the <filesystem> pieces used by OpenXR

bool FileSysUtilsIsRegularFile(const std::string& path) {
    struct stat path_stat;
    stat(path.c_str(), &path_stat);
    return S_ISREG(path_stat.st_mode);
}

bool FileSysUtilsIsDirectory(const std::string& path) {
    struct stat path_stat;
    stat(path.c_str(), &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

bool FileSysUtilsPathExists(const std::string& path) { return (access(path.c_str(), F_OK) != -1); }

bool FileSysUtilsIsAbsolutePath(const std::string& path) { return (path[0] == DIRECTORY_SYMBOL); }

bool FileSysUtilsGetCurrentPath(std::string& path) {
    char tmp_path[PATH_MAX];
    if (nullptr != getcwd(tmp_path, PATH_MAX - 1)) {
        path = tmp_path;
        return true;
    }
    return false;
}

bool FileSysUtilsGetParentPath(const std::string& file_path, std::string& parent_path) {
    std::string full_path;
    if (FileSysUtilsGetAbsolutePath(file_path, full_path)) {
        std::string::size_type lastSeperator = full_path.find_last_of(DIRECTORY_SYMBOL);
        parent_path = (lastSeperator == 0) ? full_path : full_path.substr(0, lastSeperator);
        return true;
    }
    return false;
}

bool FileSysUtilsGetAbsolutePath(const std::string& path, std::string& absolute) {
    char buf[PATH_MAX];
    if (nullptr != realpath(path.c_str(), buf)) {
        absolute = buf;
        return true;
    }
    return false;
}

bool FileSysUtilsCombinePaths(const std::string& parent, const std::string& child, std::string& combined) {
    std::string::size_type parent_len = parent.length();
    if (0 == parent_len || "." == parent || "./" == parent) {
        combined = child;
        return true;
    }
    char last_char = parent[parent_len - 1];
    if (last_char == DIRECTORY_SYMBOL) {
        parent_len--;
    }
    combined = parent.substr(0, parent_len) + DIRECTORY_SYMBOL + child;
    return true;
}

bool FileSysUtilsParsePathList(std::string& path_list, std::vector<std::string>& paths) {
    std::string::size_type start = 0;
    std::string::size_type location = path_list.find(PATH_SEPARATOR);
    while (location != std::string::npos) {
        paths.push_back(path_list.substr(start, location));
        start = location + 1;
        location = path_list.find(PATH_SEPARATOR, start);
    }
    paths.push_back(path_list.substr(start, location));
    return true;
}

bool FileSysUtilsFindFilesInPath(const std::string& path, std::vector<std::string>& files) {
    DIR* dir = opendir(path.c_str());
    if (dir == nullptr) {
        return false;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        files.emplace_back(entry->d_name);
    }
    closedir(dir);
    return true;
}

#endif
