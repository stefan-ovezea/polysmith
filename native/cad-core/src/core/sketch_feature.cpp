#include "core/sketch_feature.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/constraint_solver.h"
#include "core/formula_eval.h"
#include "core/inference_engine.h"
#include "core/sketch_profile.h"
#include "core/trim_engine.h"

namespace polysmith::core {
namespace {

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
  for (auto& point : parameters.points) {
    if (point.kind == "projected" || point.kind == "quadrant") {
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
// configuration to recover.
void enforce_sketch_fillets(SketchFeatureParameters& parameters) {
  for (auto& fillet : parameters.fillets) {
    const auto line_a_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == fillet.line_a_id; });
    const auto line_b_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == fillet.line_b_id; });
    if (line_a_it == parameters.lines.end() ||
        line_b_it == parameters.lines.end()) {
      continue;
    }

    // Identify which end of each line is the trim point. `is_start_a`
    // / `is_start_b` capture this so we know which fields to update
    // when we push the new trim coords back onto the line.
    const bool is_start_a = line_a_it->start_point_id == fillet.trim_a_point_id;
    const bool is_end_a = line_a_it->end_point_id == fillet.trim_a_point_id;
    if (!is_start_a && !is_end_a) {
      continue;
    }
    const bool is_start_b = line_b_it->start_point_id == fillet.trim_b_point_id;
    const bool is_end_b = line_b_it->end_point_id == fillet.trim_b_point_id;
    if (!is_start_b && !is_end_b) {
      continue;
    }

    const double trim_a_x = is_start_a ? line_a_it->start_x : line_a_it->end_x;
    const double trim_a_y = is_start_a ? line_a_it->start_y : line_a_it->end_y;
    const double far_a_x = is_start_a ? line_a_it->end_x : line_a_it->start_x;
    const double far_a_y = is_start_a ? line_a_it->end_y : line_a_it->start_y;
    const double trim_b_x = is_start_b ? line_b_it->start_x : line_b_it->end_x;
    const double trim_b_y = is_start_b ? line_b_it->start_y : line_b_it->end_y;
    const double far_b_x = is_start_b ? line_b_it->end_x : line_b_it->start_x;
    const double far_b_y = is_start_b ? line_b_it->end_y : line_b_it->start_y;

    // Direction A: from far_a toward (and past) the virtual corner.
    // Currently the line ends at trim_a, but the corner is further
    // along the same direction.
    const double dir_ax = trim_a_x - far_a_x;
    const double dir_ay = trim_a_y - far_a_y;
    const double dir_bx = trim_b_x - far_b_x;
    const double dir_by = trim_b_y - far_b_y;
    const double len_a = std::hypot(dir_ax, dir_ay);
    const double len_b = std::hypot(dir_bx, dir_by);
    if (len_a <= kMinimumSketchDimensionValue ||
        len_b <= kMinimumSketchDimensionValue) {
      continue;
    }

    // Virtual corner = intersection of (far_a + t*dir_a) and
    // (far_b + s*dir_b). Solve 2x2 system using the cross-product
    // form. Reject parallel lines (cross ~= 0).
    const double cross = dir_ax * dir_by - dir_ay * dir_bx;
    if (std::abs(cross) <= kMinimumSketchDimensionValue) {
      continue;
    }
    const double dx = far_b_x - far_a_x;
    const double dy = far_b_y - far_a_y;
    const double t = (dx * dir_by - dy * dir_bx) / cross;
    const double corner_x = far_a_x + t * dir_ax;
    const double corner_y = far_a_y + t * dir_ay;

    // Cache the live corner position on the fillet so the next
    // `rebuild_sketch_points` can re-emit the corner point with
    // current coords. See `rebuild_sketch_points` for why.
    fillet.corner_x = corner_x;
    fillet.corner_y = corner_y;

    // Outgoing unit directions from the corner toward each far end.
    const double out_ax = far_a_x - corner_x;
    const double out_ay = far_a_y - corner_y;
    const double out_bx = far_b_x - corner_x;
    const double out_by = far_b_y - corner_y;
    const double out_a_len = std::hypot(out_ax, out_ay);
    const double out_b_len = std::hypot(out_bx, out_by);
    if (out_a_len <= kMinimumSketchDimensionValue ||
        out_b_len <= kMinimumSketchDimensionValue) {
      continue;
    }
    const double ux_a = out_ax / out_a_len;
    const double uy_a = out_ay / out_a_len;
    const double ux_b = out_bx / out_b_len;
    const double uy_b = out_by / out_b_len;

    // Half-angle between the two outgoing directions. theta in
    // (0, pi). Reject configurations that have collapsed (parallel
    // case is caught above; this catches the colinear-but-opposite
    // case where dot ≈ -1 and tan(theta/2) blows up — which would
    // give an infinite trim distance).
    const double dot = ux_a * ux_b + uy_a * uy_b;
    const double clamped_dot = std::max(-1.0, std::min(1.0, dot));
    const double theta = std::acos(clamped_dot);
    if (theta <= kMinimumSketchDimensionValue ||
        theta >= 3.141592653589793 - kMinimumSketchDimensionValue) {
      continue;
    }
    const double half_theta = theta * 0.5;
    const double trim_distance = fillet.radius / std::tan(half_theta);
    if (trim_distance + kMinimumSketchDimensionValue >= out_a_len ||
        trim_distance + kMinimumSketchDimensionValue >= out_b_len) {
      // Trim no longer fits on at least one of the lines. Leave the
      // cached geometry alone for now; the user may resize the
      // lines back into a valid range.
      continue;
    }

    const double new_trim_a_x = corner_x + trim_distance * ux_a;
    const double new_trim_a_y = corner_y + trim_distance * uy_a;
    const double new_trim_b_x = corner_x + trim_distance * ux_b;
    const double new_trim_b_y = corner_y + trim_distance * uy_b;

    // Arc center sits along the angle bisector at distance
    // r / sin(theta/2). The bisector direction is the normalized
    // sum of the two outgoing unit vectors.
    const double bisector_x = ux_a + ux_b;
    const double bisector_y = uy_a + uy_b;
    const double bisector_len = std::hypot(bisector_x, bisector_y);
    if (bisector_len <= kMinimumSketchDimensionValue) {
      continue;
    }
    const double center_distance = fillet.radius / std::sin(half_theta);
    const double arc_center_x =
        corner_x + center_distance * (bisector_x / bisector_len);
    const double arc_center_y =
        corner_y + center_distance * (bisector_y / bisector_len);

    // CCW: positive cross product (start - center) x (end - center)
    // means the sweep from start to end is CCW. Mirrors the
    // convention `add_sketch_arc` already follows.
    const double arc_cross =
        (new_trim_a_x - arc_center_x) * (new_trim_b_y - arc_center_y) -
        (new_trim_a_y - arc_center_y) * (new_trim_b_x - arc_center_x);
    const bool arc_ccw = arc_cross > 0.0;

    // Push back to the line cache.
    if (is_start_a) {
      line_a_it->start_x = new_trim_a_x;
      line_a_it->start_y = new_trim_a_y;
    } else {
      line_a_it->end_x = new_trim_a_x;
      line_a_it->end_y = new_trim_a_y;
    }
    if (is_start_b) {
      line_b_it->start_x = new_trim_b_x;
      line_b_it->start_y = new_trim_b_y;
    } else {
      line_b_it->end_x = new_trim_b_x;
      line_b_it->end_y = new_trim_b_y;
    }

    // Update the generated arc.
    const auto arc_it = std::find_if(
        parameters.arcs.begin(),
        parameters.arcs.end(),
        [&](const SketchArc& arc) { return arc.id == fillet.arc_id; });
    if (arc_it != parameters.arcs.end()) {
      arc_it->start_x = new_trim_a_x;
      arc_it->start_y = new_trim_a_y;
      arc_it->end_x = new_trim_b_x;
      arc_it->end_y = new_trim_b_y;
      arc_it->center_x = arc_center_x;
      arc_it->center_y = arc_center_y;
      arc_it->radius = fillet.radius;
      arc_it->ccw = arc_ccw;
    }
  }
}

// Forward-declared in `sketch_feature.h` so the document.cpp Project
// tool path can re-run the recompute pipeline after appending to
// `projected_points`. Defined below in the public namespace; the
// closing brace of the anonymous namespace is right above this
// comment.
std::vector<std::string> collect_line_ids_for_point(
    const SketchFeatureParameters& parameters,
    const std::string& point_id) {
  std::vector<std::string> line_ids;

  for (const auto& line : parameters.lines) {
    if (line.start_point_id == point_id || line.end_point_id == point_id) {
      line_ids.push_back(line.id);
    }
  }

  return line_ids;
}

void replace_point_id_references(SketchFeatureParameters& parameters,
                                 const std::string& from_point_id,
                                 const std::string& to_point_id) {
  if (from_point_id == to_point_id) {
    return;
  }

  const bool merged_fixed =
      point_is_fixed(parameters, from_point_id) || point_is_fixed(parameters, to_point_id);

  for (auto& line : parameters.lines) {
    if (line.start_point_id == from_point_id) {
      line.start_point_id = to_point_id;
    }
    if (line.end_point_id == from_point_id) {
      line.end_point_id = to_point_id;
    }
  }

  if (SketchPoint* target_point = find_sketch_point(parameters, to_point_id)) {
    target_point->is_fixed = merged_fixed;
  }
}

void set_endpoint(SketchLine& line, bool is_start, double x, double y) {
  if (is_start) {
    line.start_x = x;
    line.start_y = y;
    return;
  }

  line.end_x = x;
  line.end_y = y;
}

void set_endpoint_with_constraint(SketchLine& line,
                                  bool is_start,
                                  double x,
                                  double y) {
  set_endpoint(line, is_start, x, y);

  if (line.constraint == "horizontal") {
    if (is_start) {
      line.end_y = y;
    } else {
      line.start_y = y;
    }
    return;
  }

  if (line.constraint == "vertical") {
    if (is_start) {
      line.end_x = x;
    } else {
      line.start_x = x;
    }
  }
}

void restore_fixed_line_endpoints(SketchFeatureParameters& parameters,
                                  SketchLine& line,
                                  double previous_start_x,
                                  double previous_start_y,
                                  double previous_end_x,
                                  double previous_end_y) {
  if (point_is_fixed(parameters, line.start_point_id)) {
    line.start_x = previous_start_x;
    line.start_y = previous_start_y;
  }

  if (point_is_fixed(parameters, line.end_point_id)) {
    line.end_x = previous_end_x;
    line.end_y = previous_end_y;
  }
}

void snap_line_endpoints_to_coincident_geometry(
    SketchFeatureParameters& parameters,
    SketchLine& line,
    bool snap_start = true) {
  if (snap_start) {
    const auto snapped_start =
        find_coincident_endpoint(parameters, line.id, line.start_x, line.start_y);
    if (snapped_start.has_value() &&
        !point_is_fixed(parameters, line.start_point_id)) {
      const bool can_snap_start =
          !((line.constraint == "horizontal" &&
             point_is_fixed(parameters, line.end_point_id) &&
             !nearly_equal(std::get<2>(snapped_start.value()), line.end_y)) ||
            (line.constraint == "vertical" &&
             point_is_fixed(parameters, line.end_point_id) &&
             !nearly_equal(std::get<1>(snapped_start.value()), line.end_x)));
      if (can_snap_start) {
        const std::string& snapped_point_id = std::get<0>(snapped_start.value());
        const std::string current_point_id = line.start_point_id;
        if (!current_point_id.empty() && current_point_id != snapped_point_id) {
          replace_point_id_references(parameters, current_point_id, snapped_point_id);
        }
        line.start_point_id = snapped_point_id;
        set_endpoint_with_constraint(line,
                                     true,
                                     std::get<1>(snapped_start.value()),
                                     std::get<2>(snapped_start.value()));
      }
    }
  }

  const auto snapped_end =
      find_coincident_endpoint(parameters, line.id, line.end_x, line.end_y);
  if (snapped_end.has_value() &&
      !point_is_fixed(parameters, line.end_point_id)) {
    const bool can_snap_end =
        !((line.constraint == "horizontal" &&
           point_is_fixed(parameters, line.start_point_id) &&
           !nearly_equal(std::get<2>(snapped_end.value()), line.start_y)) ||
          (line.constraint == "vertical" &&
           point_is_fixed(parameters, line.start_point_id) &&
           !nearly_equal(std::get<1>(snapped_end.value()), line.start_x)));
    if (can_snap_end) {
      const std::string& snapped_point_id = std::get<0>(snapped_end.value());
      const std::string current_point_id = line.end_point_id;
      if (!current_point_id.empty() && current_point_id != snapped_point_id) {
        replace_point_id_references(parameters, current_point_id, snapped_point_id);
      }
      line.end_point_id = snapped_point_id;
      set_endpoint_with_constraint(line,
                                   false,
                                   std::get<1>(snapped_end.value()),
                                   std::get<2>(snapped_end.value()));
    }
  }
}

void propagate_connected_point_move(SketchFeatureParameters& parameters,
                                    const std::string& point_id,
                                    double target_x,
                                    double target_y) {
  struct PendingMove {
    std::string point_id;
    double to_x;
    double to_y;
  };

  std::deque<PendingMove> frontier = {{
      .point_id = point_id,
      .to_x = target_x,
      .to_y = target_y,
  }};
  std::vector<std::string> visited_points;

  while (!frontier.empty()) {
    const auto move = frontier.front();
    frontier.pop_front();

    const auto current_point_position = find_point_position(parameters, move.point_id);
    if (point_is_fixed(parameters, move.point_id) &&
        current_point_position.has_value() &&
        !points_match(std::get<0>(current_point_position.value()),
                      std::get<1>(current_point_position.value()),
                      move.to_x,
                      move.to_y)) {
      continue;
    }

    const bool already_visited = std::any_of(
        visited_points.begin(),
        visited_points.end(),
        [&](const auto& visited) { return visited == move.point_id; });
    if (already_visited) {
      continue;
    }
    visited_points.push_back(move.point_id);

    std::vector<LineEndpointRef> connected_endpoints;
    for (size_t line_index = 0; line_index < parameters.lines.size(); ++line_index) {
      const auto& line = parameters.lines[line_index];
      if (line.start_point_id == move.point_id) {
        connected_endpoints.push_back({
            .line_index = line_index,
            .is_start = true,
        });
      }
      if (line.end_point_id == move.point_id) {
        connected_endpoints.push_back({
            .line_index = line_index,
            .is_start = false,
        });
      }
    }

    for (const auto& endpoint_ref : connected_endpoints) {
      auto& line = parameters.lines[endpoint_ref.line_index];
      const bool moved_start = endpoint_ref.is_start;
      const bool moved_end = !endpoint_ref.is_start;
      const double previous_other_x =
          moved_start ? line.end_x : line.start_x;
      const double previous_other_y =
          moved_start ? line.end_y : line.start_y;

      set_endpoint(line, endpoint_ref.is_start, move.to_x, move.to_y);

      // When a line has a horizontal or vertical constraint and only
      // one endpoint is being dragged, keep the other endpoint anchored
      // and snap the dragged endpoint along the constraint axis.  This
      // lets the line stretch / shorten while staying aligned, rather
      // than rigidly translating the whole line (the old behaviour).
      const std::string other_endpoint_point_id =
          endpoint_point_id(line, !endpoint_ref.is_start);
      const bool other_endpoint_anchored =
          point_is_anchored_to_line(parameters, other_endpoint_point_id);

      if (line.constraint == "horizontal" && !other_endpoint_anchored) {
        if (moved_start) {
          line.start_y = line.end_y;
        } else if (moved_end) {
          line.end_y = line.start_y;
        }
      } else if (line.constraint == "vertical" && !other_endpoint_anchored) {
        if (moved_start) {
          line.start_x = line.end_x;
        } else if (moved_end) {
          line.end_x = line.start_x;
        }
      }

      validate_line(line.start_x, line.start_y, line.end_x, line.end_y);

      const double next_other_x = moved_start ? line.end_x : line.start_x;
      const double next_other_y = moved_start ? line.end_y : line.start_y;
      if (!points_match(previous_other_x,
                        previous_other_y,
                        next_other_x,
                        next_other_y)) {
        frontier.push_back({
            .point_id = endpoint_point_id(line, !endpoint_ref.is_start),
            .to_x = next_other_x,
            .to_y = next_other_y,
        });
      }
    }
  }
}

