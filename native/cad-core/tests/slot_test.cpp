// Regression tests for the sketch slot entity (feature/sketch).
//
// v1: straight slot (stadium) — a parametric SketchSlot record that
// expands to 2 lines + 2 arcs tangent-by-construction on every
// recompute (slot_expansion.inc), mirroring the text expansion pattern.
// The generated entities carry generated_by="slot:<id>" and are not
// user-editable; the slot center is a regular movable vertex.
//
// Profile-set assertions use profiles_match (complete region-set
// matching, not presence-only).

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "core/document/document.h"
#include "core/geometry/body_compiler.h"
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

// Collect the 4 generated entity ids of a slot in CCW order
// (bottom line, right arc, top line, left arc). slot_id already
// carries the "slot-N" form.
std::vector<std::string> slot_entity_ids(const SketchFeatureParameters& params,
                                         const std::string& slot_id) {
  std::vector<std::string> ids;
  ids.push_back("line-" + slot_id + "-bottom");
  ids.push_back("arc-" + slot_id + "-right");
  ids.push_back("line-" + slot_id + "-top");
  ids.push_back("arc-" + slot_id + "-left");
  return ids;
}

const polysmith::core::SketchArc* find_arc(const SketchFeatureParameters& params,
                                           const std::string& id) {
  const auto it = std::find_if(params.arcs.begin(), params.arcs.end(),
                               [&](const auto& arc) { return arc.id == id; });
  return it == params.arcs.end() ? nullptr : &*it;
}

bool test_slot_creation_expands_to_two_lines_two_arcs() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  // Center (10,10), length 20, radius 3, rotation 0: a horizontal slot
  // spanning x in [0,20] with half-circles of radius 3 at both ends.
  DocumentState document = manager.add_sketch_slot(10.0, 10.0, 20.0, 3.0, 0.0);
  const auto params = sketch_params(document);
  if (!expect(params.slots.size() == 1, "slot: record created")) {
    return false;
  }
  const std::string slot_id = params.slots[0].id;
  if (!expect(params.lines.size() == 2 && params.arcs.size() == 2,
              "slot: expands to exactly 2 lines + 2 arcs")) {
    return false;
  }

  // Both generated lines carry the slot owner tag; both arcs do too.
  bool all_owned = true;
  for (const auto& line : params.lines) {
    if (!line.generated_by.has_value() ||
        line.generated_by.value() != "slot:" + slot_id) {
      all_owned = false;
    }
  }
  for (const auto& arc : params.arcs) {
    if (!arc.generated_by.has_value() ||
        arc.generated_by.value() != "slot:" + slot_id) {
      all_owned = false;
    }
  }
  if (!expect(all_owned, "slot: all generated entities carry the owner tag")) {
    return false;
  }

  // Join points must lie on both adjacent circles: the bottom line's
  // endpoints sit on the left/right arc circles at distance radius.
  const auto& bottom = params.lines[0];  // bl -> br
  const auto* left_arc = find_arc(params, "arc-" + slot_id + "-left");
  const auto* right_arc = find_arc(params, "arc-" + slot_id + "-right");
  if (!expect(left_arc != nullptr && right_arc != nullptr,
              "slot: both arcs present")) {
    return false;
  }
  const double bl_dist = std::hypot(bottom.start_x - left_arc->center_x,
                                    bottom.start_y - left_arc->center_y);
  const double br_dist = std::hypot(bottom.end_x - right_arc->center_x,
                                    bottom.end_y - right_arc->center_y);
  const double tl_dist = std::hypot(params.lines[1].end_x - left_arc->center_x,
                                    params.lines[1].end_y - left_arc->center_y);
  const double tr_dist = std::hypot(params.lines[1].start_x - right_arc->center_x,
                                    params.lines[1].start_y - right_arc->center_y);
  return expect(near(bl_dist, 3.0) && near(br_dist, 3.0) &&
                    near(tl_dist, 3.0) && near(tr_dist, 3.0),
                "slot: all four join points lie on-circle (1e-6)");
}

