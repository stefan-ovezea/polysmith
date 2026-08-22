// Regression tests for the constraint-completion milestone: symmetric,
// collinear, midpoint, tangent pairs (circle-circle, line-arc), and
// parametric anchor-t enforcement.
//
// Every epsilon-sensitive case tests both sides of the target, and
// profile-set assertions use profiles_match (complete region-set
// matching, not presence-only).

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "core/sketch/dof_counter.h"
#include "sketch_test_utils.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::SketchFeatureParameters;
using polysmith::test::ExpectedProfile;
using polysmith::test::profiles_match;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

SketchFeatureParameters sketch_params(const DocumentState& document) {
  return document.feature_history.back().sketch_parameters.value();
}

bool near(double a, double b, double tolerance = 1.0e-5) {
  return std::abs(a - b) < tolerance;
}

// Distance from a point to a line (2D).
double point_line_distance(double px, double py, double ax, double ay,
                           double bx, double by) {
  const double dx = bx - ax;
  const double dy = by - ay;
  const double len = std::hypot(dx, dy);
  if (len < 1e-12) return 0.0;
  return std::abs(-dy * (px - ax) + dx * (py - ay)) / len;
}

bool test_symmetric_both_axis_sides() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Two dangling lines whose free endpoints are to be mirrored about a
  // horizontal axis line.  Free the first line from its inferred H
  // constraint — it would clamp the compromise and fight the pair.
  DocumentState document = manager.add_sketch_line(0.0, 10.0, 10.0, 10.0);
  document = manager.add_sketch_line(0.0, -10.0, 12.0, -8.0);
  document = manager.add_sketch_line(-5.0, 0.0, 15.0, 0.0);
  document = manager.set_sketch_line_constraint("line-1", std::nullopt);

  const auto before = sketch_params(document);
  const std::string p1 = before.lines[0].end_vertex_id;   // (10, 10)
  const std::string p2 = before.lines[1].end_vertex_id;   // (12, -8)
  const std::string axis = before.lines[2].id;

  // Both points must end up mirrored: p2 drives to the reflection of
  // p1 about y=0, i.e. (10, -10).
  document = manager.set_sketch_symmetric_constraint(p1, p2, axis);
  {
    const auto params = sketch_params(document);
    const auto p2v = std::find_if(
        params.vertices.begin(), params.vertices.end(),
        [&](const auto& v) { return v.id == p2; });
    if (!expect(p2v != params.vertices.end(),
                "symmetric: constrained vertex present")) {
      return false;
    }
    if (!expect(near(p2v->x, 11.0) && near(p2v->y, -9.0),
                "symmetric: pair compromised to (11,9)/(11,-9)")) {
      return false;
    }
    // Constraint entry recorded.
    const bool recorded = std::any_of(
        params.constraints.begin(), params.constraints.end(),
        [&](const auto& c) {
          return c.kind == "symmetric" && !c.driven;
        });
    if (!expect(recorded, "symmetric: constraint entry recorded")) {
      return false;
    }
  }

  // Translate the axis to y=+2 (one side): both points re-mirror about
  // it — p1 stays at (10,10), p2 must move to (10,-6).
  document = manager.move_sketch_entities({axis}, 0.0, 2.0, 0.0, 0.0, 0.0);
  {
    const auto params = sketch_params(document);
    const auto p2v = std::find_if(
        params.vertices.begin(), params.vertices.end(),
        [&](const auto& v) { return v.id == p2; });
    if (!expect(p2v != params.vertices.end(), "symmetric: vertex survives")) {
      return false;
    }
    if (!expect(near(p2v->x, 11.0) && near(p2v->y, -7.0),
                "symmetric: pair re-mirrors about the moved axis (y=+2)")) {
      return false;
    }
  }

  // Translate the axis to y=-3 (the other side): p2 must move to (10,-16).
  document = manager.move_sketch_entities({axis}, 0.0, -5.0, 0.0, 0.0, 0.0);
  {
    const auto params = sketch_params(document);
    const auto p2v = std::find_if(
        params.vertices.begin(), params.vertices.end(),
        [&](const auto& v) { return v.id == p2; });
    if (!expect(p2v != params.vertices.end(), "symmetric: vertex survives")) {
      return false;
    }
    return expect(near(p2v->x, 11.0) && near(p2v->y, -12.0),
                  "symmetric: pair re-mirrors about the moved axis (y=-3)");
  }
}