// Enforce H/V constraints on every line. Runs during refresh so
// constraint badges always match the actual geometry. If enforcement
// would collapse a line to zero length, the constraint is cleared
// instead — geometry is never destroyed to satisfy a constraint.
//
// Endpoints are set directly on the line without propagation through
// shared points — reconcile_shared_point_positions (called later in
// refresh) handles shared-point consistency. Propagation would
// cascade destructively through connected constrained lines (e.g. a
// rectangle corner drag distorting the opposite corner).
void enforce_line_HV_constraints(SketchFeatureParameters& parameters) {
  for (auto& line : parameters.lines) {
    if (!line.constraint.has_value()) continue;

    if (line.constraint.value() == "horizontal") {
      if (nearly_equal(line.start_y, line.end_y)) continue;
      if (points_match(line.start_x, line.start_y, line.end_x, line.start_y)) {
        line.constraint = std::nullopt;
        fprintf(stderr, "WARN cleared horizontal constraint on %s "
                "(would create zero-length line)\n", line.id.c_str());
        continue;
      }
      if (point_is_fixed(parameters, line.end_point_id)) {
        line.start_y = line.end_y;
      } else {
        line.end_y = line.start_y;
      }
    } else if (line.constraint.value() == "vertical") {
      if (nearly_equal(line.start_x, line.end_x)) continue;
      if (points_match(line.start_x, line.start_y, line.start_x, line.end_y)) {
        line.constraint = std::nullopt;
        fprintf(stderr, "WARN cleared vertical constraint on %s "
                "(would create zero-length line)\n", line.id.c_str());
        continue;
      }
      if (point_is_fixed(parameters, line.end_point_id)) {
        line.start_x = line.end_x;
      } else {
        line.end_x = line.start_x;
      }
    }
  }
}

// Seedless wrappers for the relation-enforcement functions so they
// can be called from refresh without knowing a specific line.
void enforce_all_equal_length_relations(SketchFeatureParameters& parameters) {
  for (const auto& line : parameters.lines) {
    enforce_equal_length_relations(parameters, line.id);
  }
}
void enforce_all_perpendicular_relations(SketchFeatureParameters& parameters) {
  for (const auto& line : parameters.lines) {
    enforce_perpendicular_relations(parameters, line.id);
  }
}
void enforce_all_parallel_relations(SketchFeatureParameters& parameters) {
  for (const auto& line : parameters.lines) {
    enforce_parallel_relations(parameters, line.id);
  }
}

void sync_all_line_dimensions(SketchFeatureParameters& parameters) {
  for (const auto& line : parameters.lines) {
    sync_line_dimension(parameters, line);
  }
}

void sync_driven_dimensions(SketchFeatureParameters& parameters) {
  for (auto& dimension : parameters.dimensions) {
    if (!dimension.driven) {
      continue;
    }
    if (dimension.kind == "line_length") {
      const auto& line = require_line(parameters, dimension.entity_id);
      dimension.value = measure_line_length(line);
    } else if (dimension.kind == "circle_radius") {
      const auto& circle = require_circle(parameters, dimension.entity_id);
      dimension.value = circle.radius;
    } else if (dimension.kind == "polygon_radius") {
      const auto polygon_it = std::find_if(
          parameters.polygons.begin(),
          parameters.polygons.end(),
          [&](const SketchPolygon& polygon) {
            return polygon.id == dimension.entity_id;
          });
      if (polygon_it != parameters.polygons.end()) {
        dimension.value = polygon_it->radius;
      }
    } else if (dimension.kind == "line_angle") {
      const auto& line = require_line(parameters, dimension.entity_id);
      const double dx = line.end_x - line.start_x;
      const double dy = line.end_y - line.start_y;
      dimension.value = std::atan2(dy, dx);
    } else if (dimension.kind == "angle") {
      const auto& line_a = require_line(parameters, dimension.entity_id);
      const auto& line_b =
          require_line(parameters, dimension.secondary_entity_id);
      // Find shared endpoint and compute signed angle.
      const std::array<std::pair<double, double>, 2> a_ends = {{
          {line_a.start_x, line_a.start_y},
          {line_a.end_x, line_a.end_y},
      }};
      const std::array<std::pair<double, double>, 2> b_ends = {{
          {line_b.start_x, line_b.start_y},
          {line_b.end_x, line_b.end_y},
      }};
      int a_pivot = -1;
      int b_pivot = -1;
      for (int i = 0; i < 2 && a_pivot < 0; ++i) {
        for (int j = 0; j < 2; ++j) {
          if (std::abs(a_ends[i].first - b_ends[j].first) <=
                  kCoincidentTolerance &&
              std::abs(a_ends[i].second - b_ends[j].second) <=
                  kCoincidentTolerance) {
            a_pivot = i;
            b_pivot = j;
            break;
          }
        }
      }
      if (a_pivot >= 0) {
        const double px = a_ends[a_pivot].first;
        const double py = a_ends[a_pivot].second;
        const double a_dx = a_ends[1 - a_pivot].first - px;
        const double a_dy = a_ends[1 - a_pivot].second - py;
        const double b_dx = b_ends[1 - b_pivot].first - px;
        const double b_dy = b_ends[1 - b_pivot].second - py;
        // Recompute the signed angle from A to B.
        dimension.value =
            std::atan2(a_dx * b_dy - a_dy * b_dx,
                       a_dx * b_dx + a_dy * b_dy);
      }
    } else if (dimension.kind == "line_line_distance") {
      const auto& driven_line =
          require_line(parameters, dimension.entity_id);
      const auto& reference_line =
          require_line(parameters, dimension.secondary_entity_id);
      dimension.value =
          std::abs(signed_line_line_distance(driven_line, reference_line));
    } else if (dimension.kind == "circle_center_distance") {
      const auto& driven_circle =
          require_circle(parameters, dimension.entity_id);
      const auto& reference_circle =
          require_circle(parameters, dimension.secondary_entity_id);
      dimension.value =
          distance_between_circles(driven_circle, reference_circle);
    } else if (dimension.kind == "circle_line_distance") {
      const auto& circle = require_circle(parameters, dimension.entity_id);
      const auto& line =
          require_line(parameters, dimension.secondary_entity_id);
      dimension.value =
          std::abs(signed_circle_line_distance(circle, line));
    } else if (dimension.kind == "point_distance") {
      const auto point_a_it = std::find_if(
          parameters.points.begin(),
          parameters.points.end(),
          [&](const SketchPoint& p) {
            return p.id == dimension.entity_id;
          });
      const auto point_b_it = std::find_if(
          parameters.points.begin(),
          parameters.points.end(),
          [&](const SketchPoint& p) {
            return p.id == dimension.secondary_entity_id;
          });
      if (point_a_it != parameters.points.end() &&
          point_b_it != parameters.points.end()) {
        const double dx = point_b_it->x - point_a_it->x;
        const double dy = point_b_it->y - point_a_it->y;
        dimension.value = std::sqrt(dx * dx + dy * dy);
      }
    }
  }
}

void remove_line_relations_for_line(SketchFeatureParameters& parameters,
                                    const std::string& kind,
                                    const std::string& line_id) {
  parameters.line_relations.erase(
      std::remove_if(parameters.line_relations.begin(),
                     parameters.line_relations.end(),
                     [&](const SketchLineRelation& relation) {
                       return relation.kind == kind &&
                              (relation.first_line_id == line_id ||
                               relation.second_line_id == line_id);
                     }),
      parameters.line_relations.end());
}

void enforce_equal_length_relations(SketchFeatureParameters& parameters,
                                    const std::string& seed_line_id) {
  std::deque<std::string> frontier = {seed_line_id};
  std::vector<std::string> visited_line_ids;

  while (!frontier.empty()) {
    const std::string current_line_id = frontier.front();
    frontier.pop_front();

    if (std::find(visited_line_ids.begin(),
                  visited_line_ids.end(),
                  current_line_id) != visited_line_ids.end()) {
      continue;
    }
    visited_line_ids.push_back(current_line_id);

    auto& reference_line = require_line(parameters, current_line_id);
    const double target_length = measure_line_length(reference_line);

    for (const auto& relation : parameters.line_relations) {
      if (relation.kind != "equal_length") {
        continue;
      }

      const bool matches_first = relation.first_line_id == current_line_id;
      const bool matches_second = relation.second_line_id == current_line_id;
      if (!matches_first && !matches_second) {
        continue;
      }

      const std::string other_line_id =
          matches_first ? relation.second_line_id : relation.first_line_id;
      auto& driven_line = require_line(parameters, other_line_id);
      const double previous_start_x = driven_line.start_x;
      const double previous_start_y = driven_line.start_y;
      const double previous_end_x = driven_line.end_x;
      const double previous_end_y = driven_line.end_y;

      drive_line_length_respecting_fixed_points(
          parameters, driven_line, target_length);
      snap_line_endpoints_to_coincident_geometry(parameters, driven_line);
      validate_line(driven_line.start_x,
                    driven_line.start_y,
                    driven_line.end_x,
                    driven_line.end_y);

      if (!points_match(previous_start_x,
                        previous_start_y,
                        driven_line.start_x,
                        driven_line.start_y)) {
        propagate_connected_point_move(parameters,
                                       driven_line.start_point_id,
                                       driven_line.start_x,
                                       driven_line.start_y);
      }

      if (!points_match(previous_end_x,
                        previous_end_y,
                        driven_line.end_x,
                        driven_line.end_y)) {
        propagate_connected_point_move(parameters,
                                       driven_line.end_point_id,
                                       driven_line.end_x,
                                       driven_line.end_y);
      }

      sync_line_dimension(parameters, driven_line);
      frontier.push_back(other_line_id);
    }
  }
}

void drive_line_perpendicular_to_reference(SketchLine& driven_line,
                                           const SketchLine& reference_line,
                                           const SketchFeatureParameters& parameters) {
  // Skip silently when the driven line still carries an H/V
  // constraint — its angle is already fully determined by the axis
  // constraint, so there is nothing for the relation to drive.
  if (driven_line.constraint.has_value()) {
    return;
  }

  const double reference_dx = reference_line.end_x - reference_line.start_x;
  const double reference_dy = reference_line.end_y - reference_line.start_y;
  const double reference_length =
      std::sqrt(reference_dx * reference_dx + reference_dy * reference_dy);
  const double driven_length = measure_line_length(driven_line);

  if (reference_length <= kMinimumSketchDimensionValue ||
      driven_length <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Perpendicular relations require non-zero length lines");
  }

  const double current_dx = driven_line.end_x - driven_line.start_x;
  const double current_dy = driven_line.end_y - driven_line.start_y;
  const double candidate_a_x = -reference_dy / reference_length;
  const double candidate_a_y = reference_dx / reference_length;
  const double candidate_b_x = -candidate_a_x;
  const double candidate_b_y = -candidate_a_y;
  const double dot_a = current_dx * candidate_a_x + current_dy * candidate_a_y;
  const double dot_b = current_dx * candidate_b_x + current_dy * candidate_b_y;
  const double direction_x = dot_a >= dot_b ? candidate_a_x : candidate_b_x;
  const double direction_y = dot_a >= dot_b ? candidate_a_y : candidate_b_y;

  const bool start_fixed = point_is_fixed(parameters, driven_line.start_point_id);
  const bool end_fixed = point_is_fixed(parameters, driven_line.end_point_id);
  if (start_fixed && end_fixed) {
    throw std::runtime_error(
        "Cannot drive a perpendicular relation when both endpoints are fixed");
  }

  if (end_fixed) {
    driven_line.start_x = driven_line.end_x - direction_x * driven_length;
    driven_line.start_y = driven_line.end_y - direction_y * driven_length;
  } else {
    driven_line.end_x = driven_line.start_x + direction_x * driven_length;
    driven_line.end_y = driven_line.start_y + direction_y * driven_length;
  }
  validate_line(
      driven_line.start_x, driven_line.start_y, driven_line.end_x, driven_line.end_y);
}

void drive_line_parallel_to_reference(SketchLine& driven_line,
                                      const SketchLine& reference_line,
                                      const SketchFeatureParameters& parameters) {
  // Skip silently when the driven line still carries an H/V
  // constraint — its angle is already fully determined by the axis
  // constraint, so there is nothing for the relation to drive.
  if (driven_line.constraint.has_value()) {
    return;
  }

  const double reference_dx = reference_line.end_x - reference_line.start_x;
  const double reference_dy = reference_line.end_y - reference_line.start_y;
  const double reference_length =
      std::sqrt(reference_dx * reference_dx + reference_dy * reference_dy);
  const double driven_length = measure_line_length(driven_line);

  if (reference_length <= kMinimumSketchDimensionValue ||
      driven_length <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Parallel relations require non-zero length lines");
  }

  const double current_dx = driven_line.end_x - driven_line.start_x;
  const double current_dy = driven_line.end_y - driven_line.start_y;
  const double candidate_a_x = reference_dx / reference_length;
  const double candidate_a_y = reference_dy / reference_length;
  const double candidate_b_x = -candidate_a_x;
  const double candidate_b_y = -candidate_a_y;
  const double dot_a = current_dx * candidate_a_x + current_dy * candidate_a_y;
  const double dot_b = current_dx * candidate_b_x + current_dy * candidate_b_y;
  const double direction_x = dot_a >= dot_b ? candidate_a_x : candidate_b_x;
  const double direction_y = dot_a >= dot_b ? candidate_a_y : candidate_b_y;

  const bool start_fixed = point_is_fixed(parameters, driven_line.start_point_id);
  const bool end_fixed = point_is_fixed(parameters, driven_line.end_point_id);
  if (start_fixed && end_fixed) {
    throw std::runtime_error(
        "Cannot drive a parallel relation when both endpoints are fixed");
  }

  if (end_fixed) {
    driven_line.start_x = driven_line.end_x - direction_x * driven_length;
    driven_line.start_y = driven_line.end_y - direction_y * driven_length;
  } else {
    driven_line.end_x = driven_line.start_x + direction_x * driven_length;
    driven_line.end_y = driven_line.start_y + direction_y * driven_length;
  }
  validate_line(
      driven_line.start_x, driven_line.start_y, driven_line.end_x, driven_line.end_y);
}

// Pull every midpoint-anchored point to its host line's current
// midpoint, propagating the move through connected lines via the
// existing endpoint-cascade machinery. Safe to call repeatedly:
// when the host line itself doesn't change between calls, the
// computed target equals the current point position so the inner
// propagation is a no-op.
void enforce_midpoint_anchors(SketchFeatureParameters& parameters) {
  for (const auto& anchor : parameters.midpoint_anchors) {
    const auto host_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == anchor.line_id; });
    if (host_it == parameters.lines.end()) {
      continue;
    }
    const double mid_x = (host_it->start_x + host_it->end_x) / 2.0;
    const double mid_y = (host_it->start_y + host_it->end_y) / 2.0;
    propagate_connected_point_move(parameters, anchor.point_id, mid_x, mid_y);
  }
}

// Pull every line-anchored point back to its parametric position
// along the host line. Mirrors `enforce_midpoint_anchors` but uses
// the stored `t` instead of always 0.5, so the user can anchor at
// any fraction along the line and have it ride with edits.
void enforce_point_line_anchors(SketchFeatureParameters& parameters) {
  for (const auto& anchor : parameters.point_line_anchors) {
    const auto host_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == anchor.line_id; });
    if (host_it == parameters.lines.end()) {
      continue;
    }
    const double tx =
        host_it->start_x + anchor.t * (host_it->end_x - host_it->start_x);
    const double ty =
        host_it->start_y + anchor.t * (host_it->end_y - host_it->start_y);
    propagate_connected_point_move(parameters, anchor.point_id, tx, ty);
  }
}

