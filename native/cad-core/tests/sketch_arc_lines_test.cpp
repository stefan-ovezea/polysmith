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

// First object in the payload that names an arc. The document layout is not
// this test's subject, so search rather than hard-code the path.
const nlohmann::json* find_arc_payload(const nlohmann::json& node) {
  if (node.is_object()) {
    if (node.contains("arc_id")) return &node;
    for (auto it = node.begin(); it != node.end(); ++it) {
      if (const auto* found = find_arc_payload(it.value())) return found;
    }
  } else if (node.is_array()) {
    for (const auto& item : node) {
      if (const auto* found = find_arc_payload(item)) return found;
    }
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
  const double span0 = arc_sweep_angle(*arc0);
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
  ok &= expect(near(arc_sweep_angle(*arc1), span0, 1e-9),
               "trimmed: the arc's span survives the centre drag");
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

// ── Test 6: the arc's span is part of the payload ─────────────────────
// Two endpoints admit two arcs. A renderer that re-derives which one from
// `ccw` and the endpoint angles guesses, and the guess flips when the
// endpoints' order swaps — the trimmed-arc "complementary arc" report. The
// payload must therefore carry the intended span outright.
bool test_arc_span_is_serialized() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 40.0);
  manager.add_sketch_line(-150.0, 0.0, 100.0, 40.0);
  manager.add_sketch_line(-150.0, 0.0, 100.0, -40.0);
  DocumentState document = manager.trim_sketch_entity("circle-1", -40.0, 0.0);
  const auto params = sketch_params(document);
  if (!expect(params.arcs.size() == 1, "span: trimmed arc produced")) {
    return false;
  }
  const SketchArc& arc = params.arcs[0];

  const auto payload = polysmith::protocol::to_payload(document);
  const auto* arc_payload = find_arc_payload(payload);
  if (!expect(arc_payload != nullptr, "span: an arc entry exists")) {
    return false;
  }
  if (!expect(arc_payload->contains("start_angle") &&
                  arc_payload->contains("sweep_angle"),
              "span: the arc entry carries start_angle + sweep_angle")) {
    return false;
  }

  const double start_angle = (*arc_payload)["start_angle"].get<double>();
  const double sweep_angle = (*arc_payload)["sweep_angle"].get<double>();
  bool ok = true;
  ok &= expect(near(start_angle, arc_start_angle(arc), 1e-9),
               "span: start_angle is the start endpoint's angle");
  ok &= expect(near(sweep_angle, arc_sweep_angle(arc), 1e-9),
               "span: sweep_angle is the arc's signed span");
  // The whole point of the field: this survivor is a MAJOR arc, so its span
  // must exceed π. Endpoint-derived guessing would pick the minor complement.
  ok &= expect(std::abs(sweep_angle) > 3.14159265358979323846,
               "span: the major arc's span exceeds pi, not its complement");
  // start + sweep must land on the end angle (mod 2π) — the identity that
  // makes the pair sufficient.
  const double two_pi = 2.0 * 3.14159265358979323846;
  const double end_angle =
      std::atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x);
  double landed = start_angle + sweep_angle;
  while (landed - end_angle > two_pi / 2) landed -= two_pi;
  while (end_angle - landed > two_pi / 2) landed += two_pi;
  ok &= expect(std::abs(landed - end_angle) < 1e-9,
               "span: start_angle + sweep_angle lands on the end angle");
  return ok;
}

