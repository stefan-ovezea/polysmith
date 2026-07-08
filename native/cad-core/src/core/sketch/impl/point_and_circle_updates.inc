void set_sketch_coincident_constraint(FeatureEntry& feature,
                                      const std::string& point_id,
                                      const std::string& other_point_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set coincident constraints");
  }

  auto& parameters = feature.sketch_parameters.value();

  if (point_id == other_point_id) {
    return;
  }

  const auto point_position = find_point_position(parameters, point_id);
  if (!point_position.has_value()) {
    throw std::runtime_error("Sketch point not found: " + point_id);
  }

  const auto other_point_position = find_point_position(parameters, other_point_id);
  if (!other_point_position.has_value()) {
    throw std::runtime_error("Sketch point not found: " + other_point_id);
  }

  for (const auto& line : parameters.lines) {
    const bool spans_points =
        (line.start_point_id == point_id && line.end_point_id == other_point_id) ||
        (line.start_point_id == other_point_id && line.end_point_id == point_id);
    if (spans_points) {
      throw std::runtime_error(
          "Cannot make both endpoints of a sketch line coincident");
    }
  }

  auto affected_line_ids = collect_line_ids_for_point(parameters, point_id);
  const auto other_line_ids =
      collect_line_ids_for_point(parameters, other_point_id);
  for (const auto& line_id : other_line_ids) {
    if (std::find(affected_line_ids.begin(), affected_line_ids.end(), line_id) ==
        affected_line_ids.end()) {
      affected_line_ids.push_back(line_id);
    }
  }

  // Only move the point if the two points are not already coincident.
  // When the user bonds two points that already sit at the same position
  // (e.g. two lines drawn end-to-end without a constraint), we skip the
  // move and just merge the point IDs.
  if (!points_match(std::get<0>(point_position.value()),
                    std::get<1>(point_position.value()),
                    std::get<0>(other_point_position.value()),
                    std::get<1>(other_point_position.value()))) {
    propagate_connected_point_move(parameters,
                                   point_id,
                                   std::get<0>(other_point_position.value()),
                                   std::get<1>(other_point_position.value()));
  }
  replace_point_id_references(parameters, point_id, other_point_id);
  sync_all_line_dimensions(parameters);

  for (const auto& line_id : affected_line_ids) {
    enforce_equal_length_relations(parameters, line_id);
    enforce_perpendicular_relations(parameters, line_id);
    enforce_parallel_relations(parameters, line_id);
  }

  sync_all_line_dimensions(parameters);
  refresh_sketch_derived_state(feature);

  // Store an explicit constraint entry so the viewport can render a
  // badge and the DOF counter accounts for the user-applied constraint.
  // target_ids lists the lines that share this coincident point.
  {
    bool already_exists = false;
    for (const auto& c : parameters.constraints) {
      if (c.kind == "coincident" && c.target_ids.size() == affected_line_ids.size()) {
        bool match = true;
        for (size_t i = 0; i < affected_line_ids.size(); ++i) {
          if (c.target_ids[i] != affected_line_ids[i]) { match = false; break; }
        }
        if (match) { already_exists = true; break; }
      }
    }
    if (!already_exists && !affected_line_ids.empty()) {
      parameters.constraints.push_back(SketchConstraint{
          .constraint_id = "constraint-coincident-" + other_point_id,
          .kind = "coincident",
          .target_ids = affected_line_ids,
      });
    }
  }
}

void delete_sketch_coincident_constraint(FeatureEntry& feature,
                                         const std::string& constraint_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can delete coincident constraints");
  }

  auto& parameters = feature.sketch_parameters.value();

  // Extract the merged point ID from the constraint_id pattern:
  // "constraint-coincident-<point_id>"
  const std::string prefix = "constraint-coincident-";
  if (constraint_id.size() <= prefix.size() ||
      constraint_id.substr(0, prefix.size()) != prefix) {
    throw std::runtime_error("Not a coincident constraint: " + constraint_id);
  }
  const std::string merged_point_id = constraint_id.substr(prefix.size());

  // Remove the constraint entry.
  const auto constraint_it = std::find_if(
      parameters.constraints.begin(), parameters.constraints.end(),
      [&](const SketchConstraint& c) { return c.constraint_id == constraint_id; });
  if (constraint_it == parameters.constraints.end()) {
    throw std::runtime_error("Coincident constraint not found: " + constraint_id);
  }
  parameters.constraints.erase(constraint_it);

  // Find all lines sharing the merged point.
  std::vector<SketchLine*> affected_lines;
  for (auto& line : parameters.lines) {
    if (line.start_point_id == merged_point_id ||
        line.end_point_id == merged_point_id) {
      affected_lines.push_back(&line);
    }
  }

  // Assign fresh unique point IDs to all but the first line so each
  // can move independently. The first line keeps the original point ID.
  if (affected_lines.size() > 1) {
    int fresh_counter = static_cast<int>(parameters.lines.size()) * 100;
    for (size_t i = 1; i < affected_lines.size(); ++i) {
      const std::string new_id =
          "vertex-" + std::to_string(parameters.next_vertex_index++);
      auto* line = affected_lines[i];
      if (line->start_point_id == merged_point_id) {
        line->start_point_id = new_id;
      }
      if (line->end_point_id == merged_point_id) {
        line->end_point_id = new_id;
      }
    }
  }

  refresh_sketch_derived_state(feature);
}

void delete_sketch_mirror_relation(FeatureEntry& feature,
                                    const std::string& relation_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can delete mirror relations");
  }
  auto& parameters = feature.sketch_parameters.value();

  // Find the relation.
  const auto it = std::find_if(
      parameters.mirror_relations.begin(), parameters.mirror_relations.end(),
      [&](const SketchMirrorRelation& rel) { return rel.id == relation_id; });
  if (it == parameters.mirror_relations.end()) return;

  // Un-mark the mirrored entity's dimensions as driven.
  for (auto& dim : parameters.dimensions) {
    if (dim.entity_id == it->mirror_id) {
      dim.driven = false;
    }
  }

  parameters.mirror_relations.erase(it);
  refresh_sketch_derived_state(feature);
}

void set_sketch_point_fixed(FeatureEntry& feature,
                            const std::string& point_id,
                            bool is_fixed) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set fixed point state");
  }

  auto& parameters = feature.sketch_parameters.value();
  SketchVertex* point = find_sketch_point(parameters, point_id);
  if (point == nullptr) {
    throw std::runtime_error("Sketch point not found: " + point_id);
  }

  point->is_fixed = is_fixed;
  refresh_sketch_derived_state(feature);
}

void update_sketch_circle(FeatureEntry& feature,
                          const std::string& circle_id,
                          double center_x,
                          double center_y,
                          double radius) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch circles");
  }

  if (radius <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch circles must have non-zero radius");
  }

  const auto circle_it = std::find_if(
      feature.sketch_parameters->circles.begin(),
      feature.sketch_parameters->circles.end(),
      [&](const SketchCircle& circle) { return circle.id == circle_id; });

  if (circle_it == feature.sketch_parameters->circles.end()) {
    throw std::runtime_error("Sketch circle not found: " + circle_id);
  }

  const std::string center_point_id = circle_center_point_id(*circle_it);
  if (!point_is_fixed(*feature.sketch_parameters, center_point_id)) {
    circle_it->center_x = center_x;
    circle_it->center_y = center_y;
  }
  circle_it->radius = radius;
  sync_circle_dimension(*feature.sketch_parameters, *circle_it);
  refresh_sketch_derived_state(feature);
}
