// Mill Engrave generator test — sketch-profile on-line tracing at a
// fixed depth below the sketch plane, end-to-end.
//
// Profile attestation → per-region sketch frame → horizontal-plane
// guard → outer CCW + hole CW walks → exact arcs with chord fallback
// → standalone-circle synthesis → cutZ = sketch plane Z − depth →
// rapid-plunge-feed-rapid per loop → guards → error paths →
// dependency degradation → payload round-trip.

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "core/cam/cam_generate.h"
#include "core/cam/cam_generator.h"
#include "core/cam/cam_operation.h"
#include "core/cam/cam_profile_reference.h"
#include "core/document/document.h"
#include "core/geometry/body_compiler.h"
#include "core/viewport/viewport.h"
#include "protocol/serialization.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Pnt.hxx>

namespace {

using polysmith::core::CamOperation;
using polysmith::core::CamSetup;
using polysmith::core::CompiledBody;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::EdgeAttestation;
using polysmith::core::EngraveParameters;
using polysmith::core::GeometryReference;
using polysmith::core::SketchProfileAttestation;
using polysmith::core::ToolEntry;
using polysmith::core::Toolpath;
using polysmith::core::ToolpathMove;
using polysmith::core::ToolpathMoveKind;
using polysmith::core::ViewportSolidFace;

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

// ── Fixtures ──────────────────────────────────────────────────────

const CamOperation* find_op(const DocumentState& document,
                            const std::string& opId) {
  for (const auto& op : document.cam.operations) {
    if (op.op_id == opId) {
      return &op;
    }
  }
  return nullptr;
}

// The frame of a viewport solid face (the start_sketch_on_face
// contract — the face_projection_arc_test helper).
polysmith::core::SketchFeatureParameters::SketchPlaneFrame frame_of(
    const ViewportSolidFace& face) {
  return polysmith::core::SketchFeatureParameters::SketchPlaneFrame{
      .origin_x = face.plane_frame.origin_x,
      .origin_y = face.plane_frame.origin_y,
      .origin_z = face.plane_frame.origin_z,
      .x_axis_x = face.plane_frame.x_axis_x,
      .x_axis_y = face.plane_frame.x_axis_y,
      .x_axis_z = face.plane_frame.x_axis_z,
      .y_axis_x = face.plane_frame.y_axis_x,
      .y_axis_y = face.plane_frame.y_axis_y,
      .y_axis_z = face.plane_frame.y_axis_z,
      .normal_x = face.plane_frame.normal_x,
      .normal_y = face.plane_frame.normal_y,
      .normal_z = face.plane_frame.normal_z,
  };
}

// The first upward-facing body face (normal +Z).
std::optional<ViewportSolidFace> top_face(const DocumentState& document) {
  const auto viewport = polysmith::core::build_viewport_state(
      std::optional<polysmith::core::DocumentState>(document));
  for (const auto& face : viewport.solid_faces) {
    if (std::abs(face.normal_z - 1.0) < 1e-6) {
      return face;
    }
  }
  return std::nullopt;
}

// Region ids of the named sketch: polygon-kind first (the rect with
// its inner holes), then circle-kind regions in creation order.
struct RegionIds {
  std::string polygon;            // empty when none
  std::vector<std::string> circles;
};
RegionIds region_ids(const DocumentState& document,
                     const std::string& sketch_feature_id) {
  RegionIds out;
  for (const auto& feature : document.feature_history) {
    if (feature.id != sketch_feature_id ||
        !feature.sketch_parameters.has_value()) {
      continue;
    }
    for (const auto& region : feature.sketch_parameters->profiles) {
      if (region.kind == "circle" || region.source_circle_id.has_value()) {
        out.circles.push_back(region.id);
      } else if (out.polygon.empty()) {
        out.polygon = region.id;
      }
    }
  }
  return out;
}

// Witness reference for one named sketch region (the
// cam_generators_test capture pattern).
GeometryReference profile_ref(const DocumentState& document,
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
        throw std::runtime_error("engrave fixture: profile capture failed");
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
  throw std::runtime_error("engrave fixture: no profile matched");
}

// Mill setup + 6 mm endmill + engrave op on the given sketch-region
// witnesses.  Returns the op id.
std::string make_engrave_op(
    DocumentManager& manager, DocumentState& document,
    const std::string& sketch_feature_id,
    const std::vector<std::string>& profile_ids, double depth,
    double retractHeight = 25.0,
    const std::optional<std::array<double, 3>>& stockSize = std::nullopt,
    const std::string& cuttingDirection = "climb") {
  CamSetup setup;
  setup.name = "Mill setup";
  setup.machine_type = "3_axis_mill";
  setup.retract_height = retractHeight;
  if (stockSize.has_value()) {
    setup.stock.type = "bounding_box";
    setup.stock.size = stockSize;
    setup.stock.margin = 0.0;
  }
  document = manager.cam_setup_create(setup);

  ToolEntry tool;
  tool.name = "6mm endmill";
  tool.type = "endmill_flat";
  tool.diameter_mm = 6.0;
  tool.default_feedrate_mm_per_min = 500.0;
  tool.default_plunge_feedrate_mm_per_min = 200.0;
  document = manager.cam_tool_add(tool);

  CamOperation op;
  op.name = "Engrave 1";
  op.type = "engrave";
  op.tool_id = document.cam.tool_library[0].tool_id;
  op.parameters.feedrate_mm_per_min = 1200.0;
  op.parameters.plunge_feedrate_mm_per_min = 600.0;
  op.parameters.cutting_direction = cuttingDirection;
  op.parameters.engrave = EngraveParameters{depth};
  for (const auto& profile_id : profile_ids) {
    op.geometry_references.machining_regions.push_back(
        profile_ref(document, sketch_feature_id, profile_id));
  }
  document = manager.cam_operation_add(op);
  return document.cam.operations.back().op_id;
}

// Box 20×20×10 + a rectangle(0,0,20,10) + circle(10,5,2) sketch on
// ref-plane-xy; the rect region carries the circle as an inner hole.
// Returns the sketch feature id + the rect region id.
std::pair<std::string, std::string> make_rect_hole_fixture(
    DocumentManager& manager, DocumentState& document) {
  document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  document = manager.start_sketch_on_plane("ref-plane-xy");
  const std::string sketchId = document.feature_history.back().id;
  document = manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);
  document = manager.add_sketch_circle(10.0, 5.0, 2.0);
  const RegionIds regions = region_ids(document, sketchId);
  if (regions.polygon.empty()) {
    throw std::runtime_error("engrave fixture: rect region not found");
  }
  return {sketchId, regions.polygon};
}

