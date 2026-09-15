// Regression tests for sketch fillets on arc operands (feature/sketch).
//
// v2 fillets: line-line (v1, unchanged), line-arc, and arc-arc. The
// tangent-circle solve trims each operand back to its tangent point
// and inserts a tangent SketchArc, re-derived on every recompute
// (private_fillet_refresh.inc) and reverted on delete. All geometry
// values below are hand-computed; the tests pin the solver contract.
//
// Profile-set assertions use profiles_match (complete region-set
// matching, not presence-only).

#include <algorithm>
#include <cmath>
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

bool near(double a, double b, double tolerance = 1.0e-3) {
  return std::abs(a - b) < tolerance;
}

const SketchLine* find_line(const SketchFeatureParameters& params,
                            const std::string& id) {
  const auto it = std::find_if(
      params.lines.begin(), params.lines.end(),
      [&](const auto& l) { return l.id == id; });
  return it == params.lines.end() ? nullptr : &*it;
}

const SketchArc* find_arc(const SketchFeatureParameters& params,
                          const std::string& id) {
  const auto it = std::find_if(
      params.arcs.begin(), params.arcs.end(),
      [&](const auto& a) { return a.id == id; });
  return it == params.arcs.end() ? nullptr : &*it;
}

// ── Canonical line-arc corner ─────────────────────────────────────
//
// Line (0,0)→(40,0) meets arc {start (40,0), end (30,10), center
// (30,0), R=10, ccw} at the corner (40,0). For r=2 the unique valid
// fillet is the internal-tangency one:
//   O   = (30 + sqrt(60), 2)         ≈ (37.746, 2)
//   T_line = perpendicular foot      = (37.746, 0)
//   T_arc  = Ca + 10·unit(O − Ca)    ≈ (39.6825, 2.5)
// (|O−Ca| = R−r = 8; the only other candidate family fails the
// operand fit checks — external tangency lands off the line, and the
// mirrored internal candidate leaves the arc sweep.)
struct LineArcSetup {
  std::string corner_vertex_id;
  std::string line_id;
  std::string arc_id;
};
LineArcSetup make_line_arc_corner(DocumentManager& manager,
                                  const std::string& line_mode =
                                      "corner_at_line_end") {
  // line (0,0)→(40,0); corner at the line's END.
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 40.0, 0.0);
  const auto line_params = sketch_params(document);
  const std::string line_id = line_params.lines.back().id;
  const std::string line_end_vertex = line_params.lines.back().end_vertex_id;

  // Arc sharing the corner: ccw from (40,0) to (30,10) around (30,0).
  document = manager.add_sketch_arc(40.0, 0.0, 30.0, 10.0, 30.0, 0.0,
                                    "center_start_end");
  const auto arc_params = sketch_params(document);
  const std::string arc_id = arc_params.arcs.back().id;
  if (!expect(line_end_vertex == arc_params.arcs.back().start_vertex_id,
              "setup: arc start welds to the line end vertex")) {
    return {};
  }
  return {.corner_vertex_id = line_end_vertex,
          .line_id = line_id,
          .arc_id = arc_id};
}

// ── Creation: line-arc ────────────────────────────────────────────

bool test_line_arc_fillet_creates() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  const LineArcSetup setup = make_line_arc_corner(manager);
  if (setup.corner_vertex_id.empty()) return false;

  DocumentState document = manager.add_sketch_fillet(
      setup.corner_vertex_id, setup.line_id, "", 2.0, "", setup.arc_id);
  const auto params = sketch_params(document);
  if (!expect(params.fillets.size() == 1, "line-arc: fillet record created")) {
    return false;
  }
  const auto& fillet = params.fillets[0];
  if (!expect(fillet.line_a_id == setup.line_id &&
                  fillet.arc_b_id == setup.arc_id &&
                  fillet.line_b_id.empty() && fillet.arc_a_id.empty(),
              "line-arc: operands recorded with derived kinds")) {
    return false;
  }
  if (!expect(params.arcs.size() == 2,
              "line-arc: operand arc + generated fillet arc")) {
    return false;
  }

  const auto* line = find_line(params, setup.line_id);
  const auto* arc = find_arc(params, setup.arc_id);
  const auto* fillet_arc = find_arc(params, fillet.arc_id);
  if (!expect(line != nullptr && arc != nullptr && fillet_arc != nullptr,
              "line-arc: all entities present")) {
    return false;
  }

  // Hand-computed values: O=(30+√60, 2), line trim (37.746,0),
  // arc trim (39.6825, 2.5).
  const double cx = 30.0 + std::sqrt(60.0);
  bool ok = true;
  ok &= expect(near(fillet_arc->center_x, cx) &&
                   near(fillet_arc->center_y, 2.0) &&
                   near(fillet_arc->radius, 2.0),
               "line-arc: fillet arc center + radius");
  ok &= expect(line->end_vertex_id == fillet.trim_a_vertex_id &&
                   near(line->end_x, cx) && near(line->end_y, 0.0),
               "line-arc: line trimmed to the perpendicular foot");
  ok &= expect(arc->start_vertex_id == fillet.trim_b_vertex_id &&
                   near(arc->start_x, 39.6825) && near(arc->start_y, 2.5),
               "line-arc: arc trimmed to the radial tangent point");
  ok &= expect(fillet_arc->start_vertex_id == fillet.trim_a_vertex_id &&
                   fillet_arc->end_vertex_id == fillet.trim_b_vertex_id,
               "line-arc: fillet arc endpoints are the trim points");
  // Tangency invariants: |O.y| = r above the line; |O−Ca| = R−r.
  ok &= expect(near(std::hypot(fillet_arc->center_x - 30.0,
                               fillet_arc->center_y - 0.0),
                    8.0),
               "line-arc: internal tangency |O−Ca| = R−r");
  return ok;
}

