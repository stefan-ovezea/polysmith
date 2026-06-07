DocumentState DocumentManager::project_profile_into_sketch(
    const std::string& profile_id) {
  require_document();
  const auto sketch_it = require_projection_target(*document_);

  for (const auto& projection : sketch_it->sketch_parameters->projections) {
    if (projection.source_id == profile_id) {
      return document_.value();
    }
  }

  const auto source_it = find_sketch_feature_owning_profile(
      document_->feature_history, profile_id);
  if (source_it == document_->feature_history.end() ||
      !source_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }

  const SketchFeatureParameters source_sketch = source_it->sketch_parameters.value();
  const auto profile_it = std::find_if(
      source_sketch.profiles.begin(), source_sketch.profiles.end(),
      [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });
  if (profile_it == source_sketch.profiles.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }
  const SketchProfileRegion source_profile = *profile_it;
  const auto target_frame = sketch_it->sketch_parameters->plane_frame.value();

  auto project_source_point =
      [&](const SketchProfilePoint& point) -> std::pair<double, double> {
    const auto world = sketch_local_to_world(source_sketch, point.x, point.y);
    return world_to_sketch_local(target_frame, world.x, world.y, world.z);
  };

  push_undo_state();
  clear_redo_stack();

  SketchProjection record;
  record.id =
      "projection-" + std::to_string(next_sketch_projection_id_++);
  record.source_id = profile_id;
  record.source_kind = "profile";

  auto add_projected_loop =
      [&](const std::vector<SketchProfilePoint>& loop) {
    if (loop.size() < 3) {
      return;
    }
    std::vector<std::pair<double, double>> local;
    local.reserve(loop.size());
    for (const auto& point : loop) {
      local.push_back(project_source_point(point));
    }
    const size_t lines_before = sketch_it->sketch_parameters->lines.size();
    for (size_t index = 0; index < local.size(); ++index) {
      const auto& a = local[index];
      const auto& b = local[(index + 1) % local.size()];
      polysmith::core::add_sketch_line(*sketch_it,
                                       next_sketch_line_id_++,
                                       a.first,
                                       a.second,
                                       b.first,
                                       b.second);
    }
    for (size_t index = lines_before;
         index < sketch_it->sketch_parameters->lines.size(); ++index) {
      const auto& line = sketch_it->sketch_parameters->lines[index];
      polysmith::core::set_sketch_point_fixed(
          *sketch_it, line.start_point_id, true);
      polysmith::core::set_sketch_point_fixed(
          *sketch_it, line.end_point_id, true);
      record.generated_line_ids.push_back(line.id);
    }
  };

  const auto add_projected_circle =
      [&](const SketchCircle& circle) {
    const auto center_world = sketch_local_to_world(
        source_sketch, circle.center_x, circle.center_y);
    const auto center_local = world_to_sketch_local(
        target_frame, center_world.x, center_world.y, center_world.z);
    const size_t circles_before = sketch_it->sketch_parameters->circles.size();
    polysmith::core::add_sketch_circle(*sketch_it,
                                       next_sketch_circle_id_++,
                                       center_local.first,
                                       center_local.second,
                                       circle.radius);
    for (size_t index = circles_before;
         index < sketch_it->sketch_parameters->circles.size(); ++index) {
      const auto& circle = sketch_it->sketch_parameters->circles[index];
      polysmith::core::set_sketch_point_fixed(
          *sketch_it, "point-circle-" + circle.id + "-center", true);
      record.generated_circle_ids.push_back(
          circle.id);
    }
  };

  if (source_profile.kind == "circle" ||
      source_profile.source_circle_id.has_value()) {
    if (!normals_parallel(sketch_normal(source_sketch), target_frame)) {
      throw std::runtime_error(
          "Circular profile projects to an ellipse (its sketch plane is not "
          "parallel to the active sketch). Not supported in v1.");
    }
    const auto source_circle_it = source_profile.source_circle_id.has_value()
        ? std::find_if(
              source_sketch.circles.begin(), source_sketch.circles.end(),
              [&](const SketchCircle& circle) {
                return circle.id == source_profile.source_circle_id.value();
              })
        : std::find_if(
              source_sketch.circles.begin(), source_sketch.circles.end(),
              [&](const SketchCircle& circle) {
                return circle.id == source_profile.source_circle_id.value_or("");
              });
    if (source_profile.kind == "circle") {
      SketchCircle circle{};
      circle.id = source_profile.source_circle_id.value_or("");
      circle.center_x = source_profile.center_x;
      circle.center_y = source_profile.center_y;
      circle.radius = source_profile.radius;
      add_projected_circle(circle);
    } else if (source_circle_it != source_sketch.circles.end()) {
      add_projected_circle(*source_circle_it);
      for (const auto& candidate : source_sketch.circles) {
        if (candidate.is_construction ||
            !circle_contains_circle_for_projection(*source_circle_it,
                                                   candidate)) {
          continue;
        }
        add_projected_circle(candidate);
      }
    } else {
      add_projected_loop(source_profile.points);
    }
  } else {
    add_projected_loop(source_profile.points);
  }

  if (record.generated_circle_ids.empty()) {
    for (const auto& inner_loop : source_profile.inner_loops) {
      add_projected_loop(inner_loop);
    }
  }

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

DocumentState DocumentManager::project_edge_into_sketch(
    const std::string& edge_id) {
  require_document();
  const auto sketch_it = require_projection_target(*document_);

  // Reject body edges that belong to an extrude built from this same
  // sketch — same rationale as the face guard above.
  const auto separator = edge_id.find(":edge:");
  if (separator == std::string::npos || separator == 0) {
    throw std::runtime_error("Malformed edge id: " + edge_id);
  }
  const std::string owner_id = edge_id.substr(0, separator);
  for (const auto& feature : document_->feature_history) {
    if (feature.kind != "extrude" || !feature.extrude_parameters.has_value()) {
      continue;
    }
    if (feature.extrude_parameters->sketch_feature_id == sketch_it->id &&
        feature.id == owner_id) {
      throw std::runtime_error(
          "Cannot project an edge from an extrude back onto its own source "
          "sketch");
    }
  }

  // Idempotency: same dedup index that face projection uses.
  for (const auto& projection :
       sketch_it->sketch_parameters->projections) {
    if (projection.source_id == edge_id) {
      return document_.value();
    }
  }

  const auto edge_geometry = compute_edge_geometry(*document_, edge_id);
  if (!edge_geometry.has_value()) {
    throw std::runtime_error(
        "Edge could not be resolved against the current document: " +
        edge_id);
  }

  const auto& frame = sketch_it->sketch_parameters->plane_frame.value();

  push_undo_state();
  clear_redo_stack();

  // See `project_face_into_sketch` for the rationale: we capture the
  // ids of every emitted entity so `refresh_sketch_projections` can
  // re-derive them from the upstream edge on every recompute.
  SketchProjection record;
  record.id =
      "projection-" + std::to_string(next_sketch_projection_id_++);
  record.source_id = edge_id;
  record.source_kind = "edge";

  if (edge_geometry->kind == "line") {
    const auto a = world_to_sketch_local(frame,
                                         edge_geometry->start.x,
                                         edge_geometry->start.y,
                                         edge_geometry->start.z);
    const auto b = world_to_sketch_local(frame,
                                         edge_geometry->end.x,
                                         edge_geometry->end.y,
                                         edge_geometry->end.z);
    // Reject zero-length projections — happens when the edge runs
    // perpendicular to the sketch plane and collapses to a point.
    // v1 leaves these alone rather than generating a degenerate
    // line. Vertex projection covers the "I want a point at the
    // collapsed location" intent.
    const double dx = b.first - a.first;
    const double dy = b.second - a.second;
    if (dx * dx + dy * dy < 1e-12) {
      throw std::runtime_error(
          "Edge projects to a zero-length segment (perpendicular to "
          "sketch plane). Project the endpoint as a vertex instead.");
    }

    const size_t lines_before = sketch_it->sketch_parameters->lines.size();
    polysmith::core::add_sketch_line(*sketch_it,
                                     next_sketch_line_id_++,
                                     a.first,
                                     a.second,
                                     b.first,
                                     b.second);
    // Lock both endpoints — projected geometry is derived.
    for (size_t i = lines_before;
         i < sketch_it->sketch_parameters->lines.size(); ++i) {
      const auto& line = sketch_it->sketch_parameters->lines[i];
      polysmith::core::set_sketch_point_fixed(
          *sketch_it, line.start_point_id, true);
      polysmith::core::set_sketch_point_fixed(
          *sketch_it, line.end_point_id, true);
      record.generated_line_ids.push_back(line.id);
    }
  } else if (edge_geometry->kind == "circle" || edge_geometry->kind == "arc") {
    if (!circle_axis_parallel_to_sketch(frame, edge_geometry->axis)) {
      throw std::runtime_error(
          "Curved edge projects to an ellipse (its plane isn't parallel "
          "to the sketch). Not supported in v1.");
    }

    const auto center =
        world_to_sketch_local(frame,
                              edge_geometry->center.x,
                              edge_geometry->center.y,
                              edge_geometry->center.z);

    if (edge_geometry->kind == "circle") {
      const size_t circles_before =
          sketch_it->sketch_parameters->circles.size();
      polysmith::core::add_sketch_circle(*sketch_it,
                                         next_sketch_circle_id_++,
                                         center.first,
                                         center.second,
                                         edge_geometry->radius);
      for (size_t i = circles_before;
           i < sketch_it->sketch_parameters->circles.size(); ++i) {
        record.generated_circle_ids.push_back(
            sketch_it->sketch_parameters->circles[i].id);
      }
    } else {
      const auto start = world_to_sketch_local(frame,
                                               edge_geometry->start.x,
                                               edge_geometry->start.y,
                                               edge_geometry->start.z);
      const auto end = world_to_sketch_local(frame,
                                             edge_geometry->end.x,
                                             edge_geometry->end.y,
                                             edge_geometry->end.z);
      // Determine winding by checking whether the body axis points
      // in the same direction as the sketch normal. When parallel,
      // the body's natural CCW direction matches the sketch's CCW
      // direction; when anti-parallel, it's flipped.
      const double dot = edge_geometry->axis.x * frame.normal_x +
                         edge_geometry->axis.y * frame.normal_y +
                         edge_geometry->axis.z * frame.normal_z;
      const bool ccw = dot > 0.0;
      const int start_point_index = next_sketch_line_id_++;
      const int end_point_index = next_sketch_line_id_++;
      const size_t arcs_before = sketch_it->sketch_parameters->arcs.size();
      polysmith::core::add_sketch_arc(*sketch_it,
                                      next_sketch_arc_id_++,
                                      start_point_index,
                                      end_point_index,
                                      start.first,
                                      start.second,
                                      end.first,
                                      end.second,
                                      center.first,
                                      center.second,
                                      edge_geometry->radius,
                                      ccw);
      for (size_t i = arcs_before;
           i < sketch_it->sketch_parameters->arcs.size(); ++i) {
        record.generated_arc_ids.push_back(
            sketch_it->sketch_parameters->arcs[i].id);
      }
    }
  } else {
    // "unsupported" — propagate as a controlled error. Caller (the
    // UI) surfaces it as a transient toast.
    throw std::runtime_error(
        "Edge type is not supported by the Project tool yet: " + edge_id);
  }

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

DocumentState DocumentManager::project_vertex_into_sketch(
    const std::string& vertex_id) {
  require_document();
  const auto sketch_it = require_projection_target(*document_);

  // Idempotency: same dedup index that face / edge projection uses.
  for (const auto& projection :
       sketch_it->sketch_parameters->projections) {
    if (projection.source_id == vertex_id) {
      return document_.value();
    }
  }

  const auto position = compute_vertex_position(*document_, vertex_id);
  if (!position.has_value()) {
    throw std::runtime_error(
        "Vertex could not be resolved against the current document: " +
        vertex_id);
  }

  const auto& frame = sketch_it->sketch_parameters->plane_frame.value();
  const auto local = world_to_sketch_local(frame,
                                           position->x,
                                           position->y,
                                           position->z);

  push_undo_state();
  clear_redo_stack();

  const std::string point_id = "projected-point-" +
                               std::to_string(next_sketch_projected_point_id_++);
  sketch_it->sketch_parameters->projected_points.push_back(
      SketchProjectedPoint{
          .id = point_id,
          .source_id = vertex_id,
          .x = local.first,
          .y = local.second,
      });

  // Live-link record. Same shape as face / edge but only points to a
  // single `SketchProjectedPoint`; `refresh_sketch_projections` uses
  // `generated_point_id` to find and patch the cached (x, y).
  SketchProjection projection_record;
  projection_record.id =
      "projection-" + std::to_string(next_sketch_projection_id_++);
  projection_record.source_id = vertex_id;
  projection_record.source_kind = "vertex";
  projection_record.generated_point_id = point_id;
  sketch_it->sketch_parameters->projections.push_back(
      std::move(projection_record));

  // Force the points list to pick up the new projected point in this
  // frame. (`refresh_linked_extrudes` won't run sketch derived state
  // for us — it only refreshes downstream features.)
  refresh_sketch_derived_state(*sketch_it);
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

DocumentState DocumentManager::create_offset_plane(
    const std::string& source_plane_id, double offset) {
  require_document();

  // The source must be a real plane that exists in the current
  // document. We allow the three origin planes, any earlier
  // construction plane, any sketch profile id, and any planar body
  // face id (face ids of the form "<body_id>:face:<index>"). The
  // validity check is intentionally lighter than `select_face`'s —
  // `resolve_plane_source_frame` returns nullopt for unknown /
  // non-planar / missing sources, so the `frame.has_value()` guard
  // below catches every failure mode in one place.
  const auto frame =
      resolve_plane_source_frame(*document_, source_plane_id);
  if (!frame.has_value()) {
    throw std::runtime_error(
        "Offset plane source not found: " + source_plane_id);
  }

  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(create_construction_plane_feature(
      next_feature_id_++, source_plane_id, offset, frame.value()));
  const std::string feature_id = document_->feature_history.back().id;
  // Select the new plane both as a feature (for the hierarchy /
  // timeline) and as a reference (for the viewport quad highlight
  // and the Sketch button gate). Origin-plane selection only sets
  // selected_reference_id, but construction planes are *both* a
  // feature and a reference, so we keep both in sync.
  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = feature_id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
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

DocumentState DocumentManager::create_midplane(
    const std::string& first_source_id,
    const std::string& second_source_id) {
  require_document();
  const auto first_frame =
      resolve_plane_source_frame(*document_, first_source_id);
  const auto second_frame =
      resolve_plane_source_frame(*document_, second_source_id);
  if (!first_frame.has_value() || !second_frame.has_value()) {
    throw std::runtime_error("Midplane source not found");
  }

  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(create_midplane_feature(
      next_feature_id_++,
      first_source_id,
      second_source_id,
      first_frame.value(),
      second_frame.value()));
  const std::string feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = feature_id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
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

DocumentState DocumentManager::create_tangent_plane(
    const std::string& source_face_id) {
  require_document();
  const auto tangent_frame =
      resolve_tangent_plane_source_frame(*document_, source_face_id);
  if (!tangent_frame.has_value()) {
    throw std::runtime_error("Tangent plane source face not found");
  }

  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(create_tangent_plane_feature(
      next_feature_id_++, source_face_id, tangent_frame.value()));
  const std::string feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = feature_id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
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

DocumentState DocumentManager::create_angle_plane(
    const std::string& source_plane_id,
    const std::string& source_axis_id,
    double angle_degrees) {
  require_document();
  const auto source_frame =
      resolve_plane_source_frame(*document_, source_plane_id);
  const auto axis =
      resolve_angle_plane_axis(*document_, source_axis_id);
  if (!source_frame.has_value()) {
    throw std::runtime_error("Angle plane source not found");
  }
  if (!axis.has_value()) {
    throw std::runtime_error("Angle plane axis not found or not linear");
  }

  push_undo_state();
  clear_redo_stack();

  document_->feature_history.push_back(create_angle_plane_feature(
      next_feature_id_++,
      source_plane_id,
      source_axis_id,
      angle_degrees,
      source_frame.value(),
      axis.value()));
  const std::string feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = feature_id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
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

DocumentState DocumentManager::create_construction_axis(
    const std::string& source_id) {
  require_document();
  const auto axis = resolve_construction_axis_source(*document_, source_id);
  if (!axis.has_value()) {
    throw std::runtime_error(
        "Construction axis source not found or not linear: " + source_id);
  }

  push_undo_state();
  clear_redo_stack();

  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "construction_axis";
  feature.name = "Axis";
  feature.status = "healthy";
  feature.parameters_summary =
      axis->source_kind == "edge" ? "From edge" : "From sketch line";
  feature.construction_axis_parameters = axis.value();
  document_->feature_history.push_back(feature);

  const std::string feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
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

DocumentState DocumentManager::create_construction_point(
    const std::string& source_id) {
  require_document();
  const auto point = resolve_construction_point_source(*document_, source_id);
  if (!point.has_value()) {
    throw std::runtime_error(
        "Construction point source not found: " + source_id);
  }

  push_undo_state();
  clear_redo_stack();

  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "construction_point";
  feature.name = "Point";
  feature.status = "healthy";
  feature.parameters_summary =
      point->source_kind == "vertex" ? "From vertex" : "From sketch point";
  feature.construction_point_parameters = point.value();
  document_->feature_history.push_back(feature);

  const std::string feature_id = document_->feature_history.back().id;
  document_->selected_feature_id = feature_id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
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

DocumentState DocumentManager::create_hole(
    const std::string& face_id,
    double center_x,
    double center_y,
    double center_z,
    const HoleFeatureParameters& parameters) {
  require_document();
  const auto frame = resolve_plane_source_frame(*document_, face_id);
  if (!frame.has_value()) {
    throw std::runtime_error("Hole requires a planar face source");
  }
  const auto face_marker = face_id.find(":face:");
  if (face_marker == std::string::npos || face_marker == 0) {
    throw std::runtime_error("Hole requires a body face id");
  }

  HoleFeatureParameters next = parameters;
  next.target_body_id = face_id.substr(0, face_marker);
  next.source_face_id = face_id;
  next.plane_frame = frame.value();
  const double dx = center_x - frame->origin_x;
  const double dy = center_y - frame->origin_y;
  const double dz = center_z - frame->origin_z;
  next.center_x = dx * frame->x_axis_x + dy * frame->x_axis_y + dz * frame->x_axis_z;
  next.center_y = dx * frame->y_axis_x + dy * frame->y_axis_y + dz * frame->y_axis_z;
  next.is_pending = true;
  if (next.thread_enabled && next.thread_depth <= 0.0) {
    next.thread_depth = next.depth;
  }

  push_undo_state();
  clear_redo_stack();

  FeatureEntry feature{};
  feature.id = "feature-" + std::to_string(next_feature_id_++);
  feature.kind = "hole";
  feature.name = "Hole";
  feature.status = "healthy";
  feature.parameters_summary = next.hole_type + " · " +
                               std::to_string(next.diameter) + " mm";
  feature.hole_parameters = next;
  document_->feature_history.push_back(feature);
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
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

DocumentState DocumentManager::update_hole_parameters(
    const std::string& feature_id,
    const HoleFeatureParameters& parameters) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "hole" ||
      !feature_it->hole_parameters.has_value()) {
    throw std::runtime_error("update_hole_parameters requires a hole feature");
  }
  HoleFeatureParameters next = parameters;
  next.target_body_id = feature_it->hole_parameters->target_body_id;
  next.source_face_id = feature_it->hole_parameters->source_face_id;
  next.plane_frame = feature_it->hole_parameters->plane_frame;
  next.center_x = feature_it->hole_parameters->center_x;
  next.center_y = feature_it->hole_parameters->center_y;
  next.is_pending = feature_it->hole_parameters->is_pending;
  feature_it->hole_parameters = next;
  feature_it->parameters_summary = next.hole_type + " · " +
                                   std::to_string(next.diameter) + " mm";
  feature_it->status = "healthy";
  feature_it->dependency_broken = false;
  feature_it->dependency_warning.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::confirm_hole(const std::string& feature_id) {
  require_document();
  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "hole" ||
      !feature_it->hole_parameters.has_value()) {
    throw std::runtime_error("confirm_hole requires a hole feature");
  }
  feature_it->hole_parameters->is_pending = false;
  bump_geometry_revision();
  return document_.value();
}

namespace {

std::vector<double> make_helix_points(const ConstructionAxisFeatureParameters& axis,
                                      const HelixFeatureParameters& params) {
  const double ax = axis.end_x - axis.start_x;
  const double ay = axis.end_y - axis.start_y;
  const double az = axis.end_z - axis.start_z;
  const double length = std::sqrt(ax * ax + ay * ay + az * az);
  if (length <= 1.0e-9 || params.radius <= 0.0 || params.pitch <= 0.0 ||
      params.height <= 0.0) {
    return {};
  }
  const std::array<double, 3> dir{ax / length, ay / length, az / length};
  std::array<double, 3> seed =
      std::abs(dir[0]) < 0.9 ? std::array<double, 3>{1.0, 0.0, 0.0}
                             : std::array<double, 3>{0.0, 1.0, 0.0};
  std::array<double, 3> u{
      dir[1] * seed[2] - dir[2] * seed[1],
      dir[2] * seed[0] - dir[0] * seed[2],
      dir[0] * seed[1] - dir[1] * seed[0],
  };
  const double u_len = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
  if (u_len <= 1.0e-9) {
    return {};
  }
  u = {u[0] / u_len, u[1] / u_len, u[2] / u_len};
  const std::array<double, 3> v{
      dir[1] * u[2] - dir[2] * u[1],
      dir[2] * u[0] - dir[0] * u[2],
      dir[0] * u[1] - dir[1] * u[0],
  };
  const double turns = params.height / params.pitch;
  const int samples = std::max(24, static_cast<int>(std::ceil(turns * 48.0)));
  const double handed = params.handedness == "left" ? -1.0 : 1.0;
  const double start = params.start_angle_degrees * kPi / 180.0;
  std::vector<double> points;
  points.reserve(static_cast<size_t>(samples + 1) * 3);
  for (int i = 0; i <= samples; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(samples);
    const double along = params.height * t;
    const double angle = start + handed * 2.0 * kPi * turns * t;
    const double radial_u = std::cos(angle) * params.radius;
    const double radial_v = std::sin(angle) * params.radius;
    points.push_back(axis.start_x + dir[0] * along + u[0] * radial_u + v[0] * radial_v);
    points.push_back(axis.start_y + dir[1] * along + u[1] * radial_u + v[1] * radial_v);
    points.push_back(axis.start_z + dir[2] * along + u[2] * radial_u + v[2] * radial_v);
  }
  return points;
}

}  // namespace