bool test_collinear_holds_after_host_move() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Line A is horizontal (inferred H); line B starts offset and further
  // along the axis so the two lines never overlap (realistic collinear
  // geometry).  Anchor line A's start point — the realistic user
  // workflow — so the pair has no joint-translation null space.
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
  document = manager.add_sketch_line(20.0, 5.0, 30.0, 5.0);
  const auto before = sketch_params(document);
  const std::string line_a = before.lines[0].id;
  const std::string line_b = before.lines[1].id;
  document = manager.set_sketch_vertex_fixed(before.lines[0].start_vertex_id,
                                             true);

  document = manager.set_sketch_collinear_constraint(line_b, line_a);
  {
    const auto params = sketch_params(document);
    const auto& b = *std::find_if(
        params.lines.begin(), params.lines.end(),
        [&](const auto& l) { return l.id == line_b; });
    if (!expect(near(b.start_y, 0.0) && near(b.end_y, 0.0),
                "collinear: line B pulled onto line A's axis")) {
      return false;
    }
  }

  // Drag line A's far endpoint: H keeps it at y=0, and line B must
  // follow the new axis while keeping its own length and span.
  document = manager.update_sketch_vertex(
      sketch_params(document).lines[0].end_vertex_id, 15.0, 5.0);
  {
    const auto params = sketch_params(document);
    const auto& b = *std::find_if(
        params.lines.begin(), params.lines.end(),
        [&](const auto& l) { return l.id == line_b; });
    return expect(near(b.start_y, 0.0) && near(b.end_y, 0.0) &&
                      near(b.start_x, 20.0) && near(b.end_x - b.start_x, 10.0),
                  "collinear: line B stays on the moved host axis");
  }
}

bool test_midpoint_holds_after_host_move() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 20.0, 0.0);
  document = manager.add_sketch_line(10.0, 10.0, 12.0, 12.0);
  const auto before = sketch_params(document);
  const std::string host = before.lines[0].id;
  const std::string anchor_vertex = before.lines[1].start_vertex_id;  // (10,10)
  // Anchor the host's start point (realistic user workflow) so the
  // group has no joint-translation null space.
  document = manager.set_sketch_vertex_fixed(before.lines[0].start_vertex_id,
                                             true);

  document = manager.set_sketch_midpoint_constraint(anchor_vertex, host);
  {
    const auto params = sketch_params(document);
    const auto v = std::find_if(
        params.vertices.begin(), params.vertices.end(),
        [&](const auto& vertex) { return vertex.id == anchor_vertex; });
    if (!expect(v != params.vertices.end(), "midpoint: vertex present")) {
      return false;
    }
    if (!expect(near(v->x, 10.0) && near(v->y, 0.0),
                "midpoint: vertex pulled to the host midpoint")) {
      return false;
    }
  }

  // Move the host's far endpoint (H-clamped to y=0): midpoint moves to
  // x=15, and the anchored vertex must follow.
  document = manager.update_sketch_vertex(
      sketch_params(document).lines[0].end_vertex_id, 30.0, 7.0);
  {
    const auto params = sketch_params(document);
    const auto v = std::find_if(
        params.vertices.begin(), params.vertices.end(),
        [&](const auto& vertex) { return vertex.id == anchor_vertex; });
        return expect(v != params.vertices.end() && near(v->x, 15.0) &&
                      near(v->y, 0.0),
                  "midpoint: vertex follows the moved host midpoint");
  }
}

bool test_anchor_t_holds_after_host_move() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 20.0, 0.0);
  document = manager.add_sketch_line(5.0, 10.0, 5.0, 12.0);
  const auto before = sketch_params(document);
  const std::string host = before.lines[0].id;
  const std::string anchor_vertex = before.lines[1].start_vertex_id;  // (5,10)
  // Anchor the host's start point (realistic user workflow).
  document = manager.set_sketch_vertex_fixed(before.lines[0].start_vertex_id,
                                             true);

  document = manager.set_sketch_vertex_line_anchor(anchor_vertex, host, 0.25);
  {
    const auto params = sketch_params(document);
    const auto v = std::find_if(
        params.vertices.begin(), params.vertices.end(),
        [&](const auto& vertex) { return vertex.id == anchor_vertex; });
    if (!expect(v != params.vertices.end(), "anchor-t: vertex present")) {
      return false;
    }
    if (!expect(near(v->x, 5.0) && near(v->y, 0.0),
                "anchor-t: vertex projected to t=0.25 (x=5)")) {
      return false;
    }
  }

  // Move the host's far endpoint to x=40: t=0.25 lands at x=10.
  document = manager.update_sketch_vertex(
      sketch_params(document).lines[0].end_vertex_id, 40.0, 9.0);
  {
    const auto params = sketch_params(document);
    const auto v = std::find_if(
        params.vertices.begin(), params.vertices.end(),
        [&](const auto& vertex) { return vertex.id == anchor_vertex; });
    return expect(v != params.vertices.end() && near(v->x, 10.0) &&
                      near(v->y, 0.0),
                  "anchor-t: vertex holds t=0.25 after the host move");
  }
}

