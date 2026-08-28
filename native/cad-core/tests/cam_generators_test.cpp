// CAM generators test — the laser cutting generator end-to-end.
//
// Sketch profile → kerf offset → lead-in/out + pierce → ordering →
// toolpath IR.  Covers both kerf directions (outer loop outward, hole
// loop inward), hole-before-outer ordering, laser on/off transitions,
// exact circle arcs with I/J, engrave (no kerf/lead), and the
// narrower-than-kerf error path.

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "core/cam/cam_generate.h"
#include "core/cam/cam_generator.h"
#include "core/cam/cam_operation.h"
#include "core/cam/cam_profile_reference.h"
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
using polysmith::core::LaserCutParameters;
using polysmith::core::ToolEntry;
using polysmith::core::Toolpath;
using polysmith::core::ToolpathMoveKind;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

bool near(double a, double b, double tolerance = 1e-6) {
  return std::abs(a - b) < tolerance;
}

double dist(double x, double y, double cx, double cy) {
  return std::hypot(x - cx, y - cy);
}

// Creates a laser setup + tool + op capturing the named sketch profile
// as a witness reference.  Returns the op id.
std::string make_laser_op(DocumentManager& manager, DocumentState& document,
                          const std::string& sketch_feature_id,
                          const std::string& profile_id,
                          const LaserCutParameters& laser) {
  CamSetup setup;
  setup.name = "Laser setup";
  setup.machine_type = "laser";
  document = manager.cam_setup_create(setup);

  ToolEntry tool;
  tool.name = "CO2 laser";
  tool.type = "laser";
  document = manager.cam_tool_add(tool);

  CamOperation op;
  op.name = "2D Cut 1";
  op.type = "laser_cut";
  op.tool_id = document.cam.tool_library[0].tool_id;
  op.parameters.laser = laser;

  // Capture the witness reference from the sketch region directly.
  for (const auto& feature : document.feature_history) {
    if (feature.id != sketch_feature_id ||
        !feature.sketch_parameters.has_value()) {
      continue;
    }
    for (const auto& region : feature.sketch_parameters->profiles) {
      if (region.id != profile_id) {
        continue;
      }
      const auto ref =
          polysmith::core::capture_profile_reference(sketch_feature_id, region);
      if (!ref.has_value()) {
        throw std::runtime_error("profile capture failed");
      }
      polysmith::core::SketchProfileAttestation att;
      att.sketch_feature_id = ref->sketchFeatureId;
      att.profile_id = ref->profileId;
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
      polysmith::core::GeometryReference stored;
      stored.persistent_id = region.id;
      stored.attestation = att;
      op.geometry_references.machining_regions.push_back(stored);
      break;
    }
  }
  if (op.geometry_references.machining_regions.empty()) {
    throw std::runtime_error("no profile matched");
  }

  document = manager.cam_operation_add(op);
  return document.cam.operations.back().op_id;
}

std::string sketch_feature_id(const DocumentState& document) {
  for (const auto& feature : document.feature_history) {
    if (feature.kind == "sketch") {
      return feature.id;
    }
  }
  return "";
}

// ── Test 1: registry lookups ─────────────────────────────────────

bool test_registry() {
  return expect(polysmith::core::find_cam_generator("laser_cut") != nullptr,
                "registry: laser_cut found") &&
         expect(polysmith::core::find_cam_generator("pocket_2d") == nullptr,
                "registry: unregistered type reports null");
}

// ── Test 2: rectangle + hole — kerf directions, ordering, laser
// transitions, corner joins ────────────────────────────────────────