// Re-projects each tangent-bound line's end onto the closer of the
// two tangent points from its start to the host circle. The line's
// start, the circle center, and the radius can all change between
// refreshes; recomputing the tangent on every pass keeps the line
// geometrically tangent without us having to know which input
// changed. The branch selection (T₁ vs T₂) uses the current end as
// a tiebreaker so the line doesn't flip across the circle on small
// edits — flips can still happen if the user drags the start
// through the circle interior, but the user-visible result matches
// what they'd expect from a "stickier" tangent.
void enforce_tangent_line_circle_relations(
    SketchFeatureParameters& parameters) {
  for (const auto& relation : parameters.line_relations) {
    if (relation.kind != "tangent_line_circle") {
      continue;
    }
    const auto line_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) {
          return line.id == relation.first_line_id;
        });
    if (line_it == parameters.lines.end()) {
      continue;
    }
    const auto circle_it = std::find_if(
        parameters.circles.begin(),
        parameters.circles.end(),
        [&](const SketchCircle& circle) {
          return circle.id == relation.second_line_id;
        });
    if (circle_it == parameters.circles.end()) {
      continue;
    }

    const double sx = line_it->start_x;
    const double sy = line_it->start_y;
    const double cx = circle_it->center_x;
    const double cy = circle_it->center_y;
    const double r = circle_it->radius;
    const double dx = cx - sx;
    const double dy = cy - sy;
    const double d_squared = dx * dx + dy * dy;
    // Start lies inside (or on) the circle: no real tangent line
    // through it. Skip rather than throw — the user might be
    // mid-drag and the geometry will become valid again. Validation
    // at op-add time prevents creating tangents that are
    // permanently degenerate.
    if (d_squared <= r * r + 1e-12) {
      continue;
    }
    const double d = std::sqrt(d_squared);
    const double tangent_length = std::sqrt(d_squared - r * r);
    // Unit vector from start toward center, plus its left-perp.
    const double ux = dx / d;
    const double uy = dy / d;
    const double vx = -uy;
    const double vy = ux;
    // Tangent point T = S + (L²/d) * u ± (L*r/d) * v. Derivation:
    // (T-C) ⊥ (T-S) and |T-C| = r, so T sits at angle θ off the
    // S→C ray with sin θ = r/d, cos θ = L/d, at distance L from S.
    const double along = (tangent_length * tangent_length) / d;
    const double perp = (tangent_length * r) / d;
    const double t1x = sx + along * ux + perp * vx;
    const double t1y = sy + along * uy + perp * vy;
    const double t2x = sx + along * ux - perp * vx;
    const double t2y = sy + along * uy - perp * vy;

    const double end_x = line_it->end_x;
    const double end_y = line_it->end_y;
    const double dist1_sq =
        (end_x - t1x) * (end_x - t1x) + (end_y - t1y) * (end_y - t1y);
    const double dist2_sq =
        (end_x - t2x) * (end_x - t2x) + (end_y - t2y) * (end_y - t2y);
    const double tx = dist1_sq <= dist2_sq ? t1x : t2x;
    const double ty = dist1_sq <= dist2_sq ? t1y : t2y;

    if (points_match(end_x, end_y, tx, ty)) {
      continue;
    }
    propagate_connected_point_move(
        parameters, line_it->end_point_id, tx, ty);
  }
}

void enforce_perpendicular_relations(SketchFeatureParameters& parameters,
                                     const std::string& seed_line_id) {
  std::deque<std::string> frontier = {seed_line_id};
  std::vector<std::string> visited_line_ids;

  while (!frontier.empty()) {
    const std::string current_line_id = frontier.front();
    frontier.pop_front();

    if (std::find(visited_line_ids.begin(),
                  visited_line_ids.end(),
                  current_line_id) != visited_line_ids.end()) {
      continue;
    }
    visited_line_ids.push_back(current_line_id);

    auto& reference_line = require_line(parameters, current_line_id);

    for (const auto& relation : parameters.line_relations) {
      if (relation.kind != "perpendicular") {
        continue;
      }

      const bool matches_first = relation.first_line_id == current_line_id;
      const bool matches_second = relation.second_line_id == current_line_id;
      if (!matches_first && !matches_second) {
        continue;
      }

      const std::string other_line_id =
          matches_first ? relation.second_line_id : relation.first_line_id;
      auto& driven_line = require_line(parameters, other_line_id);
      const double previous_start_x = driven_line.start_x;
      const double previous_start_y = driven_line.start_y;
      const double previous_end_x = driven_line.end_x;
      const double previous_end_y = driven_line.end_y;

      drive_line_perpendicular_to_reference(
          driven_line, reference_line, parameters);
      snap_line_endpoints_to_coincident_geometry(parameters, driven_line);
      validate_line(driven_line.start_x,
                    driven_line.start_y,
                    driven_line.end_x,
                    driven_line.end_y);

      if (!points_match(previous_start_x,
                        previous_start_y,
                        driven_line.start_x,
                        driven_line.start_y)) {
        propagate_connected_point_move(parameters,
                                       driven_line.start_point_id,
                                       driven_line.start_x,
                                       driven_line.start_y);
      }

      if (!points_match(previous_end_x,
                        previous_end_y,
                        driven_line.end_x,
                        driven_line.end_y)) {
        propagate_connected_point_move(parameters,
                                       driven_line.end_point_id,
                                       driven_line.end_x,
                                       driven_line.end_y);
      }

      sync_line_dimension(parameters, driven_line);
      frontier.push_back(other_line_id);
    }
  }
}

void enforce_parallel_relations(SketchFeatureParameters& parameters,
                                const std::string& seed_line_id) {
  std::deque<std::string> frontier = {seed_line_id};
  std::vector<std::string> visited_line_ids;

  while (!frontier.empty()) {
    const std::string current_line_id = frontier.front();
    frontier.pop_front();

    if (std::find(visited_line_ids.begin(),
                  visited_line_ids.end(),
                  current_line_id) != visited_line_ids.end()) {
      continue;
    }
    visited_line_ids.push_back(current_line_id);

    auto& reference_line = require_line(parameters, current_line_id);

    for (const auto& relation : parameters.line_relations) {
      if (relation.kind != "parallel") {
        continue;
      }

      const bool matches_first = relation.first_line_id == current_line_id;
      const bool matches_second = relation.second_line_id == current_line_id;
      if (!matches_first && !matches_second) {
        continue;
      }

      const std::string other_line_id =
          matches_first ? relation.second_line_id : relation.first_line_id;
      auto& driven_line = require_line(parameters, other_line_id);
      const double previous_start_x = driven_line.start_x;
      const double previous_start_y = driven_line.start_y;
      const double previous_end_x = driven_line.end_x;
      const double previous_end_y = driven_line.end_y;

      drive_line_parallel_to_reference(driven_line, reference_line, parameters);
      snap_line_endpoints_to_coincident_geometry(parameters, driven_line);
      validate_line(driven_line.start_x,
                    driven_line.start_y,
                    driven_line.end_x,
                    driven_line.end_y);

      if (!points_match(previous_start_x,
                        previous_start_y,
                        driven_line.start_x,
                        driven_line.start_y)) {
        propagate_connected_point_move(parameters,
                                       driven_line.start_point_id,
                                       driven_line.start_x,
                                       driven_line.start_y);
      }

      if (!points_match(previous_end_x,
                        previous_end_y,
                        driven_line.end_x,
                        driven_line.end_y)) {
        propagate_connected_point_move(parameters,
                                       driven_line.end_point_id,
                                       driven_line.end_x,
                                       driven_line.end_y);
      }

      sync_line_dimension(parameters, driven_line);
      frontier.push_back(other_line_id);
    }
  }
}

// After constraint enforcement, multiple lines that share a point ID may
// have been driven to different (x, y) coordinates for that shared point.
// `rebuild_sketch_points` deduplicates by point ID and renders a single
// sphere at whichever line's coordinates it encounters first — leaving the
// other line's endpoint visually orphaned.  This pass forces every shared
// point to a single canonical position before the rebuild so the sphere
// matches all lines that reference it.
void reconcile_shared_point_positions(SketchFeatureParameters& parameters) {
  // Build a map from point_id → list of (line_index, is_start_endpoint)
  struct Reference { size_t line_index; bool is_start; };
  std::unordered_map<std::string, std::vector<Reference>> shared;

  for (size_t i = 0; i < parameters.lines.size(); ++i) {
    const auto& line = parameters.lines[i];
    shared[line.start_point_id].push_back({i, true});
    shared[line.end_point_id].push_back({i, false});
  }

  for (const auto& [point_id, refs] : shared) {
    if (refs.size() <= 1) continue;

    // Pick the canonical position.  Priority:
    //   1. A line with a fixed endpoint at this point (authoritative).
    //   2. A line with an H/V constraint (angle is determined).
    //   3. The first reference (fallback).
    size_t canonical_ref = 0;
    bool found_authoritative = false;
    for (size_t r = 0; r < refs.size(); ++r) {
      const auto& l = parameters.lines[refs[r].line_index];
      const std::string& pt =
          refs[r].is_start ? l.start_point_id : l.end_point_id;
      if (point_is_fixed(parameters, pt)) {
        canonical_ref = r;
        found_authoritative = true;
        break;
      }
    }
    if (!found_authoritative) {
      for (size_t r = 0; r < refs.size(); ++r) {
        if (parameters.lines[refs[r].line_index].constraint.has_value()) {
          canonical_ref = r;
          break;
        }
      }
    }

    const auto& canonical_line = parameters.lines[refs[canonical_ref].line_index];
    const double canonical_x = refs[canonical_ref].is_start
        ? canonical_line.start_x : canonical_line.end_x;
    const double canonical_y = refs[canonical_ref].is_start
        ? canonical_line.start_y : canonical_line.end_y;

    for (size_t r = 1; r < refs.size(); ++r) {
      auto& line = parameters.lines[refs[r].line_index];
      const double current_x = refs[r].is_start ? line.start_x : line.end_x;
      const double current_y = refs[r].is_start ? line.start_y : line.end_y;

      if (!points_match(canonical_x, canonical_y, current_x, current_y)) {
        if (refs[r].is_start) {
          line.start_x = canonical_x;
          line.start_y = canonical_y;
        } else {
          line.end_x = canonical_x;
          line.end_y = canonical_y;
        }
      }
    }
  }
}

}  // namespace

void refresh_sketch_derived_state(FeatureEntry& feature) {
  if (!feature.sketch_parameters.has_value()) {
    return;
  }

  // Drop anchors whose host line has been deleted.  Anchors whose
  // anchored point is no longer referenced by any live entity are
  // harmless (they are a no‑op during enforcement and the point is
  // dropped by `rebuild_sketch_points`), but cleaning them up here
  // keeps the data model tidy and prevents stale viewport sprites.
  auto& p = *feature.sketch_parameters;
  p.midpoint_anchors.erase(
      std::remove_if(p.midpoint_anchors.begin(),
                     p.midpoint_anchors.end(),
                     [&](const SketchMidpointAnchor& a) {
                       return std::none_of(
                           p.lines.begin(), p.lines.end(),
                           [&](const SketchLine& l) { return l.id == a.line_id; });
                     }),
      p.midpoint_anchors.end());
  p.point_line_anchors.erase(
      std::remove_if(p.point_line_anchors.begin(),
                     p.point_line_anchors.end(),
                     [&](const SketchPointLineAnchor& a) {
                       return std::none_of(
                           p.lines.begin(), p.lines.end(),
                           [&](const SketchLine& l) { return l.id == a.line_id; });
                     }),
      p.point_line_anchors.end());

  // Clean up zero‑length lines — a symptom of a constraint‑resolver
  // or trim bug. Collect their ids first, then erase the lines and
  // every piece of associated data (constraints, dimensions, relations,
  // anchors, fillets) so they don't keep re‑logging on every refresh.
  {
    std::vector<std::string> zero_ids;
    for (const auto& line : p.lines) {
      if (points_match(line.start_x, line.start_y, line.end_x, line.end_y)) {
        zero_ids.push_back(line.id);
      }
    }
    if (!zero_ids.empty()) {
      for (const auto& zid : zero_ids) {
        fprintf(stderr, "ERROR deleted zero-length line %s\n", zid.c_str());

        // Erase constraints that target this line or its endpoints.
        std::string zsp_id;
        std::string zep_id;
        for (const auto& line : p.lines) {
          if (line.id == zid) {
            zsp_id = line.start_point_id;
            zep_id = line.end_point_id;
            break;
          }
        }
        p.constraints.erase(
            std::remove_if(p.constraints.begin(), p.constraints.end(),
                           [&](const SketchConstraint& c) {
                             for (const auto& tid : c.target_ids) {
                               if (tid == zid || tid == zsp_id ||
                                   tid == zep_id)
                                 return true;
                             }
                             return false;
                           }),
            p.constraints.end());

        // Erase dimensions on this line.
        p.dimensions.erase(
            std::remove_if(p.dimensions.begin(), p.dimensions.end(),
                           [&](const SketchDimension& d) {
                             return d.entity_id == zid ||
                                    d.secondary_entity_id == zid;
                           }),
            p.dimensions.end());

        // Erase relations involving this line.
        p.line_relations.erase(
            std::remove_if(p.line_relations.begin(), p.line_relations.end(),
                           [&](const SketchLineRelation& rel) {
                             return rel.first_line_id == zid ||
                                    rel.second_line_id == zid;
                           }),
            p.line_relations.end());

        // Erase fillets that involve this line.
        p.fillets.erase(
            std::remove_if(p.fillets.begin(), p.fillets.end(),
                           [&](const SketchFillet& f) {
                             return f.line_a_id == zid ||
                                    f.line_b_id == zid;
                           }),
            p.fillets.end());

        // Erase anchors hosted on this line.
        p.midpoint_anchors.erase(
            std::remove_if(p.midpoint_anchors.begin(),
                           p.midpoint_anchors.end(),
                           [&](const SketchMidpointAnchor& a) {
                             return a.line_id == zid;
                           }),
            p.midpoint_anchors.end());
        p.point_line_anchors.erase(
            std::remove_if(p.point_line_anchors.begin(),
                           p.point_line_anchors.end(),
                           [&](const SketchPointLineAnchor& a) {
                             return a.line_id == zid;
                           }),
            p.point_line_anchors.end());

        // Erase the line itself.
        p.lines.erase(
            std::remove_if(p.lines.begin(), p.lines.end(),
                           [&](const SketchLine& line) {
                             return line.id == zid;
                           }),
            p.lines.end());
      }
    }
  }

  // Re-anchor midpoint-bound points to their host line's current
  // midpoint before rebuilding the points list. The cascade may
  // shift other line endpoints which `rebuild_sketch_points` then
  // mirrors into the points vector.

  // --- planegcs constraint solver ---
  // Solve the geometric constraint system before running ad-hoc
  // enforce functions. If the solver succeeds, enforce functions
  // will see already-correct geometry and be no-ops. On failure,
  // the enforce functions provide a fallback.
  if (!feature.sketch_parameters->constraints.empty() ||
      !feature.sketch_parameters->line_relations.empty()) {
    ConstraintSolver solver;
    solver.build(*feature.sketch_parameters);
    auto result = solver.solve();
    if (result.ok()) {
      solver.apply(*feature.sketch_parameters);
    }
    // Log solver diagnostics for now; later we'll pipe to UI.
    if (!result.ok()) {
      fprintf(stderr, "constraint solver: status=%d dofs=%d conflicting=%zu redundant=%zu\n",
              static_cast<int>(result.status), result.dofs,
              result.conflicting.size(), result.redundant.size());
    }
  }

  enforce_midpoint_anchors(*feature.sketch_parameters);
  enforce_point_line_anchors(*feature.sketch_parameters);
  enforce_tangent_line_circle_relations(*feature.sketch_parameters);

  // Enforce stored constraints so badges always match geometry.
  // H/V runs first (axis alignment), then relations (inter-line).
  // Any constraint that would destroy geometry is cleared instead.
  enforce_line_HV_constraints(*feature.sketch_parameters);
  enforce_all_equal_length_relations(*feature.sketch_parameters);
  enforce_all_perpendicular_relations(*feature.sketch_parameters);
  enforce_all_parallel_relations(*feature.sketch_parameters);

  // Fillets must run *after* the anchor / tangent passes (so they
  // see the latest line endpoints) and *before* `rebuild_sketch_points`
  // (which pulls cached coords off lines and arcs into the points
  // vector — we want it to see the fillet-corrected values).
  enforce_sketch_fillets(*feature.sketch_parameters);

  // Driven (reference-only) dimensions: re-measure from current geometry
  // so their displayed values stay correct without driving anything.
  sync_driven_dimensions(*feature.sketch_parameters);

  // Force all lines sharing a point ID to agree on its position before
  // we rebuild the deduplicated points list.  Without this pass,
  // constraint enforcement (H/V, relations) can leave two lines with
  // different coords for the same shared endpoint, causing orphaned
  // spheres and visual divergence.
  reconcile_shared_point_positions(*feature.sketch_parameters);

  const std::vector<SketchPoint> previous_points = feature.sketch_parameters->points;
  rebuild_sketch_points(*feature.sketch_parameters);
  sync_fixed_point_flags(*feature.sketch_parameters, previous_points);
  // Derived points (projected, quadrant) are derived geometry;
  // force them locked unconditionally even if `sync_fixed_point_flags`
  // happened to copy a transient false from the previous frame.
  enforce_derived_points_fixed(*feature.sketch_parameters);
  rebuild_sketch_profiles(*feature.sketch_parameters);
  feature.parameters_summary = make_parameters_summary(feature.sketch_parameters.value());
}

