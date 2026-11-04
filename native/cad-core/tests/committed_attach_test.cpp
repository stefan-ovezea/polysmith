// A line the USER drew onto geometry stays attached - from the GEOMETRY.
//
// Fusion behaves this way: start a line on another line's midpoint and it is held
// there; you never had to notice a hint. PolySmith used to make that depend on a snap
// flavour surviving draft start -> commit -> dispatch, so a lost flag left two lines
// that merely touched. The reported symptom was exact: a T-junction whose stem starts
// on the host's midpoint had "midpoint_anchors": [] in the saved document, dragging the
// stem detached it and it stayed detached, and "Merge coincident points" had nothing to
// merge because nothing had ever been bound.
//
// The attachment is OPT-IN (`attach_to_geometry`), because running it unconditionally
// broke four suites - trim, CAM, a partial-event test and auto-constrain all create lines
// through the same function and must not gain anchors. The last case below is that
// guard: with the flag off, nothing is attached.
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

#include "core/document/document.h"
#include "core/sketch/sketch_feature.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::SketchFeatureParameters;

int failures = 0;

bool expect(bool condition, const char* what) {
  if (!condition) {
    std::cout << "FAIL: " << what << "\n";
    ++failures;
    return false;
  }
  return true;
}

const SketchFeatureParameters* sketch_of(const DocumentState& document) {
  for (const auto& feature : document.feature_history) {
    if (feature.sketch_parameters.has_value()) return &feature.sketch_parameters.value();
  }
  return nullptr;
}

bool vertex_xy(const SketchFeatureParameters& parameters, const std::string& id,
               double& x, double& y) {
  for (const auto& vertex : parameters.vertices) {
    if (vertex.id != id) continue;
    x = vertex.x;
    y = vertex.y;
    return true;
  }
  return false;
}

// A horizontal host from (0,0) to (100,0), then a stem from (stem_x, stem_y).
DocumentState host_then_stem(DocumentManager& manager, double stem_x, double stem_y,
                             bool attach) {
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_line(0.0, 0.0, 100.0, 0.0, false, "", attach);
  return manager.add_sketch_line(stem_x, stem_y, stem_x + 10.0, stem_y + 40.0, false,
                                 "", attach);
}

bool test_a_stem_on_the_midpoint_is_attached() {
  DocumentManager manager;
  const DocumentState document = host_then_stem(manager, 50.0, 0.0, true);
  const SketchFeatureParameters* parameters = sketch_of(document);
  if (!expect(parameters != nullptr, "T: the sketch exists")) return false;
  if (!expect(parameters->midpoint_anchors.size() == 1,
              ("T: one midpoint anchor - got " +
               std::to_string(parameters->midpoint_anchors.size()))
                  .c_str())) {
    return false;
  }
  return expect(parameters->midpoint_anchors.front().line_id == "line-1",
                "T: anchored to the host line");
}

bool test_the_attached_end_follows_the_host() {
  // The point of the anchor: stretch the host and the stem's end rides to the new
  // midpoint instead of staying behind.
  DocumentManager manager;
  DocumentState document = host_then_stem(manager, 50.0, 0.0, true);
  const SketchFeatureParameters* before = sketch_of(document);
  if (!expect(before != nullptr && before->midpoint_anchors.size() == 1,
              "follow: the anchor exists")) {
    return false;
  }
  const std::string stem_vertex = before->midpoint_anchors.front().vertex_id;

  document = manager.update_sketch_vertex("vertex-2", 160.0, 0.0);
  const SketchFeatureParameters* after = sketch_of(document);
  if (!expect(after != nullptr, "follow: the sketch survives")) return false;
  double stem_x = 0.0;
  double stem_y = 0.0;
  bool found = false;
  for (const auto& vertex : after->vertices) {
    if (vertex.id == stem_vertex) {
      stem_x = vertex.x;
      stem_y = vertex.y;
      found = true;
    }
  }
  if (!expect(found, "follow: the attached vertex still exists")) return false;
  return expect(std::abs(stem_x - 80.0) < 1e-6 && std::abs(stem_y) < 1e-6,
                ("follow: the attached end rides to the new midpoint (80,0) - got (" +
                 std::to_string(stem_x) + ", " + std::to_string(stem_y) + ")")
                    .c_str());
}

bool test_a_stem_off_the_middle_gets_a_fractional_anchor() {
  DocumentManager manager;
  const DocumentState document = host_then_stem(manager, 30.0, 0.0, true);
  const SketchFeatureParameters* parameters = sketch_of(document);
  if (!expect(parameters != nullptr, "t: the sketch exists")) return false;
  if (!expect(parameters->point_line_anchors.size() == 1,
              ("t: one point-on-line anchor - got " +
               std::to_string(parameters->point_line_anchors.size()))
                  .c_str())) {
    return false;
  }
  const auto& anchor = parameters->point_line_anchors.front();
  return expect(std::abs(anchor.t - 0.3) < 1e-6,
                ("t: anchored at the clicked fraction 0.3 - got " +
                 std::to_string(anchor.t))
                    .c_str());
}

bool test_a_stem_far_from_the_host_is_not_attached() {
  DocumentManager manager;
  const DocumentState document = host_then_stem(manager, 50.0, 25.0, true);
  const SketchFeatureParameters* parameters = sketch_of(document);
  if (!expect(parameters != nullptr, "far: the sketch exists")) return false;
  return expect(parameters->midpoint_anchors.empty() &&
                    parameters->point_line_anchors.empty(),
                "far: a line in open space stays free");
}

bool test_a_stem_at_the_host_endpoint_is_not_anchored() {
  // A coincidence, handled by endpoint sharing. An anchor as well would give the
  // solver two constraints for one fact.
  DocumentManager manager;
  const DocumentState document = host_then_stem(manager, 0.0, 0.0, true);
  const SketchFeatureParameters* parameters = sketch_of(document);
  if (!expect(parameters != nullptr, "endpoint: the sketch exists")) return false;
  return expect(parameters->midpoint_anchors.empty() &&
                    parameters->point_line_anchors.empty(),
                "endpoint: a shared endpoint is not also anchored");
}

