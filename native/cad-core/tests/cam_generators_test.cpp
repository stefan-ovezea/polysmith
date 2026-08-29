// CAM generators test — the laser cutting generator end-to-end.
//
// Sketch profile → kerf offset → lead-in/out + pierce → ordering →
// toolpath IR.  Covers both kerf directions (outer loop outward, hole
// loop inward), hole-before-outer ordering, laser on/off transitions,
// exact circle arcs with I/J, engrave (no kerf/lead), the
// narrower-than-kerf error path, world-mapped arcs on rotated sketch
// planes, mirrored-frame sweep flips, non-horizontal plane rejection,
// per-loop degradation instead of hard failure, and the v2 parameter
// model (passes, speed_mm_per_s, kerf_side, mode validation,
// thickness warnings, score mode).

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "core/cam/cam_export.h"
#include "core/cam/cam_generate.h"
#include "core/cam/cam_generator.h"
#include "core/cam/cam_operation.h"
#include "core/cam/cam_profile_reference.h"
#include "core/document/document.h"
#include "core/sketch/sketch_feature_parameters.h"
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
using polysmith::core::SketchFeatureParameters;
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

// Creates a laser setup + tool + op capturing MULTIPLE named sketch
// profiles as witness references.  Returns the op id.
std::string make_multi_laser_op(
    DocumentManager& manager, DocumentState& document,
    const std::string& sketch_feature_id,
    const std::vector<std::string>& profile_ids,
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

  for (const auto& profile_id : profile_ids) {
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

// ── Test 9: rotated sketch plane — arcs map to world I/J ──────────

bool test_rotated_plane_arcs() {
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

  // Rotate the sketch frame 45° about Z on a copy of the document —
  // the generator reads the frame off the document it is given.
  DocumentState rotated = manager.get_document().value();
  const double c = std::cos(3.14159265358979323846 / 4.0);
  const double s = std::sin(3.14159265358979323846 / 4.0);
  for (auto& feature : rotated.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    feature.sketch_parameters->plane_frame =
        SketchFeatureParameters::SketchPlaneFrame{
            .origin_x = 0.0, .origin_y = 0.0, .origin_z = 0.0,
            .x_axis_x = c, .x_axis_y = s, .x_axis_z = 0.0,
            .y_axis_x = -s, .y_axis_y = c, .y_axis_z = 0.0,
            .normal_x = 0.0, .normal_y = 0.0, .normal_z = 1.0,
        };
  }

  const auto outcome = polysmith::core::generate_operation_toolpath(
      rotated, opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "rotated plane: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  // World I/J: center offset rotated into machine XY.  Sketch-local
  // center−start is (−5.1, 0); the 45° frame maps it to
  // (−5.1·c, −5.1·s), i.e. atan2(j, i) = −135°.
  bool sawRotatedArc = false;
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::FeedArcCW ||
        move.kind == ToolpathMoveKind::FeedArcCCW) {
      if (near(std::hypot(move.i, move.j), 5.1, 0.02) &&
          near(std::atan2(move.j, move.i), -3.0 * 3.14159265358979323846 / 4.0,
               0.03)) {
        sawRotatedArc = true;
      }
    }
  }
  if (!expect(sawRotatedArc, "rotated plane: arc I/J mapped to world")) {
    for (const auto& move : toolpath.moves) {
      if (move.kind == ToolpathMoveKind::FeedArcCW ||
          move.kind == ToolpathMoveKind::FeedArcCCW) {
        std::cerr << "  arc i=" << move.i << " j=" << move.j << "\n";
      }
    }
    return false;
  }

  // A laser-on move lands at the world image of the sketch point
  // (15.1, 5): (15.1c − 5s, 15.1s + 5c).
  const double wx = 15.1 * c - 5.0 * s;
  const double wy = 15.1 * s + 5.0 * c;
  for (const auto& move : toolpath.moves) {
    if (move.laser_on && near(move.x, wx, 0.05) && near(move.y, wy, 0.05)) {
      return true;
    }
  }
  std::cerr << "  no laser-on move near world (" << wx << ", " << wy << ")\n";
  return false;
}

// ── Test 10: mirrored frame flips the arc sweep ───────────────────

bool test_mirrored_frame_flips_sweep() {
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

  // Mirror the sketch across the X axis: x=(1,0,0), y=(0,−1,0),
  // normal=(0,0,1).  The frame pair is left-handed, so the CCW sketch
  // circle must emit as a CW world arc.
  DocumentState mirrored = manager.get_document().value();
  for (auto& feature : mirrored.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    feature.sketch_parameters->plane_frame =
        SketchFeatureParameters::SketchPlaneFrame{
            .origin_x = 0.0, .origin_y = 0.0, .origin_z = 0.0,
            .x_axis_x = 1.0, .x_axis_y = 0.0, .x_axis_z = 0.0,
            .y_axis_x = 0.0, .y_axis_y = -1.0, .y_axis_z = 0.0,
            .normal_x = 0.0, .normal_y = 0.0, .normal_z = 1.0,
        };
  }

  const auto outcome = polysmith::core::generate_operation_toolpath(
      mirrored, opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "mirrored plane: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  bool sawFlippedArc = false;
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::FeedArcCW &&
        near(std::hypot(move.i, move.j), 5.1, 0.02)) {
      sawFlippedArc = true;
    }
  }
  if (!expect(sawFlippedArc,
              "mirrored plane: CCW sketch emits as CW world arc")) {
    for (const auto& move : toolpath.moves) {
      if (move.kind == ToolpathMoveKind::FeedArcCW ||
          move.kind == ToolpathMoveKind::FeedArcCCW) {
        std::cerr << "  arc kind=" << static_cast<int>(move.kind)
                  << " i=" << move.i << " j=" << move.j << "\n";
      }
    }
    return false;
  }

  // The world image of the arc start (15.1, 5) is (15.1, −5).
  for (const auto& move : toolpath.moves) {
    if (move.laser_on && near(move.x, 15.1, 0.05) && near(move.y, -5.0, 0.05)) {
      return true;
    }
  }
  std::cerr << "  no laser-on move near world (15.1, -5)\n";
  return false;
}

// ── Test 11: non-horizontal sketch planes are rejected ────────────

bool test_non_horizontal_plane_rejected() {
  for (const char* planeId : {"ref-plane-yz", "ref-plane-xz"}) {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane(planeId);
    DocumentState document = manager.add_sketch_circle(10.0, 5.0, 5.0);

    std::string circleProfile;
    for (const auto& feature : document.feature_history) {
      if (feature.kind != "sketch") {
        continue;
      }
      circleProfile = feature.sketch_parameters->profiles[0].id;
    }

    LaserCutParameters laser;
    laser.lead_in_mm = 0.0;
    laser.lead_out_mm = 0.0;
    const std::string opId = make_laser_op(
        manager, document, sketch_feature_id(document), circleProfile, laser);

    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    const std::string label = std::string(planeId) + ": rejected loudly";
    if (!expect(outcome.found && !outcome.result.ok, label.c_str())) {
      std::cerr << "  " << planeId << " generated "
                << outcome.result.toolpath.moves.size() << " moves\n";
      return false;
    }
    const std::string planes = std::string(planeId) + ": error is actionable";
    if (!expect(outcome.result.error_message.find("horizontal") !=
                    std::string::npos,
                planes.c_str())) {
      std::cerr << "  error: " << outcome.result.error_message << "\n";
      return false;
    }
  }
  return true;
}

