#pragma once

#include <string>

namespace polysmith::core {

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
