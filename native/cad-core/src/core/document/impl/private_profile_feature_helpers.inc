std::vector<SketchProfilePoint> sample_circle_profile_points(
    const SketchProfileRegion& profile) {
  std::vector<SketchProfilePoint> points;
  constexpr int kCircleSegments = 64;
  points.reserve(kCircleSegments);
  for (int index = 0; index < kCircleSegments; ++index) {
    const double angle =
        (static_cast<double>(index) / static_cast<double>(kCircleSegments)) *
        2.0 * 3.14159265358979323846;
    points.push_back(SketchProfilePoint{
        .x = profile.center_x + profile.radius * std::cos(angle),
        .y = profile.center_y + profile.radius * std::sin(angle),
    });
  }
  return points;
}

std::optional<ExtrudeFeatureParameters> make_extrude_parameters_for_profile(
    const FeatureEntry& sketch_feature,
    const SketchProfileRegion& profile,
    double depth) {
  if (!sketch_feature.sketch_parameters.has_value()) {
    return std::nullopt;
  }

  const auto& sketch = sketch_feature.sketch_parameters.value();
  const std::string plane_id = sketch.plane_frame.has_value()
                                   ? plane_id_from_frame(sketch.plane_frame.value())
                                   : sketch.plane_id;
  const std::optional<PlaneFrame> plane_frame =
      sketch.plane_frame.has_value()
          ? std::optional<PlaneFrame>(make_plane_frame(sketch.plane_frame.value()))
          : std::nullopt;

  if (profile.kind == "polygon") {
    return ExtrudeFeatureParameters{
        .sketch_feature_id = sketch_feature.id,
        .profile_id = profile.id,
        .profile_ids = {profile.id},
        .plane_id = plane_id,
        .plane_frame = plane_frame,
        .profile_kind = "polygon",
        .start_x = 0.0,
        .start_y = 0.0,
        .width = 0.0,
        .height = 0.0,
        .radius = 0.0,
        .profile_points = profile.points,
        .inner_loops = profile.inner_loops,
        .depth = depth,
    };
  }

  if (profile.kind == "circle") {
    return ExtrudeFeatureParameters{
        .sketch_feature_id = sketch_feature.id,
        .profile_id = profile.id,
        .profile_ids = {profile.id},
        .plane_id = plane_id,
        .plane_frame = plane_frame,
        .profile_kind = "circle",
        .start_x = profile.center_x,
        .start_y = profile.center_y,
        .width = 0.0,
        .height = 0.0,
        .radius = profile.radius,
        .profile_points = {},
        .depth = depth,
    };
  }

  return std::nullopt;
}

std::vector<SketchProfilePoint> profile_boundary_points(
    const SketchProfileRegion& profile) {
  return profile.kind == "circle" ? sample_circle_profile_points(profile)
                                  : profile.points;
}

double squared_distance(const SketchProfilePoint& a,
                        const SketchProfilePoint& b) {
  const double dx = a.x - b.x;
  const double dy = a.y - b.y;
  return dx * dx + dy * dy;
}

