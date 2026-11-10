#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <TopExp_Explorer.hxx>
#include <TopAbs.hxx>

#include "core/geometry/body_compiler.h"
#include "core/document/document.h"
#include "core/document/feature.h"

namespace {

using polysmith::core::compile_bodies;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FeatureEntry;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cout << "  FAIL: " << message << "\n";
  return false;
}

std::vector<std::string> profile_ids(const DocumentState& document) {
  std::vector<std::string> ids;
  for (const auto& feature : document.feature_history) {
    if (feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
      ids.clear();
      for (const auto& profile : feature.sketch_parameters->profiles) {
        ids.push_back(profile.id);
      }
      std::sort(ids.begin(), ids.end());
    }
  }
  return ids;
}

int extrude_feature_count(const DocumentState& document) {
  return static_cast<int>(
      std::count_if(document.feature_history.begin(),
                    document.feature_history.end(),
                    [](const FeatureEntry& feature) {
                      return feature.kind == "extrude";
                    }));
}

bool test_new_body_splits_disconnected_profiles() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);
  DocumentState document =
      manager.add_sketch_rectangle(30.0, 0.0, 40.0, 10.0);

  const std::vector<std::string> ids = profile_ids(document);
  if (!expect(ids.size() == 2,
              "expected two sketch profiles before extrude")) {
    return false;
  }

  document = manager.extrude_profiles(ids, 5.0, "new_body");
  const auto compiled = compile_bodies(document);
  return expect(extrude_feature_count(document) == 2,
                "expected two extrude features for disconnected new bodies") &&
         expect(compiled.bodies.size() == 2,
                "expected two compiled bodies for disconnected new bodies");
}

bool test_join_groups_touching_profiles_without_merging_distant_profiles() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);
  // Corner-touching (shared point at (10,10)) rather than edge-sharing:
  // coincident overlapping edges are ambiguous for the exact arrangement
  // (documented limitation) and would merge the two faces.
  manager.add_sketch_rectangle(10.0, 10.0, 20.0, 20.0);
  DocumentState document =
      manager.add_sketch_rectangle(40.0, 0.0, 50.0, 10.0);

  const std::vector<std::string> ids = profile_ids(document);
  if (!expect(ids.size() == 3,
              "expected three sketch profiles before joined extrude")) {
    return false;
  }

  document = manager.extrude_profiles(ids, 5.0, "join");
  const auto compiled = compile_bodies(document);
  return expect(extrude_feature_count(document) == 2,
                "expected touching profiles to share one join feature") &&
         expect(compiled.bodies.size() == 2,
                "expected joined touching profiles plus distant profile to make two bodies");
}

bool test_join_adjacent_profiles_creates_one_body_feature() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);
  // Corner-touching: coincident overlapping edges are ambiguous for the
  // exact arrangement (documented limitation).
  DocumentState document =
      manager.add_sketch_rectangle(10.0, 10.0, 20.0, 20.0);

  const std::vector<std::string> ids = profile_ids(document);
  if (!expect(ids.size() == 2,
              "expected two sketch profiles before adjacent join")) {
    return false;
  }

  document = manager.extrude_profiles(ids, 5.0, "join");
  const auto compiled = compile_bodies(document);
  const auto extrude_it = std::find_if(
      document.feature_history.begin(),
      document.feature_history.end(),
      [](const FeatureEntry& feature) { return feature.kind == "extrude"; });
  return expect(extrude_feature_count(document) == 1,
                "expected adjacent joined profiles to share one feature") &&
         expect(compiled.bodies.size() == 1,
                "expected adjacent joined profiles to make one body") &&
         expect(extrude_it != document.feature_history.end() &&
                    extrude_it->extrude_parameters.has_value() &&
                    extrude_it->extrude_parameters->mode == "new_body" &&
                    extrude_it->extrude_parameters->operation == "join",
                "expected untargeted join to remain a visible new body");
}

