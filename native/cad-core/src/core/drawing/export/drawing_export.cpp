#include "core/drawing/export/drawing_export.h"

#include <stdexcept>

#include "core/document/document_state.h"

namespace polysmith::core {

ExportResult export_drawing_sheet(const DocumentState& document,
                                  const std::string& drawing_id,
                                  const std::string& sheet_id,
                                  const std::string& format,
                                  const std::string& file_path) {
  if (file_path.empty()) {
    throw std::runtime_error("Drawing export file path is empty");
  }
  // The sheet must exist on the drawing (flatten_sheet would return
  // nullopt for a dangling id — the mutators prevent it).
  const auto stream = flatten_sheet(document, drawing_id, sheet_id);
  if (!stream.has_value()) {
    throw std::runtime_error("Unknown drawing or sheet for export");
  }
  if (format == "svg") {
    return export_sheet_as_svg(stream.value(), file_path);
  }
  if (format == "dxf") {
    return export_sheet_as_dxf(stream.value(), file_path);
  }
  throw std::runtime_error("Unknown drawing export format: " + format);
}

}  // namespace polysmith::core