bool test_rectangle_with_hole() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);
  document = manager.add_sketch_circle(10.0, 5.0, 2.0);

  // Find the outer region (has a hole).
  std::string outerProfile;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    for (const auto& region : feature.sketch_parameters->profiles) {
      if (!region.inner_loops.empty()) {
        outerProfile = region.id;
      }
    }
  }
  if (!expect(!outerProfile.empty(), "rect+hole: outer region found")) {
    return false;
  }

  LaserCutParameters laser;
  laser.kerf_width_mm = 0.2;  // d = 0.1
  laser.lead_in_mm = 2.0;
  laser.lead_out_mm = 2.0;
  laser.pierce_dwell_seconds = 0.3;
  laser.power_percent = 85.0;
  const std::string opId = make_laser_op(
      manager, document, sketch_feature_id(document), outerProfile, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "rect+hole: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  // Laser state transitions: starts off, turns on, turns off again.
  bool sawOff = false;
  bool sawOn = false;
  bool sawOnToOff = false;
  bool previousOn = false;
  bool havePrevious = false;
  bool sawDwell = false;
  for (const auto& move : toolpath.moves) {
    if (!move.laser_on) {
      sawOff = true;
    } else {
      sawOn = true;
      if (move.dwell_seconds > 0.0) {
        sawDwell = true;
      }
    }
    if (havePrevious && previousOn && !move.laser_on) {
      sawOnToOff = true;
    }
    previousOn = move.laser_on;
    havePrevious = true;
  }
  if (!expect(sawOff && sawOn && sawOnToOff,
              "rect+hole: laser transitions off→on→off")) {
    std::cerr << "  moves=" << toolpath.moves.size()
              << " sawOff=" << sawOff << " sawOn=" << sawOn
              << " sawOnToOff=" << sawOnToOff << "\n";
    for (size_t m = 0; m < toolpath.moves.size(); ++m) {
      std::cerr << "  [" << m << "] kind="
                << static_cast<int>(toolpath.moves[m].kind)
                << " laser_on=" << toolpath.moves[m].laser_on
                << " x=" << toolpath.moves[m].x
                << " y=" << toolpath.moves[m].y << "\n";
    }
    return false;
  }
  if (!expect(sawDwell, "rect+hole: pierce dwell present")) {
    return false;
  }
  if (!expect(toolpath.moves.front().kind == ToolpathMoveKind::Rapid,
              "rect+hole: starts with a rapid")) {
    return false;
  }

  // Kerf direction — outer loop outward: a move sits at x ≈ -0.1
  // (left edge offset outward) and y ≈ -0.1 (bottom edge offset).
  bool sawMinX = false;
  bool sawMinY = false;
  for (const auto& move : toolpath.moves) {
    if (near(move.x, -0.1, 0.02)) {
      sawMinX = true;
    }
    if (near(move.y, -0.1, 0.02)) {
      sawMinY = true;
    }
  }
  if (!expect(sawMinX && sawMinY,
              "rect+hole: outer loop offset OUTWARD by kerf/2")) {
    return false;
  }
  // The original corners (0,0) must not be on the cut path.
  for (const auto& move : toolpath.moves) {
    if (near(move.x, 0.0, 0.02) && near(move.y, 0.0, 0.02) && move.laser_on) {
      std::cerr << "  original corner on path at " << move.x << ", " << move.y
                << "\n";
      return false;
    }
  }

  // Kerf direction — hole loop inward: the closest approach to the
  // circle center (10,5) is the offset hole radius 2 - 0.1 = 1.9.
  double minDistance = 1e9;
  for (const auto& move : toolpath.moves) {
    minDistance = std::min(minDistance, dist(move.x, move.y, 10.0, 5.0));
  }
  if (!expect(near(minDistance, 1.9, 0.05),
              "rect+hole: hole loop offset INWARD by kerf/2")) {
    std::cerr << "  min distance: " << minDistance << "\n";
    return false;
  }

  // Ordering: the hole is cut before the outer loop.
  size_t holeIndex = toolpath.moves.size();
  size_t outerIndex = toolpath.moves.size();
  for (size_t i = 0; i < toolpath.moves.size(); ++i) {
    const auto& move = toolpath.moves[i];
    if (dist(move.x, move.y, 10.0, 5.0) < 2.1 && holeIndex == toolpath.moves.size()) {
      holeIndex = i;
    }
    if ((move.x < -0.05 || move.y < -0.05) &&
        outerIndex == toolpath.moves.size()) {
      outerIndex = i;
    }
  }
  if (!expect(holeIndex < outerIndex,
              "rect+hole: hole cut before the outer loop")) {
    return false;
  }

  // Corner joins are arcs with radius kerf/2.
  bool sawJoinArc = false;
  for (const auto& move : toolpath.moves) {
    if ((move.kind == ToolpathMoveKind::FeedArcCW ||
         move.kind == ToolpathMoveKind::FeedArcCCW) &&
        near(std::hypot(move.i, move.j), 0.1, 0.02)) {
      sawJoinArc = true;
    }
  }
  return expect(sawJoinArc,
                "rect+hole: corner join arcs with radius kerf/2");
}

// ── Test 3: circle profile emits an exact concentric arc ─────────

