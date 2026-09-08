// 2D Contour generator test — the contour operation end-to-end.
//
// Face witness → largest-|area| wire → tool-radius offset (inside /
// outside / on-line) → single closed loop at a fixed cut plane, with
// exact G2/G3 arcs for circular wires and a chord-tolerance polyline
// fallback for ellipse edges.  Pins all four side × direction combos
// (climb external = CCW, climb internal = CW — the pocket precedent),
// the on-line allowance warning, sketch-profile input (world mapping +
// left-handed frame arc flips), face-over-profile precedence, the
// absent-block defaults, the error paths (no input, non-horizontal
// face), the multi-wire outer selection, and the contour parameter
// payload round-trip.

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "core/cam/cam_generate.h"
#include "core/cam/cam_operation.h"
#include "core/cam/cam_profile_reference.h"
#include "core/document/document.h"
#include "core/geometry/body_compiler.h"
#include "core/sketch/sketch_feature_parameters.h"
#include "protocol/serialization.h"

#include <BRepAdaptor_Surface.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

namespace {

using polysmith::core::CamGenerateOutcome;
using polysmith::core::CamGenerateResult;
using polysmith::core::CamOperation;
using polysmith::core::CamSetup;
using polysmith::core::CompiledBody;
using polysmith::core::ContourParameters;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FaceAttestation;
using polysmith::core::GeometryReference;
using polysmith::core::SketchProfileAttestation;
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

// 0-based face-map index of an upward face of `body` centered within
// `tolerance` of `targetZ`; -1 when none (pocket fixture clone).
int find_upward_face_at_z(const CompiledBody& body, double targetZ,
                          double tolerance = 0.01) {
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
        // Orientation-correct: both caps parameterize +Z.
        if (face.Orientation() == TopAbs_REVERSED) {
          normal.Reverse();
        }
        if (normal.Z() > 0.99 && std::abs(center.Z() - targetZ) < tolerance) {
          return i - 1;
        }
      }
    } catch (const std::exception&) {
      continue;
    }
  }
  return -1;
}

// 0-based face-map index of a VERTICAL face of `body` (normal within
// ~5° of the XY plane); -1 when none (pocket fixture clone).
int find_vertical_face(const CompiledBody& body) {
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
        if (std::abs(normal.Z()) < 0.1) {
          return i - 1;
        }
      }
    } catch (const std::exception&) {
      continue;
    }
  }
  return -1;
}

// Face witness reference for (body, faceIndex) — the capture pattern
// shared with the face-milling and pocket tests.
GeometryReference capture_face_ref(const CompiledBody& body, int faceIndex) {
  const auto ref = polysmith::core::capture_face_reference(
      body.id, body.shape, faceIndex, "face");
  FaceAttestation att;
  att.area = ref->capturedArea;
  att.normal = ref->capturedNormal;
  for (const auto& p : ref->samplePoints) {
    att.sample_points.push_back(p);
  }
  GeometryReference stored;
  stored.persistent_id = body.id + ":face:" + std::to_string(faceIndex);
  stored.attestation = att;
  return stored;
}

// Profile witness reference for a sketch region — the laser-cut
// capture pattern (make_laser_op).
GeometryReference capture_profile_ref(const DocumentState& document,
                                      const std::string& sketch_feature_id,
                                      const std::string& profile_id) {
  for (const auto& feature : document.feature_history) {
    if (feature.id != sketch_feature_id ||
        !feature.sketch_parameters.has_value()) {
      continue;
    }
    for (const auto& region : feature.sketch_parameters->profiles) {
      if (region.id != profile_id) {
        continue;
      }
      const auto ref = polysmith::core::capture_profile_reference(
          sketch_feature_id, region);
      if (!ref.has_value()) {
        throw std::runtime_error("profile capture failed");
      }
      SketchProfileAttestation att;
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
      GeometryReference stored;
      stored.persistent_id = region.id;
      stored.attestation = att;
      return stored;
    }
  }
  throw std::runtime_error("no profile matched");
}

// The last sketch feature's id; throws when none exists.
std::string sketch_feature_id(const DocumentState& document) {
  for (auto it = document.feature_history.rbegin();
       it != document.feature_history.rend(); ++it) {
    if (it->kind == "sketch") {
      return it->id;
    }
  }
  throw std::runtime_error("no sketch feature");
}

// The last sketch's first profile id; throws when none exists.
std::string first_profile_id(const DocumentState& document) {
  for (auto it = document.feature_history.rbegin();
       it != document.feature_history.rend(); ++it) {
    if (it->kind == "sketch" && it->sketch_parameters.has_value() &&
        !it->sketch_parameters->profiles.empty()) {
      return it->sketch_parameters->profiles[0].id;
    }
  }
  throw std::runtime_error("no sketch profile");
}

