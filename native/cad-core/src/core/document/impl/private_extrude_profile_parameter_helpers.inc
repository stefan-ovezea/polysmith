// Touching or overlapping hole wires make the planar cap face invalid —
// OCCT cannot classify a face whose hole wires touch or cross, and the
// extrude used to emit a silently-broken body (user-reported: flower
// petals drawn with sub-0.01 mm slits against the hub boundary arrived
// here as hole loops sharing welded vertices; the plate compiled with 2
// invalid faces).  Refuse with a clear message instead.  Only exact
// boundary-edge profiles are checked (legacy sampled profiles keep the
// old path).
void validate_hole_separation(const ExtrudeFeatureParameters& params) {
  constexpr double kMinHoleSeparation = 0.05;  // mm
  if (params.inner_loop_edges.empty() && params.inner_loops.empty()) {
    return;
  }

  // Sample a set of edges into points.  Lines contribute endpoints;
  // circular pieces are sampled along their param span (the span's
  // direction does not matter — the same geometric arc results from
  // either walk direction).
  const auto sample = [](const std::vector<ProfileBoundaryEdge>& edges) {
    constexpr int kSamples = 16;
    std::vector<SketchProfilePoint> pts;
    for (const auto& e : edges) {
      if (e.entity_kind == "line") {
        pts.push_back(SketchProfilePoint{.x = e.start_x, .y = e.start_y});
        pts.push_back(SketchProfilePoint{.x = e.end_x, .y = e.end_y});
        continue;
      }
      for (int i = 0; i <= kSamples; ++i) {
        const double t = static_cast<double>(i) / kSamples;
        const double a = e.param_start + (e.param_end - e.param_start) * t;
        pts.push_back(SketchProfilePoint{
            .x = e.center_x + e.radius * std::cos(a),
            .y = e.center_y + e.radius * std::sin(a)});
      }
    }
    return pts;
  };
  const auto gap_between = [](const std::vector<SketchProfilePoint>& a,
                              const std::vector<SketchProfilePoint>& b) {
    double best = std::numeric_limits<double>::max();
    for (const auto& pa : a) {
      for (const auto& pb : b) {
        best = std::min(best, std::hypot(pa.x - pb.x, pa.y - pb.y));
      }
    }
    return best;
  };
  const auto format_gap = [](double gap) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.3f", gap);
    return std::string(buffer);
  };

  const std::vector<SketchProfilePoint> boundary = sample(params.boundary_edges);
  std::vector<std::vector<SketchProfilePoint>> holes;
  for (size_t h = 0; h < params.inner_loop_edges.size(); ++h) {
    const auto exact = sample(params.inner_loop_edges[h]);
    // Circle holes keep the circle_holes descriptor path and may leave
    // inner_loop_edges empty — fall back to the sampled loop (which
    // also covers legacy profiles).
    holes.push_back(exact.empty() && h < params.inner_loops.size()
                        ? params.inner_loops[h]
                        : exact);
  }
  // Sampled-only holes beyond the exact-edge list (legacy profiles).
  for (size_t h = params.inner_loop_edges.size();
       h < params.inner_loops.size(); ++h) {
    holes.push_back(params.inner_loops[h]);
  }

  // A hole must not touch the outer outline — a dangling stub welded
  // to the boundary produces a degenerate lens hole exactly there.
  // Collect EVERY violation first (the user's flower had three
  // touching pairs at once) and refuse once with the complete picture.
  int boundary_hits = 0;
  int pair_hits = 0;
  double worst_gap = std::numeric_limits<double>::max();
  for (size_t h = 0; h < holes.size(); ++h) {
    const double gap = gap_between(holes[h], boundary);
    if (gap < kMinHoleSeparation) {
      worst_gap = std::min(worst_gap, gap);
      ++boundary_hits;
      log_warn("extrude",
               "hole loop " + std::to_string(h) +
                   " touches the profile outline (gap " + format_gap(gap) +
                   " mm)");
    }
  }
  // And holes must not touch each other.
  for (size_t a = 0; a < holes.size(); ++a) {
    for (size_t b = a + 1; b < holes.size(); ++b) {
      const double gap = gap_between(holes[a], holes[b]);
      if (gap < kMinHoleSeparation) {
        worst_gap = std::min(worst_gap, gap);
        ++pair_hits;
        log_warn("extrude",
                 "hole loops " + std::to_string(a) + " and " +
                     std::to_string(b) + " touch (gap " + format_gap(gap) +
                     " mm)");
      }
    }
  }
  if (boundary_hits == 0 && pair_hits == 0) {
    return;
  }

  std::string problem;
  if (boundary_hits > 0 && pair_hits == 0) {
    problem = "a hole touches the profile outline";
  } else if (boundary_hits == 0 && pair_hits == 1) {
    problem = "two holes touch or overlap";
  } else if (boundary_hits == 0) {
    problem = std::to_string(pair_hits) + " pairs of holes touch or overlap";
  } else {
    problem = "holes touch the profile outline or each other";
  }
  throw std::runtime_error(
      "Extrude refused: " + problem + " (smallest gap " +
      format_gap(worst_gap) +
      " mm). Separate the pieces by at least 0.05 mm and extrude "
      "again.");
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
    ExtrudeFeatureParameters params{
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

    if (!profile.ordered_edge_ids.empty()) {
      params.sketch_edge_ids = profile.ordered_edge_ids;
      // Copy the sketch geometry that the wire builder needs so it can
      // reconstruct exact OCCT curves (lines → GC_MakeSegment, arcs →
      // GC_MakeArcOfCircle, circles → gp_Circ) from the profile boundary.
      for (const auto& line : sketch.lines) {
        if (!line.is_construction) params.sketch_lines.push_back(line);
      }
      for (const auto& arc : sketch.arcs) {
        if (!arc.is_construction) params.sketch_arcs.push_back(arc);
      }
      for (const auto& circle : sketch.circles) {
        if (!circle.is_construction) params.sketch_circles.push_back(circle);
      }
    }

    // Carry exact arc descriptors so the wire builder never guesses.
    params.arc_descriptors = profile.arc_descriptors;
    // Exact boundary edges — the wire builder's primary input.
    params.boundary_edges = profile.boundary_edges;
    // Exact per-hole edges — holes become exact wires instead of
    // polygon-approximated cutouts (empty for legacy profiles).
    params.inner_loop_edges = profile.inner_loop_edges;

    validate_hole_separation(params);

    return params;
  }

  if (profile.kind == "circle") {
    ExtrudeFeatureParameters params{
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

    // Carry the sketch geometry so that cut/join modes can route this
    // circle through the wire path (matching the target body's
    // construction method for OCCT 8 boolean compatibility).
    if (profile.source_circle_id.has_value()) {
      for (const auto& circle : sketch.circles) {
        if (circle.id == profile.source_circle_id.value() &&
            !circle.is_construction) {
          params.sketch_circles.push_back(circle);
          params.sketch_edge_ids.push_back(circle.id);
          break;
        }
      }
      for (const auto& line : sketch.lines) {
        if (!line.is_construction) params.sketch_lines.push_back(line);
      }
      for (const auto& arc : sketch.arcs) {
        if (!arc.is_construction) params.sketch_arcs.push_back(arc);
      }
    }

    return params;
  }

  if (profile.kind == "ellipse") {
    // Routed through the exact wire path (boundary_edges carry the
    // analytic ellipse edge) — same pattern as the polygon branch.
    ExtrudeFeatureParameters params{
        .sketch_feature_id = sketch_feature.id,
        .profile_id = profile.id,
        .profile_ids = {profile.id},
        .plane_id = plane_id,
        .plane_frame = plane_frame,
        .profile_kind = "ellipse",
        .start_x = profile.center_x,
        .start_y = profile.center_y,
        .width = 0.0,
        .height = 0.0,
        .radius = 0.0,
        .profile_points = profile.points,
        .inner_loops = profile.inner_loops,
        .depth = depth,
    };

    params.sketch_edge_ids = profile.ordered_edge_ids;
    for (const auto& line : sketch.lines) {
      if (!line.is_construction) params.sketch_lines.push_back(line);
    }
    for (const auto& arc : sketch.arcs) {
      if (!arc.is_construction) params.sketch_arcs.push_back(arc);
    }
    for (const auto& circle : sketch.circles) {
      if (!circle.is_construction) params.sketch_circles.push_back(circle);
    }
    params.arc_descriptors = profile.arc_descriptors;
    params.boundary_edges = profile.boundary_edges;
    params.inner_loop_edges = profile.inner_loop_edges;

    validate_hole_separation(params);

    return params;
  }

  return std::nullopt;
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

  // Copy the sketch geometry so the wire-path builder can construct
  // exact OCCT curves for the primary profile boundary.  Additional
  // profiles are handled separately by wrap_with_additional_profiles
  // in build_single_depth_extrude — they are built as polygon prisms
  // and combined with the wire-path primary into one compound.
  for (const auto& line : sketch.lines) {
    if (!line.is_construction) parameters.sketch_lines.push_back(line);
  }
  for (const auto& arc : sketch.arcs) {
    if (!arc.is_construction) parameters.sketch_arcs.push_back(arc);
  }
  for (const auto& circle : sketch.circles) {
    if (!circle.is_construction) parameters.sketch_circles.push_back(circle);
  }
  // Use the ordered edge IDs from the first profile only, so
  // sketch_edge_ids always matches profile_points (also from first).
  if (!first.ordered_edge_ids.empty()) {
    parameters.sketch_edge_ids = first.ordered_edge_ids;
  }
  parameters.boundary_edges = first.boundary_edges;
  parameters.inner_loop_edges = first.inner_loop_edges;

  for (size_t index = 1; index < profiles.size(); ++index) {
    const auto& profile = profiles[index];
    parameters.additional_profile_points.push_back(
        profile_boundary_points(profile));
    parameters.additional_inner_loops.push_back(profile.inner_loops);
    parameters.additional_sketch_edge_ids.push_back(
        profile.ordered_edge_ids);
    parameters.additional_arc_descriptors.push_back(
        profile.arc_descriptors);
    parameters.additional_boundary_edges.push_back(
        profile.boundary_edges);
  }

  validate_hole_separation(parameters);

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
  target.additional_sketch_edge_ids = source.additional_sketch_edge_ids;
  target.additional_arc_descriptors = source.additional_arc_descriptors;
  target.additional_boundary_edges = source.additional_boundary_edges;
  target.sketch_edge_ids = source.sketch_edge_ids;
  target.sketch_lines = source.sketch_lines;
  target.sketch_arcs = source.sketch_arcs;
  target.sketch_circles = source.sketch_circles;
  target.arc_descriptors = source.arc_descriptors;
  target.boundary_edges = source.boundary_edges;
  target.inner_loop_edges = source.inner_loop_edges;
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