// ── Test 1: registry ──────────────────────────────────────────────

bool test_registry() {
  const auto* generator = polysmith::core::find_cam_generator("engrave");
  return expect(generator != nullptr && generator->generate != nullptr,
                "registry: engrave generator registered");
}

// ── Test 2: basic rect + hole — on-line trace, hole loop ──────────
//
// Rect region (0,0)-(20,10) with a (10,5) r=2 circle hole, sketch at
// z=0, depth 0.5 → outer rect: rapid(z=25) → plunge(z=−0.5, 600) →
// 4 corner feeds (1200) → rapid(z=25); hole: rapid → plunge → one
// FeedArcCW (10,5) r=2 → rapid.  11 moves.

bool test_basic_rect_with_hole() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document;
  const auto [sketchId, rectId] = make_rect_hole_fixture(manager, document);
  const std::string opId =
      make_engrave_op(manager, document, sketchId, {rectId}, 0.5);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "basic: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;
  if (!expect(toolpath.moves.size() == 11, "basic: exactly eleven moves")) {
    std::cerr << "  moves: " << toolpath.moves.size() << "\n";
    return false;
  }
  const auto& m = toolpath.moves;
  if (!expect(m[0].kind == ToolpathMoveKind::Rapid && near(m[0].z, 25.0, 0.001),
              "basic: rapid to the trace start at retract")) {
    return false;
  }
  if (!expect(m[1].kind == ToolpathMoveKind::FeedLinear &&
                  near(m[1].z, -0.5, 0.001) &&
                  near(m[1].feedrate_mm_per_min, 600.0),
              "basic: plunge feed at the cut plane")) {
    return false;
  }
  // The four corner feeds — the start corner comes from the edge
  // chaining, so assert the SET of the corner coordinates.
  const std::array<std::pair<double, double>, 4> corners = {
      std::pair{0.0, 0.0}, std::pair{20.0, 0.0}, std::pair{20.0, 10.0},
      std::pair{0.0, 10.0}};
  for (size_t i = 2; i <= 5; ++i) {
    if (!expect(m[i].kind == ToolpathMoveKind::FeedLinear &&
                    near(m[i].z, -0.5, 0.001) &&
                    near(m[i].feedrate_mm_per_min, 1200.0),
                "basic: corner feed")) {
      return false;
    }
  }
  for (const auto& corner : corners) {
    bool found = false;
    for (size_t i = 2; i <= 5; ++i) {
      if (near(m[i].x, corner.first) && near(m[i].y, corner.second)) {
        found = true;
      }
    }
    if (!expect(found, "basic: all four corners traced")) {
      return false;
    }
  }
  if (!expect(m[6].kind == ToolpathMoveKind::Rapid && near(m[6].z, 25.0, 0.001),
              "basic: rapid out after the rect")) {
    return false;
  }
  if (!expect(m[7].kind == ToolpathMoveKind::Rapid && near(m[7].z, 25.0, 0.001),
              "basic: rapid over the hole")) {
    return false;
  }
  if (!expect(m[8].kind == ToolpathMoveKind::FeedLinear &&
                  near(m[8].z, -0.5, 0.001) && near(m[8].x, 12.0) &&
                  near(m[8].y, 5.0),
              "basic: hole plunge at the arc start")) {
    return false;
  }
  if (!expect(m[9].kind == ToolpathMoveKind::FeedArcCW && near(m[9].z, -0.5, 0.001) &&
                  near(m[9].x, 12.0) && near(m[9].y, 5.0) &&
                  near(m[9].i, -2.0) && near(m[9].j, 0.0),
              "basic: hole traced as one exact CW arc (10,5) r=2")) {
    std::cerr << "  [9] kind=" << static_cast<int>(m[9].kind) << " ("
              << m[9].x << ", " << m[9].y << ") i=" << m[9].i
              << " j=" << m[9].j << "\n";
    return false;
  }
  if (!expect(m[10].kind == ToolpathMoveKind::Rapid &&
                  near(m[10].z, 25.0, 0.001),
              "basic: rapid out after the hole")) {
    return false;
  }
  for (const auto& move : toolpath.moves) {
    if (move.laser_on) {
      return expect(false, "basic: laser_on false on every move");
    }
  }
  return true;
}

