// Trim redesign — stage-isolated verification program.
//
// Each building stage of the redesign (spec: Trim-Tool-Redesign-
// Requirements.md) has a dedicated test group. Run the whole program
// (joins the automatic gate run) or isolate one stage:
//
//   cad_core_trim_stages_test            -> all stages
//   cad_core_trim_stages_test --stage 5  -> corner trim only
//   cad_core_trim_stages_test --list     -> stage names
//
// Stage map:
//   1  Phase 0 trim behaviors (line / circle / delete / construction
//      cutter / boundary click with no ghost segments)
//   2  Phase 1 constraint transfer (H/V badge, parallel /
//      perpendicular / equal_length, coincident survival,
//      point-on-object anchors)
//   3  Phase 1 driven dimension re-derivation + fillet record cleanup
//   4  Phase 1 projection generated-id pruning
//   5  Phase 2 corner trim (virtual corner + hover preview)
//   6  Phase 2 extend
//   7  Phase 2 split
//   8  Phase 3 drag-paint stroke with ONE undo entry

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "core/document/feature.h"
#include "core/sketch/sketch_feature.h"
#include "core/sketch/sketch_feature_parameters.h"
#include "core/sketch/sketch_geometry_types.h"
#include "core/sketch/sketch_projection_types.h"
#include "sketch_test_utils.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FeatureEntry;
using polysmith::core::SketchFeatureParameters;
using polysmith::core::SketchLine;
using polysmith::core::SketchProjection;
using polysmith::core::TrimStrokeEntry;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << message << "\n";
  return false;
}

constexpr double kPi = 3.14159265358979323846;

// Finds a line by id (nullptr when missing).
const SketchLine* find_line(const SketchFeatureParameters& params,
                            const std::string& id) {
  for (const auto& l : params.lines) {
    if (l.id == id) return &l;
  }
  return nullptr;
}

// True when a line with the given endpoints (any id) exists.
bool line_with_endpoints(const SketchFeatureParameters& params,
                         double sx, double sy, double ex, double ey,
                         double eps) {
  for (const auto& l : params.lines) {
    if (std::abs(l.start_x - sx) <= eps && std::abs(l.start_y - sy) <= eps &&
        std::abs(l.end_x - ex) <= eps && std::abs(l.end_y - ey) <= eps) {
      return true;
    }
    if (std::abs(l.start_x - ex) <= eps && std::abs(l.start_y - ey) <= eps &&
        std::abs(l.end_x - sx) <= eps && std::abs(l.end_y - sy) <= eps) {
      return true;
    }
  }
  return false;
}

double arc_sweep(const polysmith::core::SketchArc& arc) {
  const double s =
      std::atan2(arc.start_y - arc.center_y, arc.start_x - arc.center_x);
  const double e =
      std::atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x);
  double d = arc.ccw ? e - s : s - e;
  while (d < 0.0) d += 2.0 * kPi;
  return d;
}

// ── Stage 1: Phase 0 trim behaviors ──────────────────────────────