bool test_automatic_mode_joins_adjacent_profiles() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);
  // Corner-touching: coincident overlapping edges are ambiguous for the
  // exact arrangement (documented limitation).
  DocumentState document =
      manager.add_sketch_rectangle(10.0, 10.0, 20.0, 20.0);

  const std::vector<std::string> ids = profile_ids(document);
  document = manager.extrude_profiles(ids, 5.0, "");
  const auto compiled = compile_bodies(document);
  const auto extrude_it = std::find_if(
      document.feature_history.begin(),
      document.feature_history.end(),
      [](const FeatureEntry& feature) { return feature.kind == "extrude"; });
  return expect(extrude_feature_count(document) == 1,
                "expected automatic adjacent profiles to share one feature") &&
         expect(compiled.bodies.size() == 1,
                "expected automatic adjacent profiles to make one body") &&
         expect(extrude_it != document.feature_history.end() &&
                    extrude_it->extrude_parameters.has_value() &&
                    extrude_it->extrude_parameters->mode == "new_body" &&
                    extrude_it->extrude_parameters->operation == "join",
                "expected automatic adjacent profiles to record join operation");
}

bool test_automatic_mode_joins_touching_existing_body() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);
  manager.extrude_profiles(profile_ids(document), 5.0, "new_body");

  manager.start_sketch_on_plane("ref-plane-xy");
  // Corner-touching the first rectangle at (10,10) — coincident
  // overlapping edges are ambiguous for the exact arrangement.
  document = manager.add_sketch_rectangle(10.0, 10.0, 20.0, 20.0);
  document = manager.extrude_profiles(profile_ids(document), 5.0, "");
  const auto compiled = compile_bodies(document);
  const auto& last = document.feature_history.back();
  return expect(compiled.bodies.size() == 1,
                "expected touching automatic extrude to join existing body") &&
         expect(last.extrude_parameters.has_value() &&
                    last.extrude_parameters->mode == "join" &&
                    last.extrude_parameters->operation == "join" &&
                    last.extrude_parameters->target_body_id.has_value(),
                "expected automatic touching extrude to target join");
}

bool test_automatic_mode_cuts_intersecting_existing_body() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);
  manager.extrude_profiles(profile_ids(document), 5.0, "new_body");

  manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.add_sketch_rectangle(5.0, 0.0, 15.0, 10.0);
  document = manager.extrude_profiles(profile_ids(document), 5.0, "");
  const auto compiled = compile_bodies(document);
  const auto& last = document.feature_history.back();
  return expect(compiled.bodies.size() == 1,
                "expected intersecting automatic extrude to keep one target body") &&
         expect(last.extrude_parameters.has_value() &&
                    last.extrude_parameters->mode == "cut" &&
                    last.extrude_parameters->operation == "cut" &&
                    last.extrude_parameters->target_body_id.has_value(),
                "expected automatic intersecting extrude to target cut");
}

bool test_new_body_touching_profiles_produces_one_body() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);
  // Corner-touching: coincident overlapping edges are ambiguous for the
  // exact arrangement (documented limitation).
  DocumentState document =
      manager.add_sketch_rectangle(10.0, 10.0, 20.0, 20.0);

  const std::vector<std::string> ids = profile_ids(document);
  if (!expect(ids.size() == 2,
              "expected two sketch profiles before new_body extrude")) {
    return false;
  }

  document = manager.extrude_profiles(ids, 5.0, "new_body");
  const auto compiled = compile_bodies(document);

  // The two profiles share ONE extrude feature and compile into ONE
  // body entry. Inside that body, two prisms that touch only along a
  // single edge legitimately stay two solids (a non-manifold union is
  // not a valid solid). Before the 2026-08 face-walk hole fix the
  // first rectangle's profile carried its own exterior as a spurious
  // hole, which destroyed its prism and left exactly one solid by
  // accident. Two solids is the correct geometry here.
  int solid_count = 0;
  if (!compiled.bodies.empty()) {
    for (TopExp_Explorer exp(compiled.bodies.front().shape, TopAbs_SOLID);
         exp.More(); exp.Next()) {
      ++solid_count;
    }
  }

  return expect(extrude_feature_count(document) == 1,
                "expected touching new_body profiles to share one feature") &&
         expect(compiled.bodies.size() == 1,
                "expected touching new_body profiles to produce one body") &&
         expect(solid_count == 2,
                "expected two corner-touching prisms to remain two solids");
}