// ── Test 3: standalone circle region ──────────────────────────────

bool test_standalone_circle() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  document = manager.start_sketch_on_plane("ref-plane-xy");
  const std::string sketchId = document.feature_history.back().id;
  document = manager.add_sketch_circle(10.0, 5.0, 5.0);
  const RegionIds regions = region_ids(document, sketchId);
  if (!expect(regions.circles.size() == 1, "circle: region found")) {
    return false;
  }
  const std::string opId = make_engrave_op(
      manager, document, sketchId, {regions.circles[0]}, 0.5);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "circle: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;
  if (!expect(toolpath.moves.size() == 4, "circle: exactly four moves")) {
    return false;
  }
  const auto& m = toolpath.moves;
  // Synthesized CCW full circle: start = (15,5), center (10,5).
  return expect(m[0].kind == ToolpathMoveKind::Rapid &&
                    near(m[0].x, 15.0) && near(m[0].y, 5.0) &&
                    near(m[0].z, 25.0, 0.001) &&
                    m[1].kind == ToolpathMoveKind::FeedLinear &&
                    near(m[1].z, -0.5, 0.001) &&
                    m[2].kind == ToolpathMoveKind::FeedArcCCW &&
                    near(m[2].x, 15.0) && near(m[2].y, 5.0) &&
                    near(m[2].i, -5.0) && near(m[2].j, 0.0) &&
                    near(m[2].z, -0.5, 0.001) &&
                    m[3].kind == ToolpathMoveKind::Rapid,
                "circle: rapid-plunge-CCW-arc-rapid");
}

