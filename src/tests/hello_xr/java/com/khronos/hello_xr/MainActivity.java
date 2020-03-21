package com.khronos.hello_xr;

public class MainActivity extends android.app.NativeActivity {
  static {
    // Note: Must handle loading of dependent shared libraries manually before
    // the shared library that depends on them is loaded, since there is not
    // currently a way to specify a shared library dependency for NativeActivity
    // via the manifest meta-data.
    System.loadLibrary("hello_xr");
  }
}
