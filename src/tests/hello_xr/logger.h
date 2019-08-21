#pragma once

namespace Log {
enum class Level { Verbose, Info, Warning, Error };

void SetLevel(Level minSeverity);
void Write(Level severity, const std::string& msg);
}  // namespace Log
