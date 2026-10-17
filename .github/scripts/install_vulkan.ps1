# Copyright (c) 2019-2026 The Khronos Group Inc.
# SPDX-License-Identifier: Apache-2.0

$ErrorActionPreference = 'Stop'

if (-not $env:VULKAN_SDK_VERSION) {
    $env:VULKAN_SDK_VERSION = "1.1.114.0"
}

$Arm64 = $env:PROCESSOR_ARCHITECTURE -eq "ARM64"
if ($Arm64) {
    $env:VULKAN_SDK_VERSION = "1.4.357.0"
}

$SDK_VER = $env:VULKAN_SDK_VERSION

if (-not (Test-Path env:VULKAN_SDK)) {
    if ($env:SYSTEM_DEFAULTWORKINGDIRECTORY) {
        $env:VULKAN_SDK = "$env:SYSTEM_DEFAULTWORKINGDIRECTORY\vulkan_sdk\$SDK_VER"
    }
    else {
        $env:VULKAN_SDK = Join-Path (Get-Location) "VulkanSDK\$SDK_VER"
    }
}
$parent = Split-Path -Path $env:VULKAN_SDK
Write-Output "Trying for Vulkan SDK $SDK_VER"
if ($Arm64) {
    $FN = "vulkansdk-windows-ARM64-$SDK_VER.exe"
    $URL = "https://sdk.lunarg.com/sdk/download/$SDK_VER/warm/$FN"
}
else {
    $FN = "vksdk-$SDK_VER-lite.7z"
    $URL = "https://people.collabora.com/~rpavlik/ci_resources/$FN"
}
if (-not (Test-Path "$env:VULKAN_SDK/Include/vulkan/vulkan.h")) {
    Write-Output "Downloading $URL"
    $wc = New-Object System.Net.WebClient
    $wc.DownloadFile($URL, "$(Get-Location)\$FN")

    Write-Output "Installing $FN in silent, blocking mode to $env:VULKAN_SDK"
    if ($Arm64) {
        Start-Process "$(Get-Location)\$FN" -ArgumentList "--root", $env:VULKAN_SDK, "--accept-licenses", "--default-answer", "--confirm-command", "install", "copy_only=1" -Wait
    }
    else {
        Start-Process "c:\Program Files\7-Zip\7z" -ArgumentList "x", $FN, "-o$parent" -Wait
    }
}
else {
    Write-Output "$env:VULKAN_SDK found and contains header"
}

Write-Output "VULKAN_SDK=$env:VULKAN_SDK" >> $env:GITHUB_ENV
