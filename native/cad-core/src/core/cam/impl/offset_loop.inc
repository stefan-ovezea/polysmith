// ── Shared closed-loop offset machinery ──────────────────────────
//
// Offsets one closed, orientation-normalized loop (material on the
// LEFT of the walk) by `d` to the right (the scrap side), with round
// join arcs at steep corners and miter trims at shallow corners.
// Used by the laser kerf offset and the face-milling inset.

namespace {

constexpr double kTwoPiConst = 6.28318530717958647692;

struct OffsetSegment {
  bool is_arc = false;
  XY start;
  XY end;
  XY center;          // arc only
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
void reverse_segments(std::vector<BaseSegment>& segments) {
  std::reverse(segments.begin(), segments.end());
  for (auto& segment : segments) {
    std::swap(segment.start, segment.end);
    segment.ccw = !segment.ccw;
  }
}

// Signed area of a segment list (shoelace over the endpoints).
double base_segments_signed_area(const std::vector<BaseSegment>& segments) {
  if (segments.empty()) {
    return 0.0;
  }
  double twiceArea = 0.0;
  for (const auto& segment : segments) {
    twiceArea += segment.start.x * segment.end.y -
                 segment.end.x * segment.start.y;
  }
  return twiceArea / 2.0;
}

// Offsets one base segment by `d` to the right of the walk.
//   line  → parallel line at distance d
//   arc   → concentric arc, radius +/- d (right of a CCW walk is
//           outward; right of a CW walk is inward)
OffsetSegment offset_segment(const BaseSegment& base, double d) {
  OffsetSegment out;
  out.is_join = false;
  if (!base.is_arc) {
    out.is_arc = false;
    const XY n = right_normal(base.end.x - base.start.x,
                              base.end.y - base.start.y);
    out.start = {base.start.x + n.x * d, base.start.y + n.y * d};
    out.end = {base.end.x + n.x * d, base.end.y + n.y * d};
    return out;
  }
  out.is_arc = true;
  out.center = base.center;
  const double delta = base.ccw ? d : -d;
  out.radius = base.radius + delta;
  // Endpoints move along their radial directions from the center.
  const auto offset_point = [&](const XY& p) {
    const double len = xy_length(p.x - base.center.x, p.y - base.center.y);
    if (len < kOffsetEps) {
      return p;
    }
    const double scale = (len + delta) / len;
    return XY{base.center.x + (p.x - base.center.x) * scale,
              base.center.y + (p.y - base.center.y) * scale};
  };
  out.start = offset_point(base.start);
  out.end = offset_point(base.end);
  out.cw = !base.ccw;
  return out;
}

// Line-line intersection of two infinite lines given by a point and a
// direction.  Returns false when parallel.
bool line_line_intersection(const XY& p1, const XY& d1, const XY& p2,
                            const XY& d2, XY& out) {
  const double denom = d1.x * d2.y - d1.y * d2.x;
  if (std::abs(denom) < kOffsetEps) {
    return false;
  }
  const double t = ((p2.x - p1.x) * d2.y - (p2.y - p1.y) * d2.x) / denom;
  out = {p1.x + t * d1.x, p1.y + t * d1.y};
  return true;
}

// Builds a round join arc around the shared original vertex between
// two adjacent offset segments and appends [current + join] to `out`.
void append_round_join(OffsetSegment current, OffsetSegment nextOffset,
                       const XY& vertex, double d,
                       std::vector<OffsetSegment>& out) {
  const double raw = std::atan2(
      (nextOffset.start.y - vertex.y) * (current.end.x - vertex.x) -
          (nextOffset.start.x - vertex.x) * (current.end.y - vertex.y),
      (nextOffset.start.x - vertex.x) * (current.end.x - vertex.x) +
          (nextOffset.start.y - vertex.y) * (current.end.y - vertex.y));
  const double turn = std::atan2(
      (current.end.y - vertex.y) * (nextOffset.start.x - vertex.x) -
          (current.end.x - vertex.x) * (nextOffset.start.y - vertex.y),
      (current.end.x - vertex.x) * (nextOffset.start.x - vertex.x) +
          (current.end.y - vertex.y) * (nextOffset.start.y - vertex.y));
  double sweep = raw;
  if (turn > 0 && sweep < 0) {
    sweep += kTwoPiConst;
  } else if (turn < 0 && sweep > 0) {
    sweep -= kTwoPiConst;
  }

  OffsetSegment join;
  join.is_arc = true;
  join.is_join = true;
  join.center = vertex;
  join.radius = d;
  join.start = current.end;
  join.end = nextOffset.start;
  join.cw = sweep < 0;
  out.push_back(current);
  out.push_back(join);
}

// Offsets one closed, orientation-normalized loop by `d` to the right
// of the walk.  Line-line corner handling:
//   - round_joins=true (laser kerf on the scrap side): round join arcs
//     at steep corners; miter trims at shallow corners (polygon
//     approximations of circles — the lines genuinely cross inside the
//     truncated segments and the miter point is the true boundary).
//   - round_joins=false (face-milling inset into the material): every
//     edge is rebuilt between its two adjacent MITER points (the
//     intersections of neighbouring offset lines) — a round join would
//     cut into the material corner.
// Arc corners are tangent by sketch construction; non-tangent ones get
// a join arc with the final self-intersection scan as the safety net.
bool offset_closed_loop(const std::vector<BaseSegment>& base, double d,
                        std::vector<OffsetSegment>& out,
                        bool round_joins = true) {
  out.clear();
  const size_t count = base.size();
  if (count == 0) {
    return false;
  }

  std::vector<OffsetSegment> offsets;
  offsets.reserve(count);
  for (const auto& segment : base) {
    offsets.push_back(offset_segment(segment, d));
  }

  // ── Miter mode ────────────────────────────────────────────────
  if (!round_joins) {
    for (const auto& segment : base) {
      if (segment.is_arc) {
        // Milling boundaries are sampled polylines; arcs would need
        // line-circle miter math.  Fall back to the round-join path.
        return offset_closed_loop(base, d, out, /*round_joins=*/true);
      }
    }
    // Miter point per corner: intersection of the two neighbouring
    // offset LINES (the offset segments are truncated pieces, so use
    // their directions, not their endpoints).  Collinear pieces (one
    // straight edge sampled into many pieces) have parallel lines — no
    // crossing — so the real miter value PROPAGATES to them from the
    // next real corner (two-pass fill covers the wrap-around corner).
    std::vector<std::optional<XY>> miter(count);
    for (size_t i = 0; i < count; ++i) {
      const auto& cur = offsets[i];
      const auto& nxt = offsets[(i + 1) % count];
      const XY dCur{cur.end.x - cur.start.x, cur.end.y - cur.start.y};
      const XY dNxt{nxt.end.x - nxt.start.x, nxt.end.y - nxt.start.y};
      XY crossing;
      if (line_line_intersection(cur.start, dCur, nxt.start, dNxt,
                                 crossing)) {
        miter[i] = crossing;
      }
    }
    std::vector<std::optional<XY>> endMiter(count);
    std::optional<XY> pending;
    // Scan BACKWARD: endMiter[i] = the first real corner at or after
    // piece i (pieces inside one straight edge share the corner at the
    // edge's far end).  Two passes cover the wrap-around.
    for (int pass = 0; pass < 2; ++pass) {
      for (size_t i = count; i-- > 0;) {
        if (miter[i].has_value()) {
          pending = miter[i];
        }
        endMiter[i] = pending;
      }
    }
    // Rebuild each edge from the previous real miter to the next real
    // miter — the piece span between them is the valid boundary.
    for (size_t i = 0; i < count; ++i) {
      OffsetSegment segment;
      segment.is_arc = false;
      segment.start =
          endMiter[(i + count - 1) % count].value_or(offsets[i].start);
      segment.end = endMiter[i].value_or(offsets[i].end);
      out.push_back(segment);
    }
    return !out.empty();
  }

  // ── Round-join mode ───────────────────────────────────────────
  // Start overrides propagated when the previous corner miter-trimmed
  // into this segment.
  std::vector<std::optional<XY>> startOverride(count);

  for (size_t i = 0; i < count; ++i) {
    const size_t nextIndex = (i + 1) % count;
    OffsetSegment current = offsets[i];
    OffsetSegment nextOffset = offsets[nextIndex];
    if (startOverride[i].has_value()) {
      current.start = startOverride[i].value();
    }

    if (current.is_arc || nextOffset.is_arc) {
      // Arc corner: tangent by sketch construction — snap the shared
      // endpoint; non-tangent gaps get a join arc.
      if (xy_length(current.end.x - nextOffset.start.x,
                    current.end.y - nextOffset.start.y) > 1e-9) {
        append_round_join(current, nextOffset, base[i].end, d, out);
      } else {
        current.end = nextOffset.start;
        out.push_back(current);
      }
      continue;
    }

    // Line-line corner: decide miter vs round join by where the two
    // offset lines cross.
    const XY dCurrent{current.end.x - current.start.x,
                      current.end.y - current.start.y};
    const XY dNext{nextOffset.end.x - nextOffset.start.x,
                   nextOffset.end.y - nextOffset.start.y};
    XY crossing;
    const bool parallel =
        !line_line_intersection(current.start, dCurrent, nextOffset.start,
                                dNext, crossing);
    if (parallel) {
      // Collinear continuation: snap.
      current.end = nextOffset.start;
      out.push_back(current);
      continue;
    }
    // Parameter of the crossing along each segment (0 = start, 1 = end).
    const double tCurrent =
        std::abs(dCurrent.x) > kOffsetEps
            ? (crossing.x - current.start.x) / dCurrent.x
            : (crossing.y - current.start.y) / dCurrent.y;
    const double tNext =
        std::abs(dNext.x) > kOffsetEps
            ? (crossing.x - nextOffset.start.x) / dNext.x
            : (crossing.y - nextOffset.start.y) / dNext.y;

    const bool crossingInsideBoth =
        tCurrent > 1e-9 && tCurrent < 1.0 - 1e-9 &&
        tNext > 1e-9 && tNext < 1.0 - 1e-9;
    if (crossingInsideBoth) {
      // Shallow corner: the lines cross inside the truncated segments —
      // miter-trim both at the crossing (the miter point IS the true
      // boundary).
      current.end = crossing;
      out.push_back(current);
      startOverride[nextIndex] = crossing;
    } else {
      // Steep corner (or the crossing lies away from both pieces): the
      // round join arc is the true boundary.
      append_round_join(current, nextOffset, base[i].end, d, out);
    }
  }
  // Patch the wrap-around corner: a miter at the LAST corner overrides
  // the FIRST segment's start, which was already emitted.
  if (startOverride[0].has_value()) {
    if (!out.empty()) {
      out.back().end = startOverride[0].value();
      out.front().start = startOverride[0].value();
    }
  }
  return !out.empty();
}

// Total length of an offset loop.
double offset_loop_length(const std::vector<OffsetSegment>& segments) {
  double length = 0.0;
  for (const auto& segment : segments) {
    if (!segment.is_arc) {
      length += xy_length(segment.end.x - segment.start.x,
                          segment.end.y - segment.start.y);
      continue;
    }
    const double startAngle = std::atan2(segment.start.y - segment.center.y,
                                         segment.start.x - segment.center.x);
    const double endAngle = std::atan2(segment.end.y - segment.center.y,
                                       segment.end.x - segment.center.x);
    double sweep = endAngle - startAngle;
    // Full circles: start == end, so the sweep must come from the
    // walk direction.
    if (xy_length(segment.end.x - segment.start.x,
                  segment.end.y - segment.start.y) < 1e-9) {
      sweep = segment.cw ? -kTwoPiConst : kTwoPiConst;
    } else if (segment.cw && sweep > 0) {
      sweep -= kTwoPiConst;
    } else if (!segment.cw && sweep < 0) {
      sweep += kTwoPiConst;
    }
    length += segment.radius * std::abs(sweep);
  }
  return length;
}

// Samples the loop into small straight pieces (for self-intersection
// scanning and any consumer needing a polyline).
std::vector<XY> sample_offset_loop(const std::vector<OffsetSegment>& segments,
                                   double tolerance) {
  std::vector<XY> points;
  for (const auto& segment : segments) {
    if (points.empty() ||
        xy_length(segment.start.x - points.back().x,
                  segment.start.y - points.back().y) > 1e-12) {
      points.push_back(segment.start);
    }
    if (!segment.is_arc) {
      points.push_back(segment.end);
      continue;
    }
    const double radius = segment.radius;
    const double toleranceRatio =
        std::max(0.0, std::min(1.0, tolerance / radius));
    const double maxAngle = 2.0 * std::acos(1.0 - toleranceRatio);
    const double startAngle = std::atan2(segment.start.y - segment.center.y,
                                         segment.start.x - segment.center.x);
    const double endAngle = std::atan2(segment.end.y - segment.center.y,
                                       segment.end.x - segment.center.x);
    double sweep = endAngle - startAngle;
    // Full circles: start == end, so the sweep comes from the walk
    // direction.
    if (xy_length(segment.end.x - segment.start.x,
                  segment.end.y - segment.start.y) < 1e-9) {
      sweep = segment.cw ? -kTwoPiConst : kTwoPiConst;
    } else if (segment.cw && sweep > 0) {
      sweep -= kTwoPiConst;
    } else if (!segment.cw && sweep < 0) {
      sweep += kTwoPiConst;
    }
    const int steps =
        std::max(1, static_cast<int>(std::ceil(std::abs(sweep) / maxAngle)));
    for (int step = 1; step <= steps; ++step) {
      const double angle = startAngle + sweep * static_cast<double>(step) / steps;
      points.push_back({segment.center.x + radius * std::cos(angle),
                        segment.center.y + radius * std::sin(angle)});
    }
  }
  return points;
}

// O(n²) proper-intersection scan over the sampled loop.  Adjacent
// pieces legitimately share endpoints; a crossing anywhere else means
// a feature narrower than the offset collapsed the loop.
bool offset_loop_self_intersects(const std::vector<XY>& samples) {
  const size_t count = samples.size();
  if (count < 4) {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    for (size_t j = i + 2; j < count; ++j) {
      // Skip wrap-around neighbours.
      if (j == count - 1 && i == 0) {
        continue;
      }
      if (xy_segments_intersect(samples[i], samples[(i + 1) % count],
                                samples[j], samples[(j + 1) % count])) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace
