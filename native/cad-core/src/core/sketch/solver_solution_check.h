#pragma once

// Does the geometry satisfy the constraints the solver was asked to satisfy?
//
// planegcs is not a black box here, and treating it as one was unsound:
// `System::initSolution()` runs `diagnose()` before solving, and diagnose REMOVES
// the redundant constraints from the system it then solves (`clistR`). Whatever
// comes back therefore satisfies a SUBSET of what the sketch asks for, and the
// dropped constraints are free to drift — which is what "the solver randomly
// destroyed my sketch" looked like.
//
// So every solution is verified before it is applied. This header holds the check
// itself as a pure function of the sketch parameters, so it can be unit-tested
// without a solver, a document or an app.
//
// Coverage is deliberately limited to constraints whose geometry can be checked
// without re-deriving the solver's internal orientation conventions:
//
//   dimensions          line_length, line_angle, angle, circle_radius,
//                       point_distance, circle_center_distance,
//                       circle_line_distance
//   line relations      parallel, perpendicular, equal_length
//   axis constraints    horizontal, vertical
//   anchors             midpoint, point-on-line (fraction t)
//
// NOT covered: the `constraints` records (coincident, concentric, collinear,
// midpoint, symmetric, endpoint, center, the four tangency kinds). Their
// `target_ids` semantics differ per kind, and guessing them wrongly would reject
// valid solutions — a verifier that freezes a healthy sketch is worse than no
// verifier. Both directions are still left to the solver; this is a net, not a
// proof.
//
// An empty result means "no violation detected", never "the solve is correct".

#include <algorithm>
#include <cmath>
#include <string>

#include "core/sketch/sketch_feature_parameters.h"

namespace polysmith::core {

namespace solver_check_detail {

constexpr double kLengthTolerance = 1e-3;     // mm
constexpr double kAngleTolerance = 1e-4;      // rad (~0.006 deg)
constexpr double kDirectionTolerance = 1e-6;  // relative

inline const SketchLine* find_line(const SketchFeatureParameters& parameters,
                                   const std::string& id) {
  for (const auto& line : parameters.lines) {
    if (line.id == id) return &line;
  }
  return nullptr;
}

inline const SketchCircle* find_circle(const SketchFeatureParameters& parameters,
                                       const std::string& id) {
  for (const auto& circle : parameters.circles) {
    if (circle.id == id) return &circle;
  }
  return nullptr;
}

inline const SketchVertex* find_vertex(const SketchFeatureParameters& parameters,
                                       const std::string& id) {
  for (const auto& vertex : parameters.vertices) {
    if (vertex.id == id) return &vertex;
  }
  return nullptr;
}

// Where a vertex ACTUALLY is right after a solve.
//
// The vertex table is rebuilt later in the refresh (Phase 5), so immediately after
// `solver.apply` it still holds the PRE-solve positions while the entity caches hold the
// solved ones. Measuring through the table therefore rejected good solutions and told the
// user "point_on_circle off by 18.97 mm" for a point that was exactly on its circle - and
// the rejection then restored the pre-solve geometry, so the constraint looked like it did
// nothing at all (which is what D-008 chased for a day). Entity caches first, table last.
inline bool vertex_position(const SketchFeatureParameters& parameters,
                            const std::string& id, double& x, double& y) {
  for (const auto& line : parameters.lines) {
    if (line.start_vertex_id == id) {
      x = line.start_x;
      y = line.start_y;
      return true;
    }
    if (line.end_vertex_id == id) {
      x = line.end_x;
      y = line.end_y;
      return true;
    }
  }
  for (const auto& arc : parameters.arcs) {
    if (arc.start_vertex_id == id) {
      x = arc.start_x;
      y = arc.start_y;
      return true;
    }
    if (arc.end_vertex_id == id) {
      x = arc.end_x;
      y = arc.end_y;
      return true;
    }
    if (arc.center_vertex_id == id) {
      x = arc.center_x;
      y = arc.center_y;
      return true;
    }
  }
  for (const auto& circle : parameters.circles) {
    if (circle.center_vertex_id == id) {
      x = circle.center_x;
      y = circle.center_y;
      return true;
    }
  }
  const SketchVertex* vertex = find_vertex(parameters, id);
  if (vertex == nullptr) return false;
  x = vertex->x;
  y = vertex->y;
  return true;
}

inline double line_length(const SketchLine& line) {
  return std::hypot(line.end_x - line.start_x, line.end_y - line.start_y);
}

// Perpendicular distance from a point to an infinite line.
inline double point_line_distance(double px, double py, const SketchLine& line) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double length = std::hypot(dx, dy);
  if (length <= kLengthTolerance) return std::hypot(px - line.start_x,
                                                    py - line.start_y);
  return std::abs((px - line.start_x) * dy - (py - line.start_y) * dx) / length;
}

// The angle between two lines, without depending on which of the four
// direction combinations the solver happened to use: the check accepts either a
// measured angle or its supplement, so it can only miss a violation, never
// invent one.
inline bool angle_matches(double ax, double ay, double bx, double by,
                          double expected) {
  const double cross = ax * by - ay * bx;
  const double dot = ax * bx + ay * by;
  const double measured = std::abs(std::atan2(cross, dot));
  const double magnitude = std::abs(expected);
  if (std::abs(measured - magnitude) <= kAngleTolerance) return true;
  return std::abs((M_PI - measured) - magnitude) <= kAngleTolerance;
}

}  // namespace solver_check_detail

