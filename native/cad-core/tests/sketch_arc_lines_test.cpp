// Regression suite for arc ↔ line shared-endpoint behaviour
// (wiki/Sketch-Arc-Lines-Bugs.md, consolidated 2026-11-02).
//
// Pins the fixes that survived review:
//  1. A real arc + two lines welded to its ends: a centre drag moves the
//     whole junction rigidly (radius preserved, endpoints stay welded).
//  2. A TRIMMED arc's endpoints are frozen by the trim to protect the loop;
//     a deliberate centre drag must still move them (and the lines welded
//     to them), or the radius drifts off them and the lines detach.
//  3. Legacy documents that already carry a redundant point_on_arc on the
//     arc's own endpoints must not double-translate those points.
//  4. Trimming a circle with TANGENT lines that overshoot the tangent point
//     must truncate, not split, so no stub is left behind.
//  5. Arc endpoints start UNFIXED (is_fixed is not what distinguishes a
//     trimmed arc from a real one).

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "protocol/serialization.h"
#include "sketch_test_utils.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::SketchArc;
using polysmith::core::SketchFeatureParameters;
using polysmith::core::SketchLine;
using polysmith::core::SketchVertex;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

bool near(double a, double b, double tolerance = 1.0e-6) {
  return std::abs(a - b) < tolerance;
}

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

// How far either cached endpoint sits off the arc's own radius. Zero means
// the drawn arc (centre + radius) reaches both endpoints — the invariant a
// rigid drag must preserve.
double radial_gap(const SketchArc& arc) {
  const double rs =
      std::hypot(arc.start_x - arc.center_x, arc.start_y - arc.center_y);
  const double re =
      std::hypot(arc.end_x - arc.center_x, arc.end_y - arc.center_y);
  return std::max(std::abs(rs - arc.radius), std::abs(re - arc.radius));
}

// Largest distance between the arc's cached endpoints and the nearest line
// endpoint that shares their vertex id (should be 0 when welded).
double weld_gap(const SketchFeatureParameters& params, const SketchArc& arc) {
  double gap = 0.0;
  for (const auto& line : params.lines) {
    if (line.start_vertex_id == arc.start_vertex_id) {
      gap = std::max(gap, std::hypot(line.start_x - arc.start_x,
                                     line.start_y - arc.start_y));
    }
    if (line.end_vertex_id == arc.start_vertex_id) {
      gap = std::max(gap, std::hypot(line.end_x - arc.start_x,
                                     line.end_y - arc.start_y));
    }
    if (line.start_vertex_id == arc.end_vertex_id) {
      gap = std::max(gap, std::hypot(line.start_x - arc.end_x,
                                     line.start_y - arc.end_y));
    }
    if (line.end_vertex_id == arc.end_vertex_id) {
      gap = std::max(gap, std::hypot(line.end_x - arc.end_x,
                                     line.end_y - arc.end_y));
    }
  }
  return gap;
}

// ── Test 1: real arc + two welded lines stay rigid on a centre drag ──
bool test_real_arc_centre_drag_stays_rigid() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_line(20.0, 10.0, 10.0, 0.0);
  manager.add_sketch_line(10.0, 20.0, 0.0, 10.0);
  DocumentState document = manager.add_sketch_arc(10.0, 0.0, 0.0, 10.0, 0.0,
                                                  0.0, "center_start_end");
  const auto before = sketch_params(document);
  if (!expect(before.arcs.size() == 1, "real: arc created")) return false;
  if (!expect(weld_gap(before, before.arcs[0]) < 1e-6,
              "real: arc endpoints welded to the lines at creation")) {
    return false;
  }
  const SketchArc* arc0 = &before.arcs[0];
  const std::string centre = arc0->center_vertex_id;
  const double gap0 = radial_gap(*arc0);
  const double r0 = arc0->radius;

  document = manager.update_sketch_vertex(centre, 5.0, 5.0);
  const auto after = sketch_params(document);
  const SketchArc* arc1 = find_arc(after, arc0->id);
  if (!expect(arc1 != nullptr, "real: arc survives the drag")) return false;
  bool ok = true;
  ok &= expect(near(arc1->center_x, 5.0) && near(arc1->center_y, 5.0),
               "real: centre lands at the dragged position");
  ok &= expect(near(arc1->radius, r0), "real: radius preserved");
  ok &= expect(near(radial_gap(*arc1), gap0, 1e-6),
               "real: endpoints keep their radius offset");
  ok &= expect(weld_gap(after, *arc1) < 1e-6,
               "real: endpoints stay welded to the lines");
  return ok;
}

// ── Test 2: trimmed arc — centre drag must move the frozen endpoints ─
bool test_trimmed_arc_centre_drag_stays_rigid() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 40.0);
  manager.add_sketch_line(-150.0, 0.0, 100.0, 40.0);
  manager.add_sketch_line(-150.0, 0.0, 100.0, -40.0);

  DocumentState document = manager.trim_sketch_entity("circle-1", -40.0, 0.0);
  const auto before = sketch_params(document);
  if (!expect(before.arcs.size() == 1, "trimmed: arc produced")) {
    std::cerr << "  arcs=" << before.arcs.size() << "\n";
    return false;
  }
  const SketchArc* arc0 = &before.arcs[0];
  const double gap0 = radial_gap(*arc0);
  const std::string centre = arc0->center_vertex_id;

  document = manager.update_sketch_vertex(centre, arc0->center_x + 10.0,
                                          arc0->center_y + 5.0);
  const auto after = sketch_params(document);
  const SketchArc* arc1 = find_arc(after, arc0->id);
  if (!expect(arc1 != nullptr, "trimmed: arc survives the drag")) return false;
  bool ok = true;
  ok &= expect(near(arc1->center_x, arc0->center_x + 10.0) &&
                   near(arc1->center_y, arc0->center_y + 5.0),
               "trimmed: centre lands at the dragged position");
  ok &= expect(near(radial_gap(*arc1), gap0, 1e-6),
               "trimmed: radius offset unchanged by the centre drag");
  ok &= expect(weld_gap(after, *arc1) < 1e-6,
               "trimmed: endpoints stay welded to the lines");
  return ok;
}

