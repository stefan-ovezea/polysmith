// Regression tests for parametric arcs.
//
// Arc dimension drives (arc_radius / arc_angle) are enforced by the
// deterministic enforce_arc_dimensions pass rather than through planegcs:
// the solver wanders the null space of an unanchored arc (free
// translation + rotation + sweep) even from a zero-residual reference.
// The pass honors shared H/V-constrained lines (sliding the endpoint to
// the circle∩line intersection) and degrades the dimension to driven
// when a fixed vertex would have to move.
//
// Every profile-set assertion uses profiles_match (complete region-set
// matching, not presence-only) and every epsilon-sensitive case tests
// both sides of the target value.

#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "sketch_test_utils.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::SketchArc;
using polysmith::core::SketchFeatureParameters;
using polysmith::core::SketchVertex;
using polysmith::test::ExpectedProfile;
using polysmith::test::profiles_match;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

// Returns a VALUE COPY of the active sketch's parameters — callers
// reassign `document` after setup, so a reference would dangle.
SketchFeatureParameters sketch_params(const DocumentState& document) {
  return document.feature_history.back().sketch_parameters.value();
}

const SketchArc* find_arc(const SketchFeatureParameters& params,
                          const std::string& id) {
  for (const auto& arc : params.arcs) {
    if (arc.id == id) return &arc;
  }
  return nullptr;
}

const SketchVertex* find_vertex(const SketchFeatureParameters& params,
                                const std::string& id) {
  for (const auto& vertex : params.vertices) {
    if (vertex.id == id) return &vertex;
  }
  return nullptr;
}

bool near(double a, double b, double tolerance = 1.0e-6) {
  return std::abs(a - b) < tolerance;
}

double sweep_of(const SketchArc& arc) {
  double raw = std::atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x) -
               std::atan2(arc.start_y - arc.center_y, arc.start_x - arc.center_x);
  if (raw < 0.0) raw += 2.0 * 3.14159265358979323846;
  return raw;
}

// Three-point semicircle from (20,10) to (20,-10) through (30,0):
// center (20,0), radius 10, ccw=false.
DocumentState add_semicircle_arc(DocumentManager& manager) {
  return manager.add_sketch_arc(20.0, 10.0, 20.0, -10.0, 30.0, 0.0,
                                "three_point");
}

bool test_arc_radius_dim_drives_both_sides() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = add_semicircle_arc(manager);
  const auto before = sketch_params(document);
  if (!expect(before.arcs.size() == 1, "radius: arc created")) return false;
  const std::string arc_id = before.arcs[0].id;
  const bool original_ccw = before.arcs[0].ccw;

  // add_sketch_arc already emitted an auto arc_radius dimension;
  // adding the dimension again promotes it to a driving dimension.
  document = manager.add_sketch_arc_radius_dimension(arc_id);

  auto radius_dim = [&](const SketchFeatureParameters& params) {
    const auto it = std::find_if(
        params.dimensions.begin(), params.dimensions.end(),
        [&](const auto& dim) {
          return dim.kind == "arc_radius" && dim.entity_id == arc_id;
        });
    return it;
  };

  // Larger radius — both epsilon sides of the target.
  document = manager.update_sketch_dimension(
      radius_dim(sketch_params(document))->id, 12.0);
  {
    const auto params = sketch_params(document);
    const SketchArc* arc = find_arc(params, arc_id);
    if (!expect(arc != nullptr, "radius: arc present after update")) return false;
    if (!expect(!radius_dim(params)->driven, "radius: dim stays driving (up)")) {
      return false;
    }
    if (!expect(near(arc->radius, 12.0), "radius: solved to 12.0")) return false;
    if (!expect(arc->ccw == original_ccw, "radius: ccw preserved (up)")) {
      return false;
    }
    const double start_dist =
        std::hypot(arc->start_x - arc->center_x, arc->start_y - arc->center_y);
    const double end_dist =
        std::hypot(arc->end_x - arc->center_x, arc->end_y - arc->center_y);
    if (!expect(near(start_dist, 12.0) && near(end_dist, 12.0),
                "radius: endpoints on-circle within 1e-6 (up)")) {
      return false;
    }
  }

  // Smaller radius — the other epsilon side.
  document = manager.update_sketch_dimension(
      radius_dim(sketch_params(document))->id, 8.0);
  {
    const auto params = sketch_params(document);
    const SketchArc* arc = find_arc(params, arc_id);
    if (!expect(arc != nullptr, "radius: arc present after shrink")) return false;
    if (!expect(!radius_dim(params)->driven, "radius: dim stays driving (down)")) {
      return false;
    }
    if (!expect(near(arc->radius, 8.0), "radius: solved to 8.0")) return false;
    if (!expect(arc->ccw == original_ccw, "radius: ccw preserved (down)")) {
      return false;
    }
    const double start_dist =
        std::hypot(arc->start_x - arc->center_x, arc->start_y - arc->center_y);
    const double end_dist =
        std::hypot(arc->end_x - arc->center_x, arc->end_y - arc->center_y);
    if (!expect(near(start_dist, 8.0) && near(end_dist, 8.0),
                "radius: endpoints on-circle within 1e-6 (down)")) {
      return false;
    }
  }
  return true;
}