// ── Creation: arc-arc ─────────────────────────────────────────────

bool test_arc_arc_fillet_creates() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Arc A: center (30,0) R10, start (40,0) → end (30,10), ccw.
  DocumentState document = manager.add_sketch_arc(40.0, 0.0, 30.0, 10.0,
                                                  30.0, 0.0,
                                                  "center_start_end");
  const auto a_params = sketch_params(document);
  const std::string arc_a_id = a_params.arcs.back().id;
  const std::string corner_vertex = a_params.arcs.back().start_vertex_id;

  // Arc B: center (40,10) R10, start (40,0) → end (50,10), ccw — the
  // start welds to the same corner vertex.
  document = manager.add_sketch_arc(40.0, 0.0, 50.0, 10.0, 40.0, 10.0,
                                    "center_start_end");
  const auto b_params = sketch_params(document);
  const std::string arc_b_id = b_params.arcs.back().id;
  if (!expect(corner_vertex == b_params.arcs.back().start_vertex_id,
              "arc-arc: both arcs share the corner vertex")) {
    return false;
  }

  document = manager.add_sketch_fillet(corner_vertex, "", "", 2.0,
                                       arc_a_id, arc_b_id);
  const auto params = sketch_params(document);
  if (!expect(params.fillets.size() == 1, "arc-arc: fillet record created")) {
    return false;
  }
  const auto& fillet = params.fillets[0];
  if (!expect(fillet.arc_a_id == arc_a_id && fillet.arc_b_id == arc_b_id &&
                  fillet.line_a_id.empty() && fillet.line_b_id.empty(),
              "arc-arc: both operands recorded as arcs")) {
    return false;
  }
  const auto* arc_a = find_arc(params, arc_a_id);
  const auto* arc_b = find_arc(params, arc_b_id);
  const auto* fillet_arc = find_arc(params, fillet.arc_id);
  if (!expect(arc_a != nullptr && arc_b != nullptr &&
                  fillet_arc != nullptr,
              "arc-arc: all entities present")) {
    return false;
  }

  // Hand-computed: external tangency on A (|O−Ca|=12), internal on B
  // (|O−Cb|=8): O=(41.7958, 2.2042), T_A=(39.8298, 1.8368),
  // T_B=(42.2448, 0.2552). The other candidates all fail the
  // within-sweep fit on one side.
  bool ok = true;
  ok &= expect(near(fillet_arc->center_x, 41.7958) &&
                   near(fillet_arc->center_y, 2.2042) &&
                   near(fillet_arc->radius, 2.0),
               "arc-arc: fillet arc center + radius");
  ok &= expect(near(std::hypot(fillet_arc->center_x - 30.0,
                               fillet_arc->center_y),
                    12.0),
               "arc-arc: |O−Ca| = Ra+r");
  ok &= expect(near(std::hypot(fillet_arc->center_x - 40.0,
                               fillet_arc->center_y - 10.0),
                    8.0),
               "arc-arc: |O−Cb| = Rb−r");
  ok &= expect(arc_a->start_vertex_id == fillet.trim_a_vertex_id &&
                   near(arc_a->start_x, 39.8298) &&
                   near(arc_a->start_y, 1.8368),
               "arc-arc: arc A trimmed to the radial tangent point");
  ok &= expect(arc_b->start_vertex_id == fillet.trim_b_vertex_id &&
                   near(arc_b->start_x, 42.2448) &&
                   near(arc_b->start_y, 0.2552),
               "arc-arc: arc B trimmed to the radial tangent point");
  return ok;
}

// ── Creation: corner at the arc's END endpoint ────────────────────

bool test_line_arc_fillet_corner_at_arc_end() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 40.0, 0.0);
  const auto line_params = sketch_params(document);
  const std::string line_id = line_params.lines.back().id;
  const std::string corner_vertex = line_params.lines.back().end_vertex_id;

  // Same arc point-set as the canonical case, walked the other way:
  // start (30,10) → end (40,0) around center (30,0) (cw). The corner
  // is the arc's END endpoint.
  document = manager.add_sketch_arc(30.0, 10.0, 40.0, 0.0, 30.0, 0.0,
                                    "center_start_end");
  const auto arc_params = sketch_params(document);
  const std::string arc_id = arc_params.arcs.back().id;
  if (!expect(corner_vertex == arc_params.arcs.back().end_vertex_id,
              "end-corner setup: arc end welds to the line end vertex")) {
    return false;
  }

  document = manager.add_sketch_fillet(corner_vertex, line_id, "", 2.0,
                                       "", arc_id);
  const auto params = sketch_params(document);
  if (!expect(params.fillets.size() == 1,
              "end-corner: fillet record created")) {
    return false;
  }
  const auto& fillet = params.fillets[0];
  const auto* arc = find_arc(params, arc_id);
  if (!expect(arc != nullptr, "end-corner: operand arc present")) {
    return false;
  }
  // The corner endpoint is the arc's END — the trim mutation must
  // swap the end (the start keeps the far endpoint (30,10)).
  return expect(arc->end_vertex_id == fillet.trim_b_vertex_id &&
                    near(arc->end_x, 39.6825) && near(arc->end_y, 2.5) &&
                    near(arc->start_x, 30.0) && near(arc->start_y, 10.0),
                "end-corner: arc END trimmed, START untouched");
}