// ── Test 12: hairline slot degrades instead of failing the op ─────

bool test_hairline_slot_degrades() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);
  // A hairline slot (8 × 0.5) in the middle — far narrower than 2·kerf.
  document = manager.add_sketch_rectangle(6.0, 4.75, 14.0, 5.25);

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
  if (!expect(!outerProfile.empty(), "hairline slot: outer region found")) {
    return false;
  }

  LaserCutParameters laser;
  laser.kerf_width_mm = 6.0;  // d = 3: the 0.5-wide slot collapses
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 0.0;
  const std::string opId = make_laser_op(
      manager, document, sketch_feature_id(document), outerProfile, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "hairline slot: operation succeeds with the slot skipped")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  bool sawKerfWarning = false;
  for (const auto& warning : outcome.result.warnings) {
    if (warning.find("kerf") != std::string::npos) {
      sawKerfWarning = true;
    }
  }
  if (!expect(sawKerfWarning, "hairline slot: warning names the kerf")) {
    return false;
  }

  // The outer rectangle still cuts (offset outward by 3 → x ≈ −3),
  // and nothing cuts inside the collapsed slot.
  bool sawOuter = false;
  for (const auto& move : toolpath.moves) {
    if (move.laser_on && near(move.x, -3.0, 0.05)) {
      sawOuter = true;
    }
    if (move.laser_on && move.x > 6.0 && move.x < 14.0 && move.y > 4.75 &&
        move.y < 5.25) {
      std::cerr << "  laser-on move inside the skipped slot at " << move.x
                << ", " << move.y << "\n";
      return false;
    }
  }
  return expect(sawOuter, "hairline slot: outer contour still cut");
}

// ── Test 13: passes repeat the contour, laser stays on ───────────

bool test_laser_passes() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);

  std::string profile;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    profile = feature.sketch_parameters->profiles[0].id;
  }

  LaserCutParameters laser;
  laser.kerf_width_mm = 0.2;
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 0.0;
  laser.passes = 3;
  const std::string opId = make_laser_op(
      manager, document, sketch_feature_id(document), profile, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "passes: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  // Rapid + pierce + 3 × (4 edges + 4 join arcs).
  if (!expect(toolpath.moves.size() == 2 + 3 * 8,
              "passes: contour block emitted 3 times")) {
    std::cerr << "  moves=" << toolpath.moves.size() << "\n";
    return false;
  }
  // The laser stays ON across every pass — no re-pierce, no M5.
  for (size_t i = 2; i < toolpath.moves.size(); ++i) {
    if (!toolpath.moves[i].laser_on) {
      std::cerr << "  laser off at move " << i << "\n";
      return expect(false, "passes: laser stays on across passes");
    }
  }
  return true;
}

// ── Test 14: speed_mm_per_s drives the feedrate; legacy fallback ─

bool test_laser_speed() {
  // speed 10 mm/s → F600.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    DocumentState document = manager.add_sketch_circle(10.0, 5.0, 5.0);

    std::string profile;
    for (const auto& feature : document.feature_history) {
      if (feature.kind != "sketch") {
        continue;
      }
      profile = feature.sketch_parameters->profiles[0].id;
    }
    LaserCutParameters laser;
    laser.lead_in_mm = 0.0;
    laser.lead_out_mm = 0.0;
    laser.speed_mm_per_s = 10.0;
    const std::string opId = make_laser_op(
        manager, document, sketch_feature_id(document), profile, laser);
    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && outcome.result.ok,
                "speed: generation succeeds")) {
      return false;
    }
    for (const auto& move : outcome.result.toolpath.moves) {
      if (move.kind != ToolpathMoveKind::Rapid &&
          !near(move.feedrate_mm_per_min, 600.0, 0.001)) {
        std::cerr << "  feedrate " << move.feedrate_mm_per_min << "\n";
        return expect(false, "speed: 10 mm/s emits F600");
      }
    }
  }
  // No speed field → legacy feedrate_mm_per_min (F500 golden path).
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    DocumentState document = manager.add_sketch_circle(10.0, 5.0, 5.0);

    std::string profile;
    for (const auto& feature : document.feature_history) {
      if (feature.kind != "sketch") {
        continue;
      }
      profile = feature.sketch_parameters->profiles[0].id;
    }
    LaserCutParameters laser;
    laser.lead_in_mm = 0.0;
    laser.lead_out_mm = 0.0;
    const std::string opId = make_laser_op(
        manager, document, sketch_feature_id(document), profile, laser);
    // The op default feedrate is 1200 — push it to the legacy golden.
    DocumentState patched = manager.get_document().value();
    for (auto& op2 : patched.cam.operations) {
      if (op2.op_id == opId) {
        op2.parameters.feedrate_mm_per_min = 500.0;
      }
    }
    const auto outcome = polysmith::core::generate_operation_toolpath(
        patched, opId, /*preview=*/false);
    if (!expect(outcome.found && outcome.result.ok,
                "speed: legacy generation succeeds")) {
      return false;
    }
    for (const auto& move : outcome.result.toolpath.moves) {
      if (move.kind != ToolpathMoveKind::Rapid &&
          !near(move.feedrate_mm_per_min, 500.0, 0.001)) {
        std::cerr << "  feedrate " << move.feedrate_mm_per_min << "\n";
        return expect(false, "speed: legacy feedrate F500 kept");
      }
    }
  }
  return true;
}

// ── Test 15: kerf_side inside/outside override the auto side ─────

bool test_kerf_side_override() {
  // "inside": the outer cut moves INSIDE the contour (x ≈ +0.1).
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    DocumentState document =
        manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);

    std::string profile;
    for (const auto& feature : document.feature_history) {
      if (feature.kind != "sketch") {
        continue;
      }
      profile = feature.sketch_parameters->profiles[0].id;
    }
    LaserCutParameters laser;
    laser.kerf_width_mm = 0.2;
    laser.lead_in_mm = 0.0;
    laser.lead_out_mm = 0.0;
    laser.kerf_side = "inside";
    const std::string opId = make_laser_op(
        manager, document, sketch_feature_id(document), profile, laser);
    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && outcome.result.ok,
                "kerf side inside: generation succeeds")) {
      return false;
    }
    bool sawInside = false;
    bool sawOutside = false;
    for (const auto& move : outcome.result.toolpath.moves) {
      if (move.laser_on && near(move.x, 0.1, 0.02)) {
        sawInside = true;
      }
      if (move.laser_on && near(move.x, -0.1, 0.02)) {
        sawOutside = true;
      }
    }
    if (!expect(sawInside && !sawOutside,
                "kerf side inside: cut offset INWARD")) {
      return false;
    }
  }
  // "outside": the outer cut moves OUTSIDE the contour (x ≈ -0.1).
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    DocumentState document =
        manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);

    std::string profile;
    for (const auto& feature : document.feature_history) {
      if (feature.kind != "sketch") {
        continue;
      }
      profile = feature.sketch_parameters->profiles[0].id;
    }
    LaserCutParameters laser;
    laser.kerf_width_mm = 0.2;
    laser.lead_in_mm = 0.0;
    laser.lead_out_mm = 0.0;
    laser.kerf_side = "outside";
    const std::string opId = make_laser_op(
        manager, document, sketch_feature_id(document), profile, laser);
    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && outcome.result.ok,
                "kerf side outside: generation succeeds")) {
      return false;
    }
    bool sawInside = false;
    bool sawOutside = false;
    for (const auto& move : outcome.result.toolpath.moves) {
      if (move.laser_on && near(move.x, 0.1, 0.02)) {
        sawInside = true;
      }
      if (move.laser_on && near(move.x, -0.1, 0.02)) {
        sawOutside = true;
      }
    }
    return expect(sawOutside && !sawInside,
                  "kerf side outside: cut offset OUTWARD");
  }
}

