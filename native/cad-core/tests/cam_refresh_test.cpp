// CAM dependency refresh test — the TNP degradation matrix.
//
// refresh_cam_dependencies runs inside every bump_geometry_revision:
// operations re-resolve their references and degrade with status
// "error" + a human-readable message (never a throw, never a guess).
// A toolpath cached at the current revision keeps an op "generated";
// any real mutation invalidates it back to "needs_regenerate".

#include <iostream>
#include <string>
#include <variant>

#include "core/cam/cam_generate.h"
#include "core/cam/cam_operation.h"
#include "core/cam/cam_profile_reference.h"
#include "core/cam/cam_runtime.h"
#include "core/document/document.h"
#include "core/geometry/body_compiler.h"

#include <BRepAdaptor_Surface.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

namespace {

using polysmith::core::CamOperation;
using polysmith::core::CamSetup;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FaceAttestation;
using polysmith::core::GeometryReference;
using polysmith::core::SketchProfileAttestation;
using polysmith::core::ToolEntry;
using polysmith::core::Toolpath;
using polysmith::core::ToolpathMove;
using polysmith::core::ToolpathMoveKind;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

const CamOperation* find_op(const DocumentState& document,
                            const std::string& op_id) {
  for (const auto& op : document.cam.operations) {
    if (op.op_id == op_id) {
      return &op;
    }
  }
  return nullptr;
}

// Laser setup + tool + laser op over a sketch profile (selected
// profiles get captured into the op as witness references).
std::string make_laser_op(DocumentManager& manager,
                          const DocumentState& document,
                          const std::string& sketch_feature_id,
                          const std::string& profile_id) {
  const auto setup = manager.cam_setup_create(
      [] {
        CamSetup s;
        s.name = "Laser setup";
        s.machine_type = "laser";
        return s;
      }());
  const auto withTool = manager.cam_tool_add(
      [] {
        ToolEntry t;
        t.name = "CO2 laser";
        t.type = "laser";
        return t;
      }());

  CamOperation op;
  op.name = "2D Cut 1";
  op.type = "laser_cut";
  op.tool_id = withTool.cam.tool_library[0].tool_id;

  SketchProfileAttestation att;
  att.sketch_feature_id = sketch_feature_id;
  att.profile_id = profile_id;
  const auto& sketch =
      [&]() -> const polysmith::core::SketchFeatureParameters& {
    for (const auto& feature : document.feature_history) {
      if (feature.id == sketch_feature_id &&
          feature.sketch_parameters.has_value()) {
        return feature.sketch_parameters.value();
      }
    }
    throw std::runtime_error("sketch not found");
  }();
  for (const auto& region : sketch.profiles) {
    if (region.id == profile_id) {
      const auto ref =
          polysmith::core::capture_profile_reference(sketch_feature_id, region);
      if (!ref.has_value()) {
        throw std::runtime_error("profile capture failed");
      }
      att.center_x = ref->centerX;
      att.center_y = ref->centerY;
      att.area = ref->area;
      att.min_x = ref->minX;
      att.min_y = ref->minY;
      att.max_x = ref->maxX;
      att.max_y = ref->maxY;
      att.boundary_edge_kinds = ref->boundaryEdgeKinds;
      att.inner_loop_count = ref->innerLoopCount;
      att.source_circle_id = ref->sourceCircleId;
      break;
    }
  }
  GeometryReference stored;
  stored.persistent_id = profile_id;
  stored.attestation = att;
  op.geometry_references.machining_regions.push_back(stored);

  const auto withOp = manager.cam_operation_add(op);
  return withOp.cam.operations.back().op_id;
}

// ── Test 1: a cached path survives as "generated" until an edit ───

bool test_generated_status_and_invalidation() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);

  const auto& sketch =
      document.feature_history.back().sketch_parameters.value();
  const std::string sketchId = document.feature_history.back().id;
  const std::string profileId = sketch.profiles[0].id;
  const std::string opId =
      make_laser_op(manager, document, sketchId, profileId);

  document = manager.get_document().value();
  if (!expect(find_op(document, opId)->status == "pending",
              "refresh: fresh op starts pending")) {
    return false;
  }

  // Fake a generated path (the laser generator lands in the next
  // task; the refresh machinery must not depend on it).
  Toolpath path;
  path.op_id = opId;
  path.moves.push_back({ToolpathMoveKind::Rapid, 0.0, 0.0, 5.0});
  path.moves.push_back({ToolpathMoveKind::FeedLinear, 20.0, 0.0, 0.0});
  polysmith::core::cam_runtime::store_generated(document, opId, path);
  document = manager.cam_operation_set_generated(opId, *find_op(document, opId));
  if (!expect(find_op(document, opId)->status == "generated",
              "refresh: generated after storing a path")) {
    return false;
  }

  // An unrelated CAM mutation bumps the revision → the cache entry is
  // stale → the op needs regeneration again.
  document = manager.cam_tool_add(
      [] {
        ToolEntry t;
        t.name = "second tool";
        t.type = "laser";
        return t;
      }());
  if (!expect(find_op(document, opId)->status == "needs_regenerate",
              "refresh: any bump flips generated → needs_regenerate")) {
    return false;
  }
  if (!expect(polysmith::core::cam_runtime::cached_toolpath(document, opId) ==
                  nullptr,
              "refresh: stale cache entry dropped")) {
    return false;
  }

  // Regenerate (store at the new revision) and confirm the status
  // sticks without further bumps.
  polysmith::core::cam_runtime::store_generated(document, opId, path);
  document = manager.cam_operation_set_generated(opId, *find_op(document, opId));
  return expect(find_op(document, opId)->status == "generated",
                "refresh: generated sticks until the next real mutation");
}

