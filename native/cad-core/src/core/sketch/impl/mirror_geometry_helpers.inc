std::pair<double, double> reflect_point_across_line(double px, double py,
                                                    double ax, double ay,
                                                    double bx, double by) {
  const double dx = bx - ax;
  const double dy = by - ay;
  const double len_sq = dx * dx + dy * dy;
  // Defensive: zero-length axis can't reflect. Return the source
  // unchanged so callers get a benign no-op rather than NaNs.
  if (len_sq <= 0.0) {
    return {px, py};
  }
  const double t = ((px - ax) * dx + (py - ay) * dy) / len_sq;
  const double foot_x = ax + t * dx;
  const double foot_y = ay + t * dy;
  return {2.0 * foot_x - px, 2.0 * foot_y - py};
}

// Regenerate `pending_mirror.generated_lines/circles` from the
// current `axis_line_id` + `object_ids` selection. Called on every
// preview parameter change. Cheap to run: it just walks the
// object list, reflects each entity's geometry across the axis,
// and pushes a transient SketchLine / SketchCircle into the
// pending state. The transient ids use a `pending-mirror-...`
// prefix so they never collide with committed entities.
//
// If the axis is unset, isn't actually a line, or has zero length,
// the generated arrays are simply cleared (no preview to draw).
void regenerate_mirror_preview(SketchFeatureParameters &parameters) {
  if (!parameters.pending_mirror.has_value()) {
    return;
  }
  auto &pending = *parameters.pending_mirror;
  pending.generated_lines.clear();
  pending.generated_circles.clear();

  if (!pending.axis_line_id.has_value()) {
    return;
  }
  const auto &axis_id = *pending.axis_line_id;
  const auto axis_it =
      std::find_if(parameters.lines.begin(), parameters.lines.end(),
                   [&](const SketchLine &line) { return line.id == axis_id; });
  if (axis_it == parameters.lines.end()) {
    return;
  }
  const double ax = axis_it->start_x;
  const double ay = axis_it->start_y;
  const double bx = axis_it->end_x;
  const double by = axis_it->end_y;
  // Zero-length axis can't reflect; bail out so the user sees an
  // empty preview rather than a misleading copy.
  if ((bx - ax) * (bx - ax) + (by - ay) * (by - ay) <= 0.0) {
    return;
  }

  int local_line_counter = 0;
  int local_circle_counter = 0;
  for (const auto &object_id : pending.object_ids) {
    if (object_id == axis_id) {
      continue; // Axis to itself is a no-op.
    }
    const auto line_it = std::find_if(
        parameters.lines.begin(), parameters.lines.end(),
        [&](const SketchLine &line) { return line.id == object_id; });
    if (line_it != parameters.lines.end()) {
      const auto [new_start_x, new_start_y] = reflect_point_across_line(
          line_it->start_x, line_it->start_y, ax, ay, bx, by);
      const auto [new_end_x, new_end_y] = reflect_point_across_line(
          line_it->end_x, line_it->end_y, ax, ay, bx, by);
      SketchLine reflected{};
      const std::string suffix = std::to_string(local_line_counter++);
      reflected.id = "pending-mirror-line-" + suffix;
      reflected.start_vertex_id = reflected.id + "-start";
      reflected.end_vertex_id = reflected.id + "-end";
      reflected.start_x = new_start_x;
      reflected.start_y = new_start_y;
      reflected.end_x = new_end_x;
      reflected.end_y = new_end_y;
      reflected.constraint = std::nullopt;
      reflected.is_construction = line_it->is_construction;
      pending.generated_lines.push_back(reflected);
      continue;
    }
    const auto circle_it = std::find_if(
        parameters.circles.begin(), parameters.circles.end(),
        [&](const SketchCircle &circle) { return circle.id == object_id; });
    if (circle_it != parameters.circles.end()) {
      const auto [new_cx, new_cy] = reflect_point_across_line(
          circle_it->center_x, circle_it->center_y, ax, ay, bx, by);
      SketchCircle reflected{};
      reflected.id =
          "pending-mirror-circle-" + std::to_string(local_circle_counter++);
      reflected.center_x = new_cx;
      reflected.center_y = new_cy;
      reflected.radius = circle_it->radius;
      reflected.is_construction = circle_it->is_construction;
      pending.generated_circles.push_back(reflected);
      continue;
    }
    // Unknown id — silently skip. The UI may have stale selections
    // and we'd rather draw a partial preview than throw.
  }
}