// Empty string = the geometry satisfies everything this can check. Otherwise a
// short description of the first violation, naming the entities involved.
inline std::string solver_constraint_violation(
    const SketchFeatureParameters& parameters) {
  using namespace solver_check_detail;

  // Axis constraints live inline on the line.
  for (const auto& line : parameters.lines) {
    if (!line.constraint.has_value()) continue;
    if (line.constraint.value() == "horizontal") {
      if (std::abs(line.start_y - line.end_y) > kLengthTolerance) {
        return "horizontal on " + line.id + " (dy = " +
               std::to_string(line.end_y - line.start_y) + ")";
      }
    } else if (line.constraint.value() == "vertical") {
      if (std::abs(line.start_x - line.end_x) > kLengthTolerance) {
        return "vertical on " + line.id + " (dx = " +
               std::to_string(line.end_x - line.start_x) + ")";
      }
    }
  }

  // Inter-line relations.
  for (const auto& relation : parameters.line_relations) {
    const SketchLine* first = find_line(parameters, relation.first_line_id);
    const SketchLine* second = find_line(parameters, relation.second_line_id);
    if (first == nullptr || second == nullptr) continue;
    const double ax = first->end_x - first->start_x;
    const double ay = first->end_y - first->start_y;
    const double bx = second->end_x - second->start_x;
    const double by = second->end_y - second->start_y;
    const double a_length = std::hypot(ax, ay);
    const double b_length = std::hypot(bx, by);
    if (a_length <= kLengthTolerance || b_length <= kLengthTolerance) continue;
    const double scale = a_length * b_length;
    if (relation.kind == "parallel") {
      if (std::abs(ax * by - ay * bx) > kDirectionTolerance * scale) {
        return "parallel " + first->id + "/" + second->id;
      }
    } else if (relation.kind == "perpendicular") {
      if (std::abs(ax * bx + ay * by) > kDirectionTolerance * scale) {
        return "perpendicular " + first->id + "/" + second->id;
      }
    } else if (relation.kind == "equal_length") {
      if (std::abs(a_length - b_length) > kLengthTolerance) {
        return "equal length " + first->id + "/" + second->id;
      }
    }
  }

  // Anchors: the solver models them, so a solution that leaves an anchored point
  // off its host has silently broken a promise the user cannot see.
  for (const auto& anchor : parameters.midpoint_anchors) {
    const SketchLine* host = find_line(parameters, anchor.line_id);
    double vx = 0.0;
    double vy = 0.0;
    if (host == nullptr ||
        !vertex_position(parameters, anchor.vertex_id, vx, vy)) {
      continue;
    }
    const double mid_x = (host->start_x + host->end_x) * 0.5;
    const double mid_y = (host->start_y + host->end_y) * 0.5;
    if (std::hypot(vx - mid_x, vy - mid_y) > kLengthTolerance) {
      return "midpoint anchor of " + anchor.vertex_id + " on " + host->id;
    }
  }
  for (const auto& anchor : parameters.point_line_anchors) {
    const SketchLine* host = find_line(parameters, anchor.line_id);
    double vx = 0.0;
    double vy = 0.0;
    if (host == nullptr ||
        !vertex_position(parameters, anchor.vertex_id, vx, vy)) {
      continue;
    }
    const double expected_x =
        host->start_x + anchor.t * (host->end_x - host->start_x);
    const double expected_y =
        host->start_y + anchor.t * (host->end_y - host->start_y);
    if (std::hypot(vx - expected_x, vy - expected_y) > kLengthTolerance) {
      return "point-on-line anchor of " + anchor.vertex_id + " on " + host->id;
    }
  }

  // Point-on-circle constraints (a vertex that rides a circle's boundary). These are
  // stored as constraint records rather than anchors, and they are exactly the kind
  // whose violation is invisible: the point still looks near the circle.
  for (const auto& constraint : parameters.constraints) {
    if (constraint.kind != "point_on_circle") continue;
    if (constraint.target_ids.size() < 2) continue;
    double px = 0.0;
    double py = 0.0;
    const SketchCircle* circle = find_circle(parameters, constraint.target_ids[1]);
    if (circle == nullptr ||
        !vertex_position(parameters, constraint.target_ids[0], px, py)) {
      continue;
    }
    const double radial =
        std::abs(std::hypot(px - circle->center_x, py - circle->center_y) -
                 circle->radius);
    if (radial > kLengthTolerance) {
      return "point_on_circle " + constraint.target_ids[0] + "/" + circle->id +
             " off by " + std::to_string(radial) + " mm";
    }
  }

  // Driving dimensions. Auto and driven ones are measurements, not demands.
  for (const auto& dimension : parameters.dimensions) {
    if (dimension.driven) continue;
    if (dimension.is_auto && dimension.expression.empty()) continue;

    if (dimension.kind == "line_length") {
      const SketchLine* line = find_line(parameters, dimension.entity_id);
      if (line == nullptr) continue;
      const double length = line_length(*line);
      if (std::abs(length - dimension.value) > kLengthTolerance) {
        return "length of " + line->id + " = " + std::to_string(length) +
               " (wanted " + std::to_string(dimension.value) + ")";
      }
    } else if (dimension.kind == "line_angle") {
      const SketchLine* line = find_line(parameters, dimension.entity_id);
      if (line == nullptr) continue;
      const double angle =
          std::atan2(line->end_y - line->start_y, line->end_x - line->start_x);
      if (std::abs(angle - dimension.value) > kAngleTolerance) {
        return "angle of " + line->id;
      }
    } else if (dimension.kind == "angle" &&
               !dimension.secondary_entity_id.empty()) {
      const SketchLine* first = find_line(parameters, dimension.entity_id);
      const SketchLine* second =
          find_line(parameters, dimension.secondary_entity_id);
      if (first == nullptr || second == nullptr) continue;
      if (!angle_matches(first->end_x - first->start_x,
                         first->end_y - first->start_y,
                         second->end_x - second->start_x,
                         second->end_y - second->start_y, dimension.value)) {
        return "angle " + first->id + "/" + second->id;
      }
    } else if (dimension.kind == "circle_radius") {
      const SketchCircle* circle = find_circle(parameters, dimension.entity_id);
      if (circle == nullptr) continue;
      if (std::abs(circle->radius - dimension.value) > kLengthTolerance) {
        return "radius of " + circle->id + " = " +
               std::to_string(circle->radius) + " (wanted " +
               std::to_string(dimension.value) + ")";
      }
    } else if (dimension.kind == "circle_center_distance" &&
               !dimension.secondary_entity_id.empty()) {
      const SketchCircle* first = find_circle(parameters, dimension.entity_id);
      const SketchCircle* second =
          find_circle(parameters, dimension.secondary_entity_id);
      if (first == nullptr || second == nullptr) continue;
      const double measured = std::hypot(first->center_x - second->center_x,
                                         first->center_y - second->center_y);
      if (std::abs(measured - dimension.value) > kLengthTolerance) {
        return "center distance " + first->id + "/" + second->id + " = " +
               std::to_string(measured) + " (wanted " +
               std::to_string(dimension.value) + ")";
      }
    } else if (dimension.kind == "circle_line_distance" &&
               !dimension.secondary_entity_id.empty()) {
      const SketchCircle* circle = find_circle(parameters, dimension.entity_id);
      const SketchLine* line =
          find_line(parameters, dimension.secondary_entity_id);
      if (circle == nullptr || line == nullptr) continue;
      const double measured =
          point_line_distance(circle->center_x, circle->center_y, *line);
      if (std::abs(measured - dimension.value) > kLengthTolerance) {
        return "center-to-line distance " + circle->id + "/" + line->id +
               " = " + std::to_string(measured) + " (wanted " +
               std::to_string(dimension.value) + ")";
      }
    } else if (dimension.kind == "point_distance") {
      double ax = 0.0;
      double ay = 0.0;
      double bx = 0.0;
      double by = 0.0;
      if (!vertex_position(parameters, dimension.entity_id, ax, ay) ||
          !vertex_position(parameters, dimension.secondary_entity_id, bx, by)) {
        continue;
      }
      const double dx = bx - ax;
      const double dy = by - ay;
      const double measured = dimension.display_as == "x"   ? std::abs(dx)
                              : dimension.display_as == "y" ? std::abs(dy)
                                                            : std::hypot(dx, dy);
      if (std::abs(measured - dimension.value) > kLengthTolerance) {
        return "distance " + dimension.entity_id + "-" +
               dimension.secondary_entity_id + " = " +
               std::to_string(measured) + " (wanted " +
               std::to_string(dimension.value) + ")";
      }
    }
  }
  return std::string();
}

}  // namespace polysmith::core