// Mill setup + 4 mm endmill + contour_2d op with the given machining
// region references.  Returns the op id.
std::string make_contour_op(
    DocumentManager& manager, DocumentState& document,
    const std::vector<GeometryReference>& regions,
    const ContourParameters& contour, const std::string& direction = "climb",
    bool includeContourBlock = true, double retractHeight = 20.0,
    const std::optional<std::array<double, 3>>& wcsOrigin = std::nullopt) {
  CamSetup setup;
  setup.name = "Mill setup";
  setup.machine_type = "3_axis_mill";
  setup.retract_height = retractHeight;
  if (wcsOrigin.has_value()) {
    setup.wcs_origin.anchor = "point";
    setup.wcs_origin.position = wcsOrigin;
  }
  document = manager.cam_setup_create(setup);

  ToolEntry tool;
  tool.name = "4mm endmill";
  tool.type = "endmill_flat";
  tool.diameter_mm = 4.0;
  tool.default_feedrate_mm_per_min = 500.0;
  tool.default_plunge_feedrate_mm_per_min = 200.0;
  document = manager.cam_tool_add(tool);

  CamOperation op;
  op.name = "Contour 1";
  op.type = "contour_2d";
  op.tool_id = document.cam.tool_library[0].tool_id;
  op.parameters.feedrate_mm_per_min = 500.0;
  op.parameters.plunge_feedrate_mm_per_min = 200.0;
  op.parameters.cutting_direction = direction;
  if (includeContourBlock) {
    op.parameters.contour = contour;
  }
  op.geometry_references.machining_regions = regions;
  document = manager.cam_operation_add(op);
  return document.cam.operations.back().op_id;
}

CamGenerateOutcome generate(DocumentManager& manager,
                            const std::string& opId) {
  return polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
}

bool has_warning(const CamGenerateResult& result,
                 const std::string& needle) {
  for (const auto& warning : result.warnings) {
    if (warning.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

size_t count_feeds(const Toolpath& toolpath) {
  size_t count = 0;
  for (const auto& move : toolpath.moves) {
    if (move.kind != ToolpathMoveKind::Rapid) {
      ++count;
    }
  }
  return count;
}

int count_arcs(const Toolpath& toolpath) {
  int count = 0;
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::FeedArcCW ||
        move.kind == ToolpathMoveKind::FeedArcCCW) {
      ++count;
    }
  }
  return count;
}

// Signed twice-area of the consecutive feed chain (plunge + closed
// loop; rapids break the chain).  Positive = CCW walk.  The plunge
// repeats the first loop point — a zero-length shoelace edge.
double feed_walk_twice_area(const Toolpath& toolpath) {
  double twice = 0.0;
  double prevX = 0.0, prevY = 0.0;
  bool havePrev = false;
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::Rapid) {
      havePrev = false;
      continue;
    }
    if (havePrev) {
      twice += prevX * move.y - move.x * prevY;
    }
    prevX = move.x;
    prevY = move.y;
    havePrev = true;
  }
  return twice;
}

// Every feed endpoint lies at `z` and near one of `corners`, and every
// corner is hit (the 4 miter corners of a rect offset are the full
// endpoint set — plunge + closing loop).
bool feeds_cover_corners(
    const Toolpath& toolpath,
    const std::vector<std::array<double, 2>>& corners, double z,
    double tolerance = 0.01) {
  std::vector<bool> hit(corners.size(), false);
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::Rapid) {
      continue;
    }
    if (!near(move.z, z, 0.001)) {
      std::cerr << "  feed at z=" << move.z << " (expected " << z << ")\n";
      return false;
    }
    bool matched = false;
    for (size_t i = 0; i < corners.size(); ++i) {
      if (near(move.x, corners[i][0], tolerance) &&
          near(move.y, corners[i][1], tolerance)) {
        hit[i] = true;
        matched = true;
        break;
      }
    }
    if (!matched) {
      std::cerr << "  feed at (" << move.x << ", " << move.y
                << ") not on the expected band\n";
      return false;
    }
  }
  for (const bool cornerHit : hit) {
    if (!cornerHit) {
      return false;
    }
  }
  return true;
}

// The shared rect-contour checker for the four side × direction
// combos: exact move shape (rapid → plunge → 4 corners → rapid), the
// corner locus, and the walk direction.
bool check_rect_contour(
    const Toolpath& toolpath,
    const std::vector<std::array<double, 2>>& corners, bool expectCcw,
    double z, double retractZ, const char* label) {
  if (!expect(toolpath.moves.size() == 7 && count_feeds(toolpath) == 5,
              label)) {
    std::cerr << "  moves=" << toolpath.moves.size()
              << " feeds=" << count_feeds(toolpath) << "\n";
    return false;
  }
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::Rapid &&
        !near(move.z, retractZ, 0.001)) {
      std::cerr << "  rapid at z=" << move.z << " (expected retract "
                << retractZ << ")\n";
      return expect(false, label);
    }
  }
  if (!feeds_cover_corners(toolpath, corners, z)) {
    return expect(false, label);
  }
  const double twice = feed_walk_twice_area(toolpath);
  if (expectCcw && twice <= 0.0) {
    std::cerr << "  expected CCW walk, twice-area=" << twice << "\n";
    return expect(false, label);
  }
  if (!expectCcw && twice >= 0.0) {
    std::cerr << "  expected CW walk, twice-area=" << twice << "\n";
    return expect(false, label);
  }
  return true;
}

