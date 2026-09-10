// 2D Pocket generator test — the pocket operation end-to-end.
//
// Face witness → outer-loop selection (largest |area| wire) → tool-radius
// inset → inner-wire boss/hole classification (adjacent-wall COM probe) →
// island growth → per-level island filter → island-top level insertion →
// multi-piece row clipping → per-level finishing contours.  Covers the
// plain box, a through-hole floor (rows CROSS the open hole), a boss
// wire on the floor (avoided + circular finishing contour), island
// avoidance, the island-top flush pass, single-pass semantics, the
// single-pass island-on-boss hint, non-horizontal islands, broken
// island attestations (generate AND refresh), the level cap, the
// retract guards, and the avoidance-region payload round-trip.

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
// shared with the face-milling tests.
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

// Mill setup + 6 mm endmill + pocket_2d op on an upward face of body 0,
// with the given island references.  floorZ >= 0 targets the upward
// face at that height (a boss on the floor would otherwise win the
// "highest face" scan); floorZ < 0 picks the highest upward face.
// Returns the op id.
std::string make_pocket_op(
    DocumentManager& manager, DocumentState& document,
    const std::vector<GeometryReference>& islands,
    double retractHeight = 20.0,
    const std::optional<std::array<double, 3>>& stockSize = std::nullopt,
    double floorZ = -1.0,
    const std::optional<std::array<double, 3>>& wcsOrigin = std::nullopt) {
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

  const auto compiled = polysmith::core::compile_bodies(document);
  const auto& body = compiled.bodies[0];
  int topIndex = -1;
  if (floorZ >= 0.0) {
    topIndex = find_upward_face_at_z(body, floorZ);
  } else {
    // Highest upward face of body 0 = the pocket floor.
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
    throw std::runtime_error("pocket floor face not found");
  }

  CamOperation op;
  op.name = "Pocket 1";
  op.type = "pocket_2d";
  op.tool_id = document.cam.tool_library[0].tool_id;
  op.parameters.feedrate_mm_per_min = 500.0;
  op.parameters.plunge_feedrate_mm_per_min = 200.0;
  op.parameters.stepover_percent = 50.0;
  op.geometry_references.machining_regions.push_back(
      capture_face_ref(body, topIndex));
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

// A closed finishing contour: a run of >= minPoints consecutive feed
// moves whose every endpoint satisfies `band` and whose last endpoint
// equals the first (the loop closes).
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

// Sets the op's stepdown through the update command (which also runs
// the refresh pass).
void set_stepdown(DocumentManager& manager, DocumentState& document,
                  const std::string& opId, double stepdown) {
  auto op = std::find_if(document.cam.operations.begin(),
                         document.cam.operations.end(),
                         [&](const CamOperation& candidate) {
                           return candidate.op_id == opId;
                         });
  if (op == document.cam.operations.end()) {
    throw std::runtime_error("op not found");
  }
  op->parameters.stepdown_mm = stepdown;
  document = manager.cam_operation_update(opId, *op);
}

// ── Test 1: plain box — inset rows, zigzag alternation, retract ────

bool test_plain_box() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const std::string opId = make_pocket_op(manager, document, {});

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "plain box: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  // Rows stay inside the tool-radius inset [3,17]² and span it fully.
  double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
  int rapids = 0;
  int feeds = 0;
  bool sawRetractZ = false;
  bool sawFaceZ = false;
  for (const auto& move : toolpath.moves) {
    minX = std::min(minX, move.x);
    maxX = std::max(maxX, move.x);
    minY = std::min(minY, move.y);
    maxY = std::max(maxY, move.y);
    if (move.kind == ToolpathMoveKind::Rapid) {
      ++rapids;
      if (near(move.z, 20.0, 0.001)) {
        sawRetractZ = true;
      }
    } else {
      ++feeds;
      if (near(move.z, 10.0, 0.001)) {
        sawFaceZ = true;
      }
    }
  }
  if (!expect(minX >= 3.0 - 0.05 && maxX <= 17.0 + 0.05 &&
                  minY >= 3.0 - 0.05 && maxY <= 17.0 + 0.05 &&
                  near(minX, 3.0, 0.05) && near(maxX, 17.0, 0.05) &&
                  near(minY, 3.0, 0.05) && near(maxY, 17.0, 0.05),
              "plain box: rows span the full tool-radius inset")) {
    std::cerr << "  bounds: x[" << minX << ", " << maxX << "] y[" << minY
              << ", " << maxY << "]\n";
    return false;
  }
  if (!expect(sawRetractZ && sawFaceZ && rapids > 0 && feeds > 0,
              "plain box: rapids at retract, feeds at the face")) {
    return false;
  }

  // Zigzag: row 0 cuts (3,3)→(17,3); row 1 reverses to (17,6)→(3,6).
  // Move order per piece: rapid → plunge feed → cut feed → rapid.
  if (!expect(toolpath.moves.size() >= 8, "plain box: at least 2 rows")) {
    return false;
  }
  const auto& m = toolpath.moves;
  if (!expect(m[0].kind == ToolpathMoveKind::Rapid &&
                  near(m[0].x, 3.0, 0.05) && near(m[0].y, 3.0, 0.05),
              "plain box: first rapid to the first row start")) {
    return false;
  }
  if (!expect(m[1].kind == ToolpathMoveKind::FeedLinear &&
                  near(m[1].x, 3.0, 0.05) && near(m[1].y, 3.0, 0.05) &&
                  m[2].kind == ToolpathMoveKind::FeedLinear &&
                  near(m[2].x, 17.0, 0.05) && near(m[2].y, 3.0, 0.05),
              "plain box: row 0 cuts (3,3)→(17,3)")) {
    return false;
  }
  // Row 1 is global row 1 (odd) — zigzag reverses it.
  if (!expect(m[4].kind == ToolpathMoveKind::Rapid &&
                  near(m[4].x, 17.0, 0.05) && near(m[4].y, 6.0, 0.05) &&
                  m[6].kind == ToolpathMoveKind::FeedLinear &&
                  near(m[6].x, 3.0, 0.05) && near(m[6].y, 6.0, 0.05),
              "plain box: row 1 zigzags back (17,6)→(3,6)")) {
    for (size_t i = 0; i < std::min<size_t>(m.size(), 9); ++i) {
      std::cerr << "  [" << i << "] kind=" << static_cast<int>(m[i].kind)
                << " (" << m[i].x << ", " << m[i].y << ", " << m[i].z
                << ")\n";
    }
    return false;
  }
  return true;
}