// ── Test 2: deleting the source sketch degrades the op ────────────

bool test_sketch_deletion_degrades() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);

  const auto& sketch =
      document.feature_history.back().sketch_parameters.value();
  const std::string sketchId = document.feature_history.back().id;
  const std::string opId =
      make_laser_op(manager, document, sketchId, sketch.profiles[0].id);

  // The active sketch cannot be deleted — finish it first (mirrors the
  // UI flow: leave the sketch, then delete it from the tree).
  document = manager.finish_sketch();
  document = manager.delete_feature(sketchId);
  const auto* op = find_op(document, opId);
  if (!expect(op != nullptr, "delete sketch: op still exists")) {
    return false;
  }
  if (!expect(op->status == "error",
              "delete sketch: op degrades to error")) {
    std::cerr << "  status: " << op->status << "\n";
    return false;
  }
  return expect(!op->status_message.empty(),
                "delete sketch: error carries a human message");
}

// ── Test 3: face reference resolves through a box edit ────────────

bool test_face_op_survives_unrelated_edit() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document =
      manager.add_box_feature({.width = 20.0, .height = 20.0, .depth = 10.0});
  manager.cam_setup_create(
      [] {
        CamSetup s;
        s.name = "Mill setup";
        s.machine_type = "3_axis_mill";
        return s;
      }());
  document = manager.cam_tool_add(
      [] {
        ToolEntry t;
        t.name = "6mm endmill";
        t.type = "endmill_flat";
        return t;
      }());

  // Capture the top face as an attestation via the face machinery.
  const auto compiled = polysmith::core::compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1, "face: one body")) {
    return false;
  }
  const auto& body = compiled.bodies[0];
  // Find the UPWARD face by orientation-corrected normal (OCCT face
  // ordering is not contractual, and both box caps parameterize +Z —
  // only the orientation distinguishes them).
  int topIndex = -1;
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faceMap;
  TopExp::MapShapes(body.shape, TopAbs_FACE, faceMap);
  for (int i = 1; i <= faceMap.Extent(); ++i) {
    const auto face = TopoDS::Face(faceMap(i));
    try {
      BRepAdaptor_Surface surface(face);
      const double uMid =
          0.5 * (surface.FirstUParameter() + surface.LastUParameter());
      const double vMid =
          0.5 * (surface.FirstVParameter() + surface.LastVParameter());
      gp_Pnt center;
      gp_Vec d1u, d1v;
      surface.D1(uMid, vMid, center, d1u, d1v);
      gp_Vec normal = d1u.Crossed(d1v);
      if (normal.Magnitude() > 1e-12) {
        normal.Normalize();
        if (face.Orientation() == TopAbs_REVERSED) {
          normal.Reverse();
        }
        if (normal.Z() > 0.99) {
          topIndex = i - 1;
          break;
        }
      }
    } catch (const std::exception&) {
      continue;
    }
  }
  if (!expect(topIndex >= 0, "face: found the top face")) {
    return false;
  }

  const auto ref = polysmith::core::capture_face_reference(
      body.id, body.shape, topIndex, "top");
  if (!expect(ref.has_value(), "face: witness captured")) {
    return false;
  }

  FaceAttestation att;
  att.area = ref->capturedArea;
  att.normal = ref->capturedNormal;
  for (const auto& p : ref->samplePoints) {
    att.sample_points.push_back(p);
  }
  GeometryReference stored;
  stored.persistent_id = body.id + ":face:" + std::to_string(topIndex);
  stored.attestation = att;

  CamOperation op;
  op.name = "Face 1";
  op.type = "face_milling";
  op.tool_id = document.cam.tool_library[0].tool_id;
  op.geometry_references.machining_regions.push_back(stored);
  document = manager.cam_operation_add(op);
  const std::string opId = document.cam.operations.back().op_id;
  if (!expect(find_op(document, opId)->status == "pending",
              "face: fresh op pending (reference resolved)")) {
    return false;
  }

  // An unrelated edit (second box) must not break the face reference.
  document = manager.add_box_feature(
      {.width = 5.0, .height = 5.0, .depth = 5.0});
  const auto* after = find_op(document, opId);
  return expect(after->status != "error",
                "face: unrelated edit keeps the reference resolvable");
}

