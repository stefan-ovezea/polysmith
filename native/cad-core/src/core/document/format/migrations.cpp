#include "core/document/format/migrations.h"

#include "core/document/format/format_version.h"

#include <stdexcept>
#include <string>

namespace polysmith::core {
namespace {

// v0 (raw DocumentState) -> v1 ({"format", "document"} envelope). The document
// schema is unchanged; this step only wraps it.
nlohmann::json migrate_0_to_1(const nlohmann::json& payload) {
  return {
      {"format", "polysmith/1"},
      {"document", payload},
  };
}

nlohmann::json apply_step(const nlohmann::json& payload, int from_version) {
  if (from_version == 0) {
    return migrate_0_to_1(payload);
  }
  throw std::runtime_error("Unknown document format migration from " +
                           std::to_string(from_version));
}

}  // namespace

int document_format_version(const nlohmann::json& payload) {
  if (!payload.is_object() || !payload.contains("format") ||
      !payload.at("format").is_string()) {
    return 0;
  }
  const std::string tag = payload.at("format").get<std::string>();
  const auto slash = tag.rfind('/');
  if (slash == std::string::npos) {
    return 0;
  }
  try {
    return std::stoi(tag.substr(slash + 1));
  } catch (const std::exception&) {
    return 0;
  }
}

nlohmann::json migrate_document_format(const nlohmann::json& payload,
                                       int from_version) {
  nlohmann::json current = payload;
  for (int v = from_version; v < kCurrentFormatVersion; ++v) {
    current = apply_step(current, v);
  }
  return current;
}

}  // namespace polysmith::core
