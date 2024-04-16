// Copyright (c) 2017-2024, The Khronos Group Inc.
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

#if defined(XR_USE_PLATFORM_ANDROID)
#include <android_native_app_glue.h>
#include <android/log.h>
#endif  // defined(XR_USE_PLATFORM_ANDROID)

#include "xr_dependencies.h"
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <type_traits>
static_assert(sizeof(XrStructureType) == 4, "This should be a 32-bit enum");

#if defined(XR_USE_PLATFORM_ANDROID)
#define TESTLOG(...) __android_log_print(ANDROID_LOG_INFO, "LoaderTest", __VA_ARGS__)
#else
#define TESTLOG(...) printf(__VA_ARGS__)
#endif  // defined(XR_USE_PLATFORM_ANDROID)

// Filter out the loader's messages to std::cerr if this is defined to 1.  This allows a
// clean output for the test.
#define FILTER_OUT_LOADER_ERRORS 1

#define DEFINE_TEST(test_name) void test_name(uint32_t& total, uint32_t& passed, uint32_t& failed)

#define INIT_TEST(test_name)   \
    uint32_t local_total = 0;  \
    uint32_t local_passed = 0; \
    uint32_t local_failed = 0; \
    TESTLOG("    Starting " #test_name "\n")

#define TEST_REPORT(test_name)                                                                                           \
    if (local_failed > 0) {                                                                                              \
        TESTLOG("    Finished " #test_name ": Failed (Local - Passed: %u, Failed: %u)\n\n", local_passed, local_failed); \
    } else {                                                                                                             \
        TESTLOG("    Finished " #test_name ": Passed (Local - Passed: %u, Failed: %u)\n\n", local_passed, local_failed); \
    }                                                                                                                    \
    total += local_total;                                                                                                \
    passed += local_passed;                                                                                              \
    failed += local_failed;

#define TEST_HEADER(cout_string) TESTLOG("%s\n", cout_string)

#define TEST_INFO(cout_string) TESTLOG("           - %s\n", cout_string)

#define TEST_EQUAL(test, expected, cout_string)       \
    local_total++;                                    \
    if (expected != (test)) {                         \
        TESTLOG("        %s: Failed\n", cout_string); \
        local_failed++;                               \
    } else {                                          \
        TESTLOG("        %s: Passed\n", cout_string); \
        local_passed++;                               \
    }

#define TEST_FAIL(cout_string) \
    local_total++;             \
    local_failed++;            \
    TESTLOG("        %s: Failed\n", cout_string)

#define RUN_TEST(test) test(total_tests, total_passed, total_failed);

bool g_has_installed_runtime = false;

#if defined(XR_USE_PLATFORM_ANDROID)
static JavaVM* AndroidApplicationVM = NULL;
static jobject AndroidApplicationActivity = NULL;
#endif  // defined(XR_USE_PLATFORM_ANDROID)

#if defined(XR_KHR_LOADER_INIT_SUPPORT)
static void InitLoader() {
    // TODO: we should validate that xrCreateInstance, xrEnumerateInstanceExtensionProperties and xrEnumApiLayers
    //       all return XR_ERROR_INITIALIZATION_FAILED if XR_KHR_LOADER_INIT_SUPPORT but xrInitializeLoaderKHR
    //       has not been called.

    PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
    if (XR_SUCCEEDED(
            xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)(&xrInitializeLoaderKHR))) &&
        xrInitializeLoaderKHR != NULL) {
#if defined(XR_USE_PLATFORM_ANDROID)
        XrLoaderInitInfoAndroidKHR loaderInitializeInfoAndroid = {XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        loaderInitializeInfoAndroid.applicationVM = AndroidApplicationVM;
        loaderInitializeInfoAndroid.applicationContext = AndroidApplicationActivity;
        xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*)&loaderInitializeInfoAndroid);
#else
#error "Platform needs implementation of KHR_loader_init functionality"
#endif
    }
}
#endif  // defined(XR_KHR_LOADER_INIT_SUPPORT)