// ── Test 2: through-hole floor — rows CROSS the open hole ─────────
//
// A 2.5 mm-radius hole through the floor (walls below the floor → open
// hole).  Avoiding it would leave a stock plug sitting in the finished
// part's hole — instead the row at y=9 mills straight across it as one
// piece (3,9)→(17,9), passing within 2.5 mm of the hole centre.

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
  const std::string opId = make_pocket_op(manager, document, {});

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "hole: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  // The row at y=9 (global row 2, even — no zigzag reversal) must be a
  // SINGLE piece (3,9)→(17,9) whose segment passes through the hole
  // (distance from the centre (10,10) below the 2.5 hole radius).
  bool sawCrossingRow = false;
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::Rapid || !near(move.z, 10.0, 0.001)) {
      continue;
    }
    if (near(move.x, 3.0, 0.05) && near(move.y, 9.0, 0.05)) {
      sawCrossingRow = true;
    }
  }
  if (!expect(sawCrossingRow, "hole: the row starts at (3,9) (unsplit)")) {
    return false;
  }
  // A feed SEGMENT (previous feed point → this one) must pass within
  // the 2.5 mm hole radius of the centre — the crossing row itself.
  // (Endpoint checks are not enough: the row endpoints sit outside the
  // hole; only the segment between them crosses it.)
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
                "hole: a feed segment crosses the open hole (no stock plug)");
}