bool test_join_adjacent_compound_into_existing_body() {
  DocumentManager manager;
  manager.create_document();
  // Create a standalone body first.
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 5.0, 10.0);
  std::vector<std::string> ids = profile_ids(document);
  document = manager.extrude_profiles(ids, 5.0, "new_body");

  // Create two touching profiles on the same plane and extrude with
  // automatic mode — touches the existing body so auto-detects join.
  // Exercises the face-fused multi-profile solid joining into target.
  // Corner-touching: coincident overlapping edges are ambiguous for the
  // exact arrangement (documented limitation).
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(5.0, 0.0, 15.0, 10.0);
  document = manager.add_sketch_rectangle(15.0, 10.0, 25.0, 20.0);

  ids = profile_ids(document);
  if (!expect(ids.size() == 2,
              "expected two sketch profiles for join-into-existing")) {
    return false;
  }

  // Automatic mode (empty string) — body contact detection should
  // promote this to a join targeting the existing body.
  document = manager.extrude_profiles(ids, 5.0, "");
  const auto compiled = compile_bodies(document);
  const auto& last = document.feature_history.back();
  return expect(compiled.bodies.size() == 1,
                "expected auto-detected join to fuse into one body") &&
         expect(last.extrude_parameters.has_value() &&
                    last.extrude_parameters->mode == "join" &&
                    last.extrude_parameters->target_body_id.has_value(),
                "expected auto mode to target the existing body");
}

bool test_cut_adjacent_compound_from_existing_body() {
  DocumentManager manager;
  manager.create_document();
  // Create a standalone body first — wide enough to contain the cut.
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 30.0, 10.0);
  std::vector<std::string> ids = profile_ids(document);
  document = manager.extrude_profiles(ids, 5.0, "new_body");

  // Create two corner-touching profiles inside the existing body and
  // extrude with automatic mode — intersects so auto-detects cut.
  // Corner-touching: coincident overlapping edges are ambiguous for the
  // exact arrangement (documented limitation).
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(5.0, 0.0, 15.0, 5.0);
  document = manager.add_sketch_rectangle(15.0, 5.0, 25.0, 10.0);

  ids = profile_ids(document);
  if (!expect(ids.size() == 2,
              "expected two sketch profiles for cut-from-existing")) {
    return false;
  }

  // Automatic mode (empty string) — body intersection detection should
  // promote this to a cut targeting the existing body.
  document = manager.extrude_profiles(ids, 5.0, "");
  const auto compiled = compile_bodies(document);
  const auto& last = document.feature_history.back();
  std::cerr << "cut test: bodies=" << compiled.bodies.size()
            << " mode=" << (last.extrude_parameters.has_value()
                                ? last.extrude_parameters->mode
                                : "none")
            << "\n";
  return expect(compiled.bodies.size() == 1,
                "expected auto-detected cut to keep one body") &&
         expect(last.extrude_parameters.has_value() &&
                    last.extrude_parameters->mode == "cut" &&
                    last.extrude_parameters->target_body_id.has_value(),
                "expected auto mode intersecting to target cut");
}