bool stage1_trim_behaviors() {
  // 1a. Line trim shortens to the crossing.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.add_sketch_line(0.0, 0.0, 100.0, 0.0);
    manager.add_sketch_line(50.0, -10.0, 50.0, 10.0);
    const auto doc = manager.trim_sketch_entity("line-1", 75.0, 0.0);
    const auto& p = doc.feature_history.back().sketch_parameters.value();
    if (!expect(find_line(p, "line-1") != nullptr &&
                    std::abs(find_line(p, "line-1")->end_x - 50.0) < 0.01,
                "stage1: line trim should shorten to the crossing at x=50")) {
      return false;
    }
  }

  // 1b. Circle trim converts to a single arc.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.add_sketch_circle(0.0, 0.0, 10.0);
    manager.add_sketch_line(-20.0, 0.0, 20.0, 0.0);
    const auto doc = manager.trim_sketch_entity("circle-1", 0.0, 10.0);
    const auto& p = doc.feature_history.back().sketch_parameters.value();
    if (!expect(p.circles.empty() && p.arcs.size() == 1,
                "stage1: circle trim should leave one arc")) {
      return false;
    }
    if (p.arcs.size() == 1) {
      const auto& a = p.arcs[0];
      const double e1 = std::hypot(a.start_x + 10.0, a.start_y);
      const double e2 = std::hypot(a.end_x - 10.0, a.end_y);
      if (!expect(e1 < 0.01 && e2 < 0.01,
                  "stage1: the arc should span (-10,0) to (10,0)")) {
        return false;
      }
    }
  }

  // 1c. Isolated entity: trim deletes it.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.add_sketch_line(0.0, 0.0, 50.0, 0.0);
    const auto doc = manager.trim_sketch_entity("line-1", 25.0, 0.0);
    const auto& p = doc.feature_history.back().sketch_parameters.value();
    if (!expect(p.lines.empty(),
                "stage1: isolated line trim should delete the line")) {
      return false;
    }
  }

  // 1d. Construction line acts as a cutting edge.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.add_sketch_line(0.0, 0.0, 100.0, 0.0);
    manager.add_sketch_line(50.0, -10.0, 50.0, 10.0, true);
    const auto doc = manager.trim_sketch_entity("line-1", 75.0, 0.0);
    const auto& p = doc.feature_history.back().sketch_parameters.value();
    const auto* l = find_line(p, "line-1");
    if (!expect(l != nullptr && std::abs(l->end_x - 50.0) < 0.01,
                "stage1: construction line must cut the solid line")) {
      return false;
    }
  }

  // 1e. Click exactly at a split point — no ghost segments: exactly
  // one half-circle arc, never a silent no-op or two arcs.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.add_sketch_circle(0.0, 0.0, 10.0);
    manager.add_sketch_line(-20.0, 0.0, 20.0, 0.0);
    const auto doc = manager.trim_sketch_entity("circle-1", -9.9999, 0.0001);
    const auto& p = doc.feature_history.back().sketch_parameters.value();
    if (!expect(p.circles.empty() && p.arcs.size() == 1,
                "stage1: boundary click must leave exactly one arc")) {
      return false;
    }
    if (p.arcs.size() == 1) {
      if (!expect(std::abs(arc_sweep(p.arcs[0]) - kPi) < 0.02,
                  "stage1: boundary click should keep a half-circle arc")) {
        return false;
      }
    }
  }

  return true;
}

// ── Stage 2: Phase 1 constraint transfer ─────────────────────────

