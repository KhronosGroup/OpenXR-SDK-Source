// Copyright (c) 2017-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "pch.h"

inline XrFormFactor GetXrFormFactor(const std::string& formFactorStr) {
    if (EqualsIgnoreCase(formFactorStr, "Hmd")) {
        return XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    }
    if (EqualsIgnoreCase(formFactorStr, "Handheld")) {
        return XR_FORM_FACTOR_HANDHELD_DISPLAY;
    }
    throw std::invalid_argument(Fmt("Unknown form factor '%s'", formFactorStr.c_str()));
}

inline XrViewConfigurationType GetXrViewConfigurationType(const std::string& viewConfigurationStr) {
    if (EqualsIgnoreCase(viewConfigurationStr, "Mono")) {
        return XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO;
    }
    if (EqualsIgnoreCase(viewConfigurationStr, "Stereo")) {
        return XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    }
    throw std::invalid_argument(Fmt("Unknown view configuration '%s'", viewConfigurationStr.c_str()));
}

inline XrEnvironmentBlendMode GetXrEnvironmentBlendMode(const std::string& environmentBlendModeStr) {
    if (EqualsIgnoreCase(environmentBlendModeStr, "Opaque")) {
        return XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    }
    if (EqualsIgnoreCase(environmentBlendModeStr, "Additive")) {
        return XR_ENVIRONMENT_BLEND_MODE_ADDITIVE;
    }
    if (EqualsIgnoreCase(environmentBlendModeStr, "AlphaBlend")) {
        return XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
    }
    throw std::invalid_argument(Fmt("Unknown environment blend mode '%s'", environmentBlendModeStr.c_str()));
}

inline const char* GetXrEnvironmentBlendModeStr(XrEnvironmentBlendMode environmentBlendMode) {
    switch (environmentBlendMode) {
        case XR_ENVIRONMENT_BLEND_MODE_OPAQUE:
            return "Opaque";
        case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE:
            return "Additive";
        case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND:
            return "AlphaBlend";
        default:
            throw std::invalid_argument(Fmt("Unknown environment blend mode '%s'", to_string(environmentBlendMode)));
    }
}

struct Options {
    std::string GraphicsPlugin;

    std::string FormFactor{"Hmd"};

    std::string ViewConfiguration{"Stereo"};

    std::string EnvironmentBlendMode{"Opaque"};

    std::string AppSpace{"Local"};

    struct {
        XrFormFactor FormFactor{XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY};

        XrViewConfigurationType ViewConfigType{XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};

        XrEnvironmentBlendMode EnvironmentBlendMode{XR_ENVIRONMENT_BLEND_MODE_OPAQUE};
    } Parsed;

    void ParseStrings() {
        Parsed.FormFactor = GetXrFormFactor(FormFactor);
        Parsed.ViewConfigType = GetXrViewConfigurationType(ViewConfiguration);
        Parsed.EnvironmentBlendMode = GetXrEnvironmentBlendMode(EnvironmentBlendMode);
    }

    std::array<float, 4> GetBackgroundClearColor() const {
        static const std::array<float, 4> SlateGrey{0.184313729f, 0.309803933f, 0.309803933f, 1.0f};
        static const std::array<float, 4> TransparentBlack{0.0f, 0.0f, 0.0f, 0.0f};
        static const std::array<float, 4> Black{0.0f, 0.0f, 0.0f, 1.0f};

        switch (Parsed.EnvironmentBlendMode) {
            case XR_ENVIRONMENT_BLEND_MODE_OPAQUE:
                return SlateGrey;
            case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE:
                return Black;
            case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND:
                return TransparentBlack;
            default:
                return SlateGrey;
        }
    }

    void SetEnvironmentBlendMode(XrEnvironmentBlendMode environmentBlendMode) {
        EnvironmentBlendMode = GetXrEnvironmentBlendModeStr(environmentBlendMode);
        Parsed.EnvironmentBlendMode = environmentBlendMode;
    }
};
