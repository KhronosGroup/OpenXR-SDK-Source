// Copyright (c) 2017-2020 The Khronos Group Inc
//
// SPDX-License-Identifier: Apache-2.0
package com.khronos.hello_xr;

public class MainActivity extends android.app.NativeActivity {
  static {
    // Note: Must handle loading of dependent shared libraries manually before
    // the shared library that depends on them is loaded, since there is not
    // currently a way to specify a shared library dependency for NativeActivity
    // via the manifest meta-data.
    System.loadLibrary("openxr_loader");
    System.loadLibrary("hello_xr");
  }
}