bool stage2_constraint_transfer() {
  // ── Sketch A: H badge + coincident + point-on-object anchor.
  // (The relation completion suppresses H/V badges while a relation
  // holds, so badge transfer is tested on a relation-free line.)
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");

    // L1 horizontal, crossed by L2 at x=50; L4 starts near the origin
    // with an explicit coincident to L1's start (distinct id — the
    // add-time snap only merges within 0.01 mm).
    manager.add_sketch_line(0.0, 0.0, 100.0, 0.0);
    manager.add_sketch_line(50.0, -10.0, 50.0, 10.0);
    manager.add_sketch_line(0.02, 0.0, 10.0, 40.0);

    auto doc = manager.set_sketch_line_constraint(
        "line-1", std::optional<std::string>("horizontal"));

    {
      const auto& p = doc.feature_history.back().sketch_parameters.value();
      const auto* l1 = find_line(p, "line-1");
      const auto* l3 = find_line(p, "line-3");
      if (!expect(l1 != nullptr && l3 != nullptr,
                  "stage2: setup A — lines 1 and 3 must exist")) {
        return false;
      }
      doc = manager.set_sketch_coincident_constraint(l1->start_vertex_id,
                                                     l3->start_vertex_id);
    }

    // Trim the right half of L1: it keeps its start, cut at x=50.
    doc = manager.trim_sketch_entity("line-1", 75.0, 0.0);

    const auto& p = doc.feature_history.back().sketch_parameters.value();
    const auto* l1 = find_line(p, "line-1");
    if (!expect(l1 != nullptr, "stage2: L1 must survive the trim")) {
      return false;
    }

    // H badge survives on the surviving piece.
    if (!expect(l1->constraint.has_value() &&
                    *l1->constraint == "horizontal",
                "stage2: horizontal badge must survive on the trimmed "
                "line")) {
      return false;
    }

    // Point-on-object anchor at the cut (the cut sits on cutter line-2).
    {
      bool found = false;
      for (const auto& a : p.point_line_anchors) {
        if (a.line_id == "line-2" && a.vertex_id == l1->end_vertex_id) {
          found = true;
          break;
        }
      }
      if (!expect(found,
                  "stage2: the cut endpoint must get a point-on-object "
                  "anchor onto the cutting line")) {
        return false;
      }
    }

    // Coincident (L1.start, line-3.start) survives: the record lists
    // the sharing LINES in target_ids and the merged point in the
    // constraint id; both lines still reference that point.
    {
      bool found = false;
      for (const auto& c : p.constraints) {
        if (c.kind != "coincident") continue;
        const bool a =
            std::find(c.target_ids.begin(), c.target_ids.end(),
                      "line-1") != c.target_ids.end();
        const bool b =
            std::find(c.target_ids.begin(), c.target_ids.end(),
                      "line-3") != c.target_ids.end();
        const std::string prefix = "constraint-coincident-";
        const bool point_match =
            c.constraint_id.rfind(prefix, 0) == 0 &&
            c.constraint_id.substr(prefix.size()) ==
                l1->start_vertex_id;
        if (a && b && point_match) found = true;
      }
      if (!expect(found,
                  "stage2: surviving coincident constraint must be "
                  "re-applied")) {
        return false;
      }
    }
  }

  // ── Sketch B: parallel / perpendicular / equal_length transfer.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");

    manager.add_sketch_line(0.0, 0.0, 100.0, 0.0);
    manager.add_sketch_line(50.0, -10.0, 50.0, 10.0);
    manager.add_sketch_line(0.0, 50.0, 100.0, 50.0);

    auto doc = manager.set_sketch_parallel_constraint(
        "line-1", std::optional<std::string>("line-3"));
    doc = manager.set_sketch_perpendicular_constraint(
        "line-1", std::optional<std::string>("line-2"));
    doc = manager.set_sketch_equal_length_constraint(
        "line-1", std::optional<std::string>("line-3"));

    // Trim the right half of L1.
    doc = manager.trim_sketch_entity("line-1", 75.0, 0.0);

    const auto& p = doc.feature_history.back().sketch_parameters.value();
    const auto* l1 = find_line(p, "line-1");
    if (!expect(l1 != nullptr, "stage2: L1 must survive the trim")) {
      return false;
    }

    auto relation_exists = [&](const std::string& kind, const std::string& a,
                               const std::string& b) {
      for (const auto& rel : p.line_relations) {
        if (rel.kind != kind) continue;
        if ((rel.first_line_id == a && rel.second_line_id == b) ||
            (rel.first_line_id == b && rel.second_line_id == a)) {
          return true;
        }
      }
      return false;
    };
    if (!expect(relation_exists("parallel", "line-1", "line-3"),
                "stage2: parallel relation must transfer")) return false;
    if (!expect(relation_exists("perpendicular", "line-1", "line-2"),
                "stage2: perpendicular relation must transfer")) return false;
    if (!expect(relation_exists("equal_length", "line-1", "line-3"),
                "stage2: equal_length relation must transfer")) return false;

    // The re-created equal_length is ENFORCED by the solver: the free
    // endpoints re-balance until both lines share one length (the
    // survivor's cut end is frozen; its free start and the partner
    // move). Equality holding is the contract — the exact value is
    // the solver's choice.
    {
      const auto* l3 = find_line(p, "line-3");
      if (!expect(l3 != nullptr, "stage2: L3 must exist")) {
        return false;
      }
      const double len1 = std::hypot(l1->end_x - l1->start_x,
                                     l1->end_y - l1->start_y);
      const double len3 = std::hypot(l3->end_x - l3->start_x,
                                     l3->end_y - l3->start_y);
      if (!expect(std::abs(len1 - len3) < 1.0,
                  "stage2: equal_length must be enforced by the solver "
                  "(|L1| == |L3| after the pass)")) {
        return false;
      }
    }
  }

  return true;
}

// ── Stage 3: Phase 1 driven dims + fillet cleanup ────────────────

