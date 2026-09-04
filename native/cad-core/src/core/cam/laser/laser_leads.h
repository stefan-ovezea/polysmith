#pragma once

// ── Laser leads + pierce placement ───────────────────────────────
//
// Picks the pierce vertex for one loop and builds the lead-in /
// lead-out geometry.  Leads are emitted as ordinary move segments
// (straight or tangent arcs); the pierce dwell stays on the laser-on
// move at the lead entry — on the kerf side, off the contour.
//
// Angle convention (LightBurn): 0° = tangent continuation, 90° =
// perpendicular to the contour tangent at the pierce vertex.

#include <string>
#include <vector>

#include "core/cam/cam2d.h"
#include "core/cam/cam_types.h"
#include "core/cam/laser/laser_generate.h"

namespace polysmith::core::laser {

// Corners with a sharper INTERIOR angle are skipped as pierce
// candidates — the beam would dwell on the corner.
inline constexpr double kMinPierceCornerAngleDeg = 60.0;

// Resolves the pierce vertex for one loop.  Candidates are the loop's
// vertices (segment starts — never mid-arc).  Selection:
//   "auto" / "nearest_centroid" — the qualifying vertex nearest the
//                                  loop centroid;
//   "lead_start"               — the qualifying vertex nearest the
//                                  machine origin (the incoming
//                                  approach).
// When every vertex is too sharp, falls back to the raw nearest
// vertex and warns.
cam2d::XY select_pierce_vertex(const PlannedLoop& loop,
                               const LaserCutParameters& params,
                               std::vector<std::string>& warnings);

// Pierces the loop where the ray from the loop centroid at
// `angle_deg` (loop-local, CCW from sketch +X) first crosses the
// contour — the lead-side control.  Falls back to
// select_pierce_vertex with a warning when the ray misses the
// contour entirely (e.g. the centroid lies outside a concave loop).
cam2d::XY select_pierce_at_angle(const PlannedLoop& loop, double angle_deg,
                                 const LaserCutParameters& params,
                                 std::vector<std::string>& warnings);

// Lead-in segment list from the pierce vertex:
//   "line" — a straight segment entering at lead_in_angle_deg to the
//            contour tangent;
//   "arc"  — a 90° tangent roll-in arc of radius lead_in_mm.
// `contour` is the cut walk already rotated (and split) so it starts
// and ends at the pierce; the lead tangents come from its exact
// boundary segments.  Empty when lead_in_mm <= 0 or the mode is
// engrave (leads disabled).
//
// `interior` follows the kerf offset side: false = exterior offset
// (tangent lead — a tangent line lies outside the contour); true =
// interior offset (the lead is a spoke along the pierce→`centroid`
// ray — perpendicular to the contour on circles — because a tangent
// line cannot lie inside a closed contour).  `lead_in_angle_deg`
// rotates the spoke relative to that ray.
std::vector<cam2d::OffsetSegment> build_lead_in(
    const std::vector<cam2d::OffsetSegment>& contour,
    const cam2d::XY& pierce, const LaserCutParameters& params,
    const cam2d::XY& centroid, bool interior);

// Lead-out segment list leaving the pierce vertex along the exit
// tangent (exterior side) or back INTO the interior along the spoke
// (interior side — the lead retreats into the kerf side).  `overcut_mm`
// extends the cut PAST the pierce vertex along the tangent (guaranteed
// joint separation): a straight overcut segment for the "arc" style,
// folded into the straight lead for the "line" style.
std::vector<cam2d::OffsetSegment> build_lead_out(
    const std::vector<cam2d::OffsetSegment>& contour,
    const cam2d::XY& pierce, const LaserCutParameters& params,
    const cam2d::XY& centroid, bool interior);

}  // namespace polysmith::core::laser
