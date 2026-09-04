#pragma once

// ── Shared 2D CAM math ───────────────────────────────────────────
//
// Pure planar geometry used by the laser kerf offset and the
// face-milling inset: closed-loop offsetting with round/miter corner
// handling, loop sampling, clipping, and containment.  No OCCT — the
// generators feed it sketch-local or world-XY points interchangeably.
//
// Historically these lived as preprocessor-included .inc files
// duplicated into laser_cut.cpp and face_milling.cpp; they are now a
// single compiled unit.

#include <vector>

namespace polysmith::core::cam2d {

struct XY {
  double x;
  double y;
};

constexpr double kOffsetEps = 1e-9;
constexpr double kPi = 3.14159265358979323846;

double xy_length(double dx, double dy);

// Signed shoelace area (positive = counter-clockwise walk).
double xy_signed_area(const std::vector<XY>& points);

// Vertex average (pierce anchoring).
XY xy_centroid(const std::vector<XY>& points);

// Area-weighted (shoelace) centroid; falls back to the vertex
// average for degenerate point sets.
XY xy_area_centroid(const std::vector<XY>& points);

// Unit normal pointing RIGHT of the direction (dx, dy): (dy, -dx)/len.
// Rotating a direction by -90° (clockwise).  With the loop normalized
// so material is on the left of the walk, right is always the scrap
// side.
XY right_normal(double dx, double dy);

// Proper intersection of segments (a1,a2) and (b1,b2): shared
// endpoints do NOT count (offset corners legitimately touch).
bool xy_segments_intersect(const XY& a1, const XY& a2, const XY& b1,
                           const XY& b2);

// Distance from point p to segment (a,b).
double xy_point_segment_distance(const XY& p, const XY& a, const XY& b);

// Sutherland–Hodgman clip of a segment against a CCW polygon (inside
// = left of each edge).  Returns the clipped point list (0, 1, or 2
// points).
std::vector<XY> clip_segment_to_polygon(XY p1, XY p2,
                                        const std::vector<XY>& poly);

// True when every vertex of `inner` lies inside `outer` AND the inner
// centroid is strictly inside (bbox precheck + ray crossing).  Used
// for loop nesting depth.
bool loop_contains(const std::vector<XY>& outer, const std::vector<XY>& inner);

struct OffsetSegment {
  bool is_arc = false;
  XY start;
  XY end;
  XY center;            // arc only
  double radius = 0.0;  // arc only
  bool cw = false;      // arc walk direction around the center
  bool is_join = false; // corner join arc (radius == d)
};

struct BaseSegment {
  bool is_arc = false;
  XY start;
  XY end;
  XY center;
  double radius = 0.0;
  bool ccw = false;  // true = counter-clockwise around the center
};

// Reverses a segment list in place (swap endpoints, flip the arc walk
// direction).  Used to normalize walk orientation.
void reverse_segments(std::vector<BaseSegment>& segments);

// Signed area of a segment list (shoelace over the endpoints).
double base_segments_signed_area(const std::vector<BaseSegment>& segments);

// Offsets one closed, orientation-normalized loop by `d` to the right
// of the walk (the scrap side).  Corner handling:
//   - round_joins=true (laser kerf on the scrap side): round join arcs
//     at steep corners; miter trims at shallow corners.
//   - round_joins=false (face-milling inset into the material): every
//     edge is rebuilt between its two adjacent MITER points — a round
//     join would cut into the material corner.
// Returns false for an empty input loop.
bool offset_closed_loop(const std::vector<BaseSegment>& base, double d,
                        std::vector<OffsetSegment>& out,
                        bool round_joins = true);

// Total length of an offset loop.
double offset_loop_length(const std::vector<OffsetSegment>& segments);

// Signed sweep angle of one offset arc segment (positive = CCW,
// negative = CW), with the full-circle special case resolved from the
// walk direction.  Returns 0.0 for a non-arc segment.
double offset_arc_sweep(const OffsetSegment& segment);

// True when `p` lies on the arc segment's swept span (on the circle,
// angle within the signed sweep).  Full circles contain every point
// on their circumference.
bool offset_arc_contains_point(const OffsetSegment& segment, const XY& p);

// Samples the loop into small straight pieces (for self-intersection
// scanning and any consumer needing a polyline).
std::vector<XY> sample_offset_loop(const std::vector<OffsetSegment>& segments,
                                   double tolerance);

// O(n²) proper-intersection scan over the sampled loop.  Adjacent
// pieces legitimately share endpoints; a crossing anywhere else means
// a feature narrower than the offset collapsed the loop.
bool offset_loop_self_intersects(const std::vector<XY>& samples);

}  // namespace polysmith::core::cam2d
