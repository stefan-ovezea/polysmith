// Adaptive Clearing generator test — the contour-parallel spiral
// operation end-to-end.
//
// Face witness → outer-wire selection (largest |area| wire) → rEff
// inset (tool radius + stock allowance) → concentric inward spiral
// (spacing = max(diameter × stepover%, 0.1), collapse ends the
// family) → inner-wire boss/hole classification → island growth →
// per-level island filter + island-top level insertion → climb
// directions (outer CW / island CCW) → clipping (no entry into grown
// avoidances or the wall band) → stepdown multipass → warnings →
// error paths → dependency degradation → payload round-trip.

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "core/cam/cam_generate.h"
#include "core/cam/cam_generator.h"
#include "core/cam/cam_operation.h"
#include "core/document/document.h"
#include "core/geometry/body_compiler.h"
#include "protocol/serialization.h"

#include <BRepAdaptor_Surface.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

namespace {

using polysmith::core::CamOperation;
using polysmith::core::CamOperationParameters;
using polysmith::core::CamSetup;
using polysmith::core::CompiledBody;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::FaceAttestation;
using polysmith::core::GeometryReference;
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

// Distance from point (px,py) to segment (ax,ay)-(bx,by).
double point_segment_dist(double px, double py, double ax, double ay,
                          double bx, double by) {
  const double dx = bx - ax;
  const double dy = by - ay;
  const double len2 = dx * dx + dy * dy;
  double t = len2 > 0 ? ((px - ax) * dx + (py - ay) * dy) / len2 : 0.0;
  t = std::max(0.0, std::min(1.0, t));
  return std::hypot(px - (ax + t * dx), py - (ay + t * dy));
}

// Distance from point (x,y) to the axis-aligned rectangle [x0,x1]×[y0,y1]
// (0 when inside).
double rect_distance(double x, double y, double x0, double y0, double x1,
                     double y1) {
  const double dx = std::max(0.0, std::max(x0 - x, x - x1));
  const double dy = std::max(0.0, std::max(y0 - y, y - y1));
  return std::hypot(dx, dy);
}

// 0-based face-map index of an upward face of `body` centered within
// `tolerance` of `targetZ`; -1 when none.
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
// ~5° of the XY plane); -1 when none.
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
// shared with the pocket tests.
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

// Mill setup + 6 mm endmill + adaptive_clearing op on an upward face of
// body 0, with the given island references.  floorZ >= 0 targets the
// upward face at that height (a boss on the floor would otherwise win
// the "highest face" scan); floorZ < 0 picks the highest upward face.
// withFace=false creates the op with no machining region (the error
// path); faceIndexOverride selects a specific face (e.g. a vertical
// one).  Returns the op id.
std::string make_adaptive_op(
    DocumentManager& manager, DocumentState& document,
    const std::vector<GeometryReference>& islands,
    double retractHeight = 20.0,
    const std::optional<std::array<double, 3>>& stockSize = std::nullopt,
    double floorZ = -1.0,
    const std::optional<std::array<double, 3>>& wcsOrigin = std::nullopt,
    bool withFace = true, int faceIndexOverride = -1) {
  CamSetup setup;
  setup.name = "Mill setup";
  setup.machine_type = "3_axis_mill";
  setup.retract_height = retractHeight;
  if (wcsOrigin.has_value()) {
    setup.wcs_origin.anchor = "point";
    setup.wcs_origin.position = wcsOrigin;
  }
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
  op.name = "Adaptive 1";
  op.type = "adaptive_clearing";
  op.tool_id = document.cam.tool_library[0].tool_id;
  op.parameters.feedrate_mm_per_min = 500.0;
  op.parameters.plunge_feedrate_mm_per_min = 200.0;
  op.parameters.stepover_percent = 50.0;

  if (withFace) {
    const auto compiled = polysmith::core::compile_bodies(document);
    const auto& body = compiled.bodies[0];
    int topIndex = faceIndexOverride;
    if (topIndex < 0 && floorZ >= 0.0) {
      topIndex = find_upward_face_at_z(body, floorZ);
    } else if (topIndex < 0) {
      // Highest upward face of body 0 = the floor.
      double topZ = -1e9;
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
            if (normal.Z() > 0.99 && center.Z() > topZ) {
              topZ = center.Z();
              topIndex = i - 1;
            }
          }
        } catch (const std::exception&) {
          continue;
        }
      }
    }
    if (topIndex < 0) {
      throw std::runtime_error("adaptive floor face not found");
    }
    op.geometry_references.machining_regions.push_back(
        capture_face_ref(body, topIndex));
  }
  for (const auto& island : islands) {
    op.geometry_references.avoidance_regions.push_back(island);
  }
  document = manager.cam_operation_add(op);
  return document.cam.operations.back().op_id;
}

