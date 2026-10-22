#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace polysmith::core {

// Returns the format version encoded in a saved document, or 0 when the payload
// has no "format" field (the pre-versioned raw DocumentState).
int document_format_version(const nlohmann::json& payload);

// Migrates `payload` from `from_version` to `kCurrentFormatVersion` by applying
// the pending steps in order. Throws std::runtime_error on a version this build
// does not know how to migrate.
nlohmann::json migrate_document_format(const nlohmann::json& payload,
                                       int from_version);

}  // namespace polysmith::core
