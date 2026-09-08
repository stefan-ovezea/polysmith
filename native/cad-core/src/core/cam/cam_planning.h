#pragma once

// ── OCCT-dependent CAM planning helpers ──────────────────────────
//
// Wire sampling (chord-tolerance-based), face cut-plane probing, and
// the shared face-reference index guard.  The laser and face-milling
// generators both consume these (they previously each carried private
// copies).

#include <string>
#include <vector>

#include "core/cam/cam2d.h"

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

// Samples one planar wire into a closed polyline of its X/Y
// coordinates.  Every edge is sampled adaptively so the chord height
// (sagitta) stays within `chord_tolerance`; the pieces are then
// chained into one loop by endpoint matching (wire edge orientations
// vary, so pieces may arrive reversed — match-and-flip handles both).
// Returns false when the sampling produced fewer than 3 points.
bool sample_planar_wire(const TopoDS_Wire& wire, double chord_tolerance,
                        std::vector<cam2d::XY>& out_loop);

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