FeatureEntry create_sketch_feature(
    int feature_index,
    const std::string& plane_id,
    std::optional<SketchFeatureParameters::SketchPlaneFrame> plane_frame) {
  SketchFeatureParameters parameters{
      .plane_id = plane_id,
      .plane_frame = plane_frame,
      .active_tool = "select",
      .lines = {},
      .circles = {},
      .points = {},
      .dimensions = {},
      .line_relations = {},
      .profiles = {},
  };

  FeatureEntry feature{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "sketch",
      .name = "Sketch",
      .status = "editing",
      .parameters_summary = make_parameters_summary(parameters),
      .box_parameters = std::nullopt,
      .cylinder_parameters = std::nullopt,
      .extrude_parameters = std::nullopt,
      .sketch_parameters = parameters,
  };

  refresh_sketch_derived_state(feature);
  return feature;
}

void set_sketch_tool(FeatureEntry& feature, const std::string& tool) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can change sketch tools");
  }

  validate_tool(tool);
  feature.sketch_parameters->active_tool = tool;
}

void update_sketch_line(FeatureEntry& feature,
                        const std::string& line_id,
                        double start_x,
                        double start_y,
                        double end_x,
                        double end_y) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch lines");
  }

  const auto line_it = std::find_if(
      feature.sketch_parameters->lines.begin(),
      feature.sketch_parameters->lines.end(),
      [&](const SketchLine& line) { return line.id == line_id; });

  if (line_it == feature.sketch_parameters->lines.end()) {
    throw std::runtime_error("Sketch line not found: " + line_id);
  }

  const double previous_start_x = line_it->start_x;
  const double previous_start_y = line_it->start_y;
  const double previous_end_x = line_it->end_x;
  const double previous_end_y = line_it->end_y;

  line_it->start_x = start_x;
  line_it->start_y = start_y;
  line_it->end_x = end_x;
  line_it->end_y = end_y;
  restore_fixed_line_endpoints(*feature.sketch_parameters,
                               *line_it,
                               previous_start_x,
                               previous_start_y,
                               previous_end_x,
                               previous_end_y);
  apply_line_constraint_respecting_fixed_points(
      *feature.sketch_parameters, *line_it);
  restore_fixed_line_endpoints(*feature.sketch_parameters,
                               *line_it,
                               previous_start_x,
                               previous_start_y,
                               previous_end_x,
                               previous_end_y);
  snap_line_endpoints_to_coincident_geometry(*feature.sketch_parameters, *line_it);
  apply_line_constraint_respecting_fixed_points(
      *feature.sketch_parameters, *line_it);
  validate_line(line_it->start_x, line_it->start_y, line_it->end_x, line_it->end_y);

  if (!points_match(previous_start_x,
                    previous_start_y,
                    line_it->start_x,
                    line_it->start_y)) {
    propagate_connected_point_move(*feature.sketch_parameters,
                                   line_it->start_point_id,
                                   line_it->start_x,
                                   line_it->start_y);
  }
  if (!points_match(previous_end_x,
                    previous_end_y,
                    line_it->end_x,
                    line_it->end_y)) {
    propagate_connected_point_move(*feature.sketch_parameters,
                                   line_it->end_point_id,
                                   line_it->end_x,
                                   line_it->end_y);
  }
  sync_all_line_dimensions(*feature.sketch_parameters);
  enforce_equal_length_relations(*feature.sketch_parameters, line_it->id);
  enforce_perpendicular_relations(*feature.sketch_parameters, line_it->id);
  enforce_parallel_relations(*feature.sketch_parameters, line_it->id);
  sync_all_line_dimensions(*feature.sketch_parameters);
  refresh_sketch_derived_state(feature);
}

void update_sketch_point(FeatureEntry& feature,
                         const std::string& point_id,
                         double x,
                         double y) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch points");
  }

  auto& parameters = feature.sketch_parameters.value();
  const SketchPoint* point = find_sketch_point(parameters, point_id);
  if (point == nullptr) {
    throw std::runtime_error("Sketch point not found: " + point_id);
  }

  if (point->is_fixed && !points_match(point->x, point->y, x, y)) {
    throw std::runtime_error("Cannot move a fixed sketch point");
  }

  const std::vector<std::string> affected_line_ids =
      collect_line_ids_for_point(parameters, point_id);

  if (point->kind == "center") {
    const std::string prefix = "point-circle-";
    const std::string suffix = "-center";
    if (point_id.rfind(prefix, 0) != 0 ||
        point_id.size() <= prefix.size() + suffix.size() ||
        point_id.substr(point_id.size() - suffix.size()) != suffix) {
      throw std::runtime_error("Unsupported sketch center point: " + point_id);
    }

    const std::string circle_id =
        point_id.substr(prefix.size(),
                        point_id.size() - prefix.size() - suffix.size());
    const auto circle_it = std::find_if(
        parameters.circles.begin(),
        parameters.circles.end(),
        [&](const SketchCircle& circle) { return circle.id == circle_id; });
    if (circle_it == parameters.circles.end()) {
      throw std::runtime_error("Sketch circle not found for point: " + point_id);
    }

    circle_it->center_x = x;
    circle_it->center_y = y;
    refresh_sketch_derived_state(feature);
    return;
  }

  propagate_connected_point_move(parameters, point_id, x, y);
  sync_all_line_dimensions(parameters);

  for (const auto& line_id : affected_line_ids) {
    enforce_equal_length_relations(parameters, line_id);
    enforce_perpendicular_relations(parameters, line_id);
    enforce_parallel_relations(parameters, line_id);
  }

  sync_all_line_dimensions(parameters);
  refresh_sketch_derived_state(feature);
}

void set_sketch_line_constraint(FeatureEntry& feature,
                                const std::string& line_id,
                                const std::optional<std::string>& constraint) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set line constraints");
  }

  validate_constraint(constraint);

  const auto line_it = std::find_if(
      feature.sketch_parameters->lines.begin(),
      feature.sketch_parameters->lines.end(),
      [&](const SketchLine& line) { return line.id == line_id; });

  if (line_it == feature.sketch_parameters->lines.end()) {
    throw std::runtime_error("Sketch line not found: " + line_id);
  }

  const double previous_start_x = line_it->start_x;
  const double previous_start_y = line_it->start_y;
  const double previous_end_x = line_it->end_x;
  const double previous_end_y = line_it->end_y;
  line_it->constraint = constraint;
  apply_line_constraint_respecting_fixed_points(
      *feature.sketch_parameters, *line_it);
  snap_line_endpoints_to_coincident_geometry(*feature.sketch_parameters, *line_it);
  apply_line_constraint_respecting_fixed_points(
      *feature.sketch_parameters, *line_it);
  validate_line(line_it->start_x, line_it->start_y, line_it->end_x, line_it->end_y);

  if (!points_match(previous_start_x,
                    previous_start_y,
                    line_it->start_x,
                    line_it->start_y)) {
    propagate_connected_point_move(*feature.sketch_parameters,
                                   line_it->start_point_id,
                                   line_it->start_x,
                                   line_it->start_y);
  }
  if (!points_match(previous_end_x,
                    previous_end_y,
                    line_it->end_x,
                    line_it->end_y)) {
    propagate_connected_point_move(*feature.sketch_parameters,
                                   line_it->end_point_id,
                                   line_it->end_x,
                                   line_it->end_y);
  }
  sync_all_line_dimensions(*feature.sketch_parameters);
  enforce_equal_length_relations(*feature.sketch_parameters, line_it->id);
  enforce_perpendicular_relations(*feature.sketch_parameters, line_it->id);
  enforce_parallel_relations(*feature.sketch_parameters, line_it->id);
  sync_all_line_dimensions(*feature.sketch_parameters);
  refresh_sketch_derived_state(feature);
}

void set_sketch_equal_length_constraint(
    FeatureEntry& feature,
    const std::string& line_id,
    const std::optional<std::string>& other_line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set equal-length constraints");
  }

  auto& parameters = feature.sketch_parameters.value();
  auto& line = require_line(parameters, line_id);

  remove_line_relations_for_line(parameters, "equal_length", line_id);

  if (!other_line_id.has_value()) {
    refresh_sketch_derived_state(feature);
    return;
  }

  if (other_line_id.value() == line_id) {
    throw std::runtime_error("A sketch line cannot be equal-length to itself");
  }

  auto& other_line = require_line(parameters, other_line_id.value());

  // Canonical ID: sorted pair so the same two lines always get the
  // same relation id regardless of which is passed as `line_id`.
  const std::string el_a = std::min(line_id, other_line_id.value());
  const std::string el_b = std::max(line_id, other_line_id.value());

  // Remove any pre-existing relation between this pair (regardless of
  // order or id) before inserting — guards against file-load duplicates.
  remove_line_relations_for_line(parameters, "equal_length", other_line_id.value());

  parameters.line_relations.push_back(SketchLineRelation{
      .id = "rel-equal-length-" + el_a + "-" + el_b,
      .kind = "equal_length",
      .first_line_id = line_id,
      .second_line_id = other_line_id.value(),
  });

  const double target_length = measure_line_length(other_line);
  const double previous_end_x = line.end_x;
  const double previous_end_y = line.end_y;
  drive_line_length_respecting_fixed_points(parameters, line, target_length);
  snap_line_endpoints_to_coincident_geometry(parameters, line);
  validate_line(line.start_x, line.start_y, line.end_x, line.end_y);

  if (!points_match(previous_end_x, previous_end_y, line.end_x, line.end_y)) {
    propagate_connected_point_move(
        parameters, line.end_point_id, line.end_x, line.end_y);
  }

  sync_all_line_dimensions(parameters);
  enforce_equal_length_relations(parameters, line_id);
  enforce_perpendicular_relations(parameters, line_id);
  enforce_parallel_relations(parameters, line_id);
  sync_all_line_dimensions(parameters);
  refresh_sketch_derived_state(feature);
}

void set_sketch_perpendicular_constraint(
    FeatureEntry& feature,
    const std::string& line_id,
    const std::optional<std::string>& other_line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set perpendicular constraints");
  }

  auto& parameters = feature.sketch_parameters.value();
  auto& line = require_line(parameters, line_id);

  remove_line_relations_for_line(parameters, "perpendicular", line_id);

  if (!other_line_id.has_value()) {
    refresh_sketch_derived_state(feature);
    return;
  }

  if (other_line_id.value() == line_id) {
    throw std::runtime_error("A sketch line cannot be perpendicular to itself");
  }

  auto& other_line = require_line(parameters, other_line_id.value());

  // Save the driven line's full state so we can roll back completely
  // if enforcement fails.
  const auto saved_line_constraint = line.constraint;
  const double saved_line_start_x = line.start_x;
  const double saved_line_start_y = line.start_y;
  const double saved_line_end_x = line.end_x;
  const double saved_line_end_y = line.end_y;

  // Clear the driven line's H/V constraint — it's incompatible with
  // perpendicular. The reference line keeps its own constraints;
  // enforcement will skip it silently (see drive_line_perpendicular_to_reference).
  line.constraint = std::nullopt;

  {
    const std::string pa = std::min(line_id, other_line_id.value());
    const std::string pb = std::max(line_id, other_line_id.value());
    remove_line_relations_for_line(parameters, "perpendicular", other_line_id.value());
    parameters.line_relations.push_back(SketchLineRelation{
        .id = "rel-perpendicular-" + pa + "-" + pb,
        .kind = "perpendicular",
        .first_line_id = line_id,
        .second_line_id = other_line_id.value(),
    });
  }

  try {
    drive_line_perpendicular_to_reference(line, other_line, parameters);
    snap_line_endpoints_to_coincident_geometry(parameters, line);
    validate_line(line.start_x, line.start_y, line.end_x, line.end_y);

    if (!points_match(saved_line_start_x, saved_line_start_y,
                      line.start_x, line.start_y)) {
      propagate_connected_point_move(
          parameters, line.start_point_id, line.start_x, line.start_y);
    }

    if (!points_match(saved_line_end_x, saved_line_end_y,
                      line.end_x, line.end_y)) {
      propagate_connected_point_move(
          parameters, line.end_point_id, line.end_x, line.end_y);
    }

    sync_all_line_dimensions(parameters);
    enforce_equal_length_relations(parameters, line_id);
    enforce_perpendicular_relations(parameters, line_id);
    enforce_parallel_relations(parameters, line_id);
    sync_all_line_dimensions(parameters);
    refresh_sketch_derived_state(feature);
  } catch (...) {
    // Roll back completely: remove the relation, restore the driven
    // line's endpoints, and restore its original constraint.
    remove_line_relations_for_line(parameters, "perpendicular", line_id);
    line.start_x = saved_line_start_x;
    line.start_y = saved_line_start_y;
    line.end_x = saved_line_end_x;
    line.end_y = saved_line_end_y;
    line.constraint = saved_line_constraint;
    throw;
  }
}

void set_sketch_tangent_constraint(FeatureEntry& feature,
                                   const std::string& line_id,
                                   const std::string& circle_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error(
        "Only sketch features can set tangent constraints");
  }

  auto& parameters = feature.sketch_parameters.value();
  auto& line = require_line(parameters, line_id);

  // Tangent is one-per-line — there's no useful semantics for a
  // single line being tangent to multiple circles simultaneously
  // (it would over-constrain the end point). Drop any prior tangent
  // before reseating.
  remove_line_relations_for_line(parameters, "tangent_line_circle", line_id);

  if (circle_id.empty()) {
    refresh_sketch_derived_state(feature);
    return;
  }

  const auto circle_it = std::find_if(
      parameters.circles.begin(),
      parameters.circles.end(),
      [&](const SketchCircle& circle) { return circle.id == circle_id; });
  if (circle_it == parameters.circles.end()) {
    throw std::runtime_error("Sketch circle not found: " + circle_id);
  }

  // Pre-validate the geometric prerequisite. If the start lies
  // inside or on the circle the enforcer would silently no-op every
  // refresh, leaving the relation but never satisfying it. Better
  // to refuse the op so the user knows immediately.
  const double dx = circle_it->center_x - line.start_x;
  const double dy = circle_it->center_y - line.start_y;
  const double d_squared = dx * dx + dy * dy;
  if (d_squared <= circle_it->radius * circle_it->radius + 1e-9) {
    throw std::runtime_error(
        "Tangent constraint requires the line's start to lie outside the "
        "circle");
  }

  parameters.line_relations.push_back(SketchLineRelation{
      .id = "rel-tangent-" + line_id + "-" + circle_id,
      .kind = "tangent_line_circle",
      .first_line_id = line_id,
      .second_line_id = circle_id,
  });

  refresh_sketch_derived_state(feature);
}

