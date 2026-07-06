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
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();

  return document_.value();
}