// ── Test 4: depth validation ──────────────────────────────────────

bool test_depth_validation() {
  for (const double badDepth : {0.0, -5.0}) {
    DocumentManager manager;
    manager.create_document();
    DocumentState document;
    const auto [sketchId, rectId] = make_rect_hole_fixture(manager, document);
    const std::string opId =
        make_engrave_op(manager, document, sketchId, {rectId}, 0.5);
    auto op = *find_op(document, opId);
    op.parameters.engrave = EngraveParameters{badDepth};
    document = manager.cam_operation_update(opId, op);

    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && !outcome.result.ok,
                "depth: non-positive depth fails loudly")) {
      return false;
    }
    if (!expect(outcome.result.error_message.find("depth must be "
                                                  "positive") !=
                    std::string::npos,
                "depth: error names the cause")) {
      std::cerr << "  error: " << outcome.result.error_message << "\n";
      return false;
    }
  }
  return true;
}

// ── Test 5: non-horizontal sketch plane rejected ──────────────────

bool test_non_horizontal_plane() {
  for (const char* planeId : {"ref-plane-yz", "ref-plane-xz"}) {
    DocumentManager manager;
    manager.create_document();
    DocumentState document;
    document = manager.start_sketch_on_plane(planeId);
    const std::string sketchId = document.feature_history.back().id;
    document = manager.add_sketch_circle(10.0, 5.0, 5.0);
    const RegionIds regions = region_ids(document, sketchId);
    if (!expect(regions.circles.size() == 1,
                "plane: circle region found")) {
      return false;
    }
    const std::string opId = make_engrave_op(
        manager, document, sketchId, {regions.circles[0]}, 0.5);

    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && !outcome.result.ok,
                "plane: vertical sketch plane fails loudly")) {
      return false;
    }
    if (!expect(outcome.result.error_message.find("horizontal") !=
                    std::string::npos,
                "plane: error demands a horizontal plane")) {
      std::cerr << "  error: " << outcome.result.error_message << "\n";
      return false;
    }
  }
  return true;
}

// ── Test 6: input kinds rejected ──────────────────────────────────

// 0-based edge-map index of a LINE edge (the slot-suite helper).
int find_top_edge_index(const CompiledBody& body, double targetZ,
                        bool minY) {
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edgeMap;
  TopExp::MapShapes(body.shape, TopAbs_EDGE, edgeMap);
  int bestIndex = -1;
  double bestCoord = 1e9;
  for (int i = 1; i <= edgeMap.Extent(); ++i) {
    const auto edge = TopoDS::Edge(edgeMap(i));
    try {
      BRepAdaptor_Curve curve(edge);
      if (curve.GetType() != GeomAbs_Line) {
        continue;
      }
    } catch (const std::exception&) {
      continue;
    }
    TopoDS_Vertex first;
    TopoDS_Vertex last;
    TopExp::Vertices(edge, first, last, /*CumOri=*/true);
    const gp_Pnt a = BRep_Tool::Pnt(first);
    const gp_Pnt b = BRep_Tool::Pnt(last);
    if (std::abs(a.Z() - targetZ) > 1e-4 ||
        std::abs(b.Z() - targetZ) > 1e-4) {
      continue;
    }
    const double coord = minY ? std::max(a.Y(), b.Y())
                              : std::max(a.X(), b.X());
    if (coord < bestCoord - 1e-9) {
      bestCoord = coord;
      bestIndex = i - 1;
    }
  }
  return bestIndex;
}