// ── Test 16: mode validation + thickness/power warning ───────────

bool test_laser_mode_validation() {
  // Unknown mode → hard error.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    DocumentState document = manager.add_sketch_circle(10.0, 5.0, 5.0);

    std::string profile;
    for (const auto& feature : document.feature_history) {
      if (feature.kind != "sketch") {
        continue;
      }
      profile = feature.sketch_parameters->profiles[0].id;
    }
    LaserCutParameters laser;
    laser.mode = "waterjet";
    const std::string opId = make_laser_op(
        manager, document, sketch_feature_id(document), profile, laser);
    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && !outcome.result.ok,
                "mode: unknown mode fails loudly")) {
      return false;
    }
    if (!expect(outcome.result.error_message.find("Unknown laser mode") !=
                    std::string::npos,
                "mode: error names the bad mode")) {
      return false;
    }
  }
  // Thick material at low power → penetration warning.
  {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");
    DocumentState document =
        manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);

    std::string profile;
    for (const auto& feature : document.feature_history) {
      if (feature.kind != "sketch") {
        continue;
      }
      profile = feature.sketch_parameters->profiles[0].id;
    }
    LaserCutParameters laser;
    laser.material_thickness_mm = 8.0;
    laser.power_percent = 40.0;
    laser.lead_in_mm = 0.0;
    laser.lead_out_mm = 0.0;
    const std::string opId = make_laser_op(
        manager, document, sketch_feature_id(document), profile, laser);
    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && outcome.result.ok,
                "thickness: generation still succeeds")) {
      return false;
    }
    bool sawWarning = false;
    for (const auto& warning : outcome.result.warnings) {
      if (warning.find("penetrate") != std::string::npos) {
        sawWarning = true;
      }
    }
    return expect(sawWarning, "thickness: penetration warning present");
  }
}

// ── Test 17: score mode = cut geometry at the user's power ───────

bool test_score_mode() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_circle(10.0, 5.0, 5.0);

  std::string profile;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    profile = feature.sketch_parameters->profiles[0].id;
  }

  LaserCutParameters laser;
  laser.mode = "score";
  laser.kerf_width_mm = 0.2;
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 0.0;
  laser.power_percent = 12.0;
  const std::string opId = make_laser_op(
      manager, document, sketch_feature_id(document), profile, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "score: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;
  bool sawArc = false;
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::FeedArcCW ||
        move.kind == ToolpathMoveKind::FeedArcCCW) {
      sawArc = true;
      // Kerf still applies to score (same geometry as cut), and every
      // laser-on move carries the user's power.
      if (!near(std::hypot(move.i, move.j), 5.1, 0.02)) {
        return expect(false, "score: kerf offset applies like cut");
      }
    }
    if (move.laser_on && !near(move.power_percent, 12.0, 0.001)) {
      std::cerr << "  power " << move.power_percent << "\n";
      return expect(false, "score: user power on every cut move");
    }
  }
  return expect(sawArc, "score: contour emitted as exact arc");
}

// ── Test 18: inner_first — holes, then their outer, then the next
// region (group contiguity, no freed-part drift) ─────────────────

bool test_inner_first_ordering() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);
  document = manager.add_sketch_circle(5.0, 5.0, 1.0);
  document = manager.add_sketch_circle(15.0, 5.0, 1.0);
  // A separate small part far away.
  document = manager.add_sketch_rectangle(30.0, 30.0, 33.0, 33.0);

  std::string outerProfile;
  std::string smallProfile;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    for (const auto& region : feature.sketch_parameters->profiles) {
      if (!region.inner_loops.empty()) {
        outerProfile = region.id;
      }
      if (region.inner_loops.empty() && !region.points.empty() &&
          region.points[0].x > 25.0) {
        smallProfile = region.id;
      }
    }
  }
  if (!expect(!outerProfile.empty() && !smallProfile.empty(),
              "inner first: regions found")) {
    return false;
  }

  LaserCutParameters laser;
  laser.kerf_width_mm = 0.2;
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 0.0;
  const std::string opId = make_multi_laser_op(
      manager, document, sketch_feature_id(document),
      {outerProfile, smallProfile}, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "inner first: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }

  // Classify every laser-on move into a loop; record the sequence of
  // loop starts.
  enum LoopClass { kHoleA, kHoleB, kOuterA, kOuterB };
  const auto classify = [](const auto& move) {
    if (dist(move.x, move.y, 5.0, 5.0) < 1.2) {
      return kHoleA;
    }
    if (dist(move.x, move.y, 15.0, 5.0) < 1.2) {
      return kHoleB;
    }
    if (move.x > 29.0 && move.y > 29.0) {
      return kOuterB;
    }
    return kOuterA;
  };
  std::vector<int> sequence;
  for (const auto& move : outcome.result.toolpath.moves) {
    if (!move.laser_on) {
      continue;
    }
    const int c = classify(move);
    if (sequence.empty() || sequence.back() != c) {
      sequence.push_back(c);
    }
  }
  // Holes first (either order), then their own outer, then the other
  // region — no freed-part drift.
  if (!expect(sequence.size() == 4,
              "inner first: four loops cut")) {
    std::cerr << "  sequence size " << sequence.size() << "\n";
    return false;
  }
  const bool holesFirst =
      (sequence[0] == kHoleA && sequence[1] == kHoleB) ||
      (sequence[0] == kHoleB && sequence[1] == kHoleA);
  return expect(holesFirst && sequence[2] == kOuterA &&
                    sequence[3] == kOuterB,
                "inner first: holes, their outer, then the next region");
}

// ── Test 19: nearest_neighbor beats by_area on rapid travel ──────