bool test_arc_angle_dim_drives_sweep_both_sides() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Quarter arc: center (0,0), start (10,0), end (0,10) — ccw, sweep 90°.
  DocumentState document =
      manager.add_sketch_arc(10.0, 0.0, 0.0, 10.0, 0.0, 0.0, "center_start_end");
  const auto before = sketch_params(document);
  if (!expect(before.arcs.size() == 1, "angle: arc created")) return false;
  const std::string arc_id = before.arcs[0].id;

  document = manager.add_sketch_arc_angle_dimension(arc_id);

  auto angle_dim = [&](const SketchFeatureParameters& params) {
    const auto it = std::find_if(
        params.dimensions.begin(), params.dimensions.end(),
        [&](const auto& dim) {
          return dim.kind == "arc_angle" && dim.entity_id == arc_id;
        });
    return it;
  };

  const double pi = 3.14159265358979323846;
  const double initial = angle_dim(sketch_params(document))->value;
  if (!expect(near(initial, pi / 2.0, 1e-9), "angle: initial sweep is 90°")) {
    return false;
  }

  // Widen to 135°.
  document = manager.update_sketch_dimension(angle_dim(sketch_params(document))->id,
                                             3.0 * pi / 4.0);
  {
    const auto params = sketch_params(document);
    const SketchArc* arc = find_arc(params, arc_id);
    if (!expect(!angle_dim(params)->driven, "angle: dim stays driving (wide)")) {
      return false;
    }
    if (!expect(near(sweep_of(*arc), 3.0 * pi / 4.0, 1e-6),
                "angle: sweep solved to 135°")) {
      return false;
    }
    if (!expect(near(arc->radius, 10.0), "angle: radius preserved (wide)")) {
      return false;
    }
    if (!expect(arc->ccw, "angle: ccw preserved (wide)")) return false;
    if (!expect(near(arc->center_x, 0.0) && near(arc->center_y, 0.0),
                "angle: center stays put (wide)")) {
      return false;
    }
  }

  // Narrow to 45° — the other direction.
  document = manager.update_sketch_dimension(angle_dim(sketch_params(document))->id,
                                             pi / 4.0);
  {
    const auto params = sketch_params(document);
    const SketchArc* arc = find_arc(params, arc_id);
    if (!expect(!angle_dim(params)->driven, "angle: dim stays driving (narrow)")) {
      return false;
    }
    if (!expect(near(sweep_of(*arc), pi / 4.0, 1e-6),
                "angle: sweep solved to 45°")) {
      return false;
    }
    if (!expect(near(arc->radius, 10.0), "angle: radius preserved (narrow)")) {
      return false;
    }
    if (!expect(arc->ccw, "angle: ccw preserved (narrow)")) return false;
  }
  return true;
}

