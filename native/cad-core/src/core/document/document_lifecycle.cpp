#include "core/document/document.h"
#include "core/document/document_internal.h"
#include "core/document/format/atomic_save.h"
#include "core/document/format/format_version.h"
#include "core/document/format/journal.h"
#include "core/document/format/migrations.h"
#include "core/cam/cam_export.h"
#include "core/cam/cam_profile_reference.h"
#include "core/cam/cam_runtime.h"
#include "core/cam/cam_refresh.h"
#include "core/cam/cam_types.h"
#include "core/cam/tool_identity.h"
#include "core/diagnostics/logger.h"
#include "core/drawing/drawing_annotation_geometry.h"
#include "core/drawing/drawing_projection.h"
#include "core/drawing/drawing_resolution.h"
#include "core/drawing/drawing_runtime.h"
#include "core/drawing/drawing_refresh.h"
#include "core/drawing/drawing_sheet.h"
#include "core/drawing/drawing_types.h"
#include "core/drawing/export/drawing_export.h"
#include "core/export/export.h"
#include "core/geometry/refresh_dependents.h"
#include "core/geometry/import_runtime.h"
#include "core/plugin/plugin_feature.h"
#include "core/primitive/box_feature.h"
#include "core/primitive/cylinder_feature.h"
#include "core/sketch/formula_eval.h"
#include "core/sketch/sketch_feature.h"
#include "core/sketch/sketch_profile.h"
#include "protocol/serialization.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <source_location>
#include <sstream>
#include <unordered_set>

namespace polysmith::core {

DocumentState DocumentManager::create_document() {
  // A fresh document replaces the previous one — drop its runtime
  // toolpath cache (keyed by document id; it would otherwise leak).
  if (document_.has_value()) {
    cam_runtime::clear(document_->id);
    drawing_runtime::clear(document_->id);
  }
  clear_imported_shapes();

  DocumentState document{
      .id = "doc-" + std::to_string(next_document_id_++),
      .name = "Untitled Part",
      .units = "mm",
      .revision = 1,
      .selected_feature_id = std::nullopt,
      .selected_reference_id = std::nullopt,
      .selected_face_id = std::nullopt,
      .selected_edge_ids = {},
      .selected_vertex_ids = {},
      .active_sketch_plane_id = std::nullopt,
      .active_sketch_face_id = std::nullopt,
      .active_sketch_feature_id = std::nullopt,
      .active_sketch_tool = std::nullopt,
      .selected_sketch_vertex_id = std::nullopt,
      .selected_sketch_entity_id = std::nullopt,
      .selected_sketch_dimension_id = std::nullopt,
      .selected_sketch_profile_id = std::nullopt,
      .selected_sketch_profile_ids = {},
      .timeline_cursor = std::nullopt,
      .feature_history = {make_root_feature()},
      .parameters = {},
      .appearance = {},
      .selection_filter = SelectionFilter{},
  };

  document_ = document;
  document_count_ = 1;
  undo_stack_.clear();
  redo_stack_.clear();
  return document;
}

FeatureEntry DocumentManager::make_root_feature() {
  return FeatureEntry{
      .id = "feature-" + std::to_string(next_feature_id_++),
      .kind = "root_part",
      .name = "Base Part",
      .status = "healthy",
      .parameters_summary = "Document root",
      .box_parameters = std::nullopt,
      .cylinder_parameters = std::nullopt,
      .extrude_parameters = std::nullopt,
      .sketch_parameters = std::nullopt,
  };
}

void DocumentManager::require_document() const {
  if (!document_.has_value()) {
    throw std::runtime_error("No active document");
  }
}

void DocumentManager::push_undo_state(const std::source_location& location) {
  require_document();
  if (!undo_group_stack_.empty()) {
    undo_group_stack_.back().changed = true;
  }
  undo_stack_.push_back(document_.value());
  undo_names_.push_back(undo_step_name_from_location(location));
  while (undo_stack_.size() > static_cast<size_t>(max_undo_steps_)) {
    undo_stack_.erase(undo_stack_.begin());
    undo_names_.erase(undo_names_.begin());
  }
  sync_undo_step_names();
}

void DocumentManager::sync_sketch_id_counters() {
  if (!document_.has_value()) {
    return;
  }
  const auto next_index = [](const std::string& id) {
    const size_t dash = id.rfind('-');
    if (dash == std::string::npos || dash + 1 >= id.size()) {
      return 0;
    }
    const std::string suffix = id.substr(dash + 1);
    if (suffix.find_first_not_of("0123456789") != std::string::npos) {
      return 0;
    }
    return std::stoi(suffix) + 1;
  };
  const auto bump = [](int& counter, int candidate) {
    counter = std::max(counter, candidate);
  };

  for (const auto& feature : document_->feature_history) {
    if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
      continue;
    }
    const auto& sketch = feature.sketch_parameters.value();
    for (const auto& e : sketch.lines) bump(next_sketch_line_id_, next_index(e.id));
    for (const auto& e : sketch.circles) bump(next_sketch_circle_id_, next_index(e.id));
    for (const auto& e : sketch.polygons) bump(next_sketch_polygon_id_, next_index(e.id));
    for (const auto& e : sketch.arcs) bump(next_sketch_arc_id_, next_index(e.id));
    for (const auto& e : sketch.ellipses) bump(next_sketch_ellipse_id_, next_index(e.id));
    for (const auto& e : sketch.splines) bump(next_sketch_spline_id_, next_index(e.id));
    for (const auto& e : sketch.slots) bump(next_sketch_slot_id_, next_index(e.id));
    for (const auto& e : sketch.fillets) bump(next_sketch_fillet_id_, next_index(e.id));
    for (const auto& e : sketch.chamfers) bump(next_sketch_chamfer_id_, next_index(e.id));
    for (const auto& e : sketch.projected_points) {
      bump(next_sketch_projected_point_id_, next_index(e.id));
    }
    for (const auto& e : sketch.projections) {
      bump(next_sketch_projection_id_, next_index(e.id));
    }
    for (const auto& e : sketch.texts) bump(next_sketch_text_id_, next_index(e.id));
  }
}

void DocumentManager::bump_geometry_revision(bool shape_changed) {
  require_document();
  sync_sketch_id_counters();
  refresh_history_dependencies(*document_);
  refresh_cam_dependencies(*document_, document_->revision + 1);
  refresh_drawing_dependencies(*document_, document_->revision + 1);
  normalize_timeline_cursor(*document_);
  prune_document_selection();
  const int next = document_->revision + 1;
  document_->revision = next;
  if (shape_changed) {
    document_->geometry_revision = document_->geometry_revision + 1;
  }
  document_->can_undo = !undo_stack_.empty();
  document_->can_redo = !redo_stack_.empty();
  sync_undo_step_names();
}

void DocumentManager::sync_undo_step_names() {
  if (!document_.has_value()) {
    return;
  }
  document_->undo_step_names.assign(undo_names_.rbegin(), undo_names_.rend());
  document_->redo_step_names.assign(redo_names_.rbegin(), redo_names_.rend());
}

void DocumentManager::prune_document_selection() {
  std::unordered_set<std::string> live;
  for (const auto& feature : document_->feature_history) {
    if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
      continue;
    }
    const auto& sketch = feature.sketch_parameters.value();
    for (const auto& line : sketch.lines) {
      live.insert(line.id);
      live.insert(line.start_vertex_id);
      live.insert(line.end_vertex_id);
    }
    for (const auto& circle : sketch.circles) {
      live.insert(circle.id);
      live.insert(circle.center_vertex_id);
    }
    for (const auto& arc : sketch.arcs) {
      live.insert(arc.id);
      live.insert(arc.start_vertex_id);
      live.insert(arc.end_vertex_id);
      live.insert(arc.center_vertex_id);
    }
    for (const auto& polygon : sketch.polygons) {
      live.insert(polygon.id);
    }
    for (const auto& vertex : sketch.vertices) {
      live.insert(vertex.id);
      if (!vertex.vertex_id.empty()) {
        live.insert(vertex.vertex_id);
      }
    }
    for (const auto& dimension : sketch.dimensions) {
      live.insert(dimension.id);
    }
    for (const auto& fillet : sketch.fillets) {
      live.insert(fillet.id);
    }
    for (const auto& text : sketch.texts) {
      live.insert(text.id);
    }
    for (const auto& profile : sketch.profiles) {
      live.insert(profile.id);
    }
    for (const auto& projected : sketch.projected_points) {
      live.insert(projected.id);
    }
    for (const auto& projection : sketch.projections) {
      live.insert(projection.id);
    }
  }

  const auto is_live = [&](const std::string& id) {
    return live.count(id) != 0;
  };
  const auto erase_dead = [&](std::vector<std::string>& ids) {
    ids.erase(
        std::remove_if(ids.begin(), ids.end(),
                       [&](const std::string& id) { return !is_live(id); }),
        ids.end());
  };

  if (document_->selected_sketch_vertex_id.has_value() &&
      !is_live(document_->selected_sketch_vertex_id.value())) {
    document_->selected_sketch_vertex_id.reset();
  }
  if (document_->selected_sketch_entity_id.has_value() &&
      !is_live(document_->selected_sketch_entity_id.value())) {
    document_->selected_sketch_entity_id.reset();
  }
  if (document_->selected_sketch_dimension_id.has_value() &&
      !is_live(document_->selected_sketch_dimension_id.value())) {
    document_->selected_sketch_dimension_id.reset();
  }
  if (document_->selected_sketch_text_id.has_value() &&
      !is_live(document_->selected_sketch_text_id.value())) {
    document_->selected_sketch_text_id.reset();
  }
  if (document_->selected_sketch_profile_id.has_value() &&
      !is_live(document_->selected_sketch_profile_id.value())) {
    document_->selected_sketch_profile_id.reset();
  }
  erase_dead(document_->selected_sketch_entity_ids);
  erase_dead(document_->selected_sketch_vertex_ids);
  erase_dead(document_->selected_sketch_profile_ids);

  if (!document_->selected_sketch_entity_id.has_value() &&
      !document_->selected_sketch_entity_ids.empty()) {
    document_->selected_sketch_entity_id =
        document_->selected_sketch_entity_ids.back();
  }
  if (!document_->selected_sketch_vertex_id.has_value() &&
      !document_->selected_sketch_vertex_ids.empty()) {
    document_->selected_sketch_vertex_id =
        document_->selected_sketch_vertex_ids.back();
  }
  if (!document_->selected_sketch_profile_id.has_value() &&
      !document_->selected_sketch_profile_ids.empty()) {
    document_->selected_sketch_profile_id =
        document_->selected_sketch_profile_ids.back();
  }
}