bool test_attaching_is_idempotent() {
  DocumentManager manager;
  DocumentState document = host_then_stem(manager, 50.0, 0.0, true);
  document = manager.update_sketch_vertex("vertex-3", 55.0, 10.0);
  const SketchFeatureParameters* parameters = sketch_of(document);
  if (!expect(parameters != nullptr, "idempotent: the sketch exists")) return false;
  return expect(parameters->midpoint_anchors.size() == 1,
                ("idempotent: still one anchor - got " +
                 std::to_string(parameters->midpoint_anchors.size()))
                    .c_str());
}

bool test_without_the_flag_nothing_is_attached() {
  // The guard for the four suites this broke when it ran unconditionally: trim, CAM,
  // import and auto-constrain create lines through the same function and must keep
  // their exact previous behaviour.
  DocumentManager manager;
  const DocumentState document = host_then_stem(manager, 50.0, 0.0, false);
  const SketchFeatureParameters* parameters = sketch_of(document);
  if (!expect(parameters != nullptr, "flag off: the sketch exists")) return false;
  return expect(parameters->midpoint_anchors.empty() &&
                    parameters->point_line_anchors.empty(),
                "flag off: no anchor is invented");
}

bool test_a_stem_on_a_circle_boundary_is_attached() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 10.0);
  const DocumentState document =
      manager.add_sketch_line(6.0, 8.0, 16.0, 18.0, false, "", true);
  const SketchFeatureParameters* parameters = sketch_of(document);
  if (!expect(parameters != nullptr, "circle: the sketch exists")) return false;

  const std::string& line_start = parameters->lines.back().start_vertex_id;
  const std::string& circle_id = parameters->circles.front().id;

  const std::vector<std::string>* targets = nullptr;
  int count = 0;
  for (const auto& constraint : parameters->constraints) {
    if (constraint.kind != "point_on_circle") continue;
    ++count;
    targets = &constraint.target_ids;
  }
  if (!expect(count == 1,
              ("circle: exactly one point_on_circle constraint - got " +
               std::to_string(count))
                  .c_str())) {
    return false;
  }
  if (!expect(targets != nullptr && targets->size() == 2,
              "circle: the constraint names a vertex and a circle")) {
    return false;
  }
  if (!expect((*targets)[0] == line_start,
              "circle: the bound vertex is the line start")) {
    return false;
  }
  return expect((*targets)[1] == circle_id, "circle: the host is the circle");
}

bool test_without_the_flag_a_circle_is_not_attached() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 10.0);
  const DocumentState document =
      manager.add_sketch_line(6.0, 8.0, 16.0, 18.0, false, "", false);
  const SketchFeatureParameters* parameters = sketch_of(document);
  if (!expect(parameters != nullptr, "circle flag off: sketch exists")) {
    return false;
  }
  for (const auto& constraint : parameters->constraints) {
    if (constraint.kind == "point_on_circle") {
      return expect(false, "circle flag off: no point_on_circle is invented");
    }
  }
  return true;
}

bool test_the_attached_end_stays_on_the_circle_after_an_edit() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 10.0);
  DocumentState document =
      manager.add_sketch_line(6.0, 8.0, 16.0, 18.0, false, "", true);
  const SketchFeatureParameters* before = sketch_of(document);
  if (!expect(before != nullptr, "follow: the sketch exists")) return false;
  const std::string center_vertex = before->circles.front().center_vertex_id;

  // Move the circle centre; the centre must STAY where it was dragged (not snap
  // back toward the point), and the line start must follow to stay on the boundary.
  document = manager.update_sketch_vertex(center_vertex, 5.0, 0.0);
  const SketchFeatureParameters* after = sketch_of(document);
  if (!expect(after != nullptr, "follow: the sketch survives")) return false;

  const auto& line = after->lines.back();
  const auto& circle = after->circles.front();
  if (!expect(std::abs(circle.center_x - 5.0) < 1e-3 &&
                  std::abs(circle.center_y) < 1e-3,
              ("follow: the centre stays at the dragged position - got (" +
               std::to_string(circle.center_x) + ", " +
               std::to_string(circle.center_y) + ")").c_str())) {
    return false;
  }
  // The line start was at (6,8) relative to the centre at (0,0); a move of the
  // centre to (5,0) must translate the point the same way — (11,8) — so it keeps
  // its angle around the circle (the "snapped to a different vertex" complaint).
  const double dist = std::hypot(line.start_x - 5.0, line.start_y - 0.0);
  return expect(
      std::abs(dist - circle.radius) < 1e-3 &&
          std::abs(line.start_x - 11.0) < 1e-3 &&
          std::abs(line.start_y - 8.0) < 1e-3,
      ("follow: the line start rides the circle to its same relative spot (11,8) - got (" +
       std::to_string(line.start_x) + ", " + std::to_string(line.start_y) + ")")
          .c_str());
}

bool test_a_stem_drawn_slightly_off_the_circle_is_pulled_onto_it() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 10.0);
  // (8, 6.1) is ~0.06 mm outside the boundary — inside the 0.25 mm attach
  // tolerance, but NOT exactly on the circle, so the solver must pull it in.
  const DocumentState document =
      manager.add_sketch_line(8.0, 6.1, 18.0, 8.0, false, "", true);
  const SketchFeatureParameters* parameters = sketch_of(document);
  if (!expect(parameters != nullptr, "off-circle: the sketch exists")) {
    return false;
  }
  const auto& line = parameters->lines.back();
  const auto& circle = parameters->circles.front();
  const double dist =
      std::hypot(line.start_x - circle.center_x, line.start_y - circle.center_y);
  return expect(
      std::abs(dist - circle.radius) < 1e-3,
      ("off-circle: the endpoint is pulled onto the circle (dist " +
       std::to_string(dist) + " vs radius " + std::to_string(circle.radius) +
       ")").c_str());
}