double cross(const SketchProfilePoint& a,
             const SketchProfilePoint& b,
             const SketchProfilePoint& c) {
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool point_on_segment(const SketchProfilePoint& point,
                      const SketchProfilePoint& a,
                      const SketchProfilePoint& b,
                      double tolerance) {
  if (std::abs(cross(a, b, point)) > tolerance) {
    return false;
  }
  return point.x >= std::min(a.x, b.x) - tolerance &&
         point.x <= std::max(a.x, b.x) + tolerance &&
         point.y >= std::min(a.y, b.y) - tolerance &&
         point.y <= std::max(a.y, b.y) + tolerance;
}

bool segments_touch(const SketchProfilePoint& a1,
                    const SketchProfilePoint& a2,
                    const SketchProfilePoint& b1,
                    const SketchProfilePoint& b2,
                    double tolerance) {
  if (point_on_segment(a1, b1, b2, tolerance) ||
      point_on_segment(a2, b1, b2, tolerance) ||
      point_on_segment(b1, a1, a2, tolerance) ||
      point_on_segment(b2, a1, a2, tolerance)) {
    return true;
  }

  const double c1 = cross(a1, a2, b1);
  const double c2 = cross(a1, a2, b2);
  const double c3 = cross(b1, b2, a1);
  const double c4 = cross(b1, b2, a2);
  return ((c1 > tolerance && c2 < -tolerance) ||
          (c1 < -tolerance && c2 > tolerance)) &&
         ((c3 > tolerance && c4 < -tolerance) ||
          (c3 < -tolerance && c4 > tolerance));
}

bool profile_regions_touch(const SketchProfileRegion& a,
                           const SketchProfileRegion& b) {
  constexpr double kTolerance = 1.0e-6;
  constexpr double kToleranceSquared = kTolerance * kTolerance;
  const std::vector<SketchProfilePoint> a_points = profile_boundary_points(a);
  const std::vector<SketchProfilePoint> b_points = profile_boundary_points(b);
  if (a_points.empty() || b_points.empty()) {
    return false;
  }

  for (const auto& a_point : a_points) {
    for (const auto& b_point : b_points) {
      if (squared_distance(a_point, b_point) <= kToleranceSquared) {
        return true;
      }
    }
  }

  for (size_t a_index = 0; a_index < a_points.size(); ++a_index) {
    const auto& a1 = a_points[a_index];
    const auto& a2 = a_points[(a_index + 1) % a_points.size()];
    for (size_t b_index = 0; b_index < b_points.size(); ++b_index) {
      const auto& b1 = b_points[b_index];
      const auto& b2 = b_points[(b_index + 1) % b_points.size()];
      if (segments_touch(a1, a2, b1, b2, kTolerance)) {
        return true;
      }
    }
  }

  return false;
}

std::vector<std::vector<SketchProfileRegion>> group_touching_profiles(
    const std::vector<SketchProfileRegion>& profiles) {
  std::vector<std::vector<SketchProfileRegion>> groups;
  std::vector<bool> visited(profiles.size(), false);

  for (size_t start = 0; start < profiles.size(); ++start) {
    if (visited[start]) {
      continue;
    }

    std::vector<size_t> stack{start};
    visited[start] = true;
    std::vector<SketchProfileRegion> group;

    while (!stack.empty()) {
      const size_t current = stack.back();
      stack.pop_back();
      group.push_back(profiles[current]);

      for (size_t candidate = 0; candidate < profiles.size(); ++candidate) {
        if (visited[candidate]) {
          continue;
        }
        if (!profile_regions_touch(profiles[current], profiles[candidate])) {
          continue;
        }
        visited[candidate] = true;
        stack.push_back(candidate);
      }
    }

    groups.push_back(std::move(group));
  }

  return groups;
}

std::optional<ExtrudeFeatureParameters> make_extrude_parameters_for_profiles(
    const FeatureEntry& sketch_feature,
    const std::vector<SketchProfileRegion>& profiles,
    double depth) {
  if (profiles.empty() || !sketch_feature.sketch_parameters.has_value()) {
    return std::nullopt;
  }
  if (profiles.size() == 1) {
    return make_extrude_parameters_for_profile(sketch_feature,
                                               profiles.front(),
                                               depth);
  }

  const auto& sketch = sketch_feature.sketch_parameters.value();
  const auto& first = profiles.front();
  const std::string plane_id = sketch.plane_frame.has_value()
                                   ? plane_id_from_frame(sketch.plane_frame.value())
                                   : sketch.plane_id;
  const std::optional<PlaneFrame> plane_frame =
      sketch.plane_frame.has_value()
          ? std::optional<PlaneFrame>(make_plane_frame(sketch.plane_frame.value()))
          : std::nullopt;
  std::vector<std::string> ids;
  for (const auto& profile : profiles) {
    ids.push_back(profile.id);
  }

  ExtrudeFeatureParameters parameters{
      .sketch_feature_id = sketch_feature.id,
      .profile_id = ids.front(),
      .profile_ids = ids,
      .plane_id = plane_id,
      .plane_frame = plane_frame,
      .profile_kind = "polygon",
      .start_x = 0.0,
      .start_y = 0.0,
      .width = 0.0,
      .height = 0.0,
      .radius = 0.0,
      .profile_points = profile_boundary_points(first),
      .inner_loops = first.inner_loops,
      .depth = depth,
  };

  for (size_t index = 1; index < profiles.size(); ++index) {
    const auto& profile = profiles[index];
    parameters.additional_profile_points.push_back(
        profile_boundary_points(profile));
    parameters.additional_inner_loops.push_back(profile.inner_loops);
  }

  return parameters;
}

void preserve_extrude_source_geometry(ExtrudeFeatureParameters& target,
                                      const ExtrudeFeatureParameters& source) {
  target.sketch_feature_id = source.sketch_feature_id;
  target.profile_id = source.profile_id;
  target.profile_ids = source.profile_ids;
  target.open_entity_ids = source.open_entity_ids;
  target.plane_id = source.plane_id;
  target.plane_frame = source.plane_frame;
  target.profile_kind = source.profile_kind;
  target.start_x = source.start_x;
  target.start_y = source.start_y;
  target.width = source.width;
  target.height = source.height;
  target.radius = source.radius;
  target.profile_points = source.profile_points;
  target.inner_loops = source.inner_loops;
  target.additional_profile_points = source.additional_profile_points;
  target.additional_inner_loops = source.additional_inner_loops;
}

void normalize_extrude_operation_mode(ExtrudeFeatureParameters& parameters) {
  if (parameters.operation.empty()) {
    parameters.operation =
        parameters.mode.empty() ? std::string("new_body") : parameters.mode;
  }
  if (parameters.operation == "join" && !parameters.target_body_id.has_value()) {
    parameters.mode = "new_body";
    return;
  }
  parameters.mode = parameters.operation;
}

void apply_automatic_extrude_mode(const DocumentState& document,
                                  ExtrudeFeatureParameters& parameters,
                                  bool grouped_touching_profiles) {
  parameters.operation = "new_body";
  parameters.mode = "new_body";
  parameters.target_body_id = std::nullopt;

  if (const auto touching =
          find_touching_body_for_extrude(document, parameters);
      touching.has_value()) {
    parameters.operation = "join";
    parameters.mode = "join";
    parameters.target_body_id = touching;
    return;
  }

  if (const auto intersected =
          find_intersecting_body_for_extrude(document, parameters);
      intersected.has_value()) {
    parameters.operation = "cut";
    parameters.mode = "cut";
    parameters.target_body_id = intersected;
    return;
  }

  if (grouped_touching_profiles) {
    parameters.operation = "join";
    parameters.mode = "new_body";
  }
}

std::optional<LoftSectionParameters> make_loft_section_for_profile(
    const FeatureEntry& sketch_feature,
    const SketchProfileRegion& profile) {
  if (!sketch_feature.sketch_parameters.has_value()) {
    return std::nullopt;
  }
  if (!profile.inner_loops.empty()) {
    return std::nullopt;
  }

  const auto& sketch = sketch_feature.sketch_parameters.value();
  const std::string plane_id = sketch.plane_frame.has_value()
                                   ? plane_id_from_frame(sketch.plane_frame.value())
                                   : sketch.plane_id;
  const std::optional<PlaneFrame> plane_frame =
      sketch.plane_frame.has_value()
          ? std::optional<PlaneFrame>(make_plane_frame(sketch.plane_frame.value()))
          : std::nullopt;

  return LoftSectionParameters{
      .sketch_feature_id = sketch_feature.id,
      .profile_id = profile.id,
      .plane_id = plane_id,
      .plane_frame = plane_frame,
      .profile_points = profile.kind == "circle"
                            ? sample_circle_profile_points(profile)
                            : profile.points,
  };
}

std::array<double, 3> revolve_sketch_local_to_world(
    const SketchFeatureParameters& sketch,
    double local_x,
    double local_y) {
  if (sketch.plane_frame.has_value()) {
    const auto& frame = sketch.plane_frame.value();
    return {
        frame.origin_x + frame.x_axis_x * local_x + frame.y_axis_x * local_y,
        frame.origin_y + frame.x_axis_y * local_x + frame.y_axis_y * local_y,
        frame.origin_z + frame.x_axis_z * local_x + frame.y_axis_z * local_y,
    };
  }
  if (sketch.plane_id == "ref-plane-xy") {
    return {local_x, 0.0, local_y};
  }
  if (sketch.plane_id == "ref-plane-yz") {
    return {0.0, local_x, local_y};
  }
  if (sketch.plane_id == "ref-plane-xz") {
    return {local_x, local_y, 0.0};
  }
  throw std::runtime_error("Unsupported sketch plane: " + sketch.plane_id);
}

SketchLine* find_line_by_id(SketchFeatureParameters& sketch,
                            const std::string& line_id);
const SketchLine* find_line_by_id(const SketchFeatureParameters& sketch,
                                  const std::string& line_id);

std::optional<RevolveFeatureParameters> make_revolve_parameters_for_profile(
    const FeatureEntry& profile_sketch_feature,
    const SketchProfileRegion& profile,
    const FeatureEntry& axis_sketch_feature,
    const SketchLine& axis_line,
    double angle_degrees) {
  if (!profile_sketch_feature.sketch_parameters.has_value() ||
      !axis_sketch_feature.sketch_parameters.has_value()) {
    return std::nullopt;
  }

  const auto& profile_sketch = profile_sketch_feature.sketch_parameters.value();
  const auto& axis_sketch = axis_sketch_feature.sketch_parameters.value();
  const std::string plane_id =
      profile_sketch.plane_frame.has_value()
          ? plane_id_from_frame(profile_sketch.plane_frame.value())
          : profile_sketch.plane_id;
  const std::optional<PlaneFrame> plane_frame =
      profile_sketch.plane_frame.has_value()
          ? std::optional<PlaneFrame>(
                make_plane_frame(profile_sketch.plane_frame.value()))
          : std::nullopt;
  const auto axis_start = revolve_sketch_local_to_world(
      axis_sketch, axis_line.start_x, axis_line.start_y);
  const auto axis_end = revolve_sketch_local_to_world(
      axis_sketch, axis_line.end_x, axis_line.end_y);

  return RevolveFeatureParameters{
      .sketch_feature_id = profile_sketch_feature.id,
      .profile_id = profile.id,
      .plane_id = plane_id,
      .plane_frame = plane_frame,
      .profile_kind = profile.kind,
      .profile_points = profile.kind == "circle"
                            ? sample_circle_profile_points(profile)
                            : profile.points,
      .inner_loops = profile.inner_loops,
      .axis_sketch_feature_id = axis_sketch_feature.id,
      .axis_entity_id = axis_line.id,
      .axis_start_x = axis_start[0],
      .axis_start_y = axis_start[1],
      .axis_start_z = axis_start[2],
      .axis_end_x = axis_end[0],
      .axis_end_y = axis_end[1],
      .axis_end_z = axis_end[2],
      .angle_degrees = angle_degrees,
  };
}

bool profile_id_mentions_entity(const std::string& profile_id,
                                const std::string& entity_id) {
  const std::string needle = "-" + entity_id;
  size_t position = profile_id.find(needle);
  while (position != std::string::npos) {
    const size_t after = position + needle.size();
    if (after == profile_id.size() || profile_id[after] == '-') {
      return true;
    }
    position = profile_id.find(needle, position + 1);
  }
  return false;
}

const SketchProfileRegion* find_profile_by_id(
    const std::vector<SketchProfileRegion>& profiles,
    const std::string& profile_id) {
  const auto profile_it = std::find_if(
      profiles.begin(),
      profiles.end(),
      [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });
  return profile_it == profiles.end() ? nullptr : &(*profile_it);
}

const SketchProfileRegion* find_equivalent_profile(
    const std::vector<SketchProfileRegion>& profiles,
    const std::string& source_profile_id) {
  if (const SketchProfileRegion* exact =
          find_profile_by_id(profiles, source_profile_id)) {
    return exact;
  }

  std::vector<const SketchProfileRegion*> candidates;
  for (const auto& profile : profiles) {
    if (profile.kind == "circle" && profile.source_circle_id.has_value() &&
        source_profile_id.find("profile-circle-" +
                               profile.source_circle_id.value()) == 0) {
      candidates.push_back(&profile);
      continue;
    }

    if (profile.kind == "polygon" && !profile.line_ids.empty()) {
      const bool same_entities = std::all_of(
          profile.line_ids.begin(),
          profile.line_ids.end(),
          [&](const std::string& entity_id) {
            return profile_id_mentions_entity(source_profile_id, entity_id);
          });
      if (same_entities) {
        candidates.push_back(&profile);
      }
    }
  }

  return candidates.size() == 1 ? candidates.front() : nullptr;
}

SweepFeatureParameters make_sweep_parameters(
    std::vector<FeatureEntry>& features,
    const std::string& profile_id,
    const std::string& path_entity_id);