void set_sketch_parallel_constraint(
    FeatureEntry& feature,
    const std::string& line_id,
    const std::optional<std::string>& other_line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set parallel constraints");
  }

  auto& parameters = feature.sketch_parameters.value();
  auto& line = require_line(parameters, line_id);

  remove_line_relations_for_line(parameters, "parallel", line_id);

  if (!other_line_id.has_value()) {
    refresh_sketch_derived_state(feature);
    return;
  }

  if (other_line_id.value() == line_id) {
    throw std::runtime_error("A sketch line cannot be parallel to itself");
  }

  auto& other_line = require_line(parameters, other_line_id.value());

  // Save the driven line's full state so we can roll back completely
  // if enforcement fails.
  const auto saved_line_constraint = line.constraint;
  const double saved_line_start_x = line.start_x;
  const double saved_line_start_y = line.start_y;
  const double saved_line_end_x = line.end_x;
  const double saved_line_end_y = line.end_y;

  // Clear the driven line's H/V constraint — it's incompatible with
  // parallel. The reference line keeps its own constraints;
  // enforcement will skip it silently.
  line.constraint = std::nullopt;

  {
    const std::string pa = std::min(line_id, other_line_id.value());
    const std::string pb = std::max(line_id, other_line_id.value());
    remove_line_relations_for_line(parameters, "parallel", other_line_id.value());
    parameters.line_relations.push_back(SketchLineRelation{
        .id = "rel-parallel-" + pa + "-" + pb,
        .kind = "parallel",
        .first_line_id = line_id,
        .second_line_id = other_line_id.value(),
    });
  }

  try {
    drive_line_parallel_to_reference(line, other_line, parameters);
    snap_line_endpoints_to_coincident_geometry(parameters, line);
    validate_line(line.start_x, line.start_y, line.end_x, line.end_y);

    if (!points_match(saved_line_start_x, saved_line_start_y,
                      line.start_x, line.start_y)) {
      propagate_connected_point_move(
          parameters, line.start_point_id, line.start_x, line.start_y);
    }

    if (!points_match(saved_line_end_x, saved_line_end_y,
                      line.end_x, line.end_y)) {
      propagate_connected_point_move(
          parameters, line.end_point_id, line.end_x, line.end_y);
    }

    sync_all_line_dimensions(parameters);
    enforce_equal_length_relations(parameters, line_id);
    enforce_perpendicular_relations(parameters, line_id);
    enforce_parallel_relations(parameters, line_id);
    sync_all_line_dimensions(parameters);
    refresh_sketch_derived_state(feature);
  } catch (...) {
    // Roll back completely: remove the relation, restore the driven
    // line's endpoints, and restore its original constraint.
    remove_line_relations_for_line(parameters, "parallel", line_id);
    line.start_x = saved_line_start_x;
    line.start_y = saved_line_start_y;
    line.end_x = saved_line_end_x;
    line.end_y = saved_line_end_y;
    line.constraint = saved_line_constraint;
    throw;
  }
}

void clear_sketch_line_constraints(FeatureEntry& feature,
                                   const std::string& line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can clear line constraints");
  }

  auto& parameters = feature.sketch_parameters.value();
  auto& line = require_line(parameters, line_id);

  // Clear the inline H/V constraint.
  line.constraint = std::nullopt;

  // Clear every line‑line relation that involves this line.
  remove_line_relations_for_line(parameters, "equal_length", line_id);
  remove_line_relations_for_line(parameters, "perpendicular", line_id);
  remove_line_relations_for_line(parameters, "parallel", line_id);
  remove_line_relations_for_line(parameters, "tangent_line_circle", line_id);

  // Clear midpoint and point‑line anchors that reference this line.
  parameters.midpoint_anchors.erase(
      std::remove_if(parameters.midpoint_anchors.begin(),
                     parameters.midpoint_anchors.end(),
                     [&](const SketchMidpointAnchor& a) {
                       return a.line_id == line_id;
                     }),
      parameters.midpoint_anchors.end());
  parameters.point_line_anchors.erase(
      std::remove_if(parameters.point_line_anchors.begin(),
                     parameters.point_line_anchors.end(),
                     [&](const SketchPointLineAnchor& a) {
                       return a.line_id == line_id;
                     }),
      parameters.point_line_anchors.end());

  sync_all_line_dimensions(parameters);
  refresh_sketch_derived_state(feature);
}

void set_sketch_coincident_constraint(FeatureEntry& feature,
                                      const std::string& point_id,
                                      const std::string& other_point_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set coincident constraints");
  }

  auto& parameters = feature.sketch_parameters.value();

  if (point_id == other_point_id) {
    return;
  }

  const auto point_position = find_point_position(parameters, point_id);
  if (!point_position.has_value()) {
    throw std::runtime_error("Sketch point not found: " + point_id);
  }

  const auto other_point_position = find_point_position(parameters, other_point_id);
  if (!other_point_position.has_value()) {
    throw std::runtime_error("Sketch point not found: " + other_point_id);
  }

  for (const auto& line : parameters.lines) {
    const bool spans_points =
        (line.start_point_id == point_id && line.end_point_id == other_point_id) ||
        (line.start_point_id == other_point_id && line.end_point_id == point_id);
    if (spans_points) {
      throw std::runtime_error(
          "Cannot make both endpoints of a sketch line coincident");
    }
  }

  auto affected_line_ids = collect_line_ids_for_point(parameters, point_id);
  const auto other_line_ids =
      collect_line_ids_for_point(parameters, other_point_id);
  for (const auto& line_id : other_line_ids) {
    if (std::find(affected_line_ids.begin(), affected_line_ids.end(), line_id) ==
        affected_line_ids.end()) {
      affected_line_ids.push_back(line_id);
    }
  }

  // Only move the point if the two points are not already coincident.
  // When the user bonds two points that already sit at the same position
  // (e.g. two lines drawn end-to-end without a constraint), we skip the
  // move and just merge the point IDs.
  if (!points_match(std::get<0>(point_position.value()),
                    std::get<1>(point_position.value()),
                    std::get<0>(other_point_position.value()),
                    std::get<1>(other_point_position.value()))) {
    propagate_connected_point_move(parameters,
                                   point_id,
                                   std::get<0>(other_point_position.value()),
                                   std::get<1>(other_point_position.value()));
  }
  replace_point_id_references(parameters, point_id, other_point_id);
  sync_all_line_dimensions(parameters);

  for (const auto& line_id : affected_line_ids) {
    enforce_equal_length_relations(parameters, line_id);
    enforce_perpendicular_relations(parameters, line_id);
    enforce_parallel_relations(parameters, line_id);
  }

  sync_all_line_dimensions(parameters);
  refresh_sketch_derived_state(feature);

  // Store an explicit constraint entry so the viewport can render a
  // badge and the DOF counter accounts for the user-applied constraint.
  // target_ids lists the lines that share this coincident point.
  {
    bool already_exists = false;
    for (const auto& c : parameters.constraints) {
      if (c.kind == "coincident" && c.target_ids.size() == affected_line_ids.size()) {
        bool match = true;
        for (size_t i = 0; i < affected_line_ids.size(); ++i) {
          if (c.target_ids[i] != affected_line_ids[i]) { match = false; break; }
        }
        if (match) { already_exists = true; break; }
      }
    }
    if (!already_exists && !affected_line_ids.empty()) {
      parameters.constraints.push_back(SketchConstraint{
          .constraint_id = "constraint-coincident-" + other_point_id,
          .kind = "coincident",
          .target_ids = affected_line_ids,
      });
    }
  }
}

void delete_sketch_coincident_constraint(FeatureEntry& feature,
                                         const std::string& constraint_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can delete coincident constraints");
  }

  auto& parameters = feature.sketch_parameters.value();

  // Extract the merged point ID from the constraint_id pattern:
  // "constraint-coincident-<point_id>"
  const std::string prefix = "constraint-coincident-";
  if (constraint_id.size() <= prefix.size() ||
      constraint_id.substr(0, prefix.size()) != prefix) {
    throw std::runtime_error("Not a coincident constraint: " + constraint_id);
  }
  const std::string merged_point_id = constraint_id.substr(prefix.size());

  // Remove the constraint entry.
  const auto constraint_it = std::find_if(
      parameters.constraints.begin(), parameters.constraints.end(),
      [&](const SketchConstraint& c) { return c.constraint_id == constraint_id; });
  if (constraint_it == parameters.constraints.end()) {
    throw std::runtime_error("Coincident constraint not found: " + constraint_id);
  }
  parameters.constraints.erase(constraint_it);

  // Find all lines sharing the merged point.
  std::vector<SketchLine*> affected_lines;
  for (auto& line : parameters.lines) {
    if (line.start_point_id == merged_point_id ||
        line.end_point_id == merged_point_id) {
      affected_lines.push_back(&line);
    }
  }

  // Assign fresh unique point IDs to all but the first line so each
  // can move independently. The first line keeps the original point ID.
  if (affected_lines.size() > 1) {
    int fresh_counter = static_cast<int>(parameters.lines.size()) * 100;
    for (size_t i = 1; i < affected_lines.size(); ++i) {
      const std::string new_id =
          "point-line-" + std::to_string(fresh_counter++) + "-start";
      auto* line = affected_lines[i];
      if (line->start_point_id == merged_point_id) {
        line->start_point_id = new_id;
      }
      if (line->end_point_id == merged_point_id) {
        line->end_point_id = new_id;
      }
    }
  }

  refresh_sketch_derived_state(feature);
}

void set_sketch_point_fixed(FeatureEntry& feature,
                            const std::string& point_id,
                            bool is_fixed) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set fixed point state");
  }

  auto& parameters = feature.sketch_parameters.value();
  SketchPoint* point = find_sketch_point(parameters, point_id);
  if (point == nullptr) {
    throw std::runtime_error("Sketch point not found: " + point_id);
  }

  point->is_fixed = is_fixed;
  refresh_sketch_derived_state(feature);
}

void update_sketch_circle(FeatureEntry& feature,
                          const std::string& circle_id,
                          double center_x,
                          double center_y,
                          double radius) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch circles");
  }

  if (radius <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch circles must have non-zero radius");
  }

  const auto circle_it = std::find_if(
      feature.sketch_parameters->circles.begin(),
      feature.sketch_parameters->circles.end(),
      [&](const SketchCircle& circle) { return circle.id == circle_id; });

  if (circle_it == feature.sketch_parameters->circles.end()) {
    throw std::runtime_error("Sketch circle not found: " + circle_id);
  }

  const std::string center_point_id = "point-circle-" + circle_it->id + "-center";
  if (!point_is_fixed(*feature.sketch_parameters, center_point_id)) {
    circle_it->center_x = center_x;
    circle_it->center_y = center_y;
  }
  circle_it->radius = radius;
  sync_circle_dimension(*feature.sketch_parameters, *circle_it);
  refresh_sketch_derived_state(feature);
}