// A line started on a circle's boundary takes a fresh vertex and is bound by
// point_on_circle like any other point on the circle. (Historically the endpoint
// adopted the derived quadrant vertex id, which tied it to the circle by a shared
// name instead of a constraint — and `vertex_is_derived` then skipped attaching it.)
bool test_a_line_drawn_on_a_circle_cardinal_point_is_bound_by_a_constraint() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 10.0);
  // Start exactly on the boundary at 0° (10,0) — the historical share-the-id case.
  const DocumentState document =
      manager.add_sketch_line(10.0, 0.0, 30.0, 0.0, false, "", true);
  const SketchFeatureParameters* parameters = sketch_of(document);
  if (!expect(parameters != nullptr, "circle attach: the sketch exists")) return false;

  const auto& line = parameters->lines.back();
  if (!expect(line.start_vertex_id.rfind("vertex-circle-", 0) != 0,
              ("circle attach: the endpoint owns its vertex - got " +
               line.start_vertex_id)
                  .c_str())) {
    return false;
  }

  const std::string circle_id = parameters->circles.front().id;
  const auto constraint_it = std::find_if(
      parameters->constraints.begin(), parameters->constraints.end(),
      [&](const auto& constraint) {
        return constraint.kind == "point_on_circle" &&
               constraint.target_ids.size() >= 2 &&
               constraint.target_ids[0] == line.start_vertex_id &&
               constraint.target_ids[1] == circle_id;
      });
  return expect(constraint_it != parameters->constraints.end(),
                "circle attach: the endpoint is bound by a point_on_circle constraint");
}

// A line drawn onto an ARC's body (not an endpoint) is bound by
// point_on_arc, mirroring the circle's point_on_circle. planegcs has no native
// PointOnArc, so the solver pins the circumcircle radius and
// enforce_point_on_arc_constraints keeps the point on the swept span after each
// solve.
bool test_a_line_drawn_on_an_arc_body_is_bound_by_point_on_arc() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  // Quarter arc centred at the origin: (10,0) → (0,10) through (7.07,7.07).
  manager.add_sketch_arc(10.0, 0.0, 0.0, 10.0, 7.0710678, 7.0710678,
                         "three_point");
  // Start exactly on the arc body (45°) — not one of its endpoints.
  const DocumentState document =
      manager.add_sketch_line(7.0710678, 7.0710678, 20.0, 20.0, false, "", true);
  const SketchFeatureParameters* parameters = sketch_of(document);
  if (!expect(parameters != nullptr, "arc attach: the sketch exists")) return false;

  const auto& line = parameters->lines.back();
  const std::string arc_id = parameters->arcs.front().id;
  const auto constraint_it = std::find_if(
      parameters->constraints.begin(), parameters->constraints.end(),
      [&](const auto& constraint) {
        return constraint.kind == "point_on_arc" &&
               constraint.target_ids.size() >= 2 &&
               constraint.target_ids[0] == line.start_vertex_id &&
               constraint.target_ids[1] == arc_id;
      });
  return expect(constraint_it != parameters->constraints.end(),
                "arc attach: the endpoint is bound by a point_on_arc constraint");
}

// A line drawn onto an arc rides it when the arc's centre moves — the same
// contract a circle gives point_on_circle. Plain arcs are not in the solver,
// so the centre-drag translation of point_on_arc points plus the enforcement
// pass are what keep the point on the arc.
bool test_a_line_drawn_on_an_arc_follows_the_arc() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_arc(10.0, 0.0, 0.0, 10.0, 7.0710678, 7.0710678,
                         "three_point");
  DocumentState document =
      manager.add_sketch_line(7.0710678, 7.0710678, 20.0, 20.0, false, "", true);
  const SketchFeatureParameters* before = sketch_of(document);
  if (!expect(before != nullptr, "arc follow: the sketch exists")) return false;
  const std::string center_vertex = before->arcs.front().center_vertex_id;

  // Move the arc's centre by (+5, 0); the line start rides to (12.07, 7.07).
  document = manager.update_sketch_vertex(center_vertex, 5.0, 0.0);
  const SketchFeatureParameters* after = sketch_of(document);
  if (!expect(after != nullptr, "arc follow: the sketch survives")) return false;
  const auto& line = after->lines.back();
  return expect(
      std::abs(line.start_x - 12.0710678) < 1e-2 &&
          std::abs(line.start_y - 7.0710678) < 1e-2,
      ("arc follow: the line start rides the arc to (12.07,7.07) - got (" +
       std::to_string(line.start_x) + ", " + std::to_string(line.start_y) + ")")
          .c_str());
}

// A line drawn FROM a circle keeps its endpoint on the boundary. The endpoint
// owns a real vertex and carries a point_on_circle, so it rides the circle like
// any other attached point. Regression: "line from circle does not stay attached
// when I move the circle".
bool test_a_line_drawn_from_a_circle_follows_the_circle() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 10.0);
  // Start exactly on the boundary at 0° (10,0); the line extends to the right.
  DocumentState document =
      manager.add_sketch_line(10.0, 0.0, 30.0, 0.0, false, "", true);
  const SketchFeatureParameters* before = sketch_of(document);
  if (!expect(before != nullptr, "circle attach: the sketch exists")) return false;
  const std::string center_vertex = before->circles.front().center_vertex_id;

  document = manager.update_sketch_vertex(center_vertex, 5.0, 0.0);
  const SketchFeatureParameters* after = sketch_of(document);
  if (!expect(after != nullptr, "circle attach: the sketch survives")) return false;
  const auto& line = after->lines.back();
  return expect(
      std::abs(line.start_x - 15.0) < 1e-3 && std::abs(line.start_y) < 1e-3,
      ("circle attach: the line start follows the circle to (15,0) - got (" +
       std::to_string(line.start_x) + ", " + std::to_string(line.start_y) + ")")
          .c_str());
}

