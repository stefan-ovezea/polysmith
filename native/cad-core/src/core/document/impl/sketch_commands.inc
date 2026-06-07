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
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
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
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
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
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = line_id;
  document_->selected_sketch_dimension_id = "dim-line-" + line_id;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sketch_point(const std::string& point_id,
                                                   double x,
                                                   double y) {
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
  polysmith::core::update_sketch_point(*feature_it, point_id, x, y);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = point_id;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
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
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  // Deselect after constraint op — the last-updated line may not be
  // the one the user cares about.
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::clear_sketch_line_constraints(
    const std::string& line_id) {
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
  polysmith::core::clear_sketch_line_constraints(*feature_it, line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  // Deselect after constraint op — the last-updated line may not be
  // the one the user cares about.
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_line_construction(
    const std::string& line_id,
    bool is_construction) {
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
  polysmith::core::set_sketch_line_construction(
      *feature_it, line_id, is_construction);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  // Deselect after constraint op — the last-updated line may not be
  // the one the user cares about.
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_midpoint_anchor(
    const std::string& point_id,
    const std::string& host_line_id) {
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
  polysmith::core::set_sketch_midpoint_anchor(
      *feature_it, point_id, host_line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_point_line_anchor(
    const std::string& point_id,
    const std::string& host_line_id,
    double t) {
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
  polysmith::core::set_sketch_point_line_anchor(
      *feature_it, point_id, host_line_id, t);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  bump_geometry_revision();
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
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  // Deselect after constraint op — the last-updated line may not be
  // the one the user cares about.
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
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
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  // Deselect after constraint op — the last-updated line may not be
  // the one the user cares about.
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

// Tiny helper used by all five mirror_preview wrappers — they
// all need to find the active sketch feature and bail out if
// there isn't one. Returns a non-const iterator into
// `feature_history` so the caller can mutate it.
namespace {
std::vector<FeatureEntry>::iterator require_active_sketch(
    DocumentState& document) {
  if (!document.active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }
  const auto& sketch_id = *document.active_sketch_feature_id;
  const auto feature_it = std::find_if(
      document.feature_history.begin(),
      document.feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == sketch_id; });
  if (feature_it == document.feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }
  return feature_it;
}
}  // namespace

DocumentState DocumentManager::start_mirror_preview() {
  require_document();
  const auto feature_it = require_active_sketch(*document_);
  // No undo push — preview state is transient and we don't want
  // it to clutter the undo history.
  polysmith::core::start_mirror_preview(*feature_it);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_mirror_preview_axis(
    const std::string& axis_line_id) {
  require_document();
  const auto feature_it = require_active_sketch(*document_);
  polysmith::core::update_mirror_preview_axis(*feature_it, axis_line_id);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_mirror_preview_objects(
    const std::vector<std::string>& object_ids) {
  require_document();
  const auto feature_it = require_active_sketch(*document_);
  polysmith::core::update_mirror_preview_objects(*feature_it, object_ids);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::commit_mirror_preview(bool persistent) {
  require_document();
  const auto feature_it = require_active_sketch(*document_);
  // Commit is the only mirror_preview op that actually changes
  // permanent geometry, so it's the one that pushes an undo
  // state. Cancel/start/update don't write anything that needs
  // rolling back beyond the in-memory pending struct.
  push_undo_state();
  clear_redo_stack();
  polysmith::core::commit_mirror_preview(
      *feature_it, next_sketch_line_id_, next_sketch_circle_id_, persistent);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::cancel_mirror_preview() {
  require_document();
  const auto feature_it = require_active_sketch(*document_);
  polysmith::core::cancel_mirror_preview(*feature_it);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_tangent_constraint(
    const std::string& line_id, const std::string& circle_id) {
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
  polysmith::core::set_sketch_tangent_constraint(
      *feature_it, line_id, circle_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  bump_geometry_revision();
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
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  // Deselect after constraint op — the last-updated line may not be
  // the one the user cares about.
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
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
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = other_point_id;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::delete_sketch_coincident_constraint(
    const std::string& constraint_id) {
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

  // Route mirror constraint deletes to the mirror-specific handler.
  const std::string mirror_prefix = "mirror-rel-";
  if (constraint_id.size() > mirror_prefix.size() &&
      constraint_id.substr(0, mirror_prefix.size()) == mirror_prefix) {
    polysmith::core::delete_sketch_mirror_relation(*feature_it, constraint_id);
  } else {
    polysmith::core::delete_sketch_coincident_constraint(*feature_it, constraint_id);
  }
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_sketch_point_fixed(const std::string& point_id,
                                                      bool is_fixed) {
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
  polysmith::core::set_sketch_point_fixed(*feature_it, point_id, is_fixed);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = point_id;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
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
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = circle_id;
  document_->selected_sketch_dimension_id = "dim-circle-" + circle_id;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_angle_dimension(
    const std::string& first_line_id,
    const std::string& second_line_id) {
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
  polysmith::core::add_sketch_angle_dimension(
      *feature_it, first_line_id, second_line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dimension) {
        return dimension.kind == "angle" &&
               ((dimension.entity_id == first_line_id &&
                 dimension.secondary_entity_id == second_line_id) ||
                (dimension.entity_id == second_line_id &&
                 dimension.secondary_entity_id == first_line_id));
      });
  if (dimension_it != feature_it->sketch_parameters->dimensions.end()) {
    document_->selected_sketch_entity_id = dimension_it->entity_id;
    document_->selected_sketch_dimension_id = dimension_it->id;
    document_->selected_sketch_entity_ids.clear();
    document_->selected_sketch_point_id = std::nullopt;
    document_->selected_sketch_point_ids.clear();
    document_->selected_sketch_profile_id = std::nullopt;
    document_->selected_sketch_profile_ids.clear();
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_distance_dimension(
    const std::string& first_entity_id,
    const std::string& second_entity_id) {
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
  polysmith::core::add_sketch_distance_dimension(
      *feature_it, first_entity_id, second_entity_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dimension) {
        if (dimension.kind != "line_line_distance" &&
            dimension.kind != "circle_center_distance" &&
            dimension.kind != "circle_line_distance") {
          return false;
        }
        return (dimension.entity_id == first_entity_id &&
                dimension.secondary_entity_id == second_entity_id) ||
               (dimension.entity_id == second_entity_id &&
                dimension.secondary_entity_id == first_entity_id);
      });
  if (dimension_it != feature_it->sketch_parameters->dimensions.end()) {
    document_->selected_sketch_entity_id = dimension_it->entity_id;
    document_->selected_sketch_dimension_id = dimension_it->id;
    document_->selected_sketch_entity_ids.clear();
    document_->selected_sketch_point_id = std::nullopt;
    document_->selected_sketch_point_ids.clear();
    document_->selected_sketch_profile_id = std::nullopt;
    document_->selected_sketch_profile_ids.clear();
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_line_length_dimension(
    const std::string& line_id) {
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
  polysmith::core::add_sketch_line_length_dimension(*feature_it, line_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;

  const std::string dimension_id = "dim-line-" + line_id;
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != feature_it->sketch_parameters->dimensions.end()) {
    document_->selected_sketch_entity_id = dimension_it->entity_id;
    document_->selected_sketch_dimension_id = dimension_it->id;
    document_->selected_sketch_entity_ids.clear();
    document_->selected_sketch_point_id = std::nullopt;
    document_->selected_sketch_point_ids.clear();
    document_->selected_sketch_profile_id = std::nullopt;
    document_->selected_sketch_profile_ids.clear();
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_circle_radius_dimension(
    const std::string& circle_id,
    std::optional<std::string> display_as) {
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
  polysmith::core::add_sketch_circle_radius_dimension(
      *feature_it, circle_id, display_as);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;

  const std::string dimension_id = "dim-circle-" + circle_id;
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != feature_it->sketch_parameters->dimensions.end()) {
    document_->selected_sketch_entity_id = dimension_it->entity_id;
    document_->selected_sketch_dimension_id = dimension_it->id;
    document_->selected_sketch_entity_ids.clear();
    document_->selected_sketch_point_id = std::nullopt;
    document_->selected_sketch_point_ids.clear();
    document_->selected_sketch_profile_id = std::nullopt;
    document_->selected_sketch_profile_ids.clear();
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_polygon_radius_dimension(
    const std::string& polygon_id) {
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
  polysmith::core::add_sketch_polygon_radius_dimension(*feature_it,
                                                        polygon_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;

  const std::string dimension_id = "dim-polygon-" + polygon_id;
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != feature_it->sketch_parameters->dimensions.end()) {
    document_->selected_sketch_entity_id = dimension_it->entity_id;
    document_->selected_sketch_dimension_id = dimension_it->id;
    document_->selected_sketch_entity_ids.clear();
    document_->selected_sketch_point_id = std::nullopt;
    document_->selected_sketch_point_ids.clear();
    document_->selected_sketch_profile_id = std::nullopt;
    document_->selected_sketch_profile_ids.clear();
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::trim_sketch_entity(
    const std::string& entity_id,
    double click_x,
    double click_y) {
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
  polysmith::core::trim_sketch_entity(
      *feature_it, entity_id, click_x, click_y);

  // Refresh derived state: rebuild points, recompute profiles, reify
  // dimensions, and re-sync projection/dependency state.
  polysmith::core::refresh_sketch_derived_state(*feature_it);

  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = entity_id;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_point_distance_dimension(
    const std::string& point_a_id,
    const std::string& point_b_id) {
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
  polysmith::core::add_sketch_point_distance_dimension(
      *feature_it, point_a_id, point_b_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;

  // Select the newly created dimension
  const std::string dimension_id =
      "dim-point-distance-" + point_a_id + "-" + point_b_id;
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != feature_it->sketch_parameters->dimensions.end()) {
    document_->selected_sketch_entity_id = dimension_it->entity_id;
    document_->selected_sketch_dimension_id = dimension_it->id;
    document_->selected_sketch_entity_ids.clear();
    document_->selected_sketch_point_id = std::nullopt;
    document_->selected_sketch_point_ids.clear();
    document_->selected_sketch_profile_id = std::nullopt;
    document_->selected_sketch_profile_ids.clear();
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sketch_dimension(
    const std::string& dimension_id,
    double value,
    std::optional<std::string> expression) {
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
  if (dimension_it->kind == "circle_radius") {
    for (const auto& projection : feature_it->sketch_parameters->projections) {
      if (std::find(projection.generated_circle_ids.begin(),
                    projection.generated_circle_ids.end(),
                    dimension_it->entity_id) !=
          projection.generated_circle_ids.end()) {
        throw std::runtime_error(
            "Projected circle dimensions are driven by their source geometry");
      }
    }
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::update_sketch_dimension(*feature_it, dimension_id, value, expression);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  if (feature_it->sketch_parameters.has_value()) {
    const auto updated_dimension_it = std::find_if(
        feature_it->sketch_parameters->dimensions.begin(),
        feature_it->sketch_parameters->dimensions.end(),
        [&](const SketchDimension& dimension) {
          return dimension.id == dimension_id;
        });

    if (updated_dimension_it != feature_it->sketch_parameters->dimensions.end()) {
      document_->selected_sketch_entity_id = updated_dimension_it->entity_id;
      document_->selected_sketch_dimension_id = updated_dimension_it->id;
    }
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sketch_dimension_label_position(
    const std::string& dimension_id,
    double label_x,
    double label_y) {
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

  auto& dimensions = feature_it->sketch_parameters->dimensions;
  const auto dimension_it = std::find_if(
      dimensions.begin(),
      dimensions.end(),
      [&](const SketchDimension& dimension) {
        return dimension.id == dimension_id;
      });
  if (dimension_it == dimensions.end()) {
    throw std::runtime_error("Sketch dimension not found: " + dimension_id);
  }

  push_undo_state();
  clear_redo_stack();
  dimension_it->label_x = label_x;
  dimension_it->label_y = label_y;
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = dimension_it->entity_id;
  document_->selected_sketch_dimension_id = dimension_it->id;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

namespace {

std::vector<FeatureEntry>::iterator find_sketch_feature_owning_profile(
    std::vector<FeatureEntry>& features, const std::string& profile_id) {
  return std::find_if(
      features.begin(), features.end(), [&](const FeatureEntry& feature) {
        if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
          return false;
        }
        const auto& profiles = feature.sketch_parameters->profiles;
        return std::any_of(
            profiles.begin(), profiles.end(),
            [&](const SketchProfileRegion& profile) {
              return profile.id == profile_id;
            });
      });
}

std::vector<FeatureEntry>::iterator find_sketch_feature_owning_line(
    std::vector<FeatureEntry>& features, const std::string& line_id) {
  return std::find_if(
      features.begin(), features.end(), [&](const FeatureEntry& feature) {
        if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
          return false;
        }
        const auto& lines = feature.sketch_parameters->lines;
        return std::any_of(
            lines.begin(), lines.end(),
            [&](const SketchLine& line) { return line.id == line_id; });
      });
}

SketchLine* find_line_by_id(SketchFeatureParameters& sketch,
                            const std::string& line_id) {
  const auto line_it = std::find_if(
      sketch.lines.begin(),
      sketch.lines.end(),
      [&](const SketchLine& line) { return line.id == line_id; });
  return line_it == sketch.lines.end() ? nullptr : &(*line_it);
}

const SketchLine* find_line_by_id(const SketchFeatureParameters& sketch,
                                  const std::string& line_id) {
  const auto line_it = std::find_if(
      sketch.lines.begin(),
      sketch.lines.end(),
      [&](const SketchLine& line) { return line.id == line_id; });
  return line_it == sketch.lines.end() ? nullptr : &(*line_it);
}

std::vector<FeatureEntry>::iterator find_sketch_feature_owning_path_entity(
    std::vector<FeatureEntry>& features, const std::string& entity_id) {
  return std::find_if(
      features.begin(), features.end(), [&](const FeatureEntry& feature) {
        if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
          return false;
        }
        const auto& sketch = feature.sketch_parameters.value();
        const bool owns_line = std::any_of(
            sketch.lines.begin(), sketch.lines.end(),
            [&](const SketchLine& line) { return line.id == entity_id; });
        const bool owns_arc = std::any_of(
            sketch.arcs.begin(), sketch.arcs.end(),
            [&](const SketchArc& arc) { return arc.id == entity_id; });
        return owns_line || owns_arc;
      });
}

struct SweepPathEntity {
  std::string id;
  std::string kind;
  std::string start_point_id;
  std::string end_point_id;
  const SketchLine* line = nullptr;
  const SketchArc* arc = nullptr;
};

double normalize_arc_sweep(double start_angle, double end_angle, bool ccw) {
  double sweep = end_angle - start_angle;
  if (ccw) {
    while (sweep <= 0.0) {
      sweep += 2.0 * kPi;
    }
  } else {
    while (sweep >= 0.0) {
      sweep -= 2.0 * kPi;
    }
  }
  return sweep;
}

std::vector<SweepPathEntity> order_sweep_path_entities(
    const SketchFeatureParameters& sketch,
    const std::string& seed_entity_id) {
  std::vector<SweepPathEntity> entities;
  for (const auto& line : sketch.lines) {
    if (line.is_construction) {
      continue;
    }
    entities.push_back(SweepPathEntity{.id = line.id,
                                       .kind = "line",
                                       .start_point_id = line.start_point_id,
                                       .end_point_id = line.end_point_id,
                                       .line = &line});
  }
  for (const auto& arc : sketch.arcs) {
    if (arc.is_construction) {
      continue;
    }
    entities.push_back(SweepPathEntity{.id = arc.id,
                                       .kind = "arc",
                                       .start_point_id = arc.start_point_id,
                                       .end_point_id = arc.end_point_id,
                                       .arc = &arc});
  }

  auto seed_it = std::find_if(
      entities.begin(), entities.end(),
      [&](const SweepPathEntity& entity) { return entity.id == seed_entity_id; });
  if (seed_it == entities.end()) {
    throw std::runtime_error("Sweep path entity not found: " + seed_entity_id);
  }
  const size_t seed_index = static_cast<size_t>(
      std::distance(entities.begin(), seed_it));

  std::unordered_map<std::string, std::vector<size_t>> by_point;
  for (size_t index = 0; index < entities.size(); ++index) {
    by_point[entities[index].start_point_id].push_back(index);
    by_point[entities[index].end_point_id].push_back(index);
  }

  std::vector<size_t> stack{seed_index};
  std::unordered_set<size_t> component;
  while (!stack.empty()) {
    const size_t index = stack.back();
    stack.pop_back();
    if (!component.insert(index).second) {
      continue;
    }
    for (const auto& point_id :
         {entities[index].start_point_id, entities[index].end_point_id}) {
      for (const size_t connected : by_point[point_id]) {
        if (!component.count(connected)) {
          stack.push_back(connected);
        }
      }
    }
  }

  for (const size_t index : component) {
    const auto& entity = entities[index];
    if (by_point[entity.start_point_id].size() > 2 ||
        by_point[entity.end_point_id].size() > 2) {
      throw std::runtime_error(
          "Sweep path cannot contain branches. Select a simple connected path.");
    }
  }

  size_t start_index = seed_index;
  std::string current_point = entities[seed_index].start_point_id;
  for (const size_t index : component) {
    const auto& entity = entities[index];
    if (by_point[entity.start_point_id].size() == 1) {
      start_index = index;
      current_point = entity.start_point_id;
      break;
    }
    if (by_point[entity.end_point_id].size() == 1) {
      start_index = index;
      current_point = entity.end_point_id;
      break;
    }
  }

  std::vector<SweepPathEntity> ordered;
  std::unordered_set<size_t> used;
  size_t current_index = start_index;
  while (used.size() < component.size()) {
    const auto& entity = entities[current_index];
    SweepPathEntity oriented = entity;
    if (entity.end_point_id == current_point) {
      std::swap(oriented.start_point_id, oriented.end_point_id);
    }
    ordered.push_back(oriented);
    used.insert(current_index);
    current_point = oriented.end_point_id;

    std::optional<size_t> next_index;
    for (const size_t connected : by_point[current_point]) {
      if (component.count(connected) && !used.count(connected)) {
        next_index = connected;
        break;
      }
    }
    if (!next_index.has_value()) {
      break;
    }
    current_index = next_index.value();
  }

  if (ordered.size() != component.size()) {
    throw std::runtime_error("Sweep path must be a single connected chain.");
  }
  return ordered;
}

std::vector<SweepFeatureParameters::PathSegment> make_sweep_path_segments(
    const SketchFeatureParameters& sketch,
    const std::string& seed_entity_id) {
  const auto ordered = order_sweep_path_entities(sketch, seed_entity_id);
  std::vector<SweepFeatureParameters::PathSegment> segments;
  for (const auto& entity : ordered) {
    if (entity.kind == "line" && entity.line != nullptr) {
      const bool reversed = entity.start_point_id != entity.line->start_point_id;
      const auto start = revolve_sketch_local_to_world(
          sketch, reversed ? entity.line->end_x : entity.line->start_x,
          reversed ? entity.line->end_y : entity.line->start_y);
      const auto end = revolve_sketch_local_to_world(
          sketch, reversed ? entity.line->start_x : entity.line->end_x,
          reversed ? entity.line->start_y : entity.line->end_y);
      segments.push_back(SweepFeatureParameters::PathSegment{
          .entity_id = entity.id,
          .kind = "line",
          .start_x = start[0],
          .start_y = start[1],
          .start_z = start[2],
          .end_x = end[0],
          .end_y = end[1],
          .end_z = end[2],
      });
      continue;
    }
    if (entity.kind == "arc" && entity.arc != nullptr) {
      const bool reversed = entity.start_point_id != entity.arc->start_point_id;
      const bool ccw = reversed ? !entity.arc->ccw : entity.arc->ccw;
      const double start_x = reversed ? entity.arc->end_x : entity.arc->start_x;
      const double start_y = reversed ? entity.arc->end_y : entity.arc->start_y;
      const double end_x = reversed ? entity.arc->start_x : entity.arc->end_x;
      const double end_y = reversed ? entity.arc->start_y : entity.arc->end_y;
      const double start_angle =
          std::atan2(start_y - entity.arc->center_y,
                     start_x - entity.arc->center_x);
      const double end_angle =
          std::atan2(end_y - entity.arc->center_y,
                     end_x - entity.arc->center_x);
      const double mid_angle =
          start_angle + normalize_arc_sweep(start_angle, end_angle, ccw) * 0.5;
      const double mid_x = entity.arc->center_x +
                           entity.arc->radius * std::cos(mid_angle);
      const double mid_y = entity.arc->center_y +
                           entity.arc->radius * std::sin(mid_angle);
      const auto start = revolve_sketch_local_to_world(sketch, start_x, start_y);
      const auto end = revolve_sketch_local_to_world(sketch, end_x, end_y);
      const auto center = revolve_sketch_local_to_world(
          sketch, entity.arc->center_x, entity.arc->center_y);
      const auto mid = revolve_sketch_local_to_world(sketch, mid_x, mid_y);
      segments.push_back(SweepFeatureParameters::PathSegment{
          .entity_id = entity.id,
          .kind = "arc",
          .start_x = start[0],
          .start_y = start[1],
          .start_z = start[2],
          .end_x = end[0],
          .end_y = end[1],
          .end_z = end[2],
          .center_x = center[0],
          .center_y = center[1],
          .center_z = center[2],
          .mid_x = mid[0],
          .mid_y = mid[1],
          .mid_z = mid[2],
          .radius = entity.arc->radius,
          .ccw = ccw,
      });
    }
  }
  if (segments.empty()) {
    throw std::runtime_error("Sweep path must contain at least one segment.");
  }
  return segments;
}

LoftFeatureParameters make_loft_parameters_for_profiles(
    std::vector<FeatureEntry>& features,
    const std::vector<std::string>& profile_ids,
    bool ruled) {
  if (profile_ids.size() < 2) {
    throw std::runtime_error("Loft requires at least two sketch profiles");
  }

  LoftFeatureParameters parameters{};
  parameters.ruled = ruled;
  for (const auto& profile_id : profile_ids) {
    const auto sketch_it = find_sketch_feature_owning_profile(features, profile_id);
    if (sketch_it == features.end()) {
      throw std::runtime_error("Sketch profile not found: " + profile_id);
    }
    const auto& sketch = sketch_it->sketch_parameters.value();
    const auto profile_it = std::find_if(
        sketch.profiles.begin(), sketch.profiles.end(),
        [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });
    if (profile_it == sketch.profiles.end()) {
      throw std::runtime_error("Sketch profile not found: " + profile_id);
    }
    if (!profile_it->inner_loops.empty()) {
      throw std::runtime_error("Loft does not support profiles with holes yet");
    }
    const auto section = make_loft_section_for_profile(*sketch_it, *profile_it);
    if (!section.has_value()) {
      throw std::runtime_error("Selected profile is not supported for loft");
    }
    parameters.sections.push_back(section.value());
  }

  return parameters;
}

RevolveFeatureParameters make_revolve_parameters(
    std::vector<FeatureEntry>& features,
    const std::string& profile_id,
    const std::string& axis_entity_id,
    double angle_degrees) {
  const auto profile_sketch_it =
      find_sketch_feature_owning_profile(features, profile_id);
  if (profile_sketch_it == features.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }
  auto& profile_sketch = profile_sketch_it->sketch_parameters.value();
  const auto profile_it = std::find_if(
      profile_sketch.profiles.begin(),
      profile_sketch.profiles.end(),
      [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });
  if (profile_it == profile_sketch.profiles.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }

  const auto axis_sketch_it =
      find_sketch_feature_owning_line(features, axis_entity_id);
  if (axis_sketch_it == features.end()) {
    throw std::runtime_error("Revolve axis line not found: " + axis_entity_id);
  }
  const SketchLine* axis_line =
      find_line_by_id(axis_sketch_it->sketch_parameters.value(), axis_entity_id);
  if (axis_line == nullptr) {
    throw std::runtime_error("Revolve axis line not found: " + axis_entity_id);
  }

  const auto parameters =
      make_revolve_parameters_for_profile(*profile_sketch_it,
                                          *profile_it,
                                          *axis_sketch_it,
                                          *axis_line,
                                          angle_degrees);
  if (!parameters.has_value()) {
    throw std::runtime_error("Selected profile or axis is not supported for revolve");
  }
  return parameters.value();
}

SweepFeatureParameters make_sweep_parameters(
    std::vector<FeatureEntry>& features,
    const std::string& profile_id,
    const std::string& path_entity_id) {
  const auto profile_sketch_it =
      find_sketch_feature_owning_profile(features, profile_id);
  if (profile_sketch_it == features.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }
  const auto& profile_sketch = profile_sketch_it->sketch_parameters.value();
  const auto profile_it = std::find_if(
      profile_sketch.profiles.begin(),
      profile_sketch.profiles.end(),
      [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });
  if (profile_it == profile_sketch.profiles.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }

  const auto path_sketch_it =
      find_sketch_feature_owning_path_entity(features, path_entity_id);
  if (path_sketch_it == features.end()) {
    throw std::runtime_error("Sweep path entity not found: " + path_entity_id);
  }

  const std::string plane_id =
      profile_sketch.plane_frame.has_value()
          ? plane_id_from_frame(profile_sketch.plane_frame.value())
          : profile_sketch.plane_id;
  const std::optional<PlaneFrame> plane_frame =
      profile_sketch.plane_frame.has_value()
          ? std::optional<PlaneFrame>(
                make_plane_frame(profile_sketch.plane_frame.value()))
          : std::nullopt;
  const auto path_segments = make_sweep_path_segments(
      path_sketch_it->sketch_parameters.value(), path_entity_id);
  const auto& first_segment = path_segments.front();
  const auto& last_segment = path_segments.back();

  return SweepFeatureParameters{
      .sketch_feature_id = profile_sketch_it->id,
      .profile_id = profile_it->id,
      .plane_id = plane_id,
      .plane_frame = plane_frame,
      .profile_kind = profile_it->kind,
      .profile_points = profile_it->kind == "circle"
                            ? sample_circle_profile_points(*profile_it)
                            : profile_it->points,
      .inner_loops = profile_it->inner_loops,
      .path_sketch_feature_id = path_sketch_it->id,
      .path_entity_id = path_entity_id,
      .path_start_x = first_segment.start_x,
      .path_start_y = first_segment.start_y,
      .path_start_z = first_segment.start_z,
      .path_end_x = last_segment.end_x,
      .path_end_y = last_segment.end_y,
      .path_end_z = last_segment.end_z,
      .path_segments = path_segments,
  };
}

}  // namespace
