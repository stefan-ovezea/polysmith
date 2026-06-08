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