// …and a RESIZE too. The radial projection in update_sketch_circle only walks
// point_on_circle points, so an endpoint without one would be left at the old
// radius — the line detaching from the circle the moment the circle grew.
bool test_a_line_drawn_from_a_circle_follows_a_resize() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 10.0);
  DocumentState document =
      manager.add_sketch_line(10.0, 0.0, 30.0, 0.0, false, "", true);
  const SketchFeatureParameters* before = sketch_of(document);
  if (!expect(before != nullptr, "circle resize: the sketch exists")) return false;
  const std::string circle_id = before->circles.front().id;

  document = manager.update_sketch_circle(circle_id, 0.0, 0.0, 20.0);
  const SketchFeatureParameters* after = sketch_of(document);
  if (!expect(after != nullptr, "circle resize: the sketch survives")) return false;
  const auto& line = after->lines.back();
  return expect(
      std::abs(line.start_x - 20.0) < 1e-1 && std::abs(line.start_y) < 1e-1,
      ("circle resize: the start follows to (20,0) - got (" +
       std::to_string(line.start_x) + ", " + std::to_string(line.start_y) + ")")
          .c_str());
}

// Resizing a circle with an attached line endpoint must keep the CENTRE fixed
// and ride only the endpoint (SolidCraft: centre fixed, line length modifies).
// The solver maps point_on_circle to P2PDistance(point, centre, radius); with
// both the centre and the point free, DogLeg's minimum-move splits the radius
// correction between them, so enlarging a circle also drifts its centre —
// "modifying both geometries at once" (the reported unpredictable result).
bool test_resizing_a_circle_keeps_the_centre_fixed() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 10.0);
  // (6,8) is on the boundary (dist 10) — a fresh vertex with point_on_circle.
  DocumentState document =
      manager.add_sketch_line(6.0, 8.0, 16.0, 18.0, false, "", true);
  const SketchFeatureParameters* before = sketch_of(document);
  if (!expect(before != nullptr, "resize: the sketch exists")) return false;
  const std::string circle_id = before->circles.front().id;

  // Enlarge radius 10 -> 20, centre stays (0,0).
  document = manager.update_sketch_circle(circle_id, 0.0, 0.0, 20.0);
  const SketchFeatureParameters* after = sketch_of(document);
  if (!expect(after != nullptr, "resize: the sketch survives")) return false;

  const auto& circle = after->circles.front();
  if (!expect(std::abs(circle.center_x) < 1e-3 && std::abs(circle.center_y) < 1e-3,
              ("resize: the centre stays fixed at (0,0) - got (" +
               std::to_string(circle.center_x) + ", " +
               std::to_string(circle.center_y) + ")").c_str())) {
    return false;
  }
  const auto& line = after->lines.back();
  const double dist =
      std::hypot(line.start_x - circle.center_x, line.start_y - circle.center_y);
  return expect(
      std::abs(dist - 20.0) < 1e-3,
      ("resize: the attached endpoint rides out to radius 20 - got dist " +
       std::to_string(dist)).c_str());
}

// Resizing a circle must also re-derive its PROFILE region (the extrudable
// surface) to the new radius. The extrude body is correct because it re-reads
// the wire from the circle, but the profile fill/highlight was reported as
// stuck at the original circle size.
bool test_resizing_a_circle_updates_its_profile() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  const DocumentState initial = manager.add_sketch_circle(0.0, 0.0, 10.0);
  const SketchFeatureParameters* before = sketch_of(initial);
  if (!expect(before != nullptr, "profile: the sketch exists")) return false;
  const std::string circle_id = before->circles.front().id;

  const DocumentState document =
      manager.update_sketch_circle(circle_id, 0.0, 0.0, 20.0);
  const SketchFeatureParameters* after = sketch_of(document);
  if (!expect(after != nullptr, "profile: the sketch survives")) return false;

  for (const auto& profile : after->profiles) {
    if (profile.kind != "circle") continue;
    return expect(
        std::abs(profile.radius - 20.0) < 1e-3,
        ("profile: the circle profile re-derives to radius 20 - got " +
         std::to_string(profile.radius)).c_str());
  }
  return expect(false, "profile: the circle profile was not found");
}

// Resizing a circle with an attached point AND a driving line-length dimension
// must keep the point at its same angle (radial ride), not drift it around the
// circle. The solver's minimum-move otherwise moves the point off-angle to also
// satisfy the length constraint.
bool test_resizing_a_circle_keeps_the_attached_point_at_its_angle() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 10.0);
  // Line from (6,8) [on the circle, angle atan2(8,6)] to (14,14) [diagonal,
  // length 10 — deliberately NOT axis-aligned so no H/V constraint clamps the
  // radial ride].
  DocumentState document =
      manager.add_sketch_line(6.0, 8.0, 14.0, 14.0, false, "", true);
  // Make the auto line-length dimension DRIVING so the solver must preserve it.
  document = manager.update_sketch_dimension("dim-line-line-1", 10.0);
  const SketchFeatureParameters* before = sketch_of(document);
  if (!expect(before != nullptr, "angle: the sketch exists")) return false;
  const std::string circle_id = before->circles.front().id;

  // Enlarge radius 10 -> 20, centre fixed.
  document = manager.update_sketch_circle(circle_id, 0.0, 0.0, 20.0);
  const SketchFeatureParameters* after = sketch_of(document);
  if (!expect(after != nullptr, "angle: the sketch survives")) return false;

  const auto& line = after->lines.back();
  // The start should ride radially to (12,16) — same angle, radius 20.
  return expect(
      std::abs(line.start_x - 12.0) < 1e-1 && std::abs(line.start_y - 16.0) < 1e-1,
      ("angle: the attached point stays at its angle (12,16) - got (" +
       std::to_string(line.start_x) + ", " + std::to_string(line.start_y) + ")")
          .c_str());
}

