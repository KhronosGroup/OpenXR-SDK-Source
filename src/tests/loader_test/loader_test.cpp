// Copyright (c) 2017-2021, The Khronos Group Inc.
// Copyright (c) 2017-2019 Valve Corporation
// Copyright (c) 2017-2019 LunarG, Inc.
//
// SPDX-License-Identifier: Apache-2.0
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
// Author: Dave Houlton <daveh@lunarg.com>
//

#include <iostream>
#include <sstream>
#include <cstring>
#include <vector>

#include "filesystem_utils.hpp"
#include "loader_test_utils.hpp"

#include "hex_and_handles.h"

#include "xr_dependencies.h"
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#ifdef XR_USE_GRAPHICS_API_D3D11
#include "d3d11.h"
#endif

#include <type_traits>
static_assert(sizeof(XrStructureType) == 4, "This should be a 32-bit enum");

// Add some judicious char savers
using std::cout;
using std::endl;

// Filter out the loader's messages to std::cerr if this is defined to 1.  This allows a
// clean output for the test.
#define FILTER_OUT_LOADER_ERRORS 1

enum LoaderTestGraphicsApiToUse { GRAPHICS_API_UNKONWN = 0, GRAPHICS_API_OPENGL, GRAPHICS_API_VULKAN, GRAPHICS_API_D3D };
LoaderTestGraphicsApiToUse g_graphics_api_to_use = GRAPHICS_API_UNKONWN;
bool g_debug_utils_exists = false;
bool g_has_installed_runtime = false;

void CleanupEnvironmentVariables() {
    LoaderTestUnsetEnvironmentVariable("XR_ENABLE_API_LAYERS");
    LoaderTestUnsetEnvironmentVariable("XR_API_LAYER_PATH");
    LoaderTestUnsetEnvironmentVariable("XR_RUNTIME_JSON");
}

