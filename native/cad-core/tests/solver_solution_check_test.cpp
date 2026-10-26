// The solve verifier, tested as a pure function.
//
// planegcs drops redundant constraints from the system it solves
// (initSolution -> diagnose -> clistR), so a solution can violate constraints the
// user can see. `solver_constraint_violation` is the net: after every solve the
// geometry is checked, and a violating solution is discarded instead of applied.
//
// A net that raises a false alarm is worse than no net - a healthy sketch would
// be frozen, and the user would be told a constraint "could not be met" when it
// was met. So the cases below pin BOTH directions: violations are detected, and
// satisfiable geometry passes. The angle case in particular must accept either
// the interior angle or its supplement, because the solver's direction
// convention differs per edge order and the verifier may not depend on it.
#include <cmath>
#include <iostream>
#include <string>

#include "core/sketch/sketch_feature_parameters.h"
#include "core/sketch/solver_solution_check.h"

namespace {

using polysmith::core::SketchCircle;
using polysmith::core::SketchDimension;
using polysmith::core::SketchFeatureParameters;
using polysmith::core::SketchLine;
using polysmith::core::SketchLineRelation;
using polysmith::core::SketchMidpointAnchor;
using polysmith::core::SketchVertex;
using polysmith::core::SketchVertexLineAnchor;

int failures = 0;

bool expect(bool condition, const char* what) {
  if (!condition) {
    std::cout << "FAIL: " << what << "\n";
    ++failures;
    return false;
  }
  return true;
}

SketchLine line(const std::string& id, double x1, double y1, double x2,
                double y2) {
  SketchLine value;
  value.id = id;
  value.start_x = x1;
  value.start_y = y1;
  value.end_x = x2;
  value.end_y = y2;
  return value;
}

SketchCircle circle(const std::string& id, double cx, double cy, double radius) {
  SketchCircle value;
  value.id = id;
  value.center_x = cx;
  value.center_y = cy;
  value.radius = radius;
  return value;
}

SketchDimension dimension(const std::string& kind, const std::string& entity,
                          double value, const std::string& secondary = "") {
  SketchDimension result;
  result.kind = kind;
  result.entity_id = entity;
  result.secondary_entity_id = secondary;
  result.value = value;
  result.is_auto = false;
  result.driven = false;
  return result;
}

// A square-ish rectangle that satisfies its own constraints.
SketchFeatureParameters healthy_rectangle() {
  SketchFeatureParameters parameters;
  parameters.lines = {
      line("line-1", 0.0, 0.0, 40.0, 0.0),
      line("line-2", 40.0, 0.0, 40.0, 20.0),
      line("line-3", 40.0, 20.0, 0.0, 20.0),
      line("line-4", 0.0, 20.0, 0.0, 0.0),
  };
  parameters.lines[0].constraint = "horizontal";
  SketchLineRelation horizontal_pair;
  horizontal_pair.kind = "parallel";
  horizontal_pair.first_line_id = "line-1";
  horizontal_pair.second_line_id = "line-3";
  SketchLineRelation vertical_pair;
  vertical_pair.kind = "parallel";
  vertical_pair.first_line_id = "line-2";
  vertical_pair.second_line_id = "line-4";
  SketchLineRelation corner;
  corner.kind = "perpendicular";
  corner.first_line_id = "line-1";
  corner.second_line_id = "line-2";
  parameters.line_relations = {horizontal_pair, vertical_pair, corner};
  return parameters;
}

bool test_a_healthy_sketch_reports_nothing() {
  const SketchFeatureParameters parameters = healthy_rectangle();
  const std::string violation =
      polysmith::core::solver_constraint_violation(parameters);
  return expect(violation.empty(),
                ("healthy rectangle reports no violation - got: " + violation)
                    .c_str());
}

bool test_a_broken_parallel_is_detected() {
  SketchFeatureParameters parameters = healthy_rectangle();
  // Tilt the fourth side: both vertical parallels and the right angle break.
  parameters.lines[3] = line("line-4", 0.0, 20.0, 6.0, 1.0);
  const std::string violation =
      polysmith::core::solver_constraint_violation(parameters);
  return expect(!violation.empty() && violation.find("parallel") !=
                                             std::string::npos,
                ("a broken parallel names the relation - got: " + violation)
                    .c_str());
}

bool test_a_broken_length_is_detected() {
  SketchFeatureParameters parameters = healthy_rectangle();
  parameters.dimensions = {dimension("line_length", "line-1", 60.0)};
  const std::string violation =
      polysmith::core::solver_constraint_violation(parameters);
  return expect(violation.find("length of line-1") != std::string::npos,
                ("a broken length is reported - got: " + violation).c_str());
}

bool test_a_satisfied_length_passes() {
  SketchFeatureParameters parameters = healthy_rectangle();
  parameters.dimensions = {dimension("line_length", "line-1", 40.0)};
  return expect(
      polysmith::core::solver_constraint_violation(parameters).empty(),
      "a satisfied length passes");
}

bool test_a_reference_dimension_is_not_a_demand() {
  SketchFeatureParameters parameters = healthy_rectangle();
  SketchDimension reference = dimension("line_length", "line-1", 123.0);
  reference.driven = true;
  parameters.dimensions = {reference};
  return expect(
      polysmith::core::solver_constraint_violation(parameters).empty(),
      "a driven (reference) dimension is not enforced");
}

bool test_an_angle_accepts_the_supplement() {
  // Perpendicular lines: the solver may enforce 90 deg or -90 deg (or measure the
  // supplement) depending on edge order. Both must pass, or every right angle in
  // every sketch would be reported as a violation.
  SketchFeatureParameters parameters;
  parameters.lines = {line("line-1", 0.0, 0.0, 10.0, 0.0),
                      line("line-2", 0.0, 0.0, 0.0, 10.0)};
  parameters.dimensions = {
      dimension("angle", "line-1", M_PI / 2.0, "line-2"),
      dimension("angle", "line-2", -M_PI / 2.0, "line-1"),
  };
  const std::string violation =
      polysmith::core::solver_constraint_violation(parameters);
  return expect(violation.empty(),
                ("a right angle passes in either direction - got: " + violation)
                    .c_str());
}

bool test_a_broken_angle_is_detected() {
  SketchFeatureParameters parameters;
  parameters.lines = {line("line-1", 0.0, 0.0, 10.0, 0.0),
                      line("line-2", 0.0, 0.0, 10.0, 10.0)};
  parameters.dimensions = {dimension("angle", "line-1", M_PI / 2.0, "line-2")};
  return expect(!polysmith::core::solver_constraint_violation(parameters).empty(),
                "a 45 deg angle does not satisfy a 90 deg dimension");
}

bool test_circle_distances_are_checked() {
  SketchFeatureParameters parameters;
  parameters.circles = {circle("circle-1", 0.0, 0.0, 4.0),
                        circle("circle-2", 30.0, 40.0, 4.0)};
  parameters.lines = {line("line-1", 0.0, 50.0, 100.0, 50.0)};
  // 3-4-5 triangle: centers are 50 apart, and the first center is 50 from the line.
  parameters.dimensions = {
      dimension("circle_center_distance", "circle-1", 50.0, "circle-2"),
      dimension("circle_line_distance", "circle-1", 50.0, "line-1"),
      dimension("circle_radius", "circle-1", 4.0),
  };
  const std::string satisfied =
      polysmith::core::solver_constraint_violation(parameters);
  if (!expect(satisfied.empty(),
              ("satisfied circle dimensions pass - got: " + satisfied).c_str())) {
    return false;
  }
  parameters.dimensions[0].value = 60.0;
  const std::string broken =
      polysmith::core::solver_constraint_violation(parameters);
  return expect(broken.find("center distance") != std::string::npos,
                ("a broken center distance is reported - got: " + broken)
                    .c_str());
}

bool test_anchors_are_checked() {
  SketchFeatureParameters parameters;
  parameters.lines = {line("line-1", 0.0, 0.0, 20.0, 0.0)};
  SketchVertex midpoint;
  midpoint.id = "vertex-1";
  midpoint.x = 10.0;
  midpoint.y = 0.0;
  SketchVertex third;
  third.id = "vertex-2";
  third.x = 20.0 / 3.0;
  third.y = 0.0;
  SketchVertex stray;
  stray.id = "vertex-3";
  stray.x = 30.0;
  stray.y = 0.0;
  parameters.vertices = {midpoint, third, stray};

  SketchMidpointAnchor midpoint_anchor;
  midpoint_anchor.vertex_id = "vertex-1";
  midpoint_anchor.line_id = "line-1";
  parameters.midpoint_anchors = {midpoint_anchor};

  SketchVertexLineAnchor third_anchor;
  third_anchor.vertex_id = "vertex-2";
  third_anchor.line_id = "line-1";
  third_anchor.t = 1.0 / 3.0;
  parameters.point_line_anchors = {third_anchor};

  const std::string satisfied =
      polysmith::core::solver_constraint_violation(parameters);
  if (!expect(satisfied.empty(),
              ("satisfied anchors pass - got: " + satisfied).c_str())) {
    return false;
  }

  parameters.vertices[0].x = 12.0;  // off the midpoint
  const std::string broken_midpoint =
      polysmith::core::solver_constraint_violation(parameters);
  if (!expect(broken_midpoint.find("midpoint anchor") != std::string::npos,
              ("a broken midpoint anchor is reported - got: " +
               broken_midpoint)
                  .c_str())) {
    return false;
  }

  parameters.vertices[0].x = 10.0;
  parameters.vertices[1].x = 15.0;  // off the one-third point
  const std::string broken_third =
      polysmith::core::solver_constraint_violation(parameters);
  return expect(broken_third.find("point-on-line anchor") != std::string::npos,
                ("a broken point-on-line anchor is reported - got: " +
                 broken_third)
                    .c_str());
}

bool test_a_missing_entity_is_not_a_violation() {
  // A dimension whose entity the solve deleted (a trim, a profile change) must
  // not be reported as a violated constraint: there is nothing to measure, and a
  // false alarm here would freeze the sketch on the next edit.
  SketchFeatureParameters parameters;
  parameters.dimensions = {dimension("line_length", "line-99", 10.0)};
  return expect(
      polysmith::core::solver_constraint_violation(parameters).empty(),
      "a dimension on a missing entity is skipped");
}

}  // namespace

int main() {
  if (!test_a_healthy_sketch_reports_nothing()) return 1;
  if (!test_a_broken_parallel_is_detected()) return 1;
  if (!test_a_broken_length_is_detected()) return 1;
  if (!test_a_satisfied_length_passes()) return 1;
  if (!test_a_reference_dimension_is_not_a_demand()) return 1;
  if (!test_an_angle_accepts_the_supplement()) return 1;
  if (!test_a_broken_angle_is_detected()) return 1;
  if (!test_circle_distances_are_checked()) return 1;
  if (!test_anchors_are_checked()) return 1;
  if (!test_a_missing_entity_is_not_a_violation()) return 1;

  std::cout << "solver_solution_check_test passed\n";
  return 0;
}