// ── Tests 1–4: the four side × direction combos ──────────────────
//
// A 20×20×10 box, Ø4 tool (r_eff 2), depth 1 → cut plane z=9.  The
// offset loci are the exact miter corners of the rect: outside
// [−2,22]², inside [2,18]².  Climb external = CCW, climb internal =
// CW; conventional flips both (the pocket finishing-contour walk
// convention).

bool test_face_outside_climb() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const auto compiled = polysmith::core::compile_bodies(document);
  const int topIndex = find_upward_face_at_z(compiled.bodies[0], 10.0);
  const std::string opId = make_contour_op(
      manager, document, {capture_face_ref(compiled.bodies[0], topIndex)},
      ContourParameters{.side = "outside", .depth_mm = 1.0,
                        .stock_allowance_mm = 0.0});

  const auto outcome = generate(manager, opId);
  if (!expect(outcome.found && outcome.result.ok,
              "outside/climb: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  return check_rect_contour(
      outcome.result.toolpath,
      {{-2.0, -2.0}, {22.0, -2.0}, {22.0, 22.0}, {-2.0, 22.0}},
      /*expectCcw=*/true, /*z=*/9.0, /*retractZ=*/20.0,
      "outside/climb: CCW loop 2 mm outside the wire at z=9");
}

bool test_face_inside() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const auto compiled = polysmith::core::compile_bodies(document);
  const int topIndex = find_upward_face_at_z(compiled.bodies[0], 10.0);
  const std::string opId = make_contour_op(
      manager, document, {capture_face_ref(compiled.bodies[0], topIndex)},
      ContourParameters{.side = "inside", .depth_mm = 1.0,
                        .stock_allowance_mm = 0.0});

  const auto outcome = generate(manager, opId);
  if (!expect(outcome.found && outcome.result.ok,
              "inside/climb: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  return check_rect_contour(
      outcome.result.toolpath,
      {{2.0, 2.0}, {18.0, 2.0}, {18.0, 18.0}, {2.0, 18.0}},
      /*expectCcw=*/false, /*z=*/9.0, /*retractZ=*/20.0,
      "inside/climb: CW loop 2 mm inside the wire at z=9");
}

bool test_face_outside_conventional() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const auto compiled = polysmith::core::compile_bodies(document);
  const int topIndex = find_upward_face_at_z(compiled.bodies[0], 10.0);
  const std::string opId = make_contour_op(
      manager, document, {capture_face_ref(compiled.bodies[0], topIndex)},
      ContourParameters{.side = "outside", .depth_mm = 1.0,
                        .stock_allowance_mm = 0.0},
      /*direction=*/"conventional");

  const auto outcome = generate(manager, opId);
  if (!expect(outcome.found && outcome.result.ok,
              "outside/conventional: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  return check_rect_contour(
      outcome.result.toolpath,
      {{-2.0, -2.0}, {22.0, -2.0}, {22.0, 22.0}, {-2.0, 22.0}},
      /*expectCcw=*/false, /*z=*/9.0, /*retractZ=*/20.0,
      "outside/conventional: CW loop, same locus");
}

bool test_face_inside_conventional() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const auto compiled = polysmith::core::compile_bodies(document);
  const int topIndex = find_upward_face_at_z(compiled.bodies[0], 10.0);
  const std::string opId = make_contour_op(
      manager, document, {capture_face_ref(compiled.bodies[0], topIndex)},
      ContourParameters{.side = "inside", .depth_mm = 1.0,
                        .stock_allowance_mm = 0.0},
      /*direction=*/"conventional");

  const auto outcome = generate(manager, opId);
  if (!expect(outcome.found && outcome.result.ok,
              "inside/conventional: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  return check_rect_contour(
      outcome.result.toolpath,
      {{2.0, 2.0}, {18.0, 2.0}, {18.0, 18.0}, {2.0, 18.0}},
      /*expectCcw=*/true, /*z=*/9.0, /*retractZ=*/20.0,
      "inside/conventional: CCW loop, same locus");
}

// ── Test 5: on-line — the base wire itself, no offset ─────────────

bool test_on_line() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const auto compiled = polysmith::core::compile_bodies(document);
  const int topIndex = find_upward_face_at_z(compiled.bodies[0], 10.0);
  const std::string opId = make_contour_op(
      manager, document, {capture_face_ref(compiled.bodies[0], topIndex)},
      ContourParameters{.side = "on_line", .depth_mm = 1.0,
                        .stock_allowance_mm = 0.0});

  const auto outcome = generate(manager, opId);
  if (!expect(outcome.found && outcome.result.ok,
              "on_line: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  if (!check_rect_contour(
          outcome.result.toolpath,
          {{0.0, 0.0}, {20.0, 0.0}, {20.0, 20.0}, {0.0, 20.0}},
          /*expectCcw=*/true, /*z=*/9.0, /*retractZ=*/20.0,
          "on_line: the tool centre rides the wire itself")) {
    return false;
  }
  return expect(!has_warning(outcome.result, "allowance"),
                "on_line: no allowance warning at zero allowance");
}

// ── Test 6: on-line allowance is warned and ignored ───────────────