void DocumentManager::clear_redo_stack() {
  redo_stack_.clear();
  redo_names_.clear();
  sync_undo_step_names();
}

DocumentState DocumentManager::undo() {
  require_document();
  if (undo_stack_.empty()) {
    throw std::runtime_error("Nothing to undo");
  }
  if (!undo_group_stack_.empty() &&
      undo_stack_.size() <= undo_group_stack_.back().undo_start_index) {
    return document_.value();
  }
  const int pre_restore_revision = document_->revision;
  cam_runtime::invalidate(document_->id);
  drawing_runtime::invalidate(document_->id);
  redo_stack_.push_back(document_.value());
  redo_names_.push_back(undo_names_.back());
  document_ = undo_stack_.back();
  undo_stack_.pop_back();
  undo_names_.pop_back();
  document_->revision = std::max(document_->revision, pre_restore_revision);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::redo() {
  require_document();
  if (redo_stack_.empty()) {
    throw std::runtime_error("Nothing to redo");
  }
  if (!undo_group_stack_.empty() &&
      redo_stack_.size() <= undo_group_stack_.back().redo_start_index) {
    return document_.value();
  }
  const int pre_restore_revision = document_->revision;
  cam_runtime::invalidate(document_->id);
  drawing_runtime::invalidate(document_->id);
  undo_stack_.push_back(document_.value());
  undo_names_.push_back(redo_names_.back());
  document_ = redo_stack_.back();
  redo_stack_.pop_back();
  redo_names_.pop_back();
  document_->revision = std::max(document_->revision, pre_restore_revision);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::undo_many(int count) {
  require_document();
  const int pre_restore_revision = document_->revision;
  int steps = count;
  if (steps < 0) {
    steps = -steps;
    if (!undo_group_stack_.empty()) {
      const size_t local =
          redo_stack_.size() - undo_group_stack_.back().redo_start_index;
      steps = std::min(steps, static_cast<int>(local));
    }
    steps = std::min(steps, static_cast<int>(redo_stack_.size()));
    if (steps > 0) {
      cam_runtime::invalidate(document_->id);
      drawing_runtime::invalidate(document_->id);
    }
    for (int i = 0; i < steps; ++i) {
      undo_stack_.push_back(document_.value());
      undo_names_.push_back(redo_names_.back());
      document_ = redo_stack_.back();
      redo_stack_.pop_back();
      redo_names_.pop_back();
    }
  } else {
    if (!undo_group_stack_.empty()) {
      const size_t local =
          undo_stack_.size() - undo_group_stack_.back().undo_start_index;
      steps = std::min(steps, static_cast<int>(local));
    }
    steps = std::min(steps, static_cast<int>(undo_stack_.size()));
    if (steps > 0) {
      cam_runtime::invalidate(document_->id);
      drawing_runtime::invalidate(document_->id);
    }
    for (int i = 0; i < steps; ++i) {
      redo_stack_.push_back(document_.value());
      redo_names_.push_back(undo_names_.back());
      document_ = undo_stack_.back();
      undo_stack_.pop_back();
      undo_names_.pop_back();
    }
  }
  document_->revision = std::max(document_->revision, pre_restore_revision);
  bump_geometry_revision();
  return document_.value();
}

void DocumentManager::set_undo_limit(int limit) {
  max_undo_steps_ = std::max(1, limit);
  while (undo_stack_.size() > static_cast<size_t>(max_undo_steps_)) {
    undo_stack_.erase(undo_stack_.begin());
    undo_names_.erase(undo_names_.begin());
  }
  sync_undo_step_names();
}

int DocumentManager::undo_limit() const {
  return max_undo_steps_;
}

void DocumentManager::begin_undo_group(const std::string& name) {
  require_document();
  undo_group_stack_.push_back(UndoGroupFrame{
      .name = name,
      .start_snapshot = document_.value(),
      .undo_start_index = undo_stack_.size(),
      .redo_start_index = redo_stack_.size(),
      .changed = false,
  });
}

void DocumentManager::mark_undo_group_changed() {
  if (!undo_group_stack_.empty()) {
    undo_group_stack_.back().changed = true;
  }
}

void DocumentManager::end_undo_group() {
  require_document();
  if (undo_group_stack_.empty()) {
    return;
  }
  const UndoGroupFrame frame = undo_group_stack_.back();
  undo_group_stack_.pop_back();
  const bool changed = frame.changed;
  undo_stack_.resize(frame.undo_start_index);
  undo_names_.resize(frame.undo_start_index);
  if (changed) {
    undo_stack_.push_back(frame.start_snapshot);
    undo_names_.push_back(frame.name);
    while (undo_stack_.size() > static_cast<size_t>(max_undo_steps_)) {
      undo_stack_.erase(undo_stack_.begin());
      undo_names_.erase(undo_names_.begin());
    }
    if (!undo_group_stack_.empty()) {
      undo_group_stack_.back().changed = true;
    }
  }
  clear_redo_stack();
}

DocumentState DocumentManager::abort_undo_group() {
  require_document();
  if (undo_group_stack_.empty()) {
    throw std::runtime_error("No active undo group");
  }
  const UndoGroupFrame frame = undo_group_stack_.back();
  undo_group_stack_.pop_back();
  const int pre_restore_revision = document_->revision;
  document_ = frame.start_snapshot;
  undo_stack_.resize(frame.undo_start_index);
  undo_names_.resize(frame.undo_start_index);
  redo_stack_.resize(frame.redo_start_index);
  redo_names_.resize(frame.redo_start_index);
  document_->revision = std::max(document_->revision, pre_restore_revision);
  cam_runtime::invalidate(document_->id);
  drawing_runtime::invalidate(document_->id);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::abort_all_undo_groups() {
  require_document();
  if (undo_group_stack_.empty()) {
    throw std::runtime_error("No active undo group");
  }
  const UndoGroupFrame frame = undo_group_stack_.front();
  undo_group_stack_.clear();
  const int pre_restore_revision = document_->revision;
  document_ = frame.start_snapshot;
  undo_stack_.resize(frame.undo_start_index);
  undo_names_.resize(frame.undo_start_index);
  redo_stack_.resize(frame.redo_start_index);
  redo_names_.resize(frame.redo_start_index);
  document_->revision = std::max(document_->revision, pre_restore_revision);
  cam_runtime::invalidate(document_->id);
  drawing_runtime::invalidate(document_->id);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_timeline_cursor(int included_action_count) {
  require_document();
  const int max_actions = action_count(document_.value());
  const int clamped =
      std::max(0, std::min(included_action_count, max_actions));
  if (clamped >= max_actions) {
    document_->timeline_cursor = std::nullopt;
  } else {
    document_->timeline_cursor = clamped;
  }
  if (document_->selected_feature_id.has_value() &&
      !includes_feature_at_cursor(document_.value(),
                                  document_->selected_feature_id.value())) {
    document_->selected_feature_id = std::nullopt;
  }
  if (document_->selected_reference_id.has_value() &&
      !is_origin_plane_reference(document_->selected_reference_id.value()) &&
      !includes_feature_at_cursor(document_.value(),
                                  document_->selected_reference_id.value())) {
    document_->selected_reference_id = std::nullopt;
  }
  if (document_->active_sketch_feature_id.has_value() &&
      !includes_feature_at_cursor(document_.value(),
                                  document_->active_sketch_feature_id.value())) {
    document_->active_sketch_feature_id = std::nullopt;
    document_->active_sketch_plane_id = std::nullopt;
    document_->active_sketch_face_id = std::nullopt;
    document_->active_sketch_tool = std::nullopt;
  }
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  document_->selected_sketch_profile_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_box_feature(
    const BoxFeatureParameters& parameters) {
  require_document();
  push_undo_state();
  clear_redo_stack();
  document_->feature_history.push_back(
      create_box_feature(next_feature_id_++, parameters));
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_cylinder_feature(
    const CylinderFeatureParameters& parameters) {
  require_document();
  push_undo_state();
  clear_redo_stack();
  document_->feature_history.push_back(
      create_cylinder_feature(next_feature_id_++, parameters));
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_box_feature(
    const std::string& feature_id, const BoxFeatureParameters& parameters) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  push_undo_state();
  clear_redo_stack();
  polysmith::core::update_box_feature(*feature_it, parameters);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_cylinder_feature(
    const std::string& feature_id,
    const CylinderFeatureParameters& parameters) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  push_undo_state();
  clear_redo_stack();
  polysmith::core::update_cylinder_feature(*feature_it, parameters);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::create_plugin_feature(
    const PluginFeatureParameters& parameters) {
  require_document();
  push_undo_state();
  clear_redo_stack();
  document_->feature_history.push_back(
      polysmith::core::create_plugin_feature(next_feature_id_++, parameters));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_plugin_feature(
    const std::string& feature_id,
    const PluginFeatureParameters& parameters) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  push_undo_state();
  clear_redo_stack();
  polysmith::core::update_plugin_feature(*feature_it, parameters);
  document_->selected_feature_id = feature_id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::confirm_plugin_feature(
    const std::string& feature_id) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  push_undo_state();
  clear_redo_stack();
  polysmith::core::confirm_plugin_feature(*feature_it);
  document_->selected_feature_id = feature_id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::rename_feature(const std::string& feature_id,
                                              const std::string& name) {
  require_document();
  if (name.empty()) {
    throw std::runtime_error("Feature name cannot be empty");
  }
  const auto feature_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->name == name) {
    return document_.value();
  }
  push_undo_state();
  clear_redo_stack();
  feature_it->name = name;
  bump_geometry_revision(/*shape_changed=*/false);
  return document_.value();
}

DocumentState DocumentManager::set_feature_suppressed(
    const std::string& feature_id, bool suppressed) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind == "root_part") {
    throw std::runtime_error("The root feature cannot be suppressed");
  }
  if (feature_it->suppressed == suppressed) {
    return document_.value();
  }
  push_undo_state();
  clear_redo_stack();
  feature_it->suppressed = suppressed;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::delete_feature(const std::string& feature_id) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind == "root_part") {
    throw std::runtime_error("The root feature cannot be deleted");
  }
  if (feature_it->kind == "sketch" &&
      document_->active_sketch_feature_id.has_value() &&
      document_->active_sketch_feature_id.value() == feature_id) {
    throw std::runtime_error("Finish the active sketch before deleting it");
  }
  push_undo_state();
  clear_redo_stack();
  const bool deleted_selected =
      document_->selected_feature_id.has_value() &&
      document_->selected_feature_id.value() == feature_id;
  document_->feature_history.erase(feature_it);
  if (deleted_selected) {
    document_->selected_feature_id = std::nullopt;
  }
  const auto owner_of = [](const std::string& id, const char* kind) {
    const std::string needle = std::string(":") + kind + ":";
    const auto separator = id.find(needle);
    return separator == std::string::npos ? std::string()
                                          : id.substr(0, separator);
  };
  if (document_->selected_face_id.has_value()) {
    const std::string owner =
        owner_of(document_->selected_face_id.value(), "face");
    if (owner == feature_id) {
      document_->selected_face_id = std::nullopt;
    }
  }
  document_->selected_edge_ids.erase(
      std::remove_if(document_->selected_edge_ids.begin(),
                     document_->selected_edge_ids.end(),
                     [&](const std::string& id) {
                       return owner_of(id, "edge") == feature_id;
                     }),
      document_->selected_edge_ids.end());
  document_->selected_vertex_ids.erase(
      std::remove_if(document_->selected_vertex_ids.begin(),
                     document_->selected_vertex_ids.end(),
                     [&](const std::string& id) {
                       return owner_of(id, "vertex") == feature_id;
                     }),
      document_->selected_vertex_ids.end());
  for (auto& remaining : document_->feature_history) {
    if (remaining.kind != "construction_plane" ||
        !remaining.construction_plane_parameters.has_value()) {
      continue;
    }
    auto& params = remaining.construction_plane_parameters.value();
    const auto references_deleted = [&](const std::string& source_id) {
      return source_id == feature_id ||
             source_id.rfind(feature_id + ":", 0) == 0;
    };
    const bool depends_on_deleted =
        references_deleted(params.source_plane_id) ||
        std::any_of(params.source_plane_ids.begin(),
                    params.source_plane_ids.end(), references_deleted) ||
        references_deleted(params.source_axis_id);
    if (depends_on_deleted) {
      params.plane_type = "detached";
      params.source_plane_id.clear();
      params.source_plane_ids.clear();
      params.source_axis_id.clear();
      remaining.parameters_summary = "Detached";
    }
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::select_feature(const std::string& feature_id) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  return document_.value();
}

DocumentState DocumentManager::select_reference(const std::string& reference_id) {
  require_document();
  if (!is_selectable_plane_reference(*document_, reference_id)) {
    throw std::runtime_error("Reference not found: " + reference_id);
  }
  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = reference_id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  return document_.value();
}

DocumentState DocumentManager::start_sketch_on_plane(
    const std::string& reference_id) {
  require_document();
  if (!is_selectable_plane_reference(*document_, reference_id)) {
    throw std::runtime_error("Sketch plane not found: " + reference_id);
  }
  push_undo_state();
  clear_redo_stack();
  std::optional<SketchFeatureParameters::SketchPlaneFrame> sketch_frame;
  if (!is_origin_plane_reference(reference_id)) {
    const auto frame = resolve_plane_source_frame(*document_, reference_id);
    if (!frame.has_value()) {
      throw std::runtime_error(
          "Sketch plane source could not be resolved: " + reference_id);
    }
    sketch_frame = to_sketch_plane_frame(frame.value());
  }
  document_->feature_history.push_back(create_sketch_feature(
      next_feature_id_++, reference_id, sketch_frame));
  const std::string sketch_feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = reference_id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->active_sketch_plane_id = reference_id;
  document_->active_sketch_face_id = std::nullopt;
  document_->active_sketch_feature_id = sketch_feature_id;
  document_->active_sketch_tool = "select";
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  begin_undo_group("Sketch Edit");
  return document_.value();
}

DocumentState DocumentManager::select_face(const std::string& face_id) {
  require_document();
  const std::string owner_id = face_owner_id(face_id);
  if (owner_id.empty()) {
    throw std::runtime_error("Face not found: " + face_id);
  }
  const auto feature_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == owner_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Face owner not found: " + face_id);
  }
  document_->selected_feature_id = feature_it->id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = face_id;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  return document_.value();
}

DocumentState DocumentManager::select_edge(const std::string& edge_id,
                                           bool additive) {
  require_document();
  const auto separator = edge_id.find(":edge:");
  if (separator == std::string::npos || separator == 0) {
    throw std::runtime_error("Malformed edge id: " + edge_id);
  }
  const std::string owner_id = edge_id.substr(0, separator);
  const auto feature_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == owner_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Edge owner not found: " + edge_id);
  }
  if (additive) {
    const auto existing = std::find(document_->selected_edge_ids.begin(),
                                    document_->selected_edge_ids.end(), edge_id);
    if (existing != document_->selected_edge_ids.end()) {
      document_->selected_edge_ids.erase(existing);
    } else {
      document_->selected_edge_ids.push_back(edge_id);
    }
  } else {
    document_->selected_edge_ids = {edge_id};
  }
  document_->selected_feature_id = feature_it->id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  return document_.value();
}

DocumentState DocumentManager::select_vertex(const std::string& vertex_id,
                                             bool additive) {
  require_document();
  const auto separator = vertex_id.find(":vertex:");
  if (separator == std::string::npos || separator == 0) {
    throw std::runtime_error("Malformed vertex id: " + vertex_id);
  }
  const std::string owner_id = vertex_id.substr(0, separator);
  const auto feature_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == owner_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Vertex owner not found: " + vertex_id);
  }
  if (additive) {
    const auto existing = std::find(document_->selected_vertex_ids.begin(),
                                    document_->selected_vertex_ids.end(),
                                    vertex_id);
    if (existing != document_->selected_vertex_ids.end()) {
      document_->selected_vertex_ids.erase(existing);
    } else {
      document_->selected_vertex_ids.push_back(vertex_id);
    }
  } else {
    document_->selected_vertex_ids = {vertex_id};
  }
  document_->selected_feature_id = feature_it->id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  return document_.value();
}

DocumentState DocumentManager::add_parameter(const std::string& name,
                                            const std::string& expression,
                                            const std::string& kind) {
  require_document();
  if (name.empty()) {
    throw std::runtime_error("Parameter name cannot be empty");
  }
  for (const auto& p : document_->parameters) {
    if (p.name == name) {
      throw std::runtime_error("Parameter '" + name + "' already exists");
    }
  }
  push_undo_state();
  clear_redo_stack();
  ParameterEntry entry;
  entry.name = name;
  entry.expression = expression;
  entry.kind = kind;
  document_->parameters.push_back(entry);
  reify_parameters(document_->parameters);
  for (auto& feat : document_->feature_history) {
    reify_dimension_expressions(feat, document_->parameters);
  }
  refresh_history_dependencies(document_.value());
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_parameter(const std::string& name,
                                                const std::string& expression,
                                                const std::string& kind) {
  require_document();
  if (name.empty()) {
    throw std::runtime_error("Parameter name cannot be empty");
  }
  ParameterEntry* target = nullptr;
  for (auto& p : document_->parameters) {
    if (p.name == name) {
      target = &p;
      break;
    }
  }
  if (!target) {
    throw std::runtime_error("Parameter '" + name + "' not found");
  }
  push_undo_state();
  clear_redo_stack();
  target->expression = expression;
  target->kind = kind;
  reify_parameters(document_->parameters);
  for (auto& feat : document_->feature_history) {
    reify_dimension_expressions(feat, document_->parameters);
  }
  refresh_history_dependencies(document_.value());
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::delete_parameter(const std::string& name) {
  require_document();
  if (name.empty()) {
    throw std::runtime_error("Parameter name cannot be empty");
  }
  auto it = document_->parameters.begin();
  for (; it != document_->parameters.end(); ++it) {
    if (it->name == name) {
      break;
    }
  }
  if (it == document_->parameters.end()) {
    throw std::runtime_error("Parameter '" + name + "' not found");
  }
  push_undo_state();
  clear_redo_stack();
  document_->parameters.erase(it);
  reify_parameters(document_->parameters);
  for (auto& feat : document_->feature_history) {
    reify_dimension_expressions(feat, document_->parameters);
  }
  refresh_history_dependencies(document_.value());
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_selection_filter(
    const SelectionFilter& filter) {
  require_document();
  push_undo_state();
  clear_redo_stack();
  document_->selection_filter = filter;
  return document_.value();
}

DocumentState DocumentManager::clear_selection() {
  require_document();
  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  document_->selected_sketch_text_id = std::nullopt;
  return document_.value();
}

ExportResult DocumentManager::export_document_as_step(
    const std::string& file_path) const {
  require_document();
  return polysmith::core::export_document_as_step(document_.value(), file_path);
}

ExportResult DocumentManager::export_cam_gcode(
    const std::string& file_path) const {
  require_document();
  const auto result =
      polysmith::core::export_cam_gcode(document_.value(), file_path);
  return ExportResult{result.file_path, result.format,
                      result.exported_feature_count};
}

CamGcodeTextResult DocumentManager::export_cam_gcode_text() const {
  require_document();
  return polysmith::core::post_cam_gcode_text(document_.value());
}

ExportResult DocumentManager::export_document_as_iges(
    const std::string& file_path) const {
  require_document();
  return polysmith::core::export_document_as_iges(document_.value(), file_path);
}

ExportResult DocumentManager::export_document_as_stl(
    const std::string& file_path) const {
  require_document();
  return polysmith::core::export_document_as_stl(document_.value(), file_path);
}

ExportResult DocumentManager::export_body_as_stl(
    const std::string& file_path, const std::string& body_id) const {
  require_document();
  return polysmith::core::export_body_as_stl(document_.value(), file_path,
                                             body_id);
}

ExportResult DocumentManager::export_body_as_step(
    const std::string& file_path, const std::string& body_id) const {
  require_document();
  return polysmith::core::export_body_as_step(document_.value(), file_path,
                                              body_id);
}

void DocumentManager::save_document_to_path(const std::string& file_path) const {
  require_document();
  if (file_path.empty()) {
    throw std::runtime_error("Save path cannot be empty");
  }
  const nlohmann::json document =
      polysmith::protocol::to_payload(document_.value(), /*include_opaque=*/true);
  const nlohmann::json payload = {
      {"format", "polysmith/1"},
      {"document", document},
  };
  write_file_atomically(std::filesystem::path(file_path), payload.dump(2));
  journal_truncate(document_->id);
}

DocumentState DocumentManager::load_document_from_path(
    const std::string& file_path) {
  if (file_path.empty()) {
    throw std::runtime_error("Load path cannot be empty");
  }
  std::ifstream stream(file_path);
  if (!stream.is_open()) {
    throw std::runtime_error("Failed to open file for reading: " + file_path);
  }
  std::stringstream buffer;
  buffer << stream.rdbuf();
  if (!stream.good() && !stream.eof()) {
    throw std::runtime_error("Failed to read document from: " + file_path);
  }
  nlohmann::json payload;
  try {
    payload = nlohmann::json::parse(buffer.str());
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("Document parse error: ") +
                             error.what());
  }
  const int version = document_format_version(payload);
  if (version > kCurrentFormatVersion) {
    throw std::runtime_error(
        "Document format version " + std::to_string(version) +
        " is newer than this build supports (" +
        std::to_string(kCurrentFormatVersion) + ")");
  }
  const nlohmann::json migrated = migrate_document_format(payload, version);
  DocumentState loaded =
      polysmith::protocol::document_from_payload(migrated.at("document"));
  for (auto& feature : loaded.feature_history) {
    if (feature.kind == "sketch") {
      polysmith::core::refresh_sketch_profiles(feature);
    }
  }
  if (loaded.selected_sketch_profile_id.has_value() ||
      !loaded.selected_sketch_profile_ids.empty()) {
    std::set<std::string> live_ids;
    for (const auto& feature : loaded.feature_history) {
      if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
        continue;
      }
      for (const auto& profile : feature.sketch_parameters->profiles) {
        live_ids.insert(profile.id);
      }
    }
    if (loaded.selected_sketch_profile_id.has_value() &&
        !live_ids.count(loaded.selected_sketch_profile_id.value())) {
      loaded.selected_sketch_profile_id.reset();
    }
    loaded.selected_sketch_profile_ids.erase(
        std::remove_if(loaded.selected_sketch_profile_ids.begin(),
                       loaded.selected_sketch_profile_ids.end(),
                       [&](const std::string& id) {
                         return !live_ids.count(id);
                       }),
        loaded.selected_sketch_profile_ids.end());
  }
  document_ = loaded;
  undo_stack_.clear();
  redo_stack_.clear();
  clear_imported_shapes();
  if (document_count_ == 0) {
    document_count_ = 1;
  }
  next_document_id_ =
      std::max(next_document_id_, trailing_integer(loaded.id) + 1);
  for (const auto& feature : loaded.feature_history) {
    next_feature_id_ =
        std::max(next_feature_id_, trailing_integer(feature.id) + 1);
    if (!feature.sketch_parameters.has_value()) {
      continue;
    }
    const auto& sketch = feature.sketch_parameters.value();
    for (const auto& line : sketch.lines) {
      next_sketch_line_id_ =
          std::max(next_sketch_line_id_, trailing_integer(line.id) + 1);
    }
    for (const auto& circle : sketch.circles) {
      next_sketch_circle_id_ =
          std::max(next_sketch_circle_id_, trailing_integer(circle.id) + 1);
    }
    for (const auto& arc : sketch.arcs) {
      next_sketch_arc_id_ =
          std::max(next_sketch_arc_id_, trailing_integer(arc.id) + 1);
    }
    for (const auto& fillet : sketch.fillets) {
      next_sketch_fillet_id_ =
          std::max(next_sketch_fillet_id_, trailing_integer(fillet.id) + 1);
    }
    for (const auto& projected : sketch.projected_points) {
      next_sketch_projected_point_id_ = std::max(
          next_sketch_projected_point_id_, trailing_integer(projected.id) + 1);
    }
    for (const auto& projection : sketch.projections) {
      next_sketch_projection_id_ = std::max(
          next_sketch_projection_id_, trailing_integer(projection.id) + 1);
    }
  }
  for (const auto& setup : loaded.cam.setups) {
    next_cam_setup_id_ =
        std::max(next_cam_setup_id_, trailing_integer(setup.setup_id) + 1);
  }
  for (const auto& tool : loaded.cam.tool_library) {
    next_cam_tool_id_ =
        std::max(next_cam_tool_id_, trailing_integer(tool.tool_id) + 1);
  }
  for (const auto& op : loaded.cam.operations) {
    next_cam_op_id_ =
        std::max(next_cam_op_id_, trailing_integer(op.op_id) + 1);
    for (const auto& ref : op.geometry_references.machining_regions) {
      if (ref.persistent_id.rfind("pt-", 0) == 0) {
        next_cam_point_ref_id_ = std::max(
            next_cam_point_ref_id_, trailing_integer(ref.persistent_id) + 1);
      }
    }
  }
  for (const auto& drawing : loaded.drawing.drawings) {
    next_drawing_id_ =
        std::max(next_drawing_id_, trailing_integer(drawing.drawing_id) + 1);
    for (const auto& sheet : drawing.sheets) {
      next_drawing_sheet_id_ = std::max(
          next_drawing_sheet_id_, trailing_integer(sheet.sheet_id) + 1);
    }
    for (const auto& view : drawing.views) {
      next_drawing_view_id_ = std::max(
          next_drawing_view_id_, trailing_integer(view.view_id) + 1);
    }
    for (const auto& annotation : drawing.annotations) {
      next_drawing_annotation_id_ = std::max(
          next_drawing_annotation_id_,
          trailing_integer(annotation.annotation_id) + 1);
      if (annotation.source_edge_id.rfind("drawing-edge-", 0) == 0) {
        next_drawing_edge_ref_id_ = std::max(
            next_drawing_edge_ref_id_,
            trailing_integer(annotation.source_edge_id) + 1);
      }
    }
    for (const auto& note : drawing.notes) {
      next_drawing_note_id_ =
          std::max(next_drawing_note_id_, trailing_integer(note.note_id) + 1);
    }
  }
  for (auto& feature : document_->feature_history) {
    if (feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
      polysmith::core::refresh_sketch_derived_state(feature);
    }
  }
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  if (document_) {
    bump_geometry_revision();
  }
  return document_.value();
}

std::optional<DocumentState> DocumentManager::get_document() const {
  return document_;
}

SessionState DocumentManager::get_session_state() const {
  return SessionState{
      .document_count = document_count_,
      .active_document_id =
          document_.has_value() ? std::make_optional(document_->id)
                                : std::nullopt,
      .can_undo = !undo_stack_.empty(),
      .can_redo = !redo_stack_.empty(),
  };
}

namespace {

bool is_supported_machine_type(const std::string& type) {
  static const std::set<std::string> kSupported = {
      "3_axis_mill", "4_axis_mill", "5_axis_mill", "lathe_2_axis",
      "lathe_live_tooling", "laser", "plasma", "printer"};
  return kSupported.count(type) > 0;
}

bool is_supported_tool_type(const std::string& type) {
  static const std::set<std::string> kSupported = {
      "endmill_flat", "endmill_ball", "endmill_bull", "drill",
      "facemill", "chamfer", "threadmill", "turning_insert",
      "laser", "plasma", "v_bit", "spot_drill"};
  return kSupported.count(type) > 0;
}

int next_free_tool_number(const std::vector<ToolEntry>& tools) {
  int candidate = 1;
  while (true) {
    bool used = false;
    for (const auto& tool : tools) {
      if (tool.tool_number == candidate) {
        used = true;
        break;
      }
    }
    if (!used) return candidate;
    ++candidate;
  }
}

void validate_tool_entry(const ToolEntry& tool,
                         const std::vector<ToolEntry>& tools,
                         const std::string& ignore_tool_id) {
  if (tool.name.empty()) {
    throw std::runtime_error("The tool needs a name");
  }
  if (!is_supported_tool_type(tool.type)) {
    throw std::runtime_error("Unknown tool type: " + tool.type);
  }
  if (tool.diameter_mm <= 0.0) {
    throw std::runtime_error("The tool diameter must be positive");
  }
  if (tool.shank_diameter_mm <= 0.0) {
    throw std::runtime_error("The tool shank diameter must be positive");
  }
  if (tool.flute_length_mm <= 0.0) {
    throw std::runtime_error("The tool flute length must be positive");
  }
  if (tool.overall_length_mm < tool.flute_length_mm) {
    throw std::runtime_error(
        "The overall length must be at least the flute length");
  }
  if (tool.corner_radius_mm < 0.0 ||
      tool.corner_radius_mm > tool.diameter_mm / 2.0) {
    throw std::runtime_error(
        "The corner radius must be between 0 and half the diameter");
  }
  if (tool.flutes < 1 || tool.flutes > 12) {
    throw std::runtime_error("The tool flute count must be between 1 and 12");
  }
  if (tool.orientation < 0 || tool.orientation > 9) {
    throw std::runtime_error("The tool orientation must be between 0 and 9");
  }
  if (tool.tool_number < 0) {
    throw std::runtime_error("The tool number must be positive");
  }
  if (tool.tool_number > 0) {
    for (const auto& existing : tools) {
      if (existing.tool_id != ignore_tool_id &&
          existing.tool_number == tool.tool_number) {
        throw std::runtime_error("Tool number " +
                                 std::to_string(tool.tool_number) +
                                 " is already used by " + existing.name);
      }
    }
  }
}

}  // namespace

DocumentState DocumentManager::cam_setup_create(const CamSetup& setup) {
  require_document();
  if (setup.name.empty()) {
    throw std::runtime_error("The CAM setup needs a name");
  }
  if (!is_supported_machine_type(setup.machine_type)) {
    throw std::runtime_error("Unknown machine type: " + setup.machine_type);
  }
  if (setup.units != "mm" && setup.units != "inch") {
    throw std::runtime_error("CAM units must be 'mm' or 'inch'");
  }
  if (!setup.setup_id.empty()) {
    for (const auto& existing : document_->cam.setups) {
      if (existing.setup_id == setup.setup_id) {
        throw std::runtime_error("A CAM setup with this id already exists");
      }
    }
  }
  push_undo_state();
  clear_redo_stack();
  CamSetup stored = setup;
  if (stored.setup_id.empty()) {
    stored.setup_id = "cam-setup-" + std::to_string(next_cam_setup_id_++);
  }
  document_->cam.setups.push_back(stored);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::cam_machine_settings_set(
    const LaserMachineSettings& machine_settings) {
  require_document();
  if (machine_settings.work_area_x_mm <= 0.0 ||
      machine_settings.work_area_y_mm <= 0.0) {
    throw std::runtime_error("The machine work area must be positive");
  }
  push_undo_state();
  clear_redo_stack();
  document_->cam.machine_settings = machine_settings;
  bump_geometry_revision();
  return document_.value();
}

std::optional<CamSetup> DocumentManager::cam_setup_get() const {
  return cam_setup_find("");
}

std::optional<CamSetup> DocumentManager::cam_setup_find(
    const std::string& setup_id) const {
  if (!document_.has_value() || document_->cam.setups.empty())
    return std::nullopt;
  if (setup_id.empty()) return document_->cam.setups[0];
  for (const auto& setup : document_->cam.setups) {
    if (setup.setup_id == setup_id) return setup;
  }
  return std::nullopt;
}

DocumentState DocumentManager::cam_setup_update(const CamSetup& setup) {
  require_document();
  if (setup.name.empty()) {
    throw std::runtime_error("The CAM setup needs a name");
  }
  if (!is_supported_machine_type(setup.machine_type)) {
    throw std::runtime_error("Unknown machine type: " + setup.machine_type);
  }
  for (auto& existing : document_->cam.setups) {
    if (existing.setup_id == setup.setup_id) {
      push_undo_state();
      clear_redo_stack();
      existing = setup;
      bump_geometry_revision();
      return document_.value();
    }
  }
  throw std::runtime_error("CAM setup not found: " + setup.setup_id);
}

DocumentState DocumentManager::cam_setup_delete(const std::string& setup_id) {
  require_document();
  const auto setup_it = std::find_if(
      document_->cam.setups.begin(), document_->cam.setups.end(),
      [&](const CamSetup& setup) { return setup.setup_id == setup_id; });
  if (setup_it == document_->cam.setups.end()) {
    throw std::runtime_error("CAM setup not found: " + setup_id);
  }
  const bool was_first_setup = setup_it == document_->cam.setups.begin();
  push_undo_state();
  clear_redo_stack();
  document_->cam.setups.erase(setup_it);
  document_->cam.operations.erase(
      std::remove_if(
          document_->cam.operations.begin(), document_->cam.operations.end(),
          [&](const CamOperation& op) {
            return op.setup_id == setup_id ||
                   (was_first_setup && op.setup_id.empty());
          }),
      document_->cam.operations.end());
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::cam_tool_add(const ToolEntry& tool) {
  require_document();
  validate_tool_entry(tool, document_->cam.tool_library, /*ignore_tool_id=*/"");
  if (!tool.tool_id.empty()) {
    for (const auto& existing : document_->cam.tool_library) {
      if (existing.tool_id == tool.tool_id) {
        throw std::runtime_error("A tool with this id already exists");
      }
    }
  }
  push_undo_state();
  clear_redo_stack();
  ToolEntry stored = tool;
  if (stored.tool_id.empty()) {
    stored.tool_id = "tool-" + std::to_string(next_cam_tool_id_++);
  }
  if (stored.tool_number == 0) {
    stored.tool_number = next_free_tool_number(document_->cam.tool_library);
  }
  if (stored.guid.empty()) {
    stored.guid = polysmith::core::mint_tool_guid();
  }
  document_->cam.tool_library.push_back(stored);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::cam_tool_update(const std::string& toolId,
                                               const ToolEntry& tool) {
  require_document();
  validate_tool_entry(tool, document_->cam.tool_library, toolId);
  for (auto& existing : document_->cam.tool_library) {
    if (existing.tool_id == toolId) {
      push_undo_state();
      clear_redo_stack();
      existing = tool;
      existing.tool_id = toolId;
      if (existing.tool_number == 0) {
        existing.tool_number = next_free_tool_number(document_->cam.tool_library);
      }
      if (existing.guid.empty()) {
        existing.guid = polysmith::core::mint_tool_guid();
      }
      for (auto& op : document_->cam.operations) {
        if (op.tool_id == toolId) {
          op.status = "needs_regenerate";
          op.status_message.clear();
          op.toolpath_cache.reset();
        }
      }
      bump_geometry_revision();
      return document_.value();
    }
  }
  throw std::runtime_error("Tool not found: " + toolId);
}

DocumentState DocumentManager::cam_tool_delete(const std::string& toolId) {
  require_document();
  bool found = false;
  for (auto it = document_->cam.tool_library.begin();
       it != document_->cam.tool_library.end(); ++it) {
    if (it->tool_id == toolId) {
      push_undo_state();
      clear_redo_stack();
      document_->cam.tool_library.erase(it);
      for (auto& op : document_->cam.operations) {
        if (op.tool_id == toolId) {
          op.status = "error";
          op.status_message = "The tool used by this operation was deleted.";
          op.toolpath_cache.reset();
        }
      }
      bump_geometry_revision();
      found = true;
      break;
    }
  }
  if (!found) {
    throw std::runtime_error("Tool not found: " + toolId);
  }
  return document_.value();
}

std::vector<ToolEntry> DocumentManager::cam_tool_list() const {
  if (!document_.has_value()) return {};
  return document_->cam.tool_library;
}

DocumentState DocumentManager::cam_tool_import_tools(
    std::vector<ToolEntry> incoming, const std::string& mode) {
  require_document();
  if (mode != "renumber" && mode != "overwrite" && mode != "skip") {
    throw std::runtime_error("Unknown tool import mode: " + mode);
  }
  push_undo_state();
  clear_redo_stack();
  auto& library = document_->cam.tool_library;
  int added = 0;
  int skipped = 0;
  int replaced = 0;
  std::vector<std::string> replaced_ids;
  for (auto& tool : incoming) {
    auto byGuid = library.end();
    if (!tool.guid.empty()) {
      for (auto it = library.begin(); it != library.end(); ++it) {
        if (!it->guid.empty() && it->guid == tool.guid) {
          byGuid = it;
          break;
        }
      }
    }
    auto byNumber = library.end();
    if (tool.tool_number > 0) {
      for (auto it = library.begin(); it != library.end(); ++it) {
        if (it->tool_number == tool.tool_number) {
          byNumber = it;
          break;
        }
      }
    }
    if (byGuid != library.end()) {
      if (mode == "skip") {
        ++skipped;
        continue;
      }
      const std::string id = byGuid->tool_id;
      *byGuid = tool;
      byGuid->tool_id = id;
      bool clash = false;
      if (byGuid->tool_number > 0) {
        for (auto it = library.begin(); it != library.end(); ++it) {
          if (it != byGuid && it->tool_number == byGuid->tool_number) {
            clash = true;
            break;
          }
        }
      }
      if (clash || byGuid->tool_number == 0) {
        byGuid->tool_number = next_free_tool_number(library);
      }
      if (byGuid->guid.empty()) {
        byGuid->guid = polysmith::core::mint_tool_guid();
      }
      replaced_ids.push_back(id);
      ++replaced;
      continue;
    }
    if (byNumber != library.end()) {
      if (mode == "skip") {
        ++skipped;
        continue;
      }
      if (mode == "overwrite") {
        const std::string id = byNumber->tool_id;
        const int number = byNumber->tool_number;
        *byNumber = tool;
        byNumber->tool_id = id;
        byNumber->tool_number = number;
        if (byNumber->guid.empty()) {
          byNumber->guid = polysmith::core::mint_tool_guid();
        }
        replaced_ids.push_back(id);
        ++replaced;
        continue;
      }
      tool.tool_number = 0;
    }
    ToolEntry stored = tool;
    if (stored.tool_id.empty()) {
      stored.tool_id = "tool-" + std::to_string(next_cam_tool_id_++);
    }
    if (stored.guid.empty()) {
      stored.guid = polysmith::core::mint_tool_guid();
    }
    if (stored.tool_number == 0) {
      stored.tool_number = next_free_tool_number(library);
    }
    validate_tool_entry(stored, library, /*ignore_tool_id=*/"");
    library.push_back(stored);
    ++added;
  }
  for (auto& op : document_->cam.operations) {
    if (std::find(replaced_ids.begin(), replaced_ids.end(), op.tool_id) !=
        replaced_ids.end()) {
      op.status = "needs_regenerate";
      op.status_message.clear();
      op.toolpath_cache.reset();
    }
  }
  bump_geometry_revision();
  log_info("cam", "tool import (" + mode + "): added " + std::to_string(added) +
                      ", skipped " + std::to_string(skipped) +
                      ", replaced " + std::to_string(replaced));
  return document_.value();
}

DocumentState DocumentManager::cam_operation_add(const CamOperation& op) {
  require_document();
  if (op.type.empty()) {
    throw std::runtime_error("The CAM operation needs a type");
  }
  if ((op.type == "laser_cut" || op.type == "laser_test_pattern") &&
      !document_->cam.setups.empty() &&
      document_->cam.setups[0].machine_type != "laser") {
    throw std::runtime_error("A laser operation requires a laser machine setup");
  }
  if (op.type == "drilling" && !document_->cam.setups.empty() &&
      document_->cam.setups[0].machine_type == "laser") {
    throw std::runtime_error("A drilling operation requires a milling machine setup");
  }
  if (op.type == "engrave" && !document_->cam.setups.empty() &&
      document_->cam.setups[0].machine_type == "laser") {
    throw std::runtime_error("An engrave operation requires a milling machine setup");
  }
  if (!op.op_id.empty()) {
    for (const auto& existing : document_->cam.operations) {
      if (existing.op_id == op.op_id) {
        throw std::runtime_error("A CAM operation with this id already exists");
      }
    }
  }
  CamOperation stored = op;
  if (stored.op_id.empty()) {
    stored.op_id = "cam-op-" + std::to_string(next_cam_op_id_++);
  }
  if (stored.setup_id.empty() && !document_->cam.setups.empty()) {
    stored.setup_id = document_->cam.setups[0].setup_id;
  }
  bool createDefaultTool = false;
  ToolEntry defaultTool;
  if (stored.tool_id.empty()) {
    const bool wantsLaser =
        stored.type == "laser_cut" || stored.type == "laser_test_pattern";
    const bool wantsDrill = stored.type == "drilling";
    const ToolEntry* match = nullptr;
    for (const auto& tool : document_->cam.tool_library) {
      if (wantsLaser ? tool.type == "laser"
                     : (wantsDrill ? tool.type == "drill"
                                   : tool.type == "endmill_flat")) {
        match = &tool;
        break;
      }
    }
    if (match != nullptr) {
      stored.tool_id = match->tool_id;
    } else if (wantsLaser) {
      createDefaultTool = true;
      defaultTool.tool_id = "tool-" + std::to_string(next_cam_tool_id_++);
      defaultTool.name = "CO2 laser (default)";
      defaultTool.type = "laser";
      stored.tool_id = defaultTool.tool_id;
      defaultTool.tool_number =
          next_free_tool_number(document_->cam.tool_library);
      defaultTool.guid = polysmith::core::mint_tool_guid();
    }
  } else {
    bool toolExists = false;
    for (const auto& tool : document_->cam.tool_library) {
      if (tool.tool_id == stored.tool_id) {
        toolExists = true;
        break;
      }
    }
    if (!toolExists) {
      throw std::runtime_error("Unknown tool: " + stored.tool_id);
    }
  }
  push_undo_state();
  clear_redo_stack();
  if (createDefaultTool) {
    document_->cam.tool_library.push_back(defaultTool);
  }
  stored.status = "pending";
  stored.status_message.clear();
  stored.toolpath_cache.reset();
  document_->cam.operations.push_back(stored);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::cam_operation_update(
    const std::string& operation_id, const CamOperation& op) {
  require_document();
  if (!op.tool_id.empty()) {
    bool toolExists = false;
    for (const auto& tool : document_->cam.tool_library) {
      if (tool.tool_id == op.tool_id) {
        toolExists = true;
        break;
      }
    }
    if (!toolExists) {
      throw std::runtime_error("Unknown tool: " + op.tool_id);
    }
  }
  for (auto& existing : document_->cam.operations) {
    if (existing.op_id == operation_id) {
      push_undo_state();
      clear_redo_stack();
      existing = op;
      existing.op_id = operation_id;
      existing.status = "needs_regenerate";
      existing.status_message.clear();
      existing.toolpath_cache.reset();
      bump_geometry_revision();
      return document_.value();
    }
  }
  throw std::runtime_error("CAM operation not found: " + operation_id);
}

DocumentState DocumentManager::cam_operation_delete(
    const std::string& operation_id) {
  require_document();
  for (auto it = document_->cam.operations.begin();
       it != document_->cam.operations.end(); ++it) {
    if (it->op_id == operation_id) {
      push_undo_state();
      clear_redo_stack();
      document_->cam.operations.erase(it);
      bump_geometry_revision();
      return document_.value();
    }
  }
  throw std::runtime_error("CAM operation not found: " + operation_id);
}

DocumentState DocumentManager::cam_operation_set_scope(
    const std::string& operation_id, const std::string& sketch_feature_id) {
  require_document();
  const FeatureEntry* sketch = nullptr;
  for (const auto& feature : document_->feature_history) {
    if (feature.id == sketch_feature_id && feature.kind == "sketch" &&
        feature.sketch_parameters.has_value()) {
      sketch = &feature;
      break;
    }
  }
  if (sketch == nullptr) {
    throw std::runtime_error("CAM scope sketch not found: " + sketch_feature_id);
  }
  for (auto& existing : document_->cam.operations) {
    if (existing.op_id == operation_id) {
      push_undo_state();
      clear_redo_stack();
      existing.geometry_references = CamGeometryReferences{};
      if (!polysmith::core::capture_profile_references_from_sketch(
              document_.value(), sketch_feature_id, existing)) {
        log_warn("cam", "set scope on sketch '" + sketch->name +
                            "' which has no profile regions");
      }
      existing.geometry_scope = "sketch";
      existing.status = "needs_regenerate";
      existing.status_message.clear();
      existing.toolpath_cache.reset();
      bump_geometry_revision();
      return document_.value();
    }
  }
  throw std::runtime_error("CAM operation not found: " + operation_id);
}

GeometryReference DocumentManager::cam_capture_point(
    const std::array<double, 3>& point) {
  require_document();
  GeometryReference ref;
  ref.persistent_id = "pt-" + std::to_string(next_cam_point_ref_id_++);
  ref.attestation = PointAttestation{point};
  return ref;
}

DocumentState DocumentManager::cam_post_processor_set(
    const PostProcessor& post_processor) {
  require_document();
  push_undo_state();
  clear_redo_stack();
  document_->cam.post_processor = post_processor;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::cam_operation_set_generated(
    const std::string& operation_id, const CamOperation& op,
    const Toolpath& toolpath) {
  require_document();
  for (auto& existing : document_->cam.operations) {
    if (existing.op_id == operation_id) {
      const std::string scope = existing.geometry_scope;
      push_undo_state();
      clear_redo_stack();
      existing = op;
      existing.op_id = operation_id;
      existing.geometry_scope = scope;
      existing.status = "generated";
      existing.status_message.clear();
      cam_runtime::store_generated_at(document_.value(), operation_id, toolpath,
                                      document_->revision + 1);
      bump_geometry_revision();
      return document_.value();
    }
  }
  throw std::runtime_error("CAM operation not found: " + operation_id);
}

namespace {

Drawing* find_drawing(DocumentState& document, const std::string& drawing_id) {
  for (auto& drawing : document.drawing.drawings) {
    if (drawing.drawing_id == drawing_id) {
      return &drawing;
    }
  }
  return nullptr;
}

DrawingSheet* find_sheet(Drawing& drawing, const std::string& sheet_id) {
  for (auto& sheet : drawing.sheets) {
    if (sheet.sheet_id == sheet_id) {
      return &sheet;
    }
  }
  return nullptr;
}

DrawingView* find_view(Drawing& drawing, const std::string& view_id) {
  for (auto& view : drawing.views) {
    if (view.view_id == view_id) {
      return &view;
    }
  }
  return nullptr;
}

void validate_view_definition(const DrawingView& view, const Drawing& drawing) {
  if (view.kind != "projection" && view.kind != "axonometric" &&
      view.kind != "section" && view.kind != "detail") {
    throw std::runtime_error("Unknown view kind: " + view.kind);
  }
  if (view.kind == "section") {
    if (!view.section.has_value()) {
      throw std::runtime_error("Section views need a section definition");
    }
    const auto& normal = view.section->cutting_plane_normal;
    const double length = std::sqrt(normal[0] * normal[0] +
                                    normal[1] * normal[1] +
                                    normal[2] * normal[2]);
    if (length < 1e-9) {
      throw std::runtime_error("The cutting plane normal is degenerate");
    }
    if (!(view.section->hatch_spacing_mm > 0.0)) {
      throw std::runtime_error("The hatch spacing must be positive");
    }
  } else if (view.kind == "detail") {
    if (!view.detail.has_value()) {
      throw std::runtime_error("Detail views need a detail definition");
    }
    const auto& detail = view.detail.value();
    if (detail.parent_view_id.empty()) {
      throw std::runtime_error("The detail view needs a parent view");
    }
    if (!(detail.radius > 0.0)) {
      throw std::runtime_error("The detail circle radius must be positive");
    }
    const DrawingView* parent = nullptr;
    for (const auto& candidate : drawing.views) {
      if (candidate.view_id == detail.parent_view_id) {
        parent = &candidate;
        break;
      }
    }
    if (parent == nullptr) {
      throw std::runtime_error("The detail view's parent view does not exist");
    }
    if (parent->kind != "projection") {
      throw std::runtime_error(
          "Detail views need a projection view as their parent");
    }
  } else {
    if (!view.standard_view.empty() &&
        !polysmith::core::standard_view_frame(view.standard_view).has_value()) {
      throw std::runtime_error("Unknown standard view: " + view.standard_view);
    }
    if (view.kind == "axonometric" && !view.custom_frame.has_value()) {
      throw std::runtime_error("Axonometric views need a custom frame");
    }
    if (view.standard_view.empty() && !view.custom_frame.has_value()) {
      throw std::runtime_error(
          "The view needs a standard view or a custom frame");
    }
  }
  if (view.source_body_ids.empty()) {
    throw std::runtime_error("The view needs at least one source body");
  }
}

}  // namespace

DocumentState DocumentManager::drawing_create(const Drawing& drawing) {
  require_document();
  if (drawing.name.empty()) {
    throw std::runtime_error("The drawing needs a name");
  }
  if (!drawing.drawing_id.empty()) {
    for (const auto& existing : document_->drawing.drawings) {
      if (existing.drawing_id == drawing.drawing_id) {
        throw std::runtime_error("A drawing with this id already exists");
      }
    }
  }
  if (drawing.sheets.empty()) {
    throw std::runtime_error("A drawing needs at least one sheet");
  }
  push_undo_state();
  clear_redo_stack();
  Drawing stored = drawing;
  if (stored.drawing_id.empty()) {
    stored.drawing_id = "drawing-" + std::to_string(next_drawing_id_++);
  }
  for (auto& sheet : stored.sheets) {
    if (sheet.sheet_id.empty()) {
      sheet.sheet_id =
          "drawing-sheet-" + std::to_string(next_drawing_sheet_id_++);
    }
  }
  for (auto& view : stored.views) {
    if (view.view_id.empty()) {
      view.view_id = "drawing-view-" + std::to_string(next_drawing_view_id_++);
    }
  }
  for (auto& annotation : stored.annotations) {
    if (annotation.annotation_id.empty()) {
      annotation.annotation_id =
          "drawing-annotation-" + std::to_string(next_drawing_annotation_id_++);
    }
  }
  document_->drawing.drawings.push_back(stored);
  document_->drawing.active_drawing_id = stored.drawing_id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_delete(const std::string& drawing_id) {
  require_document();
  auto& drawings = document_->drawing.drawings;
  const auto found =
      std::find_if(drawings.begin(), drawings.end(),
                   [&](const Drawing& d) { return d.drawing_id == drawing_id; });
  if (found == drawings.end()) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  push_undo_state();
  clear_redo_stack();
  drawings.erase(found);
  if (document_->drawing.active_drawing_id == drawing_id) {
    document_->drawing.active_drawing_id.reset();
  }
  if (document_->drawing.selected_view_id.has_value()) {
    const bool dead = std::none_of(
        drawings.begin(), drawings.end(), [&](const Drawing& d) {
          return std::any_of(
              d.views.begin(), d.views.end(), [&](const DrawingView& v) {
                return v.view_id == document_->drawing.selected_view_id.value();
              });
        });
    if (dead) {
      document_->drawing.selected_view_id.reset();
    }
  }
  if (document_->drawing.selected_annotation_id.has_value()) {
    const bool dead = std::none_of(
        drawings.begin(), drawings.end(), [&](const Drawing& d) {
          return std::any_of(
              d.annotations.begin(), d.annotations.end(),
              [&](const Annotation& a) {
                return a.annotation_id ==
                       document_->drawing.selected_annotation_id.value();
              });
        });
    if (dead) {
      document_->drawing.selected_annotation_id.reset();
    }
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_set_active(
    const std::string& drawing_id) {
  require_document();
  const bool exists = find_drawing(document_.value(), drawing_id) != nullptr;
  if (!exists) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  push_undo_state();
  clear_redo_stack();
  document_->drawing.active_drawing_id = drawing_id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_sheet_create(
    const std::string& drawing_id, const DrawingSheet& sheet) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  if (!sheet.sheet_id.empty() && find_sheet(*drawing, sheet.sheet_id) != nullptr) {
    throw std::runtime_error("A sheet with this id already exists");
  }
  for (const auto& view_id : sheet.view_ids) {
    const bool exists = std::any_of(
        drawing->views.begin(), drawing->views.end(),
        [&](const DrawingView& v) { return v.view_id == view_id; });
    if (!exists) {
      throw std::runtime_error("Unknown view: " + view_id);
    }
  }
  push_undo_state();
  clear_redo_stack();
  DrawingSheet stored = sheet;
  if (stored.sheet_id.empty()) {
    stored.sheet_id =
        "drawing-sheet-" + std::to_string(next_drawing_sheet_id_++);
  }
  drawing->sheets.push_back(stored);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_sheet_delete(
    const std::string& drawing_id, const std::string& sheet_id) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  DrawingSheet* sheet = find_sheet(*drawing, sheet_id);
  if (sheet == nullptr) {
    throw std::runtime_error("Unknown sheet: " + sheet_id);
  }
  push_undo_state();
  clear_redo_stack();
  const std::vector<std::string> dying_view_ids = sheet->view_ids;
  drawing->sheets.erase(
      std::remove_if(drawing->sheets.begin(), drawing->sheets.end(),
                     [&](const DrawingSheet& s) {
                       return s.sheet_id == sheet_id;
                     }),
      drawing->sheets.end());
  for (const auto& view_id : dying_view_ids) {
    drawing->views.erase(
        std::remove_if(drawing->views.begin(), drawing->views.end(),
                       [&](const DrawingView& v) {
                         return v.view_id == view_id;
                       }),
        drawing->views.end());
  }
  for (const auto& annotation : drawing->annotations) {
    if (std::find(dying_view_ids.begin(), dying_view_ids.end(),
                  annotation.view_id) != dying_view_ids.end()) {
      if (is_annotation_kind(annotation.kind)) {
        drawing_runtime::erase_attachment(document_->id, annotation.annotation_id);
      } else {
        drawing_runtime::erase_dimension(document_->id, annotation.annotation_id);
      }
    }
  }
  drawing->annotations.erase(
      std::remove_if(drawing->annotations.begin(), drawing->annotations.end(),
                     [&](const Annotation& a) {
                       return std::find(dying_view_ids.begin(),
                                        dying_view_ids.end(),
                                        a.view_id) != dying_view_ids.end();
                     }),
      drawing->annotations.end());
  drawing->notes.erase(
      std::remove_if(drawing->notes.begin(), drawing->notes.end(),
                     [&](const SheetNote& n) {
                       return n.sheet_id == sheet_id;
                     }),
      drawing->notes.end());
  if (document_->drawing.selected_view_id.has_value() &&
      std::find(dying_view_ids.begin(), dying_view_ids.end(),
                document_->drawing.selected_view_id.value()) !=
          dying_view_ids.end()) {
    document_->drawing.selected_view_id.reset();
  }
  if (document_->drawing.selected_annotation_id.has_value()) {
    const auto& selected = document_->drawing.selected_annotation_id.value();
    const bool dead = std::none_of(
        drawing->annotations.begin(), drawing->annotations.end(),
        [&](const Annotation& a) { return a.annotation_id == selected; });
    if (dead) {
      document_->drawing.selected_annotation_id.reset();
    }
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_sheet_update(
    const std::string& drawing_id, const std::string& sheet_id,
    const std::string& paper_size, const std::string& orientation,
    const std::string& projection_angle, const std::string& name) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  DrawingSheet* sheet = find_sheet(*drawing, sheet_id);
  if (sheet == nullptr) {
    throw std::runtime_error("Unknown sheet: " + sheet_id);
  }
  static const std::vector<std::string> kPaperSizes = {"A0", "A1", "A2", "A3", "A4"};
  if (std::find(kPaperSizes.begin(), kPaperSizes.end(), paper_size) ==
      kPaperSizes.end()) {
    throw std::runtime_error("Unknown paper size: " + paper_size);
  }
  if (orientation != "portrait" && orientation != "landscape") {
    throw std::runtime_error("Unknown orientation: " + orientation);
  }
  if (projection_angle != "first_angle" && projection_angle != "third_angle") {
    throw std::runtime_error("Unknown projection angle: " + projection_angle);
  }
  push_undo_state();
  clear_redo_stack();
  sheet->paper_size = paper_size;
  sheet->orientation = orientation;
  sheet->projection_angle = projection_angle;
  sheet->name = name;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_title_block_update(
    const std::string& drawing_id, const std::string& sheet_id,
    const TitleBlock& title_block) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  DrawingSheet* sheet = find_sheet(*drawing, sheet_id);
  if (sheet == nullptr) {
    throw std::runtime_error("Unknown sheet: " + sheet_id);
  }
  push_undo_state();
  clear_redo_stack();
  sheet->title_block = title_block;
  bump_geometry_revision();
  return document_.value();
}

ExportResult DocumentManager::export_drawing_sheet(
    const std::string& drawing_id, const std::string& sheet_id,
    const std::string& format, const std::string& file_path,
    const std::string& dxf_mode) const {
  require_document();
  return polysmith::core::export_drawing_sheet(
      document_.value(), drawing_id, sheet_id, format, file_path, dxf_mode);
}

DocumentState DocumentManager::drawing_view_create(
    const std::string& drawing_id, const std::string& sheet_id,
    const DrawingView& view) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  if (find_sheet(*drawing, sheet_id) == nullptr) {
    throw std::runtime_error("Unknown sheet: " + sheet_id);
  }
  if (!view.view_id.empty() && find_view(*drawing, view.view_id) != nullptr) {
    throw std::runtime_error("A view with this id already exists");
  }
  validate_view_definition(view, *drawing);
  push_undo_state();
  clear_redo_stack();
  DrawingView stored = view;
  if (stored.view_id.empty()) {
    stored.view_id = "drawing-view-" + std::to_string(next_drawing_view_id_++);
  }
  drawing->views.push_back(stored);
  find_sheet(*drawing, sheet_id)->view_ids.push_back(stored.view_id);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_view_update(
    const std::string& drawing_id, const DrawingView& view) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  DrawingView* existing = find_view(*drawing, view.view_id);
  if (existing == nullptr) {
    throw std::runtime_error("Unknown view: " + view.view_id);
  }
  validate_view_definition(view, *drawing);
  push_undo_state();
  clear_redo_stack();
  *existing = view;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_view_delete(
    const std::string& drawing_id, const std::string& view_id) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  if (find_view(*drawing, view_id) == nullptr) {
    throw std::runtime_error("Unknown view: " + view_id);
  }
  push_undo_state();
  clear_redo_stack();
  std::vector<std::string> removed_ids = {view_id};
  for (const auto& v : drawing->views) {
    if (v.kind == "detail" && v.detail.has_value() &&
        v.detail.value().parent_view_id == view_id) {
      removed_ids.push_back(v.view_id);
    }
  }
  drawing->views.erase(
      std::remove_if(drawing->views.begin(), drawing->views.end(),
                     [&](const DrawingView& v) {
                       return std::find(removed_ids.begin(), removed_ids.end(),
                                        v.view_id) != removed_ids.end();
                     }),
      drawing->views.end());
  for (const auto& annotation : drawing->annotations) {
    if (std::find(removed_ids.begin(), removed_ids.end(), annotation.view_id) !=
        removed_ids.end()) {
      if (is_annotation_kind(annotation.kind)) {
        drawing_runtime::erase_attachment(document_->id, annotation.annotation_id);
      } else {
        drawing_runtime::erase_dimension(document_->id, annotation.annotation_id);
      }
    }
  }
  drawing->annotations.erase(
      std::remove_if(drawing->annotations.begin(), drawing->annotations.end(),
                     [&](const Annotation& a) {
                       return std::find(removed_ids.begin(), removed_ids.end(),
                                        a.view_id) != removed_ids.end();
                     }),
      drawing->annotations.end());
  for (auto& sheet : drawing->sheets) {
    for (const auto& removed_id : removed_ids) {
      sheet.view_ids.erase(
          std::remove(sheet.view_ids.begin(), sheet.view_ids.end(), removed_id),
          sheet.view_ids.end());
    }
  }
  if (document_->drawing.selected_view_id == view_id) {
    document_->drawing.selected_view_id.reset();
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_view_move(
    const std::string& drawing_id, const std::string& view_id,
    const std::array<double, 2>& sheet_position) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  DrawingView* view = find_view(*drawing, view_id);
  if (view == nullptr) {
    throw std::runtime_error("Unknown view: " + view_id);
  }
  push_undo_state();
  clear_redo_stack();
  view->sheet_position = sheet_position;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_section_update(
    const std::string& drawing_id, const std::string& view_id,
    const SectionDefinition& section) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  DrawingView* view = find_view(*drawing, view_id);
  if (view == nullptr) {
    throw std::runtime_error("Unknown view: " + view_id);
  }
  if (view->kind != "section") {
    throw std::runtime_error("drawing_section_update needs a section view");
  }
  DrawingView patched = *view;
  patched.section = section;
  validate_view_definition(patched, *drawing);
  push_undo_state();
  clear_redo_stack();
  view->section = section;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_dimension_create(
    const std::string& drawing_id, const std::string& view_id,
    const std::string& dim_type, const std::array<double, 2>& pick,
    const std::optional<std::array<double, 2>>& pick_2,
    const std::optional<std::string>& annotation_id) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  DrawingView* view = find_view(*drawing, view_id);
  if (view == nullptr) {
    throw std::runtime_error("Unknown view: " + view_id);
  }
  if (view->kind == "detail") {
    throw std::runtime_error("Dimension the parent view instead — detail "
                             "views do not carry dimensions.");
  }
  if (dim_type != "linear" && dim_type != "angular" && dim_type != "radius" &&
      dim_type != "diameter") {
    throw std::runtime_error("Unknown dimension kind: " + dim_type);
  }
  const ProjectionResult* projection =
      drawing_runtime::cached_projection(document_.value(), view_id);
  if (projection == nullptr) {
    throw std::runtime_error(
        "The view has no projection yet — create the view first.");
  }
  Annotation built;
  built.kind = dim_type;
  built.view_id = view_id;
  if (annotation_id.has_value()) {
    auto found = std::find_if(
        drawing->annotations.begin(), drawing->annotations.end(),
        [&](const Annotation& a) {
          return a.annotation_id == annotation_id.value();
        });
    if (found == drawing->annotations.end()) {
      throw std::runtime_error("Unknown annotation: " + annotation_id.value());
    }
    if (found->view_id != view_id) {
      throw std::runtime_error("The annotation belongs to a different view.");
    }
    built = *found;
    built.kind = dim_type;
    built.witness_2.reset();
    built.dependency_broken = false;
    built.warning.clear();
  } else if (dim_type == "diameter") {
    built.prefix = "\xE2\x8C\x80";
  } else if (dim_type == "radius") {
    built.prefix = "R";
  }
  std::string error;
  std::vector<const ProjectedEdgeRecord*> records;
  const auto measured = measure_from_picks(
      *projection, *view, dim_type, pick, pick_2,
      /*pick_tolerance_sheetmm=*/5.0, document_->drawing.decimal_separator,
      &built, &error, &records);
  if (!measured.has_value()) {
    throw std::runtime_error(error);
  }
  if (records.empty()) {
    throw std::runtime_error("The pick resolved to no edge.");
  }
  if (!witness_from_record(*records[0], &built.witness, &error)) {
    throw std::runtime_error(error);
  }
  if (records.size() > 1) {
    SourceEdgeWitness second;
    if (!witness_from_record(*records[1], &second, &error)) {
      throw std::runtime_error(error);
    }
    built.witness_2 = second;
  }
  push_undo_state();
  clear_redo_stack();
  built.source_edge_id =
      "drawing-edge-" + std::to_string(next_drawing_edge_ref_id_++);
  if (annotation_id.has_value()) {
    auto found = std::find_if(
        drawing->annotations.begin(), drawing->annotations.end(),
        [&](const Annotation& a) {
          return a.annotation_id == annotation_id.value();
        });
    *found = std::move(built);
    document_->drawing.selected_annotation_id = annotation_id;
  } else {
    built.annotation_id =
        "drawing-annotation-" + std::to_string(next_drawing_annotation_id_++);
    document_->drawing.selected_annotation_id = built.annotation_id;
    drawing->annotations.push_back(std::move(built));
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_dimension_update(
    const std::string& drawing_id, const std::string& annotation_id,
    const std::optional<std::string>& text_override,
    const std::optional<std::string>& prefix,
    const std::optional<bool>& arrow_flip,
    const std::optional<std::array<double, 2>>& text_offset) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  auto found = std::find_if(
      drawing->annotations.begin(), drawing->annotations.end(),
      [&](const Annotation& a) { return a.annotation_id == annotation_id; });
  if (found == drawing->annotations.end()) {
    throw std::runtime_error("Unknown annotation: " + annotation_id);
  }
  push_undo_state();
  clear_redo_stack();
  if (text_override.has_value()) {
    found->text_override = text_override.value().empty()
                               ? std::optional<std::string>()
                               : text_override;
  }
  if (prefix.has_value()) {
    found->prefix = prefix.value();
  }
  if (arrow_flip.has_value()) {
    found->arrow_flip = arrow_flip.value();
  }
  if (text_offset.has_value()) {
    found->text_offset = text_offset;
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_dimension_delete(
    const std::string& drawing_id, const std::string& annotation_id) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  auto found = std::find_if(
      drawing->annotations.begin(), drawing->annotations.end(),
      [&](const Annotation& a) { return a.annotation_id == annotation_id; });
  if (found == drawing->annotations.end()) {
    throw std::runtime_error("Unknown annotation: " + annotation_id);
  }
  push_undo_state();
  clear_redo_stack();
  drawing_runtime::erase_dimension(document_->id, annotation_id);
  if (document_->drawing.selected_annotation_id == annotation_id) {
    document_->drawing.selected_annotation_id.reset();
  }
  drawing->annotations.erase(found);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_note_create(
    const std::string& drawing_id, const std::string& sheet_id,
    const std::string& text, const std::array<double, 2>& position,
    const std::optional<double>& height_mm,
    const std::optional<double>& angle_deg,
    const std::optional<std::string>& h_align) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  if (find_sheet(*drawing, sheet_id) == nullptr) {
    throw std::runtime_error("Unknown sheet: " + sheet_id);
  }
  if (height_mm.has_value() && !(height_mm.value() > 0.0)) {
    throw std::runtime_error("The note height must be positive");
  }
  push_undo_state();
  clear_redo_stack();
  SheetNote note;
  note.note_id = "drawing-note-" + std::to_string(next_drawing_note_id_++);
  note.sheet_id = sheet_id;
  note.text = text;
  note.position = {std::round(position[0] * 100.0) / 100.0,
                   std::round(position[1] * 100.0) / 100.0};
  note.height_mm = height_mm.value_or(3.5);
  note.angle_deg = angle_deg.value_or(0.0);
  note.h_align = h_align.value_or("center");
  drawing->notes.push_back(std::move(note));
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_note_update(
    const std::string& drawing_id, const std::string& note_id,
    const std::optional<std::string>& text,
    const std::optional<std::array<double, 2>>& position,
    const std::optional<double>& height_mm,
    const std::optional<double>& angle_deg,
    const std::optional<std::string>& h_align) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  auto found = std::find_if(
      drawing->notes.begin(), drawing->notes.end(),
      [&](const SheetNote& n) { return n.note_id == note_id; });
  if (found == drawing->notes.end()) {
    throw std::runtime_error("Unknown note: " + note_id);
  }
  if (height_mm.has_value() && !(height_mm.value() > 0.0)) {
    throw std::runtime_error("The note height must be positive");
  }
  push_undo_state();
  clear_redo_stack();
  if (text.has_value()) {
    found->text = text.value();
  }
  if (position.has_value()) {
    found->position = {std::round(position.value()[0] * 100.0) / 100.0,
                       std::round(position.value()[1] * 100.0) / 100.0};
  }
  if (height_mm.has_value()) {
    found->height_mm = height_mm.value();
  }
  if (angle_deg.has_value()) {
    found->angle_deg = angle_deg.value();
  }
  if (h_align.has_value()) {
    found->h_align = h_align.value();
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_note_delete(
    const std::string& drawing_id, const std::string& note_id) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  auto found = std::find_if(
      drawing->notes.begin(), drawing->notes.end(),
      [&](const SheetNote& n) { return n.note_id == note_id; });
  if (found == drawing->notes.end()) {
    throw std::runtime_error("Unknown note: " + note_id);
  }
  push_undo_state();
  clear_redo_stack();
  drawing->notes.erase(found);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_annotation_create(
    const std::string& drawing_id, const std::string& view_id,
    const std::string& kind, const std::array<double, 2>& pick,
    const std::optional<std::array<double, 2>>& pick_2,
    const std::optional<std::string>& text_override,
    const std::optional<std::string>& prefix,
    const std::vector<AnnotationExtension>& extensions,
    const std::optional<std::array<double, 2>>& placement) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  DrawingView* view = find_view(*drawing, view_id);
  if (view == nullptr) {
    throw std::runtime_error("Unknown view: " + view_id);
  }
  if (view->kind == "detail") {
    throw std::runtime_error("Annotate the parent view instead — detail "
                             "views do not carry annotations.");
  }
  if (!is_annotation_kind(kind)) {
    throw std::runtime_error("Unknown annotation kind: " + kind);
  }
  const ProjectionResult* projection =
      drawing_runtime::cached_projection(document_.value(), view_id);
  if (projection == nullptr) {
    throw std::runtime_error(
        "The view has no projection yet — create the view first.");
  }
  std::string error;
  std::vector<const ProjectedEdgeRecord*> records;
  std::optional<double> attach_param;
  const auto resolved = resolve_annotation_picks(
      *projection, *view, kind, pick, pick_2,
      /*pick_tolerance_sheetmm=*/5.0, text_override, prefix.value_or(""), &error,
      &records, &attach_param);
  if (!resolved.has_value()) {
    throw std::runtime_error(error);
  }
  if (records.empty()) {
    throw std::runtime_error("The pick resolved to no edge.");
  }
  Annotation built;
  built.kind = kind;
  built.view_id = view_id;
  built.text_override = text_override;
  built.prefix = prefix.value_or("");
  built.extensions = extensions;
  built.attach_param = attach_param;
  if (!witness_from_record(*records[0], &built.witness, &error)) {
    throw std::runtime_error(error);
  }
  if (records.size() > 1) {
    SourceEdgeWitness second;
    if (!witness_from_record(*records[1], &second, &error)) {
      throw std::runtime_error(error);
    }
    built.witness_2 = second;
  }
  if (placement.has_value()) {
    Annotation probe = built;
    probe.text_offset.reset();
    AnnotationGraphics probe_graphics = build_annotation_graphics(
        resolved.value(), probe, *view, /*stale=*/false);
    if (probe_graphics.text.has_value()) {
      built.text_offset =
          std::array<double, 2>{placement.value()[0] -
                                    probe_graphics.text->position[0],
                                placement.value()[1] -
                                    probe_graphics.text->position[1]};
    }
  }
  push_undo_state();
  clear_redo_stack();
  built.source_edge_id =
      "drawing-edge-" + std::to_string(next_drawing_edge_ref_id_++);
  built.annotation_id =
      "drawing-annotation-" + std::to_string(next_drawing_annotation_id_++);
  document_->drawing.selected_annotation_id = built.annotation_id;
  drawing->annotations.push_back(std::move(built));
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_annotation_update(
    const std::string& drawing_id, const std::string& annotation_id,
    const std::optional<std::string>& text_override,
    const std::optional<std::string>& prefix,
    const std::optional<bool>& arrow_flip,
    const std::optional<std::array<double, 2>>& text_offset,
    const std::optional<std::vector<AnnotationExtension>>& extensions,
    const std::optional<double>& attach_param) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  auto found = std::find_if(
      drawing->annotations.begin(), drawing->annotations.end(),
      [&](const Annotation& a) { return a.annotation_id == annotation_id; });
  if (found == drawing->annotations.end()) {
    throw std::runtime_error("Unknown annotation: " + annotation_id);
  }
  push_undo_state();
  clear_redo_stack();
  if (text_override.has_value()) {
    found->text_override = text_override.value().empty()
                               ? std::optional<std::string>()
                               : text_override;
  }
  if (prefix.has_value()) {
    found->prefix = prefix.value();
  }
  if (arrow_flip.has_value()) {
    found->arrow_flip = arrow_flip.value();
  }
  if (text_offset.has_value()) {
    found->text_offset = text_offset;
  }
  if (extensions.has_value()) {
    found->extensions = extensions.value();
  }
  if (attach_param.has_value()) {
    found->attach_param = attach_param;
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_annotation_delete(
    const std::string& drawing_id, const std::string& annotation_id) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  auto found = std::find_if(
      drawing->annotations.begin(), drawing->annotations.end(),
      [&](const Annotation& a) { return a.annotation_id == annotation_id; });
  if (found == drawing->annotations.end()) {
    throw std::runtime_error("Unknown annotation: " + annotation_id);
  }
  push_undo_state();
  clear_redo_stack();
  if (is_annotation_kind(found->kind)) {
    drawing_runtime::erase_attachment(document_->id, annotation_id);
  } else {
    drawing_runtime::erase_dimension(document_->id, annotation_id);
  }
  if (document_->drawing.selected_annotation_id == annotation_id) {
    document_->drawing.selected_annotation_id.reset();
  }
  drawing->annotations.erase(found);
  bump_geometry_revision();
  return document_.value();
}

}  // namespace polysmith::core
