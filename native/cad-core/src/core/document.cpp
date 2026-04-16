#include "core/document.h"

#include <algorithm>

namespace polysmith::core {
namespace {

bool is_origin_plane_reference(const std::string& reference_id) {
  return reference_id == "ref-plane-xy" || reference_id == "ref-plane-yz" ||
         reference_id == "ref-plane-xz";
}

bool is_supported_sketch_tool(const std::string& tool) {
  return tool == "select" || tool == "line" || tool == "rectangle" ||
         tool == "circle";
}

}  // namespace

void DocumentManager::require_document() const {
  if (!document_.has_value()) {
    throw std::runtime_error("No active document");
  }
}

void DocumentManager::push_undo_state() {
  require_document();
  undo_stack_.push_back(document_.value());
}

void DocumentManager::clear_redo_stack() {
  redo_stack_.clear();
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
      .sketch_parameters = std::nullopt,
  };
}

DocumentState DocumentManager::create_document() {
  DocumentState document{
      .id = "doc-" + std::to_string(next_document_id_++),
      .name = "Untitled Part",
      .units = "mm",
      .revision = 1,
      .selected_feature_id = std::nullopt,
      .selected_reference_id = std::nullopt,
      .active_sketch_plane_id = std::nullopt,
      .active_sketch_feature_id = std::nullopt,
      .active_sketch_tool = std::nullopt,
      .selected_sketch_entity_id = std::nullopt,
      .feature_history = {make_root_feature()},
  };

  document_ = document;
  document_count_ = 1;
  undo_stack_.clear();
  redo_stack_.clear();
  return document;
}

DocumentState DocumentManager::add_box_feature(
    const BoxFeatureParameters& parameters) {
  require_document();
  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(
      create_box_feature(next_feature_id_++, parameters));
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::add_cylinder_feature(
    const CylinderFeatureParameters& parameters) {
  require_document();
  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(
      create_cylinder_feature(next_feature_id_++, parameters));
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::update_box_feature(
    const std::string& feature_id, const BoxFeatureParameters& parameters) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::update_box_feature(*feature_it, parameters);
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::rename_feature(const std::string& feature_id,
                                              const std::string& name) {
  require_document();

  if (name.empty()) {
    throw std::runtime_error("Feature name cannot be empty");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  push_undo_state();
  clear_redo_stack();
  feature_it->name = name;
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::delete_feature(const std::string& feature_id) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  if (feature_it->kind == "root_part") {
    throw std::runtime_error("The root feature cannot be deleted");
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
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::undo() {
  require_document();

  if (undo_stack_.empty()) {
    throw std::runtime_error("Nothing to undo");
  }

  redo_stack_.push_back(document_.value());
  document_ = undo_stack_.back();
  undo_stack_.pop_back();
  return document_.value();
}

DocumentState DocumentManager::redo() {
  require_document();

  if (redo_stack_.empty()) {
    throw std::runtime_error("Nothing to redo");
  }

  undo_stack_.push_back(document_.value());
  document_ = redo_stack_.back();
  redo_stack_.pop_back();
  return document_.value();
}

DocumentState DocumentManager::select_feature(const std::string& feature_id) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  return document_.value();
}

DocumentState DocumentManager::select_reference(const std::string& reference_id) {
  require_document();

  if (!is_origin_plane_reference(reference_id)) {
    throw std::runtime_error("Reference not found: " + reference_id);
  }

  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = reference_id;
  document_->selected_sketch_entity_id = std::nullopt;
  return document_.value();
}

DocumentState DocumentManager::start_sketch_on_plane(
    const std::string& reference_id) {
  require_document();

  if (!is_origin_plane_reference(reference_id)) {
    throw std::runtime_error("Sketch plane not found: " + reference_id);
  }

  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(
      create_sketch_feature(next_feature_id_++, reference_id));
  const std::string sketch_feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = reference_id;
  document_->active_sketch_plane_id = reference_id;
  document_->active_sketch_feature_id = sketch_feature_id;
  document_->active_sketch_tool = "select";
  document_->selected_sketch_entity_id = std::nullopt;
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::set_sketch_tool(const std::string& tool) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  if (!is_supported_sketch_tool(tool)) {
    throw std::runtime_error("Unsupported sketch tool: " + tool);
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  polysmith::core::set_sketch_tool(*feature_it, tool);
  document_->active_sketch_tool = tool;
  document_->selected_sketch_entity_id = std::nullopt;
  return document_.value();
}

DocumentState DocumentManager::add_sketch_line(double start_x,
                                               double start_y,
                                               double end_x,
                                               double end_y) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::add_sketch_line(
      *feature_it, next_sketch_line_id_++, start_x, start_y, end_x, end_y);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->lines.back().id;
  document_->active_sketch_tool = "line";
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::add_sketch_rectangle(double start_x,
                                                    double start_y,
                                                    double end_x,
                                                    double end_y) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::add_sketch_rectangle(
      *feature_it, next_sketch_line_id_, start_x, start_y, end_x, end_y);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->lines.back().id;
  document_->active_sketch_tool = "rectangle";
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::add_sketch_circle(double center_x,
                                                 double center_y,
                                                 double radius) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::add_sketch_circle(
      *feature_it, next_sketch_circle_id_++, center_x, center_y, radius);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->circles.back().id;
  document_->active_sketch_tool = "circle";
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::select_sketch_entity(const std::string& entity_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end() ||
      !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  const bool has_line = std::any_of(
      feature_it->sketch_parameters->lines.begin(),
      feature_it->sketch_parameters->lines.end(),
      [&](const SketchLine& line) { return line.id == entity_id; });
  const bool has_circle = std::any_of(
      feature_it->sketch_parameters->circles.begin(),
      feature_it->sketch_parameters->circles.end(),
      [&](const SketchCircle& circle) { return circle.id == entity_id; });

  if (!has_line && !has_circle) {
    throw std::runtime_error("Sketch entity not found: " + entity_id);
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_entity_id = entity_id;
  return document_.value();
}

DocumentState DocumentManager::finish_sketch() {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch to finish");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  feature_it->status = "healthy";
  document_->selected_feature_id = feature_it->id;
  document_->selected_reference_id = std::nullopt;
  document_->active_sketch_plane_id = std::nullopt;
  document_->active_sketch_feature_id = std::nullopt;
  document_->active_sketch_tool = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::clear_selection() {
  require_document();

  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  return document_.value();
}

std::optional<DocumentState> DocumentManager::get_document() const {
  return document_;
}

SessionState DocumentManager::get_session_state() const {
  return SessionState{
      .document_count = document_count_,
      .active_document_id =
          document_.has_value() ? std::make_optional(document_->id) : std::nullopt,
      .can_undo = !undo_stack_.empty(),
      .can_redo = !redo_stack_.empty(),
  };
}

}  // namespace polysmith::core