#if defined(XR_USE_PLATFORM_ANDROID) && defined(XR_KHR_LOADER_INIT_SUPPORT)
static XrInstanceCreateInfoAndroidKHR GetPlatformInstanceCreateExtension() {
    XrInstanceCreateInfoAndroidKHR instanceCreateInfoAndroid{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
    instanceCreateInfoAndroid.applicationVM = AndroidApplicationVM;
    instanceCreateInfoAndroid.applicationActivity = AndroidApplicationActivity;
    return instanceCreateInfoAndroid;
}
static const char* base_extension_names[1] = {XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME};
static const uint32_t base_extension_count = 1;
#else
static XrBaseInStructure GetPlatformInstanceCreateExtension() {
    XrBaseInStructure baseIn{XR_TYPE_UNKNOWN};
    return baseIn;
}
static const char* const* base_extension_names = nullptr;
static const uint32_t base_extension_count = 0;
#endif  // defined(XR_USE_PLATFORM_ANDROID) && defined(XR_KHR_LOADER_INIT_SUPPORT)

void CleanupEnvironmentVariables() {
#if defined(XR_USE_PLATFORM_ANDROID)
    // env var override is not available on Android.
#else
    LoaderTestUnsetEnvironmentVariable("XR_API_LAYER_PATH");
    LoaderTestUnsetEnvironmentVariable("XR_RUNTIME_JSON");
#endif  // defined(XR_USE_PLATFORM_ANDROID)
}

// The loader will keep a runtime loaded after a successful xrEnumerateInstanceExtensionProperties call until an instance is created
// and destroyed.
void ForceLoaderUnloadRuntime() {
    XrInstance instance;
    XrInstanceCreateInfo instance_create_info{XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy(instance_create_info.applicationInfo.applicationName, "Force Unload");
    instance_create_info.applicationInfo.applicationVersion = 688;
    auto platform_instance_create = GetPlatformInstanceCreateExtension();
    instance_create_info.next = &platform_instance_create;
    instance_create_info.enabledExtensionCount = base_extension_count;
    instance_create_info.enabledExtensionNames = base_extension_names;
    if (XR_SUCCEEDED(xrCreateInstance(&instance_create_info, &instance))) {
        xrDestroyInstance(instance);
    }
}

bool DetectInstalledRuntime() {
    bool runtime_found = false;
    uint32_t ext_count = 0;

#if !defined(XR_USE_PLATFORM_ANDROID)
    // Disable any potential envvar override - just looking for an installed runtime
    LoaderTestUnsetEnvironmentVariable("XR_RUNTIME_JSON");
#else
    // XR_RUNTIME_JSON override not supported on Android
#endif  // !defined(XR_USE_PLATFORM_ANDROID)

    ForceLoaderUnloadRuntime();

    // Enumerating available extensions requires loading the runtime
    XrResult ret = xrEnumerateInstanceExtensionProperties(nullptr, 0, &ext_count, nullptr);

    // Successful enumeration implies an active runtime is installed (regardless of ext count)
    runtime_found = XR_SUCCEEDED(ret);

    return runtime_found;
}

// Test the xrEnumerateApiLayerProperties function through the loader.
DEFINE_TEST(TestEnumLayers) {
    INIT_TEST(TestEnumLayers);

    try {
        XrResult test_result = XR_SUCCESS;

        uint32_t in_layer_value = 0;
        uint32_t out_layer_value = 0;

#if !defined(XR_USE_PLATFORM_ANDROID) || !defined(XR_KHR_LOADER_INIT_SUPPORT)

#if !defined(XR_USE_PLATFORM_ANDROID)
        // Tests with no explicit layers set
        // NOTE: Implicit layers will still be present, need to figure out what to do here.
        LoaderTestUnsetEnvironmentVariable("XR_API_LAYER_PATH");
#else
        // XR_API_LAYER_PATH override not available on Android
#endif  // !defined(XR_USE_PLATFORM_ANDROID)

        // Test number query
        test_result = xrEnumerateApiLayerProperties(in_layer_value, &out_layer_value, nullptr);
        TEST_EQUAL(test_result, XR_SUCCESS, "No explicit layers layer count check");
        TEST_INFO((std::to_string(out_layer_value) + " layers available.").c_str());

        // If any implicit layers are found, try property return
        if (out_layer_value > 0) {
            in_layer_value = out_layer_value;
            out_layer_value = 0;
            std::vector<XrApiLayerProperties> layer_props(in_layer_value, {XR_TYPE_API_LAYER_PROPERTIES});
            test_result = xrEnumerateApiLayerProperties(in_layer_value, &out_layer_value, layer_props.data());
            TEST_EQUAL(test_result, XR_SUCCESS, "No explicit layers layer props query");
            if (test_result == XR_SUCCESS) {
                for (const auto& prop : layer_props) {
                    std::string implictLayerInfo = std::string("Found implicit layer: ") + prop.layerName;
                    TEST_INFO(implictLayerInfo.c_str());
                }
            }
        }
#else
        // XR_API_LAYER_PATH override not supported on Android
        TESTLOG("Cannot test no explicit layers on Android if using KHR_LOADER_INIT");
#endif  // !defined(XR_USE_PLATFORM_ANDROID) || !defined(XR_KHR_LOADER_INIT_SUPPORT)

        // Tests with some explicit layers instead
        in_layer_value = 0;
        out_layer_value = 0;
        uint32_t num_valid_jsons = 6;

#if defined(XR_USE_PLATFORM_ANDROID) && !defined(XR_KHR_LOADER_INIT_SUPPORT)
        // Android API layer loading from apk is only supported when using XR_KHR_LOADER_INIT_SUPPORT
        TEST_HEADER("     Cannot test loading API layer from apk on Android without XR_KHR_LOADER_INIT_SUPPORT");
#else

#if defined(XR_USE_PLATFORM_ANDROID)
        // API layers from apk on Android are always available and do not require override.
        // This is because XR_API_LAYER_PATH override is not available on Android.
#else
        // Point to json directory, contains 6 valid json files
        LoaderTestSetEnvironmentVariable("XR_API_LAYER_PATH", "./resources/layers");
#endif  // defined(XR_USE_PLATFORM_ANDROID)

        // Test number query
        test_result = xrEnumerateApiLayerProperties(in_layer_value, &out_layer_value, nullptr);
        TEST_EQUAL(test_result, XR_SUCCESS, "Simple explicit layers layer count check");
        TEST_EQUAL(out_layer_value, num_valid_jsons,
                   (std::string("Simple explicit layers expected count ") + std::to_string(num_valid_jsons) + std::string(" got ") +
                    std::to_string(out_layer_value))
                       .c_str());

        // Try property return
        in_layer_value = out_layer_value;
        out_layer_value = 0;
        std::vector<XrApiLayerProperties> layer_props(in_layer_value, {XR_TYPE_API_LAYER_PROPERTIES});
        test_result = xrEnumerateApiLayerProperties(in_layer_value, &out_layer_value, layer_props.data());
        TEST_EQUAL(test_result, XR_SUCCESS, "Simple explicit layers layer props query");

        if (test_result == XR_SUCCESS) {
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
                    TEST_INFO((std::string("Failed, found unexpected layer ") + layer_name + " in list").c_str());
                    found_bad = true;
                    break;
                }
            }

            TEST_EQUAL(found_good_absolute_test, true, "Simple explicit layers found_good_absolute_test");
            TEST_EQUAL(found_good_relative_test, true, "Simple explicit layers found_good_absolute_test");
            TEST_EQUAL(found_bad, false, "Simple explicit layers found_bad");

            for (const auto& prop : layer_props) {
                TEST_INFO((std::string("Found API layer: ") + prop.layerName).c_str());
            }
        }
#endif  // defined(XR_USE_PLATFORM_ANDROID) && !defined(XR_KHR_LOADER_INIT_SUPPORT)
    } catch (...) {
        TEST_FAIL("Exception triggered during test, automatic failure");
    }

    // Cleanup
    CleanupEnvironmentVariables();

    // Output results for this test
    TEST_REPORT(TestEnumLayers)
}