// One extrude gesture must read as ONE timeline operation even when it makes
// several bodies. Fusion shows "1 extrude, 2 bodies"; before this, two
// disconnected surfaces produced two unrelated-looking timeline blocks, and a
// user re-picking surfaces repeatedly accumulated them (reported as "selected
// multiple surfaces in one operation but when I commit created 5 extrusion
// operation").
bool test_one_commit_shares_one_operation_id() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);
  DocumentState document =
      manager.add_sketch_rectangle(30.0, 0.0, 40.0, 10.0);

  const std::vector<std::string> ids = profile_ids(document);
  if (!expect(ids.size() == 2, "fixture: two profiles")) {
    return false;
  }

  document = manager.extrude_profiles(ids, 5.0, "new_body");
  const FeatureEntry* first = nullptr;
  const FeatureEntry* second_feature = nullptr;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "extrude") {
      continue;
    }
    if (first == nullptr) {
      first = &feature;
    } else {
      second_feature = &feature;
    }
  }
  if (!expect(first != nullptr && second_feature != nullptr,
              "fixture: two extrude features")) {
    return false;
  }
  if (!expect(!first->operation_id.empty() &&
                  first->operation_id == second_feature->operation_id,
              "one commit: both features share one operation id")) {
    return false;
  }
  if (!expect(first->operation_id == first->id,
              "one commit: the first member's id names the operation")) {
    return false;
  }

  // A second, separate gesture must NOT join the first operation.
  manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.add_sketch_rectangle(60.0, 0.0, 70.0, 10.0);
  document = manager.extrude_profiles(profile_ids(document), 5.0, "new_body");
  const FeatureEntry& latest = document.feature_history.back();
  return expect(latest.kind == "extrude" &&
                    latest.operation_id == latest.id &&
                    latest.operation_id != first->operation_id,
                "second commit: a new operation id");
}

// Fusion's deletion rule: an operation is the unit. Deleting ONE body of a
// multi-surface extrude removes all of its bodies and the single timeline
// entry; deleting a standalone feature still removes exactly that feature.
bool test_deleting_a_body_deletes_its_whole_operation() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);
  DocumentState document =
      manager.add_sketch_rectangle(30.0, 0.0, 40.0, 10.0);

  document = manager.extrude_profiles(profile_ids(document), 5.0, "new_body");
  std::vector<std::string> extrude_ids;
  for (const auto& feature : document.feature_history) {
    if (feature.kind == "extrude") {
      extrude_ids.push_back(feature.id);
    }
  }
  if (!expect(extrude_ids.size() == 2,
              "fixture: two bodies from one operation")) {
    return false;
  }
  if (!expect(compile_bodies(document).bodies.size() == 2,
              "fixture: two bodies compile")) {
    return false;
  }

  // Delete the SECOND body — the whole operation must go.
  document = manager.delete_feature(extrude_ids[1]);
  const int remaining = extrude_feature_count(document);
  if (!expect(remaining == 0,
              "deleting one body of an operation removes every body of it")) {
    return false;
  }

  // A standalone feature is still deleted on its own.
  manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.add_sketch_rectangle(60.0, 0.0, 70.0, 10.0);
  document = manager.extrude_profiles(profile_ids(document), 5.0, "new_body");
  const std::string lone = document.feature_history.back().id;
  document = manager.delete_feature(lone);
  return expect(extrude_feature_count(document) == 0 &&
                    compile_bodies(document).bodies.empty(),
                "deleting a standalone feature removes only it");
}

