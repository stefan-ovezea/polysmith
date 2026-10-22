#include "core/document/document.h"

#include "core/geometry/feature_shape.h"
#include "core/geometry/mesh_import_helpers.h"
#include "core/geometry/shape_snapshot.h"

#include <filesystem>
#include <sstream>
#include <string>

namespace polysmith::core {

DocumentState DocumentManager::import_stl(const std::string& file_path,
                                          double scale) {
  require_document();

  if (file_path.empty()) {
    throw std::runtime_error("Import path cannot be empty");
  }
  if (scale <= 0.0) {
    throw std::runtime_error("Import scale must be greater than zero");
  }
  if (!std::filesystem::exists(file_path)) {
    throw std::runtime_error("STL file not found: " + file_path);
  }

  push_undo_state();
  clear_redo_stack();

  MeshImportFeatureParameters params{};
  params.file_path = file_path;
  params.scale = scale;

  // The document stores only the path — the mesh is re-read from disk
  // on every compile, so the .polysmith file never embeds triangle data.
  const std::string file_name = std::filesystem::path(file_path).filename().string();
  std::ostringstream summary;
  summary << file_name;
  if (std::abs(scale - 1.0) > 1e-12) {
    summary << " · scale " << scale;
  }

  FeatureEntry feature{
      .id = "feature-" + std::to_string(next_feature_id_++),
      .kind = "mesh_import",
      .name = "Mesh Import",
      .status = "healthy",
      .parameters_summary = summary.str(),
      .mesh_import_parameters = params,
  };
  document_->feature_history.push_back(std::move(feature));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::convert_mesh_to_body(
    const std::string& source_body_id) {
  require_document();

  if (source_body_id.empty()) {
    throw std::runtime_error("Body id cannot be empty");
  }

  const auto source_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == source_body_id; });
  if (source_it == document_->feature_history.end()) {
    throw std::runtime_error("Body not found: " + source_body_id);
  }
  if (source_it->kind != "mesh_import" ||
      !source_it->mesh_import_parameters.has_value()) {
    throw std::runtime_error(
        "convert_mesh_to_body requires a mesh body: " + source_body_id);
  }

  // Validate the conversion up front so the user gets a clear error
  // instead of a silently missing body.
  const TopoDS_Shape mesh_shape = build_feature_shape(*source_it);
  if (mesh_shape.IsNull()) {
    throw std::runtime_error(
        "Mesh source file is missing or unreadable: " +
        source_it->mesh_import_parameters->file_path);
  }
  const TopoDS_Shape converted = convert_mesh_to_solid(mesh_shape);
  if (converted.IsNull()) {
    throw std::runtime_error(
        "Mesh is not watertight — cannot convert to a solid body");
  }

  push_undo_state();
  clear_redo_stack();

  MeshToBodyFeatureParameters params{};
  params.source_body_id = source_body_id;
  // Snapshot the converted solid at creation (same pattern as a
  // standalone body copy): the result is independent of the mesh body,
  // which the user can delete afterwards without losing the solid.
  params.serialized_shape = serialize_shape_snapshot(converted);

  FeatureEntry feature{
      .id = "feature-" + std::to_string(next_feature_id_++),
      .kind = "mesh_to_body",
      .name = "Convert Mesh",
      .status = "healthy",
      .parameters_summary = "From " + source_it->name,
      .mesh_to_body_parameters = params,
  };
  document_->feature_history.push_back(std::move(feature));
  document_->selected_feature_id = document_->feature_history.back().id;
  bump_geometry_revision();
  return document_.value();
}

}  // namespace polysmith::core