// Test the xrEnumerateInstanceExtensionProperties function through the loader.
DEFINE_TEST(TestEnumInstanceExtensions) {
    INIT_TEST(TestEnumInstanceExtensions);

    try {
        XrResult test_result = XR_SUCCESS;
        uint32_t in_extension_value;
        uint32_t out_extension_value;

        for (uint32_t test = 0; test < 2; ++test) {
#if defined(XR_USE_PLATFORM_ANDROID) && defined(XR_KHR_LOADER_INIT_SUPPORT)
            if (test == 0) {
                TEST_HEADER("     Cannot test with no explicit API layers on Android");
                continue;
            }
#endif  // !defined(XR_USE_PLATFORM_ANDROID) || !defined(XR_KHR_LOADER_INIT_SUPPORT)

            std::string subtest_name;
            std::string valid_runtime_path;

            switch (test) {
                // No Explicit layers set
                case 0:
                    subtest_name = "with no explicit API layers";
                    // NOTE: Implicit layers will still be present, need to figure out what to do here.
#if !defined(XR_USE_PLATFORM_ANDROID)
                    LoaderTestUnsetEnvironmentVariable("XR_API_LAYER_PATH");
#else
                    // XR_API_LAYER_PATH not supported on Android
#endif  // !defined(XR_USE_PLATFORM_ANDROID)
                    break;
                default:
                    subtest_name = "with explicit API layers";
#if !defined(XR_USE_PLATFORM_ANDROID)
                    LoaderTestSetEnvironmentVariable("XR_API_LAYER_PATH", "./resources/layers");
#else
                    // XR_API_LAYER_PATH not supported on Android
#endif  // !defined(XR_USE_PLATFORM_ANDROID)
                    break;
            }

#if !defined(XR_USE_PLATFORM_ANDROID)
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
                        test_result =
                            xrEnumerateInstanceExtensionProperties(nullptr, in_extension_value, &out_extension_value, nullptr);
                        TEST_EQUAL(
                            test_result, XR_ERROR_RUNTIME_UNAVAILABLE,
                            (std::string("JSON ") + cur_file + " extension enum count query (" + subtest_name + ")").c_str());
                    }
                }
            }