void update_sketch_dimension(FeatureEntry& feature,
                             const std::string& dimension_id,
                             double value,
                             std::optional<std::string> expression) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch dimensions");
  }

  if (value <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch dimensions must be greater than zero");
  }

  auto& parameters = feature.sketch_parameters.value();
  auto& dimension = require_dimension(parameters, dimension_id);

  // Driven (reference-only) dimensions show the measured value but do
  // not drive geometry. Silently ignore edit attempts.
  if (dimension.driven) {
    return;
  }

  // Store expression if provided — cleared when expression is nullopt
  // (plain numeric update).
  dimension.expression = expression.value_or("");

  if (dimension.kind == "line_length") {
    const auto line_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == dimension.entity_id; });

    if (line_it == parameters.lines.end()) {
      throw std::runtime_error("Sketch line not found for dimension: " + dimension_id);
    }

    const double previous_start_x = line_it->start_x;
    const double previous_start_y = line_it->start_y;
    const double previous_end_x = line_it->end_x;
    const double previous_end_y = line_it->end_y;

    drive_line_length_respecting_fixed_points(parameters, *line_it, value);
    snap_line_endpoints_to_coincident_geometry(parameters, *line_it);
    if (!points_match(previous_start_x,
                      previous_start_y,
                      line_it->start_x,
                      line_it->start_y)) {
      propagate_connected_point_move(parameters,
                                     line_it->start_point_id,
                                     line_it->start_x,
                                     line_it->start_y);
    }
    if (!points_match(previous_end_x,
                      previous_end_y,
                      line_it->end_x,
                      line_it->end_y)) {
      propagate_connected_point_move(parameters,
                                     line_it->end_point_id,
                                     line_it->end_x,
                                     line_it->end_y);
    }
    sync_all_line_dimensions(parameters);
    enforce_equal_length_relations(parameters, line_it->id);
    enforce_perpendicular_relations(parameters, line_it->id);
    enforce_parallel_relations(parameters, line_it->id);
    sync_all_line_dimensions(parameters);
    refresh_sketch_derived_state(feature);
    return;
  }

  if (dimension.kind == "circle_radius") {
    const auto circle_it = std::find_if(
        parameters.circles.begin(),
        parameters.circles.end(),
        [&](const SketchCircle& circle) {
          return circle.id == dimension.entity_id;
        });

    if (circle_it == parameters.circles.end()) {
      throw std::runtime_error("Sketch circle not found for dimension: " + dimension_id);
    }

    circle_it->radius = value;
    sync_circle_dimension(parameters, *circle_it);
    refresh_sketch_derived_state(feature);
    return;
  }

  if (dimension.kind == "angle") {
    // Angle dimensions take a target angle in radians. We rotate the
    // SECOND line (`secondary_entity_id`) about the endpoint shared
    // with the FIRST line (`entity_id`) until the angle between
    // their outgoing direction vectors matches the target. Requires
    // the two lines to share an endpoint within
    // `kCoincidentTolerance`.
    auto& line_a = require_line(parameters, dimension.entity_id);
    auto& line_b = require_line(parameters, dimension.secondary_entity_id);

    // Locate the shared endpoint or intersection point.  First try
    // endpoint–endpoint, then endpoint-on-segment.
    const std::array<std::pair<double, double>, 2> a_ends = {{
        {line_a.start_x, line_a.start_y},
        {line_a.end_x, line_a.end_y},
    }};
    const std::array<std::pair<double, double>, 2> b_ends = {{
        {line_b.start_x, line_b.start_y},
        {line_b.end_x, line_b.end_y},
    }};

    auto pt_on_seg = [](double px, double py, double ax, double ay,
                        double bx, double by, double tol) -> bool {
      double dx = bx - ax, dy = by - ay;
      double len2 = dx * dx + dy * dy;
      if (len2 < 1e-12)
        return std::sqrt((px-ax)*(px-ax) + (py-ay)*(py-ay)) <= tol;
      double t = ((px-ax)*dx + (py-ay)*dy) / len2;
      if (t < 0.0) t = 0.0; if (t > 1.0) t = 1.0;
      double proj_x = ax + t*dx, proj_y = ay + t*dy;
      return std::sqrt((px-proj_x)*(px-proj_x) + (py-proj_y)*(py-proj_y)) <= tol;
    };

    int a_pivot_index = -1;
    int b_pivot_index = -1;
    for (int i = 0; i < 2 && a_pivot_index < 0; ++i) {
      for (int j = 0; j < 2; ++j) {
        if (std::abs(a_ends[i].first - b_ends[j].first) <=
                kCoincidentTolerance &&
            std::abs(a_ends[i].second - b_ends[j].second) <=
                kCoincidentTolerance) {
          a_pivot_index = i; b_pivot_index = j; break;
        }
      }
    }
    if (a_pivot_index < 0) {
      for (int i = 0; i < 2 && a_pivot_index < 0; ++i) {
        if (pt_on_seg(a_ends[i].first, a_ends[i].second,
                      b_ends[0].first, b_ends[0].second,
                      b_ends[1].first, b_ends[1].second,
                      kCoincidentTolerance)) {
          a_pivot_index = i; b_pivot_index = -1; break;
        }
      }
    }
    if (a_pivot_index < 0) {
      for (int j = 0; j < 2 && a_pivot_index < 0; ++j) {
        if (pt_on_seg(b_ends[j].first, b_ends[j].second,
                      a_ends[0].first, a_ends[0].second,
                      a_ends[1].first, a_ends[1].second,
                      kCoincidentTolerance)) {
          a_pivot_index = -1; b_pivot_index = j; break;
        }
      }
    }
    if (a_pivot_index < 0 && b_pivot_index < 0) {
      throw std::runtime_error(
          "Angle dimension requires the two lines to share an endpoint "
          "or have one endpoint on the other line");
    }

    double pivot_x, pivot_y;
    double a_dx, a_dy, b_dx, b_dy;
    bool drive_a = false;  // true → rotate line A; false → rotate line B
    int driven_pivot_index = 0;

    if (b_pivot_index >= 0) {
      // Pivot is an endpoint of line B.  Line B rotates.
      pivot_x = b_ends[b_pivot_index].first;
      pivot_y = b_ends[b_pivot_index].second;
      a_dx = a_ends[0].first - pivot_x; a_dy = a_ends[0].second - pivot_y;
      double a_len0 = std::sqrt(a_dx*a_dx + a_dy*a_dy);
      double a_dx1 = a_ends[1].first - pivot_x, a_dy1 = a_ends[1].second - pivot_y;
      double a_len1 = std::sqrt(a_dx1*a_dx1 + a_dy1*a_dy1);
      if (a_len1 > a_len0) { a_dx = a_dx1; a_dy = a_dy1; }
      int b_other = 1 - b_pivot_index;
      b_dx = b_ends[b_other].first - pivot_x;
      b_dy = b_ends[b_other].second - pivot_y;
      driven_pivot_index = b_pivot_index;
    } else {
      // Pivot is an endpoint of line A.  Line A rotates.
      drive_a = true;
      pivot_x = a_ends[a_pivot_index].first;
      pivot_y = a_ends[a_pivot_index].second;
      int a_other = 1 - a_pivot_index;
      a_dx = a_ends[a_other].first - pivot_x;
      a_dy = a_ends[a_other].second - pivot_y;
      b_dx = b_ends[0].first - pivot_x; b_dy = b_ends[0].second - pivot_y;
      double b_len0 = std::sqrt(b_dx*b_dx + b_dy*b_dy);
      double b_dx1 = b_ends[1].first - pivot_x, b_dy1 = b_ends[1].second - pivot_y;
      double b_len1 = std::sqrt(b_dx1*b_dx1 + b_dy1*b_dy1);
      if (b_len1 > b_len0) { b_dx = b_dx1; b_dy = b_dy1; }
      driven_pivot_index = a_pivot_index;
    }

    const double a_length = std::sqrt(a_dx * a_dx + a_dy * a_dy);
    const double b_length = std::sqrt(b_dx * b_dx + b_dy * b_dy);
    if (a_length <= kMinimumSketchDimensionValue ||
        b_length <= kMinimumSketchDimensionValue) {
      throw std::runtime_error(
          "Angle dimension requires both lines to have non-zero length");
    }

    // Current signed angle from A's direction to B's direction.
    const double current_signed = std::atan2(
        a_dx * b_dy - a_dy * b_dx, a_dx * b_dx + a_dy * b_dy);
    const double target_signed = current_signed >= 0.0 ? value : -value;
    const double delta = target_signed - current_signed;
    const double cos_delta = std::cos(delta);
    const double sin_delta = std::sin(delta);

    double new_other_x, new_other_y;
    if (drive_a) {
      // Rotate line A about the pivot.  Rotating A by +delta changes
      // the relative angle (B − A) by −delta, so we negate.
      const double rot_cos = std::cos(-delta);
      const double rot_sin = std::sin(-delta);
      const double new_a_dx = a_dx * rot_cos - a_dy * rot_sin;
      const double new_a_dy = a_dx * rot_sin + a_dy * rot_cos;
      new_other_x = pivot_x + new_a_dx;
      new_other_y = pivot_y + new_a_dy;
    } else {
      const double new_b_dx = b_dx * cos_delta - b_dy * sin_delta;
      const double new_b_dy = b_dx * sin_delta + b_dy * cos_delta;
      new_other_x = pivot_x + new_b_dx;
      new_other_y = pivot_y + new_b_dy;
    }

    // Mutate the driven line in-place.
    auto& driven_line = drive_a ? line_a : line_b;
    if (driven_pivot_index == 0) {
      driven_line.end_x = new_other_x;
      driven_line.end_y = new_other_y;
    } else {
      driven_line.start_x = new_other_x;
      driven_line.start_y = new_other_y;
    }

    snap_line_endpoints_to_coincident_geometry(parameters, driven_line);
    validate_line(driven_line.start_x, driven_line.start_y,
                  driven_line.end_x, driven_line.end_y);

    const std::string moved_point_id =
        driven_pivot_index == 0 ? driven_line.end_point_id
                                : driven_line.start_point_id;
    propagate_connected_point_move(
        parameters, moved_point_id, new_other_x, new_other_y);

    dimension.value = value;
    refresh_sketch_derived_state(feature);
    return;
  }

  if (dimension.kind == "line_angle") {
    // Line-angle dimension: absolute angle from the positive X axis
    // (horizontal). Rotate the line about its START point, preserving
    // the current length.
    auto& line = require_line(parameters, dimension.entity_id);

    const double dx = line.end_x - line.start_x;
    const double dy = line.end_y - line.start_y;
    const double current_length = std::sqrt(dx * dx + dy * dy);
    if (current_length <= kMinimumSketchDimensionValue) {
      throw std::runtime_error(
          "Line-angle dimension requires a line with non-zero length");
    }
    // Preserve the sign quadrant: the user enters a magnitude but the
    // line keeps its current orientation.  Mirrors the angle-between-lines
    // variant just above.
    const double current_angle = std::atan2(dy, dx);
    const double target = (current_angle < 0.0) ? -value : value;

    const double new_end_x = line.start_x + std::cos(target) * current_length;
    const double new_end_y = line.start_y + std::sin(target) * current_length;

    const double previous_end_x = line.end_x;
    const double previous_end_y = line.end_y;

    line.end_x = new_end_x;
    line.end_y = new_end_y;

    snap_line_endpoints_to_coincident_geometry(parameters, line);
    validate_line(line.start_x, line.start_y, line.end_x, line.end_y);

    // Propagate the moved endpoint through any coincident points.
    if (!points_match(previous_end_x, previous_end_y, line.end_x, line.end_y)) {
      propagate_connected_point_move(
          parameters, line.end_point_id, line.end_x, line.end_y);
    }

    dimension.value = value;
    sync_all_line_dimensions(parameters);
    refresh_sketch_derived_state(feature);
    return;
  }

  if (dimension.kind == "circle_center_distance") {
    auto& driven_circle = require_circle(parameters, dimension.entity_id);
    const auto& reference_circle =
        require_circle(parameters, dimension.secondary_entity_id);
    const double dx = driven_circle.center_x - reference_circle.center_x;
    const double dy = driven_circle.center_y - reference_circle.center_y;
    const double length = std::sqrt(dx * dx + dy * dy);
    if (length <= kMinimumSketchDimensionValue) {
      throw std::runtime_error(
          "Circle-center distance requires distinct circle centers");
    }
    driven_circle.center_x = reference_circle.center_x + (dx / length) * value;
    driven_circle.center_y = reference_circle.center_y + (dy / length) * value;
    dimension.value = value;
    refresh_sketch_derived_state(feature);
    return;
  }

  if (dimension.kind == "circle_line_distance") {
    auto& circle = require_circle(parameters, dimension.entity_id);
    const auto& line = require_line(parameters, dimension.secondary_entity_id);
    const double current_signed = signed_circle_line_distance(circle, line);
    const double direction = current_signed < 0.0 ? -1.0 : 1.0;
    const double dx = line.end_x - line.start_x;
    const double dy = line.end_y - line.start_y;
    const double length = std::sqrt(dx * dx + dy * dy);
    const double normal_x = -dy / length;
    const double normal_y = dx / length;
    const double delta = direction * value - current_signed;
    circle.center_x += normal_x * delta;
    circle.center_y += normal_y * delta;
    dimension.value = value;
    refresh_sketch_derived_state(feature);
    return;
  }

  if (dimension.kind == "line_line_distance") {
    auto& driven_line = require_line(parameters, dimension.entity_id);
    const auto& reference_line =
        require_line(parameters, dimension.secondary_entity_id);
    const double current_signed =
        signed_line_line_distance(driven_line, reference_line);
    const double direction = current_signed < 0.0 ? -1.0 : 1.0;
    const double dx = reference_line.end_x - reference_line.start_x;
    const double dy = reference_line.end_y - reference_line.start_y;
    const double length = std::sqrt(dx * dx + dy * dy);
    const double normal_x = -dy / length;
    const double normal_y = dx / length;
    const double delta = direction * value - current_signed;
    driven_line.start_x += normal_x * delta;
    driven_line.start_y += normal_y * delta;
    driven_line.end_x += normal_x * delta;
    driven_line.end_y += normal_y * delta;
    sync_all_line_dimensions(parameters);
    dimension.value = value;
    refresh_sketch_derived_state(feature);
    return;
  }

  if (dimension.kind == "polygon_radius") {
    const auto polygon_it = std::find_if(
        parameters.polygons.begin(),
        parameters.polygons.end(),
        [&](const SketchPolygon& polygon) {
          return polygon.id == dimension.entity_id;
        });

    if (polygon_it == parameters.polygons.end()) {
      throw std::runtime_error("Sketch polygon not found for dimension: " + dimension_id);
    }

    polygon_it->radius = value;
    dimension.value = value;
    refresh_sketch_derived_state(feature);
    return;
  }

  throw std::runtime_error("Unsupported sketch dimension kind: " + dimension.kind);
}

void reify_dimension_expressions(
    FeatureEntry& feature,
    const std::vector<struct ParameterEntry>& parameters) {
  if (!feature.sketch_parameters.has_value()) {
    return;
  }

  struct PendingDimensionUpdate {
    std::string id;
    std::string expression;
    double value;
  };
  std::vector<PendingDimensionUpdate> pending_updates;

  for (const auto& dim : feature.sketch_parameters->dimensions) {
    if (dim.expression.empty()) {
      continue;
    }

    const bool dim_is_angle =
        (dim.kind == "angle" || dim.kind == "line_angle");

    // Build a resolver against the current parameter table
    auto resolver = [&parameters,
                      dim_is_angle](const std::string& name) -> double {
      for (const auto& p : parameters) {
        if (p.name == name) {
          if (p.has_error) {
            throw std::runtime_error("Parameter '" + name +
                                     "' has an unresolved expression");
          }
          // Angle-type parameter referenced in a non-angle (length)
          // dimension — the numeric value would be misinterpreted.
          if (p.kind == "angle" && !dim_is_angle) {
            throw std::runtime_error("Angle parameter '" + name +
                "' cannot be used in a length dimension");
          }
          return p.resolved_value;
        }
      }
      throw std::runtime_error("Unknown parameter: '" + name + "'");
    };

    try {
      double resolved = evaluate_formula(dim.expression, resolver);
      if (resolved <= 0.0) {
        // Keep last good value for invalid dimension values
        continue;
      }
      // Angle dimensions store radians, but expressions are authored
      // in degrees (both literal numbers and angle-kind parameter
      // references). Convert here so the dimension's internal value
      // stays in radians while the expression stays human-readable.
      if (dim.kind == "angle" || dim.kind == "line_angle") {
        resolved = resolved * (M_PI / 180.0);
      } else if (dim.kind == "circle_radius") {
        resolved = resolved / 2.0;
      }
      // Preserve the orientation quadrant for line_angle dimensions so
      // a parameter re-eval doesn't flip the line's direction.
      if (dim.kind == "line_angle" && dim.value < 0.0) {
        resolved = -resolved;
      }
      pending_updates.push_back(PendingDimensionUpdate{
          .id = dim.id,
          .expression = dim.expression,
          .value = resolved,
      });
    } catch (const std::exception&) {
      // Silently keep last good value — expression resolution
      // failures shouldn't nuke the dimension's working value.
    }
  }

  for (const auto& update : pending_updates) {
    try {
      update_sketch_dimension(feature,
                              update.id,
                              update.value,
                              update.expression);
    } catch (const std::exception&) {
      // Keep the last valid geometry when a stored expression can no
      // longer be driven, e.g. because a referenced parameter was
      // deleted or the target entity was removed.
    }
  }
}