// Draws a circle in a fresh sketch on ref-plane-xy and returns the
// first profile id (throws when the sketch produced no profile).
std::string make_circle_profile(DocumentManager& manager,
                                DocumentState& document, double x, double y,
                                double radius) {
  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.add_sketch_circle(x, y, radius);
  for (const auto& feature : document.feature_history) {
    if (feature.kind == "sketch" && feature.sketch_parameters.has_value() &&
        !feature.sketch_parameters->profiles.empty()) {
      return feature.sketch_parameters->profiles[0].id;
    }
  }
  throw std::runtime_error("circle sketch profile not found");
}

// Mutates the op's parameters through the update command (which also
// runs the refresh pass).
template <typename Fn>
void set_op_params(DocumentManager& manager, DocumentState& document,
                   const std::string& opId, Fn mutate) {
  auto op = std::find_if(document.cam.operations.begin(),
                         document.cam.operations.end(),
                         [&](const CamOperation& candidate) {
                           return candidate.op_id == opId;
                         });
  if (op == document.cam.operations.end()) {
    throw std::runtime_error("op not found");
  }
  mutate(op->parameters);
  document = manager.cam_operation_update(opId, *op);
}

// Sets the op's stepdown through the update command.
void set_stepdown(DocumentManager& manager, DocumentState& document,
                  const std::string& opId, double stepdown) {
  set_op_params(manager, document, opId,
                [stepdown](CamOperationParameters& params) {
                  params.stepdown_mm = stepdown;
                });
}

// A closed feed run: a run of >= minPoints consecutive feed moves whose
// every endpoint satisfies `band` and whose last endpoint equals the
// first (the loop closes).
bool has_closed_contour(const Toolpath& toolpath,
                        const std::function<bool(double, double)>& band,
                        size_t minPoints) {
  size_t runStart = 0;
  bool inRun = false;
  const auto runIsContour = [&](size_t begin, size_t end) {
    if (end - begin < minPoints) {
      return false;
    }
    const auto& first = toolpath.moves[begin];
    const auto& last = toolpath.moves[end - 1];
    if (!near(first.x, last.x, 1e-6) || !near(first.y, last.y, 1e-6)) {
      return false;
    }
    for (size_t i = begin; i < end; ++i) {
      if (!band(toolpath.moves[i].x, toolpath.moves[i].y)) {
        return false;
      }
    }
    return true;
  };
  for (size_t i = 0; i <= toolpath.moves.size(); ++i) {
    const bool feed = i < toolpath.moves.size() &&
                      toolpath.moves[i].kind == ToolpathMoveKind::FeedLinear;
    if (feed) {
      if (!inRun) {
        runStart = i;
        inRun = true;
      }
    } else if (inRun) {
      if (runIsContour(runStart, i)) {
        return true;
      }
      inRun = false;
    }
  }
  return false;
}

// For every feed run (rapid-separated) with >= 3 points: the minimum
// distance of its points to the 20×20 boundary; sorted ascending.  The
// spiral spacing workhorse — loop k of a plain box registers exactly
// rEff + k·spacing.
std::vector<double> chain_wall_dists(const Toolpath& toolpath) {
  std::vector<double> dists;
  size_t runStart = 0;
  bool inRun = false;
  const auto finishRun = [&](size_t end) {
    if (end - runStart >= 3) {
      double minD = 1e9;
      for (size_t k = runStart; k < end; ++k) {
        minD = std::min(
            minD,
            std::min(std::min(toolpath.moves[k].x, 20.0 - toolpath.moves[k].x),
                     std::min(toolpath.moves[k].y,
                              20.0 - toolpath.moves[k].y)));
      }
      dists.push_back(minD);
    }
  };
  for (size_t i = 0; i <= toolpath.moves.size(); ++i) {
    const bool feed = i < toolpath.moves.size() &&
                      toolpath.moves[i].kind == ToolpathMoveKind::FeedLinear;
    if (feed) {
      if (!inRun) {
        runStart = i;
        inRun = true;
      }
    } else if (inRun) {
      finishRun(i);
      inRun = false;
    }
  }
  std::sort(dists.begin(), dists.end());
  return dists;
}

// Shoelace-signed area of one feed run's points (the run is walked in
// toolpath order).
double run_signed_area(const Toolpath& toolpath, size_t begin, size_t end) {
  double area = 0.0;
  for (size_t i = begin; i < end; ++i) {
    const auto& a = toolpath.moves[i];
    const auto& b = toolpath.moves[i + 1 < end ? i + 1 : begin];
    area += a.x * b.y - b.x * a.y;
  }
  return 0.5 * area;
}

