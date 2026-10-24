#pragma once

#include <string>

namespace polysmith::core {
// Writes the reason for an abnormal exit (terminate / unexpected exception) to
// %APPDATA%/polysmith/cad_core_crash.log. Safe to call once at startup.
void install_crash_logger();

enum class LogLevel {
  Debug,
  Info,
  Warn,
  Error,
};

std::string to_string(LogLevel level);
void log(LogLevel level, const std::string& source, const std::string& message);
void log_debug(const std::string& source, const std::string& message);
void log_info(const std::string& source, const std::string& message);
void log_warn(const std::string& source, const std::string& message);
void log_error(const std::string& source, const std::string& message);

}  // namespace polysmith::core