// ── Radius edit ───────────────────────────────────────────────────

bool test_fillet_radius_update_line_arc() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  const LineArcSetup setup = make_line_arc_corner(manager);
  if (setup.corner_vertex_id.empty()) return false;

  DocumentState document = manager.add_sketch_fillet(
      setup.corner_vertex_id, setup.line_id, "", 2.0, "", setup.arc_id);
  const std::string fillet_id = sketch_params(document).fillets[0].id;

  document = manager.update_sketch_fillet_radius(fillet_id, 1.5);
  const auto params = sketch_params(document);
  const auto* line = find_line(params, setup.line_id);
  const auto& fillet = params.fillets[0];
  const auto* fillet_arc = find_arc(params, fillet.arc_id);
  if (!expect(line != nullptr && fillet_arc != nullptr,
              "radius edit: entities present")) {
    return false;
  }
  // r=1.5 ⇒ O=(30+√70, 1.5), line foot (38.3666, 0),
  // |O−Ca| = R−r = 8.5.
  const double cx = 30.0 + std::sqrt(70.0);
  return expect(near(fillet_arc->radius, 1.5) &&
                    near(fillet_arc->center_x, cx) &&
                    near(fillet_arc->center_y, 1.5) &&
                    near(line->end_x, cx) && near(line->end_y, 0.0) &&
                    near(std::hypot(fillet_arc->center_x - 30.0,
                                    fillet_arc->center_y),
                         8.5),
                "radius edit: geometry re-derived for r=1.5");
}

// ── Delete / revert ───────────────────────────────────────────────

bool test_fillet_delete_restores_shared_corner_line_arc() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  const LineArcSetup setup = make_line_arc_corner(manager);
  if (setup.corner_vertex_id.empty()) return false;

  DocumentState document = manager.add_sketch_fillet(
      setup.corner_vertex_id, setup.line_id, "", 2.0, "", setup.arc_id);
  const std::string fillet_id = sketch_params(document).fillets[0].id;

  document = manager.delete_sketch_fillet(fillet_id);
  const auto params = sketch_params(document);
  if (!expect(params.fillets.empty() && params.arcs.size() == 1,
              "delete: fillet record + generated arc removed")) {
    return false;
  }
  const auto* line = find_line(params, setup.line_id);
  const auto* arc = find_arc(params, setup.arc_id);
  if (!expect(line != nullptr && arc != nullptr,
              "delete: both operands survive")) {
    return false;
  }
  return expect(near(line->end_x, 40.0) && near(line->end_y, 0.0) &&
                    near(arc->start_x, 40.0) && near(arc->start_y, 0.0) &&
                    line->end_vertex_id == setup.corner_vertex_id &&
                    arc->start_vertex_id == setup.corner_vertex_id,
                "delete: original corner restored and shared");
}

bool test_fillet_delete_restores_arc_arc() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_arc(40.0, 0.0, 30.0, 10.0,
                                                  30.0, 0.0,
                                                  "center_start_end");
  const auto a_params = sketch_params(document);
  const std::string arc_a_id = a_params.arcs.back().id;
  const std::string corner_vertex = a_params.arcs.back().start_vertex_id;
  document = manager.add_sketch_arc(40.0, 0.0, 50.0, 10.0, 40.0, 10.0,
                                    "center_start_end");
  const std::string arc_b_id = sketch_params(document).arcs.back().id;

  document = manager.add_sketch_fillet(corner_vertex, "", "", 2.0,
                                       arc_a_id, arc_b_id);
  const std::string fillet_id = sketch_params(document).fillets[0].id;
  document = manager.delete_sketch_fillet(fillet_id);
  const auto params = sketch_params(document);
  if (!expect(params.fillets.empty() && params.arcs.size() == 2,
              "arc-arc delete: fillet + generated arc removed")) {
    return false;
  }
  const auto* arc_a = find_arc(params, arc_a_id);
  const auto* arc_b = find_arc(params, arc_b_id);
  if (!expect(arc_a != nullptr && arc_b != nullptr,
              "arc-arc delete: both operands survive")) {
    return false;
  }
  return expect(near(arc_a->start_x, 40.0) && near(arc_a->start_y, 0.0) &&
                    near(arc_b->start_x, 40.0) && near(arc_b->start_y, 0.0) &&
                    arc_a->start_vertex_id == corner_vertex &&
                    arc_b->start_vertex_id == corner_vertex,
                "arc-arc delete: original corner restored and shared");
}

// ── Rejections ────────────────────────────────────────────────────