bool test_nearest_neighbor_travel() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);
  document = manager.add_sketch_rectangle(100.0, 0.0, 110.0, 10.0);
  document = manager.add_sketch_rectangle(50.0, 50.0, 60.0, 60.0);

  std::vector<std::string> profiles;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    for (const auto& region : feature.sketch_parameters->profiles) {
      if (region.inner_loops.empty()) {
        profiles.push_back(region.id);
      }
    }
  }
  if (!expect(profiles.size() == 3, "nn: three regions found")) {
    return false;
  }

  LaserCutParameters laser;
  laser.kerf_width_mm = 0.2;
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 0.0;
  const std::string opId = make_multi_laser_op(
      manager, document, sketch_feature_id(document), profiles, laser);

  // Total rapid travel length for a given cut_order.
  const auto rapidTravel = [&](const std::string& cutOrder) {
    DocumentState patched = manager.get_document().value();
    for (auto& op : patched.cam.operations) {
      if (op.op_id == opId) {
        op.parameters.laser->cut_order = cutOrder;
      }
    }
    const auto outcome = polysmith::core::generate_operation_toolpath(
        patched, opId, /*preview=*/false);
    if (!outcome.result.ok) {
      return 1e18;
    }
    double travel = 0.0;
    double px = 0.0;
    double py = 0.0;
    for (const auto& move : outcome.result.toolpath.moves) {
      if (move.kind == ToolpathMoveKind::Rapid) {
        travel += dist(move.x, move.y, px, py);
      }
      px = move.x;
      py = move.y;
    }
    return travel;
  };

  const double nnTravel = rapidTravel("nearest_neighbor");
  const double areaTravel = rapidTravel("by_area");
  return expect(nnTravel < areaTravel - 10.0,
                "nn: strictly less rapid travel than by_area");
}

// ── Test 20: nested separate regions — depth order + duplicate
// warning ─────────────────────────────────────────────────────────

bool test_nested_region_ordering() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_circle(10.0, 5.0, 8.0);
  document = manager.add_sketch_circle(10.0, 5.0, 2.0);

  std::string outerProfile;
  std::string innerProfile;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    for (const auto& region : feature.sketch_parameters->profiles) {
      if (!region.inner_loops.empty()) {
        outerProfile = region.id;
      } else {
        innerProfile = region.id;
      }
    }
  }
  if (!expect(!outerProfile.empty() && !innerProfile.empty(),
              "nested: regions found")) {
    return false;
  }

  LaserCutParameters laser;
  laser.kerf_width_mm = 0.2;
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 0.0;
  const std::string opId = make_multi_laser_op(
      manager, document, sketch_feature_id(document),
      {outerProfile, innerProfile}, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "nested: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }

  // Ring sequence: hole (1.9), inner region (2.1), outer (8.1) — the
  // enclosed region is released before its container.
  std::vector<double> sequence;
  for (const auto& move : outcome.result.toolpath.moves) {
    if (!move.laser_on) {
      continue;
    }
    const double d = dist(move.x, move.y, 10.0, 5.0);
    if (sequence.empty() || std::abs(d - sequence.back()) > 0.15) {
      sequence.push_back(d);
    }
  }
  if (!expect(sequence.size() == 3, "nested: three rings cut")) {
    std::cerr << "  rings=" << sequence.size() << "\n";
    return false;
  }
  if (!expect(near(sequence[0], 1.9, 0.15) && near(sequence[1], 2.1, 0.15) &&
                  near(sequence[2], 8.1, 0.15),
              "nested: hole, inner region, then the outer ring")) {
    return false;
  }

  // The selected inner region duplicates the outer region's hole —
  // warn, don't fail.
  for (const auto& warning : outcome.result.warnings) {
    if (warning.find("duplicate") != std::string::npos) {
      return true;
    }
  }
  std::cerr << "  no duplicate warning\n";
  return false;
}

// ── Test 21: arc lead-in rolls onto the contour tangentially ─────

bool test_arc_lead_in() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);

  std::string profile;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    profile = feature.sketch_parameters->profiles[0].id;
  }

  LaserCutParameters laser;
  laser.kerf_width_mm = 0.2;
  laser.lead_in_mm = 2.0;
  laser.lead_out_mm = 0.0;
  laser.lead_in_style = "arc";
  const std::string opId = make_laser_op(
      manager, document, sketch_feature_id(document), profile, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "arc lead: generation succeeds")) {
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;
  // Moves: rapid, pierce, lead-in arc, contour...
  if (!expect(toolpath.moves.size() >= 4,
              "arc lead: enough moves")) {
    return false;
  }
  const auto& lead = toolpath.moves[2];
  return expect((lead.kind == ToolpathMoveKind::FeedArcCW ||
                 lead.kind == ToolpathMoveKind::FeedArcCCW) &&
                    near(std::hypot(lead.i, lead.j), 2.0, 0.02),
                "arc lead: 90° tangent roll-in arc of radius lead_in_mm");
}

// ── Test 22: line lead at 45° to the contour tangent ─────────────

bool test_angled_lead_in() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);

  std::string profile;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    profile = feature.sketch_parameters->profiles[0].id;
  }

  LaserCutParameters laser;
  laser.kerf_width_mm = 0.2;
  laser.lead_in_mm = 2.0;
  laser.lead_out_mm = 0.0;
  laser.lead_in_angle_deg = 45.0;
  const std::string opId = make_laser_op(
      manager, document, sketch_feature_id(document), profile, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "angled lead: generation succeeds")) {
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;
  const auto& pierce = toolpath.moves[1];
  const auto& lead = toolpath.moves[2];
  const double dx = lead.x - pierce.x;
  const double dy = lead.y - pierce.y;
  // The lead enters at 45° to the tangent — for the default bottom-
  // edge pierce the tangent is +x, so the lead direction is
  // (cos45, sin45): dy/dx ≈ 1.
  if (!expect(near(std::hypot(dx, dy), 2.0, 0.02),
              "angled lead: length preserved")) {
    return false;
  }
  if (std::abs(dx) < 1e-9) {
    std::cerr << "  lead direction (" << dx << ", " << dy << ")\n";
    return false;
  }
  return expect(near(dy / dx, 1.0, 0.02),
                "angled lead: 45° to the contour tangent");
}

// ── Test 23: sharp corners are excluded from pierce placement ────

bool test_sharp_corner_pierce() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  // A thin triangle: the tip at (5, 0.5) has an interior angle of
  // ~11° — far below the pierce threshold.
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 10.0, 0.0);
  document = manager.add_sketch_line(10.0, 0.0, 5.0, 0.5);
  document = manager.add_sketch_line(5.0, 0.5, 0.0, 0.0);

  std::string profile;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    for (const auto& region : feature.sketch_parameters->profiles) {
      if (region.inner_loops.empty() && !region.points.empty()) {
        profile = region.id;
      }
    }
  }
  if (!expect(!profile.empty(), "sharp: region found")) {
    return false;
  }

  LaserCutParameters laser;
  laser.kerf_width_mm = 0.2;
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 0.0;
  const std::string opId = make_laser_op(
      manager, document, sketch_feature_id(document), profile, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "sharp: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  // The pierce (first laser-on move) must stay away from the tip.
  const auto& pierce = outcome.result.toolpath.moves[1];
  if (!expect(!near(pierce.x, 5.0, 0.5) || !near(pierce.y, 0.5, 0.5),
              "sharp: pierce avoids the acute tip")) {
    std::cerr << "  pierce at " << pierce.x << ", " << pierce.y << "\n";
    return false;
  }
  for (const auto& warning : outcome.result.warnings) {
    if (warning.find("sharp corner") != std::string::npos) {
      return true;
    }
  }
  std::cerr << "  no sharp-corner warning\n";
  return false;
}

// ── Test 24: overcut extends the cut past the pierce vertex ──────

