// Find a sketch line by id and patch its endpoint coords. The mutator
// updates the cached (start_x, start_y) / (end_x, end_y); the points
// table picks up the new coords via the next
// `refresh_sketch_derived_state` pass.
bool patch_sketch_line(SketchFeatureParameters& parameters,
                       const std::string& line_id,
                       double start_x, double start_y,
                       double end_x, double end_y) {
  for (auto& line : parameters.lines) {
    if (line.id == line_id) {
      line.start_x = start_x;
      line.start_y = start_y;
      line.end_x = end_x;
      line.end_y = end_y;
      return true;
    }
  }
  return false;
}

bool patch_sketch_circle(SketchFeatureParameters& parameters,
                         const std::string& circle_id,
                         double center_x, double center_y, double radius) {
  for (auto& circle : parameters.circles) {
    if (circle.id == circle_id) {
      circle.center_x = center_x;
      circle.center_y = center_y;
      circle.radius = radius;
      return true;
    }
  }
  return false;
}

bool patch_sketch_arc(SketchFeatureParameters& parameters,
                      const std::string& arc_id,
                      double start_x, double start_y,
                      double end_x, double end_y,
                      double center_x, double center_y,
                      double radius, bool ccw) {
  for (auto& arc : parameters.arcs) {
    if (arc.id == arc_id) {
      arc.start_x = start_x;
      arc.start_y = start_y;
      arc.end_x = end_x;
      arc.end_y = end_y;
      arc.center_x = center_x;
      arc.center_y = center_y;
      arc.radius = radius;
      arc.ccw = ccw;
      return true;
    }
  }
  return false;
}

bool patch_projected_point(SketchFeatureParameters& parameters,
                           const std::string& point_id,
                           double x, double y) {
  for (auto& projected : parameters.projected_points) {
    if (projected.id == point_id) {
      projected.x = x;
      projected.y = y;
      return true;
    }
  }
  return false;
}