bool test_oversized_radius_rejected() {
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    const LineArcSetup setup = make_line_arc_corner(manager);
    bool threw = false;
    try {
      (void)manager.add_sketch_fillet(setup.corner_vertex_id, setup.line_id,
                                      "", 11.0, "", setup.arc_id);
    } catch (const std::exception&) {
      threw = true;
    }
    if (!expect(threw, "oversized: r=11 on the canonical corner is rejected")) {
      return false;
    }
  }
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    const LineArcSetup setup = make_line_arc_corner(manager);
    bool threw = false;
    try {
      (void)manager.add_sketch_fillet(setup.corner_vertex_id, setup.line_id,
                                      "", 45.0, "", setup.arc_id);
    } catch (const std::exception&) {
      threw = true;
    }
    return expect(threw, "oversized: r=45 is rejected");
  }
}

bool test_tangent_corner_rejected() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Line (0,0)→(40,0) with an arc tangent to it at (40,0): arc
  // center (40,10), start (40,0) → end (50,10). The line's away
  // direction (−1,0) is parallel to the arc's tangent at the corner.
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 40.0, 0.0);
  const auto line_params = sketch_params(document);
  const std::string line_id = line_params.lines.back().id;
  const std::string corner_vertex = line_params.lines.back().end_vertex_id;
  document = manager.add_sketch_arc(40.0, 0.0, 50.0, 10.0, 40.0, 10.0,
                                    "center_start_end");
  const std::string arc_id = sketch_params(document).arcs.back().id;

  bool threw = false;
  try {
    (void)manager.add_sketch_fillet(corner_vertex, line_id, "", 2.0, "",
                                    arc_id);
  } catch (const std::exception&) {
    threw = true;
  }
  return expect(threw, "tangent corner: fillet is rejected");
}

bool test_construction_arc_rejected() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_line(0.0, 0.0, 40.0, 0.0);
  const auto line_params = sketch_params(document);
  const std::string line_id = line_params.lines.back().id;
  const std::string corner_vertex = line_params.lines.back().end_vertex_id;
  document = manager.add_sketch_arc(40.0, 0.0, 30.0, 10.0, 30.0, 0.0,
                                    "center_start_end", /*construction=*/true);
  const std::string arc_id = sketch_params(document).arcs.back().id;

  bool threw = false;
  try {
    (void)manager.add_sketch_fillet(corner_vertex, line_id, "", 2.0, "",
                                    arc_id);
  } catch (const std::exception&) {
    threw = true;
  }
  return expect(threw, "construction arc: fillet is rejected");
}

// ── Enforcement ───────────────────────────────────────────────────

bool test_enforcement_after_line_far_end_move() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  const LineArcSetup setup = make_line_arc_corner(manager);
  if (setup.corner_vertex_id.empty()) return false;
  DocumentState document = manager.add_sketch_fillet(
      setup.corner_vertex_id, setup.line_id, "", 2.0, "", setup.arc_id);

  // Move the line's FAR end (0,0)→(0,5); the trim endpoint coords
  // are enforcement-owned. The corner re-derives as the nearest
  // line-circle intersection to the cached corner.
  document = manager.update_sketch_line(setup.line_id, 0.0, 5.0,
                                        37.746, 0.0);
  const auto params = sketch_params(document);
  if (!expect(params.fillets.size() == 1,
              "far-end move: fillet survives")) {
    return false;
  }
  const auto& fillet = params.fillets[0];
  const auto* line = find_line(params, setup.line_id);
  const auto* fillet_arc = find_arc(params, fillet.arc_id);
  if (!expect(line != nullptr && fillet_arc != nullptr &&
                  line->end_vertex_id == fillet.trim_a_vertex_id,
              "far-end move: line still trimmed by the fillet")) {
    return false;
  }
  // Tangency invariants against the MOVED line carrier (far → trim):
  const double dir_x = line->end_x - line->start_x;
  const double dir_y = line->end_y - line->start_y;
  const double len = std::hypot(dir_x, dir_y);
  if (len < 1e-9) return false;
  const double dist_to_line = std::abs(
      (fillet_arc->center_x - line->start_x) * (-dir_y / len) +
      (fillet_arc->center_y - line->start_y) * (dir_x / len));
  return expect(near(dist_to_line, 2.0, 1e-2) &&
                    near(std::hypot(fillet_arc->center_x - 30.0,
                                    fillet_arc->center_y),
                         8.0, 1e-2) &&
                    near(fillet_arc->radius, 2.0),
                "far-end move: tangency re-derived on the moved line");
}