bool test_input_kinds_rejected() {
  // (a) An edge witness is not a sketch profile.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 10.0});
    const auto compiled = polysmith::core::compile_bodies(document);
    const auto& body = compiled.bodies[0];
    const int edgeIndex = find_top_edge_index(body, 10.0, /*minY=*/true);
    if (!expect(edgeIndex >= 0, "input: edge found")) {
      return false;
    }
    const auto ref = polysmith::core::capture_edge_reference(
        body.id, body.shape, edgeIndex, "edge");
    if (!expect(ref.has_value(), "input: edge captured")) {
      return false;
    }
    EdgeAttestation att;
    att.start_point = ref->startPoint;
    att.end_point = ref->endPoint;
    att.length = ref->length;
    att.tangent = ref->tangent;
    GeometryReference stored;
    stored.persistent_id = body.id + ":edge:" + std::to_string(edgeIndex);
    stored.attestation = att;

    CamSetup setup;
    setup.name = "Mill setup";
    setup.machine_type = "3_axis_mill";
    setup.retract_height = 25.0;
    document = manager.cam_setup_create(setup);
    ToolEntry tool;
    tool.name = "6mm endmill";
    tool.type = "endmill_flat";
    tool.diameter_mm = 6.0;
    document = manager.cam_tool_add(tool);
    CamOperation op;
    op.name = "Engrave 1";
    op.type = "engrave";
    op.tool_id = document.cam.tool_library[0].tool_id;
    op.parameters.engrave = EngraveParameters{0.5};
    op.geometry_references.machining_regions.push_back(stored);
    document = manager.cam_operation_add(op);
    const std::string opId = document.cam.operations.back().op_id;

    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && !outcome.result.ok,
                "input: edge region fails loudly")) {
      return false;
    }
    if (!expect(outcome.result.error_message.find("sketch profiles") !=
                    std::string::npos,
                "input: error demands sketch profiles")) {
      std::cerr << "  error: " << outcome.result.error_message << "\n";
      return false;
    }
  }
  // (b) Zero regions.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 10.0});
    CamSetup setup;
    setup.name = "Mill setup";
    setup.machine_type = "3_axis_mill";
    setup.retract_height = 25.0;
    document = manager.cam_setup_create(setup);
    ToolEntry tool;
    tool.name = "6mm endmill";
    tool.type = "endmill_flat";
    tool.diameter_mm = 6.0;
    document = manager.cam_tool_add(tool);
    CamOperation op;
    op.name = "Engrave 1";
    op.type = "engrave";
    op.tool_id = document.cam.tool_library[0].tool_id;
    op.parameters.engrave = EngraveParameters{0.5};
    document = manager.cam_operation_add(op);
    const std::string opId = document.cam.operations.back().op_id;

    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && !outcome.result.ok,
                "input: empty region set fails loudly")) {
      return false;
    }
    return expect(
        outcome.result.error_message.find("at least one selected sketch "
                                          "profile") != std::string::npos,
        "input: error demands at least one profile");
  }
}

// ── Test 7: tool-axis guard ───────────────────────────────────────

bool test_tool_axis_guard() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document;
  const auto [sketchId, rectId] = make_rect_hole_fixture(manager, document);
  const std::string opId =
      make_engrave_op(manager, document, sketchId, {rectId}, 0.5);
  auto op = *find_op(document, opId);
  op.parameters.tool_axis_mode = "3_plus_2";
  document = manager.cam_operation_update(opId, op);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && !outcome.result.ok,
              "axis: reserved rotary mode fails loudly")) {
    return false;
  }
  return expect(outcome.result.error_message.find("no generator supports") !=
                    std::string::npos,
                "axis: error names the unsupported mode");
}

// ── Test 8: walk direction ────────────────────────────────────────