bool test_on_line_allowance() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const auto compiled = polysmith::core::compile_bodies(document);
  const int topIndex = find_upward_face_at_z(compiled.bodies[0], 10.0);
  const std::string opId = make_contour_op(
      manager, document, {capture_face_ref(compiled.bodies[0], topIndex)},
      ContourParameters{.side = "on_line", .depth_mm = 1.0,
                        .stock_allowance_mm = 0.5});

  const auto outcome = generate(manager, opId);
  if (!expect(outcome.found && outcome.result.ok,
              "on_line allowance: generation succeeds")) {
    return false;
  }
  if (!expect(has_warning(outcome.result, "ignored"),
              "on_line allowance: the allowance is warned as ignored")) {
    return false;
  }
  // The locus stays on the wire — the allowance does not shift it.
  return check_rect_contour(
      outcome.result.toolpath,
      {{0.0, 0.0}, {20.0, 0.0}, {20.0, 20.0}, {0.0, 20.0}},
      /*expectCcw=*/true, /*z=*/9.0, /*retractZ=*/20.0,
      "on_line allowance: locus still on the wire");
}

// ── Test 7: circular face → one exact arc (G3) ────────────────────
//
// A Ø20 cylinder (top face z=10): the wire is a full circle at the
// origin, radius 10.  Outside + climb → ONE full-circle arc, radius
// 12, CCW — emitted as FeedArcCCW with I/J = center − start in world
// coordinates.

bool test_circle_face_exact_arc() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_cylinder_feature(
      polysmith::core::CylinderFeatureParameters{.radius = 10.0,
                                                 .height = 10.0});
  const auto compiled = polysmith::core::compile_bodies(document);
  const int topIndex = find_upward_face_at_z(compiled.bodies[0], 10.0);
  const std::string opId = make_contour_op(
      manager, document, {capture_face_ref(compiled.bodies[0], topIndex)},
      ContourParameters{.side = "outside", .depth_mm = 1.0,
                        .stock_allowance_mm = 0.0});

  const auto outcome = generate(manager, opId);
  if (!expect(outcome.found && outcome.result.ok,
              "circle: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;
  if (!expect(toolpath.moves.size() == 4 && count_arcs(toolpath) == 1,
              "circle: rapid → plunge → ONE arc → rapid")) {
    for (const auto& move : toolpath.moves) {
      std::cerr << "  kind=" << static_cast<int>(move.kind)
                << " (" << move.x << ", " << move.y << ", " << move.z
                << ") i=" << move.i << " j=" << move.j << "\n";
    }
    return false;
  }
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::FeedArcCW ||
        move.kind == ToolpathMoveKind::FeedArcCCW) {
      // Locus: radius 12 around the origin; I/J = centre − start.
      if (!expect(move.kind == ToolpathMoveKind::FeedArcCCW,
                  "circle: outside/climb emits CCW (G3)")) {
        return false;
      }
      if (!expect(near(dist(move.x, move.y, 0.0, 0.0), 12.0, 0.01) &&
                      near(std::hypot(move.i, move.j), 12.0, 0.01),
                  "circle: arc rides radius 12")) {
        return false;
      }
      // Start = (12, 0): I/J = (0 − 12, 0 − 0).
      return expect(near(move.i, -12.0, 0.01) && near(move.j, 0.0, 0.01),
                    "circle: I/J = centre − start");
    }
  }
  return expect(false, "circle: no arc move found");
}

// ── Test 8: circle inside → radius 8, CW (G2) ─────────────────────

bool test_circle_inside() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_cylinder_feature(
      polysmith::core::CylinderFeatureParameters{.radius = 10.0,
                                                 .height = 10.0});
  const auto compiled = polysmith::core::compile_bodies(document);
  const int topIndex = find_upward_face_at_z(compiled.bodies[0], 10.0);
  const std::string opId = make_contour_op(
      manager, document, {capture_face_ref(compiled.bodies[0], topIndex)},
      ContourParameters{.side = "inside", .depth_mm = 1.0,
                        .stock_allowance_mm = 0.0});

  const auto outcome = generate(manager, opId);
  if (!expect(outcome.found && outcome.result.ok,
              "circle inside: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;
  if (!expect(count_arcs(toolpath) == 1,
              "circle inside: exactly one arc")) {
    return false;
  }
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::FeedArcCW ||
        move.kind == ToolpathMoveKind::FeedArcCCW) {
      if (!expect(move.kind == ToolpathMoveKind::FeedArcCW,
                  "circle inside: climb emits CW (G2)")) {
        return false;
      }
      if (!expect(near(dist(move.x, move.y, 0.0, 0.0), 8.0, 0.01) &&
                      near(std::hypot(move.i, move.j), 8.0, 0.01),
                  "circle inside: arc rides radius 8")) {
        return false;
      }
      return expect(near(move.i, -8.0, 0.01) && near(move.j, 0.0, 0.01),
                    "circle inside: I/J = centre − start");
    }
  }
  return expect(false, "circle inside: no arc move found");
}