// Arming Extrude first and picking surfaces afterwards must build the SAME
// model as picking them first. Reported: "pre selecting the surfaces makes 2
// body one operation in time line and selecting after arming the extrude gives
// one body (but 2 disconnected cylinders) and one operation" — the live re-bind
// path put every region into one feature, i.e. one body holding two
// disconnected solids, which later booleans/fillets cannot treat separately.
bool test_live_rebind_splits_like_creation() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);
  DocumentState document =
      manager.add_sketch_rectangle(30.0, 0.0, 40.0, 10.0);
  const std::vector<std::string> ids = profile_ids(document);
  if (!expect(ids.size() == 2, "fixture: two profiles")) {
    return false;
  }

  // Arm with ONE surface, then add the second one while the panel is open.
  document = manager.extrude_profiles({ids[0]}, 5.0, "new_body");
  std::string feature_id;
  for (const auto& feature : document.feature_history) {
    if (feature.kind == "extrude") {
      feature_id = feature.id;
    }
  }
  if (!expect(!feature_id.empty(), "fixture: one extrude feature")) {
    return false;
  }
  document = manager.update_extrude_profiles(feature_id, ids);

  const int grown = extrude_feature_count(document);
  const auto grown_bodies = compile_bodies(document);
  std::string grown_operation;
  for (const auto& feature : document.feature_history) {
    if (feature.kind == "extrude") {
      grown_operation = feature.operation_id;
    }
  }
  if (!expect(grown == 2,
              "live re-bind: two disconnected surfaces make two bodies")) {
    return false;
  }
  if (!expect(grown_bodies.bodies.size() == 2,
              "live re-bind: both bodies compile (regression: one body holding "
              "two disconnected solids)")) {
    return false;
  }
  if (!expect(!grown_operation.empty(),
              "live re-bind: the new body joins the same operation")) {
    return false;
  }

  // Shrinking the selection back to one surface must remove the extra body
  // rather than leave an orphan behind.
  document = manager.update_extrude_profiles(feature_id, {ids[0]});
  const int shrunk = extrude_feature_count(document);
  return expect(shrunk == 1 &&
                    compile_bodies(document).bodies.size() == 1,
                "live re-bind: deselecting a surface removes its body");
}

// Two TANGENT circles inside a plate: the plate's hole is a self-touching loop
// (the face walk welds both closed circle edges into one figure-8 boundary), and
// capping that face silently produced a 100x60x5 plate inflated to 106x87x72.
// It must be refused with a message instead. The circles themselves keep
// extruding; the invalid one is the plate whose hole touches itself.
bool test_a_self_touching_hole_is_refused() {
  const auto build_plate = [](double second_circle_x, DocumentManager& manager) {
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    manager.add_sketch_rectangle(0.0, 0.0, 100.0, 60.0);
    manager.add_sketch_circle(30.0, 30.0, 10.0);
    return manager.add_sketch_circle(second_circle_x, 30.0, 10.0);
  };

  DocumentManager manager;
  DocumentState document = build_plate(50.0, manager);  // exactly tangent
  std::string plate_id;
  for (const auto& profile : profile_ids(document)) {
    if (profile.find("profile-poly-line-") == 0) {
      plate_id = profile;
    }
  }
  if (!expect(!plate_id.empty(), "fixture: the plate region was detected")) {
    return false;
  }

  bool refused = false;
  std::string message;
  try {
    manager.extrude_profiles({plate_id}, 5.0, "new_body");
  } catch (const std::exception& error) {
    refused = true;
    message = error.what();
  }
  if (!expect(refused,
              "tangent holes: the extrude is refused (regression: an invalid "
              "body inflated to 106x87x72)")) {
    return false;
  }
  if (!expect(message.find("touches itself") != std::string::npos,
              "tangent holes: the message says what is wrong")) {
    std::cerr << "  message=" << message << "\n";
    return false;
  }

  // Control: the same plate with the circles a hair apart is fine.
  DocumentManager apart_manager;
  DocumentState apart = build_plate(50.2, apart_manager);
  std::string apart_plate;
  for (const auto& profile : profile_ids(apart)) {
    if (profile.find("profile-poly-line-") == 0) {
      apart_plate = profile;
    }
  }
  try {
    apart = apart_manager.extrude_profiles({apart_plate}, 5.0, "new_body");
  } catch (const std::exception& error) {
    std::cerr << "  control threw: " << error.what() << "\n";
    return expect(false, "control: a 0.2 mm gap extrudes normally");
  }
  return expect(compile_bodies(apart).bodies.size() == 1,
                "control: a 0.2 mm gap leaves one plate body");
}