// ── Test 7: enlarging a trimmed arc's radius drags its welded lines ───
// User report: typing a bigger radius enlarged the arc but left the lines
// behind ("it enlarges but disconnects the line", the circle-resize class).
// Cause: the trim freezes the arc's endpoints, and the move propagation
// refuses to move a fixed vertex, so the radius changed while the vertex
// table — and the lines welded to it — stayed put.
bool test_arc_radius_dimension_drag_keeps_lines_welded() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 40.0);
  manager.add_sketch_line(-150.0, 0.0, 100.0, 40.0);
  manager.add_sketch_line(-150.0, 0.0, 100.0, -40.0);

  DocumentState document = manager.trim_sketch_entity("circle-1", -40.0, 0.0);
  auto params = sketch_params(document);
  if (!expect(params.arcs.size() == 1, "arc radius: trimmed arc produced")) {
    return false;
  }
  const std::string arc_id = params.arcs[0].id;
  const double r0 = params.arcs[0].radius;
  // PREMISE: the reproduction depends on the trim freezing the arc's
  // endpoints — that freeze is what made the move propagation drop the radius
  // change instead of carrying it to the welded lines. If this ever stops
  // being true the scenario no longer reproduces the reported bug.
  const SketchVertex* sv = find_vertex(params, params.arcs[0].start_vertex_id);
  const SketchVertex* ev = find_vertex(params, params.arcs[0].end_vertex_id);
  bool premise = expect(sv != nullptr && ev != nullptr,
                        "arc radius: endpoints exist") &&
                 expect(sv->is_fixed && ev->is_fixed,
                        "arc radius: PREMISE — the trim froze the endpoints");

  document = manager.add_sketch_arc_radius_dimension(arc_id);
  document = manager.update_sketch_dimension("dim-arc-" + arc_id, r0 * 2.0);

  const auto after = sketch_params(document);
  const SketchArc* arc = find_arc(after, arc_id);
  if (!expect(arc != nullptr, "arc radius: arc survives the edit")) {
    return false;
  }
  bool ok = true;
  ok &= expect(near(arc->radius, r0 * 2.0, 1e-6),
               "arc radius: the new radius is applied");
  ok &= expect(radial_gap(*arc) < 1e-6,
               "arc radius: the drawn arc still reaches its own endpoints");
  ok &= expect(weld_gap(after, *arc) < 1e-6,
               "arc radius: the welded lines follow the enlarged endpoints");
  // The trim's internal freeze used to demote the dimension to reference-only
  // on creation, and a driven dimension silently ignores every later edit —
  // so this is the difference between "it works" and "it never moves again".
  const auto dimension_it = std::find_if(
      after.dimensions.begin(), after.dimensions.end(),
      [&](const polysmith::core::SketchDimension& dimension) {
        return dimension.id == "dim-arc-" + arc_id;
      });
  ok &= expect(dimension_it != after.dimensions.end() && !dimension_it->driven,
               "arc radius: the dimension stays driving (not demoted)");
  return ok && premise;
}

// ── Test 8: a stale radius is rebuilt from the welded endpoints ───────
// The reported document carried radius 80 with its endpoints 103.65 and
// 219.79 from the centre — a 307 mm chord, so no r=80 circle spans them and
// the renderer drew the arc detached from both lines. The endpoints are the
// real, shared geometry; the radius is the cache that went stale.
bool test_arc_radius_reconciles_from_endpoints() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 40.0);
  manager.add_sketch_line(-150.0, 0.0, 100.0, 40.0);
  manager.add_sketch_line(-150.0, 0.0, 100.0, -40.0);
  DocumentState document = manager.trim_sketch_entity("circle-1", -40.0, 0.0);
  const auto params = sketch_params(document);
  if (!expect(params.arcs.size() == 1, "reconcile: arc produced")) return false;
  const SketchArc before = params.arcs[0];
  const double span = std::abs(arc_sweep_angle(before));
  const double chord = std::hypot(before.end_x - before.start_x,
                                  before.end_y - before.start_y);
  const double expected_radius = chord / (2.0 * std::sin(span * 0.5));

  // Corrupt the radius the way the reported document had it: a value no circle
  // through both welded endpoints could have.
  document.feature_history.back().sketch_parameters->arcs[0].radius = 5.0;

  // Any refresh reconciles it.
  document = manager.set_sketch_vertex_fixed(before.center_vertex_id, false);
  const auto after = sketch_params(document);
  const SketchArc* arc = find_arc(after, before.id);
  if (!expect(arc != nullptr, "reconcile: arc survives the refresh")) {
    return false;
  }

  bool ok = true;
  ok &= expect(radial_gap(*arc) < 1e-6,
               "reconcile: the drawn arc passes through both endpoints");
  ok &= expect(near(arc->radius, expected_radius, 1e-6),
               "reconcile: the radius is rebuilt from the chord and span");
  ok &= expect(near(arc->start_x, before.start_x, 1e-9) &&
                   near(arc->end_x, before.end_x, 1e-9),
               "reconcile: the welded endpoints are not moved");
  ok &= expect(weld_gap(after, *arc) < 1e-6,
               "reconcile: the lines stay welded to the arc");
  return ok;
}