#else
                // XR_RUNTIME_JSON override not supported on Android
#endif  // !defined(XR_USE_PLATFORM_ANDROID)

            // Test the active runtime, if installed
            if (g_has_installed_runtime) {
#if !defined(XR_USE_PLATFORM_ANDROID)
                LoaderTestUnsetEnvironmentVariable("XR_RUNTIME_JSON");
#else
                // XR_RUNTIME_JSON override not supported on Android
#endif  // !defined(XR_USE_PLATFORM_ANDROID)

                ForceLoaderUnloadRuntime();

                // Query the count (should return 2)
                in_extension_value = 0;
                out_extension_value = 0;
                test_result = xrEnumerateInstanceExtensionProperties(nullptr, in_extension_value, &out_extension_value, nullptr);
                TEST_EQUAL(test_result, XR_SUCCESS, ("Active runtime extension enum count query (" + subtest_name + ")").c_str());

                // Get the properties
                std::vector<XrExtensionProperties> properties(out_extension_value, {XR_TYPE_EXTENSION_PROPERTIES});
                in_extension_value = out_extension_value;
                out_extension_value = 0;
                test_result =
                    xrEnumerateInstanceExtensionProperties(nullptr, in_extension_value, &out_extension_value, properties.data());
                TEST_EQUAL(test_result, XR_SUCCESS,
                           ("Active runtime extension enum properties query (" + subtest_name + ")").c_str());

                if (test_result == XR_SUCCESS) {
                    bool found_error = false;
                    for (const XrExtensionProperties& prop : properties) {
                        // Just check if extension name begins with "XR_"
                        if (strlen(prop.extensionName) < 4 || 0 != strncmp(prop.extensionName, "XR_", 3)) {
                            found_error = true;
                        }
                    }
                    TEST_EQUAL(found_error, false, "Active runtime extension enum properties query malformed extension name");
                }

                // Try with a garbage layer name
                in_extension_value = 0;
                out_extension_value = 0;
                test_result = xrEnumerateInstanceExtensionProperties("XrApiLayer_no_such_layer", in_extension_value,
                                                                     &out_extension_value, nullptr);
                TEST_EQUAL(test_result, XR_ERROR_API_LAYER_NOT_PRESENT,
                           ("Garbage Layer extension enum count query (" + subtest_name + ")").c_str());

                // Try with a valid layer name (if layer is present)
                in_extension_value = 0;
                out_extension_value = 0;
                test_result =
                    xrEnumerateInstanceExtensionProperties("XR_APILAYER_test", in_extension_value, &out_extension_value, nullptr);

                if (test == 0) {
                    TEST_EQUAL(test_result, XR_ERROR_API_LAYER_NOT_PRESENT,
                               ("Valid Layer extension enum count query (" + subtest_name + ")").c_str());
                } else if (test == 1) {
                    TEST_EQUAL(test_result, XR_SUCCESS, ("Valid Layer extension enum count query (" + subtest_name + ")").c_str());
                } else {
                    TEST_FAIL("Unknown test");
                }
            }
        }
    } catch (std::exception const& e) {
        TEST_FAIL((std::string("Exception triggered during test (") + e.what() + "), automatic failure").c_str());
    } catch (...) {
        TEST_FAIL("Exception triggered during test, automatic failure");
    }

    // Cleanup
    CleanupEnvironmentVariables();

    // Output results for this test
    TEST_REPORT(TestEnumInstanceExtensions)
}

