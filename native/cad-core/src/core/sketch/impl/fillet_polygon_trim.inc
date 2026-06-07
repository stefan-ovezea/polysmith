namespace {

// Helper: which endpoint of `line` references `point_id`. Returns
// `true` for the start endpoint, `false` for the end endpoint.
// Caller must have already verified one of them does match.
bool line_start_matches(const SketchLine& line, const std::string& point_id) {
  return line.start_point_id == point_id;
}

}  // namespace

void add_sketch_fillet(FeatureEntry& feature,
                       int fillet_index,
                       int trim_a_point_index,
                       int trim_b_point_index,
                       int arc_index,
                       const std::string& corner_point_id,
                       const std::string& line_a_id,
                       const std::string& line_b_id,
                       double radius) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch fillets");
  }
  if (radius <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch fillets must have a positive radius");
  }
  if (line_a_id == line_b_id) {
    throw std::runtime_error("Sketch fillet requires two distinct lines");
  }

  auto& parameters = *feature.sketch_parameters;
  auto& line_a = require_line(parameters, line_a_id);
  auto& line_b = require_line(parameters, line_b_id);

  // Both lines must reference the corner point as one of their
  // endpoints — otherwise the user clicked a point that isn't shared.
  const bool a_at_start = line_a.start_point_id == corner_point_id;
  const bool a_at_end = line_a.end_point_id == corner_point_id;
  const bool b_at_start = line_b.start_point_id == corner_point_id;
  const bool b_at_end = line_b.end_point_id == corner_point_id;
  if ((!a_at_start && !a_at_end) || (!b_at_start && !b_at_end)) {
    throw std::runtime_error(
        "Sketch fillet: corner point must be shared by both lines");
  }

  // Refuse to fillet a line that's already part of another fillet at
  // this corner — the lines need a clean shared endpoint to start
  // from. Lines filleted at the *other* end are fine.
  for (const auto& existing : parameters.fillets) {
    const bool a_collides =
        (existing.line_a_id == line_a_id || existing.line_b_id == line_a_id) &&
        (existing.trim_a_point_id == corner_point_id ||
         existing.trim_b_point_id == corner_point_id);
    const bool b_collides =
        (existing.line_a_id == line_b_id || existing.line_b_id == line_b_id) &&
        (existing.trim_a_point_id == corner_point_id ||
         existing.trim_b_point_id == corner_point_id);
    if (a_collides || b_collides) {
      throw std::runtime_error(
          "Sketch fillet: this corner is already filleted");
    }
  }

  // "Far" endpoint of each line — the one opposite the corner.
  const double far_a_x = a_at_start ? line_a.end_x : line_a.start_x;
  const double far_a_y = a_at_start ? line_a.end_y : line_a.start_y;
  const double far_b_x = b_at_start ? line_b.end_x : line_b.start_x;
  const double far_b_y = b_at_start ? line_b.end_y : line_b.start_y;
  const double corner_x = a_at_start ? line_a.start_x : line_a.end_x;
  const double corner_y = a_at_start ? line_a.start_y : line_a.end_y;

  // Outgoing directions from the corner toward each far endpoint.
  const double out_ax = far_a_x - corner_x;
  const double out_ay = far_a_y - corner_y;
  const double out_bx = far_b_x - corner_x;
  const double out_by = far_b_y - corner_y;
  const double len_a = std::hypot(out_ax, out_ay);
  const double len_b = std::hypot(out_bx, out_by);
  if (len_a <= kMinimumSketchDimensionValue ||
      len_b <= kMinimumSketchDimensionValue) {
    throw std::runtime_error(
        "Sketch fillet: both lines must have non-zero length");
  }
  const double ux_a = out_ax / len_a;
  const double uy_a = out_ay / len_a;
  const double ux_b = out_bx / len_b;
  const double uy_b = out_by / len_b;

  const double cross = ux_a * uy_b - uy_a * ux_b;
  if (std::abs(cross) <= kMinimumSketchDimensionValue) {
    throw std::runtime_error(
        "Sketch fillet: the two lines are parallel or colinear");
  }

  const double dot = ux_a * ux_b + uy_a * uy_b;
  const double clamped_dot = std::max(-1.0, std::min(1.0, dot));
  const double theta = std::acos(clamped_dot);
  if (theta <= kMinimumSketchDimensionValue) {
    throw std::runtime_error(
        "Sketch fillet: the two lines have no measurable angle between them");
  }
  const double half_theta = theta * 0.5;
  const double trim_distance = radius / std::tan(half_theta);
  if (trim_distance + kMinimumSketchDimensionValue >= len_a ||
      trim_distance + kMinimumSketchDimensionValue >= len_b) {
    throw std::runtime_error(
        "Sketch fillet: radius is too large for at least one of the lines");
  }

  // New trim point coordinates and arc center.
  const double trim_a_x = corner_x + trim_distance * ux_a;
  const double trim_a_y = corner_y + trim_distance * uy_a;
  const double trim_b_x = corner_x + trim_distance * ux_b;
  const double trim_b_y = corner_y + trim_distance * uy_b;
  const double bisector_x = ux_a + ux_b;
  const double bisector_y = uy_a + uy_b;
  const double bisector_len = std::hypot(bisector_x, bisector_y);
  // bisector_len > 0 here because we rejected near-180° above.
  const double center_distance = radius / std::sin(half_theta);
  const double arc_center_x =
      corner_x + center_distance * (bisector_x / bisector_len);
  const double arc_center_y =
      corner_y + center_distance * (bisector_y / bisector_len);
  const double arc_cross =
      (trim_a_x - arc_center_x) * (trim_b_y - arc_center_y) -
      (trim_a_y - arc_center_y) * (trim_b_x - arc_center_x);
  const bool arc_ccw = arc_cross > 0.0;

  // Allocate generated entity ids. Trim points share the same
  // namespace as line endpoints (point-N), the arc its own (arc-N).
  // The fillet itself uses fillet-N.
  const std::string trim_a_id = "point-" + std::to_string(trim_a_point_index);
  const std::string trim_b_id = "point-" + std::to_string(trim_b_point_index);
  const std::string arc_id = "arc-" + std::to_string(arc_index);
  const std::string fillet_id = "fillet-" + std::to_string(fillet_index);

  // Mutate the lines: replace their corner-pointing endpoint with
  // the new trim point. Also update the cached coords so they match
  // the trim position right away. The recompute pass will refresh
  // these on every subsequent edit.
  if (a_at_start) {
    line_a.start_point_id = trim_a_id;
    line_a.start_x = trim_a_x;
    line_a.start_y = trim_a_y;
  } else {
    line_a.end_point_id = trim_a_id;
    line_a.end_x = trim_a_x;
    line_a.end_y = trim_a_y;
  }
  if (b_at_start) {
    line_b.start_point_id = trim_b_id;
    line_b.start_x = trim_b_x;
    line_b.start_y = trim_b_y;
  } else {
    line_b.end_point_id = trim_b_id;
    line_b.end_x = trim_b_x;
    line_b.end_y = trim_b_y;
  }

  // Append the generated arc (its endpoints reference the new trim
  // points) and the fillet record.
  parameters.arcs.push_back(SketchArc{
      .id = arc_id,
      .start_point_id = trim_a_id,
      .end_point_id = trim_b_id,
      .center_x = arc_center_x,
      .center_y = arc_center_y,
      .radius = radius,
      .start_x = trim_a_x,
      .start_y = trim_a_y,
      .end_x = trim_b_x,
      .end_y = trim_b_y,
      .ccw = arc_ccw,
  });
  parameters.fillets.push_back(SketchFillet{
      .id = fillet_id,
      .corner_point_id = corner_point_id,
      .corner_x = corner_x,
      .corner_y = corner_y,
      .line_a_id = line_a_id,
      .line_b_id = line_b_id,
      .trim_a_point_id = trim_a_id,
      .trim_b_point_id = trim_b_id,
      .arc_id = arc_id,
      .radius = radius,
  });

  refresh_sketch_derived_state(feature);

  // Trim points are fixed by the fillet's parametric solve — the
  // user can't drag them. Same convention as bare arcs.
  for (auto& point : feature.sketch_parameters->points) {
    if (point.id == trim_a_id || point.id == trim_b_id) {
      point.is_fixed = true;
    }
  }
}

