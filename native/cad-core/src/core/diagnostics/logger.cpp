#include "core/diagnostics/logger.h"

#include <chrono>
#include <ctime>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#include "protocol/ipc.h"

namespace polysmith::core {
namespace {

std::string current_timestamp();

std::string crash_log_path() {
  const char* base = std::getenv("APPDATA");
  if (base == nullptr || *base == '\0') {
    return "cad_core_crash.log";
  }
  return std::string(base) + "/polysmith/cad_core_crash.log";
}

}  // namespace
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

void install_crash_logger() {
  // The handler must not allocate if it can avoid it, but a terminate path is
  // already lost - writing a file is worth more than purity. One line, appended,
  // so repeated failures accumulate instead of overwriting the first one.
  std::set_terminate([]() {
    std::ofstream out(crash_log_path(), std::ios::app);
    if (out) {
      out << "terminate at " << current_timestamp() << ": ";
      if (const std::exception_ptr error = std::current_exception()) {
        try {
          std::rethrow_exception(error);
        } catch (const std::exception& exception) {
          out << exception.what();
        } catch (...) {
          out << "<non-std exception>";
        }
      } else {
        out << "<no active exception: std::terminate called directly>";
      }
      out << "\n";
      out.flush();
    }
    std::abort();
  });
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
  const std::string line = "[" + timestamp + "] [" + to_string(level) +
                           "] [" + source + "] " + message;
  polysmith::protocol::write_log(line);
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

void install_crash_logger() {
  // A thrown std::exception is caught by the command loop and answered with an
  // error event, so a process that DISAPPEARS is either an access violation or
  // std::terminate. Neither leaves a trace in the in-app log, which dies with the
  // process. Append the reason to %APPDATA%/polysmith/cad_core_crash.log so one
  // reproduction names the cause instead of "it crashed".
  std::set_terminate([]() {
    std::ofstream out(crash_log_path(), std::ios::app);
    if (out) {
      out << "terminate at " << current_timestamp() << ": ";
      if (const std::exception_ptr error = std::current_exception()) {
        try {
          std::rethrow_exception(error);
        } catch (const std::exception& exception) {
          out << exception.what();
        } catch (...) {
          out << "<non-std exception>";
        }
      } else {
        out << "<no active exception: std::terminate called directly>";
      }
      out << "\n";
      out.flush();
    }
    std::abort();
  });
}

}  // namespace polysmith::core