// The signed area of the LONGEST feed run whose every point satisfies
// `band` (0.0 when none) — the climb-direction probe: the outer spiral
// loops walk CW (negative), island/boss loops walk CCW (positive).
double chain_signed_area(const Toolpath& toolpath,
                         const std::function<bool(double, double)>& band) {
  double bestArea = 0.0;
  size_t bestLength = 0;
  size_t runStart = 0;
  bool inRun = false;
  for (size_t i = 0; i <= toolpath.moves.size(); ++i) {
    const bool feed = i < toolpath.moves.size() &&
                      toolpath.moves[i].kind == ToolpathMoveKind::FeedLinear;
    if (feed) {
      if (!inRun) {
        runStart = i;
        inRun = true;
      }
    } else if (inRun) {
      bool allInBand = true;
      for (size_t k = runStart; k < i; ++k) {
        if (!band(toolpath.moves[k].x, toolpath.moves[k].y)) {
          allInBand = false;
          break;
        }
      }
      if (allInBand && (i - runStart) > bestLength) {
        bestLength = i - runStart;
        bestArea = run_signed_area(toolpath, runStart, i);
      }
      inRun = false;
    }
  }
  return bestArea;
}

// ── Test 1: registry ──────────────────────────────────────────────

bool test_registry() {
  const auto* generator =
      polysmith::core::find_cam_generator("adaptive_clearing");
  return expect(generator != nullptr && generator->generate != nullptr,
                "registry: adaptive_clearing generator registered");
}

// ── Test 2: plain box spiral — inward loops, CW climb, retract ─────
//
// 20×20×10 box, 6 mm tool, allowance 0.2 → rEff 3.2; stepover 50 →
// spacing 3.0 → loops at wall distances 3.2, 6.2, 9.2 (the loop at
// 12.2 collapses — past the 10 mm inradius).  Each loop is one
// rapid-plunge-feed-rapid cycle at the retract plane; loop 0 walks CW
// (climb: material left, wall outside).

bool test_plain_box_spiral() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const std::string opId = make_adaptive_op(manager, document, {});

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "plain box: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  // Single pass: rapids at retract, feeds at the face.
  double minWall = 1e9;
  double maxWall = -1e9;
  int rapids = 0;
  int feeds = 0;
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::Rapid) {
      ++rapids;
      if (!near(move.z, 20.0, 0.001)) {
        std::cerr << "  rapid at z=" << move.z << " (expected 20)\n";
        return expect(false, "plain box: every rapid at the retract plane");
      }
    } else {
      ++feeds;
      if (!near(move.z, 10.0, 0.001)) {
        std::cerr << "  feed at z=" << move.z << " (expected 10)\n";
        return expect(false, "plain box: every feed at the face");
      }
      const double d =
          std::min(std::min(move.x, 20.0 - move.x),
                   std::min(move.y, 20.0 - move.y));
      minWall = std::min(minWall, d);
      maxWall = std::max(maxWall, d);
    }
  }
  if (!expect(rapids > 0 && feeds > 0,
              "plain box: rapids and feeds present")) {
    return false;
  }
  if (!expect(minWall >= 3.1 && maxWall <= 10.05,
              "plain box: feeds stay inside the rEff inset and inside "
              "the box")) {
    std::cerr << "  wall dist range [" << minWall << ", " << maxWall
              << "]\n";
    return false;
  }

  // Loop spacing: chains register 3.2, 6.2, 9.2 and nothing deeper
  // (the 12.2 loop collapsed).
  const auto dists = chain_wall_dists(toolpath);
  if (!expect(dists.size() >= 3,
              "plain box: at least three spiral loops")) {
    return false;
  }
  if (!expect(near(dists[0], 3.2, 0.15) && near(dists[1], 6.2, 0.15) &&
                  near(dists[2], 9.2, 0.15),
              "plain box: loop wall distances 3.2, 6.2, 9.2")) {
    std::cerr << "  chain dists:";
    for (const double d : dists) {
      std::cerr << " " << d;
    }
    std::cerr << "\n";
    return false;
  }
  if (!expect(dists.back() <= 9.3,
              "plain box: the family ends when the offset collapses")) {
    return false;
  }

  // Loop 0 rides the rEff inset and walks CW (climb — negative signed
  // area).
  const double outerArea = chain_signed_area(
      toolpath, [](double x, double y) {
        const double d =
            std::min(std::min(x, 20.0 - x), std::min(y, 20.0 - y));
        return d > 2.9 && d < 3.5;
      });
  if (!expect(outerArea < 0.0,
              "plain box: loop 0 walks CW (climb)")) {
    std::cerr << "  loop 0 signed area: " << outerArea << "\n";
    return false;
  }

  // Emission shape: rapid → plunge feed → cut feed.
  if (!expect(toolpath.moves.size() >= 6 &&
                  toolpath.moves[0].kind == ToolpathMoveKind::Rapid &&
                  near(toolpath.moves[0].z, 20.0, 0.001) &&
                  toolpath.moves[1].kind == ToolpathMoveKind::FeedLinear &&
                  near(toolpath.moves[1].z, 10.0, 0.001) &&
                  toolpath.moves[2].kind == ToolpathMoveKind::FeedLinear &&
                  near(toolpath.moves[2].z, 10.0, 0.001),
              "plain box: rapid-plunge-feed emission shape")) {
    return false;
  }
  return true;
}