void update_sketch_fillet_radius(FeatureEntry& feature,
                                 const std::string& fillet_id,
                                 double radius) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch fillets");
  }
  if (radius <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch fillets must have a positive radius");
  }

  auto& parameters = *feature.sketch_parameters;
  const auto fillet_it = std::find_if(
      parameters.fillets.begin(),
      parameters.fillets.end(),
      [&](const SketchFillet& fillet) { return fillet.id == fillet_id; });
  if (fillet_it == parameters.fillets.end()) {
    throw std::runtime_error("Sketch fillet not found: " + fillet_id);
  }

  // Defer the validity check (radius too large) to the recompute
  // pass: it'll silently skip the update if the new radius doesn't
  // fit, leaving the previous radius's geometry in place. We do
  // *commit* the new radius value so the user can drag a line longer
  // and see the requested radius take effect — symmetric with how
  // line-length / dimension drives behave around fixed-point
  // conflicts.
  fillet_it->radius = radius;
  refresh_sketch_derived_state(feature);
}

void delete_sketch_fillet(FeatureEntry& feature,
                          const std::string& fillet_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can delete sketch fillets");
  }

  auto& parameters = *feature.sketch_parameters;
  const auto fillet_it = std::find_if(
      parameters.fillets.begin(),
      parameters.fillets.end(),
      [&](const SketchFillet& fillet) { return fillet.id == fillet_id; });
  if (fillet_it == parameters.fillets.end()) {
    throw std::runtime_error("Sketch fillet not found: " + fillet_id);
  }

  const SketchFillet fillet_copy = *fillet_it;

  // Restore from the fillet's cached corner coords (refreshed on
  // every recompute) rather than re-reading the points table. The
  // points table is rebuilt from the fillet on every refresh, so
  // these two values are always in sync; the cache is what makes
  // the round-trip work even when no other entity references the
  // corner anymore.
  const double corner_x = fillet_copy.corner_x;
  const double corner_y = fillet_copy.corner_y;

  // Restore line A's filleted endpoint (the trim point) back to the
  // corner point. Same for line B.
  const auto restore_line = [&](const std::string& line_id,
                                const std::string& trim_point_id) {
    const auto line_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == line_id; });
    if (line_it == parameters.lines.end()) {
      return;
    }
    if (line_start_matches(*line_it, trim_point_id)) {
      line_it->start_point_id = fillet_copy.corner_point_id;
      line_it->start_x = corner_x;
      line_it->start_y = corner_y;
    } else if (line_it->end_point_id == trim_point_id) {
      line_it->end_point_id = fillet_copy.corner_point_id;
      line_it->end_x = corner_x;
      line_it->end_y = corner_y;
    }
  };
  restore_line(fillet_copy.line_a_id, fillet_copy.trim_a_point_id);
  restore_line(fillet_copy.line_b_id, fillet_copy.trim_b_point_id);

  // Remove the generated arc.
  parameters.arcs.erase(
      std::remove_if(
          parameters.arcs.begin(),
          parameters.arcs.end(),
          [&](const SketchArc& arc) { return arc.id == fillet_copy.arc_id; }),
      parameters.arcs.end());

  // Remove the fillet record itself; trim points get cleaned up on
  // the next `rebuild_sketch_points` because nothing references
  // them anymore.
  parameters.fillets.erase(
      std::remove_if(
          parameters.fillets.begin(),
          parameters.fillets.end(),
          [&](const SketchFillet& fillet) { return fillet.id == fillet_id; }),
      parameters.fillets.end());

  refresh_sketch_derived_state(feature);
}

