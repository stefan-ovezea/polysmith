// Minimal logger shim for cad_core_sketch_profile_test.
//
// That executable is built without the protocol/OCCT dependency chain,
// while the real logger (core/diagnostics/logger.cpp) also emits a
// structured IPC event through protocol.  Tests only need the stderr
// line — this shim keeps the lightweight target linkable while sketch
// code (e.g. the PS_TRACE_FACES face-walk trace) logs through the
// sanctioned logger API.

#include <cstdio>
#include <string>

#include "core/diagnostics/logger.h"

namespace polysmith::core {

std::string to_string(LogLevel level) {
  switch (level) {
    case LogLevel::Debug:
      return "debug";
    case LogLevel::Info:
      return "info";
    case LogLevel::Warn:
      return "warn";
    case LogLevel::Error:
      return "error";
  }
  return "info";
}

void log(LogLevel level, const std::string& source,
         const std::string& message) {
  std::fprintf(stderr, "[%s] [%s] %s\n", to_string(level).c_str(),
               source.c_str(), message.c_str());
}

void log_debug(const std::string& source, const std::string& message) {
  log(LogLevel::Debug, source, message);
}

void log_info(const std::string& source, const std::string& message) {
  log(LogLevel::Info, source, message);
}

void log_warn(const std::string& source, const std::string& message) {
  log(LogLevel::Warn, source, message);
}

void log_error(const std::string& source, const std::string& message) {
  log(LogLevel::Error, source, message);
}

}  // namespace polysmith::core