// The loader will keep a runtime loaded after a successful xrEnumerateInstanceExtensionProperties call until an instance is created
// and destroyed.
void ForceLoaderUnloadRuntime() {
    XrInstance instance;
    XrInstanceCreateInfo instance_create_info{XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy(instance_create_info.applicationInfo.applicationName, "Force Unload");
    instance_create_info.applicationInfo.applicationVersion = 688;
    if (XR_SUCCEEDED(xrCreateInstance(&instance_create_info, &instance))) {
        xrDestroyInstance(instance);
    }
}

bool DetectInstalledRuntime() {
    bool runtime_found = false;
    uint32_t ext_count = 0;

    // Disable any potential envar override - just looking for an installed runtime
    LoaderTestUnsetEnvironmentVariable("XR_RUNTIME_JSON");

    ForceLoaderUnloadRuntime();

    // Enumerating available extensions requires loading the runtime
    XrResult ret = xrEnumerateInstanceExtensionProperties(nullptr, 0, &ext_count, nullptr);

    // Successful enumeration implies an active runtime is installed (regardless of ext count)
    runtime_found = XR_SUCCEEDED(ret);

    // See which display extension (if any) to use
    // If multiple are supported, pick the first one found
    if (runtime_found && (0 < ext_count)) {
        std::vector<XrExtensionProperties> properties;
        properties.resize(ext_count);
        for (uint32_t prop = 0; prop < ext_count; ++prop) {
            properties[prop] = {XR_TYPE_EXTENSION_PROPERTIES, nullptr, {0, 0}};
        }
        ret = xrEnumerateInstanceExtensionProperties(nullptr, ext_count, &ext_count, properties.data());
        if (XR_FAILED(ret)) {
            return runtime_found;
        }

        for (uint32_t ext = 0; ext < ext_count; ext++) {
#ifdef XR_USE_GRAPHICS_API_D3D11
            if (!strcmp(properties[ext].extensionName, XR_KHR_D3D11_ENABLE_EXTENSION_NAME)) {
                g_graphics_api_to_use = GRAPHICS_API_D3D;
                break;
            }
#endif  // XR_USE_GRAPHICS_API_D3D11

#ifdef XR_USE_GRAPHICS_API_VULKAN
            if (strcmp(properties[ext].extensionName, XR_KHR_VULKAN_ENABLE_EXTENSION_NAME) == 0) {
                g_graphics_api_to_use = GRAPHICS_API_VULKAN;
                break;
            }
#endif  // XR_USE_GRAPHICS_API_VULKAN

#ifdef XR_USE_GRAPHICS_API_OPENGL
            if (strcmp(properties[ext].extensionName, XR_KHR_OPENGL_ENABLE_EXTENSION_NAME) == 0) {
                g_graphics_api_to_use = GRAPHICS_API_OPENGL;
                break;
            }
#endif  // XR_USE_GRAPHICS_API_OPENGL
        }

        // While we're here, also check for debug utils support
        for (uint32_t ext = 0; ext < ext_count; ext++) {
            if (strcmp(properties[ext].extensionName, XR_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
                g_debug_utils_exists = true;
                break;
            }
        }
    }

    return runtime_found;
}

// Test the xrEnumerateApiLayerProperties function through the loader.
void TestEnumLayers(uint32_t& total, uint32_t& passed, uint32_t& skipped, uint32_t& failed) {
    uint32_t local_total = 0;
    uint32_t local_passed = 0;
    uint32_t local_skipped = 0;
    uint32_t local_failed = 0;
#if FILTER_OUT_LOADER_ERRORS == 1
    std::streambuf* original_cerr = nullptr;
#endif
    try {
        XrResult test_result = XR_SUCCESS;
        std::vector<XrApiLayerProperties> layer_props;

#if FILTER_OUT_LOADER_ERRORS == 1
        // Re-direct std::cerr to a string since we're intentionally causing errors and we don't
        // want it polluting the output stream.
        std::stringstream buffer;
        original_cerr = std::cerr.rdbuf(buffer.rdbuf());
#endif

        cout << "    Starting TestEnumLayers" << endl;

        uint32_t in_layer_value = 0;
        uint32_t out_layer_value = 0;
        std::string subtest_name;

        // Tests with no explicit layers set
        subtest_name = "No explicit layers";
        // NOTE: Implicit layers will still be present, need to figure out what to do here.
        LoaderTestUnsetEnvironmentVariable("XR_ENABLE_API_LAYERS");
        LoaderTestUnsetEnvironmentVariable("XR_API_LAYER_PATH");

        // Test number query
        local_total++;
        cout << "        " << subtest_name << " layer count check: ";
        test_result = xrEnumerateApiLayerProperties(in_layer_value, &out_layer_value, nullptr);
        if (XR_FAILED(test_result)) {
            cout << "Failed with return " << std::to_string(test_result) << endl;
            local_failed++;
        } else {
            local_passed++;
            cout << "Passed: " << out_layer_value << " layers available." << endl;
        }

        // If any implicit layers are found, try property return
        if (out_layer_value > 0) {
            in_layer_value = out_layer_value;
            out_layer_value = 0;
            layer_props.resize(in_layer_value);
            for (uint32_t prop = 0; prop < in_layer_value; ++prop) {
                layer_props[prop] = {XR_TYPE_API_LAYER_PROPERTIES, nullptr, {0, 0}};
            }
            local_total++;
            cout << "        " << subtest_name << " layer props query: ";
            test_result = xrEnumerateApiLayerProperties(in_layer_value, &out_layer_value, layer_props.data());
            if (XR_FAILED(test_result)) {
                cout << "Failed with return " << std::to_string(test_result) << endl;
                local_failed++;
            } else {
                local_passed++;
                cout << "Passed" << endl;
                for (const auto& prop : layer_props) {
                    cout << "           - " << prop.layerName << endl;
                }
            }
        }

        // Tests with some explicit layers instead
        in_layer_value = 0;
        out_layer_value = 0;
        subtest_name = "Simple explicit layers";
        uint32_t num_valid_jsons = 6;

        // Point to json directory, contains 6 valid json files
        LoaderTestSetEnvironmentVariable("XR_API_LAYER_PATH", "resources/layers");
        // LoaderTestSetEnvironmentVariable("XR_ENABLE_API_LAYERS", "XrApiLayer_test:XrApiLayer_test_good_relative_path");

        // Test number query
        local_total++;
        cout << "        " << subtest_name << " layer count check: ";
        test_result = xrEnumerateApiLayerProperties(in_layer_value, &out_layer_value, nullptr);
        if (XR_FAILED(test_result)) {
            cout << "Failed with return " << std::to_string(test_result) << endl;
            local_failed++;
        } else {
            if (out_layer_value != num_valid_jsons) {
                cout << "Failed, expected count " << num_valid_jsons << ", got " << std::to_string(out_layer_value) << endl;
                local_failed++;
            } else {
                local_passed++;
                cout << "Passed" << endl;
            }
        }

        // Try property return
        in_layer_value = out_layer_value;
        out_layer_value = 0;
        layer_props.resize(in_layer_value);
        for (uint32_t prop = 0; prop < in_layer_value; ++prop) {
            layer_props[prop] = {XR_TYPE_API_LAYER_PROPERTIES, nullptr, {0, 0}};
        }
        local_total++;
        cout << "        " << subtest_name << " layer props query: ";
        test_result = xrEnumerateApiLayerProperties(in_layer_value, &out_layer_value, layer_props.data());
        if (XR_FAILED(test_result)) {
            cout << "Failed with return " << std::to_string(test_result) << endl;
            local_failed++;
        } else {
            bool found_bad = false;
            bool found_good_absolute_test = false;
            bool found_good_relative_test = false;
            uint16_t expected_major = XR_VERSION_MAJOR(XR_CURRENT_API_VERSION);
            uint16_t expected_minor = XR_VERSION_MINOR(XR_CURRENT_API_VERSION);
            for (uint32_t iii = 0; iii < out_layer_value; ++iii) {
                std::string layer_name = layer_props[iii].layerName;
                if ("XR_APILAYER_test" == layer_name && 1 == layer_props[iii].layerVersion &&
                    XR_MAKE_VERSION(expected_major, expected_minor, 0U) == layer_props[iii].specVersion &&
                    0 == strcmp(layer_props[iii].description, "Test_description")) {
                    found_good_absolute_test = true;
                } else if ("XR_APILAYER_LUNARG_test_good_relative_path" == layer_name && 1 == layer_props[iii].layerVersion &&
                           XR_MAKE_VERSION(expected_major, expected_minor, 0U) == layer_props[iii].specVersion &&
                           0 == strcmp(layer_props[iii].description, "Test_description")) {
                    found_good_relative_test = true;
                } else if (std::string::npos != layer_name.find("_badjson")) {
                    // If we enumerated any other layers (excepting the 'badjson' variants), it's an error
                    cout << "Failed, found unexpected layer " << layer_name << " in list" << endl;
                    found_bad = true;
                    local_failed++;
                    break;
                }
            }

            if (!found_good_absolute_test || !found_good_relative_test) {
                cout << "Failed, did not find ";
                if (!found_good_absolute_test) {
                    cout << "XrApiLayer_test";
                }
                if (!found_good_relative_test) {
                    if (!found_good_absolute_test) {
                        cout << " or ";
                    }
                    cout << "XrApiLayer_test_good_relative_path";
                }
                cout << endl;
                local_failed++;
            } else if (!found_bad) {
                cout << "Passed" << endl;
                local_passed++;
            }

            for (const auto& prop : layer_props) {
                cout << "           - " << prop.layerName << endl;
            }
        }

    } catch (...) {
        cout << "Exception triggered during test, automatic failure" << endl;
        local_failed++;
        local_total++;
    }

#if FILTER_OUT_LOADER_ERRORS == 1
    // Restore std::cerr to the original buffer
    std::cerr.rdbuf(original_cerr);
#endif

    // Cleanup
    CleanupEnvironmentVariables();

    // Output results for this test
    cout << "    Finished TestEnumLayers : ";
    if (local_failed > 0) {
        cout << "Failed (Local - Passed: " << std::to_string(local_passed) << ", Failed: " << std::to_string(local_failed)
             << ", Skipped: " << std::to_string(local_skipped) << ")" << endl
             << endl;
    } else {
        cout << "Passed (Local - Passed: " << std::to_string(local_passed) << ", Failed: " << std::to_string(local_failed)
             << ", Skipped: " << std::to_string(local_skipped) << ")" << endl
             << endl;
    }
    total += local_total;
    passed += local_passed;
    failed += local_failed;
    skipped += local_skipped;
}

// Test the xrEnumerateInstanceExtensionProperties function through the loader.
void TestEnumInstanceExtensions(uint32_t& total, uint32_t& passed, uint32_t& skipped, uint32_t& failed) {
    uint32_t local_total = 0;
    uint32_t local_passed = 0;
    uint32_t local_skipped = 0;
    uint32_t local_failed = 0;
#if FILTER_OUT_LOADER_ERRORS == 1
    std::streambuf* original_cerr = nullptr;
#endif

    try {
        XrResult test_result = XR_SUCCESS;
        uint32_t in_extension_value;
        uint32_t out_extension_value;
        std::vector<XrExtensionProperties> properties;

        cout << "    Starting TestEnumInstanceExtensions" << endl;

#if FILTER_OUT_LOADER_ERRORS == 1
        // Re-direct std::cerr to a string since we're intentionally causing errors and we don't
        // want it polluting the output stream.
        std::stringstream buffer;
        original_cerr = std::cerr.rdbuf(buffer.rdbuf());
#endif

        for (uint32_t test = 0; test < 2; ++test) {
            std::string subtest_name;
            std::string valid_runtime_path;

            switch (test) {
                // No Explicit layers set
                case 0:
                    subtest_name = "with no explicit API layers";
                    // NOTE: Implicit layers will still be present, need to figure out what to do here.
                    LoaderTestUnsetEnvironmentVariable("XR_ENABLE_API_LAYERS");
                    LoaderTestUnsetEnvironmentVariable("XR_API_LAYER_PATH");
                    break;
                default:
                    subtest_name = "with explicit API layers";
                    LoaderTestSetEnvironmentVariable("XR_API_LAYER_PATH", "resources/layers");
                    LoaderTestSetEnvironmentVariable("XR_ENABLE_API_LAYERS", "XrApiLayer_test");
                    break;
            }

            // Test various bad runtime json files
            std::vector<std::string> files;
            std::string test_path;
            FileSysUtilsGetCurrentPath(test_path);
            test_path = test_path + TEST_DIRECTORY_SYMBOL + "resources" + TEST_DIRECTORY_SYMBOL + "runtimes";
            if (FileSysUtilsFindFilesInPath(test_path, files)) {
                for (std::string& cur_file : files) {
                    std::string full_name = test_path + TEST_DIRECTORY_SYMBOL + cur_file;
                    if (std::string::npos != cur_file.find("_badjson_") || std::string::npos != cur_file.find("_badnegotiate_")) {
                        // This is a "bad" runtime path.
                        FileSysUtilsGetCurrentPath(full_name);
                        LoaderTestSetEnvironmentVariable("XR_RUNTIME_JSON", full_name);

                        ForceLoaderUnloadRuntime();

                        in_extension_value = 0;
                        out_extension_value = 0;
                        local_total++;
                        cout << "        JSON " << cur_file << " extension enum count query (" << subtest_name << "): ";
                        test_result =
                            xrEnumerateInstanceExtensionProperties(nullptr, in_extension_value, &out_extension_value, nullptr);
                        if (XR_SUCCEEDED(test_result)) {
                            cout << "Failed" << endl;
                            local_failed++;  // Success is unexpected.
                        } else {
                            cout << "Passed" << endl;
                            local_passed++;  // Failure is expected.
                        }
                    }
                }
            }

            // Test the active runtime, if installed
            if (g_has_installed_runtime) {
                LoaderTestUnsetEnvironmentVariable("XR_RUNTIME_JSON");

                ForceLoaderUnloadRuntime();

                // Query the count (should return 2)
                in_extension_value = 0;
                out_extension_value = 0;
                local_total++;
                cout << "        Active runtime extension enum count query (" << subtest_name << "): ";
                test_result = xrEnumerateInstanceExtensionProperties(nullptr, in_extension_value, &out_extension_value, nullptr);
                if (XR_FAILED(test_result)) {
                    cout << "Failed" << endl;
                    local_failed++;
                } else {
                    cout << "Passed" << endl;
                    local_passed++;
                }

                // Get the properties
                properties.resize(out_extension_value);
                for (uint32_t prop = 0; prop < out_extension_value; ++prop) {
                    properties[prop] = {XR_TYPE_EXTENSION_PROPERTIES, nullptr, {0, 0}};
                }
                in_extension_value = out_extension_value;
                out_extension_value = 0;
                local_total++;
                cout << "        Active runtime extension enum properties query (" << subtest_name << "): ";
                test_result =
                    xrEnumerateInstanceExtensionProperties(nullptr, in_extension_value, &out_extension_value, properties.data());
                if (XR_FAILED(test_result)) {
                    cout << "Failed" << endl;
                    local_failed++;
                } else {
                    bool found_error = false;
                    for (XrExtensionProperties prop : properties) {
                        // Just check if extension name begins with "XR_"
                        if (strlen(prop.extensionName) < 4 || 0 != strncmp(prop.extensionName, "XR_", 3)) {
                            found_error = true;
                        }
                    }

                    if (found_error) {
                        cout << "Failed, malformed extension name." << endl;
                        local_failed++;
                    } else {
                        cout << "Passed" << endl;
                        local_passed++;
                    }
                }

                // Try with a garbage layer name
                in_extension_value = 0;
                out_extension_value = 0;
                local_total++;
                cout << "        Garbage Layer extension enum count query (" << subtest_name << "): ";
                test_result = xrEnumerateInstanceExtensionProperties("XrApiLayer_no_such_layer", in_extension_value,
                                                                     &out_extension_value, nullptr);
                if (XR_ERROR_API_LAYER_NOT_PRESENT != test_result) {
                    cout << "Failed" << endl;
                    local_failed++;
                } else {
                    cout << "Passed" << endl;
                    local_passed++;
                }

                // Try with a valid layer name (if layer is present)
                in_extension_value = 0;
                out_extension_value = 0;
                local_total++;
                cout << "        Valid Layer extension enum count query (" << subtest_name << "): ";
                test_result =
                    xrEnumerateInstanceExtensionProperties("XR_APILAYER_test", in_extension_value, &out_extension_value, nullptr);
                if ((test == 0 && XR_ERROR_API_LAYER_NOT_PRESENT != test_result) || (test == 1 && XR_FAILED(test_result))) {
                    cout << "Failed" << endl;
                    local_failed++;
                } else {
                    cout << "Passed" << endl;
                    local_passed++;
                }
            }
        }
    } catch (std::exception const& e) {
        cout << "Exception triggered during test (" << e.what() << "), automatic failure" << endl;
        local_failed++;
        local_total++;

    } catch (...) {
        cout << "Exception triggered during test, automatic failure" << endl;
        local_failed++;
        local_total++;
    }

#if FILTER_OUT_LOADER_ERRORS == 1
    // Restore std::cerr to the original buffer
    std::cerr.rdbuf(original_cerr);
#endif

    // Cleanup
    CleanupEnvironmentVariables();

    // Output results for this test
    cout << "    Finished TestEnumInstanceExtensions : ";
    if (local_failed > 0) {
        cout << "Failed (Local - Passed: " << std::to_string(local_passed) << ", Failed: " << std::to_string(local_failed)
             << ", Skipped: " << std::to_string(local_skipped) << ")" << endl
             << endl;
    } else {
        cout << "Passed (Local - Passed: " << std::to_string(local_passed) << ", Failed: " << std::to_string(local_failed)
             << ", Skipped: " << std::to_string(local_skipped) << ")" << endl
             << endl;
    }
    total += local_total;
    passed += local_passed;
    failed += local_failed;
    skipped += local_skipped;
}

#define DEFINE_TEST(test_name) void test_name(uint32_t& total, uint32_t& passed, uint32_t& skipped, uint32_t& failed)

#define INIT_TEST(test_name)    \
    uint32_t local_total = 0;   \
    uint32_t local_passed = 0;  \
    uint32_t local_skipped = 0; \
    uint32_t local_failed = 0;  \
    cout << "    Starting " << #test_name << endl;

#define TEST_REPORT(test_name)                                                                                             \
    cout << "    Finished " << #test_name << ": ";                                                                         \
    if (local_failed > 0) {                                                                                                \
        cout << "Failed (Local - Passed: " << std::to_string(local_passed) << ", Failed: " << std::to_string(local_failed) \
             << ", Skipped: " << std::to_string(local_skipped) << ")" << endl                                              \
             << endl;                                                                                                      \
    } else {                                                                                                               \
        cout << "Passed (Local - Passed: " << std::to_string(local_passed) << ", Failed: " << std::to_string(local_failed) \
             << ", Skipped: " << std::to_string(local_skipped) << ")" << endl                                              \
             << endl;                                                                                                      \
    }                                                                                                                      \
    total += local_total;                                                                                                  \
    passed += local_passed;                                                                                                \
    failed += local_failed;                                                                                                \
    skipped += local_skipped;

#define TEST_EQUAL(test, expected, cout_string)                  \
    local_total++;                                               \
    if (expected != (test)) {                                    \
        cout << "        " << cout_string << ": Failed" << endl; \
        local_failed++;                                          \
    } else {                                                     \
        cout << "        " << cout_string << ": Passed" << endl; \
        local_passed++;                                          \
    }

#define TEST_NOT_EQUAL(test, expected, cout_string)              \
    local_total++;                                               \
    if (expected == (test)) {                                    \
        cout << "        " << cout_string << ": Failed" << endl; \
        local_failed++;                                          \
    } else {                                                     \
        cout << "        " << cout_string << ": Passed" << endl; \
        local_passed++;                                          \
    }

#define TEST_FAIL(cout_string) \
    local_total++;             \
    local_failed++;            \
    cout << "        " << cout_string << ": Failed" << endl;

// Test creating and destroying an OpenXR instance through the loader.
DEFINE_TEST(TestCreateDestroyInstance) {
    INIT_TEST(TestCreateDestroyInstance)

    try {
        XrResult expected_result = XR_SUCCESS;
        char valid_layer_to_enable[] = "XR_APILAYER_LUNARG_api_dump";
        char invalid_layer_to_enable[] = "XrApiLayer_invalid_layer_test";
        char invalid_extension_to_enable[] = "XR_KHR_fake_ext1";
        const char* const valid_layer_name_array[1] = {valid_layer_to_enable};
        const char* const invalid_layer_name_array[1] = {invalid_layer_to_enable};
        const char* valid_extension_name_array[1];
        const char* const invalid_extension_name_array[1] = {invalid_extension_to_enable};
        XrApplicationInfo application_info = {};
        strcpy(application_info.applicationName, "Loader Test");
        application_info.applicationVersion = 688;
        strcpy(application_info.engineName, "Infinite Improbability Drive");
        application_info.engineVersion = 42;
        application_info.apiVersion = XR_CURRENT_API_VERSION;

        std::string current_path;
        std::string layer_path;

        // Ensure using installed runtime
        LoaderTestUnsetEnvironmentVariable("XR_RUNTIME_JSON");

        // Convert relative test layer path to absolute, set envar
        FileSysUtilsGetCurrentPath(layer_path);
        layer_path =
            layer_path + TEST_DIRECTORY_SYMBOL + ".." + TEST_DIRECTORY_SYMBOL + ".." + TEST_DIRECTORY_SYMBOL + "api_layers";
        LoaderTestSetEnvironmentVariable("XR_API_LAYER_PATH", layer_path);

        // Look to see if the runtime supports any extensions, if it does, use the first one in this test.
        bool missing_extensions = true;

        // Make API dump write to a file, when loaded
        LoaderTestSetEnvironmentVariable("XR_API_DUMP_FILE_NAME", "api_dump_out.txt");

        for (uint32_t test_num = 0; test_num < 6; ++test_num) {
            XrInstance instance = XR_NULL_HANDLE;
            XrResult create_result = XR_ERROR_VALIDATION_FAILURE;
            std::string current_test_string;
            XrInstanceCreateInfo instance_create_info = {};
            instance_create_info.type = XR_TYPE_INSTANCE_CREATE_INFO;
            instance_create_info.applicationInfo = application_info;

            switch (test_num) {
                // Test 0 - Basic, plain-vanilla xrCreateInstance
                case 0:
                    current_test_string = "Simple xrCreateInstance";
                    expected_result = XR_SUCCESS;
                    break;
                // Test 1 - xrCreateInstance with a valid layer
                case 1:
                    current_test_string = "xrCreateInstance with a single valid layer";
                    expected_result = XR_SUCCESS;
                    instance_create_info.enabledApiLayerCount = 1;
                    instance_create_info.enabledApiLayerNames = valid_layer_name_array;
                    break;
                // Test 2 - xrCreateInstance with an invalid layer
                case 2:
                    current_test_string = "xrCreateInstance with a single invalid layer";
                    expected_result = XR_ERROR_API_LAYER_NOT_PRESENT;
                    instance_create_info.enabledApiLayerCount = 1;
                    instance_create_info.enabledApiLayerNames = invalid_layer_name_array;
                    break;
                // Test 3 - xrCreateInstance with a valid extension
                case 3:
                    if (missing_extensions) {
                        continue;
                    }
                    current_test_string = "xrCreateInstance with a single valid extension";
                    expected_result = XR_SUCCESS;
                    instance_create_info.enabledExtensionCount = 1;
                    instance_create_info.enabledExtensionNames = valid_extension_name_array;
                    break;
                // Test 4 - xrCreateInstance with an invalid extension
                case 4:
                    if (missing_extensions) {
                        continue;
                    }
                    current_test_string = "xrCreateInstance with a single invalid extension";
                    expected_result = XR_ERROR_EXTENSION_NOT_PRESENT;
                    instance_create_info.enabledExtensionCount = 1;
                    instance_create_info.enabledExtensionNames = invalid_extension_name_array;
                    break;
                // Test 5 - xrCreateInstance with everything
                case 5:
                    if (missing_extensions) {
                        continue;
                    }
                    current_test_string = "xrCreateInstance with app info, valid layer, and a valid extension";
                    expected_result = XR_SUCCESS;
                    instance_create_info.enabledApiLayerCount = 1;
                    instance_create_info.enabledApiLayerNames = valid_layer_name_array;
                    instance_create_info.enabledExtensionCount = 1;
                    instance_create_info.enabledExtensionNames = valid_extension_name_array;
                    break;
            }

            // Try create instance and look for the correct return
            std::string cur_message = current_test_string + " - xrCreateInstance";
            create_result = xrCreateInstance(&instance_create_info, &instance);
            TEST_EQUAL(create_result, expected_result, cur_message)

            // If the expected return was a success, clean up the created instance
            if (XR_SUCCEEDED(create_result)) {
                cur_message = current_test_string + " - xrDestroyInstance";
                TEST_EQUAL(xrDestroyInstance(instance), XR_SUCCESS, cur_message)
            }
        }

        // Make sure DestroyInstance with a NULL handle gives correct error
        TEST_EQUAL(xrDestroyInstance(XR_NULL_HANDLE), XR_ERROR_HANDLE_INVALID, "xrDestroyInstance(XR_NULL_HANDLE)")
    } catch (...) {
        TEST_FAIL("Exception triggered during test, automatic failure")
    }

    // Cleanup
    CleanupEnvironmentVariables();

    // Output results for this test
    TEST_REPORT(TestCreateDestroyInstance)
}

// Test at least one XrInstance function not directly implemented in the loader's manual code section.
// This is to make sure that the automatic instance functions work.
DEFINE_TEST(TestGetSystem) {
    INIT_TEST(TestGetSystem)

    try {
        std::string current_path;
        std::string layer_path;
        if (!FileSysUtilsGetCurrentPath(current_path) || !FileSysUtilsCombinePaths(current_path, "../../api_layers", layer_path)) {
            TEST_FAIL("Unable to set API layer path")
            TEST_REPORT(TestGetSystem)
            return;
        }
        LoaderTestSetEnvironmentVariable("XR_API_LAYER_PATH", layer_path);

        XrInstance instance = XR_NULL_HANDLE;
        XrApplicationInfo application_info = {};
        strcpy(application_info.applicationName, "Loader Test");
        application_info.applicationVersion = 688;
        strcpy(application_info.engineName, "Infinite Improbability Drive");
        application_info.engineVersion = 42;
        application_info.apiVersion = XR_CURRENT_API_VERSION;
        XrInstanceCreateInfo instance_create_info = {};
        instance_create_info.type = XR_TYPE_INSTANCE_CREATE_INFO;
        instance_create_info.applicationInfo = application_info;

        TEST_EQUAL(xrCreateInstance(&instance_create_info, &instance), XR_SUCCESS, "Creating instance")

        XrSystemGetInfo system_get_info = {};
        system_get_info.type = XR_TYPE_SYSTEM_GET_INFO;
        system_get_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

        XrSystemId systemId;
        TEST_EQUAL(xrGetSystem(instance, &system_get_info, &systemId), XR_SUCCESS, "xrGetSystem");

        TEST_EQUAL(xrDestroyInstance(instance), XR_SUCCESS, "Destroying instance")
    } catch (...) {
        TEST_FAIL("Exception triggered during test, automatic failure")
    }

    // Cleanup
    CleanupEnvironmentVariables();

    // Output results for this test
    TEST_REPORT(TestGetSystem)
}

// Test at least one non-XrInstance function to make sure that the automatic non-instance functions work.
DEFINE_TEST(TestCreateDestroySession) {
    INIT_TEST(TestCreateDestroySession)

    try {
        std::string current_path;
        std::string layer_path;

        if (!FileSysUtilsGetCurrentPath(current_path) || !FileSysUtilsCombinePaths(current_path, "../../api_layers", layer_path)) {
            TEST_FAIL("Unable to set API layer path")
            TEST_REPORT(TestCreateDestroySystem)
            return;
        }
        LoaderTestSetEnvironmentVariable("XR_API_LAYER_PATH", layer_path);

        XrInstance instance = XR_NULL_HANDLE;

        XrApplicationInfo app_info = {};
        strcpy(app_info.applicationName, "Loader Test");
        app_info.applicationVersion = 688;
        strcpy(app_info.engineName, "Infinite Improbability Drive");
        app_info.engineVersion = 42;
        app_info.apiVersion = XR_CURRENT_API_VERSION;

        XrInstanceCreateInfo instance_ci = {};
        instance_ci.type = XR_TYPE_INSTANCE_CREATE_INFO;
        instance_ci.applicationInfo = app_info;

        std::string ext_name;
        const char* ext_name_array[2];
        switch (g_graphics_api_to_use) {
            case GRAPHICS_API_D3D:
                ext_name = "XR_KHR_D3D11_enable";
                break;
            case GRAPHICS_API_VULKAN:
                ext_name = "XR_KHR_vulkan_enable";
                break;
            case GRAPHICS_API_OPENGL:
                ext_name = "XR_KHR_opengl_enable";
                break;
            default:
                TEST_FAIL("Unable to continue - no graphics binding extension found")
                TEST_REPORT(TestCreateDestroySession)
                return;
        }
        ext_name_array[0] = ext_name.c_str();
        instance_ci.enabledExtensionNames = ext_name_array;
        instance_ci.enabledExtensionCount = 1;
        cout << "        Display binding extension: " << ext_name << endl;

        TEST_EQUAL(xrCreateInstance(&instance_ci, &instance), XR_SUCCESS, "Creating instance")

        XrSystemGetInfo system_get_info;
        memset(&system_get_info, 0, sizeof(system_get_info));
        system_get_info.type = XR_TYPE_SYSTEM_GET_INFO;
        system_get_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

        XrSystemId systemId;
        TEST_EQUAL(xrGetSystem(instance, &system_get_info, &systemId), XR_SUCCESS, "xrGetSystem");

        if (systemId != XR_NULL_SYSTEM_ID) {
            void* graphics_binding = nullptr;

#ifdef XR_USE_GRAPHICS_API_VULKAN
            XrGraphicsBindingVulkanKHR vulkan_graphics_binding = {};
            if (g_graphics_api_to_use == GRAPHICS_API_VULKAN) {
                PFN_xrGetVulkanInstanceExtensionsKHR pfn_get_vulkan_instance_extensions_khr;
                PFN_xrGetVulkanDeviceExtensionsKHR pfn_get_vulkan_device_extensions_khr;
                PFN_xrGetVulkanGraphicsDeviceKHR pfn_get_vulkan_graphics_device_khr;
                PFN_xrGetVulkanGraphicsRequirementsKHR pfn_get_vulkan_graphics_requirements_khr;
                TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrGetVulkanInstanceExtensionsKHR",
                                                 reinterpret_cast<PFN_xrVoidFunction*>(&pfn_get_vulkan_instance_extensions_khr)),
                           XR_SUCCESS, "TestCreateDestroySession get xrGetVulkanInstanceExtensionsKHR function pointer");
                TEST_NOT_EQUAL(pfn_get_vulkan_instance_extensions_khr, nullptr,
                               "TestCreateDestroySession invalid xrGetVulkanInstanceExtensionsKHR function pointer");
                TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrGetVulkanDeviceExtensionsKHR",
                                                 reinterpret_cast<PFN_xrVoidFunction*>(&pfn_get_vulkan_device_extensions_khr)),
                           XR_SUCCESS, "TestCreateDestroySession get xrGetVulkanDeviceExtensionsKHR function pointer");
                TEST_NOT_EQUAL(pfn_get_vulkan_device_extensions_khr, nullptr,
                               "TestCreateDestroySession invalid xrGetVulkanDeviceExtensionsKHR function pointer");
                TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsDeviceKHR",
                                                 reinterpret_cast<PFN_xrVoidFunction*>(&pfn_get_vulkan_graphics_device_khr)),
                           XR_SUCCESS, "TestCreateDestroySession get xrGetVulkanGraphicsDeviceKHR function pointer");
                TEST_NOT_EQUAL(pfn_get_vulkan_graphics_device_khr, nullptr,
                               "TestCreateDestroySession invalid xrGetVulkanGraphicsDeviceKHR function pointer");
                TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirementsKHR",
                                                 reinterpret_cast<PFN_xrVoidFunction*>(&pfn_get_vulkan_graphics_requirements_khr)),
                           XR_SUCCESS, "TestCreateDestroySession get xrGetVulkanGraphicsRequirementsKHR function pointer");
                TEST_NOT_EQUAL(pfn_get_vulkan_graphics_requirements_khr, nullptr,
                               "TestCreateDestroySession invalid xrGetVulkanGraphicsRequirementsKHR function pointer");

                XrGraphicsRequirementsVulkanKHR vulkan_graphics_requirements = {};
                vulkan_graphics_requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR;
                vulkan_graphics_requirements.next = nullptr;
                TEST_EQUAL(pfn_get_vulkan_graphics_requirements_khr(instance, systemId, &vulkan_graphics_requirements), XR_SUCCESS,
                           "TestCreateDestroySession calling xrGetVulkanGraphicsRequirementsKHR");

                // TODO: Just need something other than NULL here for now (for validation).  Eventually need
                //       to correctly put in a valid pointer to an Display
                vulkan_graphics_binding.type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
                vulkan_graphics_binding.instance = VK_NULL_HANDLE;
                vulkan_graphics_binding.physicalDevice = VK_NULL_HANDLE;
                vulkan_graphics_binding.device = VK_NULL_HANDLE;
                vulkan_graphics_binding.queueFamilyIndex = 0;
                vulkan_graphics_binding.queueIndex = 0;
                graphics_binding = reinterpret_cast<void*>(&vulkan_graphics_binding);
            }