// ── Test 3: legacy doc carrying redundant point_on_arc constraints ───
bool test_legacy_redundant_constraints_centre_drag() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_arc(
      -50.566257733042065, 25.0, -50.566257733042065, -45.0, 42.8778, -5.0,
      "three_point");
  manager.add_sketch_line(-95.20379146919431, -1.7288539549504738,
                          -50.566257733042065, 25.0);
  document = manager.add_sketch_line(-95.20379146919431, -1.7288539549504738,
                                     -50.566257733042065, -45.0);

  // Inject the redundant constraints a legacy attach path created on the
  // arc's own endpoints.
  {
    auto& params = document.feature_history.back().sketch_parameters.value();
    if (!expect(params.arcs.size() == 1, "legacy: arc present")) return false;
    const SketchArc& arc = params.arcs[0];
    params.constraints.push_back(polysmith::core::SketchConstraint{
        .constraint_id = "constraint-point-on-arc-legacy-1",
        .kind = "point_on_arc",
        .target_ids = {arc.start_vertex_id, arc.id}});
    params.constraints.push_back(polysmith::core::SketchConstraint{
        .constraint_id = "constraint-point-on-arc-legacy-2",
        .kind = "point_on_arc",
        .target_ids = {arc.end_vertex_id, arc.id}});
  }

  const auto path = std::filesystem::temp_directory_path() /
                    "polysmith_arc_legacy_constraint_test.polysmith";
  {
    std::ofstream stream(path);
    stream << polysmith::protocol::to_payload(document).dump();
  }
  DocumentManager loaded;
  DocumentState restored = loaded.load_document_from_path(path.string());
  const auto& loaded_params =
      restored.feature_history.back().sketch_parameters.value();
  if (!expect(loaded_params.constraints.size() >= 2,
              "legacy: injected constraints survived the round trip")) {
    return false;
  }
  const std::string centre = loaded_params.arcs[0].center_vertex_id;
  const double gap0 = radial_gap(loaded_params.arcs[0]);

  bool ok = true;
  for (int i = 1; i <= 3; ++i) {
    restored = loaded.update_sketch_vertex(centre, 5.0 * i, 5.0 * i);
    const auto p = restored.feature_history.back().sketch_parameters.value();
    ok &= expect(near(radial_gap(p.arcs[0]), gap0, 1e-6),
                 "legacy: endpoints keep their radius offset after a drag");
  }
  return ok;
}

// ── Test 4: tangent lines that overshoot are truncated, not split ─────
// The user's exact initial geometry: circle (5,0) r=45.9046, two lines
// from (-100,0) ending on the circle at (-12.21, ±42.556), ~3 mm past the
// true tangent point. Trimming must not leave a stub.
bool test_tangent_overshoot_trim_does_not_split() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(5.0, 0.0, 45.904601744101846);
  manager.add_sketch_line(-100.0, 0.0, -12.210569599770722, 42.556183514690865);
  manager.add_sketch_line(-100.0, 0.0, -12.210569599770722, -42.556183514690865);

  DocumentState document = manager.trim_sketch_entity("circle-1", 50.0, 0.0);
  const auto params = sketch_params(document);
  bool ok = true;
  ok &= expect(params.arcs.size() == 1 && params.circles.empty(),
               "overshoot: circle converted to one arc");
  ok &= expect(params.lines.size() == 2,
               "overshoot: tangent lines truncated, not split into stubs");
  return ok;
}

// ── Test 5: arc endpoints start unfixed ───────────────────────────────
bool test_arc_endpoints_start_unfixed() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_arc(10.0, 0.0, 0.0, 10.0, 0.0,
                                                  0.0, "center_start_end");
  const auto params = sketch_params(document);
  const SketchArc& a = params.arcs[0];
  const SketchVertex* sv = find_vertex(params, a.start_vertex_id);
  const SketchVertex* ev = find_vertex(params, a.end_vertex_id);
  const SketchVertex* cv = find_vertex(params, a.center_vertex_id);
  return expect(sv != nullptr && !sv->is_fixed, "arc start is NOT fixed") &&
         expect(ev != nullptr && !ev->is_fixed, "arc end is NOT fixed") &&
         expect(cv != nullptr && !cv->is_fixed, "arc centre is NOT fixed");
}

}  // namespace

int main() {
  bool ok = true;
  ok = test_real_arc_centre_drag_stays_rigid() && ok;
  ok = test_trimmed_arc_centre_drag_stays_rigid() && ok;
  ok = test_legacy_redundant_constraints_centre_drag() && ok;
  ok = test_tangent_overshoot_trim_does_not_split() && ok;
  ok = test_arc_endpoints_start_unfixed() && ok;

  if (!ok) {
    std::cout << "sketch_arc_lines_test FAILED\n";
    return 1;
  }
  std::cout << "sketch_arc_lines_test passed\n";
  return 0;
}
