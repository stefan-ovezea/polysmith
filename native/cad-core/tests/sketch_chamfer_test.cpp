// Regression tests for the sketch chamfer entity (feature/sketch).
//
// v1: line-line chamfer — a parametric SketchChamfer record that trims
// two lines sharing a corner and inserts a straight chamfer line
// between the trim points, re-derived on every recompute
// (private_chamfer_refresh.inc). Mirrors the fillet lifecycle with a
// line instead of an arc; a corner holding both a fillet and a chamfer
// is rejected.
//
// Profile-set assertions use profiles_match (complete region-set
// matching, not presence-only).

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "core/document/document.h"
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

bool near(double a, double b, double tolerance = 1.0e-6) {
  return std::abs(a - b) < tolerance;
}

// Rectangle 40x20 at the origin. Returns the id of the corner shared
// by the bottom (line-1) and right (line-2) sides at (40,0), plus the
// two line ids.
struct CornerPick {
  std::string corner_vertex_id;
  std::string line_a_id;  // bottom
  std::string line_b_id;  // right
};
CornerPick pick_corner(const SketchFeatureParameters& params) {
  // add_sketch_rectangle ordering: line-1 bottom, line-2 right.
  return {.corner_vertex_id = params.lines[0].end_vertex_id,
          .line_a_id = params.lines[0].id,
          .line_b_id = params.lines[1].id};
}

bool test_symmetric_chamfer_creates_line_and_trims() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  const auto before = sketch_params(document);
  const CornerPick corner = pick_corner(before);

  document = manager.add_sketch_chamfer(corner.corner_vertex_id,
                                        corner.line_a_id, corner.line_b_id,
                                        5.0, 5.0);
  const auto after = sketch_params(document);
  if (!expect(after.chamfers.size() == 1, "chamfer: record created")) {
    return false;
  }
  if (!expect(after.lines.size() == 5 && after.arcs.size() == 0,
              "chamfer: 4 rectangle lines + 1 chamfer line")) {
    return false;
  }

  const auto& chamfer = after.chamfers[0];
  // Trim points: line A (bottom, toward +x) trimmed at (35,0); line B
  // (right, toward +y) trimmed at (40,5).
  const auto find_line = [&](const std::string& id) {
    const auto it = std::find_if(after.lines.begin(), after.lines.end(),
                                 [&](const auto& l) { return l.id == id; });
    return it == after.lines.end() ? nullptr : &*it;
  };
  const auto* line_a = find_line(corner.line_a_id);
  const auto* line_b = find_line(corner.line_b_id);
  const auto* chamfer_line = find_line(chamfer.chamfer_line_id);
  if (!expect(line_a != nullptr && line_b != nullptr && chamfer_line != nullptr,
              "chamfer: all entities present")) {
    return false;
  }
  return expect(near(line_a->end_x, 35.0) && near(line_a->end_y, 0.0) &&
                    near(line_b->start_x, 40.0) && near(line_b->start_y, 5.0) &&
                    near(chamfer_line->start_x, 35.0) &&
                    near(chamfer_line->start_y, 0.0) &&
                    near(chamfer_line->end_x, 40.0) &&
                    near(chamfer_line->end_y, 5.0),
                "chamfer: symmetric trim points + chamfer line geometry");
}

bool test_asymmetric_chamfer() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  const auto before = sketch_params(document);
  const CornerPick corner = pick_corner(before);

  document = manager.add_sketch_chamfer(corner.corner_vertex_id,
                                        corner.line_a_id, corner.line_b_id,
                                        5.0, 8.0);
  const auto after = sketch_params(document);
  if (!expect(after.chamfers.size() == 1,
              "asymmetric chamfer: record created")) {
    return false;
  }
  const auto& chamfer = after.chamfers[0];
  const auto find_line = [&](const std::string& id) {
    const auto it = std::find_if(after.lines.begin(), after.lines.end(),
                                 [&](const auto& l) { return l.id == id; });
    return it == after.lines.end() ? nullptr : &*it;
  };
  const auto* line_a = find_line(corner.line_a_id);
  const auto* line_b = find_line(corner.line_b_id);
  const auto* chamfer_line = find_line(chamfer.chamfer_line_id);
  if (!expect(line_a != nullptr && line_b != nullptr && chamfer_line != nullptr,
              "asymmetric chamfer: entities present")) {
    return false;
  }
  // distance_a=5 along the bottom (35,0); distance_b=8 up the right
  // side (40,8).
  return expect(near(line_a->end_x, 35.0) && near(line_a->end_y, 0.0) &&
                    near(line_b->start_x, 40.0) && near(line_b->start_y, 8.0) &&
                    near(chamfer_line->start_x, 35.0) &&
                    near(chamfer_line->end_x, 40.0) &&
                    near(chamfer_line->end_y, 8.0),
                "asymmetric chamfer: trim geometry follows the distances");
}

