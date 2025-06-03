// Copyright (c) 2017-2025 The Khronos Group Inc.
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

#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <sstream>
#include <type_traits>
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
#include <openxr/openxr_reflection.h>

#include <catch2/catch_message.hpp>
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_case_info.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_predicate.hpp>
#include <catch2/internal/catch_clara.hpp>  // for customizing arg parsing
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#if defined(XR_USE_PLATFORM_ANDROID)
#define TESTLOG(...) __android_log_print(ANDROID_LOG_INFO, "LoaderTest", __VA_ARGS__)
#else
#define TESTLOG(...) printf(__VA_ARGS__)
#endif  // defined(XR_USE_PLATFORM_ANDROID)

namespace {

enum MessageType {
    MessageType_Stdout,
    MessageType_Stderr,
};

/// Console output redirection
class ConsoleStream : public std::streambuf {
   public:
    ConsoleStream(MessageType messageType) : m_messageType(messageType) {}

    int overflow(int c) override {
        auto c_as_char = traits_type::to_char_type(c);
        m_s += c_as_char;  // add to local buffer
        if (c_as_char == '\n') {
            sync();  // flush on newlines
        }
        return c;
    }

    int sync() override {
        // if our buffer has anything meaningful, flush to the conformance_test host.
        if (m_s.length() > 0) {
#if defined(XR_USE_PLATFORM_ANDROID)
            __android_log_print(ANDROID_LOG_INFO, "LoaderTest", "%s", m_s.c_str());
#else
            printf("%s", m_s.c_str());
#endif  // defined(XR_USE_PLATFORM_ANDROID)
            (void)m_messageType;
            m_s.clear();
        }
        return 0;
    }

   private:
    std::string m_s;
    const MessageType m_messageType;
};

void InstanceDeleter(XrInstance* instance) {
    if (instance != XR_NULL_HANDLE) {
        xrDestroyInstance(*instance);
    }
}

}  // namespace

// We need to redirect catch2 output through the reporting infrastructure.
// Note that if "-o" is used, Catch will redirect the returned ostream to the file instead.
namespace Catch {
std::ostream& cout() {
    static ConsoleStream acs(MessageType_Stdout);
    static std::ostream ret(&acs);
    return ret;
}
std::ostream& clog() {
    static ConsoleStream acs(MessageType_Stdout);
    static std::ostream ret(&acs);
    return ret;
}
std::ostream& cerr() {
    static ConsoleStream acs(MessageType_Stderr);
    static std::ostream ret(&acs);
    return ret;
}
}  // namespace Catch

// Filter out the loader's messages to std::cerr if this is defined to 1.  This allows a
// clean output for the test.
#define FILTER_OUT_LOADER_ERRORS 1

#define TEST_HEADER(cout_string) INFO(cout_string)

#define TEST_INFO(cout_string) INFO(cout_string)

#define TEST_EQUAL(test, expected, msg) \
    do {                                \
        INFO(msg);                      \
        REQUIRE((test) == (expected));  \
    } while ((void)0, 0)

#define TEST_FAIL(cout_string) \
    do {                       \
        INFO(cout_string);     \
        FAIL();                \
    } while ((void)0, 0)

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

std::string to_string(const XrResult& value) {
#define XR_ENUM_CASE_STR(name, val) \
    case val:                       \
        return #name;

    switch (value) {
        XR_LIST_ENUM_XrResult(XR_ENUM_CASE_STR)  // nbsp
            default : {
            char buffer[XR_MAX_RESULT_STRING_SIZE]{};
            const char* desc = XR_SUCCEEDED(value) ? "XR_UNKNOWN_SUCCESS_" : "XR_UNKNOWN_FAILURE_";
            snprintf(buffer, XR_MAX_RESULT_STRING_SIZE, "%s%d", desc, (int)value);
            return buffer;
        }
    }
#undef XR_ENUM_CASE_STR
}

namespace Catch {

template <>
struct StringMaker<XrApiLayerProperties> {
    static std::string convert(XrApiLayerProperties const& value) {
        std::ostringstream oss;
        oss << value.layerName << " (v" << value.layerVersion << ", " << value.description << ")";
        return oss.str();
    }
};

template <>
struct StringMaker<XrResult> {
    static std::string convert(XrResult const& value) { return to_string(value); }
};

}  // namespace Catch

