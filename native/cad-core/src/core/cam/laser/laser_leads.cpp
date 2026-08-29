#include "core/cam/laser/laser_leads.h"

#include <cmath>
#include <limits>

namespace polysmith::core::laser {

namespace {

using cam2d::XY;
using cam2d::kPi;
using cam2d::right_normal;
using cam2d::xy_length;

// Tangent directions at a vertex of the sampled contour (walk order,
// with wrap).  u_out = direction leaving the vertex along the walk;
// u_in = direction arriving at the vertex.
void vertex_tangents(const PlannedLoop& loop, size_t vertexIndex, XY& u_out,
                     XY& u_in) {
  const auto& samples = loop.samples;
  const size_t n = samples.size();
  const XY& v = samples[vertexIndex];
  const XY out{samples[(vertexIndex + 1) % n].x - v.x,
               samples[(vertexIndex + 1) % n].y - v.y};
  const XY in{v.x - samples[(vertexIndex - 1 + n) % n].x,
              v.y - samples[(vertexIndex - 1 + n) % n].y};
  const double outLen = xy_length(out.x, out.y);
  const double inLen = xy_length(in.x, in.y);
  u_out = outLen > 1e-12 ? XY{out.x / outLen, out.y / outLen} : XY{1.0, 0.0};
  u_in = inLen > 1e-12 ? XY{in.x / inLen, in.y / inLen} : XY{1.0, 0.0};
}

// Interior (material-side) angle in radians at the vertex.
double interior_angle(const PlannedLoop& loop, size_t vertexIndex) {
  XY u_out;
  XY u_in;
  vertex_tangents(loop, vertexIndex, u_out, u_in);
  const double turn = std::atan2(u_in.x * u_out.y - u_in.y * u_out.x,
                                 u_in.x * u_out.x + u_in.y * u_out.y);
  return kPi - turn;  // left turns (convex) shrink the interior
}

// Sample index of a segment-start vertex.
size_t sample_index_of(const PlannedLoop& loop, const XY& vertex) {
  for (size_t i = 0; i < loop.samples.size(); ++i) {
    if (xy_length(loop.samples[i].x - vertex.x,
                  loop.samples[i].y - vertex.y) < 1e-9) {
      return i;
    }
  }
  return 0;
}

XY rotate_by(const XY& v, double radians) {
  const double c = std::cos(radians);
  const double s = std::sin(radians);
  return XY{v.x * c - v.y * s, v.x * s + v.y * c};
}

// Straight lead segment from `entry` to `end`.
cam2d::OffsetSegment line_segment(const XY& entry, const XY& end) {
  cam2d::OffsetSegment segment;
  segment.is_arc = false;
  segment.start = entry;
  segment.end = end;
  return segment;
}

// 90° tangent roll arc: center at `vertex + radius * right(tangent)`,
// walking from `start` to `end` with the given rotation sign.
cam2d::OffsetSegment roll_arc(const XY& vertex, const XY& tangent,
                              double radius, const XY& start, const XY& end,
                              bool cw) {
  cam2d::OffsetSegment segment;
  segment.is_arc = true;
  segment.is_join = false;
  segment.center = {vertex.x + tangent.x * radius,
                    vertex.y + tangent.y * radius};
  segment.radius = radius;
  segment.start = start;
  segment.end = end;
  segment.cw = cw;
  return segment;
}

}  // namespace

cam2d::XY select_pierce_vertex(const PlannedLoop& loop,
                               const LaserCutParameters& params,
                               std::vector<std::string>& warnings) {
  // Candidate vertices: segment starts in walk order.
  std::vector<XY> candidates;
  for (const auto& segment : loop.segments) {
    candidates.push_back(segment.start);
  }
  if (candidates.empty()) {
    return loop.centroid;
  }

  // Qualifying vertices: interior angle at least the corner threshold.
  std::vector<bool> qualifies(candidates.size(), false);
  int sharpCount = 0;
  for (size_t i = 0; i < candidates.size(); ++i) {
    const size_t sampleIndex = sample_index_of(loop, candidates[i]);
    const double interior = interior_angle(loop, sampleIndex);
    qualifies[i] =
        interior >= kMinPierceCornerAngleDeg * kPi / 180.0;
    if (!qualifies[i]) {
      ++sharpCount;
    }
  }
  if (sharpCount > 0) {
    warnings.push_back(
        std::to_string(sharpCount) +
        " sharp corner(s) were excluded from pierce placement.");
  }

  // Reference point per pierce_position; "auto" and
  // "nearest_centroid" anchor on the centroid, "lead_start" on the
  // machine-origin approach.
  const XY reference =
      params.pierce_position == "lead_start" ? XY{0.0, 0.0}
                                             : loop.centroid;

  size_t best = candidates.size();
  double bestDistance = std::numeric_limits<double>::max();
  for (size_t i = 0; i < candidates.size(); ++i) {
    const double d = xy_length(candidates[i].x - reference.x,
                               candidates[i].y - reference.y);
    if (d < bestDistance) {
      bestDistance = d;
      best = i;
    }
  }
  if (best < candidates.size() && qualifies[best]) {
    return candidates[best];
  }
  // The nearest vertex is sharp — prefer the nearest QUALIFYING
  // vertex; fall back to the raw nearest when every vertex is sharp.
  double qualifyingDistance = std::numeric_limits<double>::max();
  size_t qualifyingBest = candidates.size();
  for (size_t i = 0; i < candidates.size(); ++i) {
    if (!qualifies[i]) {
      continue;
    }
    const double d = xy_length(candidates[i].x - reference.x,
                               candidates[i].y - reference.y);
    if (d < qualifyingDistance) {
      qualifyingDistance = d;
      qualifyingBest = i;
    }
  }
  if (qualifyingBest < candidates.size()) {
    return candidates[qualifyingBest];
  }
  warnings.push_back(
      "Every corner is sharper than the pierce threshold — using the "
      "nearest vertex.");
  return candidates[best];
}

std::vector<cam2d::OffsetSegment> build_lead_in(
    const PlannedLoop& loop, const cam2d::XY& pierce,
    const LaserCutParameters& params) {
  std::vector<cam2d::OffsetSegment> segments;
  if (params.mode == "engrave" || params.lead_in_mm <= 0.0) {
    return segments;
  }
  const double length = params.lead_in_mm;
  const size_t vertexIndex = sample_index_of(loop, pierce);
  XY u_out;
  XY u_in;
  vertex_tangents(loop, vertexIndex, u_out, u_in);

  if (params.lead_in_style == "arc") {
    // 90° tangent roll-in: enter outside the contour and sweep onto
    // the walk tangent.
    const XY n = right_normal(u_out.x, u_out.y);
    const XY entry{pierce.x + n.x * length + u_out.x * length,
                   pierce.y + n.y * length + u_out.y * length};
    segments.push_back(roll_arc(pierce, n, length, entry, pierce,
                                /*cw=*/true));
    return segments;
  }
  // Straight lead at lead_in_angle_deg to the contour tangent.
  const double angle = params.lead_in_angle_deg * kPi / 180.0;
  const XY direction = rotate_by(u_out, angle);
  const XY entry{pierce.x - direction.x * length,
                 pierce.y - direction.y * length};
  segments.push_back(line_segment(entry, pierce));
  return segments;
}

std::vector<cam2d::OffsetSegment> build_lead_out(
    const PlannedLoop& loop, const cam2d::XY& pierce,
    const LaserCutParameters& params) {
  std::vector<cam2d::OffsetSegment> segments;
  if (params.mode == "engrave" ||
      (params.lead_out_mm <= 0.0 && params.overcut_mm <= 0.0)) {
    return segments;
  }
  const size_t vertexIndex = sample_index_of(loop, pierce);
  XY u_out;
  XY u_in;
  vertex_tangents(loop, vertexIndex, u_out, u_in);

  if (params.lead_out_style == "arc" && params.lead_out_mm > 0.0) {
    // Overcut: continue straight along the exit tangent PAST the
    // pierce vertex, then roll out.
    XY start = pierce;
    if (params.overcut_mm > 0.0) {
      const XY overcutEnd{pierce.x + u_in.x * params.overcut_mm,
                          pierce.y + u_in.y * params.overcut_mm};
      segments.push_back(line_segment(pierce, overcutEnd));
      start = overcutEnd;
    }
    const double length = params.lead_out_mm;
    const XY n = right_normal(u_in.x, u_in.y);
    const XY exit{start.x + n.x * length + u_in.x * length,
                  start.y + n.y * length + u_in.y * length};
    segments.push_back(roll_arc(start, n, length, start, exit,
                                /*cw=*/true));
    return segments;
  }
  // Straight lead along the exit tangent; the overcut folds into the
  // same segment (the cut runs past the pierce vertex).
  const double total = params.lead_out_mm + params.overcut_mm;
  if (total > 0.0) {
    const XY end{pierce.x + u_in.x * total, pierce.y + u_in.y * total};
    segments.push_back(line_segment(pierce, end));
  }
  return segments;
}

}  // namespace polysmith::core::laser