bool test_chamfer_distance_edits_both_directions() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  const auto before = sketch_params(document);
  const CornerPick corner = pick_corner(before);
  document = manager.add_sketch_chamfer(corner.corner_vertex_id,
                                        corner.line_a_id, corner.line_b_id,
                                        5.0, 5.0);
  const std::string chamfer_id = sketch_params(document).chamfers[0].id;

  const auto trim_a_x = [&](const SketchFeatureParameters& params) {
    const auto it = std::find_if(
        params.lines.begin(), params.lines.end(),
        [&](const auto& l) { return l.id == corner.line_a_id; });
    return it->end_x;
  };
  const auto trim_b_y = [&](const SketchFeatureParameters& params) {
    const auto it = std::find_if(
        params.lines.begin(), params.lines.end(),
        [&](const auto& l) { return l.id == corner.line_b_id; });
    return it->start_y;
  };

  // Shrink both distances.
  document = manager.update_sketch_chamfer(chamfer_id, 4.5, 4.5);
  auto after = sketch_params(document);
  if (!expect(near(trim_a_x(after), 35.5) && near(trim_b_y(after), 4.5),
              "chamfer edit: shrink re-derives both trims")) {
    return false;
  }

  // Grow both distances (asymmetric this time).
  document = manager.update_sketch_chamfer(chamfer_id, 6.0, 3.0);
  after = sketch_params(document);
  return expect(near(trim_a_x(after), 34.0) && near(trim_b_y(after), 3.0),
                "chamfer edit: grow re-derives both trims");
}

bool test_chamfered_rectangle_full_profile() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  const auto before = sketch_params(document);
  const CornerPick corner = pick_corner(before);
  document = manager.add_sketch_chamfer(corner.corner_vertex_id,
                                        corner.line_a_id, corner.line_b_id,
                                        5.0, 5.0);
  const auto after = sketch_params(document);

  // The chamfered rectangle closes with all 5 lines.
  std::vector<std::string> expected_ids;
  for (const auto& line : after.lines) expected_ids.push_back(line.id);
  std::string reason;
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = expected_ids, .kind = "polygon"},
  };
  return expect(profiles_match(document, expected, &reason),
                ("chamfered rectangle profile: " + reason).c_str());
}

bool test_chamfer_delete_restores_corner() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  const auto before = sketch_params(document);
  const CornerPick corner = pick_corner(before);
  document = manager.add_sketch_chamfer(corner.corner_vertex_id,
                                        corner.line_a_id, corner.line_b_id,
                                        5.0, 5.0);
  const std::string chamfer_id = sketch_params(document).chamfers[0].id;

  document = manager.delete_sketch_chamfer(chamfer_id);
  const auto after = sketch_params(document);
  if (!expect(after.chamfers.empty() && after.lines.size() == 4,
              "chamfer delete: record + chamfer line removed")) {
    return false;
  }
  const auto find_line = [&](const std::string& id) {
    const auto it = std::find_if(after.lines.begin(), after.lines.end(),
                                 [&](const auto& l) { return l.id == id; });
    return it == after.lines.end() ? nullptr : &*it;
  };
  const auto* line_a = find_line(corner.line_a_id);
  const auto* line_b = find_line(corner.line_b_id);
  if (!expect(line_a != nullptr && line_b != nullptr,
              "chamfer delete: both lines survive")) {
    return false;
  }
  // The corner is restored to (40,0) as the shared endpoint.
  return expect(near(line_a->end_x, 40.0) && near(line_a->end_y, 0.0) &&
                    near(line_b->start_x, 40.0) && near(line_b->start_y, 0.0) &&
                    line_a->end_vertex_id == line_b->start_vertex_id,
                "chamfer delete: original corner restored and shared");
}

