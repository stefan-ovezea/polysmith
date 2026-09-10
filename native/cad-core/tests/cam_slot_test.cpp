// Open Slot generator test — the edge-driven slot operation
// end-to-end.
//
// Edge witness → live-edge re-open → straight-line guard → adjacent
// top-face find (steepest upward normal) → inward direction (face COM
// probe) → tool-centre path at edge + radius × inward → climb/
// conventional walk (material left) → stepdown levels → cut-major
// emission → guards → error paths → dependency degradation → payload
// round-trip.

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
#include "core/document/document.h"
#include "core/geometry/body_compiler.h"
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
using polysmith::core::GeometryReference;
using polysmith::core::SlotParameters;
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

bool near(double a, double b, double tolerance = 1e-6) {
  return std::abs(a - b) < tolerance;
}

bool near3(const std::array<double, 3>& a, const std::array<double, 3>& b,
           double tolerance = 1e-6) {
  return near(a[0], b[0], tolerance) && near(a[1], b[1], tolerance) &&
         near(a[2], b[2], tolerance);
}

// 0-based edge-map index of a LINE edge whose BOTH endpoints sit at
// z ≈ targetZ; among the candidates, the one with the smallest MAX-Y
// (minY=true — the y=0 top edge: max y 0 beats x=0's 20, which also
// has min y 0) or smallest MAX-X (the x=0 top edge); -1 when none.
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

