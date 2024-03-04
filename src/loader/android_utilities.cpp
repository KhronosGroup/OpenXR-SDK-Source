// Copyright (c) 2020-2024, The Khronos Group Inc.
// Copyright (c) 2020-2021, Collabora, Ltd.
//
// SPDX-License-Identifier:  Apache-2.0 OR MIT
//
// Initial Author: Rylie Pavlik <rylie.pavlik@collabora.com>

#include "android_utilities.h"

#ifdef __ANDROID__
#include <wrap/android.net.h>
#include <wrap/android.content.h>
#include <wrap/android.database.h>
#include <json/value.h>

#include <openxr/openxr.h>

#include <dlfcn.h>
#include <vector>
#include <android/log.h>

#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, "OpenXR-Loader", __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, "OpenXR-Loader", __VA_ARGS__)
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, "OpenXR-Loader", __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, "OpenXR-Loader", __VA_ARGS__)

namespace openxr_android {
using wrap::android::database::Cursor;
using wrap::android::net::Uri;
using wrap::android::net::Uri_Builder;

// Code in here corresponds roughly to the Java "BrokerContract" class and subclasses.
namespace {
constexpr auto AUTHORITY = "org.khronos.openxr.runtime_broker";
constexpr auto SYSTEM_AUTHORITY = "org.khronos.openxr.system_runtime_broker";
constexpr auto BASE_PATH = "openxr";
constexpr auto ABI_PATH = "abi";
constexpr auto RUNTIMES_PATH = "runtimes";
constexpr auto API_LAYERS_PATH = "api_layers";
constexpr auto IMP_LAYER = "implicit";
constexpr auto EXP_LAYER = "explicit";

constexpr const char *getBrokerAuthority(bool systemBroker) { return systemBroker ? SYSTEM_AUTHORITY : AUTHORITY; }
constexpr const char *getLayerTypePath(bool implicitLayer) { return implicitLayer ? IMP_LAYER : EXP_LAYER; }

struct BaseColumns {
    /**
     * The unique ID for a row.
     */
    [[maybe_unused]] static constexpr auto ID = "_id";
};

/**
 * Contains details for the /openxr/[major_ver]/abi/[abi]/runtimes/active URI.
 * <p>
 * This URI represents a "table" containing at most one item, the currently active runtime. The
 * policy of which runtime is chosen to be active (if more than one is installed) is left to the
 * content provider.
 * <p>
 * No sort order is required to be honored by the content provider.
 */
namespace active_runtime {
/**
 * Final path component to this URI.
 */
static constexpr auto TABLE_PATH = "active";

/**
 * Create a content URI for querying the data on the active runtime for a
 * given major version of OpenXR.
 *
 * @param systemBroker If the system runtime broker (instead of the installable one) should be queried.
 * @param majorVer The major version of OpenXR.
 * @param abi The Android ABI name in use.
 * @return A content URI for a single item: the active runtime.
 */
static Uri makeContentUri(bool systemBroker, int majorVersion, const char *abi) {
    auto builder = Uri_Builder::construct();
    builder.scheme("content")
        .authority(getBrokerAuthority(systemBroker))
        .appendPath(BASE_PATH)
        .appendPath(std::to_string(majorVersion))
        .appendPath(ABI_PATH)
        .appendPath(abi)
        .appendPath(RUNTIMES_PATH)
        .appendPath(TABLE_PATH);
    return builder.build();
}

struct Columns : BaseColumns {
    /**
     * Constant for the PACKAGE_NAME column name
     */
    static constexpr auto PACKAGE_NAME = "package_name";

    /**
     * Constant for the NATIVE_LIB_DIR column name
     */
    static constexpr auto NATIVE_LIB_DIR = "native_lib_dir";

    /**
     * Constant for the SO_FILENAME column name
     */
    static constexpr auto SO_FILENAME = "so_filename";