// ── Test 3: stepover 100% — spacing = diameter ─────────────────────

bool test_stepover_100() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const std::string opId = make_adaptive_op(manager, document, {});
  set_op_params(manager, document, opId,
                [](CamOperationParameters& params) {
                  params.stepover_percent = 100.0;
                });

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "stepover 100: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const auto dists = chain_wall_dists(outcome.result.toolpath);
  if (!expect(dists.size() >= 2 && near(dists[0], 3.2, 0.15) &&
                  near(dists[1], 9.2, 0.15),
              "stepover 100: loops at 3.2 and 9.2 (spacing = diameter)")) {
    std::cerr << "  chain dists:";
    for (const double d : dists) {
      std::cerr << " " << d;
    }
    std::cerr << "\n";
    return false;
  }
  for (const double d : dists) {
    if (d > 5.7 && d < 6.7) {
      std::cerr << "  unexpected loop at wall distance " << d << "\n";
      return expect(false, "stepover 100: no loop in (5.7, 6.7)");
    }
  }
  return true;
}

// ── Test 4: min-spacing guard — spacing clamps at 0.1 mm ───────────

bool test_min_spacing_guard() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const std::string opId = make_adaptive_op(manager, document, {});
  set_op_params(manager, document, opId,
                [](CamOperationParameters& params) {
                  params.stepover_percent = 1.0;  // 0.06 → clamped 0.1
                });

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "min spacing: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const auto dists = chain_wall_dists(outcome.result.toolpath);
  if (!expect(dists.size() >= 2,
              "min spacing: at least two loops")) {
    return false;
  }
  if (!expect(near(dists[0], 3.2, 0.06) && near(dists[1], 3.3, 0.06) &&
                  dists[1] - dists[0] >= 0.09,
              "min spacing: loops at 3.2 and 3.3 (0.1 mm clamp)")) {
    std::cerr << "  chain dists:";
    for (const double d : dists) {
      std::cerr << " " << d;
    }
    std::cerr << "\n";
    return false;
  }
  return true;
}

// ── Island fixtures ───────────────────────────────────────────────
//
// Base 20×20×10 (floor z=10) + boss 8×8×15 (top z=15, footprint
// [0,8]²).  The op targets the base top; the boss top is the island.

struct IslandFixture {
  std::string opId;
  GeometryReference island;
  double faceZ = 10.0;
  double islandTopZ = 15.0;
};

IslandFixture make_island_fixture(
    DocumentManager& manager, DocumentState& document,
    double retractHeight = 20.0,
    const std::optional<std::array<double, 3>>& stockSize = std::nullopt) {
  document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  document = manager.add_box_feature(
      {.width = 8.0, .height = 8.0, .depth = 15.0});
  const auto compiled = polysmith::core::compile_bodies(document);
  const int islandIndex =
      find_upward_face_at_z(compiled.bodies[1], 15.0);
  if (islandIndex < 0) {
    throw std::runtime_error("island top face not found");
  }
  const GeometryReference island =
      capture_face_ref(compiled.bodies[1], islandIndex);
  IslandFixture fixture;
  fixture.island = island;
  fixture.opId =
      make_adaptive_op(manager, document, {island}, retractHeight, stockSize);
  return fixture;
}

// ── Test 5: island avoided at the floor level ─────────────────────
//
// The grown island [0,8]² + rEff 3.2 reaches into the wall band at the
// bottom-left, so the island family loop 0 clips to an open chain
// there (the closed-contour climb pin lives on the centered cylinder
// boss in Test 7).  The outer loops clip around the island and loop 2
// (9.2) lies fully inside the grown footprint — clipped away.

bool test_island_single_pass() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document;
  IslandFixture fixture = make_island_fixture(manager, document);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), fixture.opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "island: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  bool sawFeed = false;
  bool sawIslandBand = false;
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::Rapid) {
      continue;
    }
    sawFeed = true;
    if (!near(move.z, fixture.faceZ, 0.001)) {
      return expect(false, "island: single-pass feeds only at the face");
    }
    const double rd = rect_distance(move.x, move.y, 0.0, 0.0, 8.0, 8.0);
    // The grown footprint (rEff 3.2) must keep its clearance.
    if (rd < 3.0) {
      std::cerr << "  feed inside the grown island at (" << move.x << ", "
                << move.y << ")\n";
      return expect(false, "island: no feed inside the grown footprint");
    }
    if (rd > 2.9 && rd < 3.5) {
      sawIslandBand = true;  // the island family rides the grown loop
    }
  }
  if (!expect(sawFeed, "island: feeds present")) {
    return false;
  }
  if (!expect(sawIslandBand,
              "island: the island family loop rides the grown "
              "footprint")) {
    return false;
  }

  // Outer loops clip around the island: chains at 3.2 and 6.2.  Loop 2
  // (9.2) is NOT fully clipped — the grown region's corner notch (the
  // quarter-disc arc bulges INWARD of the [−3.2,11.2]² bounding
  // square) leaves loop 2's top-right corner OUTSIDE the clearance,
  // so the clip correctly keeps it machinable (it stays ≥ 3.0 from
  // the island — pinned above).  A bounding-square clip would wrongly
  // remove it; the arc-aware clip keeps it.
  const auto dists = chain_wall_dists(toolpath);
  bool saw32 = false;
  bool saw62 = false;
  bool saw92 = false;
  for (const double d : dists) {
    saw32 = saw32 || near(d, 3.2, 0.15);
    saw62 = saw62 || near(d, 6.2, 0.15);
    saw92 = saw92 || (d > 8.7 && d < 9.7);
  }
  if (!expect(saw32 && saw62,
              "island: outer loops at 3.2 and 6.2 clip around the "
              "island")) {
    std::cerr << "  chain dists:";
    for (const double d : dists) {
      std::cerr << " " << d;
    }
    std::cerr << "\n";
    return false;
  }
  return expect(saw92,
                "island: the corner notch beyond the clearance arc is "
                "still machined (arc-aware clip)");
}