bool test_slot_rotated_geometry_both_signs() {
  // Same slot at rotation +30° and −45°: the corner points must stay on
  // the arc circles and the arc centers must be offset by L/2 along the
  // rotated axis.
  const double kPi = 3.14159265358979323846;
  const double signs[2] = {30.0 * kPi / 180.0, -45.0 * kPi / 180.0};
  for (double rotation : signs) {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");

    DocumentState document =
        manager.add_sketch_slot(10.0, 10.0, 20.0, 3.0, rotation);
    const auto params = sketch_params(document);
    const std::string slot_id = params.slots[0].id;
    const auto* left_arc = find_arc(params, "arc-" + slot_id + "-left");
    const auto* right_arc = find_arc(params, "arc-" + slot_id + "-right");
    if (left_arc == nullptr || right_arc == nullptr) {
      std::cerr << "slot rotated: arcs missing at rotation=" << rotation
                << "\n";
      return false;
    }
    const double ux = std::cos(rotation);
    const double uy = std::sin(rotation);
    const double expected_left_x = 10.0 - 10.0 * ux;
    const double expected_left_y = 10.0 - 10.0 * uy;
    const double expected_right_x = 10.0 + 10.0 * ux;
    const double expected_right_y = 10.0 + 10.0 * uy;
    if (!expect(near(left_arc->center_x, expected_left_x) &&
                    near(left_arc->center_y, expected_left_y) &&
                    near(right_arc->center_x, expected_right_x) &&
                    near(right_arc->center_y, expected_right_y),
                "slot rotated: arc centers offset by L/2 along the axis")) {
      return false;
    }
    // Join points on-circle, both rotation signs.
    for (const auto& arc : params.arcs) {
      const double d1 = std::hypot(arc.start_x - arc.center_x,
                                   arc.start_y - arc.center_y);
      const double d2 = std::hypot(arc.end_x - arc.center_x,
                                   arc.end_y - arc.center_y);
      if (!expect(near(d1, 3.0) && near(d2, 3.0),
                  "slot rotated: endpoints on-circle")) {
        return false;
      }
    }
  }
  return true;
}

bool test_slot_full_profile_exact_id_set() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_slot(10.0, 10.0, 20.0, 3.0, 0.0);
  const auto params = sketch_params(document);
  const std::string slot_id = params.slots[0].id;

  std::string reason;
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = slot_entity_ids(params, slot_id), .kind = "polygon"},
  };
  return expect(profiles_match(document, expected, &reason),
                ("slot profile: " + reason).c_str());
}

bool test_slot_update_reexpands_geometry() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_slot(10.0, 10.0, 20.0, 3.0, 0.0);
  const std::string slot_id = sketch_params(document).slots[0].id;

  // Grow length to 30 and radius to 5 — the id set must not change
  // (deterministic generated ids), the geometry must re-expand.
  document = manager.update_sketch_slot(slot_id, 10.0, 10.0, 30.0, 5.0, 0.0);
  const auto after = sketch_params(document);
  if (!expect(after.lines.size() == 2 && after.arcs.size() == 2,
              "slot update: still 2 lines + 2 arcs")) {
    return false;
  }
  const auto* left_arc = find_arc(after, "arc-" + slot_id + "-left");
  const auto* right_arc = find_arc(after, "arc-" + slot_id + "-right");
  if (!expect(left_arc != nullptr && right_arc != nullptr,
              "slot update: arcs present after re-expansion")) {
    return false;
  }
  if (!expect(near(left_arc->radius, 5.0) && near(right_arc->radius, 5.0),
              "slot update: radius re-expanded")) {
    return false;
  }
  const double center_distance = std::hypot(
      right_arc->center_x - left_arc->center_x,
      right_arc->center_y - left_arc->center_y);
  if (!expect(near(center_distance, 30.0),
              "slot update: arc centers at length distance")) {
    return false;
  }
  std::string reason;
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = slot_entity_ids(after, slot_id), .kind = "polygon"},
  };
  return expect(profiles_match(document, expected, &reason),
                ("slot update profile: " + reason).c_str());
}

bool test_slot_generated_entity_guards() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_slot(10.0, 10.0, 20.0, 3.0, 0.0);
  const auto params = sketch_params(document);
  const std::string slot_id = params.slots[0].id;
  const std::string bottom_line_id = "line-" + slot_id + "-bottom";
  const std::string corner_vertex_id = "vertex-" + slot_id + "-bl";

  // Trim on a generated line must be rejected.
  bool trim_threw = false;
  try {
    (void)manager.trim_sketch_entity(bottom_line_id, 10.0, 13.0);
  } catch (const std::exception&) {
    trim_threw = true;
  }
  if (!expect(trim_threw, "slot: trim on generated line rejected")) {
    return false;
  }

  // Moving a generated entity must be rejected.
  bool move_threw = false;
  try {
    (void)manager.move_sketch_entities({bottom_line_id}, 5.0, 5.0, 0.0, 0.0, 0.0);
  } catch (const std::exception&) {
    move_threw = true;
  }
  if (!expect(move_threw, "slot: move on generated line rejected")) {
    return false;
  }

  // Dragging a generated corner vertex must be rejected.
  bool vertex_threw = false;
  try {
    (void)manager.update_sketch_vertex(corner_vertex_id, 12.0, 12.0);
  } catch (const std::exception&) {
    vertex_threw = true;
  }
  return expect(vertex_threw, "slot: drag on generated corner vertex rejected");
}

