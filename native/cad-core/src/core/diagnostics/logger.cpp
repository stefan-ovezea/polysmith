#include "core/logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "protocol/ipc.h"

namespace polysmith::core {
namespace {

std::string current_timestamp() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t time = std::chrono::system_clock::to_time_t(now);

  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &time);
#else
  gmtime_r(&time, &tm);
#endif

  std::ostringstream stream;
  stream << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return stream.str();
}

}  // namespace

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

void log(LogLevel level, const std::string& source, const std::string& message) {
  const std::string timestamp = current_timestamp();
  polysmith::protocol::write_log("[" + timestamp + "] [" + to_string(level) +
                                 "] [" + source + "] " + message);
  polysmith::protocol::write_message(polysmith::protocol::make_log_event(
      to_string(level), source, message, timestamp));
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
