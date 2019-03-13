// Copyright (c) 2017-2019 The Khronos Group Inc.
// Copyright (c) 2017-2019 Valve Corporation
// Copyright (c) 2017-2019 LunarG, Inc.
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

#include <string>
#include <vector>

#include "loader_platform.hpp"
#include "loader_interfaces.h"

class ApiLayerInterface {
   public:
    // Factory method
    static XrResult LoadApiLayers(const std::string& openxr_command, uint32_t enabled_api_layer_count,
                                  const char* const* enabled_api_layer_names,
                                  std::vector<std::unique_ptr<ApiLayerInterface>>& api_layer_interfaces);
    // Static queries
    static XrResult GetApiLayerProperties(const std::string& openxr_command, uint32_t incoming_count, uint32_t* outgoing_count,
                                          XrApiLayerProperties* api_layer_properties);
    static XrResult GetInstanceExtensionProperties(const std::string& openxr_command, const char* layer_name,
                                                   std::vector<XrExtensionProperties>& extension_properties);

    ApiLayerInterface(std::string layer_name, LoaderPlatformLibraryHandle layer_library,
                      std::vector<std::string>& supported_extensions, PFN_xrGetInstanceProcAddr get_instant_proc_addr,
                      PFN_xrCreateApiLayerInstance create_api_layer_instance);
    virtual ~ApiLayerInterface();

    PFN_xrGetInstanceProcAddr GetInstanceProcAddrFuncPointer() { return _get_instant_proc_addr; }
    PFN_xrCreateApiLayerInstance GetCreateApiLayerInstanceFuncPointer() { return _create_api_layer_instance; }

    std::string LayerName() { return _layer_name; }

    // Generated methods
    void GenUpdateInstanceDispatchTable(XrInstance instance, std::unique_ptr<XrGeneratedDispatchTable>& table);
    bool SupportsExtension(const std::string& extension_name);

   private:
    std::string _layer_name;
    LoaderPlatformLibraryHandle _layer_library;
    PFN_xrGetInstanceProcAddr _get_instant_proc_addr;
    PFN_xrCreateApiLayerInstance _create_api_layer_instance;
    std::vector<std::string> _supported_extensions;
};
