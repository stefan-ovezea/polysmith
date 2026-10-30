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
        (line.start_vertex_id == point_id && line.end_vertex_id == other_point_id) ||
        (line.start_vertex_id == other_point_id && line.end_vertex_id == point_id);
    if (spans_points) {
      throw std::runtime_error(
          "Cannot make both endpoints of a sketch line coincident");
    }
  }

  auto affected_line_ids = collect_line_ids_for_vertex(parameters, point_id);
  const auto other_line_ids =
      collect_line_ids_for_vertex(parameters, other_point_id);
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
      add_sketch_constraint(parameters, SketchConstraint{
                                           .constraint_id =
                                               "constraint-coincident-" +
                                               other_point_id,
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
    if (line.start_vertex_id == merged_point_id ||
        line.end_vertex_id == merged_point_id) {
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
      if (line->start_vertex_id == merged_point_id) {
        line->start_vertex_id = new_id;
      }
      if (line->end_vertex_id == merged_point_id) {
        line->end_vertex_id = new_id;
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

void set_sketch_vertex_fixed(FeatureEntry& feature,
                            const std::string& point_id,
                            bool is_fixed,
                            bool refresh_immediately) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set fixed point state");
  }

  auto& parameters = feature.sketch_parameters.value();
  SketchVertex* point = find_sketch_vertex(parameters, point_id);
  if (point == nullptr) {
    throw std::runtime_error("Sketch point not found: " + point_id);
  }

  point->is_fixed = is_fixed;
  // The viewport renders the "FIX" badge only for points with an
  // EXPLICIT fixed constraint record — the Fix tool stores one here.
  // Internally frozen points (trim split points, slot-generated
  // vertices, ellipse axis points) carry only is_fixed, which pins
  // them for the solver without cluttering the viewport.
  parameters.constraints.erase(
      std::remove_if(parameters.constraints.begin(),
                     parameters.constraints.end(),
                     [&](const SketchConstraint& c) {
                       if (c.kind != "fixed") return false;
                       return std::find(c.target_ids.begin(),
                                        c.target_ids.end(),
                                        point_id) != c.target_ids.end();
                     }),
      parameters.constraints.end());
  if (is_fixed) {
    SketchConstraint fixed;
    fixed.constraint_id = "constraint-fixed-" + point_id;
    fixed.kind = "fixed";
    fixed.target_ids = {point_id};
    add_sketch_constraint(parameters, std::move(fixed));
  }  if (refresh_immediately) {
    refresh_sketch_derived_state(feature);
  }
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

  const std::string center_point_id = circle_center_vertex_id(*circle_it);
  if (!vertex_is_fixed(*feature.sketch_parameters, center_point_id)) {
    circle_it->center_x = center_x;
    circle_it->center_y = center_y;
  }
  circle_it->radius = radius;
  sync_circle_dimension(*feature.sketch_parameters, *circle_it);

  auto& parameters = feature.sketch_parameters.value();

  // Collect the attached point ids (point_on_circle on this circle) and
  // project each one radially onto the NEW radius at its same angle. The
  // solver alone (P2PDistance with a frozen centre) minimises movement and —
  // when a driving line-length dimension also pulls on the point — drifts it
  // around the circle to a different angle. This one-time projection is the
  // resize counterpart of the centre-drag's one-time translate.
  std::vector<std::string> attached_ids;
  const double cx = circle_it->center_x;
  const double cy = circle_it->center_y;
  for (const auto& constraint : parameters.constraints) {
    if (constraint.kind != "point_on_circle") continue;
    if (constraint.target_ids.size() < 2) continue;
    if (constraint.target_ids[1] != circle_id) continue;
    const auto pos = point_position_of(parameters, constraint.target_ids[0]);
    if (!pos.has_value()) continue;
    const double dx = pos->first - cx;
    const double dy = pos->second - cy;
    const double old_radius = std::hypot(dx, dy);
    if (old_radius <= kMinimumSketchDimensionValue) continue;
    propagate_connected_point_move(parameters, constraint.target_ids[0],
                                   cx + radius * (dx / old_radius),
                                   cy + radius * (dy / old_radius));
    attached_ids.push_back(constraint.target_ids[0]);
  }

  // Dragging OVERRIDES the dimensions — the same rule the centre drag and the
  // free-end drag follow. The projection above has already changed the length
  // of every line attached to this circle, so re-measure now, while the far end
  // of each line is still where the user left it: a driving length dimension
  // then has nothing to enforce and cannot snap the line back to its old
  // length ("dragging from the perimeter the line snaps").
  sync_all_line_dimensions(parameters);

  // Freeze the centre AND the attached points during the solve. The centre is
  // authoritative (the caller set it explicitly), and the points were just
  // placed at their correct radial positions — freezing both stops DogLeg from
  // drifting either one (a driving line-length dimension would otherwise pull
  // the point around the circle, and a free centre would absorb part of the
  // radius correction). Mirrors the centre-drag freeze in update_sketch_vertex.
  std::vector<std::pair<std::string, bool>> saved_fixed;
  saved_fixed.reserve(1 + attached_ids.size());
  const auto save_fixed = [&](const std::string& id) {
    for (const auto& v : parameters.vertices) {
      if (v.id == id) { saved_fixed.push_back({id, v.is_fixed}); return; }
    }
  };
  const auto set_fixed = [&](const std::string& id, bool fixed) {
    for (auto& v : parameters.vertices) {
      if (v.id == id) { v.is_fixed = fixed; return; }
    }
  };
  save_fixed(center_point_id);
  for (const auto& id : attached_ids) save_fixed(id);
  set_fixed(center_point_id, true);
  for (const auto& id : attached_ids) set_fixed(id, true);
  refresh_sketch_derived_state(feature);
  for (const auto& [id, was_fixed] : saved_fixed) set_fixed(id, was_fixed);
}