void add_sketch_angle_dimension(FeatureEntry& feature,
                                const std::string& first_line_id,
                                const std::string& second_line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error(
        "Only sketch features can hold angle dimensions");
  }
  if (first_line_id == second_line_id) {
    throw std::runtime_error(
        "Angle dimension requires two distinct lines");
  }
  auto& parameters = *feature.sketch_parameters;
  const auto& line_a = require_line(parameters, first_line_id);
  const auto& line_b = require_line(parameters, second_line_id);

  // Compute the current angle (unsigned) between outgoing directions
  // from the shared endpoint or intersection point.  First try a
  // direct endpoint match; if that fails, check whether one line's
  // endpoint lies on the other line's segment (the lines don't need
  // to share an endpoint — an endpoint-on-segment is enough).
  const std::array<std::pair<double, double>, 2> a_ends = {{
      {line_a.start_x, line_a.start_y},
      {line_a.end_x, line_a.end_y},
  }};
  const std::array<std::pair<double, double>, 2> b_ends = {{
      {line_b.start_x, line_b.start_y},
      {line_b.end_x, line_b.end_y},
  }};

  // Helper: closest point on segment AB to point P, and the distance.
  auto point_on_segment = [](double px, double py,
                             double ax, double ay,
                             double bx, double by,
                             double tolerance) -> bool {
    double dx = bx - ax;
    double dy = by - ay;
    double len2 = dx * dx + dy * dy;
    if (len2 < 1e-12) {
      return std::sqrt((px - ax) * (px - ax) + (py - ay) * (py - ay)) <=
             tolerance;
    }
    double t = ((px - ax) * dx + (py - ay) * dy) / len2;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    double proj_x = ax + t * dx;
    double proj_y = ay + t * dy;
    return std::sqrt((px - proj_x) * (px - proj_x) +
                     (py - proj_y) * (py - proj_y)) <= tolerance;
  };

  // Helper: direction away from pivot along line (to the farther endpoint).
  auto dir_away = [](double px, double py,
                     double e0x, double e0y,
                     double e1x, double e1y) -> std::pair<double, double> {
    double d0 = std::sqrt((e0x - px) * (e0x - px) + (e0y - py) * (e0y - py));
    double d1 = std::sqrt((e1x - px) * (e1x - px) + (e1y - py) * (e1y - py));
    double tx = d0 > d1 ? e0x : e1x;
    double ty = d0 > d1 ? e0y : e1y;
    double dx = tx - px;
    double dy = ty - py;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-12) return {1.0, 0.0};
    return {dx / len, dy / len};
  };

  int a_pivot_index = -1;
  int b_pivot_index = -1;
  // Pass 1: direct endpoint–endpoint match (existing behaviour).
  for (int i = 0; i < 2 && a_pivot_index < 0; ++i) {
    for (int j = 0; j < 2; ++j) {
      if (std::abs(a_ends[i].first - b_ends[j].first) <=
              kCoincidentTolerance &&
          std::abs(a_ends[i].second - b_ends[j].second) <=
              kCoincidentTolerance) {
        a_pivot_index = i;
        b_pivot_index = j;
        break;
      }
    }
  }
  // Pass 2: one line's endpoint lies on the other line's segment.
  if (a_pivot_index < 0) {
    for (int i = 0; i < 2 && a_pivot_index < 0; ++i) {
      if (point_on_segment(a_ends[i].first, a_ends[i].second,
                           b_ends[0].first, b_ends[0].second,
                           b_ends[1].first, b_ends[1].second,
                           kCoincidentTolerance)) {
        a_pivot_index = i;
        b_pivot_index = -1; // pivot is an A-endpoint, on B's segment
        break;
      }
    }
  }
  if (a_pivot_index < 0) {
    for (int j = 0; j < 2 && a_pivot_index < 0; ++j) {
      if (point_on_segment(b_ends[j].first, b_ends[j].second,
                           a_ends[0].first, a_ends[0].second,
                           a_ends[1].first, a_ends[1].second,
                           kCoincidentTolerance)) {
        a_pivot_index = -1; // pivot is a B-endpoint, on A's segment
        b_pivot_index = j;
        break;
      }
    }
  }
  if (a_pivot_index < 0 && b_pivot_index < 0) {
    throw std::runtime_error(
        "Angle dimension requires the two lines to share an endpoint "
        "or have one endpoint on the other line");
  }

  double pivot_x, pivot_y;
  double a_dx, a_dy, b_dx, b_dy;

  if (a_pivot_index >= 0 && b_pivot_index >= 0) {
    // Shared endpoint case (existing behaviour).
    pivot_x = a_ends[a_pivot_index].first;
    pivot_y = a_ends[a_pivot_index].second;
    a_dx = a_ends[1 - a_pivot_index].first - pivot_x;
    a_dy = a_ends[1 - a_pivot_index].second - pivot_y;
    b_dx = b_ends[1 - b_pivot_index].first - pivot_x;
    b_dy = b_ends[1 - b_pivot_index].second - pivot_y;
  } else if (a_pivot_index >= 0) {
    // A's endpoint lies on B's segment.
    pivot_x = a_ends[a_pivot_index].first;
    pivot_y = a_ends[a_pivot_index].second;
    auto a_dir = dir_away(pivot_x, pivot_y,
                          a_ends[1 - a_pivot_index].first,
                          a_ends[1 - a_pivot_index].second,
                          a_ends[a_pivot_index].first,
                          a_ends[a_pivot_index].second);
    a_dx = a_dir.first;
    a_dy = a_dir.second;
    auto b_dir = dir_away(pivot_x, pivot_y,
                          b_ends[0].first, b_ends[0].second,
                          b_ends[1].first, b_ends[1].second);
    b_dx = b_dir.first;
    b_dy = b_dir.second;
  } else {
    // B's endpoint lies on A's segment.
    pivot_x = b_ends[b_pivot_index].first;
    pivot_y = b_ends[b_pivot_index].second;
    auto a_dir = dir_away(pivot_x, pivot_y,
                          a_ends[0].first, a_ends[0].second,
                          a_ends[1].first, a_ends[1].second);
    a_dx = a_dir.first;
    a_dy = a_dir.second;
    auto b_dir = dir_away(pivot_x, pivot_y,
                          b_ends[1 - b_pivot_index].first,
                          b_ends[1 - b_pivot_index].second,
                          b_ends[b_pivot_index].first,
                          b_ends[b_pivot_index].second);
    b_dx = b_dir.first;
    b_dy = b_dir.second;
  }
  const double signed_angle = std::atan2(
      a_dx * b_dy - a_dy * b_dx, a_dx * b_dx + a_dy * b_dy);
  const double current_angle = std::abs(signed_angle);

  // Refuse to create a duplicate angle dimension on the same pair so
  // the dimension list doesn't accumulate stale entries when the
  // user clicks the same two lines repeatedly.
  const auto duplicate_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) {
        return dim.kind == "angle" &&
               ((dim.entity_id == first_line_id &&
                 dim.secondary_entity_id == second_line_id) ||
                (dim.entity_id == second_line_id &&
                 dim.secondary_entity_id == first_line_id));
      });
  if (duplicate_it != parameters.dimensions.end()) {
    return;
  }

  // Stable id derived from the line ids (sorted) so the same pair
  // round-trips through serialization without churning, and the
  // duplicate-detection above doesn't depend on insertion order.
  const std::string& first = first_line_id < second_line_id
                                 ? first_line_id
                                 : second_line_id;
  const std::string& second = first_line_id < second_line_id
                                  ? second_line_id
                                  : first_line_id;
  parameters.dimensions.push_back(SketchDimension{
      .id = "dim-angle-" + first + "-" + second,
      .kind = "angle",
      .entity_id = first_line_id,
      .secondary_entity_id = second_line_id,
      .value = current_angle,
  });
  refresh_sketch_derived_state(feature);
}

void add_sketch_distance_dimension(FeatureEntry& feature,
                                   const std::string& first_entity_id,
                                   const std::string& second_entity_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error(
        "Only sketch features can hold distance dimensions");
  }
  if (first_entity_id == second_entity_id) {
    throw std::runtime_error("Distance dimension requires two distinct entities");
  }

  auto& parameters = *feature.sketch_parameters;
  const bool first_is_circle = first_entity_id.rfind("circle-", 0) == 0;
  const bool second_is_circle = second_entity_id.rfind("circle-", 0) == 0;
  const bool first_is_line = first_entity_id.rfind("line-", 0) == 0;
  const bool second_is_line = second_entity_id.rfind("line-", 0) == 0;

  std::string kind;
  std::string driven_entity_id;
  std::string reference_entity_id;
  double value = 0.0;
  if (first_is_circle && second_is_circle) {
    const auto& first_circle = require_circle(parameters, first_entity_id);
    const auto& second_circle = require_circle(parameters, second_entity_id);
    kind = "circle_center_distance";
    driven_entity_id = second_entity_id;
    reference_entity_id = first_entity_id;
    value = distance_between_circles(first_circle, second_circle);
  } else if ((first_is_circle && second_is_line) ||
             (first_is_line && second_is_circle)) {
    const std::string circle_id = first_is_circle ? first_entity_id : second_entity_id;
    const std::string line_id = first_is_line ? first_entity_id : second_entity_id;
    const auto& circle = require_circle(parameters, circle_id);
    const auto& line = require_line(parameters, line_id);
    kind = "circle_line_distance";
    driven_entity_id = circle_id;
    reference_entity_id = line_id;
    value = std::abs(signed_circle_line_distance(circle, line));
  } else if (first_is_line && second_is_line) {
    const auto& first_line = require_line(parameters, first_entity_id);
    const auto& second_line = require_line(parameters, second_entity_id);
    kind = "line_line_distance";
    driven_entity_id = second_entity_id;
    reference_entity_id = first_entity_id;
    value = std::abs(signed_line_line_distance(second_line, first_line));
  } else {
    throw std::runtime_error(
        "Distance dimensions currently support line-line, circle-circle, or circle-line picks");
  }

  if (value <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Distance dimension must be greater than zero");
  }

  const auto duplicate_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) {
        return dim.kind == kind && dim.entity_id == driven_entity_id &&
               dim.secondary_entity_id == reference_entity_id;
      });
  if (duplicate_it != parameters.dimensions.end()) {
    return;
  }

  parameters.dimensions.push_back(SketchDimension{
      .id = "dim-" + kind + "-" + driven_entity_id + "-" + reference_entity_id,
      .kind = kind,
      .entity_id = driven_entity_id,
      .secondary_entity_id = reference_entity_id,
      .value = value,
  });
  refresh_sketch_derived_state(feature);
}

void add_sketch_line(FeatureEntry& feature,
                     int line_index,
                     double start_x,
                     double start_y,
                     double end_x,
                     double end_y,
                     bool is_construction,
                     bool snap_start) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch lines");
  }

  validate_line(start_x, start_y, end_x, end_y);

  const auto shared_start_point =
      find_coincident_endpoint(*feature.sketch_parameters, "", start_x, start_y);
  const auto shared_end_point =
      find_coincident_endpoint(*feature.sketch_parameters, "", end_x, end_y);

  feature.sketch_parameters->lines.push_back(SketchLine{
      .id = "line-" + std::to_string(line_index),
      .start_point_id = shared_start_point.has_value()
                            ? std::get<0>(shared_start_point.value())
                            : "point-line-" + std::to_string(line_index) + "-start",
      .end_point_id = shared_end_point.has_value()
                          ? std::get<0>(shared_end_point.value())
                          : "point-line-" + std::to_string(line_index) + "-end",
      .start_x = start_x,
      .start_y = start_y,
      .end_x = end_x,
      .end_y = end_y,
      .constraint = infer_constraint_hint(start_x, start_y, end_x, end_y),
      .is_construction = is_construction,
  });
  auto& line = feature.sketch_parameters->lines.back();
  apply_line_constraint(line);
  // The start point of user‑drawn lines must not be auto‑merged with
  // nearby geometry (see the phantom‑line bug in the impl log).
  // Rectangle / polygon / mirror lines still merge corners by default.
  snap_line_endpoints_to_coincident_geometry(*feature.sketch_parameters, line,
                                             snap_start);
  validate_line(line.start_x, line.start_y, line.end_x, line.end_y);
  // Construction lines are reference geometry; they don't get a
  // driving length dimension automatically. The user can still apply
  // one explicitly via the dimension tool if they want.
  if (!is_construction) {
    feature.sketch_parameters->dimensions.push_back(SketchDimension{
        .id = "dim-line-" + line.id,
        .kind = "line_length",
        .entity_id = line.id,
        .value = measure_line_length(line),
    });
    // Line-angle dimension: the absolute angle from the positive X axis
    // (horizontal). Stored in radians. Axis-constrained lines (rectangle
    // sides, Shift-locked lines) get a *driven* (reference-only) angle
    // dimension so the angle is still displayed but cannot be edited.
    const bool constrained_axis =
        line.constraint.has_value() &&
        (line.constraint.value() == "horizontal" ||
         line.constraint.value() == "vertical");
    feature.sketch_parameters->dimensions.push_back(SketchDimension{
        .id = "dim-line-angle-" + line.id,
        .kind = "line_angle",
        .entity_id = line.id,
        .value = std::atan2(line.end_y - line.start_y,
                            line.end_x - line.start_x),
        .driven = constrained_axis,
        .is_auto = true,
    });
  }
  if (!is_construction) {
    run_inference_on_new_line(*feature.sketch_parameters,
                              feature.sketch_parameters->lines.back());
  }
  refresh_sketch_derived_state(feature);
}

void set_sketch_line_construction(FeatureEntry& feature,
                                  const std::string& line_id,
                                  bool is_construction) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error(
        "Only sketch features can toggle construction lines");
  }

  auto& parameters = *feature.sketch_parameters;
  auto& line = require_line(parameters, line_id);
  if (line.is_construction == is_construction) {
    return;
  }
  line.is_construction = is_construction;

  // Keep the dimension list in sync with the new role: solid lines
  // get driving length + angle dimensions, construction lines lose
  // theirs since they're reference-only. Existing user-applied
  // dimensions on a line are preserved across role changes; we only
  // manage the automatic entries created at construction time.
  const std::string auto_dim_id = "dim-line-" + line.id;
  const std::string auto_angle_dim_id = "dim-line-angle-" + line.id;
  const auto auto_dim_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == auto_dim_id; });
  const auto auto_angle_dim_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) { return dim.id == auto_angle_dim_id; });
  if (is_construction) {
    if (auto_dim_it != parameters.dimensions.end()) {
      parameters.dimensions.erase(auto_dim_it);
    }
    if (auto_angle_dim_it != parameters.dimensions.end()) {
      parameters.dimensions.erase(auto_angle_dim_it);
    }
  } else {
    if (auto_dim_it == parameters.dimensions.end()) {
      parameters.dimensions.push_back(SketchDimension{
          .id = auto_dim_id,
          .kind = "line_length",
          .entity_id = line.id,
          .value = measure_line_length(line),
          .is_auto = true,
      });
    }
    if (auto_angle_dim_it == parameters.dimensions.end()) {
      const bool constrained_axis =
          line.constraint.has_value() &&
          (line.constraint.value() == "horizontal" ||
           line.constraint.value() == "vertical");
      parameters.dimensions.push_back(SketchDimension{
          .id = auto_angle_dim_id,
          .kind = "line_angle",
          .entity_id = line.id,
          .value = std::atan2(line.end_y - line.start_y,
                              line.end_x - line.start_x),
          .driven = constrained_axis,
          .is_auto = true,
      });
    }
  }

  refresh_sketch_derived_state(feature);
}

void set_sketch_midpoint_anchor(FeatureEntry& feature,
                                const std::string& point_id,
                                const std::string& host_line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error(
        "Only sketch features can hold midpoint anchors");
  }
  auto& parameters = *feature.sketch_parameters;

  // Drop any pre-existing anchor for the same point so the user can
  // re-target without leaving stale relations behind.
  parameters.midpoint_anchors.erase(
      std::remove_if(
          parameters.midpoint_anchors.begin(),
          parameters.midpoint_anchors.end(),
          [&](const SketchMidpointAnchor& anchor) {
            return anchor.point_id == point_id;
          }),
      parameters.midpoint_anchors.end());

  if (host_line_id.empty()) {
    refresh_sketch_derived_state(feature);
    return;
  }

  const auto host_it = std::find_if(
      parameters.lines.begin(),
      parameters.lines.end(),
      [&](const SketchLine& line) { return line.id == host_line_id; });
  if (host_it == parameters.lines.end()) {
    // The host line was deleted between snap-time and commit-time.
    // Silently skip — there is nothing to anchor to.
    return;
  }

  // Use a stable id derived from the bound point so the relation
  // round-trips through serialization without churning.
  parameters.midpoint_anchors.push_back(SketchMidpointAnchor{
      .id = "midpoint-anchor-" + point_id,
      .point_id = point_id,
      .line_id = host_line_id,
  });

  refresh_sketch_derived_state(feature);
}

// Anchor a point to the body of a host line at a specific parametric
// position `t` in [0, 1]. Used by the line-body snap to make the
// bound point ride with the host as it moves. Mirrors
// `set_sketch_midpoint_anchor` byte-for-byte except it also accepts
// (and clamps) the `t` value and stores it on the anchor record.
void set_sketch_point_line_anchor(FeatureEntry& feature,
                                  const std::string& point_id,
                                  const std::string& host_line_id,
                                  double t) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error(
        "Only sketch features can hold point-line anchors");
  }
  auto& parameters = *feature.sketch_parameters;

  // Drop any pre-existing anchor for the same point. Also clear any
  // midpoint anchor on the same point so the two relations don't
  // fight each other (a midpoint anchor is a degenerate point-line
  // anchor at t=0.5; the explicit point-line anchor wins).
  parameters.point_line_anchors.erase(
      std::remove_if(
          parameters.point_line_anchors.begin(),
          parameters.point_line_anchors.end(),
          [&](const SketchPointLineAnchor& anchor) {
            return anchor.point_id == point_id;
          }),
      parameters.point_line_anchors.end());
  parameters.midpoint_anchors.erase(
      std::remove_if(
          parameters.midpoint_anchors.begin(),
          parameters.midpoint_anchors.end(),
          [&](const SketchMidpointAnchor& anchor) {
            return anchor.point_id == point_id;
          }),
      parameters.midpoint_anchors.end());

  if (host_line_id.empty()) {
    refresh_sketch_derived_state(feature);
    return;
  }

  const auto host_it = std::find_if(
      parameters.lines.begin(),
      parameters.lines.end(),
      [&](const SketchLine& line) { return line.id == host_line_id; });
  if (host_it == parameters.lines.end()) {
    // The host line was deleted between snap-time and commit-time.
    // Silently skip — there is nothing to anchor to.
    return;
  }

  // Clamp `t` so out-of-range UI input can never produce an anchor
  // that re-projects past the segment's endpoints.
  const double clamped_t = std::max(0.0, std::min(1.0, t));

  parameters.point_line_anchors.push_back(SketchPointLineAnchor{
      .id = "point-line-anchor-" + point_id,
      .point_id = point_id,
      .line_id = host_line_id,
      .t = clamped_t,
  });

  refresh_sketch_derived_state(feature);
}