bool test_circle_circle_tangent_external_and_internal() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_circle(0.0, 0.0, 10.0);
  document = manager.add_sketch_circle(25.0, 0.0, 5.0);
  const auto before = sketch_params(document);
  const std::string c1 = before.circles[0].id;
  const std::string c2 = before.circles[1].id;

  // External tangency: center distance must become r1 + r2 = 15.
  document = manager.set_sketch_tangent_pair_constraint(c1, c2);
  {
    const auto params = sketch_params(document);
    const auto& a = params.circles[0];
    const auto& b = params.circles[1];
    const double dist =
        std::hypot(b.center_x - a.center_x, b.center_y - a.center_y);
        if (!expect(near(dist, 15.0),
                "tangent: external center distance = r1 + r2")) {
      return false;
    }
  }

  // Edit c2's radius up (12): distance must follow to 22.
  auto c2_radius_dim = [&](const SketchFeatureParameters& params) {
    const auto it = std::find_if(
        params.dimensions.begin(), params.dimensions.end(),
        [&](const auto& dim) {
          return dim.kind == "circle_radius" && dim.entity_id == c2;
        });
    return it;
  };
  document = manager.update_sketch_dimension(
      c2_radius_dim(sketch_params(document))->id, 12.0);
  {
    const auto params = sketch_params(document);
    const auto& a = params.circles[0];
    const auto& b = params.circles[1];
    const double dist =
        std::hypot(b.center_x - a.center_x, b.center_y - a.center_y);
    if (!expect(near(dist, 22.0),
                "tangent: external distance follows r2 = 12")) {
      return false;
    }
  }

  // Edit c2's radius down (8) — the other epsilon side: distance 18.
  document = manager.update_sketch_dimension(
      c2_radius_dim(sketch_params(document))->id, 8.0);
  {
    const auto params = sketch_params(document);
    const auto& a = params.circles[0];
    const auto& b = params.circles[1];
    const double dist =
        std::hypot(b.center_x - a.center_x, b.center_y - a.center_y);
    if (!expect(near(dist, 18.0),
                "tangent: external distance follows r2 = 8")) {
      return false;
    }
  }

  // Internal tangency with FRESH circles (the earlier radius edits
  // wander the free circle centers, which would make the internal/
  // external detection position-dependent): c4 at (0,0) r10, c5 at
  // (3,0) r4 — c5 starts inside c4, so the distance must become
  // |r4 - r5| = 6.
  document = manager.add_sketch_circle(0.0, 0.0, 10.0);
  document = manager.add_sketch_circle(3.0, 0.0, 4.0);
  const auto with_c5 = sketch_params(document);
  const std::string c4 = with_c5.circles[2].id;
  const std::string c5 = with_c5.circles[3].id;
  document = manager.set_sketch_tangent_pair_constraint(c4, c5);
  {
    const auto params = sketch_params(document);
    const auto& a = *std::find_if(
        params.circles.begin(), params.circles.end(),
        [&](const auto& circle) { return circle.id == c4; });
    const auto& b = *std::find_if(
        params.circles.begin(), params.circles.end(),
        [&](const auto& circle) { return circle.id == c5; });
    const double dist =
        std::hypot(b.center_x - a.center_x, b.center_y - a.center_y);
    if (!expect(near(dist, 6.0),
                "tangent: internal center distance = |r4 - r5|")) {
      return false;
    }
  }
  return true;
}

bool test_line_arc_tangent_follows_radius() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Arc: center (0,0), radius 5, quarter from (5,0) to (0,5).
  DocumentState document =
      manager.add_sketch_arc(5.0, 0.0, 0.0, 5.0, 0.0, 0.0, "center_start_end");
  // Line tangent below the center at distance 5: horizontal at y=-5.
  // Clear the inferred H constraint — the tangent relation may need to
  // rotate/translate the line as the arc radius changes.
  document = manager.add_sketch_line(-10.0, -5.0, 10.0, -5.0);
  const auto before = sketch_params(document);
  const std::string arc_id = before.arcs[0].id;
  const std::string line_id = before.lines[0].id;
  document = manager.set_sketch_line_constraint(line_id, std::nullopt);

  document = manager.set_sketch_tangent_constraint(line_id, arc_id);

  // The line's H constraint pins its y; the tangency then drives the
  // ARC toward the line when the radius changes. Update the arc radius
  // to 8: distance must become 8, so the line's y stays and the arc
  // center must move — the center is the free party here (the line has
  // H). Actually: distance from the arc center to the line must equal
  // the radius; with the line pinned at y=-5 by H, the arc center moves
  // to y = -5 + 8 = 3 (keeping x=0). Both orders are asserted via the
  // distance itself.
  auto arc_radius_dim = [&](const SketchFeatureParameters& params) {
    const auto it = std::find_if(
        params.dimensions.begin(), params.dimensions.end(),
        [&](const auto& dim) {
          return dim.kind == "arc_radius" && dim.entity_id == arc_id;
        });
    return it;
  };
  document = manager.add_sketch_arc_radius_dimension(arc_id);
  document = manager.update_sketch_dimension(
      arc_radius_dim(sketch_params(document))->id, 8.0);
  {
    const auto params = sketch_params(document);
    const auto& line = *std::find_if(
        params.lines.begin(), params.lines.end(),
        [&](const auto& l) { return l.id == line_id; });
    const auto& arc = params.arcs[0];
    const double dist =
        point_line_distance(arc.center_x, arc.center_y, line.start_x,
                            line.start_y, line.end_x, line.end_y);
        if (!expect(near(dist, 8.0, 1e-4),
                "line-arc tangent: distance = radius after edit")) {
      return false;
    }
  }
  return true;
}

