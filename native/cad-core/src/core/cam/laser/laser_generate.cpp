#include "core/cam/laser/laser_generate.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "core/cam/cam2d.h"
#include "core/cam/cam_planning.h"
#include "core/cam/laser/laser_fill.h"
#include "core/cam/laser/laser_leads.h"
#include "core/cam/laser/laser_order.h"
#include "core/cam/laser/laser_tabs.h"
#include "core/diagnostics/logger.h"
#include "core/geometry/body_compiler.h"
#include "core/sketch/sketch_profile_types.h"

#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

namespace polysmith::core::laser {

namespace {

using polysmith::core::CamGenerateResult;
using polysmith::core::SketchFeatureParameters;
using polysmith::core::SketchProfilePoint;
using polysmith::core::SketchProfileRegion;
using polysmith::core::Toolpath;
using polysmith::core::ToolpathMove;
using polysmith::core::ToolpathMoveKind;

using cam2d::BaseSegment;
using cam2d::OffsetSegment;
using cam2d::XY;
using cam2d::base_segments_signed_area;
using cam2d::kOffsetEps;
using cam2d::offset_closed_loop;
using cam2d::offset_loop_length;
using cam2d::offset_loop_self_intersects;
using cam2d::reverse_segments;
using cam2d::sample_offset_loop;
using cam2d::xy_centroid;
using cam2d::xy_length;
using cam2d::xy_signed_area;

// A 2-axis gantry laser cuts in the machine XY plane (G17): sketch
// planes and body faces that are not horizontal cannot be emitted as
// valid arcs.  cos(5°) tolerance.
constexpr double kMaxCutPlaneTilt = 0.9962;

// ── Base-segment builders ────────────────────────────────────────
//
// Build the exact base loop for a sketch profile region (exact
// boundary edges where possible, sampled-point fallback otherwise).

// Builds exact base segments from the region's boundary_edges.  When
// any edge is an ellipse or spline (no exact offset), falls back to
// the sampled polygon.  Returns false for the fallback.
bool build_base_segments_from_edges(const SketchProfileRegion& region,
                                    std::vector<BaseSegment>& out) {
  out.clear();
  for (const auto& edge : region.boundary_edges) {
    if (edge.entity_kind == "ellipse" || edge.entity_kind == "spline") {
      return false;
    }
    BaseSegment segment;
    segment.start = {edge.start_x, edge.start_y};
    segment.end = {edge.end_x, edge.end_y};
    if (edge.entity_kind == "circle" || edge.entity_kind == "arc") {
      segment.is_arc = true;
      segment.center = {edge.center_x, edge.center_y};
      segment.radius = edge.radius;
      segment.ccw = edge.ccw;
    }
    out.push_back(segment);
  }
  return !out.empty();
}

// Sampled-polygon fallback (legacy profiles without exact edges, or
// edges we cannot offset exactly).  Assumes the points follow the
// walk orientation.
void build_base_segments_from_points(
    const std::vector<SketchProfilePoint>& points,
    std::vector<BaseSegment>& out) {
  out.clear();
  if (points.size() < 2) {
    return;
  }
  for (size_t i = 0; i < points.size(); ++i) {
    const auto& a = points[i];
    const auto& b = points[(i + 1) % points.size()];
    BaseSegment segment;
    segment.start = {a.x, a.y};
    segment.end = {b.x, b.y};
    out.push_back(segment);
  }
}

// World point of a sketch-local 2D point on the cut plane (the sketch
// plane offset along its normal).
XY world_point(const SketchFeatureParameters::SketchPlaneFrame& frame,
               const XY& p) {
  return XY{frame.origin_x + frame.x_axis_x * p.x + frame.y_axis_x * p.y,
            frame.origin_y + frame.x_axis_y * p.x + frame.y_axis_y * p.y};
}

double world_z(const SketchFeatureParameters::SketchPlaneFrame& frame,
               const XY& p, double cut_plane_offset) {
  return frame.origin_z + frame.x_axis_z * p.x + frame.y_axis_z * p.y +
         frame.normal_z * cut_plane_offset;
}

// Builds and offsets one loop (outer or hole) into a PlannedLoop.
// Returns false with a human message on hard failure.
bool plan_loop(const std::vector<BaseSegment>& base, double kerf,
               bool is_hole, const XY& centroid, PlannedLoop& out,
               std::string& error) {
  out.is_hole = is_hole;
  out.centroid = centroid;
  if (!offset_closed_loop(base, kerf, out.segments)) {
    error = "A profile contour could not be offset (check the kerf width).";
    return false;
  }
  const auto samples = sample_offset_loop(out.segments, /*tolerance=*/0.05);
  if (offset_loop_self_intersects(samples)) {
    error = std::string("A ") + (is_hole ? "hole" : "profile") +
            " contour self-intersects after the kerf offset — the feature "
            "is narrower than the kerf width; enlarge it or reduce the kerf.";
    return false;
  }
  // Keep the sampled contour around — emission uses it for pierce
  // placement, so it is sampled exactly once.
  out.samples = samples;
  // Area comes from the OFFSET loop's sampled polygon: the
  // base-segment shoelace is 0 for synthesized full circles
  // (start == end), and containment/nesting compares offset
  // geometry anyway.
  out.area = std::abs(xy_signed_area(out.samples));
  out.length = offset_loop_length(out.segments);
  return true;
}

// Rotates the loop's segments so the walk starts at `pierce`,
// splitting the containing segment in two when the pierce lies
// mid-segment (a lead-side angle landing inside a full-circle arc).
// The returned contour starts AND ends at the pierce.
std::vector<OffsetSegment> contour_starting_at(
    const std::vector<OffsetSegment>& segments, const XY& pierce) {
  constexpr double kVertexEps = 1e-6;

  // True when the pierce lies INSIDE the segment (not on its start
  // vertex — the identity rotation handles that case).
  const auto pierce_inside = [&](const OffsetSegment& segment) {
    if (!segment.is_arc) {
      if (cam2d::xy_point_segment_distance(pierce, segment.start,
                                           segment.end) > kVertexEps) {
        return false;
      }
      // The distance test also matches the unbounded line — require
      // the projection to lie within the span.
      const double len = xy_length(segment.end.x - segment.start.x,
                                   segment.end.y - segment.start.y);
      if (len < 1e-12) {
        return false;
      }
      const double t = ((pierce.x - segment.start.x) *
                            (segment.end.x - segment.start.x) +
                        (pierce.y - segment.start.y) *
                            (segment.end.y - segment.start.y)) /
                       (len * len);
      return t >= -1e-9 && t <= 1.0 + 1e-9;
    }
    const double dist = xy_length(pierce.x - segment.center.x,
                                  pierce.y - segment.center.y);
    return std::abs(dist - segment.radius) <= kVertexEps &&
           cam2d::offset_arc_contains_point(segment, pierce);
  };

  // Locate the pierce: an exact start-vertex match first (identity
  // case — the segment selectors return segment starts for auto
  // placement), then a mid-segment hit.
  size_t containing = segments.size();
  bool atEndpoint = false;
  for (size_t i = 0; i < segments.size(); ++i) {
    if (xy_length(segments[i].start.x - pierce.x,
                  segments[i].start.y - pierce.y) < kVertexEps) {
      containing = i;
      atEndpoint = true;
      break;
    }
    if (pierce_inside(segments[i])) {
      containing = i;
    }
  }
  if (containing == segments.size()) {
    // The pierce is not on the contour (should not happen — the
    // selectors return contour points) — rotate to the nearest
    // segment start as a safety net.
    size_t nearest = 0;
    double best = std::numeric_limits<double>::max();
    for (size_t i = 0; i < segments.size(); ++i) {
      const double d = xy_length(segments[i].start.x - pierce.x,
                                 segments[i].start.y - pierce.y);
      if (d < best) {
        best = d;
        nearest = i;
      }
    }
    containing = nearest;
    atEndpoint = true;
  }

  std::vector<OffsetSegment> contour;
  contour.reserve(segments.size() + (atEndpoint ? 0 : 1));
  OffsetSegment closingHead;
  bool hasClosingHead = false;
  for (size_t k = 0; k < segments.size(); ++k) {
    const OffsetSegment& segment = segments[(containing + k) % segments.size()];
    if (k == 0 && !atEndpoint) {
      // Split [start → end] into [pierce → end] (walk starts here)
      // and [start → pierce] (closes the loop at the end).
      OffsetSegment tail = segment;
      tail.start = pierce;
      contour.push_back(tail);
      closingHead = segment;
      closingHead.end = pierce;
      hasClosingHead = true;
      continue;
    }
    contour.push_back(segment);
  }
  if (hasClosingHead) {
    contour.push_back(closingHead);
  }
  return contour;
}

}  // namespace

std::optional<SketchFeatureParameters::SketchPlaneFrame> resolve_sketch_frame(
    const SketchFeatureParameters& sketch) {
  if (sketch.plane_frame.has_value()) {
    return sketch.plane_frame;
  }
  if (sketch.plane_id == "ref-plane-xy") {
    return SketchFeatureParameters::SketchPlaneFrame{
        .origin_x = 0.0, .origin_y = 0.0, .origin_z = 0.0,
        .x_axis_x = 1.0, .x_axis_y = 0.0, .x_axis_z = 0.0,
        .y_axis_x = 0.0, .y_axis_y = 1.0, .y_axis_z = 0.0,
        .normal_x = 0.0, .normal_y = 0.0, .normal_z = 1.0,
    };
  }
  if (sketch.plane_id == "ref-plane-yz") {
    return SketchFeatureParameters::SketchPlaneFrame{
        .origin_x = 0.0, .origin_y = 0.0, .origin_z = 0.0,
        .x_axis_x = 0.0, .x_axis_y = 1.0, .x_axis_z = 0.0,
        .y_axis_x = 0.0, .y_axis_y = 0.0, .y_axis_z = 1.0,
        .normal_x = 1.0, .normal_y = 0.0, .normal_z = 0.0,
    };
  }
  // xz: x=(1,0,0), y=(0,0,-1) gives x×y=(0,1,0)=normal — the frame
  // must stay right-handed or arc sweeps mirror in world space.
  if (sketch.plane_id == "ref-plane-xz") {
    return SketchFeatureParameters::SketchPlaneFrame{
        .origin_x = 0.0, .origin_y = 0.0, .origin_z = 0.0,
        .x_axis_x = 1.0, .x_axis_y = 0.0, .x_axis_z = 0.0,
        .y_axis_x = 0.0, .y_axis_y = 0.0, .y_axis_z = -1.0,
        .normal_x = 0.0, .normal_y = 1.0, .normal_z = 0.0,
    };
  }
  return std::nullopt;
}

CamGenerateResult generate_laser_cut_toolpath(
    const polysmith::core::CamGenerateContext& context) {
  CamGenerateResult result;
  const auto& op = context.operation;

  if (!op.parameters.laser.has_value()) {
    result.ok = false;
    result.error_message =
        "The laser operation is missing its laser parameters.";
    return result;
  }
  const auto& laser = op.parameters.laser.value();
  if (context.geometry.profiles.empty() &&
      context.geometry.faces.empty()) {
    result.ok = false;
    result.error_message =
        "Laser cut requires sketch profiles or a selected face.";
    return result;
  }
  // v1: every selected profile must belong to the same sketch plane.
  std::optional<SketchFeatureParameters::SketchPlaneFrame> frame;
  if (!context.geometry.profiles.empty()) {
    const SketchFeatureParameters* owningSketch =
        context.geometry.sketches.empty() ? nullptr
                                          : context.geometry.sketches[0];
    if (owningSketch == nullptr) {
      result.ok = false;
      result.error_message =
          "The sketch plane could not be resolved.";
      return result;
    }
    for (const auto* sketch : context.geometry.sketches) {
      if (sketch != owningSketch) {
        result.ok = false;
        result.error_message =
            "All laser profiles must lie on the same sketch plane.";
        return result;
      }
    }
    frame = resolve_sketch_frame(*owningSketch);
    if (!frame.has_value()) {
      result.ok = false;
      result.error_message = "The sketch plane could not be resolved.";
      return result;
    }
    // A tilted or vertical sketch plane cannot be followed by a
    // 2-axis gantry laser — reject it before generating garbage arcs.
    if (std::abs(frame->normal_z) < kMaxCutPlaneTilt) {
      result.ok = false;
      result.error_message =
          "Laser cuts must lie in a horizontal plane parallel to the "
          "machine bed; this sketch plane is not horizontal.";
      return result;
    }
  }
  // Left-handed in-plane frames mirror the contour into world XY,
  // which reverses every arc sweep.
  double xyOrientation = 1.0;
  if (frame.has_value()) {
    xyOrientation = frame->x_axis_x * frame->y_axis_y -
                    frame->x_axis_y * frame->y_axis_x;
  }

  if (laser.mode != "cut" && laser.mode != "score" &&
      laser.mode != "engrave") {
    result.ok = false;
    result.error_message =
        "Unknown laser mode '" + laser.mode +
        "' (expected cut, score, or engrave).";
    return result;
  }
  const bool engrave = laser.mode == "engrave";
  // cutting_direction: "conventional" flips the walk (and the kerf
  // sign with it); unknown values warn and behave as climb.
  bool conventional = false;
  if (op.parameters.cutting_direction == "conventional") {
    conventional = true;
  } else if (op.parameters.cutting_direction != "climb" &&
             op.parameters.cutting_direction != "mixed") {
    result.warnings.push_back(
        "Unknown cutting direction '" + op.parameters.cutting_direction +
        "' — treated as climb.");
  }

  // Kerf side policy: the cut line sits on the SCRAP side by default
  // (holes inward, outers outward).  "outside"/"inside" force one
  // side for every loop; "none" disables compensation.  The sign is
  // relative to the (orientation-normalized) walk — a conventional
  // cut reverses the walk, so the sign flips with it.
  const auto kerf_for = [&](bool is_hole) -> double {
    if (engrave || laser.kerf_side == "none") {
      return 0.0;
    }
    const double d = laser.kerf_width_mm / 2.0;
    double signedKerf = 0.0;
    if (laser.kerf_side == "outside") {
      signedKerf = is_hole ? -d : d;
    } else if (laser.kerf_side == "inside") {
      signedKerf = is_hole ? d : -d;
    } else {
      signedKerf = d;  // auto: right of the normalized walk = scrap side
    }
    return conventional ? -signedKerf : signedKerf;
  };

  // Lead side follows the kerf offset side: exterior offsets keep
  // tangent leads, interior offsets turn the lead into a spoke from
  // the loop centroid.  Recorded per loop at plan time, where the
  // walk handedness and the offset sign are both known.
  const auto note_kerf_side = [&](PlannedLoop& loop, double kerf) {
    const bool walkCw = loop.is_hole != conventional;
    loop.kerf_inside = (kerf > 0.0) == walkCw;
  };

  // Speed: laser-native mm/s when present; legacy feedrate fallback
  // (feedrate_mm_per_min = speed × 60).
  const double feedrate = [&]() {
    if (laser.speed_mm_per_s.has_value() &&
        laser.speed_mm_per_s.value() > 0.0) {
      return laser.speed_mm_per_s.value() * 60.0;
    }
    return op.parameters.feedrate_mm_per_min > 0.0
               ? op.parameters.feedrate_mm_per_min
               : context.tool.default_feedrate_mm_per_min;
  }();

  // ── Plan loops: per selected region, holes first, then the outer ─
  std::string firstSkipReason;
  std::vector<std::vector<PlannedLoop>> groups;
  for (size_t r = 0; r < context.geometry.profiles.size(); ++r) {
    const auto& region = *context.geometry.profiles[r].region;
    std::vector<PlannedLoop> group;

    // Outer loop.
    std::vector<BaseSegment> base;
    const bool exact = build_base_segments_from_edges(region, base);
    if (!exact) {
      result.warnings.push_back(
          "Profile contour is tessellated (no exact arcs).");
      build_base_segments_from_points(region.points, base);
    }
    // Standalone circle regions carry exact center/radius but no
    // boundary edges — synthesize a full-circle base segment.
    if (base.empty() &&
        (region.kind == "circle" || region.source_circle_id.has_value())) {
      BaseSegment full;
      full.is_arc = true;
      full.center = {region.center_x, region.center_y};
      full.radius = region.radius;
      full.start = {region.center_x + region.radius, region.center_y};
      full.end = full.start;
      full.ccw = true;
      base.push_back(full);
    }
    if (base.size() >= 2 ||
        (base.size() == 1 && base[0].is_arc)) {
      if (base.size() >= 2 && base_segments_signed_area(base) < 0) {
        reverse_segments(base);  // normalize: material on the left (CCW)
      }
      if (conventional) {
        reverse_segments(base);
      }
      std::vector<XY> basePoints;
      for (const auto& segment : base) {
        basePoints.push_back(segment.start);
      }
      // Circles report their center as the pierce anchor centroid.
      const XY centroid =
          base.size() == 1 && base[0].is_arc
              ? base[0].center
              : xy_centroid(basePoints);

      PlannedLoop loop;
      std::string error;
      const double outerKerf = kerf_for(/*is_hole=*/false);
      if (!plan_loop(base, outerKerf, /*is_hole=*/false, centroid, loop,
                     error)) {
        // Drop the whole region: cutting a region's holes after its
        // outline failed would separate material with no release cut.
        if (firstSkipReason.empty()) {
          firstSkipReason = error;
        }
        result.warnings.push_back("A profile contour was skipped: " + error);
        continue;
      }
      note_kerf_side(loop, outerKerf);
      group.push_back(std::move(loop));
    }

    // Hole loops.
    for (const auto& holePoints : region.inner_loops) {
      std::vector<BaseSegment> holeBase;
      build_base_segments_from_points(holePoints, holeBase);
      if (holeBase.size() < 2) {
        continue;
      }
      if (base_segments_signed_area(holeBase) > 0) {
        reverse_segments(holeBase);  // normalize: hole interior on the right
      }
      if (conventional) {
        reverse_segments(holeBase);
      }
      std::vector<XY> holePointList;
      for (const auto& segment : holeBase) {
        holePointList.push_back(segment.start);
      }
      PlannedLoop loop;
      std::string error;
      const double holeKerf = kerf_for(/*is_hole=*/true);
      if (!plan_loop(holeBase, holeKerf, /*is_hole=*/true,
                     xy_centroid(holePointList), loop, error)) {
        result.warnings.push_back("A hole contour was skipped: " + error);
        continue;
      }
      note_kerf_side(loop, holeKerf);
      group.push_back(std::move(loop));
    }

    if (!group.empty()) {
      for (auto& loop : group) {
        loop.group = groups.size();
      }
      groups.push_back(std::move(group));
    }
  }

  // ── Face-derived loops (laser from 3D geometry) ────────────────
  // A selected planar face cuts its boundary outline: the outer wire
  // plus every hole wire, projected at the face height.
  if (!context.geometry.faces.empty()) {
    for (const auto& faceRef : context.geometry.faces) {
      TopoDS_Face face;
      std::string faceError;
      if (!cam_planning::map_face_index(faceRef.body->shape,
                                        faceRef.faceIndex, face, faceError)) {
        result.ok = false;
        result.error_message = faceError;
        return result;
      }

      std::vector<std::vector<XY>> wireLoops;
      for (TopExp_Explorer wireExp(face, TopAbs_WIRE); wireExp.More();
           wireExp.Next()) {
        std::vector<XY> loop;
        if (cam_planning::sample_planar_wire(
                TopoDS::Wire(wireExp.Current()), /*chord_tolerance=*/0.05,
                loop)) {
          wireLoops.push_back(std::move(loop));
        }
      }
      if (wireLoops.empty()) {
        result.ok = false;
        result.error_message = "The face boundary could not be sampled.";
        return result;
      }
      // Cut plane: the face's own height (planar face) plus the
      // offset.  A face that is not flat on the machine bed cannot be
      // followed by a 2-axis gantry laser.
      double faceZ = 0.0;
      {
        gp_Pnt center;
        gp_Vec normal;
        if (!cam_planning::face_cut_plane(face, center, normal)) {
          result.ok = false;
          result.error_message = "The referenced face is degenerate.";
          return result;
        }
        faceZ = center.Z();
        const double norm = normal.Magnitude();
        if (std::abs(normal.Z() / norm) < kMaxCutPlaneTilt) {
          result.ok = false;
          result.error_message =
              "Laser cuts must lie in a horizontal plane parallel to the "
              "machine bed; this face is not horizontal.";
          return result;
        }
      }

      // The largest loop is the outer boundary; the rest are holes.
      std::sort(wireLoops.begin(), wireLoops.end(),
                [](const auto& a, const auto& b) {
                  return std::abs(xy_signed_area(a)) >
                         std::abs(xy_signed_area(b));
                });

      bool faceGroupDropped = false;
      std::vector<PlannedLoop> faceGroup;
      for (size_t w = 0; w < wireLoops.size(); ++w) {
        const bool isHole = w > 0;
        std::vector<BaseSegment> base;
        for (size_t i = 0; i < wireLoops[w].size(); ++i) {
          const auto& a = wireLoops[w][i];
          const auto& b = wireLoops[w][(i + 1) % wireLoops[w].size()];
          BaseSegment segment;
          segment.start = a;
          segment.end = b;
          base.push_back(segment);
        }
        // Normalize: outer CCW (material left), holes CW.
        const double area = base_segments_signed_area(base);
        if ((!isHole && area < 0) || (isHole && area > 0)) {
          reverse_segments(base);
        }
        if (conventional) {
          reverse_segments(base);
        }
        PlannedLoop loop;
        std::string error;
        const double faceKerf = kerf_for(isHole);
        if (!plan_loop(base, faceKerf, isHole,
                       xy_centroid(wireLoops[w]), loop, error)) {
          // A failed outer boundary drops the whole face; a failed
          // hole drops only that hole.
          if (!isHole) {
            if (firstSkipReason.empty()) {
              firstSkipReason = error;
            }
            result.warnings.push_back(
                "A face outline contour was skipped: " + error);
            faceGroupDropped = true;
            break;
          }
          result.warnings.push_back(
              "A face hole contour was skipped: " + error);
          continue;
        }
        note_kerf_side(loop, faceKerf);
        loop.isWorldXY = true;
        loop.worldZ = faceZ + laser.cut_plane_offset_mm;
        faceGroup.push_back(std::move(loop));
      }
      if (!faceGroupDropped && !faceGroup.empty()) {
        for (auto& loop : faceGroup) {
          loop.group = groups.size();
        }
        groups.push_back(std::move(faceGroup));
      }
    }
  }

  if (groups.empty()) {
    result.ok = false;
    // Surface the first per-loop failure — it names the actual cause
    // (e.g. a feature narrower than the kerf).
    result.error_message = firstSkipReason.empty()
                               ? "No cuttable profile loops were found."
                               : firstSkipReason;
    return result;
  }

  // ── Nesting tree + cut ordering ────────────────────────────────
  // Flatten the groups into one list (loops carry their region group
  // index), compute the containment tree per coordinate domain, then
  // sequence with the op's cut_order strategy.
  std::vector<PlannedLoop> ordered;
  for (auto& group : groups) {
    for (auto& loop : group) {
      ordered.push_back(std::move(loop));
    }
  }

  // Smallest enclosing loop wins; depths resolve parents first by
  // ascending area.  Sketch loops share the sketch plane; face loops
  // live in world XY — containment only applies within one domain.
  const auto compute_nesting = [](std::vector<PlannedLoop>& loops) {
    std::vector<size_t> byArea(loops.size());
    for (size_t i = 0; i < byArea.size(); ++i) {
      byArea[i] = i;
    }
    std::sort(byArea.begin(), byArea.end(), [&](size_t a, size_t b) {
      return loops[a].area < loops[b].area;
    });
    for (const size_t i : byArea) {
      size_t parent = kNoParent;
      double bestArea = std::numeric_limits<double>::max();
      for (size_t j = 0; j < loops.size(); ++j) {
        if (j == i || loops[j].area <= loops[i].area ||
            loops[j].isWorldXY != loops[i].isWorldXY) {
          continue;
        }
        if (cam2d::loop_contains(loops[j].samples, loops[i].samples) &&
            loops[j].area < bestArea) {
          bestArea = loops[j].area;
          parent = j;
        }
      }
      loops[i].parent = parent;
      loops[i].depth = parent == kNoParent ? 0 : loops[parent].depth + 1;
    }
    // A loop is a hole when its enclosing loop is its own region
    // group's outer.  A loop enclosed by ANOTHER group's contour is a
    // separate part (nested-but-distinct), never a hole.
    for (auto& loop : loops) {
      loop.is_hole =
          loop.parent != kNoParent && !loops[loop.parent].is_hole &&
          loops[loop.parent].group == loop.group;
    }
  };
  compute_nesting(ordered);

  // Warn once when a selected loop coincides with another region's
  // loop ring (nested-but-separate profiles are legitimately distinct
  // parts; only coincident geometry is genuinely cut twice).
  // `hole_signature`/`matches_hole` at capture time stay the single
  // hole-matching implementation for attestation; this generate-time
  // scan surfaces the same condition on the planned loops.
  {
    int duplicates = 0;
    for (size_t i = 0; i < ordered.size(); ++i) {
      for (size_t j = i + 1; j < ordered.size(); ++j) {
        if (ordered[i].group == ordered[j].group) {
          continue;
        }
        const double radius = std::sqrt(
            std::max(std::max(ordered[i].area, ordered[j].area), 1e-9) /
            cam2d::kPi);
        const double distance =
            xy_length(ordered[i].centroid.x - ordered[j].centroid.x,
                      ordered[i].centroid.y - ordered[j].centroid.y);
        const double ratio =
            ordered[i].area / std::max(ordered[j].area, 1e-9);
        if (distance < 0.05 + 0.02 * radius &&
            std::abs(ratio - 1.0) < 0.25) {
          ++duplicates;
          break;
        }
      }
    }
    if (duplicates > 0) {
      result.warnings.push_back(
          std::to_string(duplicates) +
          " selected profile(s) duplicate holes of other regions — they may "
          "be cut twice.");
    }
  }

  // Cut-order strategy (default: inner_first).
  std::string cutOrder = laser.cut_order;
  if (cutOrder != "inner_first" && cutOrder != "nearest_neighbor" &&
      cutOrder != "by_area") {
    result.warnings.push_back("Unknown cut order '" + cutOrder +
                              "' — treated as inner_first.");
    cutOrder = "inner_first";
  }
  order_laser_loops(ordered, cutOrder);

  // ── Emit moves ──────────────────────────────────────────────────
  Toolpath toolpath;
  toolpath.op_id = op.op_id;
  // Pinned arc segment count flows to the viewport linearizer and the
  // linearized post; 0 keeps the chord-tolerance default.
  toolpath.arc_segments_per_circle = laser.arc_segments_per_circle;
  const double cut_plane_offset = laser.cut_plane_offset_mm;

  const double leadIn = engrave ? 0.0 : laser.lead_in_mm;
  const double leadOut = engrave ? 0.0 : laser.lead_out_mm;
  const double dwell = engrave ? 0.0 : laser.pierce_dwell_seconds;
  const int passCount = std::max(1, laser.passes);

  int skippedTooShort = 0;
  int skippedDegenerate = 0;

  if (engrave && laser.engrave_style == "fill") {
    // ── Engrave fill / hatch (M7) ───────────────────────────────
    // Regions are hatched with scan lines instead of traced; the
    // region order follows the cut sequence.  Bidirectional lines
    // keep the laser on across the turn between lines; jumps across
    // a hole within one line travel with the laser off.
    std::vector<std::vector<PlannedLoop>> fillGroups;
    for (const auto& loop : ordered) {
      if (fillGroups.empty() || fillGroups.back()[0].group != loop.group) {
        fillGroups.push_back({loop});
      } else {
        fillGroups.back().push_back(loop);
      }
    }
    for (const auto& group : fillGroups) {
      const auto hatchLines = hatch_region(group, laser);
      if (hatchLines.empty()) {
        result.warnings.push_back(
            "A region produced no fill lines (check the line spacing).");
        continue;
      }
      const PlannedLoop& rep = group.front();
      const auto toWorld = [&](const XY& p) {
        return rep.isWorldXY ? XY{p.x, p.y} : world_point(frame.value(), p);
      };
      const auto toZ = [&](const XY& p) {
        return rep.isWorldXY ? rep.worldZ
                             : world_z(frame.value(), p, cut_plane_offset);
      };
      const auto appendFillMove = [&](const XY& end, bool laserOn) {
        const XY w = toWorld(end);
        ToolpathMove move;
        move.kind = ToolpathMoveKind::FeedLinear;
        move.x = w.x;
        move.y = w.y;
        move.z = toZ(end);
        move.feedrate_mm_per_min = feedrate;
        move.power_percent = laser.power_percent;
        move.laser_on = laserOn;
        move.dwell_seconds = 0.0;
        toolpath.moves.push_back(move);
      };
      // Travel to the first span (laser off).
      appendFillMove(hatchLines.front().front().start, /*laserOn=*/false);
      for (size_t li = 0; li < hatchLines.size(); ++li) {
        auto spans = hatchLines[li];
        if (laser.fill_bidirectional && li % 2 == 1) {
          // Reverse the line: walk the spans right-to-left.
          std::reverse(spans.begin(), spans.end());
          for (auto& span : spans) {
            std::swap(span.start, span.end);
          }
        }
        for (size_t si = 0; si < spans.size(); ++si) {
          appendFillMove(spans[si].end, /*laserOn=*/true);
          if (si + 1 < spans.size()) {
            // Jump across a hole within this line — laser off.
            appendFillMove(spans[si + 1].start, /*laserOn=*/false);
          }
        }
        if (li + 1 < hatchLines.size()) {
          const auto& next = hatchLines[li + 1];
          const XY nextStart =
              (laser.fill_bidirectional && (li + 1) % 2 == 1)
                  ? next.back().end
                  : next.front().start;
          // Between lines: bidirectional cuts the turn; one-way
          // travels back with the laser off.
          appendFillMove(nextStart,
                         /*laserOn=*/laser.fill_bidirectional);
        }
      }
    }
  } else {
  for (auto& loop : ordered) {
    // Loop-coordinate → world mapping: sketch loops go through the
    // sketch plane frame; face loops are already world XY at a fixed
    // cut-plane height.
    // Sketch loops only exist when a sketch frame resolved above.
    const auto toWorld = [&](const XY& p) {
      return loop.isWorldXY ? XY{p.x, p.y} : world_point(frame.value(), p);
    };
    const auto toZ = [&](const XY& p) {
      return loop.isWorldXY ? loop.worldZ
                            : world_z(frame.value(), p, cut_plane_offset);
    };
    const auto append_rapid = [&](const XY& target) {
      const XY w = toWorld(target);
      ToolpathMove move;
      move.kind = ToolpathMoveKind::Rapid;
      move.x = w.x;
      move.y = w.y;
      move.z = toZ(target);
      move.feedrate_mm_per_min = 0.0;
      move.power_percent = 0.0;
      move.laser_on = false;
      toolpath.moves.push_back(move);
    };
    // Emits one segment (line or arc) as a feed move.
    const auto append_segment = [&](const OffsetSegment& segment,
                                    double power, bool laserOn) {
      const XY wEnd = toWorld(segment.end);
      ToolpathMove move;
      move.feedrate_mm_per_min = feedrate;
      move.power_percent = power;
      move.laser_on = laserOn;
      move.dwell_seconds = 0.0;
      move.z = toZ(segment.end);
      move.x = wEnd.x;
      move.y = wEnd.y;
      if (segment.is_arc) {
        // G2/G3 live in machine XY: map center and start to world and
        // emit the world-space center offset.  A left-handed sketch
        // frame mirrors the contour and reverses the arc sweep.
        const XY wCenter = toWorld(segment.center);
        const XY wStart = toWorld(segment.start);
        const bool worldCw =
            segment.cw != (!loop.isWorldXY && xyOrientation < 0.0);
        move.kind = worldCw ? ToolpathMoveKind::FeedArcCW
                            : ToolpathMoveKind::FeedArcCCW;
        move.i = wCenter.x - wStart.x;
        move.j = wCenter.y - wStart.y;
      } else {
        move.kind = ToolpathMoveKind::FeedLinear;
      }
      toolpath.moves.push_back(move);
    };
    const auto append_linear = [&](const XY& end, bool laserOn, double power,
                                   double dwellSeconds) {
      const XY w = toWorld(end);
      ToolpathMove move;
      move.kind = ToolpathMoveKind::FeedLinear;
      move.x = w.x;
      move.y = w.y;
      move.z = toZ(end);
      move.feedrate_mm_per_min = feedrate;
      move.power_percent = power;
      move.laser_on = laserOn;
      move.dwell_seconds = dwellSeconds;
      toolpath.moves.push_back(move);
    };

    const auto& samples = loop.samples;
    if (samples.size() < 3) {
      ++skippedDegenerate;
      continue;
    }
    if (!engrave && loop.length < 2.0 * (leadIn + leadOut)) {
      ++skippedTooShort;
      continue;
    }

    // Pierce vertex + lead geometry (M5): corner-aware placement,
    // line/arc lead styles, overcut.  A pinned lead-side angle pierces
    // where its ray crosses the contour; otherwise placement follows
    // pierce_position.  The contour is rotated (and split when the
    // pierce lands mid-segment) so it starts AND ends at the pierce —
    // the leads hang off its exact boundary tangents.
    const XY pierce =
        laser.pierce_angle_deg.has_value()
            ? select_pierce_at_angle(loop, laser.pierce_angle_deg.value(),
                                     laser, result.warnings)
            : select_pierce_vertex(loop, laser, result.warnings);
    const auto contour = contour_starting_at(loop.segments, pierce);
    // The lead comes from the kerf side: exterior offsets keep the
    // tangent lead, an interior offset turns it into a spoke from the
    // loop centroid.
    const auto leadInSegments =
        build_lead_in(contour, pierce, laser, loop.centroid, loop.kerf_inside);
    const auto leadOutSegments =
        build_lead_out(contour, pierce, laser, loop.centroid, loop.kerf_inside);
    const XY leadInStart = leadInSegments.empty()
                               ? pierce
                               : leadInSegments.front().start;

    // Rapid to the lead entry (laser off), pierce in place: laser on,
    // dwell — on the kerf side, off the contour.
    append_rapid(leadInStart);
    append_linear(leadInStart, /*laserOn=*/true, laser.power_percent, dwell);
    // Lead-in cut.
    for (const auto& segment : leadInSegments) {
      append_segment(segment, laser.power_percent, /*laserOn=*/true);
    }

    // Contour, starting at the pierce.  Repeated `passes` times with
    // the laser staying on — LightBurn-style re-cut, no re-pierce
    // between passes.  Tabs (M6) split the rotated contour at tab
    // boundaries, landing relative to the pierce seam — never on
    // leads.
    PlannedLoop contourLoop = loop;
    contourLoop.segments = contour;
    const auto tabbed = apply_loop_tabs(contourLoop, laser, result.warnings);
    if (tabbed.empty()) {
      for (int pass = 0; pass < passCount; ++pass) {
        for (const auto& segment : contour) {
          append_segment(segment, laser.power_percent, /*laserOn=*/true);
        }
      }
    } else {
      for (int pass = 0; pass < passCount; ++pass) {
        for (const auto& piece : tabbed) {
          const bool cutOn = !piece.in_tab || laser.tab_power_percent > 0.0;
          append_segment(piece.segment,
                         piece.in_tab ? laser.tab_power_percent
                                      : laser.power_percent,
                         cutOn);
        }
      }
    }

    // Lead-out cut.
    for (const auto& segment : leadOutSegments) {
      append_segment(segment, laser.power_percent, /*laserOn=*/true);
    }
  }
  }

  if (toolpath.moves.empty()) {
    result.ok = false;
    result.error_message = "No cuttable contours were produced.";
    return result;
  }

  if (laser.mode != "engrave" && laser.material_thickness_mm > 6.0 &&
      laser.power_percent < 60.0) {
    result.warnings.push_back(
        "The material is " + std::to_string(laser.material_thickness_mm) +
        " mm thick at " + std::to_string(laser.power_percent) +
        "% power — the cut may not penetrate.");
  }
  if (skippedTooShort > 0) {
    result.warnings.push_back(
        std::to_string(skippedTooShort) +
        " contour(s) are shorter than the lead allowance and were skipped "
        "(reduce lead-in/lead-out to cut them).");
  }
  if (skippedDegenerate > 0) {
    result.warnings.push_back(
        std::to_string(skippedDegenerate) +
        " contour(s) were too small to sample and were skipped.");
  }

  result.toolpath = std::move(toolpath);
  return result;
}

}  // namespace polysmith::core::laser