// ── Test 2b: boss wire on the floor — avoided + contoured ─────────
//
// A cylinder JOINed onto the box (the user's round-boss part): the
// floor face's inner wire borders a wall ABOVE the floor → a boss.
// Rows split around the grown footprint (r = 2.5 + 3 = 5.5), and a
// closed finishing contour rides the grown loop — the circular motion
// around a round boss — plus one around the outer inset.

bool test_boss_wire_avoided_and_contoured() {
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
      make_pocket_op(manager, document, {}, /*retractHeight=*/20.0,
                     std::nullopt, /*floorZ=*/10.0);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "boss: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  // No feed point may enter the grown boss (radius 5.5; the contour
  // ring itself sits at 5.5 ± sampling sagitta 0.05, so 5.3 is the
  // discriminating cutoff).
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::Rapid) {
      continue;
    }
    if (dist(move.x, move.y, 10.0, 10.0) < 5.3) {
      std::cerr << "  feed at distance "
                << dist(move.x, move.y, 10.0, 10.0)
                << " from the boss centre\n";
      return expect(false, "boss: no feed inside the grown footprint");
    }
  }

  // The row at y=9 splits around the grown boss: piece ends at
  // 10 ± √(5.5² − 1) ≈ 4.59 / 15.41.
  bool sawLeftEnd = false;
  bool sawRightStart = false;
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::Rapid || !near(move.y, 9.0, 0.05) ||
        !near(move.z, 10.0, 0.001)) {
      continue;
    }
    if (near(move.x, 10.0 - std::sqrt(5.5 * 5.5 - 1.0), 0.15)) {
      sawLeftEnd = true;
    }
    if (near(move.x, 10.0 + std::sqrt(5.5 * 5.5 - 1.0), 0.15)) {
      sawRightStart = true;
    }
  }
  if (!expect(sawLeftEnd && sawRightStart,
              "boss: rows split at the grown footprint boundary")) {
    return false;
  }

  // The circular finishing contour around the boss.
  const bool bossContour = has_closed_contour(
      toolpath,
      [](double x, double y) {
        const double d = std::hypot(x - 10.0, y - 10.0);
        return d > 5.3 && d < 5.8;
      },
      /*minPoints=*/10);
  if (!expect(bossContour,
              "boss: a closed circular contour rides the grown loop")) {
    return false;
  }

  // The finishing contour around the outer inset wall (distance to the
  // 20×20 boundary ≈ 3 on the whole loop).  Straight walls sample to
  // their corners only — 4 feed points plus the plunge and the closing
  // return, so the minimum is 4.
  const bool outerContour = has_closed_contour(
      toolpath,
      [](double x, double y) {
        const double d = std::min(
            std::min(x, 20.0 - x), std::min(y, 20.0 - y));
        return d > 2.8 && d < 3.2;
      },
      /*minPoints=*/4);
  if (!outerContour) {
    // Failure diagnostics: dump every feed run (length + whether it
    // closes + point range).
    size_t runLength = 0;
    for (size_t i = 0; i <= toolpath.moves.size(); ++i) {
      const bool feed =
          i < toolpath.moves.size() &&
          toolpath.moves[i].kind == ToolpathMoveKind::FeedLinear;
      if (feed) {
        ++runLength;
      } else if (runLength > 0) {
        const size_t begin = i - runLength;
        double minD = 1e9, maxD = -1e9;
        for (size_t k = begin; k < i; ++k) {
          const double d = std::min(
              std::min(toolpath.moves[k].x, 20.0 - toolpath.moves[k].x),
              std::min(toolpath.moves[k].y, 20.0 - toolpath.moves[k].y));
          minD = std::min(minD, d);
          maxD = std::max(maxD, d);
        }
        std::cerr << "  run len=" << runLength << " closes="
                  << (near(toolpath.moves[begin].x,
                           toolpath.moves[i - 1].x, 1e-6) &&
                      near(toolpath.moves[begin].y,
                           toolpath.moves[i - 1].y, 1e-6))
                  << " wallDist [" << minD << ", " << maxD << "]\n";
        runLength = 0;
      }
    }
  }
  return expect(outerContour,
                "boss: a closed contour rides the outer inset wall");
}