// ── Test 9: ellipse wire → polyline fallback, no arcs ─────────────
//
// A 6×4 ellipse boss extruded 2 mm proud of the box top (the sketch
// sits on z=0, so the join runs the full 12 mm; its cap at z=12) has
// no line/circle edges — the exact builder refuses, and the chord-
// tolerance polyline fallback emits only linear feeds outside the
// ellipse at z=11.

bool test_ellipse_fallback() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const std::string bodyId =
      polysmith::core::compile_bodies(document).bodies[0].id;
  document = manager.start_sketch_on_plane("ref-plane-xy");
  // Full ellipse: center (10,10) + major-axis point (16,10) + minor-
  // axis point (10,14) → a=6 along X, b=4 along Y.
  document = manager.add_sketch_ellipse(10.0, 10.0, 16.0, 10.0, 10.0, 14.0);
  const std::string profile = first_profile_id(document);
  document = manager.extrude_profiles({profile}, 12.0, "join", bodyId,
                                       std::nullopt);
  const auto compiled = polysmith::core::compile_bodies(document);
  const int topIndex = find_upward_face_at_z(compiled.bodies[0], 12.0);
  if (!expect(topIndex >= 0, "ellipse: prism top face found")) {
    return false;
  }
  const std::string opId = make_contour_op(
      manager, document, {capture_face_ref(compiled.bodies[0], topIndex)},
      ContourParameters{.side = "outside", .depth_mm = 1.0,
                        .stock_allowance_mm = 0.0});

  const auto outcome = generate(manager, opId);
  if (!expect(outcome.found && outcome.result.ok,
              "ellipse: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;
  if (!expect(count_arcs(toolpath) == 0 && count_feeds(toolpath) >= 16,
              "ellipse: polyline fallback — many linear feeds, no arcs")) {
    return false;
  }
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::Rapid) {
      continue;
    }
    if (!near(move.z, 11.0, 0.001)) {
      return expect(false, "ellipse: feeds at the cut plane z=11");
    }
    // The outward offset stays outside the ellipse everywhere.
    const double u = (move.x - 10.0) / 6.0;
    const double v = (move.y - 10.0) / 4.0;
    if (u * u + v * v < 1.0 - 0.05) {
      std::cerr << "  feed inside the ellipse at (" << move.x << ", "
                << move.y << ")\n";
      return expect(false, "ellipse: no feed inside the ellipse");
    }
  }
  return true;
}

// ── Test 10: sketch-profile input (rectangle) ─────────────────────
//
// A selected sketch profile replaces the face witness: the loop maps
// through the sketch frame to world coordinates at z = sketchZ − 1.

bool test_profile_input_rect() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.add_sketch_rectangle(2.0, 2.0, 18.0, 18.0);
  const std::string sketchId = sketch_feature_id(document);
  const std::string profile = first_profile_id(document);
  const std::string opId = make_contour_op(
      manager, document, {capture_profile_ref(document, sketchId, profile)},
      ContourParameters{.side = "outside", .depth_mm = 1.0,
                        .stock_allowance_mm = 0.0});

  const auto outcome = generate(manager, opId);
  if (!expect(outcome.found && outcome.result.ok,
              "profile rect: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  // The sketch plane is z=0: the cut plane sits at −1, and the 2 mm
  // offset expands [2,18]² to [0,20]² in world coordinates.
  return check_rect_contour(
      outcome.result.toolpath,
      {{0.0, 0.0}, {20.0, 0.0}, {20.0, 20.0}, {0.0, 20.0}},
      /*expectCcw=*/true, /*z=*/-1.0, /*retractZ=*/20.0,
      "profile rect: world loop 2 mm outside the profile at z=-1");
}

// ── Test 11: left-handed sketch frame flips the arc sweep ─────────
//
// A circle profile on a mirrored (left-handed) frame: the CCW sketch
// circle must emit as a CW world arc (laser precedent) at the mirrored
// world centre (10, −5), radius 7.

bool test_profile_circle_left_handed() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.add_sketch_circle(10.0, 5.0, 5.0);
  const std::string sketchId = sketch_feature_id(document);
  const std::string profile = first_profile_id(document);
  const std::string opId = make_contour_op(
      manager, document, {capture_profile_ref(document, sketchId, profile)},
      ContourParameters{.side = "outside", .depth_mm = 1.0,
                        .stock_allowance_mm = 0.0});

  // Mirror the sketch across the X axis: x=(1,0,0), y=(0,−1,0),
  // normal=(0,0,1) — a left-handed frame pair, so the CCW sketch
  // circle must emit as a CW world arc.
  DocumentState mirrored = manager.get_document().value();
  for (auto& feature : mirrored.feature_history) {
    if (feature.kind != "sketch") {
      continue;
    }
    feature.sketch_parameters->plane_frame =
        polysmith::core::SketchFeatureParameters::SketchPlaneFrame{
            .origin_x = 0.0, .origin_y = 0.0, .origin_z = 0.0,
            .x_axis_x = 1.0, .x_axis_y = 0.0, .x_axis_z = 0.0,
            .y_axis_x = 0.0, .y_axis_y = -1.0, .y_axis_z = 0.0,
            .normal_x = 0.0, .normal_y = 0.0, .normal_z = 1.0,
        };
  }

  const auto outcome = polysmith::core::generate_operation_toolpath(
      mirrored, opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "mirrored circle: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;
  if (!expect(count_arcs(toolpath) == 1,
              "mirrored circle: exactly one arc")) {
    return false;
  }
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::FeedArcCW ||
        move.kind == ToolpathMoveKind::FeedArcCCW) {
      if (!expect(move.kind == ToolpathMoveKind::FeedArcCW,
                  "mirrored circle: CCW sketch emits as CW world arc")) {
        return false;
      }
      // World centre (10, −5); radius 5 + 2 = 7; start = (17, −5).
      if (!expect(near(dist(move.x, move.y, 10.0, -5.0), 7.0, 0.01) &&
                      near(std::hypot(move.i, move.j), 7.0, 0.01),
                  "mirrored circle: arc rides radius 7 at (10, −5)")) {
        std::cerr << "  arc at (" << move.x << ", " << move.y << ") i="
                  << move.i << " j=" << move.j << "\n";
        return false;
      }
      return expect(near(move.i, -7.0, 0.01) && near(move.j, 0.0, 0.01),
                    "mirrored circle: I/J = world centre − world start");
    }
  }
  return expect(false, "mirrored circle: no arc move found");
}

