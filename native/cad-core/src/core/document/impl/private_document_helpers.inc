constexpr double kPi = 3.14159265358979323846;

bool is_origin_plane_reference(const std::string& reference_id) {
  return reference_id == "ref-plane-xy" || reference_id == "ref-plane-yz" ||
         reference_id == "ref-plane-xz";
}

int action_count(const DocumentState& document) {
  int count = 0;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "root_part") {
      ++count;
    }
  }
  return count;
}

bool includes_feature_at_cursor(const DocumentState& document,
                                const std::string& feature_id) {
  int remaining_actions =
      document.timeline_cursor.value_or(action_count(document));
  for (const auto& feature : document.feature_history) {
    if (feature.id != feature_id) {
      if (feature.kind != "root_part" && remaining_actions > 0) {
        --remaining_actions;
      }
      continue;
    }
    return feature.kind == "root_part" || remaining_actions > 0;
  }
  return false;
}

void mark_feature_healthy(FeatureEntry& feature) {
  feature.status = "healthy";
  feature.dependency_broken = false;
  feature.dependency_warning.clear();
}

std::string make_move_parameters_summary(const MoveFeatureParameters& params) {
  std::ostringstream stream;
  stream << "Move " << params.translation_x << ", " << params.translation_y
         << ", " << params.translation_z << " mm · rotate "
         << params.rotation_x_degrees << ", " << params.rotation_y_degrees
         << ", " << params.rotation_z_degrees << " deg";
  return stream.str();
}

std::string feature_name_by_id(const DocumentState& document,
                               const std::string& feature_id) {
  const auto it = std::find_if(
      document.feature_history.begin(),
      document.feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  return it == document.feature_history.end() ? std::string{} : it->name;
}

void write_copy_frame(BodyCopyFeatureParameters& params,
                      const BodyLocalFrame& frame) {
  params.local_x_axis_x = frame.x_axis_x;
  params.local_x_axis_y = frame.x_axis_y;
  params.local_x_axis_z = frame.x_axis_z;
  params.local_y_axis_x = frame.y_axis_x;
  params.local_y_axis_y = frame.y_axis_y;
  params.local_y_axis_z = frame.y_axis_z;
  params.local_z_axis_x = frame.z_axis_x;
  params.local_z_axis_y = frame.z_axis_y;
  params.local_z_axis_z = frame.z_axis_z;
}

std::string serialize_shape_snapshot(const TopoDS_Shape& shape) {
  std::ostringstream stream;
  try {
    BRepTools::Write(shape, stream);
  } catch (const Standard_Failure&) {
    throw std::runtime_error("Could not serialize copied body shape");
  }
  return stream.str();
}

void mark_extrude_preview_warning(FeatureEntry& feature,
                                  const std::string& message) {
  feature.status = "warning";
  feature.dependency_broken = true;
  feature.dependency_warning = message;
  feature.parameters_summary = "Preview unavailable";
}

void apply_extrude_parameters_with_preview_validation(
    FeatureEntry& feature,
    const ExtrudeFeatureParameters& parameters) {
  feature.extrude_parameters = parameters;
  try {
    const TopoDS_Shape preview_shape = build_extrude_shape(parameters);
    if (preview_shape.IsNull()) {
      throw std::runtime_error("Extrude preview produced an empty shape");
    }
    mark_feature_healthy(feature);
    feature.parameters_summary =
        parameters.profile_id + " · " + std::to_string(parameters.depth) + " mm";
  } catch (const std::exception& error) {
    mark_extrude_preview_warning(feature, error.what());
  }
}

void normalize_timeline_cursor(DocumentState& document) {
  if (!document.timeline_cursor.has_value()) {
    return;
  }
  const int max_actions = action_count(document);
  if (document.timeline_cursor.value() >= max_actions) {
    document.timeline_cursor = std::nullopt;
  } else if (document.timeline_cursor.value() < 0) {
    document.timeline_cursor = 0;
  }
}

// True for any plane the user can pick from the viewport's reference
// hierarchy: the three origin planes plus every construction plane
// currently in `document.feature_history`. Face picks go through
// `select_face` instead and aren't covered here.
bool is_selectable_plane_reference(const DocumentState& document,
                                   const std::string& reference_id) {
  if (is_origin_plane_reference(reference_id)) {
    return true;
  }
  for (const auto& feature : document.feature_history) {
    if (feature.id == reference_id && feature.kind == "construction_plane") {
      return true;
    }
  }
  return false;
}

// Convert a `PlaneFrame` (the core-wide form used by the dependency
// resolver) into the sketch-specific copy stored on
// `SketchFeatureParameters`. Same field-by-field layout.
SketchFeatureParameters::SketchPlaneFrame to_sketch_plane_frame(
    const PlaneFrame& frame) {
  return SketchFeatureParameters::SketchPlaneFrame{
      .origin_x = frame.origin_x,
      .origin_y = frame.origin_y,
      .origin_z = frame.origin_z,
      .x_axis_x = frame.x_axis_x,
      .x_axis_y = frame.x_axis_y,
      .x_axis_z = frame.x_axis_z,
      .y_axis_x = frame.y_axis_x,
      .y_axis_y = frame.y_axis_y,
      .y_axis_z = frame.y_axis_z,
      .normal_x = frame.normal_x,
      .normal_y = frame.normal_y,
      .normal_z = frame.normal_z,
  };
}