bool stage3_driven_dims_fillet_cleanup() {
  // 3a. line_length dimension re-derives as driven + measured.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.add_sketch_line(0.0, 0.0, 100.0, 0.0);
    manager.add_sketch_line(50.0, -10.0, 50.0, 10.0);
    auto doc = manager.add_sketch_line_length_dimension("line-1");
    doc = manager.trim_sketch_entity("line-1", 75.0, 0.0);
    const auto& p = doc.feature_history.back().sketch_parameters.value();
    bool has_driven = false;
    bool has_driving = false;
    for (const auto& d : p.dimensions) {
      if (d.entity_id != "line-1" || d.kind != "line_length") continue;
      if (d.driven) {
        has_driven = has_driven ||
                     std::abs(d.value - 50.0) < 0.5;
      } else {
        has_driving = true;
      }
    }
    if (!expect(has_driven && !has_driving,
                "stage3: trimmed line keeps a DRIVEN measured length "
                "(50 mm), the driving dim is gone")) {
      return false;
    }
  }

  // 3b. circle_radius dimension becomes a driven arc_radius on the
  // surviving arc.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.add_sketch_circle(200.0, 0.0, 20.0);
    manager.add_sketch_line(190.0, -30.0, 190.0, 30.0);
    auto doc = manager.add_sketch_circle_radius_dimension("circle-1");
    doc = manager.trim_sketch_entity("circle-1", 200.0, 20.0);
    const auto& p = doc.feature_history.back().sketch_parameters.value();
    if (!expect(p.circles.empty() && p.arcs.size() == 1,
                "stage3: circle trim should leave one arc")) {
      return false;
    }
    bool found = false;
    for (const auto& d : p.dimensions) {
      if (d.kind == "arc_radius" && d.driven &&
          std::abs(d.value - 20.0) < 0.5) {
        found = true;
        break;
      }
    }
    if (!expect(found,
                "stage3: surviving arc must carry a driven arc_radius "
                "(20 mm)")) {
      return false;
    }
  }

  // 3c. arc_angle dimension re-derives as driven with the new sweep.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.add_sketch_arc(100.0, 0.0, 0.0, 100.0, 0.0, 0.0,
                           "center_start_end");
    auto doc = manager.add_sketch_arc_angle_dimension("arc-1");
    // The arc runs CCW 0..90 deg; a horizontal line at y=50 crosses at
    // 30 deg. Clicking near the 90 deg end deletes the 30..90 piece.
    manager.add_sketch_line(0.0, 50.0, 120.0, 50.0);
    doc = manager.trim_sketch_entity("arc-1", 0.0, 100.0);
    const auto& p = doc.feature_history.back().sketch_parameters.value();
    if (p.arcs.empty()) {
      return !expect(false, "stage3: the arc must survive the trim");
    }
    const bool ccw = p.arcs.front().ccw;
    if (!expect(ccw, "stage3: center_start_end arc should be CCW")) {
      return false;
    }
    bool found = false;
    for (const auto& d : p.dimensions) {
      if (d.kind == "arc_angle" && d.driven &&
          std::abs(d.value - kPi / 6.0) < 0.05) {
        found = true;
        break;
      }
    }
    if (!expect(found,
                "stage3: surviving arc must carry a driven arc_angle "
                "(30 deg in radians)")) {
      return false;
    }
  }

  // 3d. Fillet record dies with its operand.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    auto doc = manager.add_sketch_line(0.0, 0.0, 100.0, 0.0);
    doc = manager.add_sketch_line(100.0, 0.0, 100.0, 80.0);
    std::string corner_id;
    {
      const auto& p = doc.feature_history.back().sketch_parameters.value();
      for (const auto& v : p.vertices) {
        if (std::abs(v.x - 100.0) < 0.01 && std::abs(v.y - 0.0) < 0.01) {
          corner_id = v.id;
          break;
        }
      }
    }
    if (!expect(!corner_id.empty(), "stage3: fillet corner vertex found")) {
      return false;
    }
    doc = manager.add_sketch_fillet(corner_id, "line-1", "line-2", 5.0);
    {
      const auto& p = doc.feature_history.back().sketch_parameters.value();
      if (!expect(!p.fillets.empty(),
                  "stage3: fillet record created")) {
        return false;
      }
    }
    // Trim the vertical line away entirely (its only intersection is
    // the fillet arc's tangent endpoint) — the record must go.
    doc = manager.trim_sketch_entity("line-2", 100.0, 50.0);
    const auto& p = doc.feature_history.back().sketch_parameters.value();
    if (!expect(p.fillets.empty(),
                "stage3: fillet record must be removed when its operand "
                "dies")) {
      return false;
    }
  }

  return true;
}