#endif  // XR_USE_GRAPHICS_API_VULKAN

#ifdef XR_USE_GRAPHICS_API_OPENGL
#ifdef _WIN32
            XrGraphicsBindingOpenGLWin32KHR win32_gl_graphics_binding = {};
#elif defined(OS_LINUX_XLIB)
            XrGraphicsBindingOpenGLXlibKHR glx_gl_graphics_binding = {};
#elif defined(OS_LINUX_XCB) || defined(OS_LINUX_XCB_GLX)
            XrGraphicsBindingOpenGLXcbKHR xcb_gl_graphics_binding = {};
#elif defined(OS_LINUX_WAYLAND)
            XrGraphicsBindingOpenGLWaylandKHR wayland_gl_graphics_binding = {};
#endif
            if (g_graphics_api_to_use == GRAPHICS_API_OPENGL) {
                PFN_xrGetOpenGLGraphicsRequirementsKHR pfn_get_opengl_graphics_requirements_khr;
                TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR",
                                                 reinterpret_cast<PFN_xrVoidFunction*>(&pfn_get_opengl_graphics_requirements_khr)),
                           XR_SUCCESS, "TestCreateDestroySession get xrGetOpenGLGraphicsRequirementsKHR function pointer");
                TEST_NOT_EQUAL(pfn_get_opengl_graphics_requirements_khr, nullptr,
                               "TestCreateDestroySession invalid xrGetOpenGLGraphicsRequirementsKHR function pointer");
                XrGraphicsRequirementsOpenGLKHR opengl_graphics_requirements = {};
                opengl_graphics_requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR;
                opengl_graphics_requirements.next = nullptr;
                TEST_EQUAL(pfn_get_opengl_graphics_requirements_khr(instance, systemId, &opengl_graphics_requirements), XR_SUCCESS,
                           "TestCreateDestroySession calling xrGetOpenGLGraphicsRequirementsKHR");
#ifdef _WIN32
                win32_gl_graphics_binding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
                graphics_binding = reinterpret_cast<void*>(&win32_gl_graphics_binding);
#elif defined(OS_LINUX_XLIB)
                glx_gl_graphics_binding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR;
                // TODO: Just need something other than NULL here for now (for validation).  Eventually need
                //       to correctly put in a valid pointer to an Display
                glx_gl_graphics_binding.xDisplay = reinterpret_cast<Display*>(0xFFFFFFFF);
                graphics_binding = reinterpret_cast<void*>(&glx_gl_graphics_binding);
#elif defined(OS_LINUX_XCB) || defined(OS_LINUX_XCB_GLX)
                xcb_gl_graphics_binding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_XCB_KHR;
                // TODO: Just need something other than NULL here for now (for validation).  Eventually need
                //       to correctly put in a valid pointer to an xcb_connection_t
                xcb_gl_graphics_binding.connection = reinterpret_cast<xcb_connection_t*>(0xFFFFFFFF);
                graphics_binding = reinterpret_cast<void*>(&xcb_gl_graphics_binding);
#elif defined(OS_LINUX_WAYLAND)
                wayland_gl_graphics_binding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR;
                // TODO: Just need something other than NULL here for now (for validation).  Eventually need
                //       to correctly put in a valid pointer to an wl_display
                wayland_gl_graphics_binding.display = reinterpret_cast<wl_display*>(0xFFFFFFFF);
                graphics_binding = reinterpret_cast<void*>(&wayland_gl_graphics_binding);
#endif
            }
#endif  // XR_USE_GRAPHICS_API_OPENGL

#ifdef XR_USE_GRAPHICS_API_D3D11
            XrGraphicsBindingD3D11KHR d3d11_graphics_binding = {};
            ID3D11Device* d3d_device = NULL;
            if (g_graphics_api_to_use == GRAPHICS_API_D3D) {
                PFN_xrGetD3D11GraphicsRequirementsKHR pfn_get_d3d11_reqs;
                TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrGetD3D11GraphicsRequirementsKHR",
                                                 reinterpret_cast<PFN_xrVoidFunction*>(&pfn_get_d3d11_reqs)),
                           XR_SUCCESS, "TestCreateDestroySession get xrGetD3D11GraphicsRequirementsKHR function pointer");
                TEST_NOT_EQUAL(pfn_get_d3d11_reqs, nullptr,
                               "TestCreateDestroySession invalid xrGetD3D11GraphicsRequirementsKHR function pointer");
                XrGraphicsRequirementsD3D11KHR d3d_reqs = {};
                d3d_reqs.type = XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR;
                d3d_reqs.next = nullptr;
                TEST_EQUAL(pfn_get_d3d11_reqs(instance, systemId, &d3d_reqs), XR_SUCCESS,
                           "TestCreateDestroySession calling xrGetD3D11GraphicsRequirementsKHR");
                HRESULT res =
                    D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &d3d_device, NULL, NULL);
                TEST_EQUAL(res, S_OK, "TestCreateDestroySession creating D3D11 reference device")

                d3d11_graphics_binding.type = XR_TYPE_GRAPHICS_BINDING_D3D11_KHR;
                d3d11_graphics_binding.device = d3d_device;
                graphics_binding = reinterpret_cast<void*>(&d3d11_graphics_binding);
            }