bool test_overcut() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);

  std::string profile;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    profile = feature.sketch_parameters->profiles[0].id;
  }

  LaserCutParameters laser;
  laser.kerf_width_mm = 0.2;
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 2.0;
  laser.overcut_mm = 1.0;
  const std::string opId = make_laser_op(
      manager, document, sketch_feature_id(document), profile, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "overcut: generation succeeds")) {
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;
  const auto& pierce = toolpath.moves[1];
  const auto& last = toolpath.moves.back();
  if (!expect(last.laser_on, "overcut: last move is a cut")) {
    return false;
  }
  // The cut ends 3 mm past the pierce vertex along the exit tangent
  // (lead_out + overcut).
  return expect(near(dist(last.x, last.y, pierce.x, pierce.y), 3.0, 0.15),
                "overcut: cut runs lead_out + overcut past the pierce");
}

// ── Test 25: pierce_position lead_start anchors on the origin ────

bool test_pierce_position() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  // A square with a shallow dent in the bottom edge: the dent corner
  // is nearest the centroid, the (0,0) corner is nearest the origin.
  DocumentState document = manager.add_sketch_line(0.0, 0.0, 5.0, 0.0);
  document = manager.add_sketch_line(5.0, 0.0, 5.1, -0.01);
  document = manager.add_sketch_line(5.1, -0.01, 10.0, 0.0);
  document = manager.add_sketch_line(10.0, 0.0, 10.0, 10.0);
  document = manager.add_sketch_line(10.0, 10.0, 0.0, 10.0);
  document = manager.add_sketch_line(0.0, 10.0, 0.0, 0.0);

  std::string profile;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    for (const auto& region : feature.sketch_parameters->profiles) {
      if (region.inner_loops.empty() && !region.points.empty()) {
        profile = region.id;
      }
    }
  }
  if (!expect(!profile.empty(), "pierce position: region found")) {
    return false;
  }

  LaserCutParameters laser;
  laser.kerf_width_mm = 0.2;
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 0.0;
  const std::string opId = make_laser_op(
      manager, document, sketch_feature_id(document), profile, laser);

  const auto pierceFor = [&](const std::string& position) {
    DocumentState patched = manager.get_document().value();
    for (auto& op : patched.cam.operations) {
      if (op.op_id == opId) {
        op.parameters.laser->pierce_position = position;
      }
    }
    const auto outcome = polysmith::core::generate_operation_toolpath(
        patched, opId, /*preview=*/false);
    if (!outcome.result.ok) {
      throw std::runtime_error("generation failed");
    }
    return outcome.result.toolpath.moves[1];
  };

  const auto autoPierce = pierceFor("auto");
  const auto originPierce = pierceFor("lead_start");
  if (!expect(near(autoPierce.x, 5.0, 0.15) && near(autoPierce.y, 0.0, 0.15),
              "pierce position: auto anchors on the centroid")) {
    return false;
  }
  return expect(near(originPierce.x, 0.0, 0.15) &&
                    near(originPierce.y, 0.0, 0.15),
                "pierce position: lead_start anchors on the origin");
}

// ── Test 26: tabs split the outer contour, holes stay tab-free ───

bool test_tabs_on_outer_only() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);
  document = manager.add_sketch_circle(10.0, 5.0, 2.0);

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
  if (!expect(!outerProfile.empty(), "tabs: outer region found")) {
    return false;
  }

  LaserCutParameters laser;
  laser.kerf_width_mm = 0.2;
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 0.0;
  laser.tabs_enabled = true;
  laser.tab_spacing_mm = 20.0;
  laser.tab_width_mm = 0.5;
  const std::string opId = make_laser_op(
      manager, document, sketch_feature_id(document), outerProfile, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "tabs: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }

  // Outer perimeter ≈ 60.8 → ⌈60.8/20⌉ = 4 tabs; each tab is one
  // laser-off chord.  The hole loop has none (tabs_on_holes=false).
  int offFeeds = 0;
  int onFeeds = 0;
  for (const auto& move : outcome.result.toolpath.moves) {
    if (move.kind == ToolpathMoveKind::FeedLinear && !move.laser_on) {
      ++offFeeds;
    }
    if (move.kind == ToolpathMoveKind::FeedLinear && move.laser_on) {
      ++onFeeds;
    }
  }
  return expect(offFeeds == 4 && onFeeds > 0,
                "tabs: 4 laser-off tab spans on the outer contour only");
}

// ── Test 27: tab power > 0 keeps the laser on at low power ───────

bool test_tab_power() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);

  std::string profile;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    profile = feature.sketch_parameters->profiles[0].id;
  }

  LaserCutParameters laser;
  laser.kerf_width_mm = 0.2;
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 0.0;
  laser.tabs_enabled = true;
  laser.tab_spacing_mm = 20.0;
  laser.tab_width_mm = 0.5;
  laser.tab_power_percent = 10.0;
  const std::string opId = make_laser_op(
      manager, document, sketch_feature_id(document), profile, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "tab power: generation succeeds")) {
    return false;
  }
  // Micro-joint: the tab spans stay laser-ON at the tab power.
  int tabMoves = 0;
  for (const auto& move : outcome.result.toolpath.moves) {
    if (move.kind == ToolpathMoveKind::FeedLinear && move.laser_on &&
        near(move.power_percent, 10.0, 0.001)) {
      ++tabMoves;
    }
  }
  return expect(tabMoves == 4, "tab power: 4 low-power tab spans");
}

// ── Test 28: tiny loops are cut without tabs, with a warning ─────

bool test_tabs_tiny_loop() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_circle(10.0, 5.0, 0.2);

  std::string profile;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    profile = feature.sketch_parameters->profiles[0].id;
  }

  LaserCutParameters laser;
  laser.kerf_width_mm = 0.0;  // r 0.2: length ≈ 1.26 < 3·tab_width
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 0.0;
  laser.tabs_enabled = true;
  laser.tab_width_mm = 0.5;
  const std::string opId = make_laser_op(
      manager, document, sketch_feature_id(document), profile, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "tiny tabs: generation succeeds")) {
    return false;
  }
  bool sawWarning = false;
  for (const auto& warning : outcome.result.warnings) {
    if (warning.find("too short to carry tabs") != std::string::npos) {
      sawWarning = true;
    }
  }
  return expect(sawWarning, "tiny tabs: warning names the skipped tabs");
}

// ── Test 29: engrave fill — hatch lines, bidirectional, laser on ─