// A hole made of ARCS must be cut out of the cap. Reported: "during preview I
// can see the body with the 2 circles extruded as holes but after commit I see
// the holes extruded as cylinders and the rectangle is extruded only a thin
// wall". The shape was right (STEP export: the caps carry two wires each), but
// the hole wire was added as an ISLAND because its orientation was decided from
// the walk's CHORD shoelace — two arcs are two chords, whose sign is arbitrary.
// BRepMesh then meshed each cap as the hole alone, so the viewport showed bare
// walls plus a cylinder and no plate surface.
//
// The mesh is where this is visible, so assert on the tessellation: the caps of
// a 100x70 plate with one merged ~2000 mm² hole must total ~10000 mm², not the
// hole's own ~4000 mm².
bool test_a_two_arc_hole_is_cut_out_of_the_cap() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 100.0, 70.0);
  manager.add_sketch_circle(30.0, 35.0, 20.0);
  DocumentState document = manager.add_sketch_circle(50.0, 35.0, 20.0);

  std::string plate_id;
  for (const auto& profile : profile_ids(document)) {
    if (profile.find("profile-poly-line-") == 0) {
      plate_id = profile;
    }
  }
  if (!expect(!plate_id.empty(), "fixture: the plate region was detected")) {
    return false;
  }
  document = manager.extrude_profiles({plate_id}, 20.0, "new_body");

  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1 && compiled.meshes.size() == 1,
              "fixture: one body with a mesh")) {
    return false;
  }
  const auto& mesh = compiled.meshes.front();
  const auto& v = mesh.vertices;
  const auto& idx = mesh.indices;
  double cap_area = 0.0;
  for (size_t t = 0; t + 2 < idx.size(); t += 3) {
    const auto p = [&](int vertex) {
      const size_t b = static_cast<size_t>(vertex) * 3;
      return std::array<double, 3>{v[b], v[b + 1], v[b + 2]};
    };
    const auto a = p(idx[t]);
    const auto b = p(idx[t + 1]);
    const auto c = p(idx[t + 2]);
    const double ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
    const double vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
    const double nx = uy * vz - uz * vy;
    const double ny = uz * vx - ux * vz;
    const double nz = ux * vy - uy * vx;
    const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (length <= 1e-12) {
      continue;
    }
    if (std::abs(nz) / length > 0.9) {
      cap_area += 0.5 * length;
    }
  }
  // Both caps of the plate: 2 x (7000 - ~2022) ≈ 9956 mm².
  if (!expect(cap_area > 9000.0,
              "two-arc hole: both cap faces are meshed (regression: the caps "
              "covered the hole only, ~4000 mm², and the plate looked like a "
              "thin wall)")) {
    std::cerr << "  cap area=" << cap_area << "\n";
    return false;
  }
  return true;
}

// A new feature must not land BEYOND a rolled-back timeline cursor: the
// compiler stops at the cursor, so the feature shows in the timeline, reports
// healthy, and produces no body — and the cursor never recovers, because every
// later edit session records "the cursor is set" and leaves it alone. Measured
// on the reporter's file: cursor 2, three features, the third body missing
// ("the small circle body is not visible", "very erratic").
bool test_a_new_feature_is_not_hidden_by_the_timeline_cursor() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);
  DocumentState document =
      manager.add_sketch_rectangle(30.0, 0.0, 40.0, 10.0);
  document = manager.extrude_profiles(profile_ids(document), 5.0, "new_body");
  if (!expect(compile_bodies(document).bodies.size() == 2,
              "fixture: two bodies before the roll-back")) {
    return false;
  }

  // Roll the history back to the sketch, as the timeline's edit-in-context does.
  // `build_viewport_state` renders `document_at_timeline_cursor`, so everything
  // beyond the cursor is not drawn — that is what made the new body invisible
  // while its feature sat, healthy, in the timeline.
  document = manager.set_timeline_cursor(1);
  if (!expect(document.timeline_cursor.has_value(),
              "fixture: the history is rolled back")) {
    return false;
  }

  document = manager.extrude_profiles(profile_ids(document), 5.0, "new_body");
  if (!expect(!document.timeline_cursor.has_value(),
              "a created feature moves the marker to the end, so the viewport "
              "draws it (regression: healthy feature, no body, nothing in the "
              "Logs panel)")) {
    return false;
  }
  // The UI asks for "the end of history" with a sentinel, because a count read
  // from its store can lag the core by one feature right after an edit that
  // created one. The clamp is the contract it relies on.
  document = manager.set_timeline_cursor(1);
  document = manager.set_timeline_cursor(2147483647);
  if (!expect(!document.timeline_cursor.has_value(),
              "a sentinel count means the end, whatever the history holds")) {
    return false;
  }
  return expect(compile_bodies(document).bodies.size() == 4,
                "every body compiles");
}

