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
      polysmith::core::set_sketch_vertex_fixed(
          *sketch_it, line.start_vertex_id, true);
      polysmith::core::set_sketch_vertex_fixed(
          *sketch_it, line.end_vertex_id, true);
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
      polysmith::core::set_sketch_vertex_fixed(
          *sketch_it, circle_center_vertex_id(circle), true);
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