// Test creating and destroying an OpenXR instance through the loader.
DEFINE_TEST(TestCreateDestroyInstance) {
    INIT_TEST(TestCreateDestroyInstance);

    try {
        XrResult expected_result = XR_SUCCESS;
        char valid_layer_to_enable[] = "XR_APILAYER_LUNARG_api_dump";
        char invalid_layer_to_enable[] = "XrApiLayer_invalid_layer_test";
        char valid_extension_to_enable[] = "XR_EXT_debug_utils";
        char invalid_extension_to_enable[] = "XR_KHR_fake_ext1";
        const char* const valid_layer_name_array[1] = {valid_layer_to_enable};
        const char* const invalid_layer_name_array[1] = {invalid_layer_to_enable};
#if defined(XR_USE_PLATFORM_ANDROID)
        const char* valid_extension_name_array[2] = {XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME, valid_extension_to_enable};
        const char* const invalid_extension_name_array[2] = {XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
                                                             invalid_extension_to_enable};
#else
        const char* valid_extension_name_array[1] = {valid_extension_to_enable};
        const char* const invalid_extension_name_array[1] = {invalid_extension_to_enable};
#endif
        XrApplicationInfo application_info = {};
        strcpy(application_info.applicationName, "Loader Test");
        application_info.applicationVersion = 688;
        strcpy(application_info.engineName, "Infinite Improbability Drive");
        application_info.engineVersion = 42;
        application_info.apiVersion = XR_CURRENT_API_VERSION;

        std::string current_path;
        std::string layer_path;

#if !defined(XR_USE_PLATFORM_ANDROID)
        // Ensure using installed runtime
        LoaderTestUnsetEnvironmentVariable("XR_RUNTIME_JSON");
#else
        // XR_RUNTIME_JSON override not supported on Android
#endif  // !defined(XR_USE_PLATFORM_ANDROID)

#if !defined(XR_USE_PLATFORM_ANDROID)
        // Convert relative test layer path to absolute, set envvar
        FileSysUtilsGetCurrentPath(layer_path);
        layer_path =
            layer_path + TEST_DIRECTORY_SYMBOL + ".." + TEST_DIRECTORY_SYMBOL + ".." + TEST_DIRECTORY_SYMBOL + "api_layers";
        LoaderTestSetEnvironmentVariable("XR_API_LAYER_PATH", layer_path);

        // Make API dump write to a file, when loaded
        LoaderTestSetEnvironmentVariable("XR_API_DUMP_FILE_NAME", "api_dump_out.txt");
#else
        // XR_API_LAYER_PATH override not supported on Android
#endif  // !defined(XR_USE_PLATFORM_ANDROID)

        for (uint32_t test_num = 0; test_num < 6; ++test_num) {
#if defined(XR_USE_PLATFORM_ANDROID)
            if (test_num == 1 || test_num == 5) {
                TEST_HEADER(("     Skip test " + std::to_string(test_num) + " on Android").c_str());
                continue;
            }
#endif  // defined(XR_USE_PLATFORM_ANDROID)

            XrInstance instance = XR_NULL_HANDLE;
            XrResult create_result = XR_ERROR_VALIDATION_FAILURE;
            std::string current_test_string;
            XrInstanceCreateInfo instance_create_info = {XR_TYPE_INSTANCE_CREATE_INFO};
            instance_create_info.applicationInfo = application_info;
            auto platform_instance_create = GetPlatformInstanceCreateExtension();
            instance_create_info.next = &platform_instance_create;
            instance_create_info.enabledExtensionCount = base_extension_count;
            instance_create_info.enabledExtensionNames = base_extension_names;

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
                    current_test_string = "xrCreateInstance with a single extra valid extension";
                    expected_result = XR_SUCCESS;
                    instance_create_info.enabledExtensionCount = 1 + base_extension_count;
                    instance_create_info.enabledExtensionNames = valid_extension_name_array;
                    break;
                // Test 4 - xrCreateInstance with an invalid extension
                case 4:
                    current_test_string = "xrCreateInstance with a single extra invalid extension";
                    expected_result = XR_ERROR_EXTENSION_NOT_PRESENT;
                    instance_create_info.enabledExtensionCount = 1 + base_extension_count;
                    instance_create_info.enabledExtensionNames = invalid_extension_name_array;
                    break;
                // Test 5 - xrCreateInstance with everything
                case 5:
                    current_test_string = "xrCreateInstance with app info, valid layer, and a valid extension";
                    expected_result = XR_SUCCESS;
                    instance_create_info.enabledApiLayerCount = 1;
                    instance_create_info.enabledApiLayerNames = valid_layer_name_array;
                    instance_create_info.enabledExtensionCount = 1 + base_extension_count;
                    instance_create_info.enabledExtensionNames = valid_extension_name_array;
                    break;
            }

            // Try create instance and look for the correct return
            std::string cur_message = current_test_string + " - xrCreateInstance";
            create_result = xrCreateInstance(&instance_create_info, &instance);
            TEST_EQUAL(create_result, expected_result, cur_message.c_str())

            // If the expected return was a success, clean up the created instance
            if (XR_SUCCEEDED(create_result)) {
                cur_message = current_test_string + " - xrDestroyInstance";
                TEST_EQUAL(xrDestroyInstance(instance), XR_SUCCESS, cur_message.c_str())
            }
        }

        // Make sure DestroyInstance with a NULL handle gives correct error
        TEST_EQUAL(xrDestroyInstance(XR_NULL_HANDLE), XR_ERROR_HANDLE_INVALID, "xrDestroyInstance(XR_NULL_HANDLE)")
    } catch (...) {
        TEST_FAIL("Exception triggered during test, automatic failure");
    }

    // Cleanup
    CleanupEnvironmentVariables();

    // Output results for this test
    TEST_REPORT(TestCreateDestroyInstance)
}

