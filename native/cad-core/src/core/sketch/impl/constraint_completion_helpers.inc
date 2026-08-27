// Shared helpers for the constraint-completion commands and the
// ad-hoc enforcement passes.  Included in both the anonymous helper
// namespace and the enclosing polysmith::core scope so both callers
// see the same functions.
// True when the pair (first_id, second_id) already has a constraint of
// the given kind in either target order.
bool pair_constraint_exists(const SketchFeatureParameters& parameters,
                            const std::string& kind,
                            const std::string& first_id,
                            const std::string& second_id) {
  for (const auto& c : parameters.constraints) {
    if (c.kind != kind || c.target_ids.size() != 2) continue;
    if ((c.target_ids[0] == first_id && c.target_ids[1] == second_id) ||
        (c.target_ids[0] == second_id && c.target_ids[1] == first_id)) {
      return true;
    }
  }
  return false;
}

bool relation_exists(const SketchFeatureParameters& parameters,
                     const std::string& kind,
                     const std::string& first_id,
                     const std::string& second_id) {
  for (const auto& r : parameters.line_relations) {
    if (r.kind != kind) continue;
    if ((r.first_line_id == first_id && r.second_line_id == second_id) ||
        (r.first_line_id == second_id && r.second_line_id == first_id)) {
      return true;
    }
  }
  return false;
}

// Resolves an entity id to its (center, radius, kind) triple where kind
// is "circle" or "arc".  Returns nullopt when the id is neither.
std::optional<std::tuple<double, double, double, std::string>>
curve_center_radius(const SketchFeatureParameters& parameters,
                    const std::string& entity_id) {
  for (const auto& circle : parameters.circles) {
    if (circle.id == entity_id) {
      return std::tuple<double, double, double, std::string>{
          circle.center_x, circle.center_y, circle.radius, "circle"};
    }
  }
  for (const auto& arc : parameters.arcs) {
    if (arc.id == entity_id) {
      return std::tuple<double, double, double, std::string>{
          arc.center_x, arc.center_y, arc.radius, "arc"};
    }
  }
  return std::nullopt;
}

// Resolves a point id to its position, reading the owning entity cache
// first (the vertex table may be stale between rebuilds).
std::optional<std::pair<double, double>> point_position_of(
    const SketchFeatureParameters& parameters, const std::string& point_id) {
  for (const auto& line : parameters.lines) {
    if (line.start_vertex_id == point_id) return std::pair{line.start_x, line.start_y};
    if (line.end_vertex_id == point_id) return std::pair{line.end_x, line.end_y};
  }
  for (const auto& v : parameters.vertices) {
    if (v.id == point_id) return std::pair{v.x, v.y};
  }
  return std::nullopt;
}

bool vertex_is_fixed_id(const SketchFeatureParameters& parameters,
                        const std::string& point_id) {
  for (const auto& v : parameters.vertices) {
    if (v.id == point_id) return v.is_fixed;
  }
  return false;
}

std::string next_constraint_id(SketchFeatureParameters& parameters,
                               const std::string& prefix) {
  int max_index = 0;
  for (const auto& c : parameters.constraints) {
    const std::string marker = prefix;
    if (c.constraint_id.rfind(marker, 0) == 0) {
      const std::string tail = c.constraint_id.substr(marker.size());
      try {
        max_index = std::max(max_index, std::stoi(tail));
      } catch (...) {
      }
    }
  }
  return prefix + std::to_string(max_index + 1);
}
