struct LineEndpointRef {
  size_t line_index;
  bool is_start;
};

std::string endpoint_point_id(const SketchLine& line, bool is_start) {
  return is_start ? line.start_point_id : line.end_point_id;
}

void set_endpoint_point_id(SketchLine& line,
                           bool is_start,
                           const std::string& point_id) {
  if (is_start) {
    line.start_point_id = point_id;
    return;
  }

  line.end_point_id = point_id;
}

double endpoint_x(const SketchLine& line, bool is_start) {
  return is_start ? line.start_x : line.end_x;
}

double endpoint_y(const SketchLine& line, bool is_start) {
  return is_start ? line.start_y : line.end_y;
}

std::optional<std::tuple<std::string, double, double>> find_coincident_endpoint(
    const SketchFeatureParameters& parameters,
    const std::string& ignored_entity_id,
    double x,
    double y) {
  auto match_line = [&](const SketchLine& candidate) -> bool {
    return candidate.id == ignored_entity_id;
  };
  auto match_arc = [&](const SketchArc& candidate) -> bool {
    return candidate.id == ignored_entity_id;
  };

  for (const auto& candidate : parameters.lines) {
    if (match_line(candidate)) continue;
    if (points_match(candidate.start_x, candidate.start_y, x, y))
      return std::tuple<std::string, double, double>{
          candidate.start_point_id, candidate.start_x, candidate.start_y};
    if (points_match(candidate.end_x, candidate.end_y, x, y))
      return std::tuple<std::string, double, double>{
          candidate.end_point_id, candidate.end_x, candidate.end_y};
  }
  for (const auto& candidate : parameters.arcs) {
    if (match_arc(candidate)) continue;
    if (points_match(candidate.start_x, candidate.start_y, x, y))
      return std::tuple<std::string, double, double>{
          candidate.start_point_id, candidate.start_x, candidate.start_y};
    if (points_match(candidate.end_x, candidate.end_y, x, y))
      return std::tuple<std::string, double, double>{
          candidate.end_point_id, candidate.end_x, candidate.end_y};
  }
  return std::nullopt;
}

void rebuild_sketch_points(SketchFeatureParameters& parameters) {
  parameters.points.clear();

  const auto append_point = [&](const std::string& point_id,
                                const std::string& kind,
                                double x,
                                double y) {
    const auto existing_it = std::find_if(
        parameters.points.begin(),
        parameters.points.end(),
        [&](const SketchPoint& point) { return point.id == point_id; });
    if (existing_it != parameters.points.end()) {
      return;
    }

    parameters.points.push_back(SketchPoint{
        .id = point_id,
        .kind = kind,
        .x = x,
        .y = y,
        .is_fixed = false,
    });
  };

  for (const auto& line : parameters.lines) {
    append_point(line.start_point_id, "endpoint", line.start_x, line.start_y);
    append_point(line.end_point_id, "endpoint", line.end_x, line.end_y);
  }

  for (const auto& arc : parameters.arcs) {
    // Arc endpoints share the line "endpoint" kind so the same
    // hover / snapping / loop-detection logic that already walks
    // endpoint points just works on arcs without special-casing.
    // Coords come from the cached arc params (which are kept in
    // sync at creation; v1 freezes them after that).
    append_point(arc.start_point_id, "endpoint", arc.start_x, arc.start_y);
    append_point(arc.end_point_id, "endpoint", arc.end_x, arc.end_y);
  }

  for (const auto& circle : parameters.circles) {
    append_point(
        "point-circle-" + circle.id + "-center", "center", circle.center_x, circle.center_y);

    // Derived quadrant points (cardinal directions on the circle).
    // These are always fixed — they're derived geometry, not independently
    // movable.  They give the dimension tool stable point IDs for
    // quadrant / perimeter snaps without needing a new IPC command.
    const double cx = circle.center_x;
    const double cy = circle.center_y;
    const double r = circle.radius;
    parameters.points.push_back(SketchPoint{
        .id = "point-circle-" + circle.id + "-quadrant-0",
        .kind = "quadrant",
        .x = cx + r,
        .y = cy,
        .is_fixed = true,
    });
    parameters.points.push_back(SketchPoint{
        .id = "point-circle-" + circle.id + "-quadrant-1",
        .kind = "quadrant",
        .x = cx,
        .y = cy + r,
        .is_fixed = true,
    });
    parameters.points.push_back(SketchPoint{
        .id = "point-circle-" + circle.id + "-quadrant-2",
        .kind = "quadrant",
        .x = cx - r,
        .y = cy,
        .is_fixed = true,
    });
    parameters.points.push_back(SketchPoint{
        .id = "point-circle-" + circle.id + "-quadrant-3",
        .kind = "quadrant",
        .x = cx,
        .y = cy - r,
        .is_fixed = true,
    });
  }

  // Re-emit the original corner point of every fillet. After
  // `add_sketch_fillet` mutates lines A and B to reference the new
  // trim points, no line / arc / circle references the original
  // corner anymore, so without this pass `rebuild_sketch_points`
  // would silently drop it. The fillet record carries the cached
  // (`corner_x`, `corner_y`) precisely for this purpose; they're
  // refreshed every recompute by `enforce_sketch_fillets`.
  for (const auto& fillet : parameters.fillets) {
    append_point(fillet.corner_point_id,
                 "endpoint",
                 fillet.corner_x,
                 fillet.corner_y);
  }

  // Free-standing points placed by the Project tool. Same machinery
  // as the fillet corner re-emit above: nothing else references
  // these ids, so without this loop they would vanish on the next
  // recompute. Marked `kind = "projected"` so the renderer / hit
  // testing can give them a distinct visual (a small cross, similar
  // to common CAD behavior). `is_fixed` is forced true below by
  // `enforce_derived_points_fixed` so the user can't drag them.
  for (const auto& projected : parameters.projected_points) {
    append_point(projected.id, "projected", projected.x, projected.y);
  }
}