    /**
     * Constant for the HAS_FUNCTIONS column name.
     * <p>
     * If this column contains true, you should check the /functions/ URI for that runtime.
     */
    static constexpr auto HAS_FUNCTIONS = "has_functions";
};
}  // namespace active_runtime

/**
 * Contains details for the /openxr/[major_ver]/abi/[abi]/runtimes/[package]/functions URI.
 * <p>
 * This URI is for package-specific function name remapping. Since this is an optional field in
 * the corresponding JSON manifests for OpenXR, it is optional here as well. If the active
 * runtime contains "true" in its "has_functions" column, then this table must exist and be
 * queryable.
 * <p>
 * No sort order is required to be honored by the content provider.
 */
namespace functions {
/**
 * Final path component to this URI.
 */
static constexpr auto TABLE_PATH = "functions";

/**
 * Create a content URI for querying all rows of the runtime function remapping data for a given
 * runtime package and major version of OpenXR.
 *
 * @param systemBroker If the system runtime broker (instead of the installable one) should be queried.
 * @param majorVer    The major version of OpenXR.
 * @param packageName The package name of the runtime.
 * @param abi The Android ABI name in use.
 * @return A content URI for the entire table: the function remapping for that runtime.
 */
static Uri makeRuntimeContentUri(bool systemBroker, int majorVersion, std::string const &packageName, const char *abi) {
    auto builder = Uri_Builder::construct();
    builder.scheme("content")
        .authority(getBrokerAuthority(systemBroker))
        .appendPath(BASE_PATH)
        .appendPath(std::to_string(majorVersion))
        .appendPath(ABI_PATH)
        .appendPath(abi)
        .appendPath(RUNTIMES_PATH)
        .appendPath(packageName)
        .appendPath(TABLE_PATH);
    return builder.build();
}

/**
 * Create a content URI for querying all rows of the API Layer function remapping data for a given
 * runtime package, major version of OpenXR, and layer name,
 *
 * @param systemBroker If the system runtime broker (instead of the installable one) should be queried.
 * @param majorVer    The major version of OpenXR.
 * @param packageName The package name of the runtime.
 * @param abi The Android ABI name in use.
 * @param layerName The name of the API layer.
 * @return A content URI for the entire table: the function remapping for that runtime and layer.
 */
static Uri makeLayerContentUri(bool systemBroker, int majorVersion, std::string const &packageName, const char *abi,
                               std::string const &layerName) {
    auto builder = Uri_Builder::construct();
    builder.scheme("content")
        .authority(getBrokerAuthority(systemBroker))
        .appendPath(BASE_PATH)
        .appendPath(std::to_string(majorVersion))
        .appendPath(ABI_PATH)
        .appendPath(abi)
        .appendPath(API_LAYERS_PATH)
        .appendPath(packageName)
        .appendPath(layerName)
        .appendPath(TABLE_PATH);
    return builder.build();
}

struct Columns : BaseColumns {
    /**
     * Constant for the FUNCTION_NAME column name
     */
    static constexpr auto FUNCTION_NAME = "function_name";

    /**
     * Constant for the SYMBOL_NAME column name
     */
    static constexpr auto SYMBOL_NAME = "symbol_name";
};
}  // namespace functions

namespace instance_extensions {
/**
 * Final path component to this URI.
 */
static constexpr auto TABLE_PATH = "instance_extensions";

/**
 * Create a content URI for querying all rows of the instance extensions supported by a given
 * API layer.
 *
 * @param systemBroker If the system runtime broker (instead of the installable one) should be queried.
 * @param majorVer    The major version of OpenXR.
 * @param packageName The package name of the runtime.
 * @param abi The Android ABI name in use.
 * @param layerName The API layer name.
 * @return A content URI for the entire table: the function remapping for that runtime.
 */
static Uri makeContentUri(bool systemBroker, int majorVersion, std::string const &packageName, const char *abi,
                          std::string const &layerName) {
    auto builder = Uri_Builder::construct();
    builder.scheme("content")
        .authority(getBrokerAuthority(systemBroker))
        .appendPath(BASE_PATH)
        .appendPath(std::to_string(majorVersion))
        .appendPath(ABI_PATH)
        .appendPath(abi)
        .appendPath(API_LAYERS_PATH)
        .appendPath(packageName)
        .appendPath(layerName)
        .appendPath(TABLE_PATH);
    return builder.build();
}
struct Columns : BaseColumns {
    /**
     * Constant for the INSTANCE_EXTENSION_NAMES column name
     */
    static constexpr auto INSTANCE_EXTENSION_NAMES = "instance_extension_names";