// 0-based edge-map index of a FULL-CIRCLE edge (both endpoints
// collapse — the OCCT full-circle marker); -1 when none.
int find_full_circle_edge_index(const CompiledBody& body) {
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edgeMap;
  TopExp::MapShapes(body.shape, TopAbs_EDGE, edgeMap);
  for (int i = 1; i <= edgeMap.Extent(); ++i) {
    const auto edge = TopoDS::Edge(edgeMap(i));
    try {
      BRepAdaptor_Curve curve(edge);
      if (curve.GetType() != GeomAbs_Circle) {
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
    const double dx = b.X() - a.X();
    const double dy = b.Y() - a.Y();
    const double dz = b.Z() - a.Z();
    if (std::sqrt(dx * dx + dy * dy + dz * dz) < 1e-6) {
      return i - 1;
    }
  }
  return -1;
}

// Circle-profile ids (exact circles or circle-sourced) of the named
// sketch — the cut input set (the cam_generators_test helper).
std::vector<std::string> circle_profile_ids(
    const DocumentState& document, const std::string& sketch_feature_id) {
  for (const auto& feature : document.feature_history) {
    if (feature.id != sketch_feature_id ||
        !feature.sketch_parameters.has_value()) {
      continue;
    }
    std::vector<std::string> ids;
    for (const auto& region : feature.sketch_parameters->profiles) {
      if (region.kind == "circle" || region.source_circle_id.has_value()) {
        ids.push_back(region.id);
      }
    }
    return ids;
  }
  return {};
}

// Edge witness reference for (body, edgeIndex) — the capture pattern
// shared with the drilling tests (line edges: endpoint/length/tangent
// witness only; circle fields stay unset).
GeometryReference top_edge_ref(const CompiledBody& body, int edgeIndex) {
  const auto ref = polysmith::core::capture_edge_reference(
      body.id, body.shape, edgeIndex, "slot edge");
  if (!ref.has_value()) {
    throw std::runtime_error("slot fixture: edge capture failed");
  }
  EdgeAttestation att;
  att.start_point = ref->startPoint;
  att.end_point = ref->endPoint;
  att.length = ref->length;
  att.tangent = ref->tangent;
  GeometryReference stored;
  stored.persistent_id = body.id + ":edge:" + std::to_string(edgeIndex);
  stored.attestation = att;
  return stored;
}

// Full witness reference for (body, edgeIndex) INCLUDING the circle
// fields (the rim_edge_ref drilling pattern) — resolution only accepts
// circle witnesses on circle edges.
GeometryReference rim_edge_ref(const CompiledBody& body, int edgeIndex) {
  const auto ref = polysmith::core::capture_edge_reference(
      body.id, body.shape, edgeIndex, "hole rim");
  if (!ref.has_value()) {
    throw std::runtime_error("slot fixture: rim capture failed");
  }
  EdgeAttestation att;
  att.start_point = ref->startPoint;
  att.end_point = ref->endPoint;
  att.length = ref->length;
  att.tangent = ref->tangent;
  att.center = ref->center;
  att.axis = ref->axis;
  att.radius = ref->radius;
  GeometryReference stored;
  stored.persistent_id = body.id + ":edge:" + std::to_string(edgeIndex);
  stored.attestation = att;
  return stored;
}

const CamOperation* find_op(const DocumentState& document,
                            const std::string& opId) {
  for (const auto& op : document.cam.operations) {
    if (op.op_id == opId) {
      return &op;
    }
  }
  return nullptr;
}

// Mill setup + 6 mm endmill + slot op on the given edge references.
// Returns the op id.
std::string make_slot_op(
    DocumentManager& manager, DocumentState& document,
    const std::vector<GeometryReference>& refs, double depth,
    double retractHeight = 20.0,
    const std::optional<std::array<double, 3>>& stockSize = std::nullopt,
    const std::string& cuttingDirection = "climb",
    const std::optional<double>& stepdown = std::nullopt) {
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
  tool.default_stepover_percent = 50.0;
  document = manager.cam_tool_add(tool);

  CamOperation op;
  op.name = "Slot 1";
  op.type = "slot";
  op.tool_id = document.cam.tool_library[0].tool_id;
  op.parameters.feedrate_mm_per_min = 1200.0;
  op.parameters.plunge_feedrate_mm_per_min = 600.0;
  op.parameters.cutting_direction = cuttingDirection;
  op.parameters.slot = SlotParameters{depth};
  if (stepdown.has_value()) {
    op.parameters.stepdown_mm = stepdown;
  }
  op.geometry_references.machining_regions = refs;
  document = manager.cam_operation_add(op);
  return document.cam.operations.back().op_id;
}

// Box 20×20×10 fixture + the y=0 top edge reference.
GeometryReference make_y0_top_edge_ref(DocumentManager& manager,
                                       DocumentState& document,
                                       std::string& opIdOut) {
  document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const auto compiled = polysmith::core::compile_bodies(document);
  const auto& body = compiled.bodies[0];
  const int edgeIndex = find_top_edge_index(body, 10.0, /*minY=*/true);
  if (edgeIndex < 0) {
    throw std::runtime_error("slot fixture: y=0 top edge not found");
  }
  const GeometryReference ref = top_edge_ref(body, edgeIndex);
  opIdOut = make_slot_op(manager, document, {ref}, 5.0);
  return ref;
}

// ── Test 1: registry ──────────────────────────────────────────────

bool test_registry() {
  const auto* generator = polysmith::core::find_cam_generator("slot");
  return expect(generator != nullptr && generator->generate != nullptr,
                "registry: slot generator registered");
}

// ── Test 2: basic open slot — inward path, climb walk, emission ────
//
// y=0 top edge (0,0,10)-(20,0,10), 6 mm tool (r=3), depth 5, climb:
// inward = (0,1) (the box interior), walk W = (1,0), path y=3 from
// x=0 to x=20, single pass at z=5.

bool test_basic_slot() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document;
  std::string opId;
  make_y0_top_edge_ref(manager, document, opId);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "basic: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  if (!expect(toolpath.moves.size() == 4,
              "basic: exactly four moves")) {
    return false;
  }
  const auto& m = toolpath.moves;
  if (!expect(m[0].kind == ToolpathMoveKind::Rapid && near(m[0].x, 0.0) &&
                  near(m[0].y, 3.0) && near(m[0].z, 20.0, 0.001),
              "basic: rapid to the path start at retract")) {
    return false;
  }
  if (!expect(m[1].kind == ToolpathMoveKind::FeedLinear &&
                  near(m[1].x, 0.0) && near(m[1].y, 3.0) &&
                  near(m[1].z, 5.0, 0.001) &&
                  near(m[1].feedrate_mm_per_min, 600.0),
              "basic: plunge feed at the start")) {
    return false;
  }
  if (!expect(m[2].kind == ToolpathMoveKind::FeedLinear &&
                  near(m[2].x, 20.0) && near(m[2].y, 3.0) &&
                  near(m[2].z, 5.0, 0.001) &&
                  near(m[2].feedrate_mm_per_min, 1200.0),
              "basic: cut feed to the end (climb walks +X)")) {
    return false;
  }
  if (!expect(m[3].kind == ToolpathMoveKind::Rapid && near(m[3].x, 20.0) &&
                  near(m[3].y, 3.0) && near(m[3].z, 20.0, 0.001),
              "basic: rapid out at retract")) {
    return false;
  }
  for (const auto& move : toolpath.moves) {
    if (move.laser_on) {
      return expect(false, "basic: laser_on false on every move");
    }
  }
  return true;
}

