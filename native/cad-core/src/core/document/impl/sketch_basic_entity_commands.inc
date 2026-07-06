DocumentState DocumentManager::add_sketch_line(double start_x,
                                               double start_y,
                                               double end_x,
                                               double end_y,
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
  polysmith::core::add_sketch_line(*feature_it,
                                   next_sketch_line_id_++,
                                   start_x,
                                   start_y,
                                   end_x,
                                   end_y,
                                   is_construction,
                                   /*snap_start=*/false);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->lines.back().id;
  // Construction lines don't get an auto length dimension, so the
  // dimension list may be unchanged. Pick the dimension only if one
  // was actually created for this line; otherwise clear the
  // selection so the editor doesn't open on an unrelated dimension.
  const std::string expected_dim_id =
      "dim-line-" + feature_it->sketch_parameters->lines.back().id;
  const auto& dims = feature_it->sketch_parameters->dimensions;
  const auto dim_it = std::find_if(
      dims.begin(), dims.end(),
      [&](const SketchDimension& dim) { return dim.id == expected_dim_id; });
  document_->selected_sketch_dimension_id =
      dim_it != dims.end() ? std::optional<std::string>{dim_it->id}
                           : std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  document_->active_sketch_tool = "line";
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_rectangle(double start_x,
                                                    double start_y,
                                                    double end_x,
                                                    double end_y,
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
  polysmith::core::add_sketch_rectangle(
      *feature_it, next_sketch_line_id_, start_x, start_y, end_x, end_y,
      is_construction);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->lines.back().id;
  document_->selected_sketch_dimension_id =
      is_construction ? std::nullopt
                      : std::make_optional(
                            feature_it->sketch_parameters->dimensions.back().id);
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  document_->active_sketch_tool = "rectangle";
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_arc(double start_x,
                                              double start_y,
                                              double end_x,
                                              double end_y,
                                              double anchor_x,
                                              double anchor_y,
                                              const std::string& mode,
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

  // Resolve (center, radius, ccw) up-front from whichever creation
  // mode the UI used. Doing it here keeps `polysmith::core::add_sketch_arc`
  // a pure constructor — it just stores the cached params we hand it.
  double center_x = 0.0;
  double center_y = 0.0;
  double radius = 0.0;
  double final_end_x = end_x;
  double final_end_y = end_y;
  bool ccw = true;

  if (mode == "three_point") {
    // Circumcenter of (start, end, anchor). Solved via the standard
    // perpendicular-bisector intersection:
    //   d = 2 * ((sx)(ey - ay) + (ex)(ay - sy) + (ax)(sy - ey))
    // Falls through to a degenerate-arc error when the three points
    // are colinear (d == 0) so the UI can surface the failure
    // instead of producing a malformed arc.
    const double sx = start_x;
    const double sy = start_y;
    const double ex = end_x;
    const double ey = end_y;
    const double ax = anchor_x;
    const double ay = anchor_y;
    const double d =
        2.0 * (sx * (ey - ay) + ex * (ay - sy) + ax * (sy - ey));
    if (std::abs(d) < 1e-9) {
      throw std::runtime_error(
          "Three-point arc: start, end, and anchor are colinear");
    }
    const double s2 = sx * sx + sy * sy;
    const double e2 = ex * ex + ey * ey;
    const double a2 = ax * ax + ay * ay;
    center_x =
        (s2 * (ey - ay) + e2 * (ay - sy) + a2 * (sy - ey)) / d;
    center_y =
        (s2 * (ax - ex) + e2 * (sx - ax) + a2 * (ex - sx)) / d;
    radius = std::hypot(start_x - center_x, start_y - center_y);
    // Sweep direction: CCW iff the signed area (start, anchor, end)
    // is positive. Matches "the anchor lies on the CCW arc from
    // start to end".
    const double cross =
        (anchor_x - start_x) * (end_y - start_y) -
        (anchor_y - start_y) * (end_x - start_x);
    ccw = cross > 0.0;
  } else if (mode == "center_start_end") {
    // anchor is the center; radius = |center - start|. We then
    // *snap* the user-supplied end point onto the resulting circle
    // by walking the angle direction from center→end and
    // re-projecting at radius distance — keeps the arc closed even
    // if the user's third click wasn't pixel-perfect.
    center_x = anchor_x;
    center_y = anchor_y;
    radius = std::hypot(start_x - center_x, start_y - center_y);
    // Mirrors the dimension-floor used elsewhere in sketch validation
    // (see `kMinimumSketchDimensionValue` in sketch_feature.cpp). Kept
    // as an inline literal here because the constant lives in a TU-
    // local anonymous namespace in sketch_feature.cpp; promoting it
    // to a header just for this one call site isn't worth the churn.
    constexpr double kArcMinimumDimension = 0.001;
    if (radius <= kArcMinimumDimension) {
      throw std::runtime_error("Center+start+end arc: radius is zero");
    }
    const double end_dx = end_x - center_x;
    const double end_dy = end_y - center_y;
    const double end_len = std::hypot(end_dx, end_dy);
    if (end_len <= kArcMinimumDimension) {
      throw std::runtime_error(
          "Center+start+end arc: end point coincides with center");
    }
    final_end_x = center_x + end_dx * radius / end_len;
    final_end_y = center_y + end_dy * radius / end_len;
    // CCW when going from start_angle to end_angle increases angle
    // (i.e. the cross product of the two radii is positive).
    const double cross =
        (start_x - center_x) * (final_end_y - center_y) -
        (start_y - center_y) * (final_end_x - center_x);
    ccw = cross > 0.0;
  } else {
    throw std::runtime_error("Unknown arc creation mode: " + mode);
  }

  push_undo_state();
  clear_redo_stack();

  // Endpoint points share the line counter so they live in a single
  // id space; arc id has its own counter.
  const int start_point_index = next_sketch_line_id_++;
  const int end_point_index = next_sketch_line_id_++;
  polysmith::core::add_sketch_arc(*feature_it,
                                  next_sketch_arc_id_++,
                                  start_point_index,
                                  end_point_index,
                                  start_x,
                                  start_y,
                                  final_end_x,
                                  final_end_y,
                                  center_x,
                                  center_y,
                                  radius,
                                  ccw,
                                  is_construction);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->arcs.back().id;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}