#endif  // XR_USE_GRAPHICS_API_D3D11

            if (graphics_binding != nullptr) {
                // Create a session for us to begin
                XrSession session;
                XrSessionCreateInfo session_create_info = {};
                session_create_info.type = XR_TYPE_SESSION_CREATE_INFO;
                session_create_info.systemId = systemId;
                session_create_info.next = graphics_binding;
                TEST_EQUAL(xrCreateSession(instance, &session_create_info, &session), XR_SUCCESS, "xrCreateSession")
                TEST_EQUAL(xrDestroySession(session), XR_SUCCESS, "Destroying session")
            } else {
                TEST_FAIL("No graphics binding available for CreateSession test")
            }
#ifdef XR_USE_GRAPHICS_API_D3D11
            if (d3d_device) d3d_device->Release();
#endif
        }
        TEST_EQUAL(xrDestroyInstance(instance), XR_SUCCESS, "Destroying instance")
    } catch (...) {
        TEST_FAIL("Exception triggered during test, automatic failure")
    }

    // Cleanup
    CleanupEnvironmentVariables();

    // Output results for this test
    TEST_REPORT(TestCreateDestroySession)
}

const char test_function_name[] = "MyTestFunctionName";
static char message_id[64];
static uint64_t object_handle;
static char object_name[64];
static uint32_t num_objects = 0;
const char test_message[] = "This is a test message - 1 . 2! 3";
static void* expected_user_value = nullptr;
static bool captured_error = false;
static bool captured_warning = false;
static bool captured_info = false;
static bool captured_verbose = false;
static bool captured_general_message = false;
static bool captured_validation_message = false;
static bool captured_performance_message = false;
static bool user_data_correct = false;
static bool encountered_bad_type = false;
static bool encountered_bad_severity = false;
static bool function_matches = false;
static bool message_matches = false;
static bool message_id_matches = false;
static bool num_objects_matches = false;
static bool object_contents_match = false;
const char g_first_individual_label_name[] = "First individual label";
const char g_second_individual_label_name[] = "Second individual label";
const char g_third_individual_label_name[] = "Third individual label";
const char g_first_label_region_name[] = "First Label Region";
const char g_second_label_region_name[] = "Second Label Region";
uint32_t g_in_region_num = 0;
uint32_t g_individual_label_num = 0;
static bool g_expecting_labels = false;
static bool g_captured_labels = false;
static bool g_captured_only_expected_labels = false;

XrBool32 XRAPI_PTR TestDebugUtilsCallback(XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
                                          XrDebugUtilsMessageTypeFlagsEXT messageType,
                                          const XrDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData) {
    user_data_correct = (expected_user_value == userData);

    if (strcmp(callbackData->functionName, test_function_name) == 0) {
        function_matches = true;
    }

    if (strcmp(callbackData->messageId, message_id) == 0) {
        message_id_matches = true;
    }

    if (strcmp(callbackData->message, test_message) == 0) {
        message_matches = true;
    }

    switch (messageType) {
        case XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            captured_general_message = true;
            break;
        case XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
            captured_validation_message = true;
            break;
        case XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
            captured_performance_message = true;
            break;
        default:
            encountered_bad_type = true;
            break;
    }

    switch (messageSeverity) {
        case XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            captured_error = true;
            break;
        case XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            captured_warning = true;
            break;
        case XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            captured_info = true;
            break;
        case XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            captured_verbose = true;
            break;
        default:
            encountered_bad_severity = true;
            break;
    }

    if (callbackData->objectCount == num_objects) {
        num_objects_matches = true;
        if (callbackData->objectCount == 3) {
            if (callbackData->objects[0].type == XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT &&
                callbackData->objects[0].next == nullptr && callbackData->objects[0].objectType == XR_OBJECT_TYPE_INSTANCE &&
                callbackData->objects[0].objectHandle == static_cast<uint64_t>(0xFEED0001) &&
                callbackData->objects[0].objectName == nullptr &&
                callbackData->objects[1].type == XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT &&
                callbackData->objects[1].next == nullptr && callbackData->objects[1].objectType == XR_OBJECT_TYPE_SESSION &&
                callbackData->objects[1].objectHandle == static_cast<uint64_t>(0xFEED0002) &&
                callbackData->objects[1].objectName == nullptr &&
                callbackData->objects[2].type == XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT &&
                callbackData->objects[2].next == nullptr && callbackData->objects[2].objectType == XR_OBJECT_TYPE_ACTION_SET &&
                callbackData->objects[2].objectHandle == static_cast<uint64_t>(0xFEED0003) &&
                callbackData->objects[2].objectName == nullptr) {
                object_contents_match = true;
            }
        } else if (callbackData->objectCount == 1) {
            // This should have an actual object and name
            if (callbackData->objects[0].type == XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT &&
                callbackData->objects[0].next == nullptr && callbackData->objects[0].objectType == XR_OBJECT_TYPE_INSTANCE &&
                callbackData->objects[0].objectHandle == object_handle &&
                (strcmp(callbackData->objects[0].objectName, object_name) == 0)) {
                object_contents_match = true;
            }
        }
    }

    if (callbackData->sessionLabelCount > 0) {
        g_captured_labels = true;
        g_captured_only_expected_labels = false;
        if (g_expecting_labels) {
            if (g_in_region_num == 0) {
                // Should not have any label region in list
                bool found_invalid = false;
                for (uint32_t label_index = 0; label_index < callbackData->sessionLabelCount; ++label_index) {
                    if ((strcmp(callbackData->sessionLabels[label_index].labelName, g_second_label_region_name) == 0) ||
                        (strcmp(callbackData->sessionLabels[label_index].labelName, g_first_label_region_name) == 0)) {
                        found_invalid = true;
                    }
                }
                if (!found_invalid) {
                    g_captured_only_expected_labels = true;
                }
            } else if (g_in_region_num == 2) {
                // Should have g_second_label_region_name in list
                // Should have g_first_label_region_name in list
                if (callbackData->sessionLabelCount >= 2 &&
                    ((strcmp(callbackData->sessionLabels[0].labelName, g_second_label_region_name) == 0) &&
                     (strcmp(callbackData->sessionLabels[1].labelName, g_first_label_region_name) == 0))) {
                    g_captured_only_expected_labels = true;
                }
            } else if (g_in_region_num == 1) {
                // Should not have g_second_label_region_name in list
                // Should have g_first_label_region_name in list
                if (callbackData->sessionLabelCount >= 2 &&
                    (strcmp(callbackData->sessionLabels[0].labelName, g_second_label_region_name) != 0) &&
                    (strcmp(callbackData->sessionLabels[1].labelName, g_first_label_region_name) == 0)) {
                    g_captured_only_expected_labels = true;
                } else if (callbackData->sessionLabelCount >= 1 &&
                           (strcmp(callbackData->sessionLabels[0].labelName, g_first_label_region_name) == 0)) {
                    g_captured_only_expected_labels = true;
                }
            }

            if (g_individual_label_num == 0) {
                // Should not have any individual label in list
                bool found_invalid = false;
                for (uint32_t label_index = 0; label_index < callbackData->sessionLabelCount; ++label_index) {
                    if ((strcmp(callbackData->sessionLabels[label_index].labelName, g_first_individual_label_name) == 0) ||
                        (strcmp(callbackData->sessionLabels[label_index].labelName, g_second_individual_label_name) == 0) ||
                        (strcmp(callbackData->sessionLabels[label_index].labelName, g_third_individual_label_name) == 0)) {
                        found_invalid = true;
                    }
                }
                if (found_invalid) {
                    g_captured_only_expected_labels = false;
                }
            }
            if (g_individual_label_num == 1) {
                // Should have g_first_individual_label_name in list
                // Should not have g_second_individual_label_name in list
                // Should not have g_third_individual_label_name in list
                bool found_invalid = false;
                bool found_valid = false;
                for (uint32_t label_index = 0; label_index < callbackData->sessionLabelCount; ++label_index) {
                    if ((strcmp(callbackData->sessionLabels[label_index].labelName, g_second_individual_label_name) == 0) ||
                        (strcmp(callbackData->sessionLabels[label_index].labelName, g_third_individual_label_name) == 0)) {
                        found_invalid = true;
                    }
                    if (strcmp(callbackData->sessionLabels[label_index].labelName, g_first_individual_label_name) == 0) {
                        found_valid = true;
                    }
                }
                if (found_invalid || !found_valid) {
                    g_captured_only_expected_labels = false;
                }
            } else if (g_individual_label_num == 2) {
                // Should not have g_first_individual_label_name in list
                // Should have g_second_individual_label_name in list
                // Should not have g_third_individual_label_name in list
                bool found_invalid = false;
                bool found_valid = false;
                for (uint32_t label_index = 0; label_index < callbackData->sessionLabelCount; ++label_index) {
                    if ((strcmp(callbackData->sessionLabels[label_index].labelName, g_first_individual_label_name) == 0) ||
                        (strcmp(callbackData->sessionLabels[label_index].labelName, g_third_individual_label_name) == 0)) {
                        found_invalid = true;
                    }
                    if (strcmp(callbackData->sessionLabels[label_index].labelName, g_second_individual_label_name) == 0) {
                        found_valid = true;
                    }
                }
                if (found_invalid || !found_valid) {
                    g_captured_only_expected_labels = false;
                }
            } else if (g_individual_label_num == 3) {
                // Should not have g_first_individual_label_name in list
                // Should not have g_second_individual_label_name in list
                // Should have g_third_individual_label_name in list
                bool found_invalid = false;
                bool found_valid = false;
                for (uint32_t label_index = 0; label_index < callbackData->sessionLabelCount; ++label_index) {
                    if ((strcmp(callbackData->sessionLabels[label_index].labelName, g_first_individual_label_name) == 0) ||
                        (strcmp(callbackData->sessionLabels[label_index].labelName, g_second_individual_label_name) == 0)) {
                        found_invalid = true;
                    }
                    if (strcmp(callbackData->sessionLabels[label_index].labelName, g_third_individual_label_name) == 0) {
                        found_valid = true;
                    }
                }
                if (found_invalid || !found_valid) {
                    g_captured_only_expected_labels = false;
                }
            }
        }
    }

    return XR_FALSE;
}

