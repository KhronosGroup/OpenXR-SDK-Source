// Copyright (c) 2017-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include "openxr_program.h"

struct IPlatformPlugin;
struct IGraphicsPlugin;
struct IOpenXrProgram;

std::shared_ptr<IOpenXrProgram> CreateOpenXrProgram_SpatialContainers(const std::shared_ptr<IPlatformPlugin>& platformPlugin,
                                                                      const std::shared_ptr<IGraphicsPlugin>& graphicsPlugin);