bool test_enforcement_after_arc_radius_dimension_drive() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  const LineArcSetup setup = make_line_arc_corner(manager);
  if (setup.corner_vertex_id.empty()) return false;
  DocumentState document = manager.add_sketch_fillet(
      setup.corner_vertex_id, setup.line_id, "", 2.0, "", setup.arc_id);

  // Drive the operand arc's radius 10 → 12 (center fixed). The corner
  // re-derives at (42,0) — the x-axis ∩ circle intersection nearest
  // the cached corner (40,0).
  document = manager.add_sketch_arc_radius_dimension(setup.arc_id);
  std::string dimension_id;
  for (const auto& d : sketch_params(document).dimensions) {
    if (d.entity_id == setup.arc_id) dimension_id = d.id;
  }
  if (!expect(!dimension_id.empty(), "radius drive: dimension created")) {
    return false;
  }
  document = manager.update_sketch_dimension(dimension_id, 12.0);
  const auto params = sketch_params(document);
  if (!expect(params.fillets.size() == 1,
              "radius drive: fillet survives")) {
    return false;
  }
  const auto& fillet = params.fillets[0];
  const auto* fillet_arc = find_arc(params, fillet.arc_id);
  if (!expect(fillet_arc != nullptr, "radius drive: fillet arc present")) {
    return false;
  }
  // r=2 on R=12: O=(30+√96, 2)≈(39.798, 2), |O−Ca| = 10.
  return expect(near(fillet_arc->center_x, 30.0 + std::sqrt(96.0), 1e-2) &&
                    near(fillet_arc->center_y, 2.0, 1e-2) &&
                    near(std::hypot(fillet_arc->center_x - 30.0,
                                    fillet_arc->center_y),
                         10.0, 1e-2) &&
                    near(fillet_arc->radius, 2.0) &&
                    near(fillet.corner_x, 42.0, 1e-2) &&
                    near(fillet.corner_y, 0.0, 1e-2),
                "radius drive: fillet re-derived on the R=12 circle");
}

// ── Profile detection ─────────────────────────────────────────────

bool test_filleted_line_arc_profile_detected() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Closed shape: line (0,0)→(40,0), arc (40,0)→(30,10) around
  // (30,0), then lines (30,10)→(0,10) and (0,10)→(0,0).
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 40.0, 0.0);
  const auto l1 = sketch_params(document);
  const std::string line_bottom = l1.lines.back().id;
  const std::string corner_vertex = l1.lines.back().end_vertex_id;
  document = manager.add_sketch_arc(40.0, 0.0, 30.0, 10.0, 30.0, 0.0,
                                    "center_start_end");
  const std::string arc_id = sketch_params(document).arcs.back().id;
  document = manager.add_sketch_line(30.0, 10.0, 0.0, 10.0);
  document = manager.add_sketch_line(0.0, 10.0, 0.0, 0.0);

  document = manager.add_sketch_fillet(corner_vertex, line_bottom, "", 2.0,
                                       "", arc_id);
  const auto params = sketch_params(document);
  const auto& fillet = params.fillets[0];

  // The region closes with line-bottom (trimmed), the fillet arc, the
  // operand arc (trimmed), and the two closing lines.
  std::vector<std::string> expected_ids;
  for (const auto& line : params.lines) expected_ids.push_back(line.id);
  for (const auto& arc : params.arcs) expected_ids.push_back(arc.id);
  if (!expect(params.fillets.size() == 1 &&
                  expected_ids.size() == 5 &&
                  std::find(expected_ids.begin(), expected_ids.end(),
                            fillet.arc_id) != expected_ids.end(),
              "profile: 3 lines + operand arc + fillet arc")) {
    return false;
  }
  std::string reason;
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = expected_ids, .kind = "polygon"},
  };
  return expect(profiles_match(document, expected, &reason),
                ("filleted line-arc profile: " + reason).c_str());
}

// ── Sibling cleanup ───────────────────────────────────────────────

bool test_trim_operand_line_drops_fillet() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  const LineArcSetup setup = make_line_arc_corner(manager);
  if (setup.corner_vertex_id.empty()) return false;
  DocumentState document = manager.add_sketch_fillet(
      setup.corner_vertex_id, setup.line_id, "", 2.0, "", setup.arc_id);
  if (!expect(sketch_params(document).fillets.size() == 1,
              "trim setup: fillet created")) {
    return false;
  }

  // Cross the operand line at (20,0) so the trim really splits it,
  // then trim keeping the (0,0)→(20,0) segment.
  document = manager.add_sketch_line(20.0, -5.0, 20.0, 5.0);
  document = manager.trim_sketch_entity(setup.line_id, 10.0, 0.0);
  return expect(sketch_params(document).fillets.empty(),
                "trim: fillet on the trimmed line is dropped");
}

bool test_delete_selection_of_operand_arc_drops_fillet() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  const LineArcSetup setup = make_line_arc_corner(manager);
  if (setup.corner_vertex_id.empty()) return false;
  DocumentState document = manager.add_sketch_fillet(
      setup.corner_vertex_id, setup.line_id, "", 2.0, "", setup.arc_id);
  const std::string fillet_arc_id =
      sketch_params(document).fillets[0].arc_id;

  document = manager.delete_sketch_selection({setup.arc_id}, {}, {});
  const auto params = sketch_params(document);
  if (!expect(params.fillets.empty(),
              "delete selection: fillet on the deleted arc is dropped")) {
    return false;
  }
  return expect(find_arc(params, setup.arc_id) == nullptr &&
                    find_arc(params, fillet_arc_id) == nullptr &&
                    find_line(params, setup.line_id) != nullptr,
                "delete selection: operand + generated arc gone, line stays");
}

// ── Guard semantics ───────────────────────────────────────────────