bool test_endpoint_drag_pivots_arc_with_radius_dim() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Line from (0,10) to (20,10); arc starts at (20,10) (shared vertex via
  // coordinate match) and swings to (20,-10) around center (20,0).
  manager.add_sketch_line(0.0, 10.0, 20.0, 10.0);
  DocumentState document = add_semicircle_arc(manager);
  const auto before = sketch_params(document);
  const std::string arc_id = before.arcs[0].id;
  const std::string shared_vertex_id = before.arcs[0].start_vertex_id;

  document = manager.add_sketch_arc_radius_dimension(arc_id);

  // add_sketch_line infers an H constraint for the horizontal line, so
  // the dragged shared endpoint first clamps to y=10 (the propagate
  // clamp).  With the radius dimension driving, the endpoint must ALSO
  // lie on the r=10 circle about (20,0) — the enforcement pass slides it
  // along the constraint axis to the circle∩line intersection: the only
  // circle point at y=10 is (20,10).  Both constraints are honored.
  document = manager.update_sketch_vertex(shared_vertex_id, 25.0, 5.0);

  {
    const auto params = sketch_params(document);
    const SketchArc* arc = find_arc(params, arc_id);
    if (!expect(arc != nullptr, "drag: arc present (H case)")) return false;
    if (!expect(near(arc->radius, 10.0), "drag: radius stays 10.0 (H case)")) {
      return false;
    }
    if (!expect(near(arc->start_x, 20.0) && near(arc->start_y, 10.0),
                "drag: endpoint snaps to the circle∩H-line intersection")) {
      return false;
    }
    const double start_dist =
        std::hypot(arc->start_x - arc->center_x, arc->start_y - arc->center_y);
    if (!expect(near(start_dist, 10.0),
                "drag: endpoint on-circle within 1e-6 (H case)")) {
      return false;
    }
    const auto dim = std::find_if(
        params.dimensions.begin(), params.dimensions.end(),
        [&](const auto& d) {
          return d.kind == "arc_radius" && d.entity_id == arc_id;
        });
    if (!expect(dim != params.dimensions.end() && !dim->driven,
                "drag: radius dim stays driving (H case)")) {
      return false;
    }
  }

  // Clear the line's H constraint (std::nullopt — the IPC layer maps
  // "none" to this): now a tangential drag of the shared endpoint
  // rotates the arc about its center — the endpoint slides to the
  // circle point at angle atan2(5,5) = 45°, i.e.
  // (20 + 10·cos45°, 10·sin45°).  The shared line follows.
  const auto before_clear = sketch_params(document);
  const std::string line_id = before_clear.lines[0].id;
  document = manager.set_sketch_line_constraint(line_id, std::nullopt);
  document = manager.update_sketch_vertex(shared_vertex_id, 25.0, 5.0);

  {
    const auto after = sketch_params(document);
    const SketchArc* arc = find_arc(after, arc_id);
    if (!expect(arc != nullptr, "drag: arc present (free case)")) return false;
    if (!expect(near(arc->radius, 10.0), "drag: radius stays 10.0 (free case)")) {
      return false;
    }
    const double expected_angle = 3.14159265358979323846 / 4.0;
    const double expected_x = 20.0 + 10.0 * std::cos(expected_angle);
    const double expected_y = 10.0 * std::sin(expected_angle);
    if (!expect(near(arc->start_x, expected_x) && near(arc->start_y, expected_y),
                "drag: endpoint slides onto the circle at the dragged angle")) {
      return false;
    }
    const double start_dist =
        std::hypot(arc->start_x - arc->center_x, arc->start_y - arc->center_y);
    const double end_dist =
        std::hypot(arc->end_x - arc->center_x, arc->end_y - arc->center_y);
    if (!expect(near(start_dist, 10.0) && near(end_dist, 10.0),
                "drag: endpoints on-circle within 1e-6 (free case)")) {
      return false;
    }
    // The line sharing the dragged vertex follows the arc endpoint.
    for (const auto& line : after.lines) {
      if (line.end_vertex_id == shared_vertex_id) {
        return expect(
            near(line.end_x, expected_x) && near(line.end_y, expected_y),
            "drag: shared line endpoint follows the arc");
      }
    }
    return expect(false, "drag: shared line not found");
  }
}