// ── Test 6: island-top level — a flush pass over the boss ─────────
//
// Stock top 16 (stock 24×24×17 on the z∈[0,15] model bbox), floor 10,
// island top 15, stepdown 2 → levels 15, 14, 12, 10.  At 15 the island
// is NOT subtracted (flush cut over its top); below it, it is.

bool test_island_top_level() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document;
  IslandFixture fixture = make_island_fixture(
      manager, document, /*retractHeight=*/20.0,
      std::array<double, 3>{24.0, 24.0, 17.0});
  set_stepdown(manager, document, fixture.opId, 2.0);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), fixture.opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "island-top level: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  std::set<double> feedZs;
  bool sawFlushPass = false;
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::Rapid) {
      continue;
    }
    feedZs.insert(move.z);
    if (near(move.z, fixture.islandTopZ, 0.001) &&
        rect_distance(move.x, move.y, 0.0, 0.0, 8.0, 8.0) < 0.01) {
      sawFlushPass = true;  // feed INSIDE the island base at its top
    }
  }
  // Exact set equality is fragile against fp noise — require exactly
  // the four expected levels, each within 1e-3.
  const std::array<double, 4> expectedZs = {15.0, 14.0, 12.0, 10.0};
  bool levelsMatch = feedZs.size() == expectedZs.size();
  if (levelsMatch) {
    for (const double expected : expectedZs) {
      levelsMatch = levelsMatch &&
                    std::any_of(feedZs.begin(), feedZs.end(),
                                [&](double actual) {
                                  return near(actual, expected, 1e-3);
                                });
    }
  }
  if (!expect(levelsMatch,
              "island-top level: levels are 15 (island top), 14, 12, 10")) {
    std::cerr << "  feed zs:";
    for (const double z : feedZs) {
      std::cerr << " " << z;
    }
    std::cerr << "\n";
    return false;
  }
  if (!expect(sawFlushPass,
              "island-top level: a pass cuts over the island at its top")) {
    return false;
  }
  // Below the island top the footprint is avoided.
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::Rapid ||
        near(move.z, fixture.islandTopZ, 0.001)) {
      continue;
    }
    if (rect_distance(move.x, move.y, 0.0, 0.0, 8.0, 8.0) < 2.9) {
      std::cerr << "  feed inside the grown island at z=" << move.z << "\n";
      return expect(false, "island-top level: island avoided below its top");
    }
  }
  return true;
}

// ── Test 7: boss wire on the floor — avoided + CCW climb contour ──
//
// A cylinder JOINed onto the box (the round-boss part): the floor
// face's inner wire borders a wall ABOVE the floor → a boss, always
// avoided.  Its family loop 0 rides the grown circle (r = 2.5 + 3.2 =
// 5.7) — unclipped (far from the walls), closed, walking CCW (climb
// around an outside contour: material left = the boss).