bool test_walk_direction() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  document = manager.start_sketch_on_plane("ref-plane-xy");
  const std::string sketchId = document.feature_history.back().id;
  document = manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);
  const RegionIds regions = region_ids(document, sketchId);
  if (!expect(!regions.polygon.empty(), "walk: rect region found")) {
    return false;
  }

  const auto generate = [&](const std::string& direction) {
    const std::string opId = make_engrave_op(
        manager, document, sketchId, {regions.polygon}, 0.5,
        /*retractHeight=*/25.0, /*stockSize=*/std::nullopt, direction);
    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    return std::make_pair(opId, outcome);
  };

  const auto climb = generate("climb");
  const auto conventional = generate("conventional");
  const auto mixed = generate("mixed");
  if (!expect(climb.second.found && climb.second.result.ok &&
                  conventional.second.found &&
                  conventional.second.result.ok && mixed.second.found &&
                  mixed.second.result.ok,
              "walk: all three generate")) {
    return false;
  }
  const auto& cm = climb.second.result.toolpath.moves;
  const auto& vm = conventional.second.result.toolpath.moves;
  const auto& mm = mixed.second.result.toolpath.moves;
  if (!expect(cm.size() == 7 && vm.size() == 7 && mm.size() == 7,
              "walk: rect-only trace = seven moves each")) {
    return false;
  }
  // Conventional reverses the walk: its plunge lands where climb's
  // last feed ended, and vice versa.
  if (!expect(near(cm[1].x, vm[5].x) && near(cm[1].y, vm[5].y) &&
                  near(cm[5].x, vm[1].x) && near(cm[5].y, vm[1].y),
              "walk: conventional reverses the trace")) {
    return false;
  }
  bool warned = false;
  for (const auto& warning : mixed.second.result.warnings) {
    if (warning.find("treated as climb") != std::string::npos) {
      warned = true;
    }
  }
  if (!expect(warned, "walk: mixed warns about the climb fallback")) {
    return false;
  }
  // Mixed behaves as climb.
  return expect(near(cm[1].x, mm[1].x) && near(cm[1].y, mm[1].y) &&
                    near(cm[5].x, mm[5].x) && near(cm[5].y, mm[5].y),
                "walk: mixed matches the climb path");
}

// ── Test 9: multi-profile region order ────────────────────────────

// The cached region center of one named sketch region.
std::pair<double, double> region_center(const DocumentState& document,
                                        const std::string& sketch_feature_id,
                                        const std::string& profile_id) {
  for (const auto& feature : document.feature_history) {
    if (feature.id != sketch_feature_id ||
        !feature.sketch_parameters.has_value()) {
      continue;
    }
    for (const auto& region : feature.sketch_parameters->profiles) {
      if (region.id == profile_id) {
        return {region.center_x, region.center_y};
      }
    }
  }
  throw std::runtime_error("engrave fixture: region not found");
}

bool test_multi_profile_order() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  document = manager.start_sketch_on_plane("ref-plane-xy");
  const std::string sketchId = document.feature_history.back().id;
  document = manager.add_sketch_circle(5.0, 5.0, 2.0);
  document = manager.add_sketch_circle(15.0, 5.0, 2.0);
  const RegionIds regions = region_ids(document, sketchId);
  if (!expect(regions.circles.size() == 2,
              "multi: two circle regions found")) {
    return false;
  }
  // The sketch's profile vector is not creation-ordered, so the
  // contract under test is MACHINING-REGION order: loop i traces the
  // i-th captured region, whatever that region is.
  const auto [ax, ay] = region_center(document, sketchId, regions.circles[0]);
  const auto [bx, by] = region_center(document, sketchId, regions.circles[1]);
  const std::string opId = make_engrave_op(
      manager, document, sketchId, {regions.circles[0], regions.circles[1]},
      0.5);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "multi: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;
  if (!expect(toolpath.moves.size() == 8,
              "multi: eight moves (two loops × four)")) {
    std::cerr << "  moves: " << toolpath.moves.size() << "\n";
    return false;
  }
  const auto& m = toolpath.moves;
  // Loop A: the FIRST captured region — FeedArcCCW, start at
  // center + r along X, i=−r, j=0.
  if (!expect(m[2].kind == ToolpathMoveKind::FeedArcCCW &&
                  near(m[2].x, ax + 2.0) && near(m[2].y, ay) &&
                  near(m[2].i, -2.0) && near(m[2].j, 0.0),
              "multi: first loop traces the first captured region")) {
    for (size_t i = 0; i < toolpath.moves.size(); ++i) {
      std::cerr << "  [" << i << "] kind=" << static_cast<int>(m[i].kind)
                << " (" << m[i].x << ", " << m[i].y << ", " << m[i].z
                << ") i=" << m[i].i << " j=" << m[i].j << "\n";
    }
    return false;
  }
  // Loop B: the SECOND captured region.
  if (!expect(!near(ax, bx), "multi: the two captured regions differ") ||
      !expect(m[6].kind == ToolpathMoveKind::FeedArcCCW &&
                  near(m[6].x, bx + 2.0) && near(m[6].y, by) &&
                  near(m[6].i, -2.0) && near(m[6].j, 0.0),
              "multi: second loop traces the second captured region")) {
    return false;
  }
  return expect(near(m[0].z, 25.0, 0.001) && near(m[3].z, 25.0, 0.001) &&
                    near(m[4].z, 25.0, 0.001) && near(m[7].z, 25.0, 0.001),
                "multi: rapids at the retract plane");
}