// Force every derived point (projected, quadrant) to be is_fixed = true.
// Runs after `rebuild_sketch_points` + `sync_fixed_point_flags` so it
// doesn't matter what the previous-frame value was — derived geometry is
// always locked.
void enforce_derived_points_fixed(SketchFeatureParameters& parameters) {
  // Collect arc endpoint point ids so we can freeze them too.
  // v1 doesn't support post-creation arc reshape, so arc endpoints
  // must stay locked to prevent the constraint solver from dragging
  // them away from their connections.
  //
  // EXCEPTION: arc endpoints that are also endpoints of non-construction
  // lines must stay movable.  Trimming a corner creates arcs whose
  // endpoints are shared with the trimmed lines — freezing those
  // endpoints would block the user from editing the line's dimension
  // (drive_line_length_respecting_fixed_points throws when both
  // endpoints are fixed).
  std::unordered_set<std::string> line_endpoint_ids;
  for (const auto& line : parameters.lines) {
    if (line.is_construction) continue;
    line_endpoint_ids.insert(line.start_point_id);
    line_endpoint_ids.insert(line.end_point_id);
  }
  std::unordered_set<std::string> arc_endpoint_ids;
  for (const auto& arc : parameters.arcs) {
    arc_endpoint_ids.insert(arc.start_point_id);
    arc_endpoint_ids.insert(arc.end_point_id);
  }
  for (auto& point : parameters.points) {
    if (point.kind == "projected" || point.kind == "quadrant" ||
        (arc_endpoint_ids.count(point.id) &&
         !line_endpoint_ids.count(point.id))) {
      point.is_fixed = true;
    }
  }
}

void sync_fixed_point_flags(SketchFeatureParameters& parameters,
                            const std::vector<SketchPoint>& previous_points) {
  for (auto& point : parameters.points) {
    const auto previous_it = std::find_if(
        previous_points.begin(),
        previous_points.end(),
        [&](const SketchPoint& previous) { return previous.id == point.id; });
    if (previous_it != previous_points.end()) {
      point.is_fixed = previous_it->is_fixed;
    }
  }
}

void rebuild_sketch_profiles(SketchFeatureParameters& parameters) {
  parameters.profiles = build_sketch_profile_regions(parameters);
}

// For each `SketchFillet`, re-derive the trim point coords + the
// generated arc's cached params from the *current* line endpoints.
// Runs as part of `refresh_sketch_derived_state` after every sketch
// edit, so dragging the "far" endpoint of a filleted line keeps the
// fillet tangent.
//
// Per-fillet algorithm:
//   1. Locate lines A and B and identify which endpoint of each is
//      the fillet's trim point. The opposite endpoint is "far".
//   2. Project each line as an infinite ray from `far` through its
//      *current* trim coord — that direction continues to the
//      virtual corner. Solve the two-line intersection in 2D for
//      the virtual corner coordinates.
//   3. With `theta` = angle between the two outgoing directions
//      (corner→far_a) and (corner→far_b), compute trim distance
//      `d = radius / tan(theta/2)` and shift the trim points to
//      `corner + d * (far - corner) / |far - corner|`.
//   4. Push the new trim coords back onto the lines' cached
//      endpoint fields and onto the arc's cached params (center,
//      radius, ccw, start/end).
//
// If validation fails for any reason (line missing, lines became
// parallel, trim no longer fits) we leave the fillet's cached
// geometry as-is and skip — the previous frame's render survives,
// which is preferable to a corrupted partial update. The fillet
// still survives; the user can drag the lines back into a valid
