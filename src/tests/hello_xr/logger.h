// Copyright (c) 2017-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace Log {
enum class Level { Verbose, Info, Warning, Error };

void SetLevel(Level minSeverity);
void Write(Level severity, const std::string& msg);
}  // namespace Log