// The timeline cursor is a view state and must not survive a save: a file that
// carries one opens with fewer bodies than the hierarchy lists, and the marker
// looks stuck (reported: "loading the saved file shows 2 bodies in the hierarchy
// but only one visible … clicking near the extrude makes the body show up").
bool test_the_timeline_cursor_does_not_survive_a_save() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);
  DocumentState document =
      manager.add_sketch_rectangle(30.0, 0.0, 40.0, 10.0);
  document = manager.extrude_profiles(profile_ids(document), 5.0, "new_body");
  document = manager.set_timeline_cursor(1);
  if (!expect(document.timeline_cursor.has_value(),
              "fixture: the history is rolled back")) {
    return false;
  }

  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "polysmith_cursor_save.polysmith";
  manager.save_document_to_path(path.string());

  DocumentManager reopened;
  const DocumentState loaded = reopened.load_document_from_path(path.string());
  std::filesystem::remove(path);
  if (!expect(!loaded.timeline_cursor.has_value(),
              "a loaded document is not left rolled back")) {
    return false;
  }
  return expect(compile_bodies(loaded).bodies.size() == 2,
                "both bodies are present after a reload");
}

}  // namespace

#define RUN_TEST(name)                                  \
  do {                                                  \
    std::cout << "Running " #name "...\n";              \
    if (!name()) {                                      \
      std::cout << "  -> " #name " FAILED\n";           \
      return 1;                                         \
    }                                                   \
    std::cout << "  -> " #name " passed\n";             \
  } while (0)

int main() {
  RUN_TEST(test_new_body_splits_disconnected_profiles);
  RUN_TEST(test_join_groups_touching_profiles_without_merging_distant_profiles);
  RUN_TEST(test_join_adjacent_profiles_creates_one_body_feature);
  RUN_TEST(test_automatic_mode_joins_adjacent_profiles);
  RUN_TEST(test_automatic_mode_joins_touching_existing_body);
  RUN_TEST(test_automatic_mode_cuts_intersecting_existing_body);
  RUN_TEST(test_new_body_touching_profiles_produces_one_body);
  RUN_TEST(test_join_adjacent_compound_into_existing_body);
  RUN_TEST(test_cut_adjacent_compound_from_existing_body);
  RUN_TEST(test_one_commit_shares_one_operation_id);
  RUN_TEST(test_deleting_a_body_deletes_its_whole_operation);
  RUN_TEST(test_live_rebind_splits_like_creation);
  RUN_TEST(test_a_self_touching_hole_is_refused);
  RUN_TEST(test_a_two_arc_hole_is_cut_out_of_the_cap);
  RUN_TEST(test_a_new_feature_is_not_hidden_by_the_timeline_cursor);
  RUN_TEST(test_the_timeline_cursor_does_not_survive_a_save);

  std::cout << "multi_profile_extrude_test passed\n";
  return 0;
}
