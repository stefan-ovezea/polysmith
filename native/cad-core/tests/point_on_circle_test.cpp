// A `point_on_circle` constraint must MOVE geometry, not just record itself.
//
// D-008: the constraint was stored (the probe printed "point_on_circle:…",
// solver_ok=1) but the point never moved. This drives the solver directly
// (build + solve + apply) and asserts the geometry actually moves — a standalone
// point rides the circle, and a line endpoint follows a circle-centre edit. It
// fails until the solver writes the solved point back to the right record.
//
// The circle centre is fixed in both cases so the point is the only free party:
// otherwise the solver could satisfy dist(point, centre) == radius by moving the
// centre instead, which would hide exactly the defect this test pins.
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "core/sketch/constraint_solver.h"
#include "core/sketch/sketch_feature_parameters.h"

namespace {

using polysmith::core::ConstraintSolver;
using polysmith::core::SketchCircle;
using polysmith::core::SketchConstraint;
using polysmith::core::SketchFeatureParameters;
using polysmith::core::SketchLine;
using polysmith::core::SketchVertex;

int failures = 0;

bool expect(bool condition, const char* what) {
  if (!condition) {
    std::cout << "FAIL: " << what << "\n";
    ++failures;
    return false;
  }
  return true;
}

SketchVertex vertex(const std::string& id, const std::string& kind, double x,
                    double y, bool fixed) {
  SketchVertex value;
  value.id = id;
  value.kind = kind;
  value.x = x;
  value.y = y;
  value.is_fixed = fixed;
  return value;
}

void trace_if_enabled(const ConstraintSolver& solver,
                      const SketchFeatureParameters& params,
                      const char* stage) {
  if (std::getenv("PS_TRACE_POINT_ON_CIRCLE") == nullptr) return;
  const std::string trace = solver.point_on_circle_trace(params, stage);
  if (!trace.empty()) std::cout << trace;
}

// Circle (0,0) r10 with a FIXED centre; standalone point at (2,0).
SketchFeatureParameters standalone_fixture() {
  SketchFeatureParameters params;
  params.vertices.push_back(vertex("vertex-1", "point", 2.0, 0.0, false));
  params.vertices.push_back(vertex("vertex-2", "center", 0.0, 0.0, true));

  SketchCircle circle;
  circle.id = "circle-1";
  circle.center_vertex_id = "vertex-2";
  circle.center_x = 0.0;
  circle.center_y = 0.0;
  circle.radius = 10.0;
  params.circles.push_back(circle);

  SketchConstraint constraint;
  constraint.constraint_id = "constraint-1";
  constraint.kind = "point_on_circle";
  constraint.target_ids = {"vertex-1", "circle-1"};
  params.constraints.push_back(constraint);
  return params;
}

bool test_standalone_point_moves_onto_circle() {
  auto params = standalone_fixture();

  ConstraintSolver solver;
  solver.build(params);
  const auto result = solver.solve();
  if (!expect(result.ok(), "standalone: solve ok")) return false;
  trace_if_enabled(solver, params, "post-solve");
  solver.apply(params);
  trace_if_enabled(solver, params, "post-apply");

  const double point_x = params.vertices[0].x;
  const double point_y = params.vertices[0].y;
  const double dist = std::hypot(point_x, point_y);
  if (!expect(std::abs(dist - 10.0) < 1e-3,
              ("standalone: the point rides the circle at distance 10 - got " +
               std::to_string(dist)).c_str())) {
    return false;
  }
  // The point must have MOVED from (2,0), not stayed while the constraint
  // counted itself satisfied.
  return expect(std::abs(point_x - 2.0) > 1.0,
                "standalone: the point actually moved off (2,0)");
}

// Circle (0,0) r10 with a FIXED centre; a line whose start endpoint (10,0) rides
// the circle. Moving the centre to (5,0) must drag the endpoint along.
bool test_line_endpoint_follows_circle_centre_edit() {
  SketchFeatureParameters params;
  params.vertices.push_back(vertex("vertex-1", "endpoint", 10.0, 0.0, false));
  params.vertices.push_back(vertex("vertex-2", "endpoint", 20.0, 0.0, false));
  params.vertices.push_back(vertex("vertex-3", "center", 0.0, 0.0, true));

  SketchLine line;
  line.id = "line-1";
  line.start_vertex_id = "vertex-1";
  line.end_vertex_id = "vertex-2";
  line.start_x = 10.0;
  line.start_y = 0.0;
  line.end_x = 20.0;
  line.end_y = 0.0;
  params.lines.push_back(line);

  SketchCircle circle;
  circle.id = "circle-1";
  circle.center_vertex_id = "vertex-3";
  circle.center_x = 0.0;
  circle.center_y = 0.0;
  circle.radius = 10.0;
  params.circles.push_back(circle);

  SketchConstraint constraint;
  constraint.constraint_id = "constraint-1";
  constraint.kind = "point_on_circle";
  constraint.target_ids = {"vertex-1", "circle-1"};
  params.constraints.push_back(constraint);

  // Initial solve: the endpoint is already at distance 10, so it should stay.
  {
    ConstraintSolver solver;
    solver.build(params);
    if (!expect(solver.solve().ok(), "line: initial solve ok")) return false;
    solver.apply(params);
  }

  // Edit the circle centre: (0,0) -> (5,0). The fixed centre vertex must stay in
  // step with the record, since build() reads the centre from the record.
  params.circles[0].center_x = 5.0;
  params.vertices[2].x = 5.0;

  {
    ConstraintSolver solver;
    solver.build(params);
    if (!expect(solver.solve().ok(), "line: edit solve ok")) return false;
    trace_if_enabled(solver, params, "post-solve");
    solver.apply(params);
    trace_if_enabled(solver, params, "post-apply");
  }

  const double dist =
      std::hypot(params.lines[0].start_x - 5.0, params.lines[0].start_y);
  return expect(std::abs(dist - 10.0) < 1e-3,
                ("line: the endpoint follows the centre at distance 10 - got " +
                 std::to_string(dist)).c_str());
}

}  // namespace

int main() {
  if (!test_standalone_point_moves_onto_circle()) return 1;
  if (!test_line_endpoint_follows_circle_centre_edit()) return 1;
  std::cout << "point_on_circle_test passed\n";
  return 0;
}
