#include "core/document/document.h"

#include "core/diagnostics/logger.h"
#include "core/geometry/iges_import_helpers.h"
#include "core/geometry/import_runtime.h"
#include "core/geometry/shape_snapshot.h"
#include "core/geometry/step_import_helpers.h"

#include <filesystem>
#include <sstream>
#include <string>

namespace polysmith::core {

DocumentState DocumentManager::import_step(const std::string& file_path) {
  require_document();

  if (file_path.empty()) {
    throw std::runtime_error("Import path cannot be empty");
  }

  // Parse BEFORE any document mutation so a bad file leaves the
  // document untouched (only the error event fires).
  const StepImportResult parsed = read_step_file(file_path);

  push_undo_state();
  clear_redo_stack();

  StepImportFeatureParameters params{};
  params.file_path = file_path;
  params.source_units = parsed.source_units;
  params.serialized_shape = serialize_shape_snapshot(parsed.shape);
  params.solid_count = parsed.solid_count;
  params.face_count = parsed.face_count;

  const std::string file_name =
      std::filesystem::path(file_path).filename().string();
  std::ostringstream summary;
  summary << file_name << " · " << parsed.face_count << " faces";
  const bool source_is_mm =
      parsed.source_units.find("MILLIMETRE") != std::string::npos ||
      parsed.source_units == "MM";
  if (!source_is_mm) {
    summary << " · " << parsed.source_units << " → mm";
  }

  const std::string feature_id =
      "feature-" + std::to_string(next_feature_id_++);
  store_imported_shape(feature_id, parsed.shape);
  FeatureEntry feature{
      .id = feature_id,
      .kind = "step_import",
      .name = "STEP Import",
      .status = "healthy",
      .parameters_summary = summary.str(),
      .step_import_parameters = params,
  };
  document_->feature_history.push_back(std::move(feature));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  bump_geometry_revision();

  log_info("step_import",
           "Imported '" + file_name + "': " +
               std::to_string(parsed.solid_count) + " solids, " +
               std::to_string(parsed.face_count) + " faces, units " +
               parsed.source_units);
  if (parsed.solid_count == 0) {
    log_warn("step_import",
             "Imported shape contains no solids — solid-only operations "
             "will be unavailable");
  }
  return document_.value();
}

DocumentState DocumentManager::import_iges(const std::string& file_path) {
  require_document();

  if (file_path.empty()) {
    throw std::runtime_error("Import path cannot be empty");
  }

  const IgesImportResult parsed = read_iges_file(file_path);

  push_undo_state();
  clear_redo_stack();

  IgesImportFeatureParameters params{};
  params.file_path = file_path;
  params.source_units = parsed.source_units;
  params.serialized_shape = serialize_shape_snapshot(parsed.shape);
  params.solid_count = parsed.solid_count;
  params.face_count = parsed.face_count;

  const std::string file_name =
      std::filesystem::path(file_path).filename().string();
  std::ostringstream summary;
  summary << file_name << " · " << parsed.face_count << " faces";
  const bool source_is_mm =
      parsed.source_units.find("MM") != std::string::npos ||
      parsed.source_units.find("MILLIMETRE") != std::string::npos;
  if (!source_is_mm) {
    summary << " · " << parsed.source_units << " → mm";
  }

  const std::string feature_id =
      "feature-" + std::to_string(next_feature_id_++);
  store_imported_shape(feature_id, parsed.shape);
  FeatureEntry feature{
      .id = feature_id,
      .kind = "iges_import",
      .name = "IGES Import",
      .status = "healthy",
      .parameters_summary = summary.str(),
      .iges_import_parameters = params,
  };
  document_->feature_history.push_back(std::move(feature));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  bump_geometry_revision();

  log_info("iges_import",
           "Imported '" + file_name + "': " +
               std::to_string(parsed.solid_count) + " solids, " +
               std::to_string(parsed.face_count) + " faces, units " +
               parsed.source_units);
  if (parsed.solid_count == 0) {
    log_warn("iges_import",
             "Imported shape contains no solids — solid-only operations "
             "will be unavailable");
  }
  return document_.value();
}

}  // namespace polysmith::core