// ── Test 12: face wins over a selected profile ────────────────────

bool test_face_wins() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.add_sketch_circle(10.0, 10.0, 3.0);
  const std::string sketchId = sketch_feature_id(document);
  const std::string profile = first_profile_id(document);
  const auto compiled = polysmith::core::compile_bodies(document);
  const int topIndex = find_upward_face_at_z(compiled.bodies[0], 10.0);
  const std::string opId = make_contour_op(
      manager, document,
      {capture_face_ref(compiled.bodies[0], topIndex),
       capture_profile_ref(document, sketchId, profile)},
      ContourParameters{.side = "outside", .depth_mm = 1.0,
                        .stock_allowance_mm = 0.0});

  const auto outcome = generate(manager, opId);
  if (!expect(outcome.found && outcome.result.ok,
              "face wins: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  if (!expect(has_warning(outcome.result, "uses the face"),
              "face wins: warns that the face is used")) {
    return false;
  }
  // The FACE path: outside the box wire — not the 3 mm-radius circle.
  return check_rect_contour(
      outcome.result.toolpath,
      {{-2.0, -2.0}, {22.0, -2.0}, {22.0, 22.0}, {-2.0, 22.0}},
      /*expectCcw=*/true, /*z=*/9.0, /*retractZ=*/20.0,
      "face wins: the face path is generated");
}

// ── Test 13: absent contour block → struct defaults ───────────────
//
// Old documents without the block still generate: outside, depth 1.0,
// no allowance.

bool test_depth_default() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const auto compiled = polysmith::core::compile_bodies(document);
  const int topIndex = find_upward_face_at_z(compiled.bodies[0], 10.0);
  const std::string opId = make_contour_op(
      manager, document, {capture_face_ref(compiled.bodies[0], topIndex)},
      ContourParameters{}, /*direction=*/"climb",
      /*includeContourBlock=*/false);

  const auto outcome = generate(manager, opId);
  if (!expect(outcome.found && outcome.result.ok,
              "defaults: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  return check_rect_contour(
      outcome.result.toolpath,
      {{-2.0, -2.0}, {22.0, -2.0}, {22.0, 22.0}, {-2.0, 22.0}},
      /*expectCcw=*/true, /*z=*/9.0, /*retractZ=*/20.0,
      "defaults: outside, depth 1.0 (cut plane at 9)");
}

// ── Test 14: no input → loud failure ──────────────────────────────

bool test_empty_input() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const std::string opId = make_contour_op(
      manager, document, {},
      ContourParameters{.side = "outside", .depth_mm = 1.0,
                        .stock_allowance_mm = 0.0});

  const auto outcome = generate(manager, opId);
  if (!expect(outcome.found && !outcome.result.ok,
              "empty input: generation fails loudly")) {
    return false;
  }
  return expect(outcome.result.error_message.find("requires a selected face") !=
                    std::string::npos,
                "empty input: the error names the missing input");
}

// ── Test 15: non-horizontal face → loud failure ───────────────────

bool test_non_horizontal_face() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const auto compiled = polysmith::core::compile_bodies(document);
  const int sideIndex = find_vertical_face(compiled.bodies[0]);
  if (!expect(sideIndex >= 0, "non-horizontal: vertical face found")) {
    return false;
  }
  const std::string opId = make_contour_op(
      manager, document, {capture_face_ref(compiled.bodies[0], sideIndex)},
      ContourParameters{.side = "outside", .depth_mm = 1.0,
                        .stock_allowance_mm = 0.0});

  const auto outcome = generate(manager, opId);
  if (!expect(outcome.found && !outcome.result.ok,
              "non-horizontal: generation fails loudly")) {
    return false;
  }
  return expect(outcome.result.error_message.find("horizontal") !=
                    std::string::npos,
                "non-horizontal: the error names the cause");
}

// ── Test 16: multi-wire face — largest outer wire only ────────────
//
// A 2.5 mm hole through the box top leaves two wires on the face; the
// contour rides the outer 20×20 wire — the hole is NOT contoured in
// v1 (single closed wire).