// ── Test 10: guards ───────────────────────────────────────────────

bool test_guards() {
  // (a) Sketch ON the box top face: the raised plane maps the cut to
  // z=9.5 (depth 0.5 below z=10); retract 5 sits below it.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 10.0});
    const auto top = top_face(document);
    if (!expect(top.has_value(), "guards: top face found")) {
      return false;
    }
    document = manager.start_sketch_on_face(top->face_id, frame_of(*top));
    const std::string sketchId = document.feature_history.back().id;
    document = manager.add_sketch_circle(10.0, 5.0, 2.0);
    const RegionIds regions = region_ids(document, sketchId);
    const std::string opId = make_engrave_op(
        manager, document, sketchId, {regions.circles[0]}, 0.5,
        /*retractHeight=*/5.0);

    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && outcome.result.ok,
                "guards: raised-plane generation succeeds")) {
      std::cerr << "  error: " << outcome.result.error_message << "\n";
      return false;
    }
    for (const auto& move : outcome.result.toolpath.moves) {
      if (move.kind == ToolpathMoveKind::FeedLinear ||
          move.kind == ToolpathMoveKind::FeedArcCCW ||
          move.kind == ToolpathMoveKind::FeedArcCW) {
        if (!near(move.z, 9.5, 0.001)) {
          std::cerr << "  feed at z " << move.z << "\n";
          return expect(false,
                        "guards: every feed at the raised cut plane 9.5");
        }
      }
    }
    bool warned = false;
    for (const auto& warning : outcome.result.warnings) {
      if (warning.find("below the cut plane") != std::string::npos) {
        warned = true;
      }
    }
    if (!expect(warned, "guards: retract below the cut plane warns")) {
      return false;
    }
  }
  // (b) Origin-plane sketch + stock 20×20×10 (centered on the box:
  // top 10, bottom 0): cutZ −0.5 pierces the stock bottom; retract 5
  // sits below the stock top.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document;
    const auto [sketchId, rectId] = make_rect_hole_fixture(manager, document);
    const std::string opId = make_engrave_op(
        manager, document, sketchId, {rectId}, 0.5,
        /*retractHeight=*/5.0, std::array<double, 3>{20.0, 20.0, 10.0});
    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && outcome.result.ok,
                "guards: stock fixture generates")) {
      return false;
    }
    bool topWarned = false;
    bool bottomWarned = false;
    for (const auto& warning : outcome.result.warnings) {
      if (warning.find("below the stock top") != std::string::npos) {
        topWarned = true;
      }
      if (warning.find("below the stock bottom") != std::string::npos) {
        bottomWarned = true;
      }
    }
    return expect(topWarned && bottomWarned,
                  "guards: stock-top and stock-bottom warnings");
  }
}

// ── Test 11: broken profile attestation ───────────────────────────