// ── Stage 4: Phase 1 projection pruning ──────────────────────────

bool stage4_projection_pruning() {
  // Engine-level: hand-built feature, no document refresh, so the
  // assertions see exactly what the trim cleanup did to the records.
  {
    FeatureEntry feature;
    feature.kind = "sketch";
    feature.sketch_parameters = SketchFeatureParameters{};
    auto& p = *feature.sketch_parameters;
    p.next_vertex_index = 100;
    p.lines.push_back(SketchLine{
        .id = "line-1",
        .start_vertex_id = "v-1",
        .end_vertex_id = "v-2",
        .start_x = 0.0, .start_y = 0.0,
        .end_x = 100.0, .end_y = 0.0,
        .constraint = std::nullopt,
        .is_construction = false,
    });
    p.lines.push_back(SketchLine{
        .id = "line-2",
        .start_vertex_id = "v-3",
        .end_vertex_id = "v-4",
        .start_x = 0.0, .start_y = 50.0,
        .end_x = 100.0, .end_y = 50.0,
        .constraint = std::nullopt,
        .is_construction = false,
    });
    p.projections.push_back(SketchProjection{
        .id = "proj-1",
        .source_id = "body-1",
        .source_kind = "edge",
        .generated_line_ids = {"line-1", "line-2"},
    });

    polysmith::core::trim_sketch_entity(feature, "line-1", 50.0, 0.0);

    if (!expect(p.lines.size() == 1,
                "stage4: isolated line-1 deleted by the trim")) {
      return false;
    }
    bool pruned = true;
    for (const auto& proj : p.projections) {
      if (std::find(proj.generated_line_ids.begin(),
                    proj.generated_line_ids.end(),
                    "line-1") != proj.generated_line_ids.end()) {
        pruned = false;
      }
    }
    if (!expect(pruned,
                "stage4: no projection record may still list the deleted "
                "line-1")) {
      return false;
    }
    if (!expect(p.projections.size() == 1 &&
                    p.projections[0].generated_line_ids ==
                        std::vector<std::string>{"line-2"},
                "stage4: the record survives with only line-2")) {
      return false;
    }
  }

  // Surviving trim also prunes (the live link is broken either way).
  {
    FeatureEntry feature;
    feature.kind = "sketch";
    feature.sketch_parameters = SketchFeatureParameters{};
    auto& p = *feature.sketch_parameters;
    p.next_vertex_index = 100;
    p.lines.push_back(SketchLine{
        .id = "line-1",
        .start_vertex_id = "v-1",
        .end_vertex_id = "v-2",
        .start_x = 0.0, .start_y = 0.0,
        .end_x = 100.0, .end_y = 0.0,
        .constraint = std::nullopt,
        .is_construction = false,
    });
    p.lines.push_back(SketchLine{
        .id = "line-2",
        .start_vertex_id = "v-3",
        .end_vertex_id = "v-4",
        .start_x = 50.0, .start_y = -10.0,
        .end_x = 50.0, .end_y = 10.0,
        .constraint = std::nullopt,
        .is_construction = false,
    });
    p.projections.push_back(SketchProjection{
        .id = "proj-1",
        .source_id = "body-1",
        .source_kind = "edge",
        .generated_line_ids = {"line-1", "line-2"},
    });

    polysmith::core::trim_sketch_entity(feature, "line-1", 75.0, 0.0);

    if (!expect(p.lines.size() == 2,
                "stage4: trimmed line-1 survives (shortened)")) {
      return false;
    }
    bool pruned = true;
    for (const auto& proj : p.projections) {
      if (std::find(proj.generated_line_ids.begin(),
                    proj.generated_line_ids.end(),
                    "line-1") != proj.generated_line_ids.end()) {
        pruned = false;
      }
    }
    if (!expect(pruned,
                "stage4: even a surviving trim prunes the entity from "
                "projection records")) {
      return false;
    }
  }

  return true;
}

