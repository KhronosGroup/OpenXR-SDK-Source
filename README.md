# OpenXR™ Software Development Kit (SDK) Sources Project

<!--
Copyright (c) 2017-2022, The Khronos Group Inc.

SPDX-License-Identifier: CC-BY-4.0
-->

BRANCH: QUEST CLIENT USING OCULUS INSTRUCTIONS FROM HERE:

https://developer.oculus.com/documentation/native/android/mobile-build-run-hello-xr-app/

QUEST PRO EYE TRACKING sample on PC VR via Link / AirLink (enable Dev mode + eye-tracking sliders in Oculus PC app):


![image](https://user-images.githubusercontent.com/11604039/200199048-adeef6c8-d349-436f-80ff-d237c35244c4.png)


NB This same exact code should work on Quest Pro standalone mode, but I didn't add the request permissions Java code for eye-tracking (which needs to be accepted for it to work). There is no java code in this project so it will probably have to be done via JNI / Android Native somehow. I'll try to add that soon.

Body tracking extension works fine on Standalone but gives a runtime error on PC via Link.


![image](https://user-images.githubusercontent.com/11604039/200270625-e627a78b-5d4e-409f-80da-79bebe81bb63.png)