bool test_circle_profile_exact_arc() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_circle(10.0, 5.0, 5.0);

  std::string circleProfile;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    circleProfile = feature.sketch_parameters->profiles[0].id;
  }

  LaserCutParameters laser;
  laser.kerf_width_mm = 0.2;
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 0.0;
  const std::string opId = make_laser_op(
      manager, document, sketch_feature_id(document), circleProfile, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "circle: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  bool sawArc = false;
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::FeedArcCW ||
        move.kind == ToolpathMoveKind::FeedArcCCW) {
      sawArc = true;
      // Concentric offset: radius 5 + kerf/2 = 5.1; I/J carry the
      // center offset from the arc start.
      if (!expect(near(std::hypot(move.i, move.j), 5.1, 0.02),
                  "circle: arc I/J equal the offset radius")) {
        return false;
      }
    }
  }
  return expect(sawArc, "circle: contour emitted as an exact arc");
}

// ── Test 4: engrave — no kerf, no lead-in/out ────────────────────

bool test_engrave_no_offset() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_circle(10.0, 5.0, 5.0);

  std::string circleProfile;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    circleProfile = feature.sketch_parameters->profiles[0].id;
  }

  LaserCutParameters laser;
  laser.kerf_width_mm = 0.2;  // must be ignored for engrave
  laser.mode = "engrave";
  laser.power_percent = 30.0;
  laser.lead_in_mm = 2.0;   // must be ignored for engrave
  laser.lead_out_mm = 2.0;
  const std::string opId = make_laser_op(
      manager, document, sketch_feature_id(document), circleProfile, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "engrave: generation succeeds")) {
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  for (const auto& move : toolpath.moves) {
    if (move.laser_on) {
      // Exact contour — no kerf compensation.
      if (!near(dist(move.x, move.y, 10.0, 5.0), 5.0, 0.05)) {
        std::cerr << "  engrave move at distance "
                  << dist(move.x, move.y, 10.0, 5.0) << "\n";
        return false;
      }
      if (move.dwell_seconds > 0.0) {
        std::cerr << "  engrave must not pierce\n";
        return false;
      }
    }
  }
  return true;
}

// ── Test 5: feature narrower than the kerf is a hard error ───────

bool test_too_thin_for_kerf() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 2.0);  // 2 mm tall

  std::string profile;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    profile = feature.sketch_parameters->profiles[0].id;
  }

  LaserCutParameters laser;
  laser.kerf_width_mm = 6.0;  // d = 3 > half height (1): collapses
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 0.0;
  const std::string opId =
      make_laser_op(manager, document, sketch_feature_id(document), profile, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && !outcome.result.ok,
              "too thin: generation fails loudly")) {
    return false;
  }
  return expect(outcome.result.error_message.find("kerf") != std::string::npos,
                "too thin: error names the kerf");
}

// ── Face milling tests ────────────────────────────────────────────

// Creates a mill setup + tool + face-milling op referencing the top
// face of a box via the face witness machinery.  Returns the op id.
std::string make_face_milling_op(DocumentManager& manager,
                                 DocumentState& document) {
  CamSetup setup;
  setup.name = "Mill setup";
  setup.machine_type = "3_axis_mill";
  setup.retract_height = 5.0;
  document = manager.cam_setup_create(setup);

  ToolEntry tool;
  tool.name = "6mm endmill";
  tool.type = "endmill_flat";
  tool.diameter_mm = 6.0;
  tool.default_feedrate_mm_per_min = 500.0;
  tool.default_plunge_feedrate_mm_per_min = 200.0;
  tool.default_stepover_percent = 50.0;
  document = manager.cam_tool_add(tool);

  const auto compiled = polysmith::core::compile_bodies(document);
  const auto& body = compiled.bodies[0];
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
        // Orientation-correct: both box caps parameterize +Z — only
        // the face orientation picks the UPWARD side.
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
  if (topIndex < 0) {
    throw std::runtime_error("top face not found");
  }

  const auto ref = polysmith::core::capture_face_reference(
      body.id, body.shape, topIndex, "top");
  polysmith::core::FaceAttestation att;
  att.area = ref->capturedArea;
  att.normal = ref->capturedNormal;
  for (const auto& p : ref->samplePoints) {
    att.sample_points.push_back(p);
  }
  polysmith::core::GeometryReference stored;
  stored.persistent_id = body.id + ":face:" + std::to_string(topIndex);
  stored.attestation = att;

  CamOperation op;
  op.name = "Face 1";
  op.type = "face_milling";
  op.tool_id = document.cam.tool_library[0].tool_id;
  op.parameters.feedrate_mm_per_min = 500.0;
  op.parameters.plunge_feedrate_mm_per_min = 200.0;
  op.parameters.stepover_percent = 50.0;
  op.geometry_references.machining_regions.push_back(stored);
  document = manager.cam_operation_add(op);
  return document.cam.operations.back().op_id;
}

