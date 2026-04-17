#include "core/document.h"

#include <algorithm>
#include <cmath>

namespace polysmith::core {
namespace {

bool is_origin_plane_reference(const std::string& reference_id) {
  return reference_id == "ref-plane-xy" || reference_id == "ref-plane-yz" ||
         reference_id == "ref-plane-xz";
}

std::string face_owner_id(const std::string& face_id) {
  const auto separator = face_id.find(":face:");
  if (separator == std::string::npos) {
    return "";
  }

  return face_id.substr(0, separator);
}

bool is_supported_sketch_tool(const std::string& tool) {
  return tool == "select" || tool == "line" || tool == "rectangle" ||
         tool == "circle";
}

std::string plane_id_from_frame(
    const SketchFeatureParameters::SketchPlaneFrame& plane_frame) {
  const double abs_x = std::abs(plane_frame.normal_x);
  const double abs_y = std::abs(plane_frame.normal_y);
  const double abs_z = std::abs(plane_frame.normal_z);

  if (abs_x >= abs_y && abs_x >= abs_z) {
    return "ref-plane-yz";
  }

  if (abs_y >= abs_x && abs_y >= abs_z) {
    return "ref-plane-xy";
  }

  return "ref-plane-xz";
}

PlaneFrame make_plane_frame(
    const SketchFeatureParameters::SketchPlaneFrame& plane_frame) {
  return PlaneFrame{
      .origin_x = plane_frame.origin_x,
      .origin_y = plane_frame.origin_y,
      .origin_z = plane_frame.origin_z,
      .x_axis_x = plane_frame.x_axis_x,
      .x_axis_y = plane_frame.x_axis_y,
      .x_axis_z = plane_frame.x_axis_z,
      .y_axis_x = plane_frame.y_axis_x,
      .y_axis_y = plane_frame.y_axis_y,
      .y_axis_z = plane_frame.y_axis_z,
      .normal_x = plane_frame.normal_x,
      .normal_y = plane_frame.normal_y,
      .normal_z = plane_frame.normal_z,
  };
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
      .extrude_parameters = std::nullopt,
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
      .selected_face_id = std::nullopt,
      .active_sketch_plane_id = std::nullopt,
      .active_sketch_face_id = std::nullopt,
      .active_sketch_feature_id = std::nullopt,
      .active_sketch_tool = std::nullopt,
      .selected_sketch_entity_id = std::nullopt,
      .selected_sketch_dimension_id = std::nullopt,
      .selected_sketch_profile_id = std::nullopt,
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
  document_->selected_face_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  return document_.value();
}

DocumentState DocumentManager::select_reference(const std::string& reference_id) {
  require_document();

  if (!is_origin_plane_reference(reference_id)) {
    throw std::runtime_error("Reference not found: " + reference_id);
  }

  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = reference_id;
  document_->selected_face_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
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
  document_->selected_face_id = std::nullopt;
  document_->active_sketch_plane_id = reference_id;
  document_->active_sketch_face_id = std::nullopt;
  document_->active_sketch_feature_id = sketch_feature_id;
  document_->active_sketch_tool = "select";
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::select_face(const std::string& face_id) {
  require_document();

  const std::string owner_id = face_owner_id(face_id);
  if (owner_id.empty()) {
    throw std::runtime_error("Face not found: " + face_id);
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == owner_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Face owner not found: " + face_id);
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = face_id;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  return document_.value();
}

DocumentState DocumentManager::start_sketch_on_face(
    const std::string& face_id,
    const SketchFeatureParameters::SketchPlaneFrame& plane_frame) {
  require_document();

  const std::string owner_id = face_owner_id(face_id);
  if (owner_id.empty()) {
    throw std::runtime_error("Sketch face not found: " + face_id);
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == owner_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Sketch face owner not found: " + face_id);
  }

  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(
      create_sketch_feature(next_feature_id_++, face_id, plane_frame));
  const std::string sketch_feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = face_id;
  document_->active_sketch_plane_id = face_id;
  document_->active_sketch_face_id = face_id;
  document_->active_sketch_feature_id = sketch_feature_id;
  document_->active_sketch_tool = "select";
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
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
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  return document_.value();
}

DocumentState DocumentManager::update_sketch_line(const std::string& line_id,
                                                  double start_x,
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
  polysmith::core::update_sketch_line(
      *feature_it, line_id, start_x, start_y, end_x, end_y);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_entity_id = line_id;
  document_->selected_sketch_dimension_id = "dim-line-" + line_id;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::set_sketch_line_constraint(
    const std::string& line_id,
    const std::optional<std::string>& constraint) {
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
  polysmith::core::set_sketch_line_constraint(*feature_it, line_id, constraint);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_entity_id = line_id;
  document_->selected_sketch_dimension_id = "dim-line-" + line_id;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::set_sketch_equal_length_constraint(
    const std::string& line_id,
    const std::optional<std::string>& other_line_id) {
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
  polysmith::core::set_sketch_equal_length_constraint(
      *feature_it, line_id, other_line_id);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_entity_id = line_id;
  document_->selected_sketch_dimension_id = "dim-line-" + line_id;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::set_sketch_perpendicular_constraint(
    const std::string& line_id,
    const std::optional<std::string>& other_line_id) {
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
  polysmith::core::set_sketch_perpendicular_constraint(
      *feature_it, line_id, other_line_id);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_entity_id = line_id;
  document_->selected_sketch_dimension_id = "dim-line-" + line_id;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::set_sketch_parallel_constraint(
    const std::string& line_id,
    const std::optional<std::string>& other_line_id) {
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
  polysmith::core::set_sketch_parallel_constraint(
      *feature_it, line_id, other_line_id);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_entity_id = line_id;
  document_->selected_sketch_dimension_id = "dim-line-" + line_id;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::set_sketch_coincident_constraint(
    const std::string& point_id, const std::string& other_point_id) {
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
  polysmith::core::set_sketch_coincident_constraint(
      *feature_it, point_id, other_point_id);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::update_sketch_circle(const std::string& circle_id,
                                                    double center_x,
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
  polysmith::core::update_sketch_circle(
      *feature_it, circle_id, center_x, center_y, radius);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_entity_id = circle_id;
  document_->selected_sketch_dimension_id = "dim-circle-" + circle_id;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::update_sketch_dimension(
    const std::string& dimension_id, double value) {
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
  polysmith::core::update_sketch_dimension(*feature_it, dimension_id, value);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  if (feature_it->sketch_parameters.has_value()) {
    const auto dimension_it = std::find_if(
        feature_it->sketch_parameters->dimensions.begin(),
        feature_it->sketch_parameters->dimensions.end(),
        [&](const SketchDimension& dimension) {
          return dimension.id == dimension_id;
        });

    if (dimension_it != feature_it->sketch_parameters->dimensions.end()) {
      document_->selected_sketch_entity_id = dimension_it->entity_id;
      document_->selected_sketch_dimension_id = dimension_it->id;
    }
  }
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::select_sketch_profile(const std::string& profile_id) {
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

  const auto profiles = polysmith::core::detect_sketch_profiles(*feature_it);
  const bool has_polygon = std::any_of(
      profiles.polygons.begin(),
      profiles.polygons.end(),
      [&](const PolygonSketchProfile& profile) { return profile.id == profile_id; });
  const bool has_circle = std::any_of(
      profiles.circles.begin(),
      profiles.circles.end(),
      [&](const CircleSketchProfile& profile) { return profile.id == profile_id; });

  if (!has_polygon && !has_circle) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = profile_id;
  return document_.value();
}

DocumentState DocumentManager::extrude_profile(const std::string& profile_id,
                                               double depth) {
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

  const auto profiles = polysmith::core::detect_sketch_profiles(*feature_it);
  std::optional<ExtrudeFeatureParameters> extrude_parameters;

  for (const auto& polygon : profiles.polygons) {
    if (polygon.id != profile_id) {
      continue;
    }

    extrude_parameters = ExtrudeFeatureParameters{
        .sketch_feature_id = feature_it->id,
        .profile_id = profile_id,
        .plane_id = feature_it->sketch_parameters->plane_frame.has_value()
                        ? plane_id_from_frame(
                              feature_it->sketch_parameters->plane_frame.value())
                        : polygon.plane_id,
        .plane_frame = polygon.plane_frame.has_value()
                           ? std::optional<PlaneFrame>(
                                 make_plane_frame(polygon.plane_frame.value()))
                           : std::nullopt,
        .profile_kind = "polygon",
        .start_x = 0.0,
        .start_y = 0.0,
        .width = 0.0,
        .height = 0.0,
        .radius = 0.0,
        .profile_points = polygon.points,
        .depth = depth,
    };
    break;
  }

  if (!extrude_parameters.has_value()) {
    for (const auto& circle : profiles.circles) {
      if (circle.id != profile_id) {
        continue;
      }

      extrude_parameters = ExtrudeFeatureParameters{
          .sketch_feature_id = feature_it->id,
          .profile_id = profile_id,
          .plane_id = feature_it->sketch_parameters->plane_frame.has_value()
                          ? plane_id_from_frame(
                                feature_it->sketch_parameters->plane_frame.value())
                          : circle.plane_id,
          .plane_frame = circle.plane_frame.has_value()
                             ? std::optional<PlaneFrame>(
                                   make_plane_frame(circle.plane_frame.value()))
                             : std::nullopt,
          .profile_kind = "circle",
          .start_x = circle.center_x,
          .start_y = circle.center_y,
          .width = 0.0,
          .height = 0.0,
          .radius = circle.radius,
          .profile_points = {},
          .depth = depth,
      };

      if (circle.plane_id != "ref-plane-xy") {
        throw std::runtime_error(
            "Circle extrude currently supports the XY plane only");
      }
      break;
    }
  }

  if (!extrude_parameters.has_value()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }

  push_undo_state();
  clear_redo_stack();
  document_->feature_history.push_back(
      create_extrude_feature(next_feature_id_++, extrude_parameters.value()));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->active_sketch_plane_id = std::nullopt;
  document_->active_sketch_feature_id = std::nullopt;
  document_->active_sketch_tool = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->revision += 1;
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
  document_->selected_sketch_dimension_id =
      feature_it->sketch_parameters->dimensions.back().id;
  document_->selected_sketch_profile_id = std::nullopt;
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
  document_->selected_sketch_dimension_id =
      feature_it->sketch_parameters->dimensions.back().id;
  document_->selected_sketch_profile_id = std::nullopt;
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
  document_->selected_sketch_dimension_id =
      feature_it->sketch_parameters->dimensions.back().id;
  document_->selected_sketch_profile_id = std::nullopt;
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
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dimension) {
        return dimension.entity_id == entity_id;
      });
  document_->selected_sketch_dimension_id =
      dimension_it != feature_it->sketch_parameters->dimensions.end()
          ? std::make_optional(dimension_it->id)
          : std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  return document_.value();
}

DocumentState DocumentManager::select_sketch_dimension(
    const std::string& dimension_id) {
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

  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dimension) { return dimension.id == dimension_id; });

  if (dimension_it == feature_it->sketch_parameters->dimensions.end()) {
    throw std::runtime_error("Sketch dimension not found: " + dimension_id);
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_entity_id = dimension_it->entity_id;
  document_->selected_sketch_dimension_id = dimension_it->id;
  document_->selected_sketch_profile_id = std::nullopt;
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
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->revision += 1;
  return document_.value();
}

DocumentState DocumentManager::clear_selection() {
  require_document();

  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  return document_.value();
}

ExportResult DocumentManager::export_document_as_step(
    const std::string& file_path) const {
  require_document();
  return polysmith::core::export_document_as_step(document_.value(), file_path);
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