bool test_multi_wire_face() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const std::string bodyId =
      polysmith::core::compile_bodies(document).bodies[0].id;
  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.add_sketch_circle(10.0, 10.0, 2.5);
  const std::string profile = first_profile_id(document);
  document = manager.extrude_profiles({profile}, 10.0, "cut", bodyId,
                                       std::nullopt);
  const auto compiled = polysmith::core::compile_bodies(document);
  const int topIndex = find_upward_face_at_z(compiled.bodies[0], 10.0);
  const std::string opId = make_contour_op(
      manager, document, {capture_face_ref(compiled.bodies[0], topIndex)},
      ContourParameters{.side = "outside", .depth_mm = 1.0,
                        .stock_allowance_mm = 0.0});

  const auto outcome = generate(manager, opId);
  if (!expect(outcome.found && outcome.result.ok,
              "multi-wire: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;
  // The plunge + 4 outer corners — the hole wire adds no moves.
  if (!expect(count_feeds(toolpath) == 5,
              "multi-wire: only the outer wire is contoured")) {
    std::cerr << "  feeds=" << count_feeds(toolpath) << "\n";
    return false;
  }
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::Rapid) {
      continue;
    }
    if (dist(move.x, move.y, 10.0, 10.0) < 3.0) {
      return expect(false, "multi-wire: no feed near the hole");
    }
  }
  return check_rect_contour(
      toolpath,
      {{-2.0, -2.0}, {22.0, -2.0}, {22.0, 22.0}, {-2.0, 22.0}},
      /*expectCcw=*/true, /*z=*/9.0, /*retractZ=*/20.0,
      "multi-wire: the outer wire loop");
}

// ── Test 17: contour parameters survive the payload round-trip ────

bool test_contour_payload_roundtrip() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const auto compiled = polysmith::core::compile_bodies(document);
  const int topIndex = find_upward_face_at_z(compiled.bodies[0], 10.0);
  const std::string opId = make_contour_op(
      manager, document, {capture_face_ref(compiled.bodies[0], topIndex)},
      ContourParameters{.side = "inside", .depth_mm = 2.5,
                        .stock_allowance_mm = 0.25});

  auto op = std::find_if(
      document.cam.operations.begin(), document.cam.operations.end(),
      [&](const CamOperation& candidate) {
        return candidate.op_id == opId;
      });
  if (!expect(op != document.cam.operations.end(),
              "roundtrip: op found")) {
    return false;
  }

  const polysmith::protocol::json payload =
      polysmith::protocol::to_payload(*op);
  const CamOperation restored =
      polysmith::protocol::cam_operation_from_payload(payload);

  if (!expect(restored.parameters.contour.has_value(),
              "roundtrip: the contour block survives")) {
    return false;
  }
  const auto& contour = restored.parameters.contour.value();
  return expect(contour.side == "inside" && near(contour.depth_mm, 2.5) &&
                    near(contour.stock_allowance_mm, 0.25),
                "roundtrip: side/depth/allowance preserved");
}

// ── Test 18: retract height is WCS-relative ───────────────────────
//
// The user's reported case: a setup whose WCS origin sits at world
// z=20 (origin picked on a 20 mm-tall base), a boss reaching z=30 and
// 3 mm stock → stock top 33.  retract_height is machine Z above the
// WCS origin, so the retract PLANE is world 20 + 25 = 45 — the
// generators must guard and emit against the plane, not the raw
// height.  Regression: the raw height was compared directly (25 < 33
// → bogus "through unmilled stock" warning while the exported G-code
// actually rapided at machine z=5, INSIDE the stock).

