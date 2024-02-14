# OpenXR™ Software Development Kit (SDK) Sources Project

<!--
Copyright (c) 2017-2024, The Khronos Group Inc.

SPDX-License-Identifier: CC-BY-4.0
-->

BRANCH: QUEST CLIENT USING OCULUS INSTRUCTIONS FROM HERE:

https://developer.oculus.com/documentation/native/android/mobile-build-run-hello-xr-app/

QUEST PRO EYE TRACKING sample on PC VR via Link / AirLink (enable Dev mode + eye-tracking sliders in Oculus PC app):


NB This same exact code should work on Quest Pro standalone mode, but I didn't add the request permissions Java code for eye-tracking (which needs to be accepted for it to work). There is no java code in this project so it will probably have to be done via JNI / Android Native somehow. I'll try to add that soon.

Body tracking extension works fine on Standalone but gives a runtime error on PC via Link.


![image](https://user-images.githubusercontent.com/11604039/200270625-e627a78b-5d4e-409f-80da-79bebe81bb63.png)

I also implemented waist-oriented locomotion, where available. This works on Quest 1, 2, and Pro, both on Android and Link/AirLink on PC. 

I posted a hello_xr.exe test app and an OpenGL ES / Vulkan pre-built APK for SideLoading / ADB pushing to try it out quickly.


https://github.com/BattleAxeVR/OpenXR-SDK-Source/assets/11604039/57f9c068-79d2-402f-b37b-58a24c3bdea0

NB: On standalone builds, you need to run this ADB command to enable experimental features:

adb shell setprop debug.oculus.experimentalEnabled 1

See:

https://developer.oculus.com/documentation/native/android/mobile-experimental-features/

STANDALONE TEST BUILD with SNAP TURN ENABLED:
https://github.com/BattleAxeVR/OpenXR-SDK-Source/blob/main/hello_xr-Vulkan-release.apk

PC VR TEST BUILD with SNAP TURN ENABLED:
https://github.com/BattleAxeVR/OpenXR-SDK-Source/blob/main/hello_xr.exe


-----
UPDATE: Support for HTC Vive Ultimate trackers (and presumably older HTC Trackers) has been added, for the waist to use for waist-oriented locomotion. It works VERY well, very 
responsive to body movements.

Video is here:

https://twitter.com/BattleAxeVR/status/1757592442923032595

Another video of 5 VUTs all running at the same time is here:

https://twitter.com/BattleAxeVR/status/1757593341586259986