// ── Test 3: climb/conventional walk flip + mixed warning ───────────

bool test_walk_flip() {
  // Climb (default) walks +X — already pinned in Test 2; conventional
  // must walk the same path backwards.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document;
    std::string opId;
    make_y0_top_edge_ref(manager, document, opId);
    auto op = *find_op(document, opId);
    op.parameters.cutting_direction = "conventional";
    document = manager.cam_operation_update(opId, op);

    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && outcome.result.ok,
                "conventional: generation succeeds")) {
      return false;
    }
    const auto& m = outcome.result.toolpath.moves;
    if (!expect(m.size() == 4 && near(m[1].x, 20.0) && near(m[1].y, 3.0) &&
                    near(m[2].x, 0.0) && near(m[2].y, 3.0),
                "conventional: walk reversed (20,3)→(0,3)")) {
      return false;
    }
  }
  // "mixed" warns and behaves as climb.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document;
    std::string opId;
    make_y0_top_edge_ref(manager, document, opId);
    auto op = *find_op(document, opId);
    op.parameters.cutting_direction = "mixed";
    document = manager.cam_operation_update(opId, op);

    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && outcome.result.ok,
                "mixed: generation succeeds")) {
      return false;
    }
    bool warned = false;
    for (const auto& warning : outcome.result.warnings) {
      if (warning.find("treated as climb") != std::string::npos) {
        warned = true;
      }
    }
    if (!expect(warned, "mixed: warns about the climb fallback")) {
      return false;
    }
    const auto& m = outcome.result.toolpath.moves;
    return expect(m.size() == 4 && near(m[1].x, 0.0) && near(m[2].x, 20.0),
                  "mixed: climb path (0,3)→(20,3)");
  }
}

// ── Test 4: stepdown multi-pass ───────────────────────────────────
//
// Stock 20×20×10 is flush with the model bbox (stock tops are
// CENTERED on the bbox — the pocket fixtures prove it): stock top 10,
// stepdown 2, floor 5 → levels {8, 6, 5}.

bool test_stepdown_multipass() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const auto compiled = polysmith::core::compile_bodies(document);
  const auto& body = compiled.bodies[0];
  const int edgeIndex = find_top_edge_index(body, 10.0, /*minY=*/true);
  if (!expect(edgeIndex >= 0, "multipass: edge found")) {
    return false;
  }
  const std::string opId = make_slot_op(
      manager, document, {top_edge_ref(body, edgeIndex)}, 5.0,
      /*retractHeight=*/20.0, std::array<double, 3>{20.0, 20.0, 10.0},
      /*cuttingDirection=*/"climb", /*stepdown=*/2.0);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "multipass: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;
  if (!expect(toolpath.moves.size() == 12,
              "multipass: three passes × four moves")) {
    std::cerr << "  moves: " << toolpath.moves.size() << "\n";
    return false;
  }
  // Pass order: z=8, z=6, z=5 — each rapid-plunge-feed-rapid.
  const std::array<double, 3> expectedZs = {8.0, 6.0, 5.0};
  for (size_t pass = 0; pass < 3; ++pass) {
    const size_t base = pass * 4;
    const auto& m = toolpath.moves;
    if (!expect(m[base].kind == ToolpathMoveKind::Rapid &&
                    near(m[base].z, 20.0, 0.001) &&
                    m[base + 1].kind == ToolpathMoveKind::FeedLinear &&
                    near(m[base + 1].z, expectedZs[pass], 0.001) &&
                    m[base + 2].kind == ToolpathMoveKind::FeedLinear &&
                    near(m[base + 2].z, expectedZs[pass], 0.001) &&
                    m[base + 3].kind == ToolpathMoveKind::Rapid &&
                    near(m[base + 3].z, 20.0, 0.001),
                "multipass: pass z order 8, 6, 5")) {
      std::cerr << "  pass " << pass << " mismatch\n";
      return false;
    }
  }
  // The path itself is the same at every level.
  return expect(near(toolpath.moves[1].y, 3.0) &&
                    near(toolpath.moves[5].y, 3.0) &&
                    near(toolpath.moves[9].y, 3.0),
                "multipass: path y=3 at every level");
}