bool test_fillet_chamfer_conflict_rejected() {
  {
    // Chamfer then fillet the same corner → rejected.
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");

    DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
    const auto before = sketch_params(document);
    const CornerPick corner = pick_corner(before);
    document = manager.add_sketch_chamfer(corner.corner_vertex_id,
                                          corner.line_a_id, corner.line_b_id,
                                          5.0, 5.0);

    bool threw = false;
    try {
      (void)manager.add_sketch_fillet(corner.corner_vertex_id,
                                      corner.line_a_id, corner.line_b_id, 2.0);
    } catch (const std::exception&) {
      threw = true;
    }
    if (!expect(threw, "conflict: fillet after chamfer is rejected")) {
      return false;
    }
  }
  {
    // Fillet then chamfer the same corner → rejected.
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");

    DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
    const auto before = sketch_params(document);
    const CornerPick corner = pick_corner(before);
    document = manager.add_sketch_fillet(corner.corner_vertex_id,
                                         corner.line_a_id, corner.line_b_id,
                                         3.0);

    bool threw = false;
    try {
      (void)manager.add_sketch_chamfer(corner.corner_vertex_id,
                                       corner.line_a_id, corner.line_b_id,
                                       5.0, 5.0);
    } catch (const std::exception&) {
      threw = true;
    }
    return expect(threw, "conflict: chamfer after fillet is rejected");
  }
}

bool test_chamfer_survives_dimension_drive() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  const auto before = sketch_params(document);
  const CornerPick corner = pick_corner(before);
  document = manager.add_sketch_chamfer(corner.corner_vertex_id,
                                        corner.line_a_id, corner.line_b_id,
                                        5.0, 5.0);

  // Drive the right side's length 20 -> 30. The chamfer must re-derive
  // against the moved corner and the loop must stay closed with all 5
  // lines.
  document = manager.update_sketch_dimension("dim-line-line-2", 30.0);
  const auto after = sketch_params(document);
  if (!expect(after.chamfers.size() == 1 && after.lines.size() == 5,
              "chamfer drive: record + entities survive")) {
    return false;
  }
  // The trim on the right side is 5 up from the (still y=0) corner,
  // regardless of how the length drive moved the far end.
  const auto find_line = [&](const std::string& id) {
    const auto it = std::find_if(after.lines.begin(), after.lines.end(),
                                 [&](const auto& l) { return l.id == id; });
    return it == after.lines.end() ? nullptr : &*it;
  };
  const auto* line_b = find_line(corner.line_b_id);
  const auto& chamfer = after.chamfers[0];
  if (!expect(line_b != nullptr && near(line_b->start_x, 40.0) &&
                  near(line_b->start_y, 5.0),
              "chamfer drive: trim point follows the re-derived corner")) {
    return false;
  }
  std::vector<std::string> expected_ids;
  for (const auto& line : after.lines) expected_ids.push_back(line.id);
  std::string reason;
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = expected_ids, .kind = "polygon"},
  };
  return expect(profiles_match(document, expected, &reason),
                ("chamfer drive profile: " + reason).c_str());
}

}  // namespace

int main() {
  if (!test_symmetric_chamfer_creates_line_and_trims()) return 1;
  if (!test_asymmetric_chamfer()) return 1;
  if (!test_chamfer_distance_edits_both_directions()) return 1;
  if (!test_chamfered_rectangle_full_profile()) return 1;
  if (!test_chamfer_delete_restores_corner()) return 1;
  if (!test_fillet_chamfer_conflict_rejected()) return 1;
  if (!test_chamfer_survives_dimension_drive()) return 1;

  std::cout << "sketch_chamfer_test passed\n";
  return 0;
}
