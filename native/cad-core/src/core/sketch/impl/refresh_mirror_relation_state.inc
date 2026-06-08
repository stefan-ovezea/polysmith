// Refresh persistent mirror relations: re-reflect source entities
// across their axis lines and update the mirror entity positions.
// Also mark mirrored entity dimensions as driven so the user can't
// edit them directly — they always follow the source.
for (const auto &rel : feature.sketch_parameters->mirror_relations) {
  // Find the axis line.
  const auto axis_it = std::find_if(
      feature.sketch_parameters->lines.begin(),
      feature.sketch_parameters->lines.end(),
      [&](const SketchLine &l) { return l.id == rel.axis_line_id; });
  if (axis_it == feature.sketch_parameters->lines.end())
    continue;
  const double ax = axis_it->start_x, ay = axis_it->start_y;
  const double bx = axis_it->end_x, by = axis_it->end_y;

  // Reflect a line source.
  auto line_src =
      std::find_if(feature.sketch_parameters->lines.begin(),
                   feature.sketch_parameters->lines.end(),
                   [&](const SketchLine &l) { return l.id == rel.source_id; });
  auto line_dst =
      std::find_if(feature.sketch_parameters->lines.begin(),
                   feature.sketch_parameters->lines.end(),
                   [&](const SketchLine &l) { return l.id == rel.mirror_id; });
  if (line_src != feature.sketch_parameters->lines.end() &&
      line_dst != feature.sketch_parameters->lines.end()) {
    const auto reflected_start = reflect_point_across_line(
        line_src->start_x, line_src->start_y, ax, ay, bx, by);
    const auto reflected_end = reflect_point_across_line(
        line_src->end_x, line_src->end_y, ax, ay, bx, by);
    line_dst->start_x = reflected_start.first;
    line_dst->start_y = reflected_start.second;
    line_dst->end_x = reflected_end.first;
    line_dst->end_y = reflected_end.second;
  }

  // Reflect a circle source.
  auto circle_src = std::find_if(
      feature.sketch_parameters->circles.begin(),
      feature.sketch_parameters->circles.end(),
      [&](const SketchCircle &c) { return c.id == rel.source_id; });
  auto circle_dst = std::find_if(
      feature.sketch_parameters->circles.begin(),
      feature.sketch_parameters->circles.end(),
      [&](const SketchCircle &c) { return c.id == rel.mirror_id; });
  if (circle_src != feature.sketch_parameters->circles.end() &&
      circle_dst != feature.sketch_parameters->circles.end()) {
    const auto reflected_center = reflect_point_across_line(
        circle_src->center_x, circle_src->center_y, ax, ay, bx, by);
    circle_dst->center_x = reflected_center.first;
    circle_dst->center_y = reflected_center.second;
    circle_dst->radius = circle_src->radius;
  }

  // Mark all dimensions on the mirror entity as driven so edits
  // are rejected — the value always comes from the source.
  for (auto &dim : feature.sketch_parameters->dimensions) {
    if (dim.entity_id == rel.mirror_id) {
      dim.driven = true;
    }
  }
}