bool test_slot_center_drag_and_move_preserve_shape() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_slot(10.0, 10.0, 20.0, 3.0, 0.0);
  const auto before = sketch_params(document);
  const std::string slot_id = before.slots[0].id;
  const std::string center_vertex_id = before.slots[0].center_vertex_id;

  // Drag the center vertex: the slot follows.
  document = manager.update_sketch_vertex(center_vertex_id, 25.0, 40.0);
  auto after = sketch_params(document);
  if (!expect(near(after.slots[0].center_x, 25.0) &&
                  near(after.slots[0].center_y, 40.0),
              "slot: center drag moves the record")) {
    return false;
  }

  // Move tool on the slot id: center translates, shape preserved.
  document = manager.move_sketch_entities({slot_id}, -5.0, -5.0, 0.0, 0.0, 0.0);
  after = sketch_params(document);
  if (!expect(near(after.slots[0].center_x, 20.0) &&
                  near(after.slots[0].center_y, 35.0),
              "slot: move tool translates the center")) {
    return false;
  }
  const auto* left_arc = find_arc(after, "arc-" + slot_id + "-left");
  const auto* right_arc = find_arc(after, "arc-" + slot_id + "-right");
  if (!expect(left_arc != nullptr && right_arc != nullptr &&
                  near(left_arc->radius, 3.0) && near(right_arc->radius, 3.0) &&
                  near(after.slots[0].length, 20.0),
              "slot: shape parameters preserved by the move")) {
    return false;
  }
  std::string reason;
  const std::vector<ExpectedProfile> expected = {
      {.entity_ids = slot_entity_ids(after, slot_id), .kind = "polygon"},
  };
  return expect(profiles_match(document, expected, &reason),
                ("slot moved profile: " + reason).c_str());
}

bool test_slot_distance_dimension_between_slots() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_slot(0.0, 0.0, 20.0, 3.0, 0.0);
  document = manager.add_sketch_slot(60.0, 0.0, 20.0, 3.0, 0.0);
  const auto before = sketch_params(document);
  if (!expect(before.slots.size() == 2, "slot dim: two slots created")) {
    return false;
  }
  const std::string c1 = before.slots[0].center_vertex_id;
  const std::string c2 = before.slots[1].center_vertex_id;

  // Distance dimension between the two slot centers.
  document = manager.add_sketch_vertex_distance_dimension(c1, c2);
  auto params = sketch_params(document);
  const auto dim_it = std::find_if(
      params.dimensions.begin(), params.dimensions.end(),
      [&](const auto& dim) { return dim.kind == "point_distance"; });
  if (!expect(dim_it != params.dimensions.end(),
              "slot dim: point distance dimension created")) {
    return false;
  }

  // Drive it to 30 — the second slot must slide toward the first.
  document = manager.update_sketch_dimension(dim_it->id, 30.0);
  params = sketch_params(document);
  const auto second = std::find_if(
      params.slots.begin(), params.slots.end(),
      [&](const auto& slot) { return slot.id != params.slots[0].id; });
  if (second == params.slots.end()) return false;
  return expect(near(second->center_x, 30.0) && near(second->center_y, 0.0),
                "slot dim: driving the distance moves the second slot");
}

bool test_slot_extrude_smoke_and_delete() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");

  DocumentState document = manager.add_sketch_slot(10.0, 10.0, 20.0, 3.0, 0.0);
  const auto params = sketch_params(document);
  const std::string slot_id = params.slots[0].id;
  const std::string profile_id = params.profiles[0].id;

  // Extrude the stadium profile to a body.
  document = manager.extrude_profile(profile_id, 5.0, "new_body");
  const auto compiled = polysmith::core::compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1,
              "slot: extrude produces a body from the slot profile")) {
    return false;
  }

  // Delete the slot via the selection path: the record, the generated
  // entities, and the profile all disappear. The extrude above cleared
  // the active sketch — re-enter the sketch first (mirrors the UI flow).
  const auto sketch_feature = std::find_if(
      document.feature_history.begin(), document.feature_history.end(),
      [](const auto& feature) { return feature.kind == "sketch"; });
  if (!expect(sketch_feature != document.feature_history.end(),
              "slot: sketch feature present after extrude")) {
    return false;
  }
  document = manager.reenter_sketch(sketch_feature->id);
  document = manager.delete_sketch_selection({slot_id}, {}, {});
  const auto after_sketch = std::find_if(
      document.feature_history.begin(), document.feature_history.end(),
      [](const auto& feature) { return feature.kind == "sketch"; });
  if (after_sketch == document.feature_history.end()) return false;
  const auto after = after_sketch->sketch_parameters.value();
  const bool record_gone = std::none_of(
      after.slots.begin(), after.slots.end(),
      [&](const auto& slot) { return slot.id == slot_id; });
  const bool generated_gone =
      after.lines.empty() && after.arcs.empty() && after.profiles.empty();
  return expect(record_gone && generated_gone,
                "slot: delete removes record + generated entities + profile");
}

}  // namespace

int main() {
  if (!test_slot_creation_expands_to_two_lines_two_arcs()) return 1;
  if (!test_slot_rotated_geometry_both_signs()) return 1;
  if (!test_slot_full_profile_exact_id_set()) return 1;
  if (!test_slot_update_reexpands_geometry()) return 1;
  if (!test_slot_generated_entity_guards()) return 1;
  if (!test_slot_center_drag_and_move_preserve_shape()) return 1;
  if (!test_slot_distance_dimension_between_slots()) return 1;
  if (!test_slot_extrude_smoke_and_delete()) return 1;

  std::cout << "slot_test passed\n";
  return 0;
}
