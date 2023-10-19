// Copyright (c) 2017-2023, The Khronos Group Inc.
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

#include "xr_dependencies.h"
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <type_traits>
static_assert(sizeof(XrStructureType) == 4, "This should be a 32-bit enum");

// Add some judicious char savers
using std::cout;
using std::endl;

// Filter out the loader's messages to std::cerr if this is defined to 1.  This allows a
// clean output for the test.
#define FILTER_OUT_LOADER_ERRORS 1

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

#define RUN_TEST(test) test(total_tests, total_passed, total_skipped, total_failed);

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

    return runtime_found;
}

// Test the xrEnumerateApiLayerProperties function through the loader.
DEFINE_TEST(TestEnumLayers) {
    INIT_TEST(TestEnumLayers)

    try {
        XrResult test_result = XR_SUCCESS;
        std::vector<XrApiLayerProperties> layer_props;

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
        LoaderTestSetEnvironmentVariable("XR_API_LAYER_PATH", "./resources/layers");
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
        TEST_FAIL("Exception triggered during test, automatic failure")
    }

    // Cleanup
    CleanupEnvironmentVariables();

    // Output results for this test
    TEST_REPORT(TestEnumLayers)
}

// Test the xrEnumerateInstanceExtensionProperties function through the loader.
DEFINE_TEST(TestEnumInstanceExtensions) {
    INIT_TEST(TestEnumInstanceExtensions)

    try {
        XrResult test_result = XR_SUCCESS;
        uint32_t in_extension_value;
        uint32_t out_extension_value;
        std::vector<XrExtensionProperties> properties;

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
                    LoaderTestSetEnvironmentVariable("XR_API_LAYER_PATH", "./resources/layers");
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
        TEST_FAIL("Exception triggered during test (" << e.what() << "), automatic failure")
    } catch (...) {
        TEST_FAIL("Exception triggered during test, automatic failure")
    }

    // Cleanup
    CleanupEnvironmentVariables();

    // Output results for this test
    TEST_REPORT(TestEnumInstanceExtensions)
}

// Test creating and destroying an OpenXR instance through the loader.
DEFINE_TEST(TestCreateDestroyInstance) {
    INIT_TEST(TestCreateDestroyInstance)

    try {
        XrResult expected_result = XR_SUCCESS;
        char valid_layer_to_enable[] = "XR_APILAYER_LUNARG_api_dump";
        char invalid_layer_to_enable[] = "XrApiLayer_invalid_layer_test";
        char valid_extension_to_enable[] = "XR_EXT_debug_utils";
        char invalid_extension_to_enable[] = "XR_KHR_fake_ext1";
        const char* const valid_layer_name_array[1] = {valid_layer_to_enable};
        const char* const invalid_layer_name_array[1] = {invalid_layer_to_enable};
        const char* valid_extension_name_array[1] = {valid_extension_to_enable};
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

        // Make API dump write to a file, when loaded
        LoaderTestSetEnvironmentVariable("XR_API_DUMP_FILE_NAME", "api_dump_out.txt");

        for (uint32_t test_num = 0; test_num < 6; ++test_num) {
            XrInstance instance = XR_NULL_HANDLE;
            XrResult create_result = XR_ERROR_VALIDATION_FAILURE;
            std::string current_test_string;
            XrInstanceCreateInfo instance_create_info = {XR_TYPE_INSTANCE_CREATE_INFO};
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
                    current_test_string = "xrCreateInstance with a single valid extension";
                    expected_result = XR_SUCCESS;
                    instance_create_info.enabledExtensionCount = 1;
                    instance_create_info.enabledExtensionNames = valid_extension_name_array;
                    break;
                // Test 4 - xrCreateInstance with an invalid extension
                case 4:
                    current_test_string = "xrCreateInstance with a single invalid extension";
                    expected_result = XR_ERROR_EXTENSION_NOT_PRESENT;
                    instance_create_info.enabledExtensionCount = 1;
                    instance_create_info.enabledExtensionNames = invalid_extension_name_array;
                    break;
                // Test 5 - xrCreateInstance with everything
                case 5:
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
        XrInstanceCreateInfo instance_create_info = {XR_TYPE_INSTANCE_CREATE_INFO};
        instance_create_info.applicationInfo = application_info;

        TEST_EQUAL(xrCreateInstance(&instance_create_info, &instance), XR_SUCCESS, "Creating instance")

        XrSystemGetInfo system_get_info = {XR_TYPE_SYSTEM_GET_INFO};
        system_get_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

        XrSystemId systemId = XR_NULL_SYSTEM_ID;
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
DEFINE_TEST(TestCreateDestroyAction) {
    INIT_TEST(TestCreateDestroyAction)

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

        XrInstanceCreateInfo instance_ci = {XR_TYPE_INSTANCE_CREATE_INFO};
        instance_ci.applicationInfo = app_info;
        TEST_EQUAL(xrCreateInstance(&instance_ci, &instance), XR_SUCCESS, "Creating instance")

        if (instance != XR_NULL_HANDLE) {
            XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetInfo.actionSetName, "test");
            strcpy(actionSetInfo.localizedActionSetName, "test");
            XrActionSet actionSet{XR_NULL_HANDLE};
            TEST_EQUAL(xrCreateActionSet(instance, &actionSetInfo, &actionSet), XR_SUCCESS, "Calling xrCreateActionSet");

            XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy(actionInfo.actionName, "action_test");
            strcpy(actionInfo.localizedActionName, "Action test");
            XrAction completeAction{XR_NULL_HANDLE};
            TEST_EQUAL(xrCreateAction(actionSet, &actionInfo, &completeAction), XR_SUCCESS, "Calling xrCreateAction");
        }

        TEST_EQUAL(xrDestroyInstance(instance), XR_SUCCESS, "Destroying instance")
    } catch (...) {
        TEST_FAIL("Exception triggered during test, automatic failure")
    }

    // Cleanup
    CleanupEnvironmentVariables();

    // Output results for this test
    TEST_REPORT(TestCreateDestroyAction)
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

    RUN_TEST(TestEnumLayers)
    RUN_TEST(TestEnumInstanceExtensions)

    if (g_has_installed_runtime) {
        cout << "Installed XR runtime detected - doing active runtime tests" << endl;
        cout << "----------------------------------------------------------" << endl;
        RUN_TEST(TestCreateDestroyInstance)
        RUN_TEST(TestGetSystem)
        RUN_TEST(TestCreateDestroyAction)
    } else {
        cout << "No installed XR runtime detected - active runtime tests skipped(!)" << endl;
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