// Test the EXT_debug_utils extension.  Just to make sure it properly triggers at least one callback item
DEFINE_TEST(TestDebugUtils) {
    INIT_TEST(TestDebugUtils)

    try {
        std::string current_path;
        std::string sample_impl_runtime_path;
        std::string layer_path;

        if (!FileSysUtilsGetCurrentPath(current_path) || !FileSysUtilsCombinePaths(current_path, "../../api_layers", layer_path)) {
            TEST_FAIL("Unable to set API layer path")
            TEST_REPORT(TestDebugUtils)
            return;
        }

        LoaderTestUnsetEnvironmentVariable("XR_RUNTIME_JSON");
        LoaderTestSetEnvironmentVariable("XR_API_LAYER_PATH", layer_path);

        XrInstance instance = XR_NULL_HANDLE;

        XrApplicationInfo app_info = {};
        strcpy(app_info.applicationName, "Loader Test");
        app_info.applicationVersion = 688;
        strcpy(app_info.engineName, "Infinite Improbability Drive");
        app_info.engineVersion = 42;
        app_info.apiVersion = XR_CURRENT_API_VERSION;

        XrInstanceCreateInfo instance_ci = {};
        instance_ci.type = XR_TYPE_INSTANCE_CREATE_INFO;
        instance_ci.applicationInfo = app_info;

        char dbg_ext_name[] = XR_EXT_DEBUG_UTILS_EXTENSION_NAME;
        std::string gfx_name("None");
        const char* ext_array[2];
        instance_ci.type = XR_TYPE_INSTANCE_CREATE_INFO;
        instance_ci.enabledExtensionCount = 2;
        instance_ci.enabledExtensionNames = ext_array;

#ifdef XR_USE_GRAPHICS_API_VULKAN
        if (g_graphics_api_to_use == GRAPHICS_API_VULKAN) {
            gfx_name = XR_KHR_VULKAN_ENABLE_EXTENSION_NAME;
        }
#endif  // XR_USE_GRAPHICS_API_VULKAN
#ifdef XR_USE_GRAPHICS_API_OPENGL
        if (g_graphics_api_to_use == GRAPHICS_API_OPENGL) {
            gfx_name = XR_KHR_OPENGL_ENABLE_EXTENSION_NAME;
        }
#endif  // XR_USE_GRAPHICS_API_OPENGL
#ifdef XR_USE_GRAPHICS_API_D3D11
        if (g_graphics_api_to_use == GRAPHICS_API_D3D) {
            gfx_name = XR_KHR_D3D11_ENABLE_EXTENSION_NAME;
        }
#endif  // XR_USE_GRAPHICS_API_D3D11

        ext_array[0] = dbg_ext_name;
        ext_array[1] = gfx_name.c_str();
        instance_ci.enabledExtensionNames = ext_array;

        // Create an instance with the appropriate data for the debug utils messenger
        XrDebugUtilsMessengerCreateInfoEXT dbg_msg_ci = {};
        dbg_msg_ci.type = XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        dbg_msg_ci.messageSeverities =
            XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
        dbg_msg_ci.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                  XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbg_msg_ci.userCallback = TestDebugUtilsCallback;

        // Create an instacne with the general information setup for a message
        XrDebugUtilsMessengerCallbackDataEXT callback_data = {};
        callback_data.type = XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
        callback_data.messageId = &message_id[0];
        callback_data.functionName = &test_function_name[0];
        callback_data.message = &test_message[0];
        callback_data.objectCount = 0;
        callback_data.objects = nullptr;
        callback_data.sessionLabelCount = 0;
        callback_data.sessionLabels = nullptr;

        // Path 1 - Create/Destroy with xrCreateInstance/xrDestroyInstance

        // Reset the capture values
        intptr_t def_int_value = 0xBADDECAF;
        expected_user_value = reinterpret_cast<void*>(def_int_value);
        captured_error = false;
        captured_warning = false;
        captured_info = false;
        captured_verbose = false;
        user_data_correct = false;
        captured_general_message = false;
        captured_validation_message = false;
        captured_performance_message = false;
        function_matches = false;
        message_matches = false;
        message_id_matches = false;
        num_objects_matches = false;
        num_objects = 0;
        dbg_msg_ci.userData = expected_user_value;

        instance_ci.next = reinterpret_cast<const void*>(&dbg_msg_ci);

        TEST_EQUAL(xrCreateInstance(&instance_ci, &instance), XR_SUCCESS, "Creating instance with next messenger creation")

        // Get a function pointer to the submit function to test
        PFN_xrSubmitDebugUtilsMessageEXT pfn_submit_dmsg;
        TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrSubmitDebugUtilsMessageEXT",
                                         reinterpret_cast<PFN_xrVoidFunction*>(&pfn_submit_dmsg)),
                   XR_SUCCESS, "TestDebugUtils get xrSubmitDebugUtilsMessageEXT function pointer");
        TEST_NOT_EQUAL(pfn_submit_dmsg, nullptr, "TestDebugUtils invalid xrSubmitDebugUtilsMessageEXT function pointer");

        // Test the various items
        strcpy(message_id, "General Error");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                        &callback_data);
        TEST_EQUAL(captured_error, true, "TestDebugUtils - General Error : Captured Error")
        TEST_EQUAL(captured_general_message, true, "TestDebugUtils - General Error : Captured General")
        TEST_EQUAL(user_data_correct, true, "TestDebugUtils - General Error : User Data Correct")
        TEST_EQUAL(function_matches, true, "TestDebugUtils - General Error : Function Matches")
        TEST_EQUAL(message_matches, true, "TestDebugUtils - General Error : Message Matches")
        TEST_EQUAL(message_id_matches, true, "TestDebugUtils - General Error : Message ID Matches")
        strcpy(message_id, "Validation Warning");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
                        &callback_data);
        TEST_EQUAL(captured_warning, true, "TestDebugUtils - Validation Warning : Captured Warning")
        TEST_EQUAL(captured_validation_message, true, "TestDebugUtils - Validation Warning : Captured Validation")
        TEST_EQUAL(user_data_correct, true, "TestDebugUtils - Validation Warning : User Data Correct")
        TEST_EQUAL(function_matches, true, "TestDebugUtils - Validation Warning : Function Matches")
        TEST_EQUAL(message_matches, true, "TestDebugUtils - Validation Warning : Message Matches")
        TEST_EQUAL(message_id_matches, true, "TestDebugUtils - Validation Warning : Message ID Matches")
        strcpy(message_id, "Performance Info");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                        &callback_data);
        TEST_EQUAL(captured_info, true, "TestDebugUtils - Performance Info : Captured Info")
        TEST_EQUAL(captured_performance_message, true, "TestDebugUtils - Performance Info : Captured Performance")
        TEST_EQUAL(user_data_correct, true, "TestDebugUtils - Performance Info : User Data Correct")
        TEST_EQUAL(function_matches, true, "TestDebugUtils - Performance Info : Function Matches")
        TEST_EQUAL(message_matches, true, "TestDebugUtils - Performance Info : Message Matches")
        TEST_EQUAL(message_id_matches, true, "TestDebugUtils - Performance Info : Message ID Matches")
        strcpy(message_id, "General Verbose");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                        &callback_data);
        TEST_EQUAL(captured_verbose, true, "TestDebugUtils - General Verbose : Captured Verbose")
        TEST_EQUAL(user_data_correct, true, "TestDebugUtils - General Verbose : User Data Correct")
        TEST_EQUAL(function_matches, true, "TestDebugUtils - General Verbose : Function Matches")
        TEST_EQUAL(message_matches, true, "TestDebugUtils - General Verbose : Message Matches")
        TEST_EQUAL(message_id_matches, true, "TestDebugUtils - General Verbose : Message ID Matches")

        TEST_EQUAL(xrDestroyInstance(instance), XR_SUCCESS, "Destroying instance with next messenger creation")

        // Path 2 - Create/Destroy with explicit call (xrCreateDebugUtilsMessengerEXT/xrDestroyDebugUtilsMessengerEXT)

        // Reset the capture values
        def_int_value = 0xDECAFBAD;
        expected_user_value = reinterpret_cast<void*>(def_int_value);
        captured_error = false;
        captured_warning = false;
        captured_info = false;
        captured_verbose = false;
        user_data_correct = false;
        captured_general_message = false;
        captured_validation_message = false;
        captured_performance_message = false;
        function_matches = false;
        message_matches = false;
        message_id_matches = false;
        num_objects_matches = false;
        num_objects = 0;
        dbg_msg_ci.userData = expected_user_value;
        instance_ci.next = nullptr;

        TEST_EQUAL(xrCreateInstance(&instance_ci, &instance), XR_SUCCESS, "Creating instance for separate create/destroy messenger")

        // Get a function pointer to the various debug utils functions to test
        PFN_xrCreateDebugUtilsMessengerEXT pfn_create_debug_utils_messager_ext;
        PFN_xrDestroyDebugUtilsMessengerEXT pfn_destroy_debug_utils_messager_ext;
        TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrCreateDebugUtilsMessengerEXT",
                                         reinterpret_cast<PFN_xrVoidFunction*>(&pfn_create_debug_utils_messager_ext)),
                   XR_SUCCESS, "TestDebugUtils get xrCreateDebugUtilsMessengerEXT function pointer");
        TEST_NOT_EQUAL(pfn_create_debug_utils_messager_ext, nullptr,
                       "TestDebugUtils invalid xrCreateDebugUtilsMessengerEXT function pointer");
        TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrDestroyDebugUtilsMessengerEXT",
                                         reinterpret_cast<PFN_xrVoidFunction*>(&pfn_destroy_debug_utils_messager_ext)),
                   XR_SUCCESS, "TestDebugUtils get xrDestroyDebugUtilsMessengerEXT function pointer");
        TEST_NOT_EQUAL(pfn_destroy_debug_utils_messager_ext, nullptr,
                       "TestDebugUtils invalid xrDestroyDebugUtilsMessengerEXT function pointer");
        TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrSubmitDebugUtilsMessageEXT",
                                         reinterpret_cast<PFN_xrVoidFunction*>(&pfn_submit_dmsg)),
                   XR_SUCCESS, "TestDebugUtils get xrSubmitDebugUtilsMessageEXT function pointer");
        TEST_NOT_EQUAL(pfn_submit_dmsg, nullptr, "TestDebugUtils invalid xrSubmitDebugUtilsMessageEXT function pointer");

        // Create the debug utils messenger
        XrDebugUtilsMessengerEXT debug_utils_messenger = XR_NULL_HANDLE;
        TEST_EQUAL(pfn_create_debug_utils_messager_ext(instance, &dbg_msg_ci, &debug_utils_messenger), XR_SUCCESS,
                   "Creating the debug utils messenger")

        // Test the various items
        strcpy(message_id, "General Error");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                        &callback_data);
        TEST_EQUAL(captured_error, true, "TestDebugUtils - General Error : Captured Error")
        TEST_EQUAL(captured_general_message, true, "TestDebugUtils - General Error : Captured General")
        TEST_EQUAL(user_data_correct, true, "TestDebugUtils - General Error : User Data Correct")
        TEST_EQUAL(function_matches, true, "TestDebugUtils - General Error : Function Matches")
        TEST_EQUAL(message_matches, true, "TestDebugUtils - General Error : Message Matches")
        TEST_EQUAL(message_id_matches, true, "TestDebugUtils - General Error : Message ID Matches")
        TEST_EQUAL(num_objects_matches, true, "TestDebugUtils - General Error : Number of Objects Matches")
        strcpy(message_id, "Validation Warning");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
                        &callback_data);
        TEST_EQUAL(captured_warning, true, "TestDebugUtils - Validation Warning : Captured Warning")
        TEST_EQUAL(captured_validation_message, true, "TestDebugUtils - Validation Warning : Captured Validation")
        TEST_EQUAL(user_data_correct, true, "TestDebugUtils - Validation Warning : User Data Correct")
        TEST_EQUAL(function_matches, true, "TestDebugUtils - Validation Warning : Function Matches")
        TEST_EQUAL(message_matches, true, "TestDebugUtils - Validation Warning : Message Matches")
        TEST_EQUAL(message_id_matches, true, "TestDebugUtils - Validation Warning : Message ID Matches")
        TEST_EQUAL(num_objects_matches, true, "TestDebugUtils - Validation Warning : Number of Objects Matches")
        strcpy(message_id, "Performance Info");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                        &callback_data);
        TEST_EQUAL(captured_info, true, "TestDebugUtils - Performance Info : Captured Info")
        TEST_EQUAL(captured_performance_message, true, "TestDebugUtils - Performance Info : Captured Performance")
        TEST_EQUAL(user_data_correct, true, "TestDebugUtils - Performance Info : User Data Correct")
        TEST_EQUAL(function_matches, true, "TestDebugUtils - Performance Info : Function Matches")
        TEST_EQUAL(message_matches, true, "TestDebugUtils - Performance Info : Message Matches")
        TEST_EQUAL(message_id_matches, true, "TestDebugUtils - Performance Info : Message ID Matches")
        TEST_EQUAL(num_objects_matches, true, "TestDebugUtils - Performance Info : Number of Objects Matches")
        strcpy(message_id, "General Verbose");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                        &callback_data);
        TEST_EQUAL(captured_verbose, true, "TestDebugUtils - General Verbose : Captured Verbose")
        TEST_EQUAL(user_data_correct, true, "TestDebugUtils - General Verbose : User Data Correct")
        TEST_EQUAL(function_matches, true, "TestDebugUtils - General Verbose : Function Matches")
        TEST_EQUAL(message_matches, true, "TestDebugUtils - General Verbose : Message Matches")
        TEST_EQUAL(message_id_matches, true, "TestDebugUtils - General Verbose : Message ID Matches")
        TEST_EQUAL(num_objects_matches, true, "TestDebugUtils - General Verbose : Number of Objects Matches")

        // Destroy what we created
        TEST_EQUAL(pfn_destroy_debug_utils_messager_ext(debug_utils_messenger), XR_SUCCESS, "Destroying the debug utils messenger")

        TEST_EQUAL(xrDestroyInstance(instance), XR_SUCCESS, "Destroying instance for separate create/destroy messenger")

        // Path 3 - Make sure appropriate messages only received when registered.

        // Reset the capture values
        def_int_value = 0xDECAFBAD;
        expected_user_value = reinterpret_cast<void*>(def_int_value);
        captured_error = false;
        captured_warning = false;
        captured_info = false;
        captured_verbose = false;
        user_data_correct = false;
        captured_general_message = false;
        captured_validation_message = false;
        captured_performance_message = false;
        function_matches = false;
        message_matches = false;
        message_id_matches = false;
        num_objects_matches = false;
        num_objects = 0;
        dbg_msg_ci.userData = expected_user_value;
        instance_ci.next = nullptr;

        TEST_EQUAL(xrCreateInstance(&instance_ci, &instance), XR_SUCCESS,
                   "Creating instance for individual message type/severity testing")

        // Get a function pointer to the various debug utils functions to test
        TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrCreateDebugUtilsMessengerEXT",
                                         reinterpret_cast<PFN_xrVoidFunction*>(&pfn_create_debug_utils_messager_ext)),
                   XR_SUCCESS, "TestDebugUtils get xrCreateDebugUtilsMessengerEXT function pointer");
        TEST_NOT_EQUAL(pfn_create_debug_utils_messager_ext, nullptr,
                       "TestDebugUtils invalid xrCreateDebugUtilsMessengerEXT function pointer");
        TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrDestroyDebugUtilsMessengerEXT",
                                         reinterpret_cast<PFN_xrVoidFunction*>(&pfn_destroy_debug_utils_messager_ext)),
                   XR_SUCCESS, "TestDebugUtils get xrDestroyDebugUtilsMessengerEXT function pointer");
        TEST_NOT_EQUAL(pfn_destroy_debug_utils_messager_ext, nullptr,
                       "TestDebugUtils invalid xrDestroyDebugUtilsMessengerEXT function pointer");
        TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrSubmitDebugUtilsMessageEXT",
                                         reinterpret_cast<PFN_xrVoidFunction*>(&pfn_submit_dmsg)),
                   XR_SUCCESS, "TestDebugUtils get xrSubmitDebugUtilsMessageEXT function pointer");
        TEST_NOT_EQUAL(pfn_submit_dmsg, nullptr, "TestDebugUtils invalid xrSubmitDebugUtilsMessageEXT function pointer");

        // Create the debug utils messenger, but only to receive general error messages
        debug_utils_messenger = XR_NULL_HANDLE;
        dbg_msg_ci.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbg_msg_ci.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;

        TEST_EQUAL(pfn_create_debug_utils_messager_ext(instance, &dbg_msg_ci, &debug_utils_messenger), XR_SUCCESS,
                   "Creating the debug utils messenger for general error testing")

        strcpy(message_id, "General Error");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                        &callback_data);
        strcpy(message_id, "Validation Warning");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
                        &callback_data);
        strcpy(message_id, "Performance Info");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                        &callback_data);
        strcpy(message_id, "General Verbose");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                        &callback_data);
        TEST_EQUAL(captured_error, true, "TestDebugUtils - Only General Error : Captured Error")
        TEST_EQUAL(captured_warning, false, "TestDebugUtils - Only General Error : Captured Warning")
        TEST_EQUAL(captured_info, false, "TestDebugUtils - Only General Error : Captured Info")
        TEST_EQUAL(captured_verbose, false, "TestDebugUtils - Only General Error : Captured Verbose")
        TEST_EQUAL(captured_general_message, true, "TestDebugUtils - Only General Error : Captured General")
        TEST_EQUAL(captured_validation_message, false, "TestDebugUtils - Only General Error : Captured Validation")
        TEST_EQUAL(captured_performance_message, false, "TestDebugUtils - Only General Error : Captured Performance")
        TEST_EQUAL(user_data_correct, true, "TestDebugUtils - Only General Error : User Data Correct")
        TEST_EQUAL(function_matches, true, "TestDebugUtils - Only General Error : Function Matches")
        TEST_EQUAL(message_matches, true, "TestDebugUtils - Only General Error : Message Matches")
        TEST_EQUAL(message_id_matches, true, "TestDebugUtils - Only General Error : Message ID Matches")
        TEST_EQUAL(num_objects_matches, true, "TestDebugUtils - Only General Error : Number of Objects Matches")

        // Destroy what we created
        TEST_EQUAL(pfn_destroy_debug_utils_messager_ext(debug_utils_messenger), XR_SUCCESS,
                   "Destroying the debug utils messenger for general error testing")

        // Reset the items we tested above.
        captured_error = false;
        captured_general_message = false;

        // Create the debug utils messenger, but only to receive validation warning messages
        debug_utils_messenger = XR_NULL_HANDLE;
        dbg_msg_ci.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        dbg_msg_ci.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;

        TEST_EQUAL(pfn_create_debug_utils_messager_ext(instance, &dbg_msg_ci, &debug_utils_messenger), XR_SUCCESS,
                   "Creating the debug utils messenger for validation warning testing")

        strcpy(message_id, "General Error");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                        &callback_data);
        strcpy(message_id, "Validation Warning");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
                        &callback_data);
        strcpy(message_id, "Performance Info");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                        &callback_data);
        strcpy(message_id, "General Verbose");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                        &callback_data);
        TEST_EQUAL(captured_error, false, "TestDebugUtils - Only Validation Warning : Captured Error")
        TEST_EQUAL(captured_warning, true, "TestDebugUtils - Only Validation Warning : Captured Warning")
        TEST_EQUAL(captured_info, false, "TestDebugUtils - Only Validation Warning : Captured Info")
        TEST_EQUAL(captured_verbose, false, "TestDebugUtils - Only Validation Warning : Captured Verbose")
        TEST_EQUAL(captured_general_message, false, "TestDebugUtils - Only Validation Warning : Captured General")
        TEST_EQUAL(captured_validation_message, true, "TestDebugUtils - Only Validation Warning : Captured Validation")
        TEST_EQUAL(captured_performance_message, false, "TestDebugUtils - Only Validation Warning : Captured Performance")
        TEST_EQUAL(user_data_correct, true, "TestDebugUtils - Only Validation Warning : User Data Correct")
        TEST_EQUAL(function_matches, true, "TestDebugUtils - Only Validation Warning : Function Matches")
        TEST_EQUAL(message_matches, true, "TestDebugUtils - Only Validation Warning : Message Matches")
        TEST_EQUAL(message_id_matches, true, "TestDebugUtils - Only Validation Warning : Message ID Matches")
        TEST_EQUAL(num_objects_matches, true, "TestDebugUtils - Only Validation Warning : Number of Objects Matches")

        // Destroy what we created
        TEST_EQUAL(pfn_destroy_debug_utils_messager_ext(debug_utils_messenger), XR_SUCCESS,
                   "Destroying the debug utils messenger for validation warning testing")

        // Reset the items we tested above.
        captured_warning = false;
        captured_validation_message = false;

        // Create the debug utils messenger, but only to receive validation warning messages
        debug_utils_messenger = XR_NULL_HANDLE;
        dbg_msg_ci.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
        dbg_msg_ci.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        TEST_EQUAL(pfn_create_debug_utils_messager_ext(instance, &dbg_msg_ci, &debug_utils_messenger), XR_SUCCESS,
                   "Creating the debug utils messenger for performance verbose testing")

        strcpy(message_id, "General Error");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                        &callback_data);
        strcpy(message_id, "Validation Warning");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
                        &callback_data);
        strcpy(message_id, "Performance Info");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                        &callback_data);
        strcpy(message_id, "General Verbose");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                        &callback_data);
        TEST_EQUAL(captured_error, false, "TestDebugUtils - Only Performance Verbose : Captured Error")
        TEST_EQUAL(captured_warning, false, "TestDebugUtils - Only Performance Verbose : Captured Warning")
        TEST_EQUAL(captured_info, false, "TestDebugUtils - Only Performance Verbose : Captured Info")
        TEST_EQUAL(captured_verbose, false, "TestDebugUtils - Only Performance Verbose : Captured Verbose")
        TEST_EQUAL(captured_general_message, false, "TestDebugUtils - Only Performance Verbose : Captured General")
        TEST_EQUAL(captured_validation_message, false, "TestDebugUtils - Only Performance Verbose : Captured Validation")
        TEST_EQUAL(captured_verbose, false, "TestDebugUtils - Only Performance Verbose : Captured Verbose")
        TEST_EQUAL(captured_performance_message, false, "TestDebugUtils - Only Performance Verbose : Captured Performance")
        TEST_EQUAL(user_data_correct, true, "TestDebugUtils - Only Performance Verbose : User Data Correct")
        TEST_EQUAL(function_matches, true, "TestDebugUtils - Only Performance Verbose : Function Matches")
        TEST_EQUAL(message_matches, true, "TestDebugUtils - Only Performance Verbose : Message Matches")
        TEST_EQUAL(message_id_matches, true, "TestDebugUtils - Only Performance Verbose : Message ID Matches")
        TEST_EQUAL(num_objects_matches, true, "TestDebugUtils - Only Performance Verbose : Number of Objects Matches")
        strcpy(message_id, "Performance Verbose");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                        &callback_data);
        TEST_EQUAL(captured_verbose, true, "TestDebugUtils - Only Performance Verbose : Captured Verbose")
        TEST_EQUAL(captured_performance_message, true, "TestDebugUtils - Only Performance Verbose : Captured Performance")

        // Destroy what we created
        TEST_EQUAL(pfn_destroy_debug_utils_messager_ext(debug_utils_messenger), XR_SUCCESS,
                   "Destroying the debug utils messenger for performance verbose testing")

        // Reset the items we tested above.
        captured_verbose = false;
        captured_performance_message = false;

        // Create the debug utils messenger, but only to receive validation warning messages
        debug_utils_messenger = XR_NULL_HANDLE;
        dbg_msg_ci.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
        dbg_msg_ci.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;

        TEST_EQUAL(pfn_create_debug_utils_messager_ext(instance, &dbg_msg_ci, &debug_utils_messenger), XR_SUCCESS,
                   "Creating the debug utils messenger for validation info testing")

        strcpy(message_id, "General Error");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                        &callback_data);
        strcpy(message_id, "Validation Warning");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
                        &callback_data);
        strcpy(message_id, "Performance Info");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                        &callback_data);
        strcpy(message_id, "General Verbose");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                        &callback_data);
        TEST_EQUAL(captured_error, false, "TestDebugUtils - Only Validation Info : Captured Error")
        TEST_EQUAL(captured_warning, false, "TestDebugUtils - Only Validation Info : Captured Warning")
        TEST_EQUAL(captured_info, false, "TestDebugUtils - Only Validation Info : Captured Info")
        TEST_EQUAL(captured_verbose, false, "TestDebugUtils - Only Validation Info : Captured Verbose")
        TEST_EQUAL(captured_general_message, false, "TestDebugUtils - Only Validation Info : Captured General")
        TEST_EQUAL(captured_validation_message, false, "TestDebugUtils - Only Validation Info : Captured Validation")
        TEST_EQUAL(captured_performance_message, false, "TestDebugUtils - Only Validation Info : Captured Performance")
        TEST_EQUAL(user_data_correct, true, "TestDebugUtils - Only Validation Info : User Data Correct")
        TEST_EQUAL(function_matches, true, "TestDebugUtils - Only Validation Info : Function Matches")
        TEST_EQUAL(message_matches, true, "TestDebugUtils - Only Validation Info : Message Matches")
        TEST_EQUAL(message_id_matches, true, "TestDebugUtils - Only Validation Info : Message ID Matches")
        TEST_EQUAL(num_objects_matches, true, "TestDebugUtils - Only Validation Info : Number of Objects Matches")
        strcpy(message_id, "Validation Info");
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
                        &callback_data);
        TEST_EQUAL(captured_info, true, "TestDebugUtils - Only Validation Info : Captured Info")
        TEST_EQUAL(captured_validation_message, true, "TestDebugUtils - Only Validation Info : Captured Validation")

        // Destroy what we created
        TEST_EQUAL(pfn_destroy_debug_utils_messager_ext(debug_utils_messenger), XR_SUCCESS,
                   "Destroying the debug utils messenger for validation info testing")

        // Reset the items we tested above.
        captured_info = false;
        captured_validation_message = false;

        // Path 4 - Test objects

        // Create the debug utils messenger, but only to receive validation warning messages
        debug_utils_messenger = XR_NULL_HANDLE;
        dbg_msg_ci.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
        dbg_msg_ci.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        TEST_EQUAL(pfn_create_debug_utils_messager_ext(instance, &dbg_msg_ci, &debug_utils_messenger), XR_SUCCESS,
                   "Creating the debug utils messenger for number of objects testing")

        num_objects_matches = false;
        object_contents_match = false;

        num_objects = 3;
        callback_data.objectCount = static_cast<uint8_t>(num_objects);
        XrDebugUtilsObjectNameInfoEXT objects[3];
        objects[0].type = XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        objects[0].next = nullptr;
        objects[0].objectType = XR_OBJECT_TYPE_INSTANCE;
        objects[0].objectHandle = static_cast<uint64_t>(0xFEED0001);
        objects[0].objectName = nullptr;
        objects[1].type = XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        objects[1].next = nullptr;
        objects[1].objectType = XR_OBJECT_TYPE_SESSION;
        objects[1].objectHandle = static_cast<uint64_t>(0xFEED0002);
        objects[1].objectName = nullptr;
        objects[2].type = XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        objects[2].next = nullptr;
        objects[2].objectType = XR_OBJECT_TYPE_ACTION_SET;
        objects[2].objectHandle = static_cast<uint64_t>(0xFEED0003);
        objects[2].objectName = nullptr;
        callback_data.objects = &objects[0];
        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                        &callback_data);
        TEST_EQUAL(num_objects_matches, true, "TestDebugUtils - Number of Objects Matches")
        TEST_EQUAL(object_contents_match, true, "TestDebugUtils - Object Content Matches")
        num_objects_matches = false;
        object_contents_match = false;

        // Destroy what we created
        TEST_EQUAL(pfn_destroy_debug_utils_messager_ext(debug_utils_messenger), XR_SUCCESS,
                   "Destroying the debug utils messenger for number of objects testing")
        xrDestroyInstance(instance);

        // Path 5 - Test object names

        // Need a graphics binding for these tests
        std::string gfx_bind_name;
        const char* ext_name_array[2];
        switch (g_graphics_api_to_use) {
            case GRAPHICS_API_D3D:
                gfx_name = "XR_KHR_D3D11_enable";
                break;
            case GRAPHICS_API_VULKAN:
                gfx_name = "XR_KHR_vulkan_enable";
                break;
            case GRAPHICS_API_OPENGL:
                gfx_name = "XR_KHR_opengl_enable";
                break;
            default:
                TEST_FAIL("Unable to continue - no graphics binding extension found")
                TEST_REPORT(TestDebugUtils)
                return;
        }

        ext_name_array[0] = gfx_name.c_str();
        ext_name_array[1] = dbg_ext_name;

        instance_ci.enabledExtensionNames = ext_name_array;
        instance_ci.enabledExtensionCount = 2;

        TEST_EQUAL(xrCreateInstance(&instance_ci, &instance), XR_SUCCESS, "Creating instance for number of objects testing")

        // Get a function pointer to the various debug utils functions to test
        TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrCreateDebugUtilsMessengerEXT",
                                         reinterpret_cast<PFN_xrVoidFunction*>(&pfn_create_debug_utils_messager_ext)),
                   XR_SUCCESS, "TestDebugUtils get xrCreateDebugUtilsMessengerEXT function pointer");
        TEST_NOT_EQUAL(pfn_create_debug_utils_messager_ext, nullptr,
                       "TestDebugUtils invalid xrCreateDebugUtilsMessengerEXT function pointer");
        TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrDestroyDebugUtilsMessengerEXT",
                                         reinterpret_cast<PFN_xrVoidFunction*>(&pfn_destroy_debug_utils_messager_ext)),
                   XR_SUCCESS, "TestDebugUtils get xrDestroyDebugUtilsMessengerEXT function pointer");
        TEST_NOT_EQUAL(pfn_destroy_debug_utils_messager_ext, nullptr,
                       "TestDebugUtils invalid xrDestroyDebugUtilsMessengerEXT function pointer");
        TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrSubmitDebugUtilsMessageEXT",
                                         reinterpret_cast<PFN_xrVoidFunction*>(&pfn_submit_dmsg)),
                   XR_SUCCESS, "TestDebugUtils get xrSubmitDebugUtilsMessageEXT function pointer");
        TEST_NOT_EQUAL(pfn_submit_dmsg, nullptr, "TestDebugUtils invalid xrSubmitDebugUtilsMessageEXT function pointer");

        // Create the debug utils messenger, but only to receive validation warning messages
        debug_utils_messenger = XR_NULL_HANDLE;
        dbg_msg_ci.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
        dbg_msg_ci.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        TEST_EQUAL(pfn_create_debug_utils_messager_ext(instance, &dbg_msg_ci, &debug_utils_messenger), XR_SUCCESS,
                   "Creating the debug utils messenger for session labels testing")

        num_objects_matches = false;
        object_contents_match = false;

        PFN_xrSetDebugUtilsObjectNameEXT pfn_set_obj_name;
        TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrSetDebugUtilsObjectNameEXT",
                                         reinterpret_cast<PFN_xrVoidFunction*>(&pfn_set_obj_name)),
                   XR_SUCCESS, "TestDebugUtils get xrSetDebugUtilsObjectNameEXT function pointer");
        TEST_NOT_EQUAL(pfn_set_obj_name, nullptr, "TestDebugUtils invalid xrSetDebugUtilsObjectNameEXT function pointer");
        strcpy(object_name, "My Instance Obj");
        object_handle = MakeHandleGeneric(instance);
        objects[0].type = XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        objects[0].next = nullptr;
        objects[0].objectType = XR_OBJECT_TYPE_INSTANCE;
        objects[0].objectHandle = object_handle;
        objects[0].objectName = &object_name[0];
        TEST_EQUAL(pfn_set_obj_name(instance, &objects[0]), XR_SUCCESS, "Setting object name")
        num_objects = 1;
        callback_data.objectCount = static_cast<uint8_t>(num_objects);

        pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                        &callback_data);
        TEST_EQUAL(num_objects_matches, true, "TestDebugUtils - Name Set: Objects Matches")
        TEST_EQUAL(object_contents_match, true, "TestDebugUtils - Name Set: Object Content Matches")
        num_objects_matches = false;
        object_contents_match = false;

        XrSystemGetInfo system_get_info;
        memset(&system_get_info, 0, sizeof(system_get_info));
        system_get_info.type = XR_TYPE_SYSTEM_GET_INFO;
        system_get_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        XrSystemId systemId;
        TEST_EQUAL(xrGetSystem(instance, &system_get_info, &systemId), XR_SUCCESS, "xrGetSystem");

        if (systemId != XR_NULL_SYSTEM_ID) {
            void* graphics_binding = nullptr;

#ifdef XR_USE_GRAPHICS_API_VULKAN
            XrGraphicsBindingVulkanKHR vulkan_graphics_binding = {};
            if (g_graphics_api_to_use == GRAPHICS_API_VULKAN) {
                PFN_xrGetVulkanInstanceExtensionsKHR pfn_get_vulkan_instance_extensions_khr;
                PFN_xrGetVulkanDeviceExtensionsKHR pfn_get_vulkan_device_extensions_khr;
                PFN_xrGetVulkanGraphicsDeviceKHR pfn_get_vulkan_graphics_device_khr;
                PFN_xrGetVulkanGraphicsRequirementsKHR pfn_get_vulkan_graphics_requirements_khr;
                TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrGetVulkanInstanceExtensionsKHR",
                                                 reinterpret_cast<PFN_xrVoidFunction*>(&pfn_get_vulkan_instance_extensions_khr)),
                           XR_SUCCESS, "TestDebugUtils get xrGetVulkanInstanceExtensionsKHR function pointer");
                TEST_NOT_EQUAL(pfn_get_vulkan_instance_extensions_khr, nullptr,
                               "TestDebugUtils invalid xrGetVulkanInstanceExtensionsKHR function pointer");
                TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrGetVulkanDeviceExtensionsKHR",
                                                 reinterpret_cast<PFN_xrVoidFunction*>(&pfn_get_vulkan_device_extensions_khr)),
                           XR_SUCCESS, "TestDebugUtils get xrGetVulkanDeviceExtensionsKHR function pointer");
                TEST_NOT_EQUAL(pfn_get_vulkan_device_extensions_khr, nullptr,
                               "TestDebugUtils invalid xrGetVulkanDeviceExtensionsKHR function pointer");
                TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsDeviceKHR",
                                                 reinterpret_cast<PFN_xrVoidFunction*>(&pfn_get_vulkan_graphics_device_khr)),
                           XR_SUCCESS, "TestDebugUtils get xrGetVulkanGraphicsDeviceKHR function pointer");
                TEST_NOT_EQUAL(pfn_get_vulkan_graphics_device_khr, nullptr,
                               "TestDebugUtils invalid xrGetVulkanGraphicsDeviceKHR function pointer");
                TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirementsKHR",
                                                 reinterpret_cast<PFN_xrVoidFunction*>(&pfn_get_vulkan_graphics_requirements_khr)),
                           XR_SUCCESS, "TestDebugUtils get xrGetVulkanGraphicsRequirementsKHR function pointer");
                TEST_NOT_EQUAL(pfn_get_vulkan_graphics_requirements_khr, nullptr,
                               "TestDebugUtils invalid xrGetVulkanGraphicsRequirementsKHR function pointer");

                XrGraphicsRequirementsVulkanKHR vulkan_graphics_requirements = {};
                TEST_EQUAL(pfn_get_vulkan_graphics_requirements_khr(instance, systemId, &vulkan_graphics_requirements), XR_SUCCESS,
                           "TestDebugUtils calling xrGetVulkanGraphicsRequirementsKHR");

                vulkan_graphics_binding.type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
                // TODO: Just need something other than NULL here for now (for validation).  Eventually need
                //       to correctly put in a valid pointer to an Display
                vulkan_graphics_binding.instance = VK_NULL_HANDLE;
                vulkan_graphics_binding.physicalDevice = VK_NULL_HANDLE;
                vulkan_graphics_binding.device = VK_NULL_HANDLE;
                vulkan_graphics_binding.queueFamilyIndex = 0;
                vulkan_graphics_binding.queueIndex = 0;
                graphics_binding = reinterpret_cast<void*>(&vulkan_graphics_binding);
            }