bool test_boss_wire_avoided() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const std::string bodyId =
      polysmith::core::compile_bodies(document).bodies[0].id;
  const std::string profile =
      make_circle_profile(manager, document, 10.0, 10.0, 2.5);
  document = manager.extrude_profiles({profile}, 15.0, "join", bodyId,
                                       std::nullopt);
  const std::string opId =
      make_adaptive_op(manager, document, {}, /*retractHeight=*/20.0,
                       std::nullopt, /*floorZ=*/10.0);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "boss: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  // No feed point may enter the grown boss (radius 5.7; the contour
  // ring itself sits at 5.7 ± sampling sagitta 0.05, so 5.4 is the
  // discriminating cutoff).
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::Rapid) {
      continue;
    }
    if (dist(move.x, move.y, 10.0, 10.0) < 5.4) {
      std::cerr << "  feed at distance "
                << dist(move.x, move.y, 10.0, 10.0)
                << " from the boss centre\n";
      return expect(false, "boss: no feed inside the grown footprint");
    }
  }

  // The closed loop around the grown circle — the boss family loop 0.
  const bool bossContour = has_closed_contour(
      toolpath,
      [](double x, double y) {
        const double d = std::hypot(x - 10.0, y - 10.0);
        return d > 5.5 && d < 6.0;
      },
      /*minPoints=*/10);
  if (!expect(bossContour,
              "boss: a closed loop rides the grown footprint")) {
    return false;
  }

  // Climb around a boss walks CCW — positive signed area.
  const double bossArea = chain_signed_area(
      toolpath, [](double x, double y) {
        const double d = std::hypot(x - 10.0, y - 10.0);
        return d > 5.5 && d < 6.0;
      });
  if (!expect(bossArea > 0.0, "boss: the island family walks CCW (climb)")) {
    std::cerr << "  boss loop signed area: " << bossArea << "\n";
    return false;
  }

  // The outer loop 0 still rides the rEff inset.
  const auto dists = chain_wall_dists(toolpath);
  for (const double d : dists) {
    if (near(d, 3.2, 0.15)) {
      return true;
    }
  }
  std::cerr << "  chain dists:";
  for (const double d : dists) {
    std::cerr << " " << d;
  }
  std::cerr << "\n";
  return expect(false, "boss: outer loop 0 rides the rEff inset");
}

// ── Test 8: through-hole floor — the spiral CROSSES the open hole ──
//
// A 2.5 mm-radius hole through the floor (walls below the floor → open
// hole).  Avoiding it would leave a stock plug sitting in the finished
// part's hole — the spiral loops cross it instead.

bool test_through_hole_crossed() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const std::string bodyId =
      polysmith::core::compile_bodies(document).bodies[0].id;
  const std::string profile =
      make_circle_profile(manager, document, 10.0, 10.0, 2.5);
  document = manager.extrude_profiles({profile}, 10.0, "cut", bodyId,
                                       std::nullopt);
  const std::string opId = make_adaptive_op(manager, document, {});

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "hole: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  // A feed SEGMENT (previous feed point → this one) must pass within
  // the 2.5 mm hole radius of the centre — the loop at wall distance
  // 9.2 crosses it.
  bool crossesHole = false;
  double prevX = 0.0, prevY = 0.0;
  bool havePrev = false;
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::Rapid) {
      havePrev = false;  // rapids break the feed chain
      continue;
    }
    if (havePrev && near(move.z, 10.0, 0.001) &&
        point_segment_dist(10.0, 10.0, prevX, prevY, move.x, move.y) < 2.5) {
      std::cerr << "  crossing segment (" << prevX << ", " << prevY
                << ")→(" << move.x << ", " << move.y << ")\n";
      crossesHole = true;
    }
    prevX = move.x;
    prevY = move.y;
    havePrev = true;
  }
  return expect(crossesHole,
                "hole: a spiral loop crosses the open hole (no stock plug)");
}

// ── Test 9: strategy + direction warnings ─────────────────────────

bool test_strategy_and_direction_warnings() {
  // strategy "zigzag" → warns and still generates the spiral.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 10.0});
    const std::string opId = make_adaptive_op(manager, document, {});
    set_op_params(manager, document, opId,
                  [](CamOperationParameters& params) {
                    params.strategy = "zigzag";
                  });
    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && outcome.result.ok,
                "strategy: generation still succeeds")) {
      return false;
    }
    bool warned = false;
    for (const auto& warning : outcome.result.warnings) {
      if (warning.find("adaptive strategy is implemented in v1") !=
          std::string::npos) {
        warned = true;
      }
    }
    if (!expect(warned, "strategy: non-adaptive strategy warns")) {
      return false;
    }
  }
  // strategy "adaptive" (and the absent default) → no warning.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 10.0});
    const std::string opId = make_adaptive_op(manager, document, {});
    set_op_params(manager, document, opId,
                  [](CamOperationParameters& params) {
                    params.strategy = "adaptive";
                  });
    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && outcome.result.ok,
                "strategy adaptive: generation succeeds")) {
      return false;
    }
    for (const auto& warning : outcome.result.warnings) {
      if (warning.find("adaptive strategy is implemented in v1") !=
          std::string::npos) {
        return expect(false, "strategy adaptive: no strategy warning");
      }
    }
  }
  // cutting_direction "mixed" warns and behaves as climb.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 10.0});
    const std::string opId = make_adaptive_op(manager, document, {});
    set_op_params(manager, document, opId,
                  [](CamOperationParameters& params) {
                    params.cutting_direction = "mixed";
                  });
    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && outcome.result.ok,
                "direction: generation succeeds")) {
      return false;
    }
    bool warned = false;
    for (const auto& warning : outcome.result.warnings) {
      if (warning.find("treated as climb") != std::string::npos) {
        warned = true;
      }
    }
    if (!expect(warned, "direction: 'mixed' warns about the climb "
                        "fallback")) {
      return false;
    }
  }
  // "conventional" flips the walks: the outer loop 0 walks CCW now.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 10.0});
    const std::string opId = make_adaptive_op(manager, document, {});
    set_op_params(manager, document, opId,
                  [](CamOperationParameters& params) {
                    params.cutting_direction = "conventional";
                  });
    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && outcome.result.ok,
                "conventional: generation succeeds")) {
      std::cerr << "  error: " << outcome.result.error_message << "\n";
      return false;
    }
    const double outerArea = chain_signed_area(
        outcome.result.toolpath, [](double x, double y) {
          const double d =
              std::min(std::min(x, 20.0 - x), std::min(y, 20.0 - y));
          return d > 2.9 && d < 3.5;
        });
    return expect(outerArea > 0.0,
                  "conventional: loop 0 walks CCW");
  }
}