// ── Stage 5: Phase 2 corner trim ─────────────────────────────────

bool stage5_corner_trim() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // L1 ends at (100,0); L2 starts above it at (100,50). The virtual
  // corner is (100,0): L1 stays, L2 extends down to it.
  manager.add_sketch_line(0.0, 0.0, 100.0, 0.0);
  manager.add_sketch_line(100.0, 50.0, 100.0, 100.0);

  // Hover preview on the second pick.
  {
    const auto doc = manager.get_document().value();
    const auto& feature = doc.feature_history.back();
    const auto preview = polysmith::core::corner_trim_preview(
        feature, "line-1", "line-2", 105.0, 5.0);
    if (!expect(preview.valid,
                "stage5: corner preview must be valid for crossing lines")) {
      return false;
    }
    if (!expect(std::abs(preview.corner_x - 100.0) < 0.01 &&
                    std::abs(preview.corner_y - 0.0) < 0.01,
                "stage5: preview corner at (100,0)")) {
      return false;
    }
    if (!expect(preview.a.kind == "line" && preview.b.kind == "line",
                "stage5: preview segments are lines")) {
      return false;
    }
    if (!expect(std::abs(preview.b.sx - 100.0) < 0.01 &&
                    std::abs(preview.b.sy - 0.0) < 0.01 &&
                    std::abs(preview.b.ex - 100.0) < 0.01 &&
                    std::abs(preview.b.ey - 100.0) < 0.01,
                "stage5: L2 preview extends to (100,0)")) {
      return false;
    }
  }

  auto doc = manager.corner_trim_sketch_entities("line-1", "line-2",
                                                 105.0, 5.0);
  const auto& p = doc.feature_history.back().sketch_parameters.value();
  const auto* l1 = find_line(p, "line-1");
  const auto* l2 = find_line(p, "line-2");
  if (!expect(l1 != nullptr && l2 != nullptr,
              "stage5: both lines survive")) {
    return false;
  }
  if (!expect(std::abs(l1->end_x - 100.0) < 0.01 &&
                  std::abs(l2->start_x - 100.0) < 0.01 &&
                  std::abs(l2->start_y - 0.0) < 0.01,
              "stage5: corner joined at (100,0)")) {
    return false;
  }
  if (!expect(l1->end_vertex_id == l2->start_vertex_id,
              "stage5: the two corner endpoints share one vertex id")) {
    return false;
  }

  // Parallel lines have no virtual corner — the command throws.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.add_sketch_line(0.0, 0.0, 100.0, 0.0);
    manager.add_sketch_line(0.0, 50.0, 100.0, 50.0);
    bool threw = false;
    try {
      manager.corner_trim_sketch_entities("line-1", "line-2", 50.0, 25.0);
    } catch (const std::exception&) {
      threw = true;
    }
    if (!expect(threw,
                "stage5: parallel lines must throw (no virtual corner)")) {
      return false;
    }
  }

  // Construction entities cannot be corner-trimmed.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.add_sketch_line(0.0, 0.0, 100.0, 0.0);
    manager.add_sketch_line(100.0, 50.0, 100.0, 100.0, true);
    bool threw = false;
    try {
      manager.corner_trim_sketch_entities("line-1", "line-2", 105.0, 5.0);
    } catch (const std::exception&) {
      threw = true;
    }
    if (!expect(threw,
                "stage5: construction entities must throw")) {
      return false;
    }
  }

  return true;
}

// ── Stage 6: Phase 2 extend ──────────────────────────────────────

