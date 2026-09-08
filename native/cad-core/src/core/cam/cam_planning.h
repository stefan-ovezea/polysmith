#pragma once

// ── OCCT-dependent CAM planning helpers ──────────────────────────
//
// Wire sampling (chord-tolerance-based), face cut-plane probing, and
// the shared face-reference index guard.  The laser and face-milling
// generators both consume these (they previously each carried private
// copies).

#include <optional>
#include <string>
#include <vector>

#include "core/cam/cam2d.h"
#include "core/cam/cam_types.h"
#include "core/sketch/sketch_feature_parameters.h"
#include "core/sketch/sketch_profile_types.h"

#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

namespace polysmith::core::cam_planning {

// cos(5°): a face steeper than this is not machinable/laserable as a
// horizontal 2.5D feature.
constexpr double kMaxUpwardFaceTilt = 0.9962;

// Plans the 2.5D roughing levels shared by the face-milling and pocket
// generators: the first cut at stockTop − stepdown, descending by
// stepdown, with the last level pinned to faceZ (stock 23 / face 20 /
// stepdown 2 → 21, 20).  extra_levels (island top heights) strictly
// between faceZ and stockTop join the descending plan, deduped within
// eps — they clear the stock above a short island down to its top.
// No stepdown, a non-positive stepdown, or a stock top at/below the
// face = the legacy single pass at the face.  Capped at 100 planned
// levels (the pinned faceZ can make 101 cuts) with a warning appended.
// Returns false when the single-pass fallback applied (no multi-pass
// possible for the given inputs).
bool plan_stepdown_levels(double stockTopZ, double faceZ, double stepdown,
                          const std::vector<double>& extra_levels,
                          std::vector<double>& levels,
                          std::vector<std::string>& warnings);

// World Z of the setup's retract plane.  `retract_height` is a
// WCS-relative value (millimeters of machine Z above the setup
// origin); the toolpath is emitted in WORLD coordinates, so the
// plane sits at origin Z + height.  The post-processor subtracts the
// origin again (machine = world − wcs_origin), landing the rapids
// exactly at `retract_height` in machine Z.  An unresolved origin
// (never refreshed / legacy documents) resolves to {0,0,0}, matching
// the pre-WCS behavior.
double setup_retract_plane_z(const CamSetup& setup);

// Samples one planar wire into a closed polyline of its X/Y
// coordinates.  Every edge is sampled adaptively so the chord height
// (sagitta) stays within `chord_tolerance`; the pieces are then
// chained into one loop by endpoint matching (wire edge orientations
// vary, so pieces may arrive reversed — match-and-flip handles both).
// Returns false when the sampling produced fewer than 3 points.
bool sample_planar_wire(const TopoDS_Wire& wire, double chord_tolerance,
                        std::vector<cam2d::XY>& out_loop);

// ── Exact base-segment builders (laser + contour generators) ────────
//
// Build the exact base loop for a sketch profile region or a body
// wire (exact line/circle segments where possible, sampled-point
// fallback otherwise).  Hoisted out of laser_generate.cpp so the
// contour generator shares them.

// Builds exact base segments from a profile region's boundary_edges.
// When any edge is an ellipse or spline (no exact offset), returns
// false — the caller falls back to sampled points.
bool build_base_segments_from_edges(const SketchProfileRegion& region,
                                    std::vector<cam2d::BaseSegment>& out);

// Sampled-polygon fallback (legacy profiles without exact edges).
// Assumes the points follow the walk orientation.
void build_base_segments_from_points(
    const std::vector<SketchProfilePoint>& points,
    std::vector<cam2d::BaseSegment>& out);

// Builds exact base segments from a world-space planar wire: straight
// edges become line segments, circular edges become arcs (a full
// circle is a start==end segment with ccw=true).  Any other curve
// type (ellipse, spline) returns false so the caller falls back to
// sample_planar_wire + a polyline build.  Edges are chained by
// endpoint matching — pieces may arrive reversed, and flipped arcs
// invert their sweep direction.
bool build_base_segments_from_wire(const TopoDS_Wire& wire,
                                   std::vector<cam2d::BaseSegment>& out);

// World point of a sketch-local 2D point on the sketch plane.
cam2d::XY world_point(
    const SketchFeatureParameters::SketchPlaneFrame& frame,
    const cam2d::XY& p);

// World Z of a sketch-local 2D point offset along the plane normal
// by `cut_plane_offset`.
double world_z(const SketchFeatureParameters::SketchPlaneFrame& frame,
               const cam2d::XY& p, double cut_plane_offset);

// Sketch frames: cached plane_frame when present, else the hardcoded
// origin-plane frame table (sketches on origin planes may not cache a
// frame — the history dependency pass resolves it on demand).
std::optional<SketchFeatureParameters::SketchPlaneFrame>
resolve_sketch_frame(const SketchFeatureParameters& sketch);

// Mid-UV point and (unnormalized, raw-parameterization) surface normal
// of a face.  Returns false for degenerate surfaces.  Callers apply
// their own orientation and tilt policy.
bool face_cut_plane(const TopoDS_Face& face, gp_Pnt& out_point,
                    gp_Vec& out_normal);

// Resolves a face reference index against a live body shape.
// Returns false (with an actionable message) for stale indices.
bool map_face_index(const TopoDS_Shape& body, int face_index,
                    TopoDS_Face& out_face, std::string& error_message);

}  // namespace polysmith::core::cam_planning