// Moving the circle CENTRE with an attached point AND a driving line-length
// dimension must not let the solver slide the point around the circle (the
// "end wanders on the circle" report). The point rides the centre by the
// translation delta and stays at its angle; only the free end moves.
bool test_moving_a_circle_centre_keeps_the_attached_point_on_its_radius() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 10.0);
  DocumentState document =
      manager.add_sketch_line(6.0, 8.0, 14.0, 14.0, false, "", true);
  document = manager.update_sketch_dimension("dim-line-line-1", 10.0);
  const SketchFeatureParameters* before = sketch_of(document);
  if (!expect(before != nullptr, "centre: the sketch exists")) return false;
  const std::string center_vertex = before->circles.front().center_vertex_id;

  // Move the centre (0,0) -> (5,0).
  document = manager.update_sketch_vertex(center_vertex, 5.0, 0.0);
  const SketchFeatureParameters* after = sketch_of(document);
  if (!expect(after != nullptr, "centre: the sketch survives")) return false;

  const auto& line = after->lines.back();
  // The start rides the translate to (11,8): same angle, still on the radius.
  return expect(
      std::abs(line.start_x - 11.0) < 1e-1 && std::abs(line.start_y - 8.0) < 1e-1,
      ("centre: the attached point stays at its angle (11,8) - got (" +
       std::to_string(line.start_x) + ", " + std::to_string(line.start_y) + ")")
          .c_str());
}

// Dragging the circle must OVERRIDE the length dimension, not fight it. The
// user's model is the one already used when they drag a line's free end: the
// geometry they hold onto wins and the dimension re-measures itself. Here the
// attached end is carried by the centre and the free end is never asked to
// move, so the length dimension has to record the new distance.
bool test_moving_a_circle_centre_does_not_push_the_dimensioned_free_end() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 10.0);
  DocumentState document =
      manager.add_sketch_line(6.0, 8.0, 14.0, 14.0, false, "", true);
  document = manager.update_sketch_dimension("dim-line-line-1", 10.0);
  const SketchFeatureParameters* before = sketch_of(document);
  if (!expect(before != nullptr, "free end: the sketch exists")) return false;
  const std::string center_vertex = before->circles.front().center_vertex_id;

  // Move the centre (0,0) -> (5,0): the attached end rides to (11,8).
  document = manager.update_sketch_vertex(center_vertex, 5.0, 0.0);
  const SketchFeatureParameters* after = sketch_of(document);
  if (!expect(after != nullptr, "free end: the sketch survives")) return false;

  const auto& line = after->lines.back();
  if (!expect(std::abs(line.end_x - 14.0) < 1e-1 &&
                  std::abs(line.end_y - 14.0) < 1e-1,
              ("free end: the free end stays at (14,14) - got (" +
               std::to_string(line.end_x) + ", " + std::to_string(line.end_y) +
               ")")
                  .c_str())) {
    return false;
  }

  // …and the dimension re-measures to the new length: (11,8) -> (14,14).
  const auto& dimensions = after->dimensions;
  const auto dimension_it = std::find_if(
      dimensions.begin(), dimensions.end(), [](const auto& dimension) {
        return dimension.kind == "line_length";
      });
  if (!expect(dimension_it != dimensions.end(),
              "free end: the length dimension survives")) {
    return false;
  }
  const double expected = std::sqrt(9.0 + 36.0);
  return expect(std::abs(dimension_it->value - expected) < 1e-2,
                ("free end: the dimension re-measures to " +
                 std::to_string(expected) + " - got " +
                 std::to_string(dimension_it->value))
                    .c_str());
}

// The perimeter drag is the resize counterpart: the attached point is
// projected onto the new radius, so the line has already changed length before
// the solve runs and a driving length dimension must not yank the free end to
// restore the old one ("dragging from the perimeter the line snaps").
bool test_resizing_a_circle_does_not_push_the_dimensioned_free_end() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 10.0);
  DocumentState document =
      manager.add_sketch_line(6.0, 8.0, 14.0, 14.0, false, "", true);
  document = manager.update_sketch_dimension("dim-line-line-1", 10.0);
  const SketchFeatureParameters* before = sketch_of(document);
  if (!expect(before != nullptr, "resize free end: the sketch exists")) {
    return false;
  }
  const std::string circle_id = before->circles.front().id;

  // Enlarge radius 10 -> 20: the attached end projects radially to (12,16).
  document = manager.update_sketch_circle(circle_id, 0.0, 0.0, 20.0);
  const SketchFeatureParameters* after = sketch_of(document);
  if (!expect(after != nullptr, "resize free end: the sketch survives")) {
    return false;
  }

  const auto& line = after->lines.back();
  if (!expect(std::abs(line.end_x - 14.0) < 1e-1 &&
                  std::abs(line.end_y - 14.0) < 1e-1,
              ("resize free end: the free end stays at (14,14) - got (" +
               std::to_string(line.end_x) + ", " + std::to_string(line.end_y) +
               ")")
                  .c_str())) {
    return false;
  }

  // …and the dimension re-measures to the new length: (12,16) -> (14,14).
  const auto dimension_it = std::find_if(
      after->dimensions.begin(), after->dimensions.end(), [](const auto& dimension) {
        return dimension.kind == "line_length";
      });
  if (!expect(dimension_it != after->dimensions.end(),
              "resize free end: the length dimension survives")) {
    return false;
  }
  const double expected = std::sqrt(4.0 + 4.0);
  return expect(std::abs(dimension_it->value - expected) < 1e-2,
                ("resize free end: the dimension re-measures to " +
                 std::to_string(expected) + " - got " +
                 std::to_string(dimension_it->value))
                    .c_str());
}

// Two lines meeting at one point on a circle SHARE that vertex, so the second
// line's commit routes the same vertex through the attach path again. It used to
// register a second point_on_circle for the same pair - seen in the field as
// "attached vertex-4 to the boundary of circle-1" twice, with
// constraint-point-on-circle-2 and -3 both naming vertex-4. Harmless to the solve,
// but it doubles the radius pin, which planegcs reports as redundant.
bool test_attaching_the_same_circle_point_twice_registers_one_constraint() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 10.0);
  // A line whose END lands on the boundary at (0,10)…
  manager.add_sketch_line(20.0, 0.0, 0.0, 10.0, false, "", true);
  // …then a second line starting at that same point adopts its vertex.
  const DocumentState document =
      manager.add_sketch_line(0.0, 10.0, 30.0, 10.0, false, "", true);
  const SketchFeatureParameters* parameters = sketch_of(document);
  if (!expect(parameters != nullptr, "double attach: the sketch exists")) {
    return false;
  }

  const std::string circle_id = parameters->circles.front().id;
  int attachments = 0;
  std::string detail;
  for (const auto& constraint : parameters->constraints) {
    if (constraint.kind == "point_on_circle" &&
        constraint.target_ids.size() >= 2 &&
        constraint.target_ids[1] == circle_id) {
      ++attachments;
      detail += " [" + constraint.target_ids[0] + "]";
    }
  }
  return expect(attachments == 1,
                ("double attach: one point_on_circle for the shared vertex, got " +
                 std::to_string(attachments) + detail)
                    .c_str());
}