bool test_broken_profile_attestation() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document;
  const auto [sketchId, rectId] = make_rect_hole_fixture(manager, document);
  const std::string opId =
      make_engrave_op(manager, document, sketchId, {rectId}, 0.5);

  // Corrupt the profile witness: centroid AND area far from every
  // region (the centroid alone still clears the 0.7 threshold via
  // boundary kinds 0.35 + area 0.3 + holes 0.1).  The refresh runs
  // inside the update's bump.
  document = manager.get_document().value();
  auto modified = *find_op(document, opId);
  bool corrupted = false;
  for (auto& ref : modified.geometry_references.machining_regions) {
    if (std::holds_alternative<SketchProfileAttestation>(
            ref.attestation)) {
      auto& att = std::get<SketchProfileAttestation>(ref.attestation);
      att.center_x = 1000.0;
      att.center_y = 1000.0;
      att.area = 10000.0;
      att.min_x = 999.0;
      att.min_y = 999.0;
      att.max_x = 1001.0;
      att.max_y = 1001.0;
      corrupted = true;
    }
  }
  if (!expect(corrupted, "broken: witness corrupted")) {
    return false;
  }
  document = manager.cam_operation_update(opId, modified);
  const auto* op = find_op(document, opId);
  if (!expect(op != nullptr && op->status == "error",
              "broken: refresh degrades the op")) {
    std::cerr << "  status: "
              << (op != nullptr ? op->status : std::string("<gone>"))
              << "\n";
    return false;
  }
  if (!expect(op->status_message.find("was not found") != std::string::npos,
              "broken: degradation carries the dependency message")) {
    std::cerr << "  message: " << op->status_message << "\n";
    return false;
  }
  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && !outcome.result.ok,
              "broken: generation fails loudly")) {
    return false;
  }
  return expect(outcome.result.error_message.find("was not found") !=
                    std::string::npos,
                "broken: generate carries the dependency message");
}

// ── Test 12: payload round-trip ───────────────────────────────────

bool test_payload_roundtrip() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document;
  const auto [sketchId, rectId] = make_rect_hole_fixture(manager, document);
  const std::string opId =
      make_engrave_op(manager, document, sketchId, {rectId}, 0.5);
  auto op = *find_op(document, opId);
  op.parameters.engrave = EngraveParameters{3.5};

  const polysmith::protocol::json payload =
      polysmith::protocol::to_payload(op);
  const CamOperation restored =
      polysmith::protocol::cam_operation_from_payload(payload);

  if (!expect(restored.type == "engrave", "roundtrip: op type preserved")) {
    return false;
  }
  if (!expect(restored.parameters.engrave.has_value() &&
                  near(restored.parameters.engrave.value().depth_mm, 3.5),
              "roundtrip: engrave depth preserved")) {
    return false;
  }
  if (!expect(restored.geometry_references.machining_regions.size() == 1,
              "roundtrip: profile region survives")) {
    return false;
  }
  const auto& stored = restored.geometry_references.machining_regions[0];
  if (!expect(std::holds_alternative<SketchProfileAttestation>(
                  stored.attestation),
              "roundtrip: attestation kind preserved")) {
    return false;
  }
  const auto& att = std::get<SketchProfileAttestation>(stored.attestation);
  const auto& original = std::get<SketchProfileAttestation>(
      op.geometry_references.machining_regions[0].attestation);
  if (!expect(att.sketch_feature_id == original.sketch_feature_id &&
                  att.area > 0.0 && near(att.area, original.area),
              "roundtrip: profile witness preserved")) {
    return false;
  }
  // An omitted engrave block falls back to the struct defaults.
  const auto defaulted = polysmith::protocol::engrave_parameters_from_payload(
      polysmith::protocol::json{});
  return expect(near(defaulted.depth_mm, 0.5),
                "roundtrip: omitted engrave block defaults to depth 0.5");
}

}  // namespace

int main() {
  // The app registers builtin generators from CadCoreApp::run(); the
  // test process must do it itself (cam_generators_test pattern).
  polysmith::core::register_builtin_cam_generators();
  bool allPassed = true;

  std::cout << "cam_engrave_test\n";
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
  run("Test 1: registry", test_registry);
  run("Test 2: basic rect + hole", test_basic_rect_with_hole);
  run("Test 3: standalone circle", test_standalone_circle);
  run("Test 4: depth validation", test_depth_validation);
  run("Test 5: non-horizontal plane", test_non_horizontal_plane);
  run("Test 6: input kinds rejected", test_input_kinds_rejected);
  run("Test 7: tool-axis guard", test_tool_axis_guard);
  run("Test 8: walk direction", test_walk_direction);
  run("Test 9: multi-profile order", test_multi_profile_order);
  run("Test 10: guards", test_guards);
  run("Test 11: broken profile attestation",
      test_broken_profile_attestation);
  run("Test 12: payload round-trip", test_payload_roundtrip);

  if (allPassed) {
    std::cout << "cam_engrave_test passed\n";
    return 0;
  }
  return 1;
}