void add_sketch_rectangle(FeatureEntry& feature,
                          int& next_line_index,
                          double start_x,
                          double start_y,
                          double end_x,
                          double end_y,
                          bool is_construction) {
  // Stash the indices used for each side so we can build the
  // line-id pair for the equal-length relations after the loop.
  // `add_sketch_line` itself reads the line id from `line-{index}`,
  // so the only way to know the resulting ids is to capture the
  // counter values we passed in.
  const int top_index = next_line_index++;
  const int right_index = next_line_index++;
  const int bottom_index = next_line_index++;
  const int left_index = next_line_index++;

  add_sketch_line(feature, top_index, start_x, start_y, end_x, start_y,
                  is_construction);
  add_sketch_line(feature, right_index, end_x, start_y, end_x, end_y,
                  is_construction);
  add_sketch_line(feature, bottom_index, end_x, end_y, start_x, end_y,
                  is_construction);
  add_sketch_line(feature, left_index, start_x, end_y, start_x, start_y,
                  is_construction);

  // H/V constraints on each side are already inferred by
  // `add_sketch_line` (via `infer_constraint_hint`) because the
  // sides are exactly axis-aligned by construction. What's missing
  // is the equal-length pairing: top↔bottom and left↔right. Adding
  // these two relations means editing one side's length dimension
  // also updates its mirror, matching common CAD workflow's behavior.
  if (!feature.sketch_parameters.has_value()) {
    return;
  }
  auto& parameters = *feature.sketch_parameters;
  const std::string top_id = "line-" + std::to_string(top_index);
  const std::string right_id = "line-" + std::to_string(right_index);
  const std::string bottom_id = "line-" + std::to_string(bottom_index);
  const std::string left_id = "line-" + std::to_string(left_index);

  parameters.dimensions.erase(
      std::remove_if(parameters.dimensions.begin(),
                     parameters.dimensions.end(),
                     [&](const SketchDimension& dimension) {
                       return dimension.kind == "line_angle" &&
                              (dimension.entity_id == top_id ||
                               dimension.entity_id == right_id ||
                               dimension.entity_id == bottom_id ||
                               dimension.entity_id == left_id);
                     }),
      parameters.dimensions.end());

  parameters.line_relations.push_back(SketchLineRelation{
      .id = "rel-equal-length-" + top_id,
      .kind = "equal_length",
      .first_line_id = top_id,
      .second_line_id = bottom_id,
  });
  parameters.line_relations.push_back(SketchLineRelation{
      .id = "rel-equal-length-" + left_id,
      .kind = "equal_length",
      .first_line_id = left_id,
      .second_line_id = right_id,
  });
  refresh_sketch_derived_state(feature);
}

// Reflect a 2D point across the line passing through (ax, ay) and
// (bx, by). Standard formula: project P onto the line to find the
// foot F, then P' = 2F - P. Returns NaN-free results as long as
// the line has non-zero length, which `validate_line` guarantees
// at line creation.
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
void regenerate_mirror_preview(SketchFeatureParameters& parameters) {
  if (!parameters.pending_mirror.has_value()) {
    return;
  }
  auto& pending = *parameters.pending_mirror;
  pending.generated_lines.clear();
  pending.generated_circles.clear();

  if (!pending.axis_line_id.has_value()) {
    return;
  }
  const auto& axis_id = *pending.axis_line_id;
  const auto axis_it = std::find_if(
      parameters.lines.begin(), parameters.lines.end(),
      [&](const SketchLine& line) { return line.id == axis_id; });
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
  for (const auto& object_id : pending.object_ids) {
    if (object_id == axis_id) {
      continue;  // Axis to itself is a no-op.
    }
    const auto line_it = std::find_if(
        parameters.lines.begin(), parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == object_id; });
    if (line_it != parameters.lines.end()) {
      const auto [new_start_x, new_start_y] = reflect_point_across_line(
          line_it->start_x, line_it->start_y, ax, ay, bx, by);
      const auto [new_end_x, new_end_y] = reflect_point_across_line(
          line_it->end_x, line_it->end_y, ax, ay, bx, by);
      SketchLine reflected{};
      const std::string suffix = std::to_string(local_line_counter++);
      reflected.id = "pending-mirror-line-" + suffix;
      reflected.start_point_id = reflected.id + "-start";
      reflected.end_point_id = reflected.id + "-end";
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
        [&](const SketchCircle& circle) { return circle.id == object_id; });
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

void start_mirror_preview(FeatureEntry& feature) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can host a mirror tool");
  }
  // Idempotent: re-arming the mirror tool while a preview is
  // already in progress just resets the selections. Saves the
  // caller from having to cancel + start.
  feature.sketch_parameters->pending_mirror =
      SketchFeatureParameters::PendingMirror{};
}

void update_mirror_preview_axis(FeatureEntry& feature,
                                const std::string& axis_line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can host a mirror tool");
  }
  auto& parameters = *feature.sketch_parameters;
  if (!parameters.pending_mirror.has_value()) {
    throw std::runtime_error(
        "Mirror preview must be started before setting the axis");
  }
  if (axis_line_id.empty()) {
    parameters.pending_mirror->axis_line_id = std::nullopt;
  } else {
    parameters.pending_mirror->axis_line_id = axis_line_id;
  }
  regenerate_mirror_preview(parameters);
}

void update_mirror_preview_objects(
    FeatureEntry& feature, const std::vector<std::string>& object_ids) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can host a mirror tool");
  }
  auto& parameters = *feature.sketch_parameters;
  if (!parameters.pending_mirror.has_value()) {
    throw std::runtime_error(
        "Mirror preview must be started before setting objects");
  }
  parameters.pending_mirror->object_ids = object_ids;
  regenerate_mirror_preview(parameters);
}

void commit_mirror_preview(FeatureEntry& feature,
                           int& next_line_index,
                           int& next_circle_index) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can host a mirror tool");
  }
  auto& parameters = *feature.sketch_parameters;
  if (!parameters.pending_mirror.has_value()) {
    throw std::runtime_error("No mirror preview to commit");
  }
  // Snapshot the generated geometry by value before clearing the
  // pending state — `add_sketch_line` / `add_sketch_circle` will
  // also call `refresh_sketch_derived_state` which iterates over
  // sketch state and we don't want a half-cleared pending around.
  const auto generated_lines = parameters.pending_mirror->generated_lines;
  const auto generated_circles = parameters.pending_mirror->generated_circles;
  // Source ids in the order matching `generated_lines/circles` so we
  // can build a source -> mirror id map after committing. The
  // pending state has the source ids in `object_ids` but not split
  // by entity kind; we filter against the live arrays here.
  const auto object_ids = parameters.pending_mirror->object_ids;
  parameters.pending_mirror = std::nullopt;

  // Snapshot the relations BEFORE committing so we don't iterate
  // over relations we're about to add for the mirrored copies.
  const auto source_relations = parameters.line_relations;

  // Build source -> mirror id maps as we commit. The order of
  // `generated_lines` / `generated_circles` matches the order of
  // matching ids in `object_ids` (regenerate_mirror_preview walks
  // object_ids in order and pushes one entry per recognized id).
  std::unordered_map<std::string, std::string> source_to_mirror_line;
  std::unordered_map<std::string, std::string> source_to_mirror_circle;
  std::size_t generated_line_cursor = 0;
  std::size_t generated_circle_cursor = 0;
  for (const auto& source_id : object_ids) {
    const bool is_line = std::any_of(
        parameters.lines.begin(), parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == source_id; });
    if (is_line) {
      if (generated_line_cursor >= generated_lines.size()) {
        continue;  // Defensive: stale id, no preview was generated.
      }
      const auto& line = generated_lines[generated_line_cursor++];
      const std::string new_id = "line-" + std::to_string(next_line_index);
      add_sketch_line(feature, next_line_index++, line.start_x, line.start_y,
                      line.end_x, line.end_y, line.is_construction);
      source_to_mirror_line.emplace(source_id, new_id);
      continue;
    }
    const bool is_circle = std::any_of(
        parameters.circles.begin(), parameters.circles.end(),
        [&](const SketchCircle& circle) { return circle.id == source_id; });
    if (is_circle) {
      if (generated_circle_cursor >= generated_circles.size()) {
        continue;
      }
      const auto& circle = generated_circles[generated_circle_cursor++];
      const std::string new_id = "circle-" + std::to_string(next_circle_index);
      add_sketch_circle(feature, next_circle_index++, circle.center_x,
                        circle.center_y, circle.radius,
                        circle.is_construction);
      source_to_mirror_circle.emplace(source_id, new_id);
    }
  }

  // Carry over line relations whose BOTH participants were
  // mirrored. Reflection preserves equal_length / perpendicular /
  // parallel / tangent_line_circle, so duplicating the relation on
  // the mirrored pair keeps the constraint network intact. Single-
  // sided relations (only one endpoint mirrored) are skipped on
  // purpose — coupling the mirror to the original would lock the
  // user out of editing one side independently.
  for (const auto& relation : source_relations) {
    if (relation.kind == "tangent_line_circle") {
      const auto line_it = source_to_mirror_line.find(relation.first_line_id);
      const auto circle_it =
          source_to_mirror_circle.find(relation.second_line_id);
      if (line_it == source_to_mirror_line.end() ||
          circle_it == source_to_mirror_circle.end()) {
        continue;
      }
      parameters.line_relations.push_back(SketchLineRelation{
          .id = "rel-tangent-" + line_it->second + "-" + circle_it->second,
          .kind = "tangent_line_circle",
          .first_line_id = line_it->second,
          .second_line_id = circle_it->second,
      });
      continue;
    }
    const auto first_it = source_to_mirror_line.find(relation.first_line_id);
    const auto second_it = source_to_mirror_line.find(relation.second_line_id);
    if (first_it == source_to_mirror_line.end() ||
        second_it == source_to_mirror_line.end()) {
      continue;
    }
    std::string id_prefix;
    if (relation.kind == "equal_length") {
      id_prefix = "rel-equal-length-";
    } else if (relation.kind == "perpendicular") {
      id_prefix = "rel-perpendicular-";
    } else if (relation.kind == "parallel") {
      id_prefix = "rel-parallel-";
    } else {
      continue;  // Unknown kind — leave it alone rather than guess.
    }
    parameters.line_relations.push_back(SketchLineRelation{
        .id = id_prefix + first_it->second,
        .kind = relation.kind,
        .first_line_id = first_it->second,
        .second_line_id = second_it->second,
    });
  }

  // Re-run the derived-state refresh once after relation copying so
  // the constraint glyphs / profile detection see the new edges.
  refresh_sketch_derived_state(feature);
}

void cancel_mirror_preview(FeatureEntry& feature) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can host a mirror tool");
  }
  // Plain discard. `pending_mirror` was never written to the main
  // arrays so there's nothing to roll back.
  feature.sketch_parameters->pending_mirror = std::nullopt;
}

void add_sketch_circle(FeatureEntry& feature,
                       int circle_index,
                       double center_x,
                       double center_y,
                       double radius,
                       bool is_construction) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch circles");
  }

  if (radius <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch circles must have non-zero radius");
  }

  feature.sketch_parameters->circles.push_back(SketchCircle{
      .id = "circle-" + std::to_string(circle_index),
      .center_x = center_x,
      .center_y = center_y,
      .radius = radius,
      .is_construction = is_construction,
  });
  const auto& circle = feature.sketch_parameters->circles.back();
  if (!is_construction) {
    feature.sketch_parameters->dimensions.push_back(SketchDimension{
        .id = "dim-circle-" + circle.id,
        .kind = "circle_radius",
        .entity_id = circle.id,
        .value = circle.radius,
        .is_auto = true,
    });
  }
  if (!is_construction) {
    run_inference_on_new_circle(*feature.sketch_parameters,
                                feature.sketch_parameters->circles.back());
  }
  refresh_sketch_derived_state(feature);
}

void add_sketch_arc(FeatureEntry& feature,
                    int arc_index,
                    int start_point_index,
                    int end_point_index,
                    double start_x,
                    double start_y,
                    double end_x,
                    double end_y,
                    double center_x,
                    double center_y,
                    double radius,
                    bool ccw,
                    bool is_construction) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch arcs");
  }

  if (radius <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch arcs must have non-zero radius");
  }

  // Reuse an existing endpoint id when this arc's start / end land on
  // a point that another entity (line or arc) already owns. Mirrors
  // `add_sketch_line`'s behaviour so arcs and lines share endpoint
  // points whenever they meet at the same coordinates — the profile
  // loop detector relies on shared point ids (or the coord fallback)
  // to chain edges into a closed loop.
  const auto shared_start_point = find_coincident_endpoint(
      *feature.sketch_parameters, "", start_x, start_y);
  const auto shared_end_point = find_coincident_endpoint(
      *feature.sketch_parameters, "", end_x, end_y);

  const std::string start_point_id =
      shared_start_point.has_value()
          ? std::get<0>(shared_start_point.value())
          : "point-" + std::to_string(start_point_index);
  const std::string end_point_id =
      shared_end_point.has_value()
          ? std::get<0>(shared_end_point.value())
          : "point-" + std::to_string(end_point_index);

  feature.sketch_parameters->arcs.push_back(SketchArc{
      .id = "arc-" + std::to_string(arc_index),
      .start_point_id = start_point_id,
      .end_point_id = end_point_id,
      .center_x = center_x,
      .center_y = center_y,
      .radius = radius,
      .start_x = start_x,
      .start_y = start_y,
      .end_x = end_x,
      .end_y = end_y,
      .ccw = ccw,
      .is_construction = is_construction,
  });

  refresh_sketch_derived_state(feature);

  // After the rebuild above, the arc's endpoint points exist in the
  // points list. Flag them as fixed so v1 stops the user from dragging
  // them off the cached arc — the arc's geometry is otherwise frozen
  // at creation. Editing flows can flip this back to false later.
  for (auto& point : feature.sketch_parameters->points) {
    if (point.id == start_point_id || point.id == end_point_id) {
      point.is_fixed = true;
    }
  }
}

std::optional<std::tuple<double, double>> find_point_position(
    const SketchFeatureParameters& parameters,
    const std::string& point_id) {
  for (const auto& line : parameters.lines) {
    if (line.start_point_id == point_id)
      return std::tuple<double, double>{line.start_x, line.start_y};
    if (line.end_point_id == point_id)
      return std::tuple<double, double>{line.end_x, line.end_y};
  }
  const auto point_it = std::find_if(
      parameters.points.begin(), parameters.points.end(),
      [&](const SketchPoint& point) { return point.id == point_id; });
  if (point_it != parameters.points.end())
    return std::tuple<double, double>{point_it->x, point_it->y};
  return std::nullopt;
}

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
    // Compute center from the edge. The polygon has `sides` sides
    // of equal length. The edge AB is one side. The center is
    // at the perpendicular bisector of AB at distance R where
    // R = AB / (2 * sin(π/N)) for inscribed.
    double dx = end_x - start_x;
    double dy = end_y - start_y;
    double edge_len = std::hypot(dx, dy);
    if (edge_len <= kMinimumSketchDimensionValue) {
      throw std::runtime_error("Polygon edge must have non-zero length");
    }
    double mid_x = (start_x + end_x) / 2.0;
    double mid_y = (start_y + end_y) / 2.0;
    // Unit perpendicular (rotate CCW vs CW — choose one side).
    double nx = -dy / edge_len;
    double ny = dx / edge_len;
    // For inscribed: radius = edge_len / (2 * sin(π/N))
    radius = edge_len / (2.0 * std::sin(M_PI / sides));
    center_x = mid_x + nx * radius * std::cos(M_PI / sides);
    center_y = mid_y + ny * radius * std::cos(M_PI / sides);
  }

  feature.sketch_parameters->polygons.push_back(SketchPolygon{
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
    const auto& polygon = feature.sketch_parameters->polygons.back();
    feature.sketch_parameters->dimensions.push_back(SketchDimension{
        .id = "dim-polygon-" + polygon.id,
        .kind = "polygon_radius",
        .entity_id = polygon.id,
        .value = polygon.radius,
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

}  // namespace polysmith::core