bool test_overconstrained_arc_dims_mark_driven() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = add_semicircle_arc(manager);
  const auto before = sketch_params(document);
  const std::string arc_id = before.arcs[0].id;
  const std::string start_vid = before.arcs[0].start_vertex_id;
  const std::string end_vid = before.arcs[0].end_vertex_id;
  const std::string center_vid = before.arcs[0].center_vertex_id;

  // Pin the whole arc: start, end, and center are fixed.
  document = manager.set_sketch_vertex_fixed(start_vid, true);
  document = manager.set_sketch_vertex_fixed(end_vid, true);
  document = manager.set_sketch_vertex_fixed(center_vid, true);

  // With the endpoints fixed, a driving dimension cannot move the arc
  // without violating the fix — the enforcement pass must degrade such
  // dimensions to driven (reference-only) instead of moving fixed
  // vertices.
  document = manager.add_sketch_arc_radius_dimension(arc_id);
  document = manager.add_sketch_arc_angle_dimension(arc_id);

  const auto after = sketch_params(document);
  const auto radius_it = std::find_if(
      after.dimensions.begin(), after.dimensions.end(),
      [&](const auto& dim) {
        return dim.kind == "arc_radius" && dim.entity_id == arc_id;
      });
  const auto angle_it = std::find_if(
      after.dimensions.begin(), after.dimensions.end(),
      [&](const auto& dim) {
        return dim.kind == "arc_angle" && dim.entity_id == arc_id;
      });
  if (!expect(radius_it != after.dimensions.end() && angle_it != after.dimensions.end(),
              "overconstraint: both dims exist")) {
    return false;
  }
  if (!expect(angle_it->driven,
              "overconstraint: angle dim marked driven")) {
    return false;
  }
  const SketchArc* arc = find_arc(after, arc_id);
  if (!expect(near(arc->radius, 10.0), "overconstraint: radius unchanged")) {
    return false;
  }
  const SketchVertex* start_v = find_vertex(after, start_vid);
  if (!expect(start_v != nullptr && start_v->is_fixed,
              "overconstraint: start vertex still fixed")) {
    return false;
  }
  return expect(near(start_v->x, 20.0, 1e-5) && near(start_v->y, 10.0, 1e-5),
                "overconstraint: fixed endpoint position unchanged");
}

bool test_stadium_profile_with_arc_radius_dim() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Stadium half: top edge (-20,10)->(20,10), bottom edge (-20,-10)->(20,-10),
  // left edge closes, arc semicircle closes the right end.
  DocumentState document = manager.add_sketch_line(-20.0, 10.0, 20.0, 10.0);
  document = manager.add_sketch_line(-20.0, -10.0, 20.0, -10.0);
  document = manager.add_sketch_line(-20.0, -10.0, -20.0, 10.0);
  document = add_semicircle_arc(manager);

  const auto before = sketch_params(document);
  if (!expect(before.lines.size() == 3 && before.arcs.size() == 1,
              "stadium: 3 lines + 1 arc")) {
    return false;
  }
  std::vector<std::string> expected_ids;
  for (const auto& line : before.lines) expected_ids.push_back(line.id);
  const std::string arc_id = before.arcs[0].id;
  expected_ids.push_back(arc_id);

  // Drive the arc radius — the loop must stay closed.
  document = manager.add_sketch_arc_radius_dimension(arc_id);
  auto radius_dim_id = [&](const SketchFeatureParameters& params) {
    const auto it = std::find_if(
        params.dimensions.begin(), params.dimensions.end(),
        [&](const auto& dim) {
          return dim.kind == "arc_radius" && dim.entity_id == arc_id;
        });
    return it->id;
  };
  document = manager.update_sketch_dimension(
      radius_dim_id(sketch_params(document)), 12.0);

  std::string reason;
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = expected_ids, .kind = "polygon"},
  };
  if (!expect(profiles_match(document, expected, &reason),
              ("stadium: " + reason).c_str())) {
    return false;
  }

  const auto after = sketch_params(document);
  const SketchArc* arc = find_arc(after, arc_id);
  return expect(near(arc->radius, 12.0),
                "stadium: arc radius solved to 12.0 inside the loop");
}