bool test_fillet_both_ends_of_same_arc_allowed() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Line1 (0,0)→(40,0) ─ corner1 (40,0) ─ arc (40,0)→(30,10) around
  // (30,0) ─ corner2 (30,10) ─ line2 (30,10)→(20,15) (non-tangent —
  // a tangent joint has no corner to round and is rejected).
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 40.0, 0.0);
  const auto l1 = sketch_params(document);
  const std::string line1_id = l1.lines.back().id;
  const std::string corner1 = l1.lines.back().end_vertex_id;
  document = manager.add_sketch_arc(40.0, 0.0, 30.0, 10.0, 30.0, 0.0,
                                    "center_start_end");
  const auto a1 = sketch_params(document);
  const std::string arc_id = a1.arcs.back().id;
  const std::string corner2 = a1.arcs.back().end_vertex_id;
  document = manager.add_sketch_line(30.0, 10.0, 20.0, 15.0);
  const std::string line2_id = sketch_params(document).lines.back().id;

  document = manager.add_sketch_fillet(corner1, line1_id, "", 2.0, "",
                                       arc_id);
  document = manager.add_sketch_fillet(corner2, line2_id, "", 1.5, "",
                                       arc_id);
  return expect(sketch_params(document).fillets.size() == 2,
                "both-ends: two fillets on opposite corners of one arc");
}

// ── Serialization round-trip ──────────────────────────────────────

bool test_fillet_arc_serialization_round_trip() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  const LineArcSetup setup = make_line_arc_corner(manager);
  if (setup.corner_vertex_id.empty()) return false;
  DocumentState document = manager.add_sketch_fillet(
      setup.corner_vertex_id, setup.line_id, "", 2.0, "", setup.arc_id);

  // Save/load path: payload round-trip must preserve the arc operand
  // ids so the parametric relation survives across sessions.
  const auto payload = polysmith::protocol::to_payload(document, true);
  const auto restored = polysmith::protocol::document_from_payload(payload);

  const auto params = sketch_params(restored);
  if (!expect(params.fillets.size() == 1,
              "serialization: fillet record survives round-trip")) {
    return false;
  }
  const auto& fillet = params.fillets[0];
  if (!expect(fillet.arc_b_id == setup.arc_id && fillet.arc_a_id.empty() &&
                  fillet.line_a_id == setup.line_id &&
                  fillet.line_b_id.empty() &&
                  near(fillet.radius, 2.0),
              "serialization: operand ids (line + arc) round-trip")) {
    return false;
  }
  const auto* fillet_arc = find_arc(params, fillet.arc_id);
  const auto* operand_arc = find_arc(params, setup.arc_id);
  return expect(fillet_arc != nullptr && operand_arc != nullptr &&
                    near(fillet_arc->center_x, 30.0 + std::sqrt(60.0)) &&
                    near(fillet_arc->center_y, 2.0),
                "serialization: generated fillet arc round-trips intact");
}

// ── Split-vertex heal + tolerant fillet ──────────────────────────
//
// Legacy sketches projected before the endpoint welding existed hold
// two vertex ids a few microns apart at a shared corner.  The array
// copy path is the one current-code path that deliberately mints
// fresh vertex ids at coincident positions ("copies never adopt
// coincident vertex ids"), so a zero-offset copy fabricates exactly
// the legacy split state for these tests.

bool test_merge_coincident_points_heals_splits() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  const LineArcSetup setup = make_line_arc_corner(manager);
  if (setup.corner_vertex_id.empty()) return false;

  // Fabricate a legacy split: a zero-offset array copy of the arc —
  // fresh vertex ids at exactly the same positions.
  DocumentState document =
      manager.create_linear_array({setup.arc_id}, 0.0, 0.0, 2);
  const auto split_params = sketch_params(document);
  if (!expect(split_params.arcs.size() == 2,
              "heal setup: array copy duplicates the arc")) {
    return false;
  }
  const std::string copy_id = split_params.arcs.back().id;
  const auto* original = find_arc(split_params, setup.arc_id);
  const auto* copy = find_arc(split_params, copy_id);
  if (!expect(original != nullptr && copy != nullptr &&
                  original->start_vertex_id != copy->start_vertex_id,
              "heal setup: copy holds a distinct corner vertex id")) {
    return false;
  }

  // Heal.
  document = manager.merge_coincident_sketch_points(
      document.feature_history.back().id);
  const auto healed = sketch_params(document);
  const auto* healed_orig = find_arc(healed, setup.arc_id);
  const auto* healed_copy = find_arc(healed, copy_id);
  return expect(
      healed_orig != nullptr && healed_copy != nullptr &&
          healed_orig->start_vertex_id == healed_copy->start_vertex_id &&
          healed_orig->start_vertex_id == setup.corner_vertex_id,
      "heal: split corner merged to the shared vertex");
}

bool test_merge_coincident_points_leaves_far_vertices() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Two corners 0.05 mm apart must NOT merge (beyond tolerance).
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 40.0, 0.0);
  document = manager.add_sketch_line(40.0, 0.0, 40.05, 10.0);
  document = manager.add_sketch_line(40.05, 10.0, 0.0, 10.0);
  const auto before = sketch_params(document);
  const std::string second_corner = before.lines[1].end_vertex_id;
  document = manager.merge_coincident_sketch_points(
      document.feature_history.back().id);
  const auto after = sketch_params(document);
  const bool still_distinct = std::any_of(
      after.lines.begin(), after.lines.end(), [&](const auto& line) {
        return line.start_vertex_id == second_corner ||
               line.end_vertex_id == second_corner;
      });
  return expect(still_distinct && after.lines.size() == 3,
                "heal: vertices beyond tolerance stay distinct");
}

