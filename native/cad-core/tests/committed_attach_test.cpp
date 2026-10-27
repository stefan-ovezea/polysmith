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

  std::cout << "committed_attach_test passed\n";
  return 0;
}
