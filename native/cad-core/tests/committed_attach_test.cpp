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

}  // namespace

int main() {
  if (!test_a_stem_on_the_midpoint_is_attached()) return 1;
  if (!test_the_attached_end_follows_the_host()) return 1;
  if (!test_a_stem_off_the_middle_gets_a_fractional_anchor()) return 1;
  if (!test_a_stem_far_from_the_host_is_not_attached()) return 1;
  if (!test_a_stem_at_the_host_endpoint_is_not_anchored()) return 1;
  if (!test_attaching_is_idempotent()) return 1;
  if (!test_without_the_flag_nothing_is_attached()) return 1;

  std::cout << "committed_attach_test passed\n";
  return 0;
}