bool test_contour_retract_wcs_relative() {
  // WCS z=20 + retract 25 → plane 45 clears the stock top 33: no
  // retract warnings, rapids at world z=45.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 30.0});
    const auto compiled = polysmith::core::compile_bodies(document);
    const int topIndex = find_upward_face_at_z(compiled.bodies[0], 30.0);
    const std::string opId = make_contour_op(
        manager, document, {capture_face_ref(compiled.bodies[0], topIndex)},
        ContourParameters{.side = "outside", .depth_mm = 1.0,
                          .stock_allowance_mm = 0.0},
        /*direction=*/"climb", /*includeContourBlock=*/true,
        /*retractHeight=*/25.0,
        /*wcsOrigin=*/std::array<double, 3>{0.0, 0.0, 20.0});

    const auto outcome = generate(manager, opId);
    if (!expect(outcome.found && outcome.result.ok,
                "wcs retract: generation succeeds")) {
      std::cerr << "  error: " << outcome.result.error_message << "\n";
      return false;
    }
    if (!expect(!has_warning(outcome.result, "below the stock top") &&
                    !has_warning(outcome.result, "below the cut plane"),
                "wcs retract: retract 25 above the z=20 origin clears "
                "the stock top 33")) {
      for (const auto& warning : outcome.result.warnings) {
        std::cerr << "  warning: " << warning << "\n";
      }
      return false;
    }
    return check_rect_contour(
        outcome.result.toolpath,
        {{-2.0, -2.0}, {22.0, -2.0}, {22.0, 22.0}, {-2.0, 22.0}},
        /*expectCcw=*/true, /*z=*/29.0, /*retractZ=*/45.0,
        "wcs retract: rapids at the origin-relative plane 45");
  }
  // WCS z=20 + retract 10 → plane 30 still clears the cut plane 29 but
  // sits below the stock top 33: the stock-top warning must survive.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 30.0});
    const auto compiled = polysmith::core::compile_bodies(document);
    const int topIndex = find_upward_face_at_z(compiled.bodies[0], 30.0);
    const std::string opId = make_contour_op(
        manager, document, {capture_face_ref(compiled.bodies[0], topIndex)},
        ContourParameters{.side = "outside", .depth_mm = 1.0,
                          .stock_allowance_mm = 0.0},
        /*direction=*/"climb", /*includeContourBlock=*/true,
        /*retractHeight=*/10.0,
        /*wcsOrigin=*/std::array<double, 3>{0.0, 0.0, 20.0});

    const auto outcome = generate(manager, opId);
    if (!expect(outcome.found && outcome.result.ok,
                "wcs retract low: generation succeeds")) {
      std::cerr << "  error: " << outcome.result.error_message << "\n";
      return false;
    }
    if (!expect(has_warning(outcome.result, "below the stock top") &&
                    !has_warning(outcome.result, "below the cut plane"),
                "wcs retract low: plane 30 clears the cut plane 29 but "
                "not the stock top 33 — the stock-top warning remains")) {
      for (const auto& warning : outcome.result.warnings) {
        std::cerr << "  warning: " << warning << "\n";
      }
      return false;
    }
    return check_rect_contour(
        outcome.result.toolpath,
        {{-2.0, -2.0}, {22.0, -2.0}, {22.0, 22.0}, {-2.0, 22.0}},
        /*expectCcw=*/true, /*z=*/29.0, /*retractZ=*/30.0,
        "wcs retract low: rapids at the origin-relative plane 30");
  }
  // No WCS origin (legacy default → origin z=0): the behavior must be
  // unchanged — retract 25 below the stock top 33 still warns.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 30.0});
    const auto compiled = polysmith::core::compile_bodies(document);
    const int topIndex = find_upward_face_at_z(compiled.bodies[0], 30.0);
    const std::string opId = make_contour_op(
        manager, document, {capture_face_ref(compiled.bodies[0], topIndex)},
        ContourParameters{.side = "outside", .depth_mm = 1.0,
                          .stock_allowance_mm = 0.0},
        /*direction=*/"climb", /*includeContourBlock=*/true,
        /*retractHeight=*/25.0);

    const auto outcome = generate(manager, opId);
    if (!expect(outcome.found && outcome.result.ok,
                "default wcs: generation succeeds")) {
      std::cerr << "  error: " << outcome.result.error_message << "\n";
      return false;
    }
    if (!expect(has_warning(outcome.result, "below the stock top"),
                "default wcs: retract 25 below the stock top 33 still "
                "warns")) {
      for (const auto& warning : outcome.result.warnings) {
        std::cerr << "  warning: " << warning << "\n";
      }
      return false;
    }
    return check_rect_contour(
        outcome.result.toolpath,
        {{-2.0, -2.0}, {22.0, -2.0}, {22.0, 22.0}, {-2.0, 22.0}},
        /*expectCcw=*/true, /*z=*/29.0, /*retractZ=*/25.0,
        "default wcs: legacy rapids at world z=25");
  }
}

}  // namespace

int main() {
  // The app registers builtin generators from CadCoreApp::run(); the
  // test process must do it itself (cam_generators_test pattern).
  polysmith::core::register_builtin_cam_generators();
  bool allPassed = true;

  std::cout << "contour_2d_test\n";
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
  run("Test 1: face outside, climb", test_face_outside_climb);
  run("Test 2: face inside", test_face_inside);
  run("Test 3: face outside, conventional", test_face_outside_conventional);
  run("Test 4: face inside, conventional", test_face_inside_conventional);
  run("Test 5: on-line", test_on_line);
  run("Test 6: on-line allowance warning", test_on_line_allowance);
  run("Test 7: circle face exact arc", test_circle_face_exact_arc);
  run("Test 8: circle inside", test_circle_inside);
  run("Test 9: ellipse polyline fallback", test_ellipse_fallback);
  run("Test 10: profile input (rectangle)", test_profile_input_rect);
  run("Test 11: profile circle, left-handed frame", test_profile_circle_left_handed);
  run("Test 12: face wins over profile", test_face_wins);
  run("Test 13: absent block defaults", test_depth_default);
  run("Test 14: empty input", test_empty_input);
  run("Test 15: non-horizontal face", test_non_horizontal_face);
  run("Test 16: multi-wire face", test_multi_wire_face);
  run("Test 17: payload round-trip", test_contour_payload_roundtrip);
  run("Test 18: retract height is WCS-relative",
      test_contour_retract_wcs_relative);

  if (allPassed) {
    std::cout << "contour_2d_test passed\n";
    return 0;
  }
  return 1;
}