// ── Test 5: multi-edge — cut-major region order ───────────────────

bool test_multi_edge() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const auto compiled = polysmith::core::compile_bodies(document);
  const auto& body = compiled.bodies[0];
  const int y0Index = find_top_edge_index(body, 10.0, /*minY=*/true);
  const int x0Index = find_top_edge_index(body, 10.0, /*minY=*/false);
  if (!expect(y0Index >= 0 && x0Index >= 0,
              "multi-edge: both edges found")) {
    return false;
  }
  const std::string opId = make_slot_op(
      manager, document,
      {top_edge_ref(body, y0Index), top_edge_ref(body, x0Index)}, 5.0);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "multi-edge: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;
  if (!expect(toolpath.moves.size() == 8, "multi-edge: eight moves")) {
    return false;
  }
  const auto& m = toolpath.moves;
  // Edge A (y=0): path y=3, x 0→20 first.
  if (!expect(m[0].kind == ToolpathMoveKind::Rapid && near(m[0].x, 0.0) &&
                  near(m[0].y, 3.0) && near(m[2].x, 20.0) &&
                  near(m[2].y, 3.0),
              "multi-edge: edge A first (y=3, x 0→20)")) {
    return false;
  }
  // Edge B (x=0): inward (1,0) → climb W = (0,−1): path x=3, y 20→0.
  if (!expect(m[4].kind == ToolpathMoveKind::Rapid && near(m[4].x, 3.0) &&
                  near(m[4].y, 20.0) &&
                  m[5].kind == ToolpathMoveKind::FeedLinear &&
                  near(m[6].x, 3.0) && near(m[6].y, 0.0),
              "multi-edge: edge B second (x=3, y 20→0)")) {
    for (size_t i = 4; i < 8; ++i) {
      std::cerr << "  [" << i << "] kind=" << static_cast<int>(m[i].kind)
                << " (" << m[i].x << ", " << m[i].y << ", " << m[i].z
                << ")\n";
    }
    return false;
  }
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::FeedLinear &&
        !near(move.z, 5.0, 0.001)) {
      return expect(false, "multi-edge: all feeds at the floor");
    }
  }
  return true;
}

// ── Test 6: depth validation ──────────────────────────────────────

bool test_depth_validation() {
  for (const double badDepth : {0.0, -5.0}) {
    DocumentManager manager;
    manager.create_document();
    DocumentState document;
    std::string opId;
    make_y0_top_edge_ref(manager, document, opId);
    auto op = *find_op(document, opId);
    op.parameters.slot = SlotParameters{badDepth};
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

// ── Test 7: circle edge rejected ──────────────────────────────────

bool test_circle_edge_rejected() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const std::string boxId = document.feature_history.back().id;
  // A through-hole cut leaves full-circle rims (capture accepts them;
  // the slot generator rejects them — a rim is not an open side).
  document = manager.start_sketch_on_plane("ref-plane-xy");
  const std::string sketchId = document.feature_history.back().id;
  document = manager.add_sketch_circle(10.0, 10.0, 4.0);
  const auto profileIds = circle_profile_ids(document, sketchId);
  document = manager.extrude_profiles(profileIds, 10.0, "cut", boxId);
  const auto compiled = polysmith::core::compile_bodies(document);
  const CompiledBody& body = compiled.bodies[0];
  const int rimIndex = find_full_circle_edge_index(body);
  if (!expect(rimIndex >= 0, "circle: rim edge found")) {
    return false;
  }
  const std::string opId =
      make_slot_op(manager, document, {rim_edge_ref(body, rimIndex)}, 5.0);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && !outcome.result.ok,
              "circle: rim edge fails loudly")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  return expect(outcome.result.error_message.find("straight line edges") !=
                    std::string::npos,
                "circle: error demands straight line edges");
}