bool test_fill_hatch_rect() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);

  std::string profile;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    profile = feature.sketch_parameters->profiles[0].id;
  }

  LaserCutParameters laser;
  laser.mode = "engrave";
  laser.engrave_style = "fill";
  laser.line_spacing_mm = 1.0;
  laser.fill_angle_deg = 0.0;
  laser.fill_bidirectional = true;
  laser.power_percent = 40.0;
  const std::string opId = make_laser_op(
      manager, document, sketch_feature_id(document), profile, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "fill: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  // 10 lines at 1 mm spacing; each full span is ~10 long.  Only the
  // initial travel is laser-off (bidirectional keeps the beam on).
  int spans = 0;
  int offMoves = 0;
  double lastDx = 0.0;
  bool alternate = true;
  for (size_t i = 1; i < toolpath.moves.size(); ++i) {
    const auto& prev = toolpath.moves[i - 1];
    const auto& move = toolpath.moves[i];
    const double dx = move.x - prev.x;
    const double dy = move.y - prev.y;
    if (std::abs(dx) > 9.0) {
      ++spans;
      if (spans > 1 && (dx > 0) == (lastDx > 0)) {
        alternate = false;
      }
      lastDx = dx;
    }
  }
  // Laser-off moves anywhere (the initial travel is move 0).
  offMoves = 0;
  for (const auto& move : toolpath.moves) {
    if (!move.laser_on) {
      ++offMoves;
    }
  }
  if (!expect(spans == 10, "fill: 10 hatch lines at 1 mm spacing")) {
    std::cerr << "  spans=" << spans << "\n";
    return false;
  }
  if (!expect(alternate, "fill: bidirectional alternates direction")) {
    return false;
  }
  if (!expect(offMoves == 1,
              "fill: only the initial travel is laser-off")) {
    std::cerr << "  off moves=" << offMoves << "\n";
    return false;
  }
  for (const auto& move : toolpath.moves) {
    if (move.laser_on && !near(move.power_percent, 40.0, 0.001)) {
      return expect(false, "fill: hatch moves carry the user power");
    }
  }
  return true;
}

// ── Test 30: fill skips the hole interior ────────────────────────

bool test_fill_hole_exclusion() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);
  document = manager.add_sketch_circle(5.0, 5.0, 2.0);

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
  if (!expect(!outerProfile.empty(), "fill hole: outer region found")) {
    return false;
  }

  LaserCutParameters laser;
  laser.mode = "engrave";
  laser.engrave_style = "fill";
  laser.line_spacing_mm = 1.0;
  laser.fill_angle_deg = 0.0;
  laser.fill_bidirectional = true;
  const std::string opId = make_laser_op(
      manager, document, sketch_feature_id(document), outerProfile, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "fill hole: generation succeeds")) {
    return false;
  }

  // At y = 4.5 the circle hole splits the scan line: two laser-ON
  // spans and one laser-OFF jump across the hole.
  int onSpansAtHole = 0;
  int offJumpsAtHole = 0;
  for (size_t i = 1; i < outcome.result.toolpath.moves.size(); ++i) {
    const auto& prev = outcome.result.toolpath.moves[i - 1];
    const auto& move = outcome.result.toolpath.moves[i];
    if (!near(move.y, 4.5, 0.05)) {
      continue;
    }
    // Spans displace along the line; connectors land on the line
    // without cutting across it.
    const double dx = move.x - prev.x;
    if (!move.laser_on) {
      ++offJumpsAtHole;
    } else if (std::abs(dx) > 2.0) {
      ++onSpansAtHole;
    }
  }
  return expect(onSpansAtHole == 2 && offJumpsAtHole == 1,
                "fill hole: two spans + one laser-off jump at the hole");
}

// ── Test 31: fill angle + unidirectional ─────────────────────────

bool test_fill_angle_and_unidirectional() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 10.0, 10.0);

  std::string profile;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    profile = feature.sketch_parameters->profiles[0].id;
  }

  LaserCutParameters laser;
  laser.mode = "engrave";
  laser.engrave_style = "fill";
  laser.line_spacing_mm = 1.0;
  laser.fill_angle_deg = 90.0;
  laser.fill_bidirectional = false;
  const std::string opId = make_laser_op(
      manager, document, sketch_feature_id(document), profile, laser);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "fill angle: generation succeeds")) {
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  // Vertical spans (|dy| ≈ 10), all in the SAME direction, with a
  // laser-off travel back between lines: 1 initial travel + 9
  // returns = 10 laser-off moves.
  int spans = 0;
  int offMoves = 0;
  double lastDy = 0.0;
  bool sameDirection = true;
  for (size_t i = 1; i < toolpath.moves.size(); ++i) {
    const auto& prev = toolpath.moves[i - 1];
    const auto& move = toolpath.moves[i];
    if (!move.laser_on) {
      continue;  // travel-backs between one-way lines
    }
    const double dy = move.y - prev.y;
    if (std::abs(dy) > 9.0) {
      ++spans;
      if (spans > 1 && (dy > 0) != (lastDy > 0)) {
        sameDirection = false;
      }
      lastDy = dy;
    }
  }
  // Laser-off moves anywhere (initial travel is move 0).
  offMoves = 0;
  for (const auto& move : toolpath.moves) {
    if (!move.laser_on) {
      ++offMoves;
    }
  }
  if (!expect(spans == 10, "fill angle: 10 vertical hatch lines")) {
    return false;
  }
  if (!expect(sameDirection, "fill angle: one-way keeps one direction")) {
    return false;
  }
  return expect(offMoves == 10,
                "fill angle: travel-back between one-way lines");
}

// ── Test 32: end-to-end G-code export ────────────────────────────

bool test_export_gcode_end_to_end() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document =
      manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);
  document = manager.add_sketch_circle(30.0, 5.0, 5.0);

  std::vector<std::string> regions;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    for (const auto& region : feature.sketch_parameters->profiles) {
      if (region.inner_loops.empty()) {
        regions.push_back(region.id);
      }
    }
  }
  if (!expect(regions.size() == 2, "export: two regions found")) {
    return false;
  }

  LaserCutParameters laser;
  laser.kerf_width_mm = 0.2;
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 0.0;
  make_laser_op(manager, document, sketch_feature_id(document), regions[0],
                laser);
  make_multi_laser_op(manager, document, sketch_feature_id(document),
                      {regions[1]}, laser);

  DocumentState exportDoc = manager.get_document().value();
  exportDoc.cam.post_processor = polysmith::core::PostProcessor{};
  exportDoc.cam.post_processor->type = "grbl";
  exportDoc.cam.post_processor->filename = "cut.nc";

  const auto path = std::filesystem::temp_directory_path() /
                    "polysmith_m8_export_test.nc";
  const auto result =
      polysmith::core::export_cam_gcode(exportDoc, path.string());
  if (!expect(result.exported_feature_count == 2,
              "export: both operations exported")) {
    return false;
  }

  std::ifstream stream(path.string());
  std::stringstream buffer;
  buffer << stream.rdbuf();
  const std::string gcode = buffer.str();

  int m2Count = 0;
  size_t pos = 0;
  while ((pos = gcode.find("M2", pos)) != std::string::npos) {
    ++m2Count;
    pos += 2;
  }
  if (!expect(m2Count == 1, "export: exactly one M2 at the file end")) {
    return false;
  }
  if (!expect(gcode.find("(operation:") != std::string::npos &&
                  gcode.find("(operation:", gcode.find("(operation:") + 12) !=
                      std::string::npos,
              "export: both operation blocks present")) {
    return false;
  }
  if (!expect(gcode.find("(skipped") == std::string::npos,
              "export: nothing skipped")) {
    return false;
  }
  if (!expect(gcode.find("G0 Z") == std::string::npos,
              "export: laser program never lifts Z")) {
    return false;
  }
  return expect(gcode.find("M4") != std::string::npos,
                "export: dynamic laser power used");
}

// ── Test 33: a laser operation rejects a mill tool ───────────────