// ── Test 4: ambiguous reference never guessed ─────────────────────
//
// A witness whose centroid sits exactly between two identical regions
// produces a genuine near-tie; the refresh must degrade with a message
// instead of picking one.  (Separated identical regions resolve by
// centroid — covered by the profile reference test.)

bool test_ambiguous_reference_degrades() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);
  document = manager.add_sketch_rectangle(50.0, 0.0, 70.0, 10.0);

  const auto& sketch =
      document.feature_history.back().sketch_parameters.value();
  const std::string sketchId = document.feature_history.back().id;
  const std::string opId =
      make_laser_op(manager, document, sketchId, sketch.profiles[0].id);

  // Re-center the stored witness between the two identical regions so
  // both score a near-tie.
  document = manager.get_document().value();
  auto modified = *find_op(document, opId);
  for (auto& ref : modified.geometry_references.machining_regions) {
    if (std::holds_alternative<SketchProfileAttestation>(
            ref.attestation)) {
      auto& att = std::get<SketchProfileAttestation>(ref.attestation);
      att.center_x = 35.0;  // midpoint of the two rect centers
      att.center_y = 5.0;
    }
  }
  document = manager.cam_operation_update(opId, modified);

  // The refresh runs inside the update's bump; ambiguity must already
  // be visible — never a silent guess.
  const auto* op = find_op(document, opId);
  if (!expect(op != nullptr && op->status == "error",
              "ambiguous: refresh degrades instead of guessing")) {
    std::cerr << "  status: " << (op ? op->status : "<gone>") << "\n";
    return false;
  }
  return expect(!op->status_message.empty(),
                "ambiguous: degradation carries a human message");
}

bool test_wcs_origin_populated() {
  DocumentManager manager;
  manager.create_document();

  CamSetup setup;
  setup.name = "Sheet setup";
  setup.machine_type = "laser";
  setup.stock.origin = std::array<double, 3>{10.0, 20.0, 0.0};
  const auto afterCreate = manager.cam_setup_create(setup);

  // The refresh pass (inside the setup mutation) populates the
  // machine origin from the stock origin — the exporter subtracts it
  // from every coordinate.
  const auto& position = afterCreate.cam.setups[0].wcs_origin.position;
  if (!expect(position.has_value(), "wcs: position populated")) {
    return false;
  }
  return expect(position.value() == std::array<double, 3>({10.0, 20.0, 0.0}),
                "wcs: position follows the stock origin");
}

bool test_pointer_offset_shifts_wcs() {
  DocumentManager manager;
  manager.create_document();

  CamSetup setup;
  setup.name = "Sheet setup";
  setup.machine_type = "laser";
  setup.stock.origin = std::array<double, 3>{10.0, 20.0, 0.0};
  DocumentState document = manager.cam_setup_create(setup);

  // The red pointer sits at (5, 3) from the laser focal point — parts
  // framed under the dot cut at origin - offset.
  polysmith::core::LaserMachineSettings machine;
  machine.work_area_x_mm = 400.0;
  machine.work_area_y_mm = 400.0;
  machine.pointer_offset_x_mm = 5.0;
  machine.pointer_offset_y_mm = 3.0;
  document = manager.cam_machine_settings_set(machine);

  const auto& position = document.cam.setups[0].wcs_origin.position;
  if (!expect(position.has_value(), "pointer offset: position populated")) {
    return false;
  }
  return expect(position.value() == std::array<double, 3>({5.0, 17.0, 0.0}),
                "pointer offset: WCS shifted by -offset");
}

}  // namespace

int main() {
  bool allPassed = true;

  std::cout << "cam_refresh_test\n";
  const auto run = [&](const char* label, bool (*test)()) {
    std::cout << "  " << label << "... ";
    bool ok = false;
    try {
      ok = test();
    } catch (const std::exception& error) {
      std::cerr << "\n  EXCEPTION: " << error.what() << "\n";
    } catch (...) {
      std::cerr << "\n  UNKNOWN EXCEPTION\n";
    }
    if (ok) {
      std::cout << "PASS\n";
    } else {
      std::cout << "FAIL\n";
      allPassed = false;
    }
  };
  run("Test 1: generated status + invalidation",
      test_generated_status_and_invalidation);
  run("Test 2: sketch deletion degrades", test_sketch_deletion_degrades);
  run("Test 3: face op survives unrelated edit",
      test_face_op_survives_unrelated_edit);
  run("Test 4: ambiguous reference never guessed",
      test_ambiguous_reference_degrades);
  run("Test 5: WCS origin follows the stock origin", test_wcs_origin_populated);
  run("Test 6: pointer offset shifts the WCS", test_pointer_offset_shifts_wcs);

  if (allPassed) {
    std::cout << "cam_refresh_test passed\n";
    return 0;
  }
  return 1;
}

