SketchPoint* find_sketch_point(SketchFeatureParameters& parameters,
                               const std::string& point_id) {
  const auto point_it = std::find_if(
      parameters.points.begin(),
      parameters.points.end(),
      [&](const SketchPoint& point) { return point.id == point_id; });
  return point_it == parameters.points.end() ? nullptr : &(*point_it);
}

const SketchPoint* find_sketch_point(const SketchFeatureParameters& parameters,
                                     const std::string& point_id) {
  const auto point_it = std::find_if(
      parameters.points.begin(),
      parameters.points.end(),
      [&](const SketchPoint& point) { return point.id == point_id; });
  return point_it == parameters.points.end() ? nullptr : &(*point_it);
}

bool point_is_fixed(const SketchFeatureParameters& parameters,
                    const std::string& point_id) {
  const SketchPoint* point = find_sketch_point(parameters, point_id);
  return point != nullptr && point->is_fixed;
}

// True iff `point_id` is anchored to a host line (midpoint or
// parametric). Used by the H/V rigid-translation branch in
// `propagate_connected_point_move` to suppress its
// length-preserving move of the *other* endpoint when that other
// endpoint is itself going to be re-pulled by an anchor — without
// this guard, the rigid translation overrides the anchor's target
// position and the line keeps its old length, drifting out of the
// host geometry (see the rectangle midpoint-line repro).
bool point_is_anchored_to_line(const SketchFeatureParameters& parameters,
                               const std::string& point_id) {
  for (const auto& anchor : parameters.midpoint_anchors) {
    if (anchor.point_id == point_id) {
      return true;
    }
  }
  for (const auto& anchor : parameters.point_line_anchors) {
    if (anchor.point_id == point_id) {
      return true;
    }
  }
  return false;
}