    /**
     * Constant for the INSTANCE_EXTENSION_VERSIONS column name
     */
    static constexpr auto INSTANCE_EXTENSION_VERSIONS = "instance_extension_versions";
};
}  // namespace instance_extensions

namespace api_layer {

/**
 * Create a content URI for querying all rows of the implicit/explicit API layer data for a given
 * runtime package and major version of OpenXR.
 *
 * @param systemBroker If the system runtime broker (instead of the installable one) should be queried.
 * @param majorVer    The major version of OpenXR.
 * @param layerType The layer type of the API layer.
 * @param abi The Android ABI name in use.
 * @return A content URI for the entire table: the API layers for that runtime.
 */
static Uri makeContentUri(bool systemBroker, int majorVersion, std::string const &layerType, const char *abi) {
    auto builder = Uri_Builder::construct();
    builder.scheme("content")
        .authority(getBrokerAuthority(systemBroker))
        .appendPath(BASE_PATH)
        .appendPath(std::to_string(majorVersion))
        .appendPath(ABI_PATH)
        .appendPath(abi)
        .appendPath(API_LAYERS_PATH)
        .appendPath(getLayerTypePath(layerType == IMP_LAYER));
    return builder.build();
}
struct Columns : BaseColumns {
    // implicit or explicit
    static constexpr auto PACKAGE_NAME = "package_name";
    static constexpr auto FILE_FORMAT_VERSION = "file_format_version";
    static constexpr auto NAME = "name";
    static constexpr auto NATIVE_LIB_DIR = "native_lib_dir";
    static constexpr auto SO_FILENAME = "so_filename";
    static constexpr auto API_VERSION = "api_version";
    static constexpr auto IMPLEMENTATION_VERSION = "implementation_version";
    static constexpr auto DESCRIPTION = "description";
    static constexpr auto DISABLE_ENVIRONMENT = "disable_environment";
    static constexpr auto ENABLE_ENVIRONMENT = "enable_environment";
    static constexpr auto DISABLE_SYS_PROP = "disable_sys_prop";
    static constexpr auto ENABLE_SYS_PROP = "enable_sys_prop";
    static constexpr auto HAS_FUNCTIONS = "has_functions";
    static constexpr auto HAS_INSTANCE_EXTENSIONS = "has_instance_extensions";
};
}  // namespace api_layer

}  // namespace

static inline jni::Array<std::string> makeArray(std::initializer_list<const char *> &&list) {
    auto ret = jni::Array<std::string>{(long)list.size()};
    long i = 0;
    for (auto &&elt : list) {
        ret.setElement(i, elt);
        ++i;
    }
    return ret;
}

#if defined(__arm__)
static constexpr auto ABI = "armeabi-v7l";
#elif defined(__aarch64__)
static constexpr auto ABI = "arm64-v8a";
#elif defined(__i386__)
static constexpr auto ABI = "x86";
#elif defined(__x86_64__)
static constexpr auto ABI = "x86_64";
#else
#error "Unknown ABI!"
#endif

static inline Json::Value makeMinimumVirtualRuntimeManifest(const std::string &libraryPath) {
    Json::Value root_node(Json::objectValue);

    root_node["file_format_version"] = "1.0.0";
    root_node["instance_extensions"] = Json::Value(Json::arrayValue);
    root_node["functions"] = Json::Value(Json::objectValue);
    root_node["runtime"] = Json::objectValue;
    root_node["runtime"]["library_path"] = libraryPath;
    return root_node;
}

static constexpr const char *getBrokerTypeName(bool systemBroker) { return systemBroker ? "system" : "installable"; }

// The current file relies on android-jni-wrappers and jnipp, which may throw on failure.
// This is problematic when the loader is compiled with exception handling disabled - the consumers can reasonably
// expect that the compilation with -fno-exceptions will succeed, but the compiler will not accept the code that
// uses `try` & `catch` keywords. We cannot use the `exception_handling.hpp` here since we're not at an ABI boundary,
// so we define helper macros here. This is fine for now since the only occurrence of exception-handling code is in this file.
#ifdef XRLOADER_DISABLE_EXCEPTION_HANDLING

#define ANDROID_UTILITIES_TRY
#define ANDROID_UTILITIES_CATCH_FALLBACK(...)

#else

#define ANDROID_UTILITIES_TRY try
#define ANDROID_UTILITIES_CATCH_FALLBACK(...) \
    catch (const std::exception &e) {         \
        __VA_ARGS__                           \
    }

#endif  // XRLOADER_DISABLE_EXCEPTION_HANDLING

/// Generic content resolver query function
static bool getCursor(wrap::android::content::Context const &context, jni::Array<std::string> const &projection, Uri const &uri,
                      bool systemBroker, const char *contentDesc, Cursor &out_cursor) {
    ALOGI("getCursor: Querying URI: %s", uri.toString().c_str());

    ANDROID_UTILITIES_TRY { out_cursor = context.getContentResolver().query(uri, projection); }
    ANDROID_UTILITIES_CATCH_FALLBACK({
        ALOGI("Exception when querying %s content resolver for %s: %s", getBrokerTypeName(systemBroker), contentDesc, e.what());
        out_cursor = {};
        return false;
    })

    if (out_cursor.isNull()) {
        ALOGI("Null cursor when querying %s content resolver for %s.", getBrokerTypeName(systemBroker), contentDesc);
        out_cursor = {};
        return false;
    }
    if (out_cursor.getCount() < 1) {
        ALOGI("Non-null but empty cursor when querying %s content resolver for %s.", getBrokerTypeName(systemBroker), contentDesc);
        out_cursor.close();
        out_cursor = {};
        return false;
    }
    return true;
}

/// Shared helper for populating the functions object in a manifest
static void populateFunctionsData(Cursor &cursor, Json::Value &functionsObject) {
    auto functionIndex = cursor.getColumnIndex(functions::Columns::FUNCTION_NAME);
    auto symbolIndex = cursor.getColumnIndex(functions::Columns::SYMBOL_NAME);
    while (cursor.moveToNext()) {
        functionsObject[cursor.getString(functionIndex)] = cursor.getString(symbolIndex);
    }

    cursor.close();
}

static int populateRuntimeFunctions(wrap::android::content::Context const &context, bool systemBroker,
                                    const std::string &packageName, Json::Value &manifest) {
    const jni::Array<std::string> projection = makeArray({functions::Columns::FUNCTION_NAME, functions::Columns::SYMBOL_NAME});

    auto uri = functions::makeRuntimeContentUri(systemBroker, XR_VERSION_MAJOR(XR_CURRENT_API_VERSION), packageName, ABI);
    ALOGI("populateFunctions: Querying URI: %s", uri.toString().c_str());
    Cursor cursor;
    if (!getCursor(context, projection, uri, systemBroker, "functions", cursor)) {
        return -1;
    }
    populateFunctionsData(cursor, manifest["functions"]);

    return 0;
}

/// Get cursor for active runtime, parameterized by whether or not we use the system broker
static bool getActiveRuntimeCursor(wrap::android::content::Context const &context, jni::Array<std::string> const &projection,
                                   bool systemBroker, Cursor &cursor) {
    auto uri = active_runtime::makeContentUri(systemBroker, XR_VERSION_MAJOR(XR_CURRENT_API_VERSION), ABI);
    ALOGI("getActiveRuntimeCursor: Querying URI: %s", uri.toString().c_str());
    return getCursor(context, projection, uri, systemBroker, "active runtime", cursor);
}

int getActiveRuntimeVirtualManifest(wrap::android::content::Context const &context, Json::Value &virtualManifest,
                                    bool &systemBroker) {
    const jni::Array<std::string> projection =
        makeArray({active_runtime::Columns::PACKAGE_NAME, active_runtime::Columns::NATIVE_LIB_DIR,
                   active_runtime::Columns::SO_FILENAME, active_runtime::Columns::HAS_FUNCTIONS});
    static bool hasQueryBroker = false;
    static Json::Value runtimeManifest;
    if (!hasQueryBroker) {
        // First, try getting the installable broker's provider
        systemBroker = false;
        Cursor cursor;
        if (!getActiveRuntimeCursor(context, projection, systemBroker, cursor)) {
            // OK, try the system broker as a fallback.
            systemBroker = true;
            getActiveRuntimeCursor(context, projection, systemBroker, cursor);
        }
        hasQueryBroker = true;

        if (cursor.isNull()) {
            // Couldn't find either broker
            ALOGI("Could access neither the installable nor system runtime broker.");
            return -1;
        }

        cursor.moveToFirst();

        do {
            auto filename = cursor.getString(cursor.getColumnIndex(active_runtime::Columns::SO_FILENAME));
            auto libDir = cursor.getString(cursor.getColumnIndex(active_runtime::Columns::NATIVE_LIB_DIR));
            auto packageName = cursor.getString(cursor.getColumnIndex(active_runtime::Columns::PACKAGE_NAME));

            auto hasFunctions = cursor.getInt(cursor.getColumnIndex(active_runtime::Columns::HAS_FUNCTIONS)) == 1;
            ALOGI("Got runtime: package: %s, so filename: %s, native lib dir: %s, has functions: %s", packageName.c_str(),
                  filename.c_str(), libDir.c_str(), (hasFunctions ? "yes" : "no"));

            auto lib_path = libDir + "/" + filename;
            auto *lib = dlopen(lib_path.c_str(), RTLD_LAZY | RTLD_LOCAL);
            if (lib) {
                // we found a runtime that we can dlopen, use it.
                dlclose(lib);

                Json::Value manifest = makeMinimumVirtualRuntimeManifest(lib_path);
                if (hasFunctions) {
                    int result = populateRuntimeFunctions(context, systemBroker, packageName, manifest);
                    if (result != 0) {
                        ALOGW("Unable to populate functions from runtime: %s, checking for more records...", lib_path.c_str());
                        continue;
                    }
                }
                runtimeManifest = manifest;
                cursor.close();
                virtualManifest = runtimeManifest;
                return 0;
            }
            // this runtime was not accessible, see if the broker has more runtimes on
            // offer.
            ALOGV("Unable to open broker provided runtime at %s, checking for more records...", lib_path.c_str());
        } while (cursor.moveToNext());

        ALOGE("Unable to open any of the broker provided runtimes.");
        cursor.close();
        return -1;
    }

    virtualManifest = runtimeManifest;
    return 0;
}

static int populateApiLayerFunctions(wrap::android::content::Context const &context, bool systemBroker,
                                     const std::string &packageName, std::string const &layerName, Json::Value &rootNode) {
    const jni::Array<std::string> projection = makeArray({functions::Columns::FUNCTION_NAME, functions::Columns::SYMBOL_NAME});

    auto uri = functions::makeLayerContentUri(systemBroker, XR_VERSION_MAJOR(XR_CURRENT_API_VERSION), packageName, ABI, layerName);
    ALOGI("populateApiLayerFunctions: Querying URI: %s", uri.toString().c_str());

    Cursor cursor;
    if (!getCursor(context, projection, uri, systemBroker, "API layer functions", cursor)) {
        return -1;
    }

    populateFunctionsData(cursor, rootNode["api_layer"]["functions"]);

    cursor.close();
    return 0;
}

static int populateApiLayerInstanceExtensions(wrap::android::content::Context const &context, bool systemBroker,
                                              const std::string &packageName, std::string const &layerName, Json::Value &rootNode) {
    const jni::Array<std::string> projection = makeArray(
        {instance_extensions::Columns::INSTANCE_EXTENSION_NAMES, instance_extensions::Columns::INSTANCE_EXTENSION_VERSIONS});

    auto uri =
        instance_extensions::makeContentUri(systemBroker, XR_VERSION_MAJOR(XR_CURRENT_API_VERSION), packageName, ABI, layerName);
    ALOGI("populateApiLayerInstanceExtensions: Querying URI: %s", uri.toString().c_str());

    Cursor cursor;
    if (!getCursor(context, projection, uri, systemBroker, "API layer instance extensions", cursor)) {
        return -1;
    }

    auto nameIndex = cursor.getColumnIndex(instance_extensions::Columns::INSTANCE_EXTENSION_NAMES);
    auto versionIndex = cursor.getColumnIndex(instance_extensions::Columns::INSTANCE_EXTENSION_VERSIONS);
    Json::Value extension(Json::objectValue);
    while (cursor.moveToNext()) {
        extension["name"] = cursor.getString(nameIndex);
        extension["extension_version"] = cursor.getString(versionIndex);
        rootNode["api_layer"]["instance_extensions"].append(extension);
    }

    cursor.close();
    return 0;
}

/// Get cursor for API layers, parameterized by whether or not we use the system broker
static bool getApiLayerCursor(wrap::android::content::Context const &context, jni::Array<std::string> const &projection,
                              std::string const &targetType, bool systemBroker, Cursor &cursor) {
    auto uri = api_layer::makeContentUri(systemBroker, XR_VERSION_MAJOR(XR_CURRENT_API_VERSION), targetType, ABI);
    ALOGI("getApiLayerCursor: Querying URI: %s", uri.toString().c_str());
    return getCursor(context, projection, uri, systemBroker, "API Layer", cursor);
}

static Json::Value makeApiLayerManifest(bool systemBroker, std::string layerType, wrap::android::content::Context const &context,
                                        Cursor &cursor) {
    auto packageName = cursor.getString(cursor.getColumnIndex(api_layer::Columns::PACKAGE_NAME));
    auto fileFormatVersion = cursor.getString(cursor.getColumnIndex(api_layer::Columns::FILE_FORMAT_VERSION));
    auto name = cursor.getString(cursor.getColumnIndex(api_layer::Columns::NAME));
    auto libDir = cursor.getString(cursor.getColumnIndex(active_runtime::Columns::NATIVE_LIB_DIR));
    auto fileName = cursor.getString(cursor.getColumnIndex(api_layer::Columns::SO_FILENAME));
    auto apiVersion = cursor.getString(cursor.getColumnIndex(api_layer::Columns::API_VERSION));
    auto implementationVersion = cursor.getString(cursor.getColumnIndex(api_layer::Columns::IMPLEMENTATION_VERSION));
    auto description = cursor.getString(cursor.getColumnIndex(api_layer::Columns::DESCRIPTION));
    auto disable_environment = cursor.getString(cursor.getColumnIndex(api_layer::Columns::DISABLE_ENVIRONMENT));
    auto enable_environment = cursor.getString(cursor.getColumnIndex(api_layer::Columns::ENABLE_ENVIRONMENT));
    auto disable_sys_prop = cursor.getString(cursor.getColumnIndex(api_layer::Columns::DISABLE_SYS_PROP));
    auto enable_sys_prop = cursor.getString(cursor.getColumnIndex(api_layer::Columns::ENABLE_SYS_PROP));
    auto has_instance_extensions = cursor.getString(cursor.getColumnIndex(api_layer::Columns::HAS_INSTANCE_EXTENSIONS));
    auto has_functions = cursor.getString(cursor.getColumnIndex(api_layer::Columns::HAS_FUNCTIONS));

    ALOGI("Got api layer: type: %s, name: %s, native lib dir: %s, fileName: %s", layerType.c_str(), name.c_str(), libDir.c_str(),
          fileName.c_str());

    Json::Value rootNode(Json::objectValue);
    rootNode["file_format_version"] = fileFormatVersion;
    rootNode["api_layer"] = Json::objectValue;
    rootNode["api_layer"]["name"] = name;
    rootNode["api_layer"]["library_path"] = libDir + "/" + fileName;
    rootNode["api_layer"]["api_version"] = apiVersion;
    rootNode["api_layer"]["implementation_version"] = implementationVersion;
    rootNode["api_layer"]["description"] = description;
    rootNode["api_layer"]["disable_environment"] = disable_environment;
    rootNode["api_layer"]["enable_environment"] = enable_environment;
    rootNode["api_layer"]["disable_sys_prop"] = disable_sys_prop;
    rootNode["api_layer"]["enable_sys_prop"] = enable_sys_prop;
    if (has_functions == "true") {
        rootNode["api_layer"]["functions"] = Json::Value(Json::objectValue);
        populateApiLayerFunctions(context, systemBroker, packageName, name, rootNode);
    }
    if (has_instance_extensions == "true") {
        rootNode["api_layer"]["instance_extensions"] = Json::Value(Json::arrayValue);
        populateApiLayerInstanceExtensions(context, systemBroker, packageName, name, rootNode);
    }

    return rootNode;
}

static int enumerateApiLayerManifests(std::string layerType, wrap::android::content::Context const &context,
                                      const jni::Array<std::string> &projection, std::vector<Json::Value> &virtualManifests,
                                      bool systemBroker) {
    Cursor cursor;

    if (!getApiLayerCursor(context, projection, layerType, systemBroker, cursor)) {
        return -1;
    }

    cursor.moveToFirst();
    const int32_t n = cursor.getCount();
    virtualManifests.reserve(virtualManifests.size() + n);

    for (int32_t i = 0; i < n; ++i) {
        virtualManifests.emplace_back(makeApiLayerManifest(systemBroker, layerType, context, cursor));
        cursor.moveToNext();
    }

    cursor.close();
    return 0;
}

int getApiLayerVirtualManifests(std::string layerType, wrap::android::content::Context const &context,
                                std::vector<Json::Value> &virtualManifests, bool systemBroker) {
    ALOGI("Try to get %s API layer from broker", layerType.c_str());
    const jni::Array<std::string> projection = makeArray(
        {api_layer::Columns::PACKAGE_NAME, api_layer::Columns::FILE_FORMAT_VERSION, api_layer::Columns::NAME,
         api_layer::Columns::NATIVE_LIB_DIR, api_layer::Columns::SO_FILENAME, api_layer::Columns::API_VERSION,
         api_layer::Columns::IMPLEMENTATION_VERSION, api_layer::Columns::DESCRIPTION, api_layer::Columns::DISABLE_ENVIRONMENT,
         api_layer::Columns::ENABLE_ENVIRONMENT, api_layer::Columns::DISABLE_SYS_PROP, api_layer::Columns::ENABLE_SYS_PROP,
         api_layer::Columns::HAS_FUNCTIONS, api_layer::Columns::HAS_INSTANCE_EXTENSIONS});

    if (layerType == IMP_LAYER) {
        std::vector<Json::Value> implicitLayerManifest;
        if (0 != enumerateApiLayerManifests(IMP_LAYER, context, projection, implicitLayerManifest, systemBroker)) {
            return -1;
        }
        virtualManifests = std::move(implicitLayerManifest);
    } else {
        std::vector<Json::Value> explicitLayerManifest;
        if (0 != enumerateApiLayerManifests(EXP_LAYER, context, projection, explicitLayerManifest, systemBroker)) {
            return -1;
        }
        virtualManifests = std::move(explicitLayerManifest);
    }

    return 0;
}

}  // namespace openxr_android

#endif  // __ANDROID__