// Test at least one XrInstance function not directly implemented in the loader's manual code section.
// This is to make sure that the automatic instance functions work.
DEFINE_TEST(TestGetSystem) {
    INIT_TEST(TestGetSystem);

    try {
        XrInstance instance = XR_NULL_HANDLE;
        XrApplicationInfo application_info = {};
        strcpy(application_info.applicationName, "Loader Test");
        application_info.applicationVersion = 688;
        strcpy(application_info.engineName, "Infinite Improbability Drive");
        application_info.engineVersion = 42;
        application_info.apiVersion = XR_CURRENT_API_VERSION;
        XrInstanceCreateInfo instance_create_info = {XR_TYPE_INSTANCE_CREATE_INFO};
        instance_create_info.applicationInfo = application_info;
        auto platform_instance_create = GetPlatformInstanceCreateExtension();
        instance_create_info.next = &platform_instance_create;
        instance_create_info.enabledExtensionCount = base_extension_count;
        instance_create_info.enabledExtensionNames = base_extension_names;

        TEST_EQUAL(xrCreateInstance(&instance_create_info, &instance), XR_SUCCESS, "Creating instance")

        XrSystemGetInfo system_get_info = {XR_TYPE_SYSTEM_GET_INFO};
        system_get_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

        XrSystemId systemId = XR_NULL_SYSTEM_ID;
        TEST_EQUAL(xrGetSystem(instance, &system_get_info, &systemId), XR_SUCCESS, "xrGetSystem");

        TEST_EQUAL(xrDestroyInstance(instance), XR_SUCCESS, "Destroying instance");
    } catch (...) {
        TEST_FAIL("Exception triggered during test, automatic failure");
    }

    // Cleanup
    CleanupEnvironmentVariables();

    // Output results for this test
    TEST_REPORT(TestGetSystem)
}