/// Custom matcher for use with ???_THAT() assertions, which takes a user-provided predicate and checks for at least one
/// element in the collection for which this is true.
template <typename T, typename ValType = typename T::value_type>
class ContainsPredicate : public Catch::Matchers::MatcherBase<T> {
   public:
    using ValueType = ValType;
    using PredicateType = std::function<bool(ValueType const&)>;
    ContainsPredicate(PredicateType&& predicate, const char* desc) : predicate_(std::move(predicate)), desc_(desc) {}

    virtual bool match(T const& container) const override {
        using std::begin;
        using std::end;
        return end(container) != std::find_if(begin(container), end(container), predicate_);
    }

    virtual std::string describe() const override { return std::string("contains an element such that ") + desc_; }

   private:
    PredicateType predicate_;
    const char* desc_;
};

template <typename T>
using VectorContainsPredicate = ContainsPredicate<std::vector<T>>;

// Test the xrEnumerateApiLayerProperties function through the loader.
TEST_CASE("TestEnumLayers", "") {
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
    {
        INFO("First call to xrEnumerateApiLayerProperties: get count");
        CHECK(XR_SUCCESS == xrEnumerateApiLayerProperties(in_layer_value, &out_layer_value, nullptr));
    }
    INFO("Layers available: " << out_layer_value);

    // If any implicit layers are found, try property return
    if (out_layer_value > 0) {
        in_layer_value = out_layer_value;
        out_layer_value = 0;
        std::vector<XrApiLayerProperties> layer_props(in_layer_value, {XR_TYPE_API_LAYER_PROPERTIES});
        CHECK(XR_SUCCESS == (test_result = xrEnumerateApiLayerProperties(in_layer_value, &out_layer_value, layer_props.data())));
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
    {
        INFO("Simple explicit layers");
        test_result = xrEnumerateApiLayerProperties(in_layer_value, &out_layer_value, nullptr);
        REQUIRE(test_result == XR_SUCCESS);
        CHECK(out_layer_value == num_valid_jsons);
    }

    // Try property return
    in_layer_value = out_layer_value;
    out_layer_value = 0;
    std::vector<XrApiLayerProperties> layer_props(in_layer_value, {XR_TYPE_API_LAYER_PROPERTIES});
    CHECK(XR_SUCCESS == (test_result = xrEnumerateApiLayerProperties(in_layer_value, &out_layer_value, layer_props.data())));

    if (test_result == XR_SUCCESS) {
        uint16_t expected_major = XR_VERSION_MAJOR(XR_CURRENT_API_VERSION);
        uint16_t expected_minor = XR_VERSION_MINOR(XR_CURRENT_API_VERSION);

        CAPTURE(layer_props);

        {
            INFO("Looking for good API layer that uses an absolute path to the binary");

            const auto layerNamePredicate = [](const XrApiLayerProperties& prop) {
                return 0 == strcmp(prop.layerName, "XR_APILAYER_test");
            };
            REQUIRE_THAT(layer_props,
                         VectorContainsPredicate<XrApiLayerProperties>(layerNamePredicate, "layer name is XR_APILAYER_test"));
            const auto it = std::find_if(layer_props.begin(), layer_props.end(), layerNamePredicate);
            REQUIRE(it != layer_props.end());
            CHECK(1 == it->layerVersion);
            CHECK(XR_MAKE_VERSION(expected_major, expected_minor, 0U) == it->specVersion);
            CHECK(std::string("Test_description") == it->description);
        }

        {
            INFO("Looking for good API layer that uses a relative path to the binary");

            const auto layerNamePredicate = [](const XrApiLayerProperties& prop) {
                return 0 == strcmp(prop.layerName, "XR_APILAYER_LUNARG_test_good_relative_path");
            };
            REQUIRE_THAT(layer_props, VectorContainsPredicate<XrApiLayerProperties>(
                                          layerNamePredicate, "layer name is XR_APILAYER_LUNARG_test_good_relative_path"));
            const auto it = std::find_if(layer_props.begin(), layer_props.end(), layerNamePredicate);
            REQUIRE(it != layer_props.end());
            CHECK(1 == it->layerVersion);
            CHECK(XR_MAKE_VERSION(expected_major, expected_minor, 0U) == it->specVersion);
            CHECK(std::string("Test_description") == it->description);
        }
        {
            INFO("Checking that no layers described with bad JSON were enumerated");
            CHECK_THAT(layer_props, !VectorContainsPredicate<XrApiLayerProperties>(
                                        [](const XrApiLayerProperties& prop) {
                                            return std::string::npos != std::string(prop.layerName).find("_badjson");
                                        },
                                        "layer name mentions _badjson"));
        }
    }
#endif  // defined(XR_USE_PLATFORM_ANDROID) && !defined(XR_KHR_LOADER_INIT_SUPPORT)

    // Cleanup
    CleanupEnvironmentVariables();
}

// Test the xrEnumerateInstanceExtensionProperties function through the loader.
TEST_CASE("TestEnumInstanceExtensions", "") {
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
                    TEST_EQUAL(test_result, XR_ERROR_RUNTIME_UNAVAILABLE,
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
            TEST_EQUAL(test_result, XR_SUCCESS, ("Active runtime extension enum properties query (" + subtest_name + ")").c_str());

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

    // Cleanup
    CleanupEnvironmentVariables();
}

// Test creating and destroying an OpenXR instance through the loader.
TEST_CASE("TestCreateDestroyInstance", "") {
    if (!g_has_installed_runtime) {
        SKIP("Skipped - no runtime installed");
    }

    const char invalid_layer_to_enable[] = "XrApiLayer_invalid_layer_test";
    const char valid_extension_to_enable[] = "XR_EXT_debug_utils";
    const char invalid_extension_to_enable[] = "XR_KHR_fake_ext1";
    const char* const invalid_layer_name_array[1] = {invalid_layer_to_enable};
#if defined(XR_USE_PLATFORM_ANDROID)
    const char* valid_extension_name_array[2] = {XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME, valid_extension_to_enable};
    const char* const invalid_extension_name_array[2] = {XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
                                                         invalid_extension_to_enable};
#else
    const char valid_layer_to_enable[] = "XR_APILAYER_LUNARG_api_dump";
    const char* const valid_layer_name_array[1] = {valid_layer_to_enable};
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
    layer_path = layer_path + TEST_DIRECTORY_SYMBOL + ".." + TEST_DIRECTORY_SYMBOL + ".." + TEST_DIRECTORY_SYMBOL + "api_layers";
    LoaderTestSetEnvironmentVariable("XR_API_LAYER_PATH", layer_path);

    // Make API dump write to a file, when loaded
    LoaderTestSetEnvironmentVariable("XR_API_DUMP_FILE_NAME", "api_dump_out.txt");
#else
    // XR_API_LAYER_PATH override not supported on Android
#endif  // !defined(XR_USE_PLATFORM_ANDROID)

    XrInstance instance = XR_NULL_HANDLE;
    XrResult create_result = XR_ERROR_VALIDATION_FAILURE;

    auto platform_instance_create = GetPlatformInstanceCreateExtension();
    XrInstanceCreateInfo instance_create_info = {XR_TYPE_INSTANCE_CREATE_INFO};
    instance_create_info.next = &platform_instance_create;
    instance_create_info.applicationInfo = application_info;
    instance_create_info.enabledExtensionCount = base_extension_count;
    instance_create_info.enabledExtensionNames = base_extension_names;

    // Test 0 - Basic, plain-vanilla xrCreateInstance
    SECTION("Simple xrCreateInstance") {
        INFO("xrCreateInstance");
        CHECK(XR_SUCCESS == (create_result = xrCreateInstance(&instance_create_info, &instance)));
    }

#if !defined(XR_USE_PLATFORM_ANDROID)
    // Test 1 - xrCreateInstance with a valid layer
    SECTION("xrCreateInstance with a single valid layer") {
        INFO("xrCreateInstance");
        instance_create_info.enabledApiLayerCount = 1;
        instance_create_info.enabledApiLayerNames = valid_layer_name_array;
        CHECK(XR_SUCCESS == (create_result = xrCreateInstance(&instance_create_info, &instance)));
    }
#endif  // !defined(XR_USE_PLATFORM_ANDROID)

    // Test 2 - xrCreateInstance with an invalid layer
    SECTION("xrCreateInstance with a single invalid layer") {
        INFO("xrCreateInstance");
        instance_create_info.enabledApiLayerCount = 1;
        instance_create_info.enabledApiLayerNames = invalid_layer_name_array;
        CHECK(XR_ERROR_API_LAYER_NOT_PRESENT == (create_result = xrCreateInstance(&instance_create_info, &instance)));
    }

    // Test 3 - xrCreateInstance with a valid extension
    SECTION("xrCreateInstance with a single extra valid extension") {
        INFO("xrCreateInstance");
        instance_create_info.enabledExtensionCount++;
        instance_create_info.enabledExtensionNames = valid_extension_name_array;
        CHECK(XR_SUCCESS == (create_result = xrCreateInstance(&instance_create_info, &instance)));
    }

    // Test 4 - xrCreateInstance with an invalid extension
    SECTION("xrCreateInstance with a single extra invalid extension") {
        INFO("xrCreateInstance");
        instance_create_info.enabledExtensionCount++;
        instance_create_info.enabledExtensionNames = invalid_extension_name_array;
        CHECK(XR_ERROR_EXTENSION_NOT_PRESENT == (create_result = xrCreateInstance(&instance_create_info, &instance)));
    }

#if !defined(XR_USE_PLATFORM_ANDROID)
    // Test 5 - xrCreateInstance with everything
    SECTION("xrCreateInstance with app info, valid layer, and a valid extension") {
        INFO("xrCreateInstance");
        instance_create_info.enabledApiLayerCount = 1;
        instance_create_info.enabledApiLayerNames = valid_layer_name_array;
        instance_create_info.enabledExtensionCount++;
        instance_create_info.enabledExtensionNames = valid_extension_name_array;
        CHECK(XR_SUCCESS == (create_result = xrCreateInstance(&instance_create_info, &instance)));
    }
#endif  // !defined(XR_USE_PLATFORM_ANDROID)

    SECTION("DestroyInstance with a NULL handle") {
        // Make sure DestroyInstance with a NULL handle gives correct error
        CHECK(XR_ERROR_HANDLE_INVALID == xrDestroyInstance(XR_NULL_HANDLE));
    }

    if (XR_SUCCEEDED(create_result)) {
        INFO("xrDestroyInstance");
        CHECK(XR_SUCCESS == xrDestroyInstance(instance));
    }

    // Cleanup
    CleanupEnvironmentVariables();
}

// Test at least one XrInstance function not directly implemented in the loader's manual code section.
// This is to make sure that the automatic instance functions work.
TEST_CASE("TestGetSystem", "") {
    if (!g_has_installed_runtime) {
        SKIP("Skipped - no runtime installed");
    }

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

    std::unique_ptr<XrInstance, decltype(&InstanceDeleter)> wrapperInstance({&instance, &InstanceDeleter});

    TEST_EQUAL(xrCreateInstance(&instance_create_info, &instance), XR_SUCCESS, "Creating instance");

    XrSystemGetInfo system_get_info = {XR_TYPE_SYSTEM_GET_INFO};
    system_get_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    TEST_EQUAL(xrGetSystem(instance, &system_get_info, &systemId), XR_SUCCESS, "xrGetSystem");

    TEST_EQUAL(xrDestroyInstance(instance), XR_SUCCESS, "Destroying instance");

    // Cleanup
    CleanupEnvironmentVariables();
}

// Test at least one non-XrInstance function to make sure that the automatic non-instance functions work.
TEST_CASE("TestCreateDestroyAction", "") {
    if (!g_has_installed_runtime) {
        SKIP("Skipped - no runtime installed");
    }

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

    TEST_EQUAL(xrCreateInstance(&instance_ci, &instance), XR_SUCCESS, "Creating instance");

    if (instance != XR_NULL_HANDLE) {
        XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(actionSetInfo.actionSetName, "test");
        strcpy(actionSetInfo.localizedActionSetName, "test");
        XrActionSet actionSet{XR_NULL_HANDLE};
        CHECK(XR_SUCCESS == xrCreateActionSet(instance, &actionSetInfo, &actionSet));

        if (actionSet != XR_NULL_HANDLE) {
            XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy(actionInfo.actionName, "action_test");
            strcpy(actionInfo.localizedActionName, "Action test");
            XrAction completeAction{XR_NULL_HANDLE};
            CHECK(XR_SUCCESS == xrCreateAction(actionSet, &actionInfo, &completeAction));
        }
    }

    CHECK(XR_SUCCESS == xrDestroyInstance(instance));

    // Cleanup
    CleanupEnvironmentVariables();
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

    g_has_installed_runtime = DetectInstalledRuntime();

    Catch::Session().run();

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

    g_has_installed_runtime = DetectInstalledRuntime();

    int result = Catch::Session().run();

#if FILTER_OUT_LOADER_ERRORS == 1
    // Restore std::cerr to the original buffer
    std::cerr.rdbuf(original_cerr);
#endif

    return result;
}
#endif  // defined(XR_USE_PLATFORM_ANDROID)