bool stage6_extend() {
  // 6a. Line extends to the first intersection.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.add_sketch_line(0.0, 0.0, 50.0, 0.0);
    manager.add_sketch_line(100.0, -10.0, 100.0, 10.0);
    const auto doc = manager.extend_sketch_entity("line-1", 40.0, 0.0);
    const auto& p = doc.feature_history.back().sketch_parameters.value();
    const auto* l = find_line(p, "line-1");
    if (!expect(l != nullptr && std::abs(l->end_x - 100.0) < 0.01 &&
                    std::abs(l->end_y - 0.0) < 0.01,
                "stage6: line should extend to the crossing at (100,0)")) {
      return false;
    }
  }

  // 6b. Arc extends along its circle to the first crossing.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    // CCW arc r=100 from 0 deg to 30 deg (center at the origin).
    manager.add_sketch_arc(100.0, 0.0, 86.602540378443864, 50.0,
                           0.0, 0.0, "center_start_end");
    // Vertical line crossing the arc's circle at (76.6, +-64.3).
    manager.add_sketch_line(76.6044443118976, -80.0, 76.6044443118976,
                            80.0);
    const auto doc = manager.extend_sketch_entity("arc-1", 86.0, 48.0);
    const auto& p = doc.feature_history.back().sketch_parameters.value();
    if (!expect(p.arcs.size() == 1, "stage6: the arc must survive")) {
      return false;
    }
    const auto& a = p.arcs[0];
    const double ex = a.end_x, ey = a.end_y;
    const bool near_pos =
        std::abs(ex - 76.6044443118976) < 0.1 && std::abs(ey - 64.28) < 0.1;
    const bool near_neg =
        std::abs(ex - 76.6044443118976) < 0.1 && std::abs(ey + 64.28) < 0.1;
    if (!expect(a.ccw ? near_pos : near_neg,
                "stage6: arc should extend to the crossing on its circle "
                "in the sweep direction")) {
      return false;
    }
  }

  return true;
}

// ── Stage 7: Phase 2 split ───────────────────────────────────────

bool stage7_split() {
  // 7a. Line splits at all intersections into three pieces.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.add_sketch_line(0.0, 0.0, 100.0, 0.0);
    manager.add_sketch_line(40.0, -10.0, 40.0, 10.0);
    manager.add_sketch_line(70.0, -10.0, 70.0, 10.0);
    const auto doc = manager.split_sketch_entity("line-1", 50.0, 0.0,
                                                 0.0, 0.0);
    const auto& p = doc.feature_history.back().sketch_parameters.value();
    // L1 becomes 3 pieces; the 2 crossing lines stay → 5 lines.
    if (!expect(p.lines.size() == 5,
                "stage7: the split should produce 5 lines total "
                "(3 pieces + 2 cutters)")) {
      return false;
    }
    if (!expect(line_with_endpoints(p, 0.0, 0.0, 40.0, 0.0, 0.01),
                "stage7: piece (0,0)-(40,0) missing")) return false;
    if (!expect(line_with_endpoints(p, 40.0, 0.0, 70.0, 0.0, 0.01),
                "stage7: piece (40,0)-(70,0) missing")) return false;
    if (!expect(line_with_endpoints(p, 70.0, 0.0, 100.0, 0.0, 0.01),
                "stage7: piece (70,0)-(100,0) missing")) return false;
  }

  // 7b. Circle splits at two clicks into two arcs sharing vertices.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.add_sketch_circle(0.0, 0.0, 10.0);
    const auto doc = manager.split_sketch_entity("circle-1", 10.0, 0.0,
                                                 0.0, 10.0);
    const auto& p = doc.feature_history.back().sketch_parameters.value();
    if (!expect(p.circles.empty() && p.arcs.size() == 2,
                "stage7: circle split should leave two arcs")) {
      return false;
    }
    if (p.arcs.size() == 2) {
      const auto& a1 = p.arcs[0];
      const auto& a2 = p.arcs[1];
      if (!expect(std::abs(a1.start_x - 10.0) < 0.01 &&
                      std::abs(a1.start_y) < 0.01 &&
                      std::abs(a1.end_x) < 0.01 &&
                      std::abs(a1.end_y - 10.0) < 0.01,
                  "stage7: arc 1 from (10,0) to (0,10)")) {
        return false;
      }
      if (!expect(a1.end_vertex_id == a2.start_vertex_id &&
                      a1.start_vertex_id == a2.end_vertex_id,
                  "stage7: the two arcs share their endpoint vertices")) {
        return false;
      }
    }
  }

  // 7c. No intersections -> the split throws.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.add_sketch_line(0.0, 0.0, 50.0, 0.0);
    bool threw = false;
    try {
      manager.split_sketch_entity("line-1", 25.0, 0.0, 0.0, 0.0);
    } catch (const std::exception&) {
      threw = true;
    }
    if (!expect(threw,
                "stage7: split without intersections must throw")) {
      return false;
    }
  }

  return true;
}

