#pragma once

#include <string>

namespace polysmith::core {

struct DocumentState;

struct ExportResult {
  std::string file_path;
  std::string format;
  int exported_feature_count;
};

ExportResult export_document_as_step(const DocumentState& document,
                                     const std::string& file_path);

}  // namespace polysmith::core