#endif  // XR_USE_GRAPHICS_API_VULKAN
#ifdef XR_USE_GRAPHICS_API_OPENGL
#ifdef _WIN32
            XrGraphicsBindingOpenGLWin32KHR win32_gl_graphics_binding = {};
#elif defined(OS_LINUX_XLIB)
            XrGraphicsBindingOpenGLXlibKHR glx_gl_graphics_binding = {};
#elif defined(OS_LINUX_XCB) || defined(OS_LINUX_XCB_GLX)
            XrGraphicsBindingOpenGLXcbKHR xcb_gl_graphics_binding = {};
#elif defined(OS_LINUX_WAYLAND)
            XrGraphicsBindingOpenGLWaylandKHR wayland_gl_graphics_binding = {};
#endif
            if (g_graphics_api_to_use == GRAPHICS_API_OPENGL) {
                PFN_xrGetOpenGLGraphicsRequirementsKHR pfn_get_opengl_graphics_requirements_khr;
                TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR",
                                                 reinterpret_cast<PFN_xrVoidFunction*>(&pfn_get_opengl_graphics_requirements_khr)),
                           XR_SUCCESS, "TestDebugUtils get xrGetOpenGLGraphicsRequirementsKHR function pointer");
                TEST_NOT_EQUAL(pfn_get_opengl_graphics_requirements_khr, nullptr,
                               "TestDebugUtils invalid xrGetOpenGLGraphicsRequirementsKHR function pointer");
                XrGraphicsRequirementsOpenGLKHR opengl_graphics_requirements = {};
                TEST_EQUAL(pfn_get_opengl_graphics_requirements_khr(instance, systemId, &opengl_graphics_requirements), XR_SUCCESS,
                           "TestDebugUtils calling xrGetOpenGLGraphicsRequirementsKHR");