// Test at least one non-XrInstance function to make sure that the automatic non-instance functions work.
DEFINE_TEST(TestCreateDestroyAction) {
    INIT_TEST(TestCreateDestroyAction);

    try {
        XrInstance instance = XR_NULL_HANDLE;

        XrApplicationInfo app_info = {};
        strcpy(app_info.applicationName, "Loader Test");
        app_info.applicationVersion = 688;
        strcpy(app_info.engineName, "Infinite Improbability Drive");
        app_info.engineVersion = 42;
        app_info.apiVersion = XR_CURRENT_API_VERSION;

        XrInstanceCreateInfo instance_ci = {XR_TYPE_INSTANCE_CREATE_INFO};
        instance_ci.applicationInfo = app_info;
        auto platform_instance_create = GetPlatformInstanceCreateExtension();
        instance_ci.next = &platform_instance_create;
        instance_ci.enabledExtensionCount = base_extension_count;
        instance_ci.enabledExtensionNames = base_extension_names;

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
        TEST_FAIL("Exception triggered during test, automatic failure");
    }

    // Cleanup
    CleanupEnvironmentVariables();

    // Output results for this test
    TEST_REPORT(TestCreateDestroyAction)
}

bool runTests() {
    uint32_t total_tests = 0;
    uint32_t total_passed = 0;
    uint32_t total_failed = 0;

    TEST_HEADER("Starting loader_test");
    TEST_HEADER("--------------------");

    g_has_installed_runtime = DetectInstalledRuntime();

    RUN_TEST(TestEnumLayers)
    RUN_TEST(TestEnumInstanceExtensions)

    if (g_has_installed_runtime) {
        TEST_HEADER("Installed XR runtime detected - doing active runtime tests");
        TEST_HEADER("----------------------------------------------------------");
        RUN_TEST(TestCreateDestroyInstance)
        RUN_TEST(TestGetSystem)
        RUN_TEST(TestCreateDestroyAction)
    } else {
        TEST_HEADER("No installed XR runtime detected - active runtime tests skipped(!)");
    }

    // Cleanup
    CleanupEnvironmentVariables();

    TEST_HEADER("    Results:");
    TEST_HEADER("    ------------------------------");
    TEST_HEADER(("        Total Tests:    " + std::to_string(total_tests)).c_str());
    TEST_HEADER(("        Tests Passed:   " + std::to_string(total_passed)).c_str());
    TEST_HEADER(("        Tests Failed:   " + std::to_string(total_failed)).c_str());
    if (total_failed == 0) {
        TEST_HEADER("        Overall Result: Passed");
        return true;
    } else {
        TEST_HEADER("        Overall Result: Failed");
        return false;
    }
}

#if defined(XR_USE_PLATFORM_ANDROID)
static void app_handle_cmd(struct android_app* app, int32_t cmd) {
    (void)app;
    (void)cmd;
}

void android_main(struct android_app* app) {
    AndroidApplicationVM = app->activity->vm;
    AndroidApplicationActivity = app->activity->clazz;

    JNIEnv* Env;
    app->activity->vm->AttachCurrentThread(&Env, nullptr);

    app->userData = NULL;
    app->onAppCmd = app_handle_cmd;

    InitLoader();

    runTests();

    ANativeActivity_finish(app->activity);
    app->activity->vm->DetachCurrentThread();
}
#else
int main(int /*argc*/, char* /*argv*/[]) {
#if FILTER_OUT_LOADER_ERRORS == 1
    // Re-direct std::cerr to a string since we're intentionally causing errors and we don't
    // want it polluting the output stream.
    std::stringstream buffer;
    std::streambuf* original_cerr = nullptr;
    original_cerr = std::cerr.rdbuf(buffer.rdbuf());
#endif

    bool success = runTests();

#if FILTER_OUT_LOADER_ERRORS == 1
    // Restore std::cerr to the original buffer
    std::cerr.rdbuf(original_cerr);
#endif

    return success ? 0 : -1;
}
#endif  // defined(XR_USE_PLATFORM_ANDROID)
