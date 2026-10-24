// True when an axis constraint is already implied by the stored relations, so
// storing it would add a REDUNDANT constraint.
//
// The axis propagates along the relation graph: two PARALLEL lines share an axis,
// two PERPENDICULAR lines have swapped axes. The walk stops at any line that
// declares an axis constraint — that is the statement the chain is measured
// against — so for a rectangle, whose bottom side is horizontal:
//
//   horizontal(bottom) + parallel(top, bottom)      ⇒ horizontal(top)
//   horizontal(bottom) + perpendicular(bottom, right) ⇒ vertical(right)
//   …and vertical(right) + parallel(left, right)    ⇒ vertical(left)
//
// A redundant constraint is not harmless: planegcs' diagnose() then reports a
// degree count that is not the sketch's (a rectangle side read 1 instead of 4)
// and its redundancy repair can move geometry nobody asked to move. The relation
// glyphs already tell the user why the side is axis-aligned, so skipping the
// duplicate costs nothing visually.
bool axis_constraint_is_implied(const SketchFeatureParameters& parameters,
                               const std::string& line_id,
                               const std::string& constraint) {
  if (constraint != "horizontal" && constraint != "vertical") {
    return false;
  }

  std::deque<std::pair<std::string, std::string>> frontier;
  std::vector<std::string> visited;
  frontier.push_back({line_id, constraint});

  while (!frontier.empty()) {
    const auto current = frontier.front();
    frontier.pop_front();
    if (std::find(visited.begin(), visited.end(), current.first) !=
        visited.end()) {
      continue;
    }
    visited.push_back(current.first);

    for (const auto& relation : parameters.line_relations) {
      if (relation.kind != "parallel" && relation.kind != "perpendicular") {
        continue;
      }
      const bool is_first = relation.first_line_id == current.first;
      const bool is_second = relation.second_line_id == current.first;
      if (!is_first && !is_second) {
        continue;
      }
      const std::string& neighbour_id =
          is_first ? relation.second_line_id : relation.first_line_id;
      const std::string neighbour_axis =
          relation.kind == "parallel"
              ? current.second
              : (current.second == "horizontal" ? "vertical" : "horizontal");

      const auto neighbour_it = std::find_if(
          parameters.lines.begin(), parameters.lines.end(),
          [&](const SketchLine& line) { return line.id == neighbour_id; });
      if (neighbour_it == parameters.lines.end()) {
        continue;
      }
      if (neighbour_it->constraint.has_value()) {
        if (neighbour_it->constraint.value() == neighbour_axis) {
          return true;
        }
        continue;
      }
      frontier.push_back({neighbour_id, neighbour_axis});
    }
  }
  return false;
}

void apply_line_constraint(SketchLine& line) {
  if (!line.constraint.has_value()) {
    return;
  }

  if (line.constraint.value() == "horizontal") {
    line.end_y = line.start_y;
    return;
  }

  if (line.constraint.value() == "vertical") {
    line.end_x = line.start_x;
  }
}

void apply_line_constraint_respecting_fixed_points(
    const SketchFeatureParameters& parameters,
    SketchLine& line) {
  if (!line.constraint.has_value()) {
    return;
  }

  const bool start_fixed = vertex_is_fixed(parameters, line.start_vertex_id);
  const bool end_fixed = vertex_is_fixed(parameters, line.end_vertex_id);

  if (line.constraint.value() == "horizontal") {
    if (start_fixed && end_fixed && !nearly_equal(line.start_y, line.end_y)) {
      throw std::runtime_error(
          "Cannot make a line horizontal when both endpoints are fixed");
    }
    // Changing an already-vertical line to horizontal collapses it to
    // zero length (X is already equal, now Y becomes equal too).  Clear
    // the constraint instead — same safeguard as enforce_line_HV_constraints.
    if (nearly_equal(line.start_x, line.end_x)) {
      polysmith::core::log_warn(
          "sketch", "cleared horizontal constraint on " + line.id +
          " (would create zero-length line)");
      line.constraint = std::nullopt;
      return;
    }
    if (end_fixed) {
      line.start_y = line.end_y;
    } else {
      line.end_y = line.start_y;
    }
    return;
  }

  if (line.constraint.value() == "vertical") {
    if (start_fixed && end_fixed && !nearly_equal(line.start_x, line.end_x)) {
      throw std::runtime_error(
          "Cannot make a line vertical when both endpoints are fixed");
    }
    if (nearly_equal(line.start_y, line.end_y)) {
      polysmith::core::log_warn(
          "sketch", "cleared vertical constraint on " + line.id +
          " (would create zero-length line)");
      line.constraint = std::nullopt;
      return;
    }
    if (end_fixed) {
      line.start_x = line.end_x;
    } else {
      line.end_x = line.start_x;
    }
  }
}