// Creates a laser setup + tool + test-pattern operation (no geometry
// references — the cells live in machine coordinates).
std::string make_test_pattern_op(
    DocumentManager& manager, const polysmith::core::LaserTestPatternParameters& pattern) {
  CamSetup setup;
  setup.name = "Laser setup";
  setup.machine_type = "laser";
  DocumentState document = manager.cam_setup_create(setup);

  ToolEntry tool;
  tool.name = "CO2 laser";
  tool.type = "laser";
  document = manager.cam_tool_add(tool);

  CamOperation op;
  op.name = "Test Pattern";
  op.type = "laser_test_pattern";
  op.tool_id = document.cam.tool_library[0].tool_id;
  op.parameters.test_pattern = pattern;
  document = manager.cam_operation_add(op);
  return document.cam.operations.back().op_id;
}

bool test_laser_requires_laser_tool() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState document = manager.add_sketch_circle(10.0, 5.0, 5.0);

  std::string profile;
  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    profile = feature.sketch_parameters->profiles[0].id;
  }
  LaserCutParameters laser;
  laser.lead_in_mm = 0.0;
  laser.lead_out_mm = 0.0;
  const std::string opId = make_laser_op(
      manager, document, sketch_feature_id(document), profile, laser);

  // Point the laser operation at an endmill.
  DocumentState patched = manager.get_document().value();
  ToolEntry mill;
  mill.tool_id = "tool-mill";
  mill.name = "6mm endmill";
  mill.type = "endmill_flat";
  mill.diameter_mm = 6.0;
  patched.cam.tool_library.push_back(mill);
  for (auto& op : patched.cam.operations) {
    if (op.op_id == opId) {
      op.tool_id = "tool-mill";
    }
  }

  const auto outcome = polysmith::core::generate_operation_toolpath(
      patched, opId, /*preview=*/false);
  return expect(outcome.found && !outcome.result.ok &&
                    outcome.result.error_message.find("laser tool") !=
                        std::string::npos,
                "tool check: laser op with a mill tool is rejected");
}

// ── Test 34: engrave test grid — power columns × speed rows ──────

bool test_test_pattern_engrave_grid() {
  DocumentManager manager;
  manager.create_document();

  polysmith::core::LaserTestPatternParameters pattern;
  pattern.pattern = "engrave_grid";
  pattern.power_min_percent = 10.0;
  pattern.power_max_percent = 100.0;
  pattern.power_steps = 3;
  pattern.speed_min_mm_per_s = 5.0;
  pattern.speed_max_mm_per_s = 50.0;
  pattern.speed_steps = 3;
  pattern.cell_size_mm = 10.0;
  pattern.cell_spacing_mm = 5.0;
  pattern.start_x_mm = 5.0;
  pattern.start_y_mm = 5.0;
  pattern.line_spacing_mm = 1.0;
  pattern.cell_labels = false;  // grid geometry, not label coverage
  const std::string opId = make_test_pattern_op(manager, pattern);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "test pattern: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  // Every cell gets laser-on fill at its mapped power/speed, and no
  // move escapes the grid bbox.
  const double cellStep = 15.0;
  bool cellsFound[3][3] = {};
  bool sawTravel = false;
  for (const auto& move : toolpath.moves) {
    if (move.x < 5.0 - 0.01 || move.x > 35.0 + 10.0 + 0.01 ||
        move.y < 5.0 - 0.01 || move.y > 35.0 + 10.0 + 0.01) {
      std::cerr << "  move outside the grid at " << move.x << ", " << move.y
                << "\n";
      return expect(false, "test pattern: every move stays on the card");
    }
    if (!move.laser_on) {
      sawTravel = true;
      continue;
    }
    const int col = static_cast<int>((move.x - 5.0) / cellStep);
    const int row = static_cast<int>((move.y - 5.0) / cellStep);
    if (col < 0 || col > 2 || row < 0 || row > 2) {
      continue;  // a connector landing between cells
    }
    cellsFound[row][col] = true;
  }
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      if (!cellsFound[row][col]) {
        std::cerr << "  missing cell " << row << "," << col << "\n";
        return expect(false, "test pattern: all 9 cells engraved");
      }
    }
  }
  if (!expect(sawTravel, "test pattern: laser-off travel between cells")) {
    return false;
  }
  // Corner cells carry the extremes: first cell = min power + min
  // speed (F300); last cell = max power + max speed (F3000).
  bool sawFirst = false;
  bool sawLast = false;
  for (const auto& move : toolpath.moves) {
    if (!move.laser_on) {
      continue;
    }
    if (move.x >= 5.0 && move.x <= 15.0 && move.y >= 5.0 && move.y <= 15.0 &&
        near(move.power_percent, 10.0, 0.01) &&
        near(move.feedrate_mm_per_min, 300.0, 0.01)) {
      sawFirst = true;
    }
    if (move.x >= 35.0 && move.x <= 45.0 && move.y >= 35.0 && move.y <= 45.0 &&
        near(move.power_percent, 100.0, 0.01) &&
        near(move.feedrate_mm_per_min, 3000.0, 0.01)) {
      sawLast = true;
    }
  }
  return expect(sawFirst && sawLast,
                "test pattern: extremes land on the corner cells");
}

// ── Test 35: cut test grid — through-cut squares ─────────────────

bool test_test_pattern_cut_grid() {
  DocumentManager manager;
  manager.create_document();

  polysmith::core::LaserTestPatternParameters pattern;
  pattern.pattern = "cut_grid";
  pattern.power_min_percent = 20.0;
  pattern.power_max_percent = 90.0;
  pattern.power_steps = 2;
  pattern.speed_min_mm_per_s = 8.0;
  pattern.speed_max_mm_per_s = 30.0;
  pattern.speed_steps = 2;
  pattern.cell_size_mm = 10.0;
  pattern.cell_spacing_mm = 5.0;
  pattern.start_x_mm = 0.0;
  pattern.start_y_mm = 0.0;
  pattern.cell_labels = false;  // move counting, not label coverage
  const std::string opId = make_test_pattern_op(manager, pattern);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "cut grid: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  // 4 cells × 4 contour moves, plus laser-off travels between them.
  int onMoves = 0;
  int offMoves = 0;
  bool sawMinPower = false;
  bool sawMaxPower = false;
  for (const auto& move : toolpath.moves) {
    if (move.laser_on) {
      ++onMoves;
      if (near(move.power_percent, 20.0, 0.01)) {
        sawMinPower = true;
      }
      if (near(move.power_percent, 90.0, 0.01)) {
        sawMaxPower = true;
      }
    } else {
      ++offMoves;
    }
  }
  if (!expect(onMoves == 4 * 4,
              "cut grid: 4 contour moves per cell")) {
    std::cerr << "  on moves=" << onMoves << " off moves=" << offMoves << "\n";
    return false;
  }
  if (!expect(offMoves == 4, "cut grid: one travel per cell")) {
    return false;
  }
  return expect(sawMinPower && sawMaxPower,
                "cut grid: power sweeps across the cells");
}

// ── Test 37: kerf gauge — calibration square at the current kerf ─

