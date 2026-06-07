constexpr double kMinimumSketchDimensionValue = 0.001;
constexpr double kCoincidentTolerance = 0.01;

// Forward declaration: re-anchors every midpoint-bound point to its
// host line's current midpoint. Defined further down (depends on
// `propagate_connected_point_move`); called from
// `refresh_sketch_derived_state` so every public mutator picks up
// the enforcement automatically.
void enforce_midpoint_anchors(SketchFeatureParameters& parameters);
void enforce_point_line_anchors(SketchFeatureParameters& parameters);
void enforce_line_HV_constraints(SketchFeatureParameters& parameters);
void enforce_all_equal_length_relations(SketchFeatureParameters& parameters);
void enforce_all_perpendicular_relations(SketchFeatureParameters& parameters);
void enforce_all_parallel_relations(SketchFeatureParameters& parameters);
void enforce_equal_length_relations(SketchFeatureParameters& parameters,
                                    const std::string& seed_line_id);
void enforce_perpendicular_relations(SketchFeatureParameters& parameters,
                                     const std::string& seed_line_id);
void enforce_parallel_relations(SketchFeatureParameters& parameters,
                                const std::string& seed_line_id);
// Slide each tangent-bound line's end point onto the closer of the
// two tangent points from its start to the host circle. Stored as a
// `SketchLineRelation` of kind "tangent_line_circle" with
// `first_line_id` = line id and `second_line_id` = circle id. Run
// from `refresh_sketch_derived_state` so changes to either the line
// start, the circle center, or the radius all keep the relation
// satisfied without explicit re-driving.
void enforce_tangent_line_circle_relations(SketchFeatureParameters& parameters);
void sync_driven_dimensions(SketchFeatureParameters& parameters);

std::string make_parameters_summary(const SketchFeatureParameters& parameters) {
  std::ostringstream stream;
  stream << parameters.plane_id << " · " << parameters.lines.size() << " line";
  if (parameters.lines.size() != 1) {
    stream << "s";
  }
  if (!parameters.circles.empty()) {
    stream << " · " << parameters.circles.size() << " circle";
    if (parameters.circles.size() != 1) {
      stream << "s";
    }
  }
  return stream.str();
}

void validate_line(double start_x,
                   double start_y,
                   double end_x,
                   double end_y) {
  const double dx = end_x - start_x;
  const double dy = end_y - start_y;
  if (std::sqrt(dx * dx + dy * dy) <= 0.001) {
    throw std::runtime_error("Sketch lines must have non-zero length");
  }
}

double measure_line_length(const SketchLine& line) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  return std::sqrt(dx * dx + dy * dy);
}

bool nearly_equal(double left, double right) {
  return std::abs(left - right) <= kCoincidentTolerance;
}

bool points_match(double left_x,
                  double left_y,
                  double right_x,
                  double right_y) {
  return nearly_equal(left_x, right_x) && nearly_equal(left_y, right_y);
}

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

std::optional<std::string> infer_constraint_hint(double start_x,
                                                 double start_y,
                                                 double end_x,
                                                 double end_y) {
  const double dx = std::abs(end_x - start_x);
  const double dy = std::abs(end_y - start_y);
  constexpr double kConstraintTolerance = 0.01;

  if (dx <= kConstraintTolerance) {
    return std::string("vertical");
  }

  if (dy <= kConstraintTolerance) {
    return std::string("horizontal");
  }

  return std::nullopt;
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

  const bool start_fixed = point_is_fixed(parameters, line.start_point_id);
  const bool end_fixed = point_is_fixed(parameters, line.end_point_id);

  if (line.constraint.value() == "horizontal") {
    if (start_fixed && end_fixed && !nearly_equal(line.start_y, line.end_y)) {
      throw std::runtime_error(
          "Cannot make a line horizontal when both endpoints are fixed");
    }
    // Changing an already-vertical line to horizontal collapses it to
    // zero length (X is already equal, now Y becomes equal too).  Clear
    // the constraint instead — same safeguard as enforce_line_HV_constraints.
    if (nearly_equal(line.start_x, line.end_x)) {
      fprintf(stderr, "WARN cleared horizontal constraint on %s "
              "(would create zero-length line)\n", line.id.c_str());
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
      fprintf(stderr, "WARN cleared vertical constraint on %s "
              "(would create zero-length line)\n", line.id.c_str());
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

void validate_constraint(const std::optional<std::string>& constraint) {
  if (!constraint.has_value()) {
    return;
  }

  if (constraint.value() != "horizontal" && constraint.value() != "vertical") {
    throw std::runtime_error("Unsupported sketch constraint: " + constraint.value());
  }
}

void validate_tool(const std::string& tool) {
  if (tool != "select" && tool != "line" && tool != "rectangle" &&
      tool != "circle" && tool != "polygon" && tool != "arc" && tool != "fillet" &&
      tool != "trim" && tool != "project" && tool != "dimension") {
    throw std::runtime_error("Unsupported sketch tool: " + tool + " (validate_tool)");
  }
}

SketchDimension& require_dimension(SketchFeatureParameters& parameters,
                                   const std::string& dimension_id) {
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dimension) { return dimension.id == dimension_id; });

  if (dimension_it == parameters.dimensions.end()) {
    throw std::runtime_error("Sketch dimension not found: " + dimension_id);
  }

  return *dimension_it;
}

SketchLine& require_line(SketchFeatureParameters& parameters,
                         const std::string& line_id) {
  const auto line_it = std::find_if(
      parameters.lines.begin(),
      parameters.lines.end(),
      [&](const SketchLine& line) { return line.id == line_id; });

  if (line_it == parameters.lines.end()) {
    throw std::runtime_error("Sketch line not found: " + line_id);
  }

  return *line_it;
}

SketchCircle& require_circle(SketchFeatureParameters& parameters,
                             const std::string& circle_id) {
  const auto circle_it = std::find_if(
      parameters.circles.begin(),
      parameters.circles.end(),
      [&](const SketchCircle& circle) { return circle.id == circle_id; });

  if (circle_it == parameters.circles.end()) {
    throw std::runtime_error("Sketch circle not found: " + circle_id);
  }

  return *circle_it;
}

double distance_between_circles(const SketchCircle& first,
                                const SketchCircle& second) {
  const double dx = second.center_x - first.center_x;
  const double dy = second.center_y - first.center_y;
  return std::sqrt(dx * dx + dy * dy);
}

double signed_circle_line_distance(const SketchCircle& circle,
                                   const SketchLine& line) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double length = std::sqrt(dx * dx + dy * dy);
  if (length <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Line distance dimension requires a non-zero line");
  }
  const double normal_x = -dy / length;
  const double normal_y = dx / length;
  return (circle.center_x - line.start_x) * normal_x +
         (circle.center_y - line.start_y) * normal_y;
}