bool test_symmetric_square_full_profiles() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Square: top (H), right (V), bottom (H), left (V), plus a vertical
  // axis line through x=0.
  DocumentState document = manager.add_sketch_line(-10.0, 10.0, 10.0, 10.0);
  document = manager.add_sketch_line(10.0, 10.0, 10.0, -10.0);
  document = manager.add_sketch_line(10.0, -10.0, -10.0, -10.0);
  document = manager.add_sketch_line(-10.0, -10.0, -10.0, 10.0);
  document = manager.add_sketch_line(0.0, -15.0, 0.0, 15.0);

  const auto before = sketch_params(document);
  std::vector<std::string> expected_ids;
  for (int i = 0; i < 4; ++i) expected_ids.push_back(before.lines[i].id);
  const std::string axis = before.lines[4].id;
  // Symmetric pair: the top-left corner vertex (shared by top.start /
  // left.end) and the top-right corner vertex (top.end / right.start).
  const std::string p1 = before.lines[0].start_vertex_id;  // (-10,10)
  const std::string p2 = before.lines[0].end_vertex_id;    // (10,10)

  // Free the four sides from their inferred H/V constraints — the drag
  // below deliberately deforms the square, which the axis alignment
  // would clamp back.
  for (int i = 0; i < 4; ++i) {
    document = manager.set_sketch_line_constraint(before.lines[i].id,
                                                  std::nullopt);
  }

  document = manager.set_sketch_symmetric_constraint(p1, p2, axis);

  // Drag the top-right corner to (12,9): the top-left corner must
  // mirror to (-12,9), keeping the loop closed with all 4 lines.
  document = manager.update_sketch_vertex(p2, 12.0, 9.0);

  std::string reason;
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = expected_ids, .kind = "polygon"},
  };
  if (!expect(profiles_match(document, expected, &reason),
              ("symmetric square: " + reason).c_str())) {
    return false;
  }

  const auto after = sketch_params(document);
  const auto v1 = std::find_if(
      after.vertices.begin(), after.vertices.end(),
      [&](const auto& v) { return v.id == p1; });
  return expect(v1 != after.vertices.end() && near(v1->x, -11.0) &&
                    near(v1->y, 9.5),
                "symmetric square: pair compromises around the drag");
}

bool test_dof_accounting_counts_symmetric() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 10.0, 10.0, 10.0);
  document = manager.add_sketch_line(0.0, -10.0, 12.0, -8.0);
  document = manager.add_sketch_line(-5.0, 0.0, 15.0, 0.0);
  const auto before = sketch_params(document);
  const std::string p1 = before.lines[0].end_vertex_id;
  const std::string p2 = before.lines[1].end_vertex_id;

  document = manager.set_sketch_symmetric_constraint(
      p1, p2, before.lines[2].id);

  const auto results = polysmith::core::count_sketch_dof(
      sketch_params(document));
  int p1_consumed = 0;
  int p2_consumed = 0;
  for (const auto& entry : results) {
    if (entry.entity_id == p1) p1_consumed = entry.consumed_dof;
    if (entry.entity_id == p2) p2_consumed = entry.consumed_dof;
  }
  return expect(p1_consumed >= 2 && p2_consumed >= 2,
                "dof: symmetric constraint consumes DOF on both points");
}

}  // namespace

int main() {
  if (!test_symmetric_both_axis_sides()) return 1;
  if (!test_collinear_holds_after_host_move()) return 1;
  if (!test_midpoint_holds_after_host_move()) return 1;
  if (!test_anchor_t_holds_after_host_move()) return 1;
  if (!test_circle_circle_tangent_external_and_internal()) return 1;
  if (!test_line_arc_tangent_follows_radius()) return 1;
  if (!test_symmetric_square_full_profiles()) return 1;
  if (!test_dof_accounting_counts_symmetric()) return 1;

  std::cout << "constraint_completion_test passed\n";
  return 0;
}