// ── Test 10: error paths ──────────────────────────────────────────

bool test_errors() {
  // (a) No machining region.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 10.0});
    const std::string opId = make_adaptive_op(
        manager, document, {}, /*retractHeight=*/20.0,
        /*stockSize=*/std::nullopt, /*floorZ=*/-1.0,
        /*wcsOrigin=*/std::nullopt, /*withFace=*/false);
    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && !outcome.result.ok,
                "errors: no face fails loudly")) {
      return false;
    }
    if (!expect(outcome.result.error_message.find("requires a selected "
                                                  "face") !=
                    std::string::npos,
                "errors: no face names the cause")) {
      std::cerr << "  error: " << outcome.result.error_message << "\n";
      return false;
    }
  }
  // (b) A vertical machining face.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 10.0});
    const auto compiled = polysmith::core::compile_bodies(document);
    const int sideIndex = find_vertical_face(compiled.bodies[0]);
    if (!expect(sideIndex >= 0, "errors: vertical face found")) {
      return false;
    }
    const std::string opId = make_adaptive_op(
        manager, document, {}, /*retractHeight=*/20.0,
        /*stockSize=*/std::nullopt, /*floorZ=*/-1.0,
        /*wcsOrigin=*/std::nullopt, /*withFace=*/true, sideIndex);
    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && !outcome.result.ok,
                "errors: vertical face fails loudly")) {
      return false;
    }
    if (!expect(outcome.result.error_message.find("horizontal upward-facing "
                                                  "face") !=
                    std::string::npos,
                "errors: vertical face names the cause")) {
      std::cerr << "  error: " << outcome.result.error_message << "\n";
      return false;
    }
  }
  // (c) A 5×5 face — the rEff 3.2 inset collapses immediately.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 5.0, .height = 5.0, .depth = 10.0});
    const std::string opId = make_adaptive_op(manager, document, {});
    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && !outcome.result.ok,
                "errors: too-small face fails loudly")) {
      return false;
    }
    return expect(outcome.result.error_message.find("smaller than the "
                                                    "tool") !=
                      std::string::npos,
                  "errors: too-small face names the cause");
  }
}

// ── Test 11: broken island attestation fails generate AND refresh ──

bool test_broken_island_attestation() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document;
  IslandFixture fixture = make_island_fixture(manager, document);

  // Corrupt the island witness: sample points far from every face.
  auto modified = manager.get_document().value();
  bool corrupted = false;
  for (auto& op : modified.cam.operations) {
    if (op.op_id != fixture.opId) {
      continue;
    }
    for (auto& ref : op.geometry_references.avoidance_regions) {
      if (std::holds_alternative<FaceAttestation>(ref.attestation)) {
        auto& att = std::get<FaceAttestation>(ref.attestation);
        att.area = 1.0;
        att.normal = std::array<double, 3>{0.0, 0.0, 1.0};
        att.sample_points = {{100.0, 100.0, 100.0},
                             {101.0, 100.0, 100.0},
                             {100.0, 101.0, 100.0}};
        corrupted = true;
      }
    }
  }
  if (!expect(corrupted, "broken island: witness corrupted")) {
    return false;
  }
  // The update runs the refresh pass — the op must degrade, never
  // silently regenerate a path that would cut into the boss.
  auto modifiedOp = std::find_if(
      modified.cam.operations.begin(), modified.cam.operations.end(),
      [&](const CamOperation& candidate) {
        return candidate.op_id == fixture.opId;
      });
  if (!expect(modifiedOp != modified.cam.operations.end(),
              "broken island: op found for update")) {
    return false;
  }
  document = manager.cam_operation_update(fixture.opId, *modifiedOp);
  auto op = std::find_if(
      document.cam.operations.begin(), document.cam.operations.end(),
      [&](const CamOperation& candidate) {
        return candidate.op_id == fixture.opId;
      });
  if (!expect(op != document.cam.operations.end() && op->status == "error",
              "broken island: refresh degrades the op")) {
    std::cerr << "  status: " << (op != document.cam.operations.end()
                                      ? op->status
                                      : "<gone>")
              << "\n";
    return false;
  }
  if (!expect(op->status_message.find("was not found") != std::string::npos,
              "broken island: degradation carries the dependency message")) {
    std::cerr << "  message: " << op->status_message << "\n";
    return false;
  }

  // Generation fails the same way — never skip the island.
  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), fixture.opId, /*preview=*/false);
  if (!expect(outcome.found && !outcome.result.ok,
              "broken island: generation fails loudly")) {
    return false;
  }
  return expect(outcome.result.error_message.find("was not found") !=
                    std::string::npos,
                "broken island: generate carries the dependency message");
}