// ── Test 8: no horizontal adjacent face ───────────────────────────
//
// The y=0 edge on the BOTTOM face (z=0): its adjacent faces are the
// bottom cap (orientation-corrected normal −Z) and two vertical walls
// — nothing upward.

bool test_no_horizontal_adjacent_face() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const auto compiled = polysmith::core::compile_bodies(document);
  const auto& body = compiled.bodies[0];
  const int edgeIndex = find_top_edge_index(body, 0.0, /*minY=*/true);
  if (!expect(edgeIndex >= 0, "bottom: edge found")) {
    return false;
  }
  const std::string opId =
      make_slot_op(manager, document, {top_edge_ref(body, edgeIndex)}, 5.0);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && !outcome.result.ok,
              "bottom: no machining face fails loudly")) {
    return false;
  }
  return expect(outcome.result.error_message.find("horizontal face "
                                                  "adjacent") !=
                    std::string::npos,
                "bottom: error names the cause");
}

// ── Test 9: broken edge attestation fails generate AND refresh ────

bool test_broken_edge_attestation() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document;
  std::string opId;
  make_y0_top_edge_ref(manager, document, opId);

  // Corrupt the edge witness: endpoints far from every edge.  The
  // refresh runs inside the update's bump (the ambiguous-reference
  // refresh test pattern) — degradation must already be visible.
  document = manager.get_document().value();
  auto modified = *find_op(document, opId);
  bool corrupted = false;
  for (auto& ref : modified.geometry_references.machining_regions) {
    if (std::holds_alternative<EdgeAttestation>(ref.attestation)) {
      auto& att = std::get<EdgeAttestation>(ref.attestation);
      att.start_point = {100.0, 100.0, 100.0};
      att.end_point = {101.0, 100.0, 100.0};
      att.length = 1.0;
      corrupted = true;
    }
  }
  if (!expect(corrupted, "broken edge: witness corrupted")) {
    return false;
  }
  document = manager.cam_operation_update(opId, modified);
  const auto* op = find_op(document, opId);
  if (!expect(op != nullptr && op->status == "error",
              "broken edge: refresh degrades the op")) {
    std::cerr << "  status: "
              << (op != nullptr ? op->status : std::string("<gone>"))
              << "\n";
    return false;
  }
  if (!expect(op->status_message.find("was not found") != std::string::npos,
              "broken edge: degradation carries the dependency message")) {
    std::cerr << "  message: " << op->status_message << "\n";
    return false;
  }

  // Generation fails the same way — never cut without a live edge.
  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && !outcome.result.ok,
              "broken edge: generation fails loudly")) {
    return false;
  }
  return expect(outcome.result.error_message.find("was not found") !=
                    std::string::npos,
                "broken edge: generate carries the dependency message");
}

// ── Test 10: retract guards ───────────────────────────────────────

bool test_retract_guards() {
  // Guard 1: retract below the face.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 10.0});
    const auto compiled = polysmith::core::compile_bodies(document);
    const auto& body = compiled.bodies[0];
    const int edgeIndex = find_top_edge_index(body, 10.0, /*minY=*/true);
    const std::string opId = make_slot_op(
        manager, document, {top_edge_ref(body, edgeIndex)}, 5.0,
        /*retractHeight=*/5.0);
    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    bool warned = false;
    for (const auto& warning : outcome.result.warnings) {
      if (warning.find("below the face height") != std::string::npos) {
        warned = true;
      }
    }
    if (!expect(warned, "guards: retract below the face warns")) {
      return false;
    }
  }
  // Guard 2: multi-pass with retract below the stock top (stock
  // 20×20×30 → centered top 20; retract 5, stepdown 2).
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 10.0});
    const auto compiled = polysmith::core::compile_bodies(document);
    const auto& body = compiled.bodies[0];
    const int edgeIndex = find_top_edge_index(body, 10.0, /*minY=*/true);
    const std::string opId = make_slot_op(
        manager, document, {top_edge_ref(body, edgeIndex)}, 5.0,
        /*retractHeight=*/5.0, std::array<double, 3>{20.0, 20.0, 30.0},
        /*cuttingDirection=*/"climb", /*stepdown=*/2.0);
    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    bool warned = false;
    for (const auto& warning : outcome.result.warnings) {
      if (warning.find("below the stock top") != std::string::npos) {
        warned = true;
      }
    }
    return expect(warned, "guards: retract below the stock top warns");
  }
}