// ── Drag diagnostics (2026-11-03) ──────────────────────────────────────
// Reported: dragging a point that sits on a circle leaves it displaced, and a
// tangent line's point jumps after the drag. These isolate the two mechanisms.
//
// BOTH TESTS BELOW ARE RED ON PURPOSE. They are the reproduction for plan
// items A and B in .deepseek/active-task.md; the suite is expected to fail
// until those are fixed. Do not "fix" them by relaxing the assertion.

// (a) point_on_circle ALONE, no tangent relation: the dragged endpoint must
// land where the user released it (this was fixed before and is the control).
// RED TODAY: it lands on the circle CENTRE (0,0) and the log reports
// "sketch solver failed - solver failed to converge (status=2, dofs=5)".
bool test_dragging_a_circle_bound_endpoint_lands_where_released() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 10.0);
  DocumentState document =
      manager.add_sketch_line(10.0, 0.0, 30.0, 0.0, false, "", true);
  const SketchFeatureParameters* parameters = sketch_of(document);
  if (!expect(parameters != nullptr, "circle drag: the sketch exists")) {
    return false;
  }
  const std::string vertex_id = parameters->lines.back().start_vertex_id;

  // A quarter turn around the boundary, still exactly on the circle.
  document = manager.update_sketch_vertex(vertex_id, 0.0, -10.0);
  parameters = sketch_of(document);
  if (!expect(parameters != nullptr, "circle drag: the sketch survives")) {
    return false;
  }
  const auto& line = parameters->lines.back();
  const std::string detail = " (landed at " + std::to_string(line.start_x) +
                             ", " + std::to_string(line.start_y) + ")";
  return expect(std::abs(line.start_x) < 1e-6 &&
                    std::abs(line.start_y + 10.0) < 1e-6,
                ("circle drag: the endpoint lands where it was released" + detail)
                    .c_str());
}

// (b) tangent_line_circle: dragging the tangent point must NOT be pulled back
// onto the old tangent point. The user's rule is that a drag breaks the
// tangency (and its badge) instead of moving the vertex to preserve it.
// RED TODAY: enforce_tangent_line_circle_relations re-projects the line's end
// onto the tangent point on every refresh, and the relation — with its badge —
// survives a drag that broke the tangency.
bool test_dragging_a_tangent_endpoint_lands_where_released() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 10.0);
  // From (20,0) — outside the circle — to its tangent point (5, -8.660254).
  DocumentState document =
      manager.add_sketch_line(20.0, 0.0, 5.0, -8.660254, false, "", false);
  const SketchFeatureParameters* parameters = sketch_of(document);
  if (!expect(parameters != nullptr, "tangent drag: the sketch exists")) {
    return false;
  }
  const std::string line_id = parameters->lines.back().id;
  const std::string circle_id = parameters->circles.front().id;
  const std::string end_vertex_id = parameters->lines.back().end_vertex_id;

  document = manager.set_sketch_tangent_constraint(line_id, circle_id);
  // Drag the tangent point a quarter turn along the circle.
  document = manager.update_sketch_vertex(end_vertex_id, 0.0, -10.0);
  parameters = sketch_of(document);
  if (!expect(parameters != nullptr, "tangent drag: the sketch survives")) {
    return false;
  }
  const auto& line = parameters->lines.back();
  return expect(std::abs(line.end_x) < 1e-6 &&
                    std::abs(line.end_y + 10.0) < 1e-6,
                "tangent drag: the dragged endpoint is not pulled back to the "
                "stale tangent point");
}

// ── Line-to-line T junction (plan item C) ─────────────────────────────
// A stem committed onto a host line's BODY gets a `point_line` anchor
// (attach_committed_vertex_to_geometry). The contract for that junction is
// SolidCraft's, not Fusion's: the anchored point stays on its host while the
// host is edited, the stem's own FREE end is not dragged along with it, and
// both lines may change length and orientation. The two tests below are the
// two drags the user performs on a T; they are the reproduction for plan item C.
//
// RED TODAY (2026-11-03): the first one moves the stem's free end, because the
// anchored point is treated as an authoritative corner by
// propagate_connected_point_move (it carries the line's other endpoint across
// the axis) and the stem's own axis constraint then keeps it there.

bool test_a_t_junction_rides_the_host_without_moving_the_free_end() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_line(0.0, 0.0, 100.0, 0.0, false, "", true);
  DocumentState document =
      manager.add_sketch_line(30.0, 0.0, 30.0, 40.0, false, "", true);
  const SketchFeatureParameters* before = sketch_of(document);
  if (!expect(before != nullptr, "T host: the sketch exists")) return false;
  if (!expect(before->point_line_anchors.size() == 1,
              "T host: the stem is anchored to the host line")) {
    return false;
  }
  const std::string anchored_id = before->point_line_anchors.front().vertex_id;
  const std::string free_end_id =
      before->lines.back().start_vertex_id == anchored_id
          ? before->lines.back().end_vertex_id
          : before->lines.back().start_vertex_id;

  // Stretch the host from (0,0)-(100,0) to (0,0)-(160,0). The anchored point
  // sits at t = 0.3 of the host, which is now (48,0) — it must ride there.
  document = manager.update_sketch_vertex("vertex-2", 160.0, 0.0);
  const SketchFeatureParameters* after = sketch_of(document);
  if (!expect(after != nullptr, "T host: the sketch survives")) return false;

  double anchored_x = 0.0;
  double anchored_y = 0.0;
  double free_x = 0.0;
  double free_y = 0.0;
  if (!expect(vertex_xy(*after, anchored_id, anchored_x, anchored_y) &&
                  vertex_xy(*after, free_end_id, free_x, free_y),
              "T host: both stem vertices still exist")) {
    return false;
  }
  if (!expect(
          std::abs(anchored_x - 48.0) < 1e-3 && std::abs(anchored_y) < 1e-3,
          ("T host: the anchored point rides the stretched host to (48,0) - got (" +
           std::to_string(anchored_x) + ", " + std::to_string(anchored_y) + ")")
              .c_str())) {
    return false;
  }
  return expect(
      std::abs(free_x - 30.0) < 1e-3 && std::abs(free_y - 40.0) < 1e-3,
      ("T host: the stem's free end stays at (30,40) - got (" +
       std::to_string(free_x) + ", " + std::to_string(free_y) + ")")
          .c_str());
}