void delete_sketch_dimension(FeatureEntry& feature,
                             const std::string& dimension_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can delete sketch dimensions");
  }

  auto& parameters = *feature.sketch_parameters;
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dimension) {
        return dimension.id == dimension_id;
      });
  if (dimension_it == parameters.dimensions.end()) {
    // Dimension may not exist: construction lines don't get auto-dims,
    // and the TS side may fire deletion for a shape that was just
    // committed without a dimension. Silently ignore.
    return;
  }

  parameters.dimensions.erase(dimension_it);
  refresh_sketch_derived_state(feature);
}

void add_sketch_polygon(FeatureEntry& feature,
                        int polygon_index,
                        int sides,
                        const std::string& mode,
                        double start_x,
                        double start_y,
                        double end_x,
                        double end_y,
                        bool is_construction) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch polygons");
  }
  if (sides < 3) {
    throw std::runtime_error("Polygon must have at least 3 sides");
  }

  double center_x = start_x;
  double center_y = start_y;
  double radius = std::hypot(end_x - start_x, end_y - start_y);

  if (mode == "edge") {
    double dx = end_x - start_x;
    double dy = end_y - start_y;
    double edge_len = std::hypot(dx, dy);
    if (edge_len <= kMinimumSketchDimensionValue) {
      throw std::runtime_error("Polygon edge must have non-zero length");
    }
    double mid_x = (start_x + end_x) / 2.0;
    double mid_y = (start_y + end_y) / 2.0;
    double nx = -dy / edge_len;
    double ny = dx / edge_len;
    radius = edge_len / (2.0 * std::sin(M_PI / sides));
    center_x = mid_x + nx * radius * std::cos(M_PI / sides);
    center_y = mid_y + ny * radius * std::cos(M_PI / sides);
  }

  // Decompose into N individual lines so the polygon participates in
  // snap, constraints, drag, and profile detection — same approach as
  // add_sketch_rectangle.
  if (!feature.sketch_parameters.has_value()) return;
  auto& parameters = *feature.sketch_parameters;

  // Compute vertices in CCW order starting from angle 0 (right).
  std::vector<std::pair<double, double>> vertices(sides);
  const double angle_step = 2.0 * M_PI / sides;
  for (int i = 0; i < sides; ++i) {
    const double a = angle_step * i - M_PI / 2.0;  // start from top
    vertices[i] = {center_x + radius * std::cos(a),
                   center_y + radius * std::sin(a)};
  }

  // Create lines. Pass false for snap_start so polygon vertices aren't
  // merged with nearby geometry (same as rectangle).
  std::vector<std::string> line_ids(sides);
  for (int i = 0; i < sides; ++i) {
    const int next = (i + 1) % sides;
    const int line_idx = polygon_index * 100 + i;  // unique index
    add_sketch_line(feature, line_idx,
                    vertices[i].first, vertices[i].second,
                    vertices[next].first, vertices[next].second,
                    is_construction, /*snap_start=*/false);
    line_ids[i] = "line-" + std::to_string(line_idx);
  }

  // Remove the auto line-length and line-angle dimensions that
  // add_sketch_line created — polygon lines use the polygon radius
  // dimension instead. Also clear any H/V constraints so the
  // polygon can be freely rotated.
  for (const auto& lid : line_ids) {
    auto& line = require_line(parameters, lid);
    line.constraint = std::nullopt;
  }
  parameters.dimensions.erase(
      std::remove_if(parameters.dimensions.begin(), parameters.dimensions.end(),
                     [&](const SketchDimension& d) {
                       return (d.kind == "line_length" || d.kind == "line_angle") &&
                              std::find(line_ids.begin(), line_ids.end(), d.entity_id) != line_ids.end();
                     }),
      parameters.dimensions.end());

  // Add equal-length constraints so all sides stay the same length.
  for (int i = 1; i < sides; ++i) {
    parameters.line_relations.push_back(SketchLineRelation{
        .id = "rel-equal-length-poly-" + std::to_string(polygon_index) +
              "-" + std::to_string(i),
        .kind = "equal_length",
        .first_line_id = line_ids[0],
        .second_line_id = line_ids[i],
    });
  }

  // Store the polygon entry so the UI can still select/identify it.
  parameters.polygons.push_back(SketchPolygon{
      .id = "polygon-" + std::to_string(polygon_index),
      .center_x = center_x,
      .center_y = center_y,
      .radius = radius,
      .sides = sides,
      .mode = mode,
      .start_x = start_x,
      .start_y = start_y,
      .end_x = end_x,
      .end_y = end_y,
      .is_construction = is_construction,
  });
  if (!is_construction) {
    parameters.dimensions.push_back(SketchDimension{
        .id = "dim-polygon-" + std::to_string(polygon_index),
        .kind = "polygon_radius",
        .entity_id = "polygon-" + std::to_string(polygon_index),
        .value = radius,
        .is_auto = true,
    });
  }

  refresh_sketch_derived_state(feature);
}

void add_sketch_point_distance_dimension(FeatureEntry& feature,
                                         const std::string& point_a_id,
                                         const std::string& point_b_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch dimensions");
  }

  auto& parameters = *feature.sketch_parameters;

  const auto point_a_it = std::find_if(
      parameters.points.begin(),
      parameters.points.end(),
      [&](const SketchPoint& p) { return p.id == point_a_id; });
  const auto point_b_it = std::find_if(
      parameters.points.begin(),
      parameters.points.end(),
      [&](const SketchPoint& p) { return p.id == point_b_id; });

  if (point_a_it == parameters.points.end()) {
    throw std::runtime_error("Sketch point not found: " + point_a_id);
  }
  if (point_b_it == parameters.points.end()) {
    throw std::runtime_error("Sketch point not found: " + point_b_id);
  }

  const double dx = point_b_it->x - point_a_it->x;
  const double dy = point_b_it->y - point_a_it->y;
  const double distance = std::sqrt(dx * dx + dy * dy);

  if (distance <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Point distance dimension must be greater than zero");
  }

  const std::string dimension_id =
      "dim-point-distance-" + point_a_id + "-" + point_b_id;
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != parameters.dimensions.end()) {
    // Already exists, just update the value for the current geometry.
    dimension_it->value = distance;
    return;
  }

  parameters.dimensions.push_back(SketchDimension{
      .id = dimension_id,
      .kind = "point_distance",
      .entity_id = point_a_id,
      .secondary_entity_id = point_b_id,
      .value = distance,
      // point_distance is always driven (reference-only) for now.
      // The value is re-computed from the two point coordinates during
      // sync_driven_dimensions. Making it driving requires complex
      // point-movement propagation and is a follow-up.
      .driven = true,
  });

  refresh_sketch_derived_state(feature);
}

