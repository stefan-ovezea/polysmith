DocumentState DocumentManager::update_sweep_path(
    const std::string& feature_id,
    const std::string& path_entity_id) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "sweep" || !feature_it->sweep_parameters.has_value()) {
    throw std::runtime_error(
        "update_sweep_path requires a sweep feature: " + feature_id);
  }

  const SweepFeatureParameters next_parameters =
      make_sweep_parameters(document_->feature_history,
                            feature_it->sweep_parameters->profile_id,
                            path_entity_id);
  const TopoDS_Shape next_shape = build_sweep_shape(next_parameters);
  if (next_shape.IsNull()) {
    throw std::runtime_error("Sweep update produced an empty shape");
  }
  feature_it->sweep_parameters = next_parameters;
  feature_it->parameters_summary = "Profile · path";
  feature_it->status = "healthy";
  feature_it->dependency_broken = false;
  feature_it->dependency_warning.clear();
  bump_geometry_revision();
  return document_.value();
}

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
  document_->selected_sketch_point_id = std::nullopt;
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
  document_->selected_sketch_point_ids.clear();
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
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->lines.back().id;
  document_->selected_sketch_dimension_id =
      is_construction ? std::nullopt
                      : std::make_optional(
                            feature_it->sketch_parameters->dimensions.back().id);
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
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
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->arcs.back().id;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_fillet(
    const std::string& corner_point_id,
    const std::string& line_a_id,
    const std::string& line_b_id,
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

  // Trim points share the line endpoint counter so the points table
  // stays uniform; arc and fillet have independent counters because
  // they're separate id namespaces.
  const int trim_a_point_index = next_sketch_line_id_++;
  const int trim_b_point_index = next_sketch_line_id_++;
  polysmith::core::add_sketch_fillet(*feature_it,
                                     next_sketch_fillet_id_++,
                                     trim_a_point_index,
                                     trim_b_point_index,
                                     next_sketch_arc_id_++,
                                     corner_point_id,
                                     line_a_id,
                                     line_b_id,
                                     radius);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->fillets.back().id;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sketch_fillet_radius(
    const std::string& fillet_id,
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
  polysmith::core::update_sketch_fillet_radius(*feature_it, fillet_id, radius);
  refresh_linked_extrudes(*document_, *feature_it);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::delete_sketch_fillet(
    const std::string& fillet_id) {
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
  polysmith::core::delete_sketch_fillet(*feature_it, fillet_id);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_sketch_entity_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::delete_sketch_dimension(
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
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::delete_sketch_dimension(*feature_it, dimension_id);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sketch_dimension_display(
    const std::string& dimension_id,
    const std::string& display_as) {
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

  if (!feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Sketch feature has no parameters");
  }

  auto& dimensions = feature_it->sketch_parameters->dimensions;
  const auto dim_it = std::find_if(
      dimensions.begin(), dimensions.end(),
      [&](const SketchDimension& d) { return d.id == dimension_id; });
  if (dim_it == dimensions.end()) {
    throw std::runtime_error("Sketch dimension not found: " + dimension_id);
  }
  // Only circle_radius dimensions support display_as toggling
  if (dim_it->kind != "circle_radius") {
    throw std::runtime_error(
        "display_as can only be changed on circle radius dimensions");
  }

  push_undo_state();
  clear_redo_stack();
  dim_it->display_as = display_as;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::delete_sketch_selection(
    const std::vector<std::string>& entity_ids,
    const std::vector<std::string>& point_ids,
    const std::vector<std::string>& profile_ids) {
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

  auto& parameters = *feature_it->sketch_parameters;
  std::unordered_set<std::string> line_ids;
  std::unordered_set<std::string> circle_ids;
  std::unordered_set<std::string> arc_ids;
  std::unordered_set<std::string> projected_point_ids;
  std::unordered_set<std::string> selected_point_ids;
  std::unordered_set<std::string> fillet_ids;

  add_ids(selected_point_ids, point_ids);

  for (const auto& profile_id : profile_ids) {
    const auto profile_it = std::find_if(
        parameters.profiles.begin(),
        parameters.profiles.end(),
        [&](const SketchProfileRegion& profile) {
          return profile.id == profile_id;
        });
    if (profile_it == parameters.profiles.end()) {
      continue;
    }
    add_ids(line_ids, profile_it->line_ids);
    if (profile_it->source_circle_id.has_value()) {
      add_id(circle_ids, profile_it->source_circle_id.value());
    }
  }

  for (const auto& entity_id : entity_ids) {
    add_id(line_ids, entity_id);
    add_id(circle_ids, entity_id);
    add_id(arc_ids, entity_id);
    add_id(fillet_ids, entity_id);
  }

  for (const auto& point_id : selected_point_ids) {
    for (const auto& line : parameters.lines) {
      if (line.start_point_id == point_id || line.end_point_id == point_id) {
        add_id(line_ids, line.id);
      }
    }
    for (const auto& arc : parameters.arcs) {
      if (arc.start_point_id == point_id || arc.end_point_id == point_id) {
        add_id(arc_ids, arc.id);
      }
    }
    for (const auto& circle : parameters.circles) {
      if ("point-circle-" + circle.id + "-center" == point_id) {
        add_id(circle_ids, circle.id);
      }
    }
    for (const auto& projected : parameters.projected_points) {
      if (projected.id == point_id) {
        add_id(projected_point_ids, projected.id);
      }
    }
  }

  for (const auto& fillet : parameters.fillets) {
    if (id_in_set(line_ids, fillet.line_a_id) ||
        id_in_set(line_ids, fillet.line_b_id) ||
        id_in_set(arc_ids, fillet.arc_id) ||
        id_in_set(selected_point_ids, fillet.corner_point_id) ||
        id_in_set(selected_point_ids, fillet.trim_a_point_id) ||
        id_in_set(selected_point_ids, fillet.trim_b_point_id)) {
      add_id(fillet_ids, fillet.id);
    }
  }

  const auto has_selected_line = [&](const SketchLine& line) {
    return id_in_set(line_ids, line.id);
  };
  const auto has_selected_circle = [&](const SketchCircle& circle) {
    return id_in_set(circle_ids, circle.id);
  };
  const auto has_selected_arc = [&](const SketchArc& arc) {
    return id_in_set(arc_ids, arc.id);
  };
  const auto has_selected_projected_point =
      [&](const SketchProjectedPoint& projected) {
        return id_in_set(projected_point_ids, projected.id);
      };

  const bool will_delete =
      std::any_of(parameters.lines.begin(), parameters.lines.end(), has_selected_line) ||
      std::any_of(parameters.circles.begin(),
                  parameters.circles.end(),
                  has_selected_circle) ||
      std::any_of(parameters.arcs.begin(), parameters.arcs.end(), has_selected_arc) ||
      std::any_of(parameters.projected_points.begin(),
                  parameters.projected_points.end(),
                  has_selected_projected_point) ||
      std::any_of(parameters.fillets.begin(),
                  parameters.fillets.end(),
                  [&](const SketchFillet& fillet) {
                    return id_in_set(fillet_ids, fillet.id);
                  });
  if (!will_delete) {
    return document_.value();
  }

  push_undo_state();
  clear_redo_stack();

  std::vector<std::string> fillets_to_delete;
  for (const auto& fillet : parameters.fillets) {
    if (id_in_set(fillet_ids, fillet.id)) {
      fillets_to_delete.push_back(fillet.id);
    }
  }
  for (const auto& fillet_id : fillets_to_delete) {
    polysmith::core::delete_sketch_fillet(*feature_it, fillet_id);
  }

  parameters.lines.erase(
      std::remove_if(parameters.lines.begin(), parameters.lines.end(), has_selected_line),
      parameters.lines.end());
  parameters.circles.erase(
      std::remove_if(parameters.circles.begin(),
                     parameters.circles.end(),
                     has_selected_circle),
      parameters.circles.end());
  parameters.arcs.erase(
      std::remove_if(parameters.arcs.begin(), parameters.arcs.end(), has_selected_arc),
      parameters.arcs.end());
  parameters.projected_points.erase(
      std::remove_if(parameters.projected_points.begin(),
                     parameters.projected_points.end(),
                     has_selected_projected_point),
      parameters.projected_points.end());

  const auto deleted_entity = [&](const std::string& id) {
    return id_in_set(line_ids, id) || id_in_set(circle_ids, id) ||
           id_in_set(arc_ids, id);
  };
  parameters.dimensions.erase(
      std::remove_if(parameters.dimensions.begin(),
                     parameters.dimensions.end(),
                     [&](const SketchDimension& dimension) {
                       return deleted_entity(dimension.entity_id) ||
                              deleted_entity(dimension.secondary_entity_id);
                     }),
      parameters.dimensions.end());
  parameters.line_relations.erase(
      std::remove_if(parameters.line_relations.begin(),
                     parameters.line_relations.end(),
                     [&](const SketchLineRelation& relation) {
                       return deleted_entity(relation.first_line_id) ||
                              deleted_entity(relation.second_line_id);
                     }),
      parameters.line_relations.end());
  parameters.midpoint_anchors.erase(
      std::remove_if(parameters.midpoint_anchors.begin(),
                     parameters.midpoint_anchors.end(),
                     [&](const SketchMidpointAnchor& anchor) {
                       return id_in_set(selected_point_ids, anchor.point_id) ||
                              id_in_set(line_ids, anchor.line_id);
                     }),
      parameters.midpoint_anchors.end());
  parameters.point_line_anchors.erase(
      std::remove_if(parameters.point_line_anchors.begin(),
                     parameters.point_line_anchors.end(),
                     [&](const SketchPointLineAnchor& anchor) {
                       return id_in_set(selected_point_ids, anchor.point_id) ||
                              id_in_set(line_ids, anchor.line_id);
                     }),
      parameters.point_line_anchors.end());
  for (auto& projection : parameters.projections) {
    remove_ids_from_vector(projection.generated_line_ids, line_ids);
    remove_ids_from_vector(projection.generated_circle_ids, circle_ids);
    remove_ids_from_vector(projection.generated_arc_ids, arc_ids);
    if (!projection.generated_point_id.empty() &&
        id_in_set(projected_point_ids, projection.generated_point_id)) {
      projection.generated_point_id.clear();
    }
  }
  parameters.projections.erase(
      std::remove_if(parameters.projections.begin(),
                     parameters.projections.end(),
                     [](const SketchProjection& projection) {
                       return !projection_has_generated_entities(projection);
                     }),
      parameters.projections.end());

  refresh_sketch_derived_state(*feature_it);
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

DocumentState DocumentManager::add_sketch_circle(double center_x,
                                                 double center_y,
                                                 double radius,
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
  polysmith::core::add_sketch_circle(
      *feature_it, next_sketch_circle_id_++, center_x, center_y, radius,
      is_construction);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->circles.back().id;
  document_->selected_sketch_dimension_id =
      is_construction ? std::nullopt
                      : std::make_optional(
                            feature_it->sketch_parameters->dimensions.back().id);
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  document_->active_sketch_tool = "circle";
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_polygon(int sides,
                                                  const std::string& mode,
                                                  double start_x,
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
  polysmith::core::add_sketch_polygon(
      *feature_it, next_sketch_polygon_id_++, sides, mode, start_x, start_y,
      end_x, end_y, is_construction);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->polygons.back().id;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  document_->active_sketch_tool = "polygon";
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::select_sketch_point(const std::string& point_id,
                                                   bool additive) {
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

  const auto point_it = std::find_if(
      feature_it->sketch_parameters->points.begin(),
      feature_it->sketch_parameters->points.end(),
      [&](const SketchPoint& point) { return point.id == point_id; });
  if (point_it == feature_it->sketch_parameters->points.end()) {
    throw std::runtime_error("Sketch point not found: " + point_id);
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_entity_ids.clear();
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  if (additive) {
    auto& point_ids = document_->selected_sketch_point_ids;
    const auto existing =
        std::find(point_ids.begin(), point_ids.end(), point_id);
    if (existing == point_ids.end()) {
      point_ids.push_back(point_id);
    } else {
      point_ids.erase(existing);
    }
  } else {
    document_->selected_sketch_point_ids = {point_id};
  }
  document_->selected_sketch_point_id =
      document_->selected_sketch_point_ids.empty()
          ? std::nullopt
          : std::make_optional(document_->selected_sketch_point_ids.back());
  return document_.value();
}

DocumentState DocumentManager::select_sketch_entity(
    const std::string& entity_id, bool additive) {
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
  const bool has_arc = std::any_of(
      feature_it->sketch_parameters->arcs.begin(),
      feature_it->sketch_parameters->arcs.end(),
      [&](const SketchArc& arc) { return arc.id == entity_id; });
  const bool has_polygon = std::any_of(
      feature_it->sketch_parameters->polygons.begin(),
      feature_it->sketch_parameters->polygons.end(),
      [&](const SketchPolygon& polygon) { return polygon.id == entity_id; });

  if (!has_line && !has_circle && !has_arc && !has_polygon) {
    throw std::runtime_error("Sketch entity not found: " + entity_id);
  }

  const bool is_projected_entity = std::any_of(
      feature_it->sketch_parameters->projections.begin(),
      feature_it->sketch_parameters->projections.end(),
      [&](const SketchProjection& projection) {
        return projection_generates_entity(projection, entity_id);
      });

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_point_id = std::nullopt;
  if (additive) {
    auto& entity_ids = document_->selected_sketch_entity_ids;
    const auto existing =
        std::find(entity_ids.begin(), entity_ids.end(), entity_id);
    if (existing == entity_ids.end()) {
      entity_ids.push_back(entity_id);
    } else {
      entity_ids.erase(existing);
    }
  } else {
    document_->selected_sketch_entity_ids = {entity_id};
  }
  document_->selected_sketch_entity_id =
      document_->selected_sketch_entity_ids.empty()
          ? std::nullopt
          : std::make_optional(document_->selected_sketch_entity_ids.back());
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dimension) {
        return !is_projected_entity &&
               document_->selected_sketch_entity_ids.size() == 1 &&
               dimension.entity_id == document_->selected_sketch_entity_ids.front();
      });
  document_->selected_sketch_dimension_id =
      dimension_it != feature_it->sketch_parameters->dimensions.end()
          ? std::make_optional(dimension_it->id)
          : std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
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
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = dimension_it->entity_id;
  document_->selected_sketch_dimension_id = dimension_it->id;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
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

DocumentState DocumentManager::reenter_sketch(const std::string& feature_id) {
  require_document();

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Sketch feature not found: " + feature_id);
  }

  if (feature_it->kind != "sketch" || !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Feature is not a sketch: " + feature_id);
  }

  // Re-entering does not push to undo: it only flips active flags, so undo
  // continues to refer to real geometry edits.
  document_->selected_feature_id = std::nullopt;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->active_sketch_plane_id = feature_it->sketch_parameters->plane_id;
  // Face id information is not preserved separately in sketch_parameters; the
  // plane_id matches the face id when the sketch was created on a face, which
  // is sufficient for downstream consumers (viewport hides the sketch face,
  // raycasting is plane-frame based).
  document_->active_sketch_face_id =
      feature_it->sketch_parameters->plane_frame.has_value()
          ? std::optional<std::string>(feature_it->sketch_parameters->plane_id)
          : std::nullopt;
  document_->active_sketch_feature_id = feature_it->id;
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

DocumentState DocumentManager::project_face_into_sketch(
    const std::string& face_id) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto sketch_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (sketch_it == document_->feature_history.end() ||
      !sketch_it->sketch_parameters.has_value() ||
      !sketch_it->sketch_parameters->plane_frame.has_value()) {
    throw std::runtime_error(
        "Active sketch does not have a plane frame for projection");
  }

  // Don't project a face onto its own owning sketch — the sketch is on the
  // base plane of the extrude, so projecting the base back onto it would
  // overlay the original profile.
  const std::string separator = ":face:";
  const auto sep_pos = face_id.find(separator);
  if (sep_pos == std::string::npos) {
    throw std::runtime_error("Invalid face id: " + face_id);
  }
  const std::string face_owner_id = face_id.substr(0, sep_pos);

  // Walk extrude features that point at this sketch — projecting one of
  // their faces back onto the sketch is almost always a user mistake.
  for (const auto& feature : document_->feature_history) {
    if (feature.kind != "extrude" || !feature.extrude_parameters.has_value()) {
      continue;
    }
    if (feature.extrude_parameters->sketch_feature_id == sketch_it->id &&
        feature.id == face_owner_id) {
      throw std::runtime_error(
          "Cannot project a face from an extrude back onto its own source "
          "sketch");
    }
  }

  // Idempotency: if the user already projected this face onto this
  // sketch (typical when the modal Project tool stays active and they
  // accidentally click the same face twice), skip the rebuild and
  // return the current document state unchanged. Matches common CAD workflow's
  // "second click is a no-op" behaviour. Vertex / edge projections
  // share this dedup index so all three project_* methods walk a
  // single `projections` list.
  for (const auto& projection :
       sketch_it->sketch_parameters->projections) {
    if (projection.source_id == face_id) {
      return document_.value();
    }
  }

  const auto outline = compute_face_outline(*document_, face_id);
  if (!outline.has_value()) {
    throw std::runtime_error(
        "Face is not supported by the projection helper: " + face_id);
  }

  const auto& sketch_frame = sketch_it->sketch_parameters->plane_frame.value();

  auto project_to_sketch_local =
      [&sketch_frame](const FaceOutlinePoint& world) -> std::pair<double, double> {
    const double dx = world.x - sketch_frame.origin_x;
    const double dy = world.y - sketch_frame.origin_y;
    const double dz = world.z - sketch_frame.origin_z;
    const double sx = dx * sketch_frame.x_axis_x + dy * sketch_frame.x_axis_y +
                      dz * sketch_frame.x_axis_z;
    const double sy = dx * sketch_frame.y_axis_x + dy * sketch_frame.y_axis_y +
                      dz * sketch_frame.y_axis_z;
    return {sx, sy};
  };

  push_undo_state();
  clear_redo_stack();

  // We capture the ids of every sketch entity this projection emits
  // so `refresh_sketch_projections` can later patch their cached
  // coords when the upstream face moves. Faces produce lines
  // (rectangle / polygon outline) or circles for circular loops.
  SketchProjection record;
  record.id =
      "projection-" + std::to_string(next_sketch_projection_id_++);
  record.source_id = face_id;
  record.source_kind = "face";

  if (outline->kind == "rectangle") {
    if (outline->rectangle_corners.size() != 4) {
      throw std::runtime_error("Rectangle outline must have four corners");
    }

    std::array<std::pair<double, double>, 4> local{};
    for (size_t i = 0; i < 4; ++i) {
      local[i] = project_to_sketch_local(outline->rectangle_corners[i]);
    }

    const size_t lines_before = sketch_it->sketch_parameters->lines.size();
    for (size_t i = 0; i < 4; ++i) {
      const auto& a = local[i];
      const auto& b = local[(i + 1) % 4];
      polysmith::core::add_sketch_line(*sketch_it,
                                       next_sketch_line_id_++,
                                       a.first,
                                       a.second,
                                       b.first,
                                       b.second);
    }

    // Lock every endpoint of the four projected lines so the user cannot
    // drag them away from their projected location. This mirrors common CAD workflow's
    // "Project" behaviour where projected entities are derived geometry.
    for (size_t i = lines_before; i < sketch_it->sketch_parameters->lines.size();
         ++i) {
      const auto& line = sketch_it->sketch_parameters->lines[i];
      polysmith::core::set_sketch_point_fixed(*sketch_it,
                                              line.start_point_id,
                                              true);
      polysmith::core::set_sketch_point_fixed(*sketch_it,
                                              line.end_point_id,
                                              true);
      record.generated_line_ids.push_back(line.id);
    }
  } else if (outline->kind == "circle") {
    auto add_projected_circle =
        [&](const FaceOutlinePoint& center, double radius) {
      const auto center_local = project_to_sketch_local(center);
      const size_t circles_before =
          sketch_it->sketch_parameters->circles.size();
      polysmith::core::add_sketch_circle(*sketch_it,
                                         next_sketch_circle_id_++,
                                         center_local.first,
                                         center_local.second,
                                         radius);
      for (size_t i = circles_before;
           i < sketch_it->sketch_parameters->circles.size(); ++i) {
        const auto& circle = sketch_it->sketch_parameters->circles[i];
        polysmith::core::set_sketch_point_fixed(
            *sketch_it, "point-circle-" + circle.id + "-center", true);
        record.generated_circle_ids.push_back(
            circle.id);
      }
    };

    add_projected_circle(outline->circle_center, outline->circle_radius);
    for (const auto& inner_circle : outline->inner_circles) {
      add_projected_circle(inner_circle.center, inner_circle.radius);
    }
  } else if (outline->kind == "polygon") {
    if (outline->polygon_corners.size() < 3) {
      throw std::runtime_error("Polygon outline must have at least 3 corners");
    }

    // Project every corner first, then add closed-loop sketch lines.
    std::vector<std::pair<double, double>> local;
    local.reserve(outline->polygon_corners.size());
    for (const auto& corner : outline->polygon_corners) {
      local.push_back(project_to_sketch_local(corner));
    }

    const size_t lines_before = sketch_it->sketch_parameters->lines.size();
    for (size_t i = 0; i < local.size(); ++i) {
      const auto& a = local[i];
      const auto& b = local[(i + 1) % local.size()];
      polysmith::core::add_sketch_line(*sketch_it,
                                       next_sketch_line_id_++,
                                       a.first,
                                       a.second,
                                       b.first,
                                       b.second);
    }
    for (const auto& inner_loop : outline->inner_loops) {
      if (inner_loop.size() < 3) {
        continue;
      }
      std::vector<std::pair<double, double>> inner_local;
      inner_local.reserve(inner_loop.size());
      for (const auto& corner : inner_loop) {
        inner_local.push_back(project_to_sketch_local(corner));
      }
      for (size_t i = 0; i < inner_local.size(); ++i) {
        const auto& a = inner_local[i];
        const auto& b = inner_local[(i + 1) % inner_local.size()];
        polysmith::core::add_sketch_line(*sketch_it,
                                         next_sketch_line_id_++,
                                         a.first,
                                         a.second,
                                         b.first,
                                         b.second);
      }
    }

    // Lock every endpoint of the projected lines (same CAD-style
    // "derived geometry" lock as the rectangle path).
    for (size_t i = lines_before; i < sketch_it->sketch_parameters->lines.size();
         ++i) {
      const auto& line = sketch_it->sketch_parameters->lines[i];
      polysmith::core::set_sketch_point_fixed(*sketch_it,
                                              line.start_point_id,
                                              true);
      polysmith::core::set_sketch_point_fixed(*sketch_it,
                                              line.end_point_id,
                                              true);
      record.generated_line_ids.push_back(line.id);
    }
  } else {
    throw std::runtime_error("Unsupported projected face kind: " + outline->kind);
  }

  // Register the live link so any future upstream edit of `face_id`
  // can re-derive these generated entities (see
  // `refresh_sketch_projections`).
  sketch_it->sketch_parameters->projections.push_back(std::move(record));

  refresh_linked_extrudes(*document_, *sketch_it);
  document_->selected_feature_id = sketch_it->id;
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

namespace {

// Validate that the active sketch in `document` is set up to receive
// projected geometry: there must be an active sketch with a plane
// frame. Returns the matching FeatureEntry iterator on success and
// throws a runtime_error on every failure path so all three
// project_*_into_sketch methods share the same diagnostics.
std::vector<FeatureEntry>::iterator require_projection_target(
    DocumentState& document) {
  if (!document.active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }
  const auto sketch_it = std::find_if(
      document.feature_history.begin(),
      document.feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document.active_sketch_feature_id.value();
      });
  if (sketch_it == document.feature_history.end() ||
      !sketch_it->sketch_parameters.has_value() ||
      !sketch_it->sketch_parameters->plane_frame.has_value()) {
    throw std::runtime_error(
        "Active sketch does not have a plane frame for projection");
  }
  return sketch_it;
}

// Flatten a world-space point onto the sketch's local (u, v) frame.
// Identical maths to the lambda inside `project_face_into_sketch` —
// pulled out so edge / vertex projection can reuse it without
// duplicating the dot-product expansion.
std::pair<double, double> world_to_sketch_local(
    const SketchFeatureParameters::SketchPlaneFrame& frame,
    double wx, double wy, double wz) {
  const double dx = wx - frame.origin_x;
  const double dy = wy - frame.origin_y;
  const double dz = wz - frame.origin_z;
  const double sx =
      dx * frame.x_axis_x + dy * frame.x_axis_y + dz * frame.x_axis_z;
  const double sy =
      dx * frame.y_axis_x + dy * frame.y_axis_y + dz * frame.y_axis_z;
  return {sx, sy};
}

// Returns true when `axis` is parallel (or anti-parallel) to the
// sketch frame's normal. A circular edge whose plane is parallel to
// the sketch plane projects cleanly to a circle / arc; non-parallel
// circular edges would project to ellipses, which we don't support
// in v1 (see the user-facing decision in the implementation log).
bool circle_axis_parallel_to_sketch(
    const SketchFeatureParameters::SketchPlaneFrame& frame,
    const EdgePoint& axis,
    double tolerance = 1e-6) {
  // dot product magnitude == 1 (within tolerance) iff parallel /
  // anti-parallel given both are unit vectors.
  const double dot = axis.x * frame.normal_x +
                     axis.y * frame.normal_y +
                     axis.z * frame.normal_z;
  return std::abs(std::abs(dot) - 1.0) <= tolerance;
}

FaceOutlinePoint sketch_local_to_world(const SketchFeatureParameters& sketch,
                                       double local_x,
                                       double local_y) {
  if (sketch.plane_frame.has_value()) {
    const auto& frame = sketch.plane_frame.value();
    return FaceOutlinePoint{
        .x = frame.origin_x + frame.x_axis_x * local_x +
             frame.y_axis_x * local_y,
        .y = frame.origin_y + frame.x_axis_y * local_x +
             frame.y_axis_y * local_y,
        .z = frame.origin_z + frame.x_axis_z * local_x +
             frame.y_axis_z * local_y,
    };
  }
  if (sketch.plane_id == "ref-plane-xy") {
    return FaceOutlinePoint{.x = local_x, .y = 0.0, .z = local_y};
  }
  if (sketch.plane_id == "ref-plane-yz") {
    return FaceOutlinePoint{.x = 0.0, .y = local_x, .z = local_y};
  }
  return FaceOutlinePoint{.x = local_x, .y = local_y, .z = 0.0};
}

FaceOutlinePoint sketch_normal(const SketchFeatureParameters& sketch) {
  if (sketch.plane_frame.has_value()) {
    const auto& frame = sketch.plane_frame.value();
    return FaceOutlinePoint{
        .x = frame.normal_x, .y = frame.normal_y, .z = frame.normal_z};
  }
  if (sketch.plane_id == "ref-plane-yz") {
    return FaceOutlinePoint{.x = 1.0, .y = 0.0, .z = 0.0};
  }
  if (sketch.plane_id == "ref-plane-xz") {
    return FaceOutlinePoint{.x = 0.0, .y = 0.0, .z = 1.0};
  }
  return FaceOutlinePoint{.x = 0.0, .y = 1.0, .z = 0.0};
}

bool normals_parallel(const FaceOutlinePoint& left,
                      const SketchFeatureParameters::SketchPlaneFrame& right,
                      double tolerance = 1e-6) {
  const double dot = left.x * right.normal_x + left.y * right.normal_y +
                     left.z * right.normal_z;
  return std::abs(std::abs(dot) - 1.0) <= tolerance;
}

bool circle_contains_circle_for_projection(const SketchCircle& outer,
                                           const SketchCircle& inner) {
  if (outer.id == inner.id || outer.radius <= inner.radius) {
    return false;
  }
  const double dx = inner.center_x - outer.center_x;
  const double dy = inner.center_y - outer.center_y;
  const double center_distance = std::sqrt(dx * dx + dy * dy);
  return center_distance + inner.radius <= outer.radius + 1e-6;
}

}  // namespace