// ── Test 11: payload round-trip ───────────────────────────────────

bool test_payload_roundtrip() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document;
  std::string opId;
  make_y0_top_edge_ref(manager, document, opId);
  auto op = *find_op(document, opId);
  op.parameters.slot = SlotParameters{3.5};

  const polysmith::protocol::json payload =
      polysmith::protocol::to_payload(op);
  const CamOperation restored =
      polysmith::protocol::cam_operation_from_payload(payload);

  if (!expect(restored.type == "slot", "roundtrip: op type preserved")) {
    return false;
  }
  if (!expect(restored.parameters.slot.has_value() &&
                  near(restored.parameters.slot.value().depth_mm, 3.5),
              "roundtrip: slot depth preserved")) {
    return false;
  }
  if (!expect(restored.geometry_references.machining_regions.size() == 1,
              "roundtrip: edge region survives")) {
    return false;
  }
  const auto& stored = restored.geometry_references.machining_regions[0];
  if (!expect(std::holds_alternative<EdgeAttestation>(stored.attestation),
              "roundtrip: attestation kind preserved")) {
    return false;
  }
  const auto& att = std::get<EdgeAttestation>(stored.attestation);
  const auto& original = std::get<EdgeAttestation>(
      op.geometry_references.machining_regions[0].attestation);
  if (!expect(att.length > 0.0 &&
                  near3(att.start_point, original.start_point) &&
                  near3(att.end_point, original.end_point) &&
                  near3(att.tangent, original.tangent),
              "roundtrip: edge witness preserved")) {
    return false;
  }
  // An omitted slot block falls back to the struct defaults.
  const auto defaulted = polysmith::protocol::slot_parameters_from_payload(
      polysmith::protocol::json{});
  return expect(near(defaulted.depth_mm, 5.0),
                "roundtrip: omitted slot block defaults to depth 5");
}

// ── Test 12: material-side pin — the path stays on the face side ──

bool test_material_side_pin() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document;
  std::string opId;
  make_y0_top_edge_ref(manager, document, opId);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "pin: generation succeeds")) {
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;
  // inward = (0,1) for the y=0 edge: every feed point is the edge
  // point + 3·inward (y = 3), and the feed direction is perpendicular
  // to the inward direction (parallel to the edge) — the material
  // side never flips.
  for (const auto& move : toolpath.moves) {
    if (move.kind != ToolpathMoveKind::FeedLinear) {
      continue;
    }
    if (!near(move.y, 3.0) || move.x < -1e-6 || move.x > 20.0 + 1e-6) {
      std::cerr << "  feed at (" << move.x << ", " << move.y << ")\n";
      return expect(false,
                    "pin: every feed point on the inward-offset line");
    }
  }
  const auto& m = toolpath.moves;
  const double dirDotInward =
      (m[2].x - m[1].x) * 0.0 + (m[2].y - m[1].y) * 1.0;
  return expect(near(dirDotInward, 0.0),
                "pin: the cut direction is parallel to the edge");
}

}  // namespace

int main() {
  // The app registers builtin generators from CadCoreApp::run(); the
  // test process must do it itself (cam_generators_test pattern).
  polysmith::core::register_builtin_cam_generators();
  bool allPassed = true;

  std::cout << "cam_slot_test\n";
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
  run("Test 2: basic open slot", test_basic_slot);
  run("Test 3: walk flip + mixed warning", test_walk_flip);
  run("Test 4: stepdown multi-pass", test_stepdown_multipass);
  run("Test 5: multi-edge ordering", test_multi_edge);
  run("Test 6: depth validation", test_depth_validation);
  run("Test 7: circle edge rejected", test_circle_edge_rejected);
  run("Test 8: no horizontal adjacent face",
      test_no_horizontal_adjacent_face);
  run("Test 9: broken edge attestation", test_broken_edge_attestation);
  run("Test 10: retract guards", test_retract_guards);
  run("Test 11: payload round-trip", test_payload_roundtrip);
  run("Test 12: material-side pin", test_material_side_pin);

  if (allPassed) {
    std::cout << "cam_slot_test passed\n";
    return 0;
  }
  return 1;
}