// ── Test 2c: boss + hole on the same floor — avoid, cross ─────────
//
// The realistic part: a joined boss AND a through-hole on one floor.
// The boss wire (walls above) is avoided; the hole wire (walls below)
// is crossed by the rows.

bool test_boss_and_hole_mixed() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const std::string bodyId =
      polysmith::core::compile_bodies(document).bodies[0].id;
  const std::string bossProfile =
      make_circle_profile(manager, document, 10.0, 10.0, 2.5);
  document = manager.extrude_profiles({bossProfile}, 15.0, "join", bodyId,
                                       std::nullopt);
  const std::string holeProfile =
      make_circle_profile(manager, document, 16.0, 4.0, 1.5);
  document = manager.extrude_profiles({holeProfile}, 10.0, "cut", bodyId,
                                       std::nullopt);
  const std::string opId =
      make_pocket_op(manager, document, {}, /*retractHeight=*/20.0,
                     std::nullopt, /*floorZ=*/10.0);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "mixed: generation succeeds")) {
    std::cerr << "  error: " << outcome.result.error_message << "\n";
    return false;
  }
  const Toolpath& toolpath = outcome.result.toolpath;

  // The boss stays avoided.
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::Rapid) {
      continue;
    }
    if (dist(move.x, move.y, 10.0, 10.0) < 5.3) {
      std::cerr << "  feed at distance "
                << dist(move.x, move.y, 10.0, 10.0)
                << " from the boss centre\n";
      return expect(false, "mixed: no feed inside the grown boss");
    }
  }

  // The open hole is crossed: some feed segment passes within the
  // 1.5 mm hole radius of (16,4) — the row at y=3 spans it.
  bool crossesHole = false;
  double prevX = 0.0, prevY = 0.0;
  bool havePrev = false;
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::Rapid) {
      havePrev = false;
      continue;
    }
    if (havePrev && near(move.z, 10.0, 0.001) &&
        point_segment_dist(16.0, 4.0, prevX, prevY, move.x, move.y) < 1.5) {
      std::cerr << "  crossing segment (" << prevX << ", " << prevY
                << ")→(" << move.x << ", " << move.y << ")\n";
      crossesHole = true;
    }
    prevX = move.x;
    prevY = move.y;
    havePrev = true;
  }
  if (!expect(crossesHole, "mixed: rows cross the open hole")) {
    return false;
  }

  // And the circular contour around the boss still exists.
  return expect(
      has_closed_contour(
          toolpath,
          [](double x, double y) {
            const double d = std::hypot(x - 10.0, y - 10.0);
            return d > 5.3 && d < 5.8;
          },
          /*minPoints=*/10),
      "mixed: the boss finishing contour exists");
}

// ── Test 2d: single-pass island on a face boss warns ──────────────
//
// An island picked on a boss that is part of the pocket face adds no
// level in single-pass mode (the wire is avoided anyway) — generation
// warns instead of silently doing nothing.

bool test_single_pass_island_on_boss_hint() {
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
  const auto compiled = polysmith::core::compile_bodies(document);
  const int bossTopIndex = find_upward_face_at_z(compiled.bodies[0], 15.0);
  if (!expect(bossTopIndex >= 0, "hint: boss top face found")) {
    return false;
  }
  const std::string opId = make_pocket_op(
      manager, document,
      {capture_face_ref(compiled.bodies[0], bossTopIndex)},
      /*retractHeight=*/20.0, std::nullopt, /*floorZ=*/10.0);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "hint: generation succeeds")) {
    return false;
  }
  bool warned = false;
  for (const auto& warning : outcome.result.warnings) {
    if (warning.find("already avoided") != std::string::npos) {
      warned = true;
    }
  }
  return expect(warned,
                "hint: single-pass island on a face boss warns about "
                "the no-op");
}