bool test_dragging_the_anchored_point_keeps_the_free_end() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_line(0.0, 0.0, 100.0, 0.0, false, "", true);
  DocumentState document =
      manager.add_sketch_line(30.0, 0.0, 30.0, 40.0, false, "", true);
  const SketchFeatureParameters* before = sketch_of(document);
  if (!expect(before != nullptr, "T drag: the sketch exists")) return false;
  if (!expect(before->point_line_anchors.size() == 1,
              "T drag: the stem is anchored to the host line")) {
    return false;
  }
  const std::string anchored_id = before->point_line_anchors.front().vertex_id;
  const std::string free_end_id =
      before->lines.back().start_vertex_id == anchored_id
          ? before->lines.back().end_vertex_id
          : before->lines.back().start_vertex_id;

  // Drag the junction perpendicular to the host. The anchor holds it ON the
  // host at its stored position; the stem's free end is not the dragged
  // geometry, so it must not move.
  document = manager.update_sketch_vertex(anchored_id, 30.0, 10.0);
  const SketchFeatureParameters* after = sketch_of(document);
  if (!expect(after != nullptr, "T drag: the sketch survives")) return false;

  double anchored_x = 0.0;
  double anchored_y = 0.0;
  double free_x = 0.0;
  double free_y = 0.0;
  if (!expect(vertex_xy(*after, anchored_id, anchored_x, anchored_y) &&
                  vertex_xy(*after, free_end_id, free_x, free_y),
              "T drag: both stem vertices still exist")) {
    return false;
  }
  if (!expect(std::abs(anchored_y) < 1e-3 && std::abs(anchored_x - 30.0) < 1e-3,
              ("T drag: the anchored point is held on its host at (30,0) - got (" +
               std::to_string(anchored_x) + ", " + std::to_string(anchored_y) + ")")
                  .c_str())) {
    return false;
  }
  return expect(
      std::abs(free_x - 30.0) < 1e-3 && std::abs(free_y - 40.0) < 1e-3,
      ("T drag: the stem's free end stays at (30,40) - got (" +
       std::to_string(free_x) + ", " + std::to_string(free_y) + ")")
          .c_str());
}

// (c) The MOVE tool is the same offending operation as the drag (the user's
// rule names "drag/move"): a moved tangent line keeps its new position instead
// of having its end re-projected onto the stale tangent point, and the relation
// — and therefore the "T" badge — is dropped because the line is no longer
// tangent. Moving the CIRCLE stays covered by the re-projection (the line
// riding a moved circle is what it is for).
bool test_moving_a_tangent_line_does_not_reproject_its_end() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 10.0);
  DocumentState document =
      manager.add_sketch_line(20.0, 0.0, 5.0, -8.660254, false, "", false);
  const SketchFeatureParameters* parameters = sketch_of(document);
  if (!expect(parameters != nullptr, "tangent move: the sketch exists")) {
    return false;
  }
  const std::string line_id = parameters->lines.back().id;
  const std::string circle_id = parameters->circles.front().id;
  document = manager.set_sketch_tangent_constraint(line_id, circle_id);

  // Translate the line 20 mm up. Its end is no longer tangent to the circle.
  document = manager.move_sketch_entities({line_id}, 0.0, 20.0, 0.0, 0.0, 0.0);
  parameters = sketch_of(document);
  if (!expect(parameters != nullptr, "tangent move: the sketch survives")) {
    return false;
  }
  const auto& line = parameters->lines.back();
  if (!expect(
          std::abs(line.end_x - 5.0) < 1e-6 &&
              std::abs(line.end_y - 11.339746) < 1e-3,
          ("tangent move: the end keeps the moved position (5,11.339746) - got (" +
           std::to_string(line.end_x) + ", " + std::to_string(line.end_y) + ")")
              .c_str())) {
    return false;
  }
  for (const auto& relation : parameters->line_relations) {
    if (relation.kind == "tangent_line_circle" &&
        relation.first_line_id == line_id) {
      return expect(false, "tangent move: the broken relation is dropped");
    }
  }
  return true;
}