#ifdef _WIN32
                win32_gl_graphics_binding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
                graphics_binding = reinterpret_cast<void*>(&win32_gl_graphics_binding);
#elif defined(OS_LINUX_XLIB)
                glx_gl_graphics_binding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR;
                // TODO: Just need something other than NULL here for now (for validation).  Eventually need
                //       to correctly put in a valid pointer to an Display
                glx_gl_graphics_binding.xDisplay = reinterpret_cast<Display*>(0xFFFFFFFF);
                graphics_binding = reinterpret_cast<void*>(&glx_gl_graphics_binding);
#elif defined(OS_LINUX_XCB) || defined(OS_LINUX_XCB_GLX)
                xcb_gl_graphics_binding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_XCB_KHR;
                // TODO: Just need something other than NULL here for now (for validation).  Eventually need
                //       to correctly put in a valid pointer to an xcb_connection_t
                xcb_gl_graphics_binding.connection = reinterpret_cast<xcb_connection_t*>(0xFFFFFFFF);
                graphics_binding = reinterpret_cast<void*>(&xcb_gl_graphics_binding);
#elif defined(OS_LINUX_WAYLAND)
                wayland_gl_graphics_binding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR;
                // TODO: Just need something other than NULL here for now (for validation).  Eventually need
                //       to correctly put in a valid pointer to an wl_display
                wayland_gl_graphics_binding.display = reinterpret_cast<wl_display*>(0xFFFFFFFF);
                graphics_binding = reinterpret_cast<void*>(&wayland_gl_graphics_binding);
#endif
            }
#endif  // XR_USE_GRAPHICS_API_OPENGL
#ifdef XR_USE_GRAPHICS_API_D3D11
            XrGraphicsBindingD3D11KHR d3d11_graphics_binding = {};
            if (g_graphics_api_to_use == GRAPHICS_API_D3D) {
                PFN_xrGetD3D11GraphicsRequirementsKHR pfn_get_d3d11_graphics_requirements_khr;
                TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrGetD3D11GraphicsRequirementsKHR",
                                                 reinterpret_cast<PFN_xrVoidFunction*>(&pfn_get_d3d11_graphics_requirements_khr)),
                           XR_SUCCESS, "TestDebugUtils get xrGetD3D11GraphicsRequirementsKHR function pointer");
                TEST_NOT_EQUAL(pfn_get_d3d11_graphics_requirements_khr, nullptr,
                               "TestDebugUtils invalid xrGetD3D11GraphicsRequirementsKHR function pointer");
                XrGraphicsRequirementsD3D11KHR d3d11_graphics_requirements = {XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
                TEST_EQUAL(pfn_get_d3d11_graphics_requirements_khr(instance, systemId, &d3d11_graphics_requirements), XR_SUCCESS,
                           "TestDebugUtils calling xrGetD3D11GraphicsRequirementsKHR");

                ID3D11Device* d3d_device;
                HRESULT res =
                    D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &d3d_device, NULL, NULL);
                TEST_EQUAL(res, S_OK, "TestDebugUtils creating D3D11 reference device")

                d3d11_graphics_binding.type = XR_TYPE_GRAPHICS_BINDING_D3D11_KHR;
                d3d11_graphics_binding.device = d3d_device;
                graphics_binding = reinterpret_cast<void*>(&d3d11_graphics_binding);

                d3d11_graphics_binding.type = XR_TYPE_GRAPHICS_BINDING_D3D11_KHR;
                // TODO: Just need something other than NULL here for now (for validation).  Eventually need
                //       to correctly put in a valid pointer to an Display
                d3d11_graphics_binding.device = d3d_device;
                graphics_binding = reinterpret_cast<void*>(&d3d11_graphics_binding);
            }