bool test_fillet_welds_split_arc_arc_corner() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Two welded arcs at (40,0): A center (30,0) R10 (40,0)->(30,10);
  // B center (40,10) R10 (40,0)->(50,10).
  DocumentState document = manager.add_sketch_arc(40.0, 0.0, 30.0, 10.0,
                                                  30.0, 0.0,
                                                  "center_start_end");
  const std::string arc_a = sketch_params(document).arcs.back().id;
  document = manager.add_sketch_arc(40.0, 0.0, 50.0, 10.0, 40.0, 10.0,
                                    "center_start_end");
  const auto welded = sketch_params(document);
  const std::string arc_b = welded.arcs.back().id;
  const std::string corner_vertex = welded.arcs.back().start_vertex_id;

  // Split: zero-offset copy of B (fresh corner vertex id), then
  // delete the original B so only the split copy remains.
  document = manager.create_linear_array({arc_b}, 0.0, 0.0, 2);
  const std::string copy_b = sketch_params(document).arcs.back().id;
  document = manager.delete_sketch_selection({arc_b}, {}, {});
  const auto split = sketch_params(document);
  const auto* copy_b_arc = find_arc(split, copy_b);
  if (!expect(copy_b_arc != nullptr &&
                  copy_b_arc->start_vertex_id != corner_vertex,
              "split setup: copy arc keeps a distinct corner vertex")) {
    return false;
  }

  // Fillet with the ORIGINAL corner id: the copy's endpoint is
  // coincident (distance 0) — the tolerance weld must accept it and
  // permanently unify the corner.
  document = manager.add_sketch_fillet(corner_vertex, "", "", 2.0,
                                       arc_a, copy_b);
  const auto params = sketch_params(document);
  if (!expect(params.fillets.size() == 1, "split fillet: created")) {
    return false;
  }
  const auto& fillet = params.fillets[0];
  const auto* fillet_arc = find_arc(params, fillet.arc_id);
  const auto* a_arc = find_arc(params, arc_a);
  const auto* b_arc = find_arc(params, copy_b);
  // The fillet record adopts the canonical corner id (proving the
  // tolerant weld matched the copy by position); operands' corner
  // endpoints hold the freshly minted trim ids, as in the canonical
  // case.
  bool ok = true;
  ok &= expect(fillet_arc != nullptr && a_arc != nullptr && b_arc != nullptr,
               "split fillet: all entities present");
  ok &= expect(fillet.corner_vertex_id == corner_vertex,
               "split fillet: record adopts the canonical corner id");
  ok &= expect(a_arc->start_vertex_id == fillet.trim_a_vertex_id &&
                   b_arc->start_vertex_id == fillet.trim_b_vertex_id,
               "split fillet: operands trimmed onto the minted trim ids");
  ok &= expect(near(a_arc->start_x, 39.8298) &&
                   near(a_arc->start_y, 1.8368) &&
                   near(b_arc->start_x, 42.2448) &&
                   near(b_arc->start_y, 0.2552),
               "split fillet: canonical trim coordinates");
  if (!ok) return false;
  // Same solution as the canonical arc-arc case.
  if (!expect(near(fillet_arc->center_x, 41.7958) &&
                  near(fillet_arc->center_y, 2.2042),
              "split fillet: canonical tangent-circle solve")) {
    return false;
  }

  // Delete restores both operands to the corner.
  document = manager.delete_sketch_fillet(fillet.id);
  const auto restored = sketch_params(document);
  const auto* ra = find_arc(restored, arc_a);
  const auto* rb = find_arc(restored, copy_b);
  return expect(ra != nullptr && rb != nullptr &&
                    ra->start_vertex_id == corner_vertex &&
                    rb->start_vertex_id == corner_vertex &&
                    near(ra->start_x, 40.0) && near(ra->start_y, 0.0) &&
                    near(rb->start_x, 40.0) && near(rb->start_y, 0.0),
                "split fillet: delete restores the shared corner");
}

bool test_fillet_rejects_corner_beyond_tolerance() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_arc(40.0, 0.0, 30.0, 10.0,
                                                  30.0, 0.0,
                                                  "center_start_end");
  const std::string arc_a = sketch_params(document).arcs.back().id;
  document = manager.add_sketch_arc(40.0, 0.0, 50.0, 10.0, 40.0, 10.0,
                                    "center_start_end");
  const auto welded = sketch_params(document);
  const std::string arc_b = welded.arcs.back().id;
  const std::string corner_vertex = welded.arcs.back().start_vertex_id;

  // Copy B 0.05 mm away — beyond the coincident tolerance.
  document = manager.create_linear_array({arc_b}, 0.05, 0.0, 2);
  const std::string copy_b = sketch_params(document).arcs.back().id;
  document = manager.delete_sketch_selection({arc_b}, {}, {});

  bool threw = false;
  try {
    (void)manager.add_sketch_fillet(corner_vertex, "", "", 2.0,
                                    arc_a, copy_b);
  } catch (const std::exception&) {
    threw = true;
  }
  return expect(threw,
                "split fillet: corner beyond tolerance is rejected");
}