// ── Island fixtures ───────────────────────────────────────────────
//
// Base 20×20×10 (floor z=10) + boss 8×8×15 (top z=15, footprint
// [0,8]²).  The pocket targets the base top; the boss top is the island.

struct IslandFixture {
  std::string opId;
  GeometryReference island;
  double faceZ = 10.0;
  double islandTopZ = 15.0;
};

IslandFixture make_island_fixture(DocumentManager& manager,
                                  DocumentState& document,
                                  double retractHeight = 20.0,
                                  const std::optional<std::array<double, 3>>&
                                      stockSize = std::nullopt) {
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
      make_pocket_op(manager, document, {island}, retractHeight, stockSize);
  return fixture;
}

// ── Test 3: island avoided at the floor level ─────────────────────

bool test_island_avoided() {
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
  for (const auto& move : toolpath.moves) {
    if (move.kind == ToolpathMoveKind::Rapid) {
      continue;
    }
    sawFeed = true;
    if (!near(move.z, fixture.faceZ, 0.001)) {
      return expect(false, "island: single-pass feeds only at the face");
    }
    // The grown island footprint = [0,8]² grown by the tool radius 3
    // (round joins grow a convex rect exactly) — every feed point must
    // keep the clearance.
    if (rect_distance(move.x, move.y, 0.0, 0.0, 8.0, 8.0) < 3.0 - 0.1) {
      std::cerr << "  feed inside the grown island at (" << move.x << ", "
                << move.y << ")\n";
      return expect(false, "island: no feed inside the grown footprint");
    }
  }
  if (!expect(sawFeed, "island: feeds present")) {
    return false;
  }
  // Rows past the island still span the far side.
  bool sawFarSide = false;
  for (const auto& move : toolpath.moves) {
    if (move.kind != ToolpathMoveKind::Rapid && move.x > 15.0) {
      sawFarSide = true;
    }
  }
  return expect(sawFarSide, "island: rows continue past the island");
}

// ── Test 4: island-top level — a flush pass over the boss ─────────
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
    if (rect_distance(move.x, move.y, 0.0, 0.0, 8.0, 8.0) < 3.0 - 0.1) {
      std::cerr << "  feed inside the grown island at z=" << move.z << "\n";
      return expect(false, "island-top level: island avoided below its top");
    }
  }
  return true;
}

// ── Test 5: single-pass + island — face level only, island avoided ─

bool test_single_pass_island() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document;
  IslandFixture fixture = make_island_fixture(manager, document);

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), fixture.opId, /*preview=*/false);
  if (!expect(outcome.found && outcome.result.ok,
              "single pass: generation succeeds")) {
    return false;
  }
  std::set<double> feedZs;
  for (const auto& move : outcome.result.toolpath.moves) {
    if (move.kind == ToolpathMoveKind::Rapid) {
      continue;
    }
    feedZs.insert(move.z);
  }
  // Only the face level — no island-top level in single-pass mode (the
  // stock above the island stays, documented v1 semantics).
  return expect(feedZs == std::set<double>({fixture.faceZ}),
                "single pass: feeds only at the face level");
}

// ── Test 6: non-horizontal island face errors, naming the island ──