void add_sketch_line_length_dimension(FeatureEntry& feature,
                                      const std::string& line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch dimensions");
  }

  auto& parameters = *feature.sketch_parameters;
  auto& line = require_line(parameters, line_id);

  if (line.is_construction) {
    throw std::runtime_error(
        "Cannot create a driving dimension on a construction line");
  }

  const std::string dimension_id = "dim-line-" + line.id;
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != parameters.dimensions.end()) {
    throw std::runtime_error(
        "Line length dimension already exists: " + dimension_id);
  }

  parameters.dimensions.push_back(SketchDimension{
      .id = dimension_id,
      .kind = "line_length",
      .entity_id = line.id,
      .value = measure_line_length(line),
  });

  refresh_sketch_derived_state(feature);
}

void add_sketch_circle_radius_dimension(FeatureEntry& feature,
                                        const std::string& circle_id,
                                        std::optional<std::string> display_as) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch dimensions");
  }

  auto& parameters = *feature.sketch_parameters;
  auto& circle = require_circle(parameters, circle_id);

  if (circle.is_construction) {
    throw std::runtime_error(
        "Cannot create a driving dimension on a construction circle");
  }

  const std::string dimension_id = "dim-circle-" + circle.id;
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != parameters.dimensions.end()) {
    throw std::runtime_error(
        "Circle radius dimension already exists: " + dimension_id);
  }

  parameters.dimensions.push_back(SketchDimension{
      .id = dimension_id,
      .kind = "circle_radius",
      .entity_id = circle.id,
      .value = circle.radius,
      .display_as = display_as.value_or(""),
  });

  refresh_sketch_derived_state(feature);
}

void add_sketch_polygon_radius_dimension(FeatureEntry& feature,
                                         const std::string& polygon_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch dimensions");
  }

  auto& parameters = *feature.sketch_parameters;
  const auto polygon_it = std::find_if(
      parameters.polygons.begin(),
      parameters.polygons.end(),
      [&](const SketchPolygon& polygon) { return polygon.id == polygon_id; });

  if (polygon_it == parameters.polygons.end()) {
    throw std::runtime_error("Sketch polygon not found: " + polygon_id);
  }

  if (polygon_it->is_construction) {
    throw std::runtime_error(
        "Cannot create a driving dimension on a construction polygon");
  }

  const std::string dimension_id = "dim-polygon-" + polygon_it->id;
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == dimension_id; });
  if (dimension_it != parameters.dimensions.end()) {
    throw std::runtime_error(
        "Polygon radius dimension already exists: " + dimension_id);
  }

  parameters.dimensions.push_back(SketchDimension{
      .id = dimension_id,
      .kind = "polygon_radius",
      .entity_id = polygon_it->id,
      .value = polygon_it->radius,
  });

  refresh_sketch_derived_state(feature);
}