// ── A circle drag keeps its tangent lines tangent (2026-11-04) ────────
// Measured on the user's document (`test.polysmith`, saved from the running
// app): applying tangency to lines that end on a circle is exact (d − r =
// 0.0000), but dragging the CIRCLE then translated the contact points rigidly
// with the centre — they stayed on the circle yet were no longer tangent
// points — while the "T" badge stayed up. The error grew on every further drag:
// d − r reached −0.48 mm and −2.30 mm on an r = 32 circle, i.e. the lines cut
// through the circle while both advertised tangency.
//
// Cause: the centre drag freezes every riding point (`is_fixed`) around
// refresh_sketch_derived_state so the SOLVE cannot pull it off-angle around the
// circle. That freeze is a solve-time guard — the attachment itself is the
// `point_on_circle` constraint — but `enforce_tangent_line_circle_relations`
// runs AFTER the solve and moves the contact point through
// `propagate_connected_point_move`, which refuses a fixed vertex. It silently
// no-opped.
//
// A tangency owns its contact point's position: after the centre moves, the new
// tangent point is the only correct spot on the circle, so the contact point
// must stay out of the freeze. The attachments are asserted too — the apex
// (the line's anchored start) must not move and the contact point must remain
// ON the circle: satisfying the relation may never detach the line.
bool test_dragging_a_circle_keeps_its_tangent_lines_tangent() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_circle(0.0, 0.0, 30.0);
  DocumentState document =
      manager.add_sketch_line(-100.0, 0.0, 0.0, 30.0, false, "", true);
  const SketchFeatureParameters* parameters = sketch_of(document);
  if (!expect(parameters != nullptr, "circle tangent drag: the sketch exists")) {
    return false;
  }
  const std::string line_id = parameters->lines.back().id;
  const std::string circle_id = parameters->circles.front().id;
  const std::string center_vertex_id =
      parameters->circles.front().center_vertex_id;
  document = manager.set_sketch_tangent_constraint(line_id, circle_id);
  parameters = sketch_of(document);
  if (!expect(parameters != nullptr,
              "circle tangent drag: the sketch survives the tangency")) {
    return false;
  }

  // Distance from the circle centre to the line's support (|d − r| == 0 means
  // tangent, d < r means the line cuts into the circle).
  const auto perpendicular_distance = [](const SketchFeatureParameters& p) {
    const auto& circle = p.circles.front();
    const auto& line = p.lines.back();
    const double dx = line.end_x - line.start_x;
    const double dy = line.end_y - line.start_y;
    const double length = std::hypot(dx, dy);
    if (length <= 0.0) return 0.0;
    return std::abs(dx * (circle.center_y - line.start_y) -
                    dy * (circle.center_x - line.start_x)) /
           length;
  };
  if (!expect(std::abs(perpendicular_distance(*parameters) -
                       parameters->circles.front().radius) < 1e-6,
              "circle tangent drag: the tangent tool leaves the line exactly "
              "tangent")) {
    return false;
  }

  document = manager.update_sketch_vertex(center_vertex_id, 10.0, 10.0);
  parameters = sketch_of(document);
  if (!expect(parameters != nullptr,
              "circle tangent drag: the sketch survives the drag")) {
    return false;
  }
  const auto& circle = parameters->circles.front();
  const auto& line = parameters->lines.back();
  bool relation_kept = false;
  for (const auto& relation : parameters->line_relations) {
    if (relation.kind == "tangent_line_circle" &&
        relation.first_line_id == line_id) {
      relation_kept = true;
    }
  }
  const double end_radial =
      std::hypot(line.end_x - circle.center_x, line.end_y - circle.center_y);
  const double after = perpendicular_distance(*parameters);
  return expect(std::abs(line.start_x + 100.0) < 1e-6 &&
                    std::abs(line.start_y) < 1e-6,
                ("circle tangent drag: the anchored start stays at (-100,0) - "
                 "got (" +
                 std::to_string(line.start_x) + ", " +
                 std::to_string(line.start_y) + ")")
                    .c_str()) &&
         expect(std::abs(end_radial - circle.radius) < 1e-6,
                ("circle tangent drag: the contact point stays ON the circle - "
                 "off by " +
                 std::to_string(end_radial - circle.radius))
                    .c_str()) &&
         expect(relation_kept,
                "circle tangent drag: the tangency survives the move") &&
         expect(std::abs(after - circle.radius) < 1e-6,
                ("circle tangent drag: the line is still tangent after the "
                 "move - d-r=" +
                 std::to_string(after - circle.radius))
                    .c_str());
}

}  // namespace

int main() {
  if (!test_a_stem_on_the_midpoint_is_attached()) return 1;
  if (!test_the_attached_end_follows_the_host()) return 1;
  if (!test_a_stem_off_the_middle_gets_a_fractional_anchor()) return 1;
  if (!test_a_stem_far_from_the_host_is_not_attached()) return 1;
  if (!test_a_stem_at_the_host_endpoint_is_not_anchored()) return 1;
  if (!test_attaching_is_idempotent()) return 1;
  if (!test_without_the_flag_nothing_is_attached()) return 1;
  if (!test_a_stem_on_a_circle_boundary_is_attached()) return 1;
  if (!test_without_the_flag_a_circle_is_not_attached()) return 1;
  if (!test_the_attached_end_stays_on_the_circle_after_an_edit()) return 1;
  if (!test_a_stem_drawn_slightly_off_the_circle_is_pulled_onto_it()) return 1;
  if (!test_a_line_drawn_from_a_circle_follows_the_circle()) return 1;
  if (!test_a_line_drawn_from_a_circle_follows_a_resize()) return 1;
  if (!test_a_line_drawn_on_a_circle_cardinal_point_is_bound_by_a_constraint()) return 1;
  if (!test_a_line_drawn_on_an_arc_body_is_bound_by_point_on_arc()) return 1;
  if (!test_a_line_drawn_on_an_arc_follows_the_arc()) return 1;
  if (!test_resizing_a_circle_keeps_the_centre_fixed()) return 1;
  if (!test_resizing_a_circle_updates_its_profile()) return 1;
  if (!test_resizing_a_circle_keeps_the_attached_point_at_its_angle()) return 1;
  if (!test_moving_a_circle_centre_keeps_the_attached_point_on_its_radius()) return 1;
  if (!test_moving_a_circle_centre_does_not_push_the_dimensioned_free_end()) return 1;
  if (!test_attaching_the_same_circle_point_twice_registers_one_constraint()) return 1;
  if (!test_resizing_a_circle_does_not_push_the_dimensioned_free_end()) return 1;
  if (!test_dragging_a_circle_bound_endpoint_lands_where_released()) return 1;
  if (!test_dragging_a_tangent_endpoint_lands_where_released()) return 1;
  if (!test_a_t_junction_rides_the_host_without_moving_the_free_end()) return 1;
  if (!test_dragging_the_anchored_point_keeps_the_free_end()) return 1;
  if (!test_moving_a_tangent_line_does_not_reproject_its_end()) return 1;
  if (!test_dragging_a_circle_keeps_its_tangent_lines_tangent()) return 1;

  std::cout << "committed_attach_test passed\n";
  return 0;
}