bool test_face_milling_box_top() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document =
      manager.add_box_feature({.width = 20.0, .height = 20.0, .depth = 10.0});
  const std::string opId = make_face_milling_op(manager, document);

  // The box's top face height drives the expected feed Z (the box
  // builds downward from the XY plane — don't hardcode it).
  double faceZ = 0.0;
  {
    const auto compiled = polysmith::core::compile_bodies(document);
    for (const auto& body : compiled.bodies) {
      NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faceMap;
      TopExp::MapShapes(body.shape, TopAbs_FACE, faceMap);
      for (int i = 1; i <= faceMap.Extent(); ++i) {
        const auto face = TopoDS::Face(faceMap(i));
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
          // Orientation-corrected: pick the UPWARD face (both caps
          // parameterize +Z).
          if (face.Orientation() == TopAbs_REVERSED) {
            normal.Reverse();
          }
          if (normal.Z() > 0.99) {
            faceZ = center.Z();
          }
        }
      }
    }
  }

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "face milling: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  // Rows stay inside the inset boundary: tool radius 3 → [3, 17].
  double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
  int rapids = 0;
  int feeds = 0;
  bool sawRetractZ = false;
  bool sawFaceZ = false;
  bool rowsAbove = false;
  bool rowsBelow = false;
  for (const auto& move : toolpath.moves) {
    minX = std::min(minX, move.x);
    maxX = std::max(maxX, move.x);
    minY = std::min(minY, move.y);
    maxY = std::max(maxY, move.y);
    if (move.kind == ToolpathMoveKind::Rapid) {
      ++rapids;
      if (near(move.z, 5.0, 0.001)) {
        sawRetractZ = true;
      }
    } else {
      ++feeds;
      if (near(move.z, faceZ, 0.001)) {
        sawFaceZ = true;
      }
    }
    if (move.y > 10.0) {
      rowsAbove = true;
    }
    if (move.y < 10.0) {
      rowsBelow = true;
    }
  }
  if (!expect(minX >= 3.0 - 0.05 && maxX <= 17.0 + 0.05 &&
                  minY >= 3.0 - 0.05 && maxY <= 17.0 + 0.05,
              "face milling: rows stay inside the tool-radius inset")) {
    std::cerr << "  bounds: x[" << minX << ", " << maxX << "] y[" << minY
              << ", " << maxY << "]\n";
    return false;
  }
  if (!expect(sawRetractZ && sawFaceZ,
              "face milling: rapids at retract height, feeds at face height")) {
    std::cerr << "  z values:";
    for (const auto& move : toolpath.moves) {
      std::cerr << " " << move.z << (move.kind == ToolpathMoveKind::Rapid ? "R" : "F");
    }
    std::cerr << "  expected faceZ=" << faceZ << "\n";
    return false;
  }
  if (!expect(rapids > 0 && feeds > 0 && feeds >= rapids - 1,
              "face milling: both rapid and feed moves present")) {
    std::cerr << "  rapids=" << rapids << " feeds=" << feeds << "\n";
    return false;
  }
  // Both epsilon signs of the row offset: rows exist on both sides of
  // the face center line.
  return expect(rowsAbove && rowsBelow,
                "face milling: rows on both sides of the center");
}

// ── Test 8: laser cut from a 3D face outline ─────────────────────