// ── v1 line-line regression through the new signature ─────────────

bool test_line_line_regression() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Identical to sketch_profile_test.cpp test_fillet_creates_arc_and_
  // trims_lines: rectangle 40×20, corner (40,0) shared by the bottom
  // (line-1) and right (line-2) sides, r=5.
  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  const auto before = sketch_params(document);
  const std::string corner = before.lines[0].end_vertex_id;
  const std::string line_bottom = before.lines[0].id;
  const std::string line_right = before.lines[1].id;

  // Defaulted arc ids — the v1 call shape.
  document = manager.add_sketch_fillet(corner, line_bottom, line_right, 5.0);
  const auto params = sketch_params(document);
  if (!expect(params.fillets.size() == 1 && params.arcs.size() == 1,
              "v1 regression: fillet + arc created")) {
    return false;
  }
  const auto& fillet = params.fillets[0];
  if (!expect(fillet.line_a_id == line_bottom &&
                  fillet.line_b_id == line_right &&
                  fillet.arc_a_id.empty() && fillet.arc_b_id.empty(),
              "v1 regression: record carries line ids only")) {
    return false;
  }
  const auto* bottom = find_line(params, line_bottom);
  const auto* right = find_line(params, line_right);
  const auto* fillet_arc = find_arc(params, fillet.arc_id);
  if (!expect(bottom != nullptr && right != nullptr &&
                  fillet_arc != nullptr,
              "v1 regression: entities present")) {
    return false;
  }
  // sketch_profile_test expectations: trims (35,0)/(40,5),
  // center (35,5), r=5.
  return expect(near(bottom->end_x, 35.0) && near(bottom->end_y, 0.0) &&
                    near(right->start_x, 40.0) && near(right->start_y, 5.0) &&
                    near(fillet_arc->center_x, 35.0) &&
                    near(fillet_arc->center_y, 5.0) &&
                    near(fillet_arc->radius, 5.0),
                "v1 regression: geometry identical to the line-line solve");
}

// Runs one test; catches exceptions so a rejected fillet (the
// fail-before gate at Commit A) reports its message instead of
// terminating the whole binary with an uncaught throw.
bool run_test(const char* name, bool (*fn)()) {
  try {
    const bool ok = fn();
    if (!ok) std::cout << name << ": FAILED\n";
    return ok;
  } catch (const std::exception& e) {
    std::cout << name << ": THREW — " << e.what() << "\n";
    return false;
  }
}

}  // namespace

int main() {
  // |= chain (not short-circuit) so every test runs and all failures
  // are reported.
  bool all_ok = true;
  all_ok = run_test("line-arc creates", test_line_arc_fillet_creates) && all_ok;
  all_ok = run_test("arc-arc creates", test_arc_arc_fillet_creates) && all_ok;
  all_ok = run_test("corner at arc end",
                    test_line_arc_fillet_corner_at_arc_end) && all_ok;
  all_ok = run_test("radius update",
                    test_fillet_radius_update_line_arc) && all_ok;
  all_ok = run_test("delete restores line-arc",
                    test_fillet_delete_restores_shared_corner_line_arc) &&
           all_ok;
  all_ok = run_test("delete restores arc-arc",
                    test_fillet_delete_restores_arc_arc) && all_ok;
  all_ok = run_test("oversized radius rejected",
                    test_oversized_radius_rejected) && all_ok;
  all_ok = run_test("tangent corner rejected",
                    test_tangent_corner_rejected) && all_ok;
  all_ok = run_test("construction arc rejected",
                    test_construction_arc_rejected) && all_ok;
  all_ok = run_test("enforce after line far-end move",
                    test_enforcement_after_line_far_end_move) && all_ok;
  all_ok = run_test("enforce after arc radius drive",
                    test_enforcement_after_arc_radius_dimension_drive) &&
           all_ok;
  all_ok = run_test("filleted line-arc profile",
                    test_filleted_line_arc_profile_detected) && all_ok;
  all_ok = run_test("trim operand line drops fillet",
                    test_trim_operand_line_drops_fillet) && all_ok;
  all_ok = run_test("delete operand arc drops fillet",
                    test_delete_selection_of_operand_arc_drops_fillet) &&
           all_ok;
  all_ok = run_test("both ends of one arc",
                    test_fillet_both_ends_of_same_arc_allowed) && all_ok;
  all_ok = run_test("serialization round-trip",
                    test_fillet_arc_serialization_round_trip) && all_ok;
  all_ok = run_test("v1 line-line regression",
                    test_line_line_regression) && all_ok;
  all_ok = run_test("heal merges split corner",
                    test_merge_coincident_points_heals_splits) && all_ok;
  all_ok = run_test("heal leaves far vertices",
                    test_merge_coincident_points_leaves_far_vertices) &&
           all_ok;
  all_ok = run_test("fillet welds split arc-arc corner",
                    test_fillet_welds_split_arc_arc_corner) && all_ok;
  all_ok = run_test("fillet rejects corner beyond tolerance",
                    test_fillet_rejects_corner_beyond_tolerance) && all_ok;

  if (!all_ok) {
    std::cout << "sketch_fillet_arc_test FAILED\n";
    return 1;
  }
  std::cout << "sketch_fillet_arc_test passed\n";
  return 0;
}