double signed_line_line_distance(const SketchLine& driven_line,
                                 const SketchLine& reference_line) {
  const double ref_dx = reference_line.end_x - reference_line.start_x;
  const double ref_dy = reference_line.end_y - reference_line.start_y;
  const double ref_length = std::sqrt(ref_dx * ref_dx + ref_dy * ref_dy);
  if (ref_length <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Line distance dimension requires a non-zero line");
  }
  const double driven_dx = driven_line.end_x - driven_line.start_x;
  const double driven_dy = driven_line.end_y - driven_line.start_y;
  const double driven_length =
      std::sqrt(driven_dx * driven_dx + driven_dy * driven_dy);
  if (driven_length <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Line distance dimension requires a non-zero line");
  }
  const double cross =
      (ref_dx / ref_length) * (driven_dy / driven_length) -
      (ref_dy / ref_length) * (driven_dx / driven_length);
  if (std::abs(cross) > 1e-3) {
    throw std::runtime_error(
        "Line distance dimension requires parallel sketch lines");
  }
  const double normal_x = -ref_dy / ref_length;
  const double normal_y = ref_dx / ref_length;
  const double midpoint_x = (driven_line.start_x + driven_line.end_x) / 2.0;
  const double midpoint_y = (driven_line.start_y + driven_line.end_y) / 2.0;
  return (midpoint_x - reference_line.start_x) * normal_x +
         (midpoint_y - reference_line.start_y) * normal_y;
}

void sync_line_dimension(SketchFeatureParameters& parameters,
                         const SketchLine& line) {
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dimension) {
        return dimension.kind == "line_length" && dimension.entity_id == line.id;
      });

  if (dimension_it != parameters.dimensions.end()) {
    dimension_it->value = measure_line_length(line);
  }
}

void sync_circle_dimension(SketchFeatureParameters& parameters,
                           const SketchCircle& circle) {
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dimension) {
        return dimension.kind == "circle_radius" && dimension.entity_id == circle.id;
      });

  if (dimension_it != parameters.dimensions.end()) {
    dimension_it->value = circle.radius;
  }
}

void drive_line_length(SketchLine& line, double value) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double current_length = std::sqrt(dx * dx + dy * dy);

  if (current_length <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Cannot drive a zero-length sketch line");
  }

  double direction_x = dx / current_length;
  double direction_y = dy / current_length;

  if (line.constraint.has_value()) {
    if (line.constraint.value() == "horizontal") {
      direction_x = dx >= 0.0 ? 1.0 : -1.0;
      direction_y = 0.0;
    } else if (line.constraint.value() == "vertical") {
      direction_x = 0.0;
      direction_y = dy >= 0.0 ? 1.0 : -1.0;
    }
  }

  line.end_x = line.start_x + direction_x * value;
  line.end_y = line.start_y + direction_y * value;
  apply_line_constraint(line);
  validate_line(line.start_x, line.start_y, line.end_x, line.end_y);
}

void drive_line_length_from_fixed_end(SketchLine& line, double value) {
  const double dx = line.start_x - line.end_x;
  const double dy = line.start_y - line.end_y;
  const double current_length = std::sqrt(dx * dx + dy * dy);

  if (current_length <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Cannot drive a zero-length sketch line");
  }

  double direction_x = dx / current_length;
  double direction_y = dy / current_length;

  if (line.constraint.has_value()) {
    if (line.constraint.value() == "horizontal") {
      direction_x = dx >= 0.0 ? 1.0 : -1.0;
      direction_y = 0.0;
    } else if (line.constraint.value() == "vertical") {
      direction_x = 0.0;
      direction_y = dy >= 0.0 ? 1.0 : -1.0;
    }
  }

  line.start_x = line.end_x + direction_x * value;
  line.start_y = line.end_y + direction_y * value;
  if (line.constraint == "horizontal") {
    line.start_y = line.end_y;
  } else if (line.constraint == "vertical") {
    line.start_x = line.end_x;
  }
  validate_line(line.start_x, line.start_y, line.end_x, line.end_y);
}

void drive_line_length_respecting_fixed_points(
    SketchFeatureParameters& parameters,
    SketchLine& line,
    double value) {
  const bool start_fixed = point_is_fixed(parameters, line.start_point_id);
  const bool end_fixed = point_is_fixed(parameters, line.end_point_id);

  if (start_fixed && end_fixed) {
    throw std::runtime_error(
        "Cannot drive a line length when both endpoints are fixed");
  }

  if (end_fixed) {
    drive_line_length_from_fixed_end(line, value);
    return;
  }

  drive_line_length(line, value);
}
