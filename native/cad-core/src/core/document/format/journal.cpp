#include "core/document/format/journal.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace polysmith::core {
namespace {

std::filesystem::path journal_path(const std::string& doc_id) {
  return std::filesystem::path(journal_directory()) / (doc_id + ".jsonl");
}

}  // namespace

std::string journal_directory() {
  const char* env = std::getenv("POLYSMITH_JOURNAL_DIR");
  if (env != nullptr && *env != '\0') {
    return env;
  }
  return (std::filesystem::temp_directory_path() / "polysmith-journal")
      .string();
}

void journal_append(const std::string& doc_id, const std::string& command_json) {
  std::error_code ec;
  std::filesystem::create_directories(journal_directory(), ec);
  std::ofstream stream(journal_path(doc_id), std::ios::app);
  if (stream.is_open()) {
    stream << command_json << "\n";
  }
}

void journal_truncate(const std::string& doc_id) {
  std::error_code ec;
  std::filesystem::remove(journal_path(doc_id), ec);
}

std::vector<std::string> journal_lines(const std::string& doc_id) {
  std::vector<std::string> lines;
  std::ifstream stream(journal_path(doc_id));
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  return lines;
}

}  // namespace polysmith::core
