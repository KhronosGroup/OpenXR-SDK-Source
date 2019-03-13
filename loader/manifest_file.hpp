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
// Author: Mark Young <marky@lunarg.com>
//

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "xr_dependencies.h"
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <json/json.h>

enum ManifestFileType {
    MANIFEST_TYPE_UNDEFINED = 0,
    MANIFEST_TYPE_RUNTIME,
    MANIFEST_TYPE_IMPLICIT_API_LAYER,
    MANIFEST_TYPE_EXPLICIT_API_LAYER,
};

struct JsonVersion {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
};

struct ExtensionListing {
    std::string name;
    uint32_t spec_version;
    std::vector<std::string> entrypoints;
};

// ManifestFile class -
// Base class responsible for finding and parsing manifest files.
class ManifestFile {
   public:
    ManifestFile(ManifestFileType type, const std::string &filename, const std::string &library_path);
    virtual ~ManifestFile();
    static bool IsValidJson(Json::Value &root, JsonVersion &version);

    // We don't want any copy constructors
    ManifestFile &operator=(const ManifestFile &manifest_file) = delete;

    ManifestFileType Type() { return _type; }
    std::string Filename() { return _filename; }
    std::string LibraryPath() { return _library_path; }
    void GetInstanceExtensionProperties(std::vector<XrExtensionProperties> &props);
    void GetDeviceExtensionProperties(std::vector<XrExtensionProperties> &props);
    const std::string &GetFunctionName(const std::string &func_name);

   protected:
    std::string _filename;
    ManifestFileType _type;
    std::string _library_path;
    std::vector<ExtensionListing> _instance_extensions;
    std::vector<ExtensionListing> _device_extensions;
    std::unordered_map<std::string, std::string> _functions_renamed;
};

// RuntimeManifestFile class -
// Responsible for finding and parsing Runtime-specific manifest files.
class RuntimeManifestFile : public ManifestFile {
   public:
    // Factory method
    static XrResult FindManifestFiles(ManifestFileType type, std::vector<std::unique_ptr<RuntimeManifestFile>> &manifest_files);

    RuntimeManifestFile(const std::string &filename, const std::string &library_path);
    virtual ~RuntimeManifestFile();
    static void CreateIfValid(std::string filename, std::vector<std::unique_ptr<RuntimeManifestFile>> &manifest_files);

    // We don't want any copy constructors
    RuntimeManifestFile &operator=(const RuntimeManifestFile &manifest_file) = delete;
};

// ApiLayerManifestFile class -
// Responsible for finding and parsing API Layer-specific manifest files.
class ApiLayerManifestFile : public ManifestFile {
   public:
    // Factory method
    static XrResult FindManifestFiles(ManifestFileType type, std::vector<std::unique_ptr<ApiLayerManifestFile>> &manifest_files);

    ApiLayerManifestFile(ManifestFileType type, const std::string &filename, const std::string &layer_name,
                         const std::string &description, const JsonVersion &api_version, const uint32_t &implementation_version,
                         const std::string &library_path);
    virtual ~ApiLayerManifestFile();
    static void CreateIfValid(ManifestFileType type, std::string filename,
                              std::vector<std::unique_ptr<ApiLayerManifestFile>> &manifest_files);

    // We don't want any copy constructors
    ApiLayerManifestFile &operator=(const ApiLayerManifestFile &manifest_file) = delete;

    std::string LayerName() { return _layer_name; }
    XrApiLayerProperties GetApiLayerProperties();

   private:
    JsonVersion _api_version;
    std::string _layer_name;
    std::string _description;
    uint32_t _implementation_version;
};
