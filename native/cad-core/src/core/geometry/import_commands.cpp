#include "core/document/document.h"

#include "core/diagnostics/logger.h"
#include "core/geometry/iges_import_helpers.h"
#include "core/geometry/import_runtime.h"
#include "core/geometry/shape_snapshot.h"
#include "core/geometry/step_import_helpers.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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

  const std::string file_name =
      std::filesystem::path(file_path).filename().string();
  const bool source_is_mm =
      parsed.source_units.find("MILLIMETRE") != std::string::npos ||
      parsed.source_units == "MM";

  // One body per solid. A STEP assembly is a set of parts, and keeping it as
  // one body makes it a single undifferentiated lump: it cannot be hidden,
  // renamed or coloured per part. The parts already carry the name and
  // display colour the file gives them, so the split costs nothing extra.
  std::vector<StepImportedSolid> solids = parsed.solids;
  if (solids.empty()) {
    // No solids at all (a surface model the sew could not close): keep the
    // whole shape as one body so the import still happens.
    StepImportedSolid whole;
    whole.shape = parsed.shape;
    whole.face_count = parsed.face_count;
    solids.push_back(std::move(whole));
  }

  // A board places the same footprint a dozen times, so the file's names
  // repeat; number the repeats so the list stays readable.
  std::map<std::string, int> name_uses;
  int index = 0;
  // Body created for each solid of the file, indexed like `solids` (a body's id
  // IS its feature id). The component tree below is built from this mapping.
  std::vector<std::string> body_id_by_solid(solids.size());
  // Serializing one body per solid is the bulk of the import time on a large
  // assembly, and it happens after the geometry is already transferred. Log
  // progress in quarters so the UI's busy overlay can say what is happening
  // instead of freezing on "Reading".
  // One imported file is ONE operation: every body it produces carries this id,
  // so the timeline folds them into a single row instead of 1,092 rows, and
  // delete_feature removes the whole import as one undo step (that command
  // already understands operation groups). The id is the first member's feature
  // id, which is the convention used by every other multi-body commit.
  const std::string import_operation_id =
      "feature-" + std::to_string(next_feature_id_);
  const std::size_t total_bodies = solids.size();
  const std::size_t report_every =
      std::max<std::size_t>(1, total_bodies / 4);
  for (const StepImportedSolid& solid : solids) {
    ++index;
    if (index > 1 && total_bodies > 8 &&
        static_cast<std::size_t>(index) % report_every == 0 &&
        static_cast<std::size_t>(index) < total_bodies) {
      log_info("step_import",
               "Writing part " + std::to_string(index) + "/" +
                   std::to_string(total_bodies));
    }
    std::string name = solid.name;
    if (name.empty()) {
      name = "Solid " + std::to_string(index);
    }
    const int use = ++name_uses[name];
    if (use > 1) {
      name += " #" + std::to_string(use);
    }

    StepImportFeatureParameters params{};
    params.file_path = file_path;
    params.source_units = parsed.source_units;
    // Keep the file's own presentation colours with the snapshot: the B-rep
    // is geometry only, so without this a reloaded board falls back to one
    // flat body colour.
    params.serialized_shape = serialize_shape_snapshot(solid.shape);
    params.solid_count = 1;
    params.face_count = solid.face_count;
    params.face_colors = solid.face_colors;

    std::ostringstream summary;
    summary << file_name << " · " << solid.face_count << " faces";
    if (!source_is_mm) {
      summary << " · " << parsed.source_units << " → mm";
    }

    const std::string feature_id =
        "feature-" + std::to_string(next_feature_id_++);
    store_imported_shape(feature_id, solid.shape);
    body_id_by_solid[static_cast<std::size_t>(index - 1)] = feature_id;
    FeatureEntry feature{
        .id = feature_id,
        .kind = "step_import",
        .name = std::move(name),
        .status = "healthy",
        .parameters_summary = summary.str(),
        .operation_id = import_operation_id,
        .step_import_parameters = std::move(params),
    };
    document_->feature_history.push_back(std::move(feature));
  }

  // The file's product structure becomes the Browser's component tree. Each node
  // is one occurrence ("U1:1"), its members are the bodies created above for the
  // solids it places. The reader emits nodes in pre-order and prunes empty ones,
  // so a node's parent id is always already known.
  std::vector<std::string> component_ids(parsed.assembly.size());
  for (std::size_t i = 0; i < parsed.assembly.size(); ++i) {
    const StepAssemblyNode& node = parsed.assembly[i];
    ComponentNode component;
    component.id = "component-" + std::to_string(next_component_id_++);
    component.name = node.name;
    if (node.parent >= 0 && static_cast<std::size_t>(node.parent) < i) {
      component.parent_id = component_ids[static_cast<std::size_t>(node.parent)];
    }
    for (const int solid : node.solid_indices) {
      if (solid < 0 || static_cast<std::size_t>(solid) >= body_id_by_solid.size()) {
        continue;
      }
      const std::string& body_id =
          body_id_by_solid[static_cast<std::size_t>(solid)];
      if (!body_id.empty()) {
        component.body_ids.push_back(body_id);
      }
    }
    component_ids[i] = component.id;
    document_->components.push_back(std::move(component));
  }

  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  bump_geometry_revision();

  log_info("step_import",
           "Imported '" + file_name + "': " +
               std::to_string(parsed.solid_count) + " solids as " +
               std::to_string(index) + " bodies, " +
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