// ── Stage 8: Phase 3 drag-paint stroke with one undo entry ───────

bool stage8_stroke_one_undo() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_line(0.0, 0.0, 100.0, 0.0);
  manager.add_sketch_line(30.0, -10.0, 30.0, 10.0);
  manager.add_sketch_line(60.0, -10.0, 60.0, 10.0);

  // One stroke: trim L1's middle segment (click at 50) and L3's lower
  // segment (click at -5).
  std::vector<TrimStrokeEntry> entries{
      TrimStrokeEntry{.entity_id = "line-1", .click_x = 50.0, .click_y = 0.0},
      TrimStrokeEntry{.entity_id = "line-3", .click_x = 60.0, .click_y = -5.0},
  };
  const auto doc = manager.trim_sketch_stroke(entries);

  {
    const auto& p = doc.feature_history.back().sketch_parameters.value();
    // L1's middle died: the survivors are (0,0)-(30,0) and (60,0)-(100,0).
    if (!expect(line_with_endpoints(p, 0.0, 0.0, 30.0, 0.0, 0.01) &&
                    line_with_endpoints(p, 60.0, 0.0, 100.0, 0.0, 0.01),
                "stage8: stroke should trim L1 into two survivors")) {
      return false;
    }
    const auto* l3 = find_line(p, "line-3");
    if (!expect(l3 != nullptr &&
                    std::abs(l3->start_y - 0.0) < 0.01 &&
                    std::abs(l3->end_y - 10.0) < 0.01,
                "stage8: stroke should trim L3's lower segment away")) {
      return false;
    }
  }

  // ONE undo restores the whole pre-stroke sketch — a per-entry sub-
  // undo stack would leave the stroke half-done after one undo().
  {
    const auto undone = manager.undo();
    const auto& p = undone.feature_history.back().sketch_parameters.value();
    if (!expect(p.lines.size() == 3,
                "stage8: one undo() must restore all three pre-stroke "
                "lines")) {
      return false;
    }
    if (!expect(find_line(p, "line-1") != nullptr &&
                    std::abs(find_line(p, "line-1")->end_x - 100.0) < 0.01,
                "stage8: one undo() must restore L1 whole")) {
      return false;
    }
  }

  return true;
}

// ── Runner ────────────────────────────────────────────────────────

struct Stage {
  int number;
  const char* name;
  bool (*run)();
};

const Stage kStages[] = {
    {1, "Phase 0 — trim behaviors (line, circle, delete, construction "
        "cutter, boundary click)", stage1_trim_behaviors},
    {2, "Phase 1 — constraint transfer (H/V, relations, coincident, "
        "anchors)", stage2_constraint_transfer},
    {3, "Phase 1 — driven dimension re-derivation + fillet record "
        "cleanup", stage3_driven_dims_fillet_cleanup},
    {4, "Phase 1 — projection generated-id pruning",
     stage4_projection_pruning},
    {5, "Phase 2 — corner trim (virtual corner + preview)",
     stage5_corner_trim},
    {6, "Phase 2 — extend", stage6_extend},
    {7, "Phase 2 — split", stage7_split},
    {8, "Phase 3 — drag-paint stroke with one undo entry",
     stage8_stroke_one_undo},
};

}  // namespace

int main(int argc, char** argv) {
  int only_stage = -1;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--list") {
      for (const auto& s : kStages) {
        std::cout << s.number << ": " << s.name << "\n";
      }
      return 0;
    }
    if (arg == "--stage" && i + 1 < argc) {
      only_stage = std::atoi(argv[++i]);
    }
  }

  try {
    int failed = 0;
    for (const auto& s : kStages) {
      if (only_stage != -1 && s.number != only_stage) continue;
      std::cout << "stage " << s.number << " — " << s.name << "\n";
      if (!s.run()) {
        std::cerr << "stage " << s.number << " FAILED\n";
        failed++;
      } else {
        std::cout << "stage " << s.number << " passed\n";
      }
    }
    if (failed != 0) {
      std::cerr << failed << " stage(s) failed\n";
      return 1;
    }
    std::cout << "trim_stages_test passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;
  }
}