bool test_non_horizontal_island() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  document = manager.add_box_feature(
      {.width = 8.0, .height = 8.0, .depth = 15.0});
  const auto compiled = polysmith::core::compile_bodies(document);
  const int sideIndex = find_vertical_face(compiled.bodies[1]);
  if (!expect(sideIndex >= 0, "non-horizontal: vertical face found")) {
    return false;
  }
  const std::string opId =
      make_pocket_op(manager, document,
                     {capture_face_ref(compiled.bodies[1], sideIndex)});

  const auto outcome = polysmith::core::generate_operation_toolpath(
      manager.get_document().value(), opId, /*preview=*/false);
  if (!expect(outcome.found && !outcome.result.ok,
              "non-horizontal: generation fails loudly")) {
    return false;
  }
  return expect(outcome.result.error_message.find("Island 1") !=
                        std::string::npos &&
                    outcome.result.error_message.find("horizontal") !=
                        std::string::npos,
                "non-horizontal: error names the island and the cause");
}

// ── Test 7: broken island attestation fails generate AND refresh ──

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

// ── Test 8: level cap + retract guards ────────────────────────────

bool test_level_cap_and_guards() {
  // Level cap: stock top 255, floor 10, stepdown 0.5 → capped at 100.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 10.0});
    const std::string opId = make_pocket_op(
        manager, document, {}, /*retractHeight=*/300.0,
        std::array<double, 3>{24.0, 24.0, 500.0});
    set_stepdown(manager, document, opId, 0.5);
    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && outcome.result.ok,
                "cap: generation succeeds")) {
      return false;
    }
    bool capped = false;
    for (const auto& warning : outcome.result.warnings) {
      if (warning.find("capped") != std::string::npos) {
        capped = true;
      }
    }
    if (!expect(capped, "cap: warns about the cap")) {
      return false;
    }
    std::set<double> feedZs;
    for (const auto& move : outcome.result.toolpath.moves) {
      if (move.kind != ToolpathMoveKind::Rapid) {
        feedZs.insert(move.z);
      }
    }
    if (!expect(feedZs.size() <= 101,
                "cap: at most 101 distinct feed levels")) {
      return false;
    }
  }
  // Guard 1: retract below the face.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 10.0});
    const std::string opId = make_pocket_op(manager, document, {},
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
    const std::string opId = make_pocket_op(
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

// ── Test 10: retract height is WCS-relative ───────────────────────
//
// Same contract as the contour suite: retract_height is machine Z
// above the WCS origin, so the retract PLANE is origin Z + height.
// Regression: the raw height was compared against the world face /
// stock top heights, so an origin above z=0 produced bogus warnings
// while the post emitted rapids inside the stock.

bool test_pocket_retract_wcs_relative() {
  // WCS z=20 + retract 25 → plane 45 clears the face 30: no retract
  // warnings, rapids at world z=45.
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 30.0});
    const std::string opId = make_pocket_op(
        manager, document, {}, /*retractHeight=*/25.0,
        /*stockSize=*/std::nullopt, /*floorZ=*/30.0,
        /*wcsOrigin=*/std::array<double, 3>{0.0, 0.0, 20.0});
    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && outcome.result.ok,
                "wcs retract: generation succeeds")) {
      std::cerr << "  error: " << outcome.result.error_message << "\n";
      return false;
    }
    bool warned = false;
    for (const auto& warning : outcome.result.warnings) {
      if (warning.find("below the face height") != std::string::npos ||
          warning.find("below the stock top") != std::string::npos) {
        warned = true;
      }
    }
    if (!expect(!warned,
                "wcs retract: retract 25 above the z=20 origin clears "
                "the face 30")) {
      for (const auto& warning : outcome.result.warnings) {
        std::cerr << "  warning: " << warning << "\n";
      }
      return false;
    }
    int rapids = 0;
    for (const auto& move : outcome.result.toolpath.moves) {
      if (move.kind == ToolpathMoveKind::Rapid) {
        ++rapids;
        if (!near(move.z, 45.0, 0.001)) {
          std::cerr << "  rapid at z=" << move.z << " (expected 45)\n";
          return expect(false, "wcs retract: rapids at world z=45");
        }
      }
    }
    return expect(rapids > 0, "wcs retract: at least one rapid");
  }
  // WCS z=20 + retract 12 + stepdown 2 (levels 31, 30) → plane 32
  // clears the face 30 but not the stock top 33: only the stock-top
  // warning fires (the face warning would mean the raw height was
  // compared again).
  {
    DocumentManager manager;
    manager.create_document();
    DocumentState document = manager.add_box_feature(
        {.width = 20.0, .height = 20.0, .depth = 30.0});
    const std::string opId = make_pocket_op(
        manager, document, {}, /*retractHeight=*/12.0,
        /*stockSize=*/std::nullopt, /*floorZ=*/30.0,
        /*wcsOrigin=*/std::array<double, 3>{0.0, 0.0, 20.0});
    set_stepdown(manager, document, opId, 2.0);
    const auto outcome = polysmith::core::generate_operation_toolpath(
        manager.get_document().value(), opId, /*preview=*/false);
    if (!expect(outcome.found && outcome.result.ok,
                "wcs retract low: generation succeeds")) {
      std::cerr << "  error: " << outcome.result.error_message << "\n";
      return false;
    }
    bool faceWarned = false;
    bool stockWarned = false;
    for (const auto& warning : outcome.result.warnings) {
      if (warning.find("below the face height") != std::string::npos) {
        faceWarned = true;
      }
      if (warning.find("below the stock top") != std::string::npos) {
        stockWarned = true;
      }
    }
    if (!expect(!faceWarned && stockWarned,
                "wcs retract low: plane 32 clears the face 30 but not "
                "the stock top 33 — stock-top warning only")) {
      for (const auto& warning : outcome.result.warnings) {
        std::cerr << "  warning: " << warning << "\n";
      }
      return false;
    }
    for (const auto& move : outcome.result.toolpath.moves) {
      if (move.kind == ToolpathMoveKind::Rapid &&
          !near(move.z, 32.0, 0.001)) {
        std::cerr << "  rapid at z=" << move.z << " (expected 32)\n";
        return expect(false, "wcs retract low: rapids at world z=32");
      }
    }
    return true;
  }
}