#endif  // XR_USE_GRAPHICS_API_D3D11

            // Get a function pointer to the various debug utils functions to test
            PFN_xrSessionBeginDebugUtilsLabelRegionEXT pfn_begin_debug_utils_label_region_ext;
            PFN_xrSessionEndDebugUtilsLabelRegionEXT pfn_end_debug_utils_label_region_ext;
            PFN_xrSessionInsertDebugUtilsLabelEXT pfn_insert_debug_utils_label_ext;
            TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrSessionBeginDebugUtilsLabelRegionEXT",
                                             reinterpret_cast<PFN_xrVoidFunction*>(&pfn_begin_debug_utils_label_region_ext)),
                       XR_SUCCESS, "TestDebugUtils get xrSessionBeginDebugUtilsLabelRegionEXT function pointer");
            TEST_NOT_EQUAL(pfn_begin_debug_utils_label_region_ext, nullptr,
                           "TestDebugUtils invalid xrSessionBeginDebugUtilsLabelRegionEXT function pointer");
            TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrSessionEndDebugUtilsLabelRegionEXT",
                                             reinterpret_cast<PFN_xrVoidFunction*>(&pfn_end_debug_utils_label_region_ext)),
                       XR_SUCCESS, "TestDebugUtils get xrSessionEndDebugUtilsLabelRegionEXT function pointer");
            TEST_NOT_EQUAL(pfn_end_debug_utils_label_region_ext, nullptr,
                           "TestDebugUtils invalid xrSessionEndDebugUtilsLabelRegionEXT function pointer");
            TEST_EQUAL(xrGetInstanceProcAddr(instance, "xrSessionInsertDebugUtilsLabelEXT",
                                             reinterpret_cast<PFN_xrVoidFunction*>(&pfn_insert_debug_utils_label_ext)),
                       XR_SUCCESS, "TestDebugUtils get xrSessionInsertDebugUtilsLabelEXT function pointer");
            TEST_NOT_EQUAL(pfn_insert_debug_utils_label_ext, nullptr,
                           "TestDebugUtils invalid xrSessionInsertDebugUtilsLabelEXT function pointer");

            g_expecting_labels = true;

            // Create a label struct for initial testing
            XrDebugUtilsLabelEXT first_label = {};
            first_label.type = XR_TYPE_DEBUG_UTILS_LABEL_EXT;
            first_label.labelName = g_first_individual_label_name;

            // Create a session for us to begin
            XrSession session;
            XrSessionCreateInfo session_create_info = {};
            session_create_info.type = XR_TYPE_SESSION_CREATE_INFO;
            session_create_info.systemId = systemId;
            session_create_info.next = graphics_binding;

            TEST_EQUAL(xrCreateSession(instance, &session_create_info, &session), XR_SUCCESS, "xrCreateSession")

            // Set it up to put in the session and instance to any debug utils messages
            num_objects = 2;
            callback_data.objectCount = static_cast<uint8_t>(num_objects);
            objects[0].type = XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            objects[0].next = nullptr;
            objects[0].objectType = XR_OBJECT_TYPE_INSTANCE;
            objects[0].objectHandle = MakeHandleGeneric(instance);
            objects[0].objectName = nullptr;
            objects[1].type = XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            objects[1].next = nullptr;
            objects[1].objectType = XR_OBJECT_TYPE_SESSION;
            objects[1].objectHandle = MakeHandleGeneric(session);
            objects[1].objectName = nullptr;
            callback_data.objects = &objects[0];

            // Try invalid session on each of the label functions
            TEST_EQUAL(pfn_begin_debug_utils_label_region_ext(XR_NULL_HANDLE, &first_label), XR_ERROR_HANDLE_INVALID,
                       "TestDebugUtils calling xrSessionBeginDebugUtilsLabelRegionEXT with invalid session");
            TEST_EQUAL(pfn_end_debug_utils_label_region_ext(XR_NULL_HANDLE), XR_ERROR_HANDLE_INVALID,
                       "TestDebugUtils calling xrSessionEndDebugUtilsLabelRegionEXT with invalid session");
            TEST_EQUAL(pfn_insert_debug_utils_label_ext(XR_NULL_HANDLE, &first_label), XR_ERROR_HANDLE_INVALID,
                       "TestDebugUtils calling xrSessionInsertDebugUtilsLabelEXT with invalid session");

            // Try with nullptr for the label
            TEST_EQUAL(pfn_begin_debug_utils_label_region_ext(session, nullptr), XR_ERROR_VALIDATION_FAILURE,
                       "TestDebugUtils calling xrSessionBeginDebugUtilsLabelRegionEXT with NULL label");
            TEST_EQUAL(pfn_insert_debug_utils_label_ext(session, nullptr), XR_ERROR_VALIDATION_FAILURE,
                       "TestDebugUtils calling xrSessionInsertDebugUtilsLabelEXT with NULL label");

            // Start an individual label
            TEST_EQUAL(pfn_insert_debug_utils_label_ext(session, &first_label), XR_SUCCESS,
                       "TestDebugUtils calling xrSessionInsertDebugUtilsLabelEXT");

            // Trigger a message and make sure we see "First individual label"
            g_in_region_num = 0;
            g_individual_label_num = 1;
            g_captured_labels = false;
            g_captured_only_expected_labels = false;
            strcpy(message_id, "First Individual Label");
            pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                            XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data);
            TEST_EQUAL(g_captured_labels, true, "TestDebugUtils - First Individual Label : Captured Labels")
            TEST_EQUAL(g_captured_only_expected_labels, true, "TestDebugUtils - First Individual Label : Captured Correct Labels")

            // Begin a label region
            first_label.labelName = g_first_label_region_name;
            TEST_EQUAL(pfn_begin_debug_utils_label_region_ext(session, &first_label), XR_SUCCESS,
                       "TestDebugUtils calling xrSessionBeginDebugUtilsLabelRegionEXT");

            // Trigger a message and make sure we see "Label Region" and not "First individual label"
            g_in_region_num = 1;
            g_individual_label_num = 0;
            g_captured_labels = false;
            g_captured_only_expected_labels = false;
            strcpy(message_id, "First Label Region");
            pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                            XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data);
            TEST_EQUAL(g_captured_labels, true, "TestDebugUtils - First Label Region : Captured Labels")
            TEST_EQUAL(g_captured_only_expected_labels, true, "TestDebugUtils - First Label Region : Captured Correct Labels")

            // Begin the session now.
            XrSessionBeginInfo session_begin_info = {};
            session_begin_info.type = XR_TYPE_SESSION_BEGIN_INFO;
            session_begin_info.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

            TEST_EQUAL(xrBeginSession(session, &session_begin_info), XR_SUCCESS, "xrBeginSession")

            XrDebugUtilsLabelEXT individual_label = {};
            individual_label.type = XR_TYPE_DEBUG_UTILS_LABEL_EXT;
            individual_label.labelName = g_second_individual_label_name;
            TEST_EQUAL(pfn_insert_debug_utils_label_ext(session, &individual_label), XR_SUCCESS,
                       "TestDebugUtils calling xrSessionInsertDebugUtilsLabelEXT");

            // TODO: Trigger a message and make sure we see "Second individual" and "First Label Region" and not "First
            // individual label"
            g_in_region_num = 1;
            g_individual_label_num = 2;
            g_captured_labels = false;
            g_captured_only_expected_labels = false;
            strcpy(message_id, "Second Individual and First Region");
            pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                            XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data);
            TEST_EQUAL(g_captured_labels, true, "TestDebugUtils - Second Individual and First Region : Captured Labels")
            TEST_EQUAL(g_captured_only_expected_labels, true,
                       "TestDebugUtils - Second Individual and First Region : Captured Correct Labels")

            individual_label.labelName = g_third_individual_label_name;
            TEST_EQUAL(pfn_insert_debug_utils_label_ext(session, &individual_label), XR_SUCCESS,
                       "TestDebugUtils calling xrSessionInsertDebugUtilsLabelEXT");

            // TODO: Trigger a message and make sure we see "Third individual" and "First Label Region" and not "First
            // individual label" or "Second individual label"
            g_in_region_num = 1;
            g_individual_label_num = 3;
            g_captured_labels = false;
            g_captured_only_expected_labels = false;
            strcpy(message_id, "Third Individual and First Region");
            pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                            XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data);
            TEST_EQUAL(g_captured_labels, true, "TestDebugUtils - Third Individual and First Region : Captured Labels")
            TEST_EQUAL(g_captured_only_expected_labels, true,
                       "TestDebugUtils - Third Individual and First Region : Captured Correct Labels")

            // Begin a label region
            XrDebugUtilsLabelEXT second_label_region = {};
            second_label_region.type = XR_TYPE_DEBUG_UTILS_LABEL_EXT;
            second_label_region.labelName = g_second_label_region_name;
            TEST_EQUAL(pfn_begin_debug_utils_label_region_ext(session, &second_label_region), XR_SUCCESS,
                       "TestDebugUtils calling xrSessionBeginDebugUtilsLabelRegionEXT");

            // TODO: Trigger a message and make sure we see "Second Label Region" and "First Label Region"
            g_in_region_num = 2;
            g_individual_label_num = 0;
            g_captured_labels = false;
            g_captured_only_expected_labels = false;
            strcpy(message_id, "Second and First Label Regions");
            pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                            XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data);
            TEST_EQUAL(g_captured_labels, true, "TestDebugUtils - Second and First Label Regions : Captured Labels")
            TEST_EQUAL(g_captured_only_expected_labels, true,
                       "TestDebugUtils - Second and First Label Regions : Captured Correct Labels")

            // End the last label region
            TEST_EQUAL(pfn_end_debug_utils_label_region_ext(session), XR_SUCCESS,
                       "TestDebugUtils calling xrSessionEndDebugUtilsLabelRegionEXT");

            // TODO: Trigger a message and make sure we see "First Label Region"
            g_in_region_num = 1;
            g_individual_label_num = 0;
            g_captured_labels = false;
            g_captured_only_expected_labels = false;
            strcpy(message_id, "First Label Region 2");
            pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                            XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data);
            TEST_EQUAL(g_captured_labels, true, "TestDebugUtils - First Label Region 2 : Captured Labels")
            TEST_EQUAL(g_captured_only_expected_labels, true, "TestDebugUtils - First Label Region 2 : Captured Correct Labels")

            // Now clean-up
            XrResult res = xrRequestExitSession(session);
            TEST_EQUAL(res, XR_SUCCESS, "Exiting session")
            res = xrEndSession(session);
            TEST_EQUAL(res, XR_SUCCESS, "Ending session")

            // End the last label region
            TEST_EQUAL(pfn_end_debug_utils_label_region_ext(session), XR_SUCCESS,
                       "TestDebugUtils calling xrSessionEndDebugUtilsLabelRegionEXT");

            // TODO: Trigger a message and make sure we see no labels
            g_expecting_labels = false;
            g_in_region_num = 0;
            g_individual_label_num = 0;
            g_captured_labels = false;
            g_captured_only_expected_labels = false;
            strcpy(message_id, "No Labels");
            pfn_submit_dmsg(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
                            XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &callback_data);
            TEST_EQUAL(g_captured_labels, false, "TestDebugUtils - No Labels : Captured Labels")

            TEST_EQUAL(xrDestroySession(session), XR_SUCCESS, "Destroying session")

#ifdef XR_USE_GRAPHICS_API_D3D11
            if (d3d11_graphics_binding.device) d3d11_graphics_binding.device->Release();
#endif
        }

        // Destroy what we created
        TEST_EQUAL(pfn_destroy_debug_utils_messager_ext(debug_utils_messenger), XR_SUCCESS,
                   "Destroying the debug utils messenger for number of objects testing")

        TEST_EQUAL(xrDestroyInstance(instance), XR_SUCCESS, "Destroying instance for individual message type/severity testing")
    } catch (...) {
        TEST_FAIL("Exception triggered during test, automatic failure")
    }

    // Cleanup
    CleanupEnvironmentVariables();

    // Output results for this test
    TEST_REPORT(TestDebugUtils)
}

int main(int argc, char* argv[]) {
    uint32_t total_tests = 0;
    uint32_t total_passed = 0;
    uint32_t total_skipped = 0;
    uint32_t total_failed = 0;

    // Unused for now.
    (void)argc;
    (void)argv;

#if FILTER_OUT_LOADER_ERRORS == 1
    // Re-direct std::cerr to a string since we're intentionally causing errors and we don't
    // want it polluting the output stream.
    std::stringstream buffer;
    std::streambuf* original_cerr = nullptr;
    original_cerr = std::cerr.rdbuf(buffer.rdbuf());
#endif

    cout << "Starting loader_test" << endl << "--------------------" << endl;

    g_has_installed_runtime = DetectInstalledRuntime();

    TestEnumLayers(total_tests, total_passed, total_skipped, total_failed);
    TestEnumInstanceExtensions(total_tests, total_passed, total_skipped, total_failed);

    if (g_has_installed_runtime) {
        cout << "Installed XR runtime detected - doing active runtime tests" << endl;
        cout << "----------------------------------------------------------" << endl;
        TestCreateDestroyInstance(total_tests, total_passed, total_skipped, total_failed);
        TestGetSystem(total_tests, total_passed, total_skipped, total_failed);
        TestCreateDestroySession(total_tests, total_passed, total_skipped, total_failed);
    } else {
        cout << "No installed XR runtime detected - active runtime tests skipped(!)" << endl;
    }

    if (g_debug_utils_exists) {
        TestDebugUtils(total_tests, total_passed, total_skipped, total_failed);
    }

#if FILTER_OUT_LOADER_ERRORS == 1
    // Restore std::cerr to the original buffer
    std::cerr.rdbuf(original_cerr);
#endif

    // Cleanup
    CleanupEnvironmentVariables();

    cout << "    Results:" << endl << "    ------------------------------" << endl;
    cout << "        Total Tests:    " << std::to_string(total_tests) << endl;
    cout << "        Tests Passed:   " << std::to_string(total_passed) << endl;
    cout << "        Tests Skipped:  " << std::to_string(total_skipped) << endl;
    cout << "        Tests Failed:   " << std::to_string(total_failed) << endl;
    cout << "        Overall Result: ";
    if (total_failed > 0) {
        cout << "Failed" << endl;
        return -1;
    }
    cout << "Passed" << endl;
    return 0;
}