void trim_sketch_entity(FeatureEntry& feature,
                        const std::string& entity_id,
                        double click_x,
                        double click_y) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can trim entities");
  }

  auto& params = *feature.sketch_parameters;

  // Phase 1: lines.
  const auto line_it = std::find_if(
      params.lines.begin(),
      params.lines.end(),
      [&](const SketchLine& line) { return line.id == entity_id; });

  if (line_it != params.lines.end()) {
    const auto intersections = find_all_intersections(*line_it, params);

    // 0 intersections — isolated entity: trim acts as normal delete.
    if (intersections.empty()) {
      params.constraints.erase(
          std::remove_if(params.constraints.begin(), params.constraints.end(),
                         [&](const SketchConstraint& c) {
                           return std::find(c.target_ids.begin(),
                                            c.target_ids.end(),
                                            entity_id) != c.target_ids.end();
                         }),
          params.constraints.end());
      params.dimensions.erase(
          std::remove_if(params.dimensions.begin(), params.dimensions.end(),
                         [&](const SketchDimension& d) {
                           return d.entity_id == entity_id;
                         }),
          params.dimensions.end());
      params.lines.erase(line_it);
      return;
    }

    const auto segments = split_line_at_intersections(*line_it, intersections);
    if (segments.empty()) return;

    const int clicked_index = select_clicked_segment(
        segments, *line_it, click_x, click_y);

    if (clicked_index < 0 || clicked_index >= static_cast<int>(segments.size())) {
      throw std::runtime_error(
          "Click position does not correspond to any segment on entity: " + entity_id);
    }

    // Capture old point IDs *before* vector mutations (push_back
    // may invalidate the line iterator, making later reads garbage).
    const std::string old_sp_id = line_it->start_point_id;
    const std::string old_ep_id = line_it->end_point_id;

    // If all non-clicked segments are zero-length (intersections at
    // line endpoints), the trim is effectively a full delete.
    bool only_clicked_is_real = true;
    for (int si = 0; si < static_cast<int>(segments.size()); ++si) {
      if (si == clicked_index) continue;
      const double slen = std::hypot(
          segments[si].end_x - segments[si].start_x,
          segments[si].end_y - segments[si].start_y);
      if (slen > kTrimCoincidentTolerance) { only_clicked_is_real = false; break; }
    }
    if (only_clicked_is_real) {
      // Clicked the only real segment → delete the entire line.
      params.constraints.erase(
          std::remove_if(params.constraints.begin(), params.constraints.end(),
                         [&](const SketchConstraint& c) {
                           for (const auto& tid : c.target_ids)
                             if (tid == entity_id || tid == old_sp_id || tid == old_ep_id)
                               return true;
                           return false;
                         }),
          params.constraints.end());
      params.dimensions.erase(
          std::remove_if(params.dimensions.begin(), params.dimensions.end(),
                         [&](const SketchDimension& d) {
                           return d.entity_id == entity_id ||
                                  d.secondary_entity_id == entity_id;
                         }),
          params.dimensions.end());
      params.lines.erase(line_it);
      return;
    }

    // Delete the clicked segment.
    const int last = static_cast<int>(segments.size()) - 1;

    if (clicked_index == 0) {
      line_it->start_x = segments[1].start_x;
      line_it->start_y = segments[1].start_y;
    } else if (clicked_index == last) {
      line_it->end_x = segments[clicked_index - 1].end_x;
      line_it->end_y = segments[clicked_index - 1].end_y;
    }
    if (points_match(line_it->start_x, line_it->start_y,
                     line_it->end_x, line_it->end_y)) {
      fprintf(stderr, "ERROR trim produced zero-length line %s — deleting\n",
              line_it->id.c_str());
      params.lines.erase(line_it);
      return;
    }
    if (clicked_index > 0 && clicked_index < last) {
      // Middle segment deleted — line splits into two.
      // Left portion: original line shortened to intersection before deleted segment.
      line_it->end_x = segments[clicked_index - 1].end_x;
      line_it->end_y = segments[clicked_index - 1].end_y;

      // Right portion: new line from intersection after deleted segment to end.
      const int next_index = static_cast<int>(params.lines.size()) + 1;
      const double rsx = segments[clicked_index + 1].start_x;
      const double rsy = segments[clicked_index + 1].start_y;
      const double rex = segments[last].end_x;
      const double rey = segments[last].end_y;

      const int right_idx = next_index * 10 + 2000;
      params.lines.push_back(SketchLine{
          .id = "line-" + std::to_string(next_index),
          .start_point_id = "point-trim-" + std::to_string(right_idx) + "-start",
          .end_point_id   = "point-trim-" + std::to_string(right_idx) + "-end",
          .start_x = rsx,
          .start_y = rsy,
          .end_x = rex,
          .end_y = rey,
          .constraint = infer_constraint_hint(rsx, rsy, rex, rey),
          .is_construction = line_it->is_construction,
      });
      auto& new_line = params.lines.back();
      new_line.constraint = std::nullopt;

      if (points_match(new_line.start_x, new_line.start_y,
                       new_line.end_x, new_line.end_y)) {
        fprintf(stderr, "ERROR trim split produced zero-length line %s — dropping\n",
                new_line.id.c_str());
        params.lines.pop_back();
      }
      if (points_match(line_it->start_x, line_it->start_y,
                       line_it->end_x, line_it->end_y)) {
        fprintf(stderr, "ERROR trim split collapsed left portion %s — deleting\n",
                line_it->id.c_str());
        params.lines.erase(line_it);
        return;
      }
    }

    // Trim breaks all existing constraints and dimensions on the entity.
    // Delete them using the pre-captured point IDs (line_it may be
    // invalid after push_back in the middle-segment case).
    params.constraints.erase(
        std::remove_if(params.constraints.begin(), params.constraints.end(),
                       [&](const SketchConstraint& c) {
                         for (const auto& tid : c.target_ids) {
                           if (tid == entity_id || tid == old_sp_id || tid == old_ep_id)
                             return true;
                         }
                         return false;
                       }),
        params.constraints.end());
    params.dimensions.erase(
        std::remove_if(params.dimensions.begin(), params.dimensions.end(),
                       [&](const SketchDimension& d) {
                         return d.entity_id == entity_id ||
                                d.secondary_entity_id == entity_id;
                       }),
        params.dimensions.end());

    // Delete midpoint and point-line anchors that reference the
    // trimmed line — they become invalid when the line changes.
    params.midpoint_anchors.erase(
        std::remove_if(params.midpoint_anchors.begin(), params.midpoint_anchors.end(),
                       [&](const SketchMidpointAnchor& a) {
                         return a.line_id == entity_id;
                       }),
        params.midpoint_anchors.end());
    params.point_line_anchors.erase(
        std::remove_if(params.point_line_anchors.begin(), params.point_line_anchors.end(),
                       [&](const SketchPointLineAnchor& a) {
                         return a.line_id == entity_id;
                       }),
        params.point_line_anchors.end());

    // Delete line relations (parallel, perpendicular, equal_length,
    // tangent_line_circle) that involve the trimmed line, and clear
    // the H/V constraint on every line whose relation was deleted.
    // Otherwise the badge that was suppressed by the relation pops
    // back and looks like a "morphed" constraint.
    {
      std::unordered_set<std::string> affected_line_ids;
      for (const auto& r : params.line_relations) {
        if (r.first_line_id == entity_id || r.second_line_id == entity_id) {
          if (r.first_line_id != entity_id) affected_line_ids.insert(r.first_line_id);
          if (r.second_line_id != entity_id) affected_line_ids.insert(r.second_line_id);
        }
      }
      params.line_relations.erase(
          std::remove_if(params.line_relations.begin(), params.line_relations.end(),
                         [&](const SketchLineRelation& r) {
                           return r.first_line_id == entity_id ||
                                  r.second_line_id == entity_id;
                         }),
          params.line_relations.end());
      for (auto& line : params.lines) {
        if (affected_line_ids.count(line.id)) {
          line.constraint = std::nullopt;
        }
      }
    }

    // Delete any fillet that involves this line — trimmed geometry
    // can't keep a valid fillet and orphaned fillet points cause
    // ghost dots in the viewport.
    params.fillets.erase(
        std::remove_if(params.fillets.begin(), params.fillets.end(),
                       [&](const SketchFillet& f) {
                         return f.line_a_id == entity_id ||
                                f.line_b_id == entity_id ||
                                f.arc_id == entity_id ||
                                f.corner_point_id == old_sp_id ||
                                f.corner_point_id == old_ep_id ||
                                f.trim_a_point_id == old_sp_id ||
                                f.trim_a_point_id == old_ep_id ||
                                f.trim_b_point_id == old_sp_id ||
                                f.trim_b_point_id == old_ep_id;
                       }),
        params.fillets.end());

    // Share point IDs at the new endpoints with coincident geometry
    // so profile loops stay connected after trim.  Mirrors the
    // circle→arc path (find_coincident_endpoint + shared/fallback IDs).
    const int fresh_idx = static_cast<int>(params.lines.size()) * 10 +
                          static_cast<int>(params.arcs.size()) * 10 + 1000;

    const auto shared_start = find_coincident_endpoint(
        params, entity_id, line_it->start_x, line_it->start_y);
    const auto shared_end = find_coincident_endpoint(
        params, entity_id, line_it->end_x, line_it->end_y);

    const std::string new_sp = shared_start.has_value()
        ? std::get<0>(shared_start.value())
        : "point-trim-" + std::to_string(fresh_idx) + "-start";
    const std::string new_ep = shared_end.has_value()
        ? std::get<0>(shared_end.value())
        : "point-trim-" + std::to_string(fresh_idx) + "-end";

    line_it->start_point_id = new_sp;
    line_it->end_point_id   = new_ep;

    // Dissolve all polygon records — a trimmed polygon line breaks
    // the parametric polygon. Remaining lines become independent.
    params.dimensions.erase(
        std::remove_if(params.dimensions.begin(), params.dimensions.end(),
                       [&](const SketchDimension& d) {
                         return d.kind == "polygon_radius";
                       }),
        params.dimensions.end());
    params.polygons.clear();

    // Update other lines that shared the old point IDs.  If the other
    // line's endpoint is still coincident with the trimmed line's new
    // endpoint, they continue to share.  Otherwise orphan the old ref.
    int next_fresh = fresh_idx + 1;
    auto update_shared_pt = [&](std::string& pt_id,
                                double pt_x, double pt_y,
                                const std::string& old_id,
                                const std::string& new_id,
                                double new_x, double new_y) {
      if (pt_id != old_id) return;
      if (points_match(pt_x, pt_y, new_x, new_y)) {
        pt_id = new_id;
      } else {
        pt_id = "point-trim-" + std::to_string(next_fresh++) + "-orphan";
      }
    };

    for (auto& ol : params.lines) {
      if (ol.id == entity_id) continue;
      update_shared_pt(ol.start_point_id, ol.start_x, ol.start_y,
                       old_sp_id, new_sp, line_it->start_x, line_it->start_y);
      update_shared_pt(ol.end_point_id,   ol.end_x,   ol.end_y,
                       old_sp_id, new_sp, line_it->start_x, line_it->start_y);
      update_shared_pt(ol.start_point_id, ol.start_x, ol.start_y,
                       old_ep_id, new_ep, line_it->end_x, line_it->end_y);
      update_shared_pt(ol.end_point_id,   ol.end_x,   ol.end_y,
                       old_ep_id, new_ep, line_it->end_x, line_it->end_y);
    }

    // Clear any H/V constraint the line may have had — trim may have
    // changed the direction enough to invalidate it.
    line_it->constraint = std::nullopt;

    // Safety net: delete coincident constraints referencing orphaned point IDs.
    {
      std::unordered_set<std::string> live_pt;
      for (const auto& l : params.lines) {
        live_pt.insert(l.start_point_id); live_pt.insert(l.end_point_id);
      }
      for (const auto& c : params.circles) {
        live_pt.insert("point-circle-" + c.id + "-center");
        live_pt.insert("point-circle-" + c.id + "-quadrant-0");
        live_pt.insert("point-circle-" + c.id + "-quadrant-1");
        live_pt.insert("point-circle-" + c.id + "-quadrant-2");
        live_pt.insert("point-circle-" + c.id + "-quadrant-3");
      }
      for (const auto& a : params.arcs) {
        live_pt.insert(a.start_point_id); live_pt.insert(a.end_point_id);
      }
      params.constraints.erase(
          std::remove_if(params.constraints.begin(), params.constraints.end(),
                         [&](const SketchConstraint& c) {
                           if (c.kind != "coincident") return false;
                           for (const auto& tid : c.target_ids)
                             if (!live_pt.count(tid)) return true;
                           return false;
                         }),
          params.constraints.end());
    }

    return;
  }

  // Phase 2: circles → arcs.
  const auto circle_it = std::find_if(
      params.circles.begin(),
      params.circles.end(),
      [&](const SketchCircle& c) { return c.id == entity_id; });

  if (circle_it != params.circles.end()) {
    const auto intersections = find_all_intersections(*circle_it, params);

    // 0 intersections — isolated entity: trim acts as normal delete.
    if (intersections.empty()) {
      params.constraints.erase(
          std::remove_if(params.constraints.begin(), params.constraints.end(),
                         [&](const SketchConstraint& c) {
                           return std::find(c.target_ids.begin(),
                                            c.target_ids.end(),
                                            entity_id) != c.target_ids.end();
                         }),
          params.constraints.end());
      params.dimensions.erase(
          std::remove_if(params.dimensions.begin(), params.dimensions.end(),
                         [&](const SketchDimension& d) {
                           return d.entity_id == entity_id;
                         }),
          params.dimensions.end());
      params.circles.erase(circle_it);
      return;
    }

    const auto segments = split_circle_at_intersections(*circle_it, intersections);
    if (segments.empty()) return;

    const int clicked_index = select_clicked_segment(
        segments, *circle_it, click_x, click_y);

    if (clicked_index < 0 || clicked_index >= static_cast<int>(segments.size())) {
      throw std::runtime_error(
          "Click position does not correspond to any segment on entity: " + entity_id);
    }

    // Delete the clicked arc segment — keep the complementary arc.
    // For 2 segments this is the opposite; for 3+ we build the arc
    // that spans all non-clicked segments going CCW.
    const int N = static_cast<int>(segments.size());
    const int arc_index = params.arcs.size() + 1;
    const int start_pt_idx = arc_index * 2;
    const int end_pt_idx = arc_index * 2 + 1;
    const double cx = circle_it->center_x, cy = circle_it->center_y;
    const double r = circle_it->radius;
    auto pt_at_angle = [&](double a) -> std::pair<double, double> {
      return {cx + r * std::cos(a), cy + r * std::sin(a)};
    };

    if (N == 2) {
      const auto& kept = segments[1 - clicked_index];
      auto [sx, sy] = pt_at_angle(kept.param_start);
      auto [ex, ey] = pt_at_angle(kept.param_end < 2.0 * M_PI
                                      ? kept.param_end
                                      : kept.param_end - 2.0 * M_PI);
      // Reuse existing endpoint ids when arc endpoints land on a point
      // another entity already owns — mirrors add_sketch_arc so the
      // profile loop detector can chain arcs with lines.
      const auto shared_start = find_coincident_endpoint(params, "", sx, sy);
      const auto shared_end   = find_coincident_endpoint(params, "", ex, ey);
      params.arcs.push_back(SketchArc{
          .id = "arc-" + std::to_string(arc_index),
          .start_point_id = shared_start.has_value()
              ? std::get<0>(shared_start.value())
              : "point-trim-" + std::to_string(start_pt_idx) + "-start",
          .end_point_id   = shared_end.has_value()
              ? std::get<0>(shared_end.value())
              : "point-trim-" + std::to_string(end_pt_idx) + "-end",
          .center_x = cx, .center_y = cy, .radius = r,
          .start_x = sx, .start_y = sy,
          .end_x = ex, .end_y = ey,
          .ccw = true,
          .is_construction = circle_it->is_construction,
      });
    } else {
      // Complementary arc: from segment after clicked to clicked's start,
      // going CCW the long way around the circle.
      const int next = (clicked_index + 1) % N;
      const double a_start = segments[next].param_start;
      double a_end = segments[clicked_index].param_start;
      if (a_end <= a_start) a_end += 2.0 * M_PI;
      auto [sx, sy] = pt_at_angle(a_start);
      auto [ex, ey] = pt_at_angle(segments[clicked_index].param_start);
      // Reuse existing endpoint ids for profile loop connectivity.
      const auto shared_start = find_coincident_endpoint(params, "", sx, sy);
      const auto shared_end   = find_coincident_endpoint(params, "", ex, ey);
      params.arcs.push_back(SketchArc{
          .id = "arc-" + std::to_string(arc_index),
          .start_point_id = shared_start.has_value()
              ? std::get<0>(shared_start.value())
              : "point-trim-" + std::to_string(start_pt_idx) + "-start",
          .end_point_id   = shared_end.has_value()
              ? std::get<0>(shared_end.value())
              : "point-trim-" + std::to_string(end_pt_idx) + "-end",
          .center_x = cx, .center_y = cy, .radius = r,
          .start_x = sx, .start_y = sy,
          .end_x = ex, .end_y = ey,
          .ccw = true,
          .is_construction = circle_it->is_construction,
      });
    }

    // Delete the circle.
    params.circles.erase(circle_it);

    // Mark all constraints referencing the circle as dependency_broken.
    // (The constraint and dimension structs don't have a dependency_broken
    // flag yet, so for v1 we just delete circle-related constraints.)
    params.constraints.erase(
        std::remove_if(params.constraints.begin(), params.constraints.end(),
                       [&](const SketchConstraint& c) {
                         return std::find(c.target_ids.begin(), c.target_ids.end(),
                                          entity_id) != c.target_ids.end();
                       }),
        params.constraints.end());

    params.dimensions.erase(
        std::remove_if(params.dimensions.begin(), params.dimensions.end(),
                       [&](const SketchDimension& d) {
                         return d.entity_id == entity_id ||
                                d.secondary_entity_id == entity_id;
                       }),
        params.dimensions.end());

    // Safety net: delete coincident constraints referencing orphaned point IDs.
    {
      std::unordered_set<std::string> live_pt;
      for (const auto& l : params.lines) {
        live_pt.insert(l.start_point_id); live_pt.insert(l.end_point_id);
      }
      for (const auto& c : params.circles) {
        live_pt.insert("point-circle-" + c.id + "-center");
        live_pt.insert("point-circle-" + c.id + "-quadrant-0");
        live_pt.insert("point-circle-" + c.id + "-quadrant-1");
        live_pt.insert("point-circle-" + c.id + "-quadrant-2");
        live_pt.insert("point-circle-" + c.id + "-quadrant-3");
      }
      for (const auto& a : params.arcs) {
        live_pt.insert(a.start_point_id); live_pt.insert(a.end_point_id);
      }
      params.constraints.erase(
          std::remove_if(params.constraints.begin(), params.constraints.end(),
                         [&](const SketchConstraint& c) {
                           if (c.kind != "coincident") return false;
                           for (const auto& tid : c.target_ids)
                             if (!live_pt.count(tid)) return true;
                           return false;
                         }),
          params.constraints.end());
    }
    return;
  }

  // Phase 3: arcs — not yet supported.
  const auto arc_it = std::find_if(
      params.arcs.begin(),
      params.arcs.end(),
      [&](const SketchArc& a) { return a.id == entity_id; });

  if (arc_it != params.arcs.end()) {
    const auto intersections = find_all_intersections(*arc_it, params);

    if (intersections.empty()) {
      params.constraints.erase(
          std::remove_if(params.constraints.begin(), params.constraints.end(),
                         [&](const SketchConstraint& c) {
                           return std::find(c.target_ids.begin(), c.target_ids.end(),
                                            entity_id) != c.target_ids.end();
                         }),
          params.constraints.end());
      params.dimensions.erase(
          std::remove_if(params.dimensions.begin(), params.dimensions.end(),
                         [&](const SketchDimension& d) {
                           return d.entity_id == entity_id;
                         }),
          params.dimensions.end());
      params.arcs.erase(arc_it);
      return;
    }

    const auto segments = split_arc_at_intersections(*arc_it, intersections);
    if (segments.empty()) return;

    const std::string old_sp_id = arc_it->start_point_id;
    const std::string old_ep_id = arc_it->end_point_id;

    const int clicked_index = select_clicked_segment(
        segments, *arc_it, click_x, click_y);

    if (clicked_index < 0 || clicked_index >= static_cast<int>(segments.size())) {
      throw std::runtime_error(
          "Click position does not correspond to any segment on entity: " + entity_id);
    }

    // Delete the clicked segment — keep from arc start to segment
    // boundary, or from boundary to arc end.
    const int last = static_cast<int>(segments.size()) - 1;
    auto arc_shared_pt = [&](double x, double y, const std::string& fallback) {
      const auto shared = find_coincident_endpoint(params, entity_id, x, y);
      return shared.has_value() ? std::get<0>(shared.value()) : fallback;
    };

    if (clicked_index == 0) {
      arc_it->start_x = segments[1].start_x;
      arc_it->start_y = segments[1].start_y;
      arc_it->start_point_id = arc_shared_pt(
          arc_it->start_x, arc_it->start_y,
          "point-trim-arc-" + std::to_string(params.arcs.size() * 10 + 3000) + "-start");
    } else if (clicked_index == last) {
      arc_it->end_x = segments[clicked_index - 1].end_x;
      arc_it->end_y = segments[clicked_index - 1].end_y;
      arc_it->end_point_id = arc_shared_pt(
          arc_it->end_x, arc_it->end_y,
          "point-trim-arc-" + std::to_string(params.arcs.size() * 10 + 3000) + "-end");
    } else {
      // Middle segment deleted — arc splits into two.
      arc_it->end_x = segments[clicked_index - 1].end_x;
      arc_it->end_y = segments[clicked_index - 1].end_y;
      arc_it->end_point_id = arc_shared_pt(
          arc_it->end_x, arc_it->end_y,
          "point-trim-arc-" + std::to_string(params.arcs.size() * 10 + 3000) + "-end");

      const int next_idx = static_cast<int>(params.arcs.size()) + 1;
      const auto& right = segments[clicked_index + 1];
      const auto& last_seg = segments[last];
      params.arcs.push_back(SketchArc{
          .id = "arc-" + std::to_string(next_idx),
          .start_point_id = arc_shared_pt(
              right.start_x, right.start_y,
              "point-trim-arc-" + std::to_string(next_idx * 10 + 4000) + "-start"),
          .end_point_id   = arc_shared_pt(
              last_seg.end_x, last_seg.end_y,
              "point-trim-arc-" + std::to_string(next_idx * 10 + 4000) + "-end"),
          .center_x = arc_it->center_x,
          .center_y = arc_it->center_y,
          .radius   = arc_it->radius,
          .start_x  = right.start_x,
          .start_y  = right.start_y,
          .end_x    = last_seg.end_x,
          .end_y    = last_seg.end_y,
          .ccw      = arc_it->ccw,
          .is_construction = arc_it->is_construction,
      });
    }

    // Clean up constraints/dimensions/relations/fillets on the arc.
    params.constraints.erase(
        std::remove_if(params.constraints.begin(), params.constraints.end(),
                       [&](const SketchConstraint& c) {
                         for (const auto& tid : c.target_ids)
                           if (tid == entity_id || tid == old_sp_id || tid == old_ep_id)
                             return true;
                         return false;
                       }),
        params.constraints.end());
    params.dimensions.erase(
        std::remove_if(params.dimensions.begin(), params.dimensions.end(),
                       [&](const SketchDimension& d) {
                         return d.entity_id == entity_id;
                       }),
        params.dimensions.end());
    params.line_relations.erase(
        std::remove_if(params.line_relations.begin(), params.line_relations.end(),
                       [&](const SketchLineRelation& r) {
                         return r.first_line_id == entity_id ||
                                r.second_line_id == entity_id;
                       }),
        params.line_relations.end());
    params.fillets.erase(
        std::remove_if(params.fillets.begin(), params.fillets.end(),
                       [&](const SketchFillet& f) {
                         return f.line_a_id == entity_id ||
                                f.line_b_id == entity_id ||
                                f.arc_id == entity_id ||
                                f.corner_point_id == old_sp_id ||
                                f.corner_point_id == old_ep_id ||
                                f.trim_a_point_id == old_sp_id ||
                                f.trim_a_point_id == old_ep_id ||
                                f.trim_b_point_id == old_sp_id ||
                                 f.trim_b_point_id == old_ep_id;
                       }),
        params.fillets.end());

    // Safety net: delete coincident constraints referencing orphaned point IDs.
    {
      std::unordered_set<std::string> live_pt;
      for (const auto& l : params.lines) {
        live_pt.insert(l.start_point_id); live_pt.insert(l.end_point_id);
      }
      for (const auto& c : params.circles) {
        live_pt.insert("point-circle-" + c.id + "-center");
        live_pt.insert("point-circle-" + c.id + "-quadrant-0");
        live_pt.insert("point-circle-" + c.id + "-quadrant-1");
        live_pt.insert("point-circle-" + c.id + "-quadrant-2");
        live_pt.insert("point-circle-" + c.id + "-quadrant-3");
      }
      for (const auto& a : params.arcs) {
        live_pt.insert(a.start_point_id); live_pt.insert(a.end_point_id);
      }
      params.constraints.erase(
          std::remove_if(params.constraints.begin(), params.constraints.end(),
                         [&](const SketchConstraint& c) {
                           if (c.kind != "coincident") return false;
                           for (const auto& tid : c.target_ids)
                             if (!live_pt.count(tid)) return true;
                           return false;
                         }),
          params.constraints.end());
    }

    return;
  }

  throw std::runtime_error("Sketch entity not found: " + entity_id);
}
