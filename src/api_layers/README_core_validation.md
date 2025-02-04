# The Core Validation API Layer

<!--
Copyright (c) 2017-2025 The Khronos Group Inc.

SPDX-License-Identifier: CC-BY-4.0
-->

## Layer Name

`XR_APILAYER_LUNARG_core_validation`

## Description

The Core Validation API layer is responsible for verifying that an
application is properly using the OpenXR API.  It does this by
enforcing `Valid Usage` statements made in the OpenXR spec.

Developers should use this layer during application development to ensure
that they are properly using the OpenXR API.  This layer **should not be
enabled** in any released application, or performance will be negatively impacted.

## Settings

### Outputting to Text

There are three modes currently supported:

1. Output text to stdout
2. Output text to a file
3. Output HTML content to a file
4. Output to the application using the `XR_EXT_debug_utils` extension

Core Validation API layer outputs content to stdout by default.

`XR_CORE_VALIDATION_EXPORT_TYPE` is used to change the type of output from
the API layer.  Currently, this can be set to the following:

* `html`  : This will generate HTML formatted content.
* `none`  : This will disable output.

`XR_CORE_VALIDATION_FILE_NAME` is used to define the file name that is
written to.  If not defined, the information goes to stdout.  If defined,
then the file will be written with the output of the Core Validation API
layer.

### Outputting to `XR_EXT_debug_utils`

If you desire to capture the output using the `XR_EXT_debug_utils` extension,
create a valid debug callback based on the definition of
`PFN_xrDebugUtilsMessengerCallbackEXT` and create an `XrDebugUtilsMessengerEXT`
object with the following:

* `XrDebugUtilsMessengerCreateInfoEXT`:`messageSeverities` set to accept at
  least `XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT`, but preferably also
  `XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT`.
* `XrDebugUtilsMessengerCreateInfoEXT`:`messageTypes` set to accept at least
  `XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT`.

Also, you must create an instance with the `XR_APILAYER_LUNARG_core_validation`
layer enabled.
Once this is done, all validation messages will be sent to your debug callback.

For more info on the `XR_EXT_debug_utils` extension, refer to the OpenXR
specification.

## Example Output

### Example Text Output

By default, the core_validation API layer will output information
to stdout.  However, it supports two environmental variables:

```sh
export XR_CORE_VALIDATION_EXPORT_TYPE=text
export XR_CORE_VALIDATION_FILE_NAME=my_validation_output.txt
```

On Android, the equivalent settings are:
```sh
adb shell setprop debug.core_validation_export_type 'text'
adb shell setprop debug.core_validation_file_name '/sdcard/my_validation_output.txt'
```

When the `XR_APILAYER_LUNARG_core_validation` layer is enabled, the
output (whether to stdout or a file) should look like the following:

![Core Validation Message](./core_validation_message.svg)

You'll notice that the output is broken up into various sections:

* The **severity** of message:
  * `VALID_ERROR` - The message indicates a validation error.
  * `VALID_WARNING` - The message indicates a validation warning. This may be
    valid behavior, but it may not.
  * `VALID_INFO` - The message indicates some potentially useful information
    about validation. Not an error or warning, but could be something of
    interest.
* The unique **Valid Usage ID [VUID]** for the Valid Usage statement that is of
  interest to this message
  * You can jump to the section of the OpenXR specification that is associated
    with this message by opening the HTML version of the spec, and appending the
    hash-tag symbol (`#`) followed by the VUID.
    * For example, if you were interested in the VUID
      `VUID-xrGetSystem-getInfo-parameter` as shown in the image, you would add
      `#VUID-xrGetSystem-getInfo-parameter` to the end of the spec URL and get
      something like the following: <br/> <br/>
      ![Core Validation HTML Anchor](./core_validation_html_anchor.png)
* The name of the **OpenXR function** triggering this message
  * In this case, the error occurred in a call to the `xrGetSystem` function.
* A short, but **descriptive message** of what likely caused issues.
* A list of associated OpenXR **objects** (handles) to help identify the
  particular case causing the failure.

### Example HTML Output

When the `XR_APILAYER_LUNARG_core_validation` API layer is enabled, and the user
has enabled HTML output, the resulting HTML file contents (when rendered through
an internet browser) should look like the following:

![HTML Output Example](./OpenXR_Core_Validation.png)
