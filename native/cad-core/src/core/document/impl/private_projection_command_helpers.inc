// Guard against degenerate outlines (a face whose wire walk explodes
// into hundreds of thousands of corners). With bulk insertion the
// projection itself is near-linear, and the planegcs solver is
// skipped entirely for constraint-free projected sketches — so real
// mesh faces (a fan panel projects ~900 corners) complete in seconds
// and are far below this ceiling.
constexpr int kMaxProjectionOutlineCorners = 5000;

// Validate that the active sketch in `document` is set up to receive
// projected geometry: there must be an active sketch with a plane
// frame. Returns the matching FeatureEntry iterator on success and
// throws a runtime_error on every failure path so all three
// project_*_into_sketch methods share the same diagnostics.
std::vector<FeatureEntry>::iterator require_projection_target(
    DocumentState& document) {
  if (!document.active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }
  const auto sketch_it = std::find_if(
      document.feature_history.begin(),
      document.feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document.active_sketch_feature_id.value();
      });
  if (sketch_it == document.feature_history.end() ||
      !sketch_it->sketch_parameters.has_value() ||
      !sketch_it->sketch_parameters->plane_frame.has_value()) {
    throw std::runtime_error(
        "Active sketch does not have a plane frame for projection");
  }
  return sketch_it;
}

// Flatten a world-space point onto the sketch's local (u, v) frame.
// Identical maths to the lambda inside `project_face_into_sketch` —
// pulled out so edge / vertex projection can reuse it without
// duplicating the dot-product expansion.
std::pair<double, double> world_to_sketch_local(
    const SketchFeatureParameters::SketchPlaneFrame& frame,
    double wx, double wy, double wz) {
  const double dx = wx - frame.origin_x;
  const double dy = wy - frame.origin_y;
  const double dz = wz - frame.origin_z;
  const double sx =
      dx * frame.x_axis_x + dy * frame.x_axis_y + dz * frame.x_axis_z;
  const double sy =
      dx * frame.y_axis_x + dy * frame.y_axis_y + dz * frame.y_axis_z;
  return {sx, sy};
}

// Returns true when `axis` is parallel (or anti-parallel) to the
// sketch frame's normal. A circular edge whose plane is parallel to
// the sketch plane projects cleanly to a circle / arc; non-parallel
// circular edges would project to ellipses, which we don't support
// in v1 (see the user-facing decision in the implementation log).
bool circle_axis_parallel_to_sketch(
    const SketchFeatureParameters::SketchPlaneFrame& frame,
    const EdgePoint& axis,
    double tolerance = 1e-6) {
  // dot product magnitude == 1 (within tolerance) iff parallel /
  // anti-parallel given both are unit vectors.
  const double dot = axis.x * frame.normal_x +
                     axis.y * frame.normal_y +
                     axis.z * frame.normal_z;
  return std::abs(std::abs(dot) - 1.0) <= tolerance;
}

// Sweep direction of a projected arc, derived from sketch-local
// geometry: the curve midpoint must lie between start and end, so the
// 2D cross of (mid - start) x (end - start) gives the winding.
// Orientation-proof — wire edges can be reversed relative to their
// underlying curves, so the body axis alone cannot give the sweep.
bool arc_ccw_from_local(const std::pair<double, double>& start,
                        const std::pair<double, double>& end,
                        const std::pair<double, double>& mid) {
  const double ux = mid.first - start.first;
  const double uy = mid.second - start.second;
  const double vx = end.first - start.first;
  const double vy = end.second - start.second;
  return ux * vy - uy * vx > 0.0;
}

FaceOutlinePoint sketch_local_to_world(const SketchFeatureParameters& sketch,
                                       double local_x,
                                       double local_y) {
  if (sketch.plane_frame.has_value()) {
    const auto& frame = sketch.plane_frame.value();
    return FaceOutlinePoint{
        .x = frame.origin_x + frame.x_axis_x * local_x +
             frame.y_axis_x * local_y,
        .y = frame.origin_y + frame.x_axis_y * local_x +
             frame.y_axis_y * local_y,
        .z = frame.origin_z + frame.x_axis_z * local_x +
             frame.y_axis_z * local_y,
    };
  }
  if (sketch.plane_id == "ref-plane-xy") {
    return FaceOutlinePoint{.x = local_x, .y = local_y, .z = 0.0};
  }
  if (sketch.plane_id == "ref-plane-yz") {
    return FaceOutlinePoint{.x = 0.0, .y = local_x, .z = local_y};
  }
  return FaceOutlinePoint{.x = local_x, .y = 0.0, .z = local_y};
}

FaceOutlinePoint sketch_normal(const SketchFeatureParameters& sketch) {
  if (sketch.plane_frame.has_value()) {
    const auto& frame = sketch.plane_frame.value();
    return FaceOutlinePoint{
        .x = frame.normal_x, .y = frame.normal_y, .z = frame.normal_z};
  }
  if (sketch.plane_id == "ref-plane-yz") {
    return FaceOutlinePoint{.x = 1.0, .y = 0.0, .z = 0.0};
  }
  if (sketch.plane_id == "ref-plane-xz") {
    return FaceOutlinePoint{.x = 0.0, .y = 1.0, .z = 0.0};
  }
  return FaceOutlinePoint{.x = 0.0, .y = 0.0, .z = 1.0};
}

bool normals_parallel(const FaceOutlinePoint& left,
                      const SketchFeatureParameters::SketchPlaneFrame& right,
                      double tolerance = 1e-6) {
  const double dot = left.x * right.normal_x + left.y * right.normal_y +
                     left.z * right.normal_z;
  return std::abs(std::abs(dot) - 1.0) <= tolerance;
}

bool circle_contains_circle_for_projection(const SketchCircle& outer,
                                           const SketchCircle& inner) {
  if (outer.id == inner.id || outer.radius <= inner.radius) {
    return false;
  }
  const double dx = inner.center_x - outer.center_x;
  const double dy = inner.center_y - outer.center_y;
  const double center_distance = std::sqrt(dx * dx + dy * dy);
  return center_distance + inner.radius <= outer.radius + 1e-6;
}