bool test_fillet_arc_excluded_from_solver() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 100.0, 80.0);
  const auto before = sketch_params(document);
  if (!expect(before.lines.size() == 4, "fillet: rectangle created")) {
    return false;
  }

  // Corner shared by lines[0] and lines[1].
  std::string corner_vertex_id;
  {
    const auto& a = before.lines[0];
    const auto& b = before.lines[1];
    for (const auto& va : {a.start_vertex_id, a.end_vertex_id}) {
      if (va == b.start_vertex_id || va == b.end_vertex_id) {
        corner_vertex_id = va;
      }
    }
  }
  if (!expect(!corner_vertex_id.empty(), "fillet: shared corner found")) {
    return false;
  }

  document = manager.add_sketch_fillet(corner_vertex_id, before.lines[0].id,
                                       before.lines[1].id, 10.0);
  const auto filleted = sketch_params(document);
  if (!expect(filleted.fillets.size() == 1 && filleted.arcs.size() == 1,
              "fillet: fillet + derived arc created")) {
    return false;
  }
  const std::string fillet_arc_id = filleted.fillets[0].arc_id;

  std::vector<std::string> expected_ids;
  for (const auto& line : filleted.lines) expected_ids.push_back(line.id);
  expected_ids.push_back(fillet_arc_id);

  // Force the solver to run while the derived fillet arc is present:
  // a driving dimension elsewhere in the sketch.  The fillet arc is not
  // registered in the solver (its endpoints are re-derived by the fillet
  // enforcement pass), so the solve must neither conflict nor disturb
  // the fillet.
  document = manager.add_sketch_line_length_dimension(filleted.lines[0].id);
  auto length_dim_id = [&](const SketchFeatureParameters& params) {
    const auto it = std::find_if(
        params.dimensions.begin(), params.dimensions.end(),
        [&](const auto& dim) {
          return dim.kind == "line_length" && dim.entity_id == filleted.lines[0].id;
        });
    return it->id;
  };
  document = manager.update_sketch_dimension(
      length_dim_id(sketch_params(document)), 90.0);

  std::string reason;
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = expected_ids, .kind = "polygon"},
  };
  if (!expect(profiles_match(document, expected, &reason),
              ("fillet: " + reason).c_str())) {
    return false;
  }

  const auto after = sketch_params(document);
  const SketchArc* fillet_arc = find_arc(after, fillet_arc_id);
  return expect(fillet_arc != nullptr && near(fillet_arc->radius, 10.0),
                "fillet: fillet arc radius preserved through the solve");
}

}  // namespace

int main() {
  if (!test_arc_radius_dim_drives_both_sides()) return 1;
  if (!test_arc_angle_dim_drives_sweep_both_sides()) return 1;
  if (!test_endpoint_drag_pivots_arc_with_radius_dim()) return 1;
  if (!test_overconstrained_arc_dims_mark_driven()) return 1;
  if (!test_stadium_profile_with_arc_radius_dim()) return 1;
  if (!test_fillet_arc_excluded_from_solver()) return 1;

  std::cout << "parametric_arc_test passed\n";
  return 0;
}