bool test_laser_from_face_outline() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document =
      manager.add_box_feature({.width = 20.0, .height = 20.0, .depth = 10.0});

  CamSetup setup;
  setup.name = "Laser setup";
  setup.machine_type = "laser";
  document = manager.cam_setup_create(setup);

  ToolEntry tool;
  tool.name = "CO2 laser";
  tool.type = "laser";
  document = manager.cam_tool_add(tool);

  // Capture the upward face (orientation-corrected normal).
  const auto compiled = polysmith::core::compile_bodies(document);
  const auto& body = compiled.bodies[0];
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
  if (!expect(topIndex >= 0, "laser face: upward face found")) {
    return false;
  }

  const auto ref = polysmith::core::capture_face_reference(
      body.id, body.shape, topIndex, "top");
  polysmith::core::FaceAttestation att;
  att.area = ref->capturedArea;
  att.normal = ref->capturedNormal;
  for (const auto& p : ref->samplePoints) {
    att.sample_points.push_back(p);
  }
  polysmith::core::GeometryReference stored;
  stored.persistent_id = body.id + ":face:" + std::to_string(topIndex);
  stored.attestation = att;

  CamOperation op;
  op.name = "2D Cut (face)";
  op.type = "laser_cut";
  op.tool_id = document.cam.tool_library[0].tool_id;
  LaserCutParameters laser;
  laser.kerf_width_mm = 0.2;
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 0.0;
  op.parameters.laser = laser;
  op.geometry_references.machining_regions.push_back(stored);
  document = manager.cam_operation_add(op);
  const std::string opId = document.cam.operations.back().op_id;

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "laser face: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  // The outline cuts OUTWARD of the part edge by kerf/2 (0.1): a move
  // exists at x ≈ -0.1 and all feeds sit at the face height.
  bool sawMinX = false;
  double minX = 1e9;
  double maxX = -1e9;
  double feedZ = 0.0;
  bool haveFeedZ = false;
  for (const auto& move : toolpath.moves) {
    minX = std::min(minX, move.x);
    maxX = std::max(maxX, move.x);
    if (near(move.x, -0.1, 0.05)) {
      sawMinX = true;
    }
    if (move.kind != ToolpathMoveKind::Rapid) {
      if (!haveFeedZ) {
        feedZ = move.z;
        haveFeedZ = true;
      } else if (!near(move.z, feedZ, 0.001)) {
        std::cerr << "  feeds at mixed z: " << feedZ << " vs " << move.z
                  << "\n";
        return false;
      }
    }
  }
  if (!expect(sawMinX && minX < -0.05 && maxX > 20.05,
              "laser face: outline offset outward by kerf/2")) {
    std::cerr << "  x range: [" << minX << ", " << maxX << "]\n";
    return false;
  }
  if (!expect(!toolpath.moves.empty() && haveFeedZ,
              "laser face: cutting moves at the face height")) {
    return false;
  }
  return true;
}

bool test_face_milling_face_smaller_than_tool() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document =
      manager.add_box_feature({.width = 5.0, .height = 5.0, .depth = 10.0});
  const std::string opId = make_face_milling_op(manager, document);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!outcome.result.ok) {
    std::cerr << "  small face rejected: " << outcome.result.error_message
              << "\n";
  } else {
    std::cerr << "  small face GENERATED with "
              << outcome.result.toolpath.moves.size() << " moves\n";
  }
  if (!expect(outcome.found && !outcome.result.ok,
              "small face: generation fails loudly")) {
    return false;
  }
  return expect(
      outcome.result.error_message.find("smaller than the tool") !=
          std::string::npos,
      "small face: error names the tool-size mismatch");
}

}  // namespace

int main() {
  polysmith::core::register_builtin_cam_generators();
  bool allPassed = true;

  std::cout << "cam_generators_test\n";
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
  run("Test 1: registry lookups", test_registry);
  run("Test 2: rectangle + hole kerf/ordering", test_rectangle_with_hole);
  run("Test 3: circle profile exact arc", test_circle_profile_exact_arc);
  run("Test 4: engrave no offset", test_engrave_no_offset);
  run("Test 5: too thin for kerf", test_too_thin_for_kerf);
  run("Test 6: face milling box top", test_face_milling_box_top);
  run("Test 7: face smaller than tool",
      test_face_milling_face_smaller_than_tool);
  run("Test 8: laser from face outline", test_laser_from_face_outline);

  if (allPassed) {
    std::cout << "cam_generators_test passed\n";
    return 0;
  }
  return 1;
}
#if 0
  std::cout << "  Test 1: registry lookups... ";
  if (test_registry()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 2: rectangle + hole kerf/ordering... ";
  if (test_rectangle_with_hole()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 3: circle profile exact arc... ";
  if (test_circle_profile_exact_arc()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 4: engrave no offset... ";
  if (test_engrave_no_offset()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 5: too thin for kerf... ";
  if (test_too_thin_for_kerf()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  if (allPassed) {
    std::cout << "cam_generators_test passed\n";
    return 0;
  }
  return 1;
}
#endif