bool test_test_pattern_kerf_gauge() {
  DocumentManager manager;
  manager.create_document();

  polysmith::core::LaserTestPatternParameters pattern;
  pattern.pattern = "kerf_gauge";
  pattern.cell_size_mm = 20.0;
  pattern.start_x_mm = 5.0;
  pattern.start_y_mm = 5.0;
  pattern.kerf_width_mm = 0.2;
  pattern.power_percent = 60.0;
  pattern.speed_mm_per_s = 15.0;
  const std::string opId = make_test_pattern_op(manager, pattern);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "kerf gauge: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  // The contour keeps its distance from the nominal corner: the
  // round-join arc grazes at exactly kerf/2 = 0.1 from (5, 5).
  double minDistance = 1e9;
  bool sawRawCorner = false;
  for (const auto& move : toolpath.moves) {
    if (!move.laser_on) {
      continue;
    }
    if (near(move.x, 5.0, 0.02) && near(move.y, 5.0, 0.02)) {
      sawRawCorner = true;
    }
    if (move.y > 27.0) {
      continue;  // the engraved KERF label
    }
    if (!near(move.power_percent, 60.0, 0.01) ||
        !near(move.feedrate_mm_per_min, 900.0, 0.01)) {
      std::cerr << "  gauge move power " << move.power_percent << " feed "
                << move.feedrate_mm_per_min << "\n";
      return expect(false, "kerf gauge: cut at the gauge power/speed");
    }
    minDistance = std::min(minDistance, dist(move.x, move.y, 5.0, 5.0));
  }
  if (!expect(!sawRawCorner &&
                  minDistance > 0.08 && minDistance < 0.12,
              "kerf gauge: cut offset by kerf/2 from the nominal square")) {
    std::cerr << "  min distance to the nominal corner: " << minDistance
              << "\n";
    return false;
  }
  // The KERF label engraves below the square.
  for (const auto& move : toolpath.moves) {
    if (move.laser_on && move.y > 27.0 && move.y < 30.5) {
      return true;
    }
  }
  std::cerr << "  no KERF label moves\n";
  return false;
}

// ── Test 38: test-card cells carry engraved P/S labels ───────────

bool test_test_pattern_cell_labels() {
  DocumentManager manager;
  manager.create_document();

  polysmith::core::LaserTestPatternParameters pattern;
  pattern.pattern = "engrave_grid";
  pattern.power_min_percent = 10.0;
  pattern.power_max_percent = 100.0;
  pattern.power_steps = 2;
  pattern.speed_min_mm_per_s = 5.0;
  pattern.speed_max_mm_per_s = 50.0;
  pattern.speed_steps = 2;
  pattern.cell_size_mm = 10.0;
  pattern.cell_spacing_mm = 5.0;
  pattern.start_x_mm = 5.0;
  pattern.start_y_mm = 5.0;
  pattern.line_spacing_mm = 1.0;
  const std::string opId = make_test_pattern_op(manager, pattern);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "labels: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  // Labels sit under the first row of cells (centered at y ≈ 17.5).
  bool sawLabel = false;
  for (const auto& move : outcome.result.toolpath.moves) {
    if (move.laser_on && move.y > 16.0 && move.y < 19.5) {
      sawLabel = true;
    }
  }
  if (!expect(sawLabel, "labels: engraved under the first row")) {
    return false;
  }
  for (const auto& warning : outcome.result.warnings) {
    if (warning.find("label") != std::string::npos) {
      std::cerr << "  warning: " << warning << "\n";
      return expect(false, "labels: text layout produced no warnings");
    }
  }

  // cell_labels=false suppresses them.
  pattern.cell_labels = false;
  const std::string opId2 = make_test_pattern_op(manager, pattern);
  const auto without = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId2, /*preview=*/false);
  for (const auto& move : without.result.toolpath.moves) {
    if (move.laser_on && move.y > 16.0 && move.y < 19.5) {
      return expect(false, "labels: suppressed when cell_labels is off");
    }
  }
  return true;
}

// ── Test 36: the card must fit the machine work area ─────────────

bool test_test_pattern_bed_overflow() {
  DocumentManager manager;
  manager.create_document();

  polysmith::core::LaserTestPatternParameters pattern;
  pattern.pattern = "cut_grid";
  pattern.power_steps = 2;
  pattern.speed_steps = 2;
  pattern.cell_size_mm = 30.0;
  pattern.cell_spacing_mm = 5.0;
  pattern.start_x_mm = 5.0;
  pattern.start_y_mm = 5.0;
  const std::string opId = make_test_pattern_op(manager, pattern);

  // A 60×60 card on a 40×40 bed must warn.
  DocumentState patched = manager.get_document().value();
  polysmith::core::LaserMachineSettings machine;
  machine.work_area_x_mm = 40.0;
  machine.work_area_y_mm = 40.0;
  patched.cam.machine_settings = machine;

  const auto outcome = polysmith::core::generate_operation_toolpath(
      patched, opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "bed overflow: generation still succeeds")) {
    return false;
  }
  for (const auto& warning : outcome.result.warnings) {
    if (warning.find("work area") != std::string::npos) {
      return true;
    }
  }
  std::cerr << "  no work-area warning\n";
  return false;
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
  run("Test 9: rotated plane arcs", test_rotated_plane_arcs);
  run("Test 10: mirrored frame flips sweep", test_mirrored_frame_flips_sweep);
  run("Test 11: non-horizontal plane rejected",
      test_non_horizontal_plane_rejected);
  run("Test 12: hairline slot degrades", test_hairline_slot_degrades);
  run("Test 13: passes repeat the contour", test_laser_passes);
  run("Test 14: speed drives the feedrate", test_laser_speed);
  run("Test 15: kerf side overrides", test_kerf_side_override);
  run("Test 16: mode validation + thickness warning",
      test_laser_mode_validation);
  run("Test 17: score mode", test_score_mode);
  run("Test 18: inner-first ordering", test_inner_first_ordering);
  run("Test 19: nearest-neighbor travel", test_nearest_neighbor_travel);
  run("Test 20: nested region ordering", test_nested_region_ordering);
  run("Test 21: arc lead-in", test_arc_lead_in);
  run("Test 22: angled line lead", test_angled_lead_in);
  run("Test 23: sharp corner pierce", test_sharp_corner_pierce);
  run("Test 24: overcut", test_overcut);
  run("Test 25: pierce position", test_pierce_position);
  run("Test 26: tabs on outer contours", test_tabs_on_outer_only);
  run("Test 27: tab power", test_tab_power);
  run("Test 28: tiny loop tabs", test_tabs_tiny_loop);
  run("Test 29: engrave fill hatch", test_fill_hatch_rect);
  run("Test 30: fill hole exclusion", test_fill_hole_exclusion);
  run("Test 31: fill angle + one-way", test_fill_angle_and_unidirectional);
  run("Test 32: end-to-end G-code export", test_export_gcode_end_to_end);
  run("Test 33: laser op rejects a mill tool", test_laser_requires_laser_tool);
  run("Test 34: engrave test grid", test_test_pattern_engrave_grid);
  run("Test 35: cut test grid", test_test_pattern_cut_grid);
  run("Test 36: test card bed overflow", test_test_pattern_bed_overflow);
  run("Test 37: kerf gauge square", test_test_pattern_kerf_gauge);
  run("Test 38: test-card cell labels", test_test_pattern_cell_labels);

  if (allPassed) {
    std::cout << "cam_generators_test passed\n";
    return 0;
  }
  return 1;
}