// ── Test 9: avoidance regions survive the payload round-trip ──────

bool test_avoidance_payload_roundtrip() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document;
  IslandFixture fixture = make_island_fixture(manager, document);

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
  const auto& att = std::get<FaceAttestation>(stored.attestation);
  return expect(att.sample_points.size() ==
                    std::get<FaceAttestation>(original.attestation)
                        .sample_points.size() &&
                    att.area > 0.0,
                "roundtrip: attestation content preserved");
}

}  // namespace

int main() {
  // The app registers builtin generators from CadCoreApp::run(); the
  // test process must do it itself (cam_generators_test pattern).
  polysmith::core::register_builtin_cam_generators();
  bool allPassed = true;

  std::cout << "pocket_2d_test\n";
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
  run("Test 1: plain box", test_plain_box);
  run("Test 2: through-hole crossed", test_through_hole_crossed);
  run("Test 2b: boss wire avoided + contoured",
      test_boss_wire_avoided_and_contoured);
  run("Test 2c: boss + hole mixed", test_boss_and_hole_mixed);
  run("Test 2d: single-pass island on face boss warns",
      test_single_pass_island_on_boss_hint);
  run("Test 3: island avoided", test_island_avoided);
  run("Test 4: island-top level", test_island_top_level);
  run("Test 5: single pass + island", test_single_pass_island);
  run("Test 6: non-horizontal island", test_non_horizontal_island);
  run("Test 7: broken island attestation", test_broken_island_attestation);
  run("Test 8: level cap + retract guards", test_level_cap_and_guards);
  run("Test 9: avoidance payload round-trip", test_avoidance_payload_roundtrip);
  run("Test 10: retract height is WCS-relative",
      test_pocket_retract_wcs_relative);

  if (allPassed) {
    std::cout << "pocket_2d_test passed\n";
    return 0;
  }
  return 1;
}