// ── Test 9: a curve host is not a line ────────────────────────────────
// Drawing a line whose endpoint snapped onto an ARC's perimeter sent the arc
// id as the line-anchor host, and require_user_line threw
// "Sketch line not found: arc-1", failing the draw command even though the
// point_on_arc attachment had already been created. A curve attaches through
// its own constraint, so the anchor commands must skip it silently.
bool test_curve_host_anchor_does_not_throw() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 40.0);
  manager.add_sketch_line(-150.0, 0.0, 100.0, 40.0);
  manager.add_sketch_line(-150.0, 0.0, 100.0, -40.0);
  DocumentState document = manager.trim_sketch_entity("circle-1", -40.0, 0.0);
  const auto params = sketch_params(document);
  if (!expect(params.arcs.size() == 1, "curve host: arc produced")) return false;
  const std::string arc_id = params.arcs[0].id;
  const std::string point_id = params.arcs[0].start_vertex_id;

  bool ok = true;
  try {
    document = manager.set_sketch_midpoint_anchor(point_id, arc_id);
  } catch (const std::exception& error) {
    std::cerr << "  midpoint anchor on a curve threw: " << error.what() << "\n";
    ok = false;
  }
  try {
    document = manager.set_sketch_vertex_line_anchor(point_id, arc_id, 0.5);
  } catch (const std::exception& error) {
    std::cerr << "  line anchor on a curve threw: " << error.what() << "\n";
    ok = false;
  }

  const auto after = sketch_params(document);
  ok &= expect(after.point_line_anchors.empty() && after.midpoint_anchors.empty(),
               "curve host: no line anchor is recorded for a curve");
  return ok;
}

// ── Test 10: dragging the arc's perimeter changes only the radius ─────
// The circle gesture applied to an arc: the centre is locked, both endpoints
// slide along the rays they already sit on, and the welded lines follow.
bool test_arc_perimeter_resize_keeps_centre() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 40.0);
  manager.add_sketch_line(-150.0, 0.0, 100.0, 40.0);
  manager.add_sketch_line(-150.0, 0.0, 100.0, -40.0);
  DocumentState document = manager.trim_sketch_entity("circle-1", -40.0, 0.0);
  const auto params = sketch_params(document);
  if (!expect(params.arcs.size() == 1, "resize: arc produced")) return false;
  const SketchArc before = params.arcs[0];
  const double start_angle = std::atan2(before.start_y - before.center_y,
                                        before.start_x - before.center_x);
  const double end_angle = std::atan2(before.end_y - before.center_y,
                                      before.end_x - before.center_x);
  const double new_radius = before.radius * 1.5;

  document = manager.update_sketch_arc_radius(before.id, new_radius);
  const auto after = sketch_params(document);
  const SketchArc* arc = find_arc(after, before.id);
  if (!expect(arc != nullptr, "resize: arc survives the resize")) return false;

  bool ok = true;
  ok &= expect(near(arc->center_x, before.center_x, 1e-9) &&
                   near(arc->center_y, before.center_y, 1e-9),
               "resize: the centre does not move");
  ok &= expect(near(arc->radius, new_radius, 1e-9),
               "resize: the radius is applied");
  ok &= expect(near(std::atan2(arc->start_y - arc->center_y,
                              arc->start_x - arc->center_x),
                    start_angle, 1e-9) &&
                   near(std::atan2(arc->end_y - arc->center_y,
                                   arc->end_x - arc->center_x),
                        end_angle, 1e-9),
               "resize: both endpoints stay on their own rays (shape kept)");
  ok &= expect(radial_gap(*arc) < 1e-6,
               "resize: the arc reaches its endpoints at the new radius");
  ok &= expect(weld_gap(after, *arc) < 1e-6,
               "resize: the welded lines follow the endpoints");
  return ok;
}

}  // namespace

int main() {
  bool ok = true;
  ok = test_real_arc_centre_drag_stays_rigid() && ok;
  ok = test_trimmed_arc_centre_drag_stays_rigid() && ok;
  ok = test_legacy_redundant_constraints_centre_drag() && ok;
  ok = test_tangent_overshoot_trim_does_not_split() && ok;
  ok = test_arc_endpoints_start_unfixed() && ok;
  ok = test_arc_span_is_serialized() && ok;
  ok = test_arc_radius_dimension_drag_keeps_lines_welded() && ok;
  ok = test_arc_radius_reconciles_from_endpoints() && ok;
  ok = test_curve_host_anchor_does_not_throw() && ok;
  ok = test_arc_perimeter_resize_keeps_centre() && ok;

  if (!ok) {
    std::cout << "sketch_arc_lines_test FAILED\n";
    return 1;
  }
  std::cout << "sketch_arc_lines_test passed\n";
  return 0;
}