// ── Test 12: retract guards ───────────────────────────────────────

bool test_retract_guards() {
  // Guard 1: retract below the face.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 10.0});
    const std::string opId = make_adaptive_op(manager, document, {},
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
  // Guard 2: multi-pass with retract between the face and the stock top
  // (stock top 13, face 10, retract 12).
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 10.0});
    const std::string opId = make_adaptive_op(
        manager, document, {}, /*retractHeight=*/12.0,
        std::array<double, 3>{24.0, 24.0, 16.0});
    set_stepdown(manager, document, opId, 2.0);
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

// ── Test 13: adaptive params survive the payload round-trip ────────

bool test_payload_roundtrip() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document;
  IslandFixture fixture = make_island_fixture(manager, document);
  set_op_params(manager, document, fixture.opId,
                [](CamOperationParameters& params) {
                  params.stepover_percent = 40.0;
                  params.stepdown_mm = 1.5;
                  params.engagement_angle_deg = 30.0;
                  params.strategy = "adaptive";
                  params.cutting_direction = "climb";
                });

  auto op = std::find_if(
      document.cam.operations.begin(), document.cam.operations.end(),
      [&](const CamOperation& candidate) {
        return candidate.op_id == fixture.opId;
      });
  if (!expect(op != document.cam.operations.end() &&
                  op->geometry_references.avoidance_regions.size() == 1,
              "roundtrip: op has one avoidance region")) {
    return false;
  }

  const polysmith::protocol::json payload = polysmith::protocol::to_payload(*op);
  const CamOperation restored =
      polysmith::protocol::cam_operation_from_payload(payload);

  if (!expect(restored.type == "adaptive_clearing",
              "roundtrip: op type preserved")) {
    return false;
  }
  if (!expect(restored.geometry_references.avoidance_regions.size() == 1,
              "roundtrip: avoidance region survives")) {
    return false;
  }
  const auto& original = op->geometry_references.avoidance_regions[0];
  const auto& stored = restored.geometry_references.avoidance_regions[0];
  if (!expect(stored.persistent_id == original.persistent_id,
              "roundtrip: persistent id preserved")) {
    return false;
  }
  if (!expect(std::holds_alternative<FaceAttestation>(stored.attestation),
              "roundtrip: attestation kind preserved")) {
    return false;
  }
  if (!expect(restored.parameters.stepover_percent.has_value() &&
                  near(restored.parameters.stepover_percent.value(), 40.0),
              "roundtrip: stepover preserved")) {
    return false;
  }
  if (!expect(restored.parameters.stepdown_mm.has_value() &&
                  near(restored.parameters.stepdown_mm.value(), 1.5),
              "roundtrip: stepdown preserved")) {
    return false;
  }
  if (!expect(restored.parameters.engagement_angle_deg.has_value() &&
                  near(restored.parameters.engagement_angle_deg.value(),
                       30.0),
              "roundtrip: engagement angle preserved")) {
    return false;
  }
  if (!expect(restored.parameters.strategy.has_value() &&
                  restored.parameters.strategy.value() == "adaptive",
              "roundtrip: strategy preserved")) {
    return false;
  }
  return expect(restored.parameters.cutting_direction == "climb",
                "roundtrip: cutting direction preserved");
}

}  // namespace

int main() {
  // The app registers builtin generators from CadCoreApp::run(); the
  // test process must do it itself (cam_generators_test pattern).
  polysmith::core::register_builtin_cam_generators();
  bool allPassed = true;

  std::cout << "adaptive_clearing_test\n";
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
  run("Test 2: plain box spiral", test_plain_box_spiral);
  run("Test 3: stepover 100%", test_stepover_100);
  run("Test 4: min-spacing guard", test_min_spacing_guard);
  run("Test 5: island avoided", test_island_single_pass);
  run("Test 6: island-top level", test_island_top_level);
  run("Test 7: boss wire avoided", test_boss_wire_avoided);
  run("Test 8: through-hole crossed", test_through_hole_crossed);
  run("Test 9: strategy + direction warnings",
      test_strategy_and_direction_warnings);
  run("Test 10: error paths", test_errors);
  run("Test 11: broken island attestation",
      test_broken_island_attestation);
  run("Test 12: retract guards", test_retract_guards);
  run("Test 13: payload round-trip", test_payload_roundtrip);

  if (allPassed) {
    std::cout << "adaptive_clearing_test passed\n";
    return 0;
  }
  return 1;
}
