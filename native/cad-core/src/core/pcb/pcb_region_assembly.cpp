#include "core/pcb/pcb_region_assembly.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "clipper2/clipper.h"

namespace polysmith::core::pcb {
namespace {

using Clipper2Lib::ClipType;
using Clipper2Lib::Clipper64;
using Clipper2Lib::FillRule;
using Clipper2Lib::Path64;
using Clipper2Lib::Paths64;
using Clipper2Lib::Point64;

Point64 to_point(const cam2d::XY& point) {
  return Point64(static_cast<int64_t>(std::llround(point.x * kBooleanScale)),
                 static_cast<int64_t>(std::llround(point.y * kBooleanScale)));
}

Path64 to_path(const std::vector<cam2d::XY>& points) {
  Path64 path;
  path.reserve(points.size());
  for (const cam2d::XY& point : points) {
    path.push_back(to_point(point));
  }
  return path;
}

/// Signed area in scaled units — Clipper2's own orientation convention is
/// positive for counter-clockwise outer boundaries and negative for holes.
double scaled_signed_area(const Path64& path) {
  if (path.size() < 3) {
    return 0.0;
  }
  double area = 0.0;
  for (size_t i = 0; i < path.size(); ++i) {
    const Point64& a = path[i];
    const Point64& b = path[(i + 1) % path.size()];
    area += static_cast<double>(a.x) * static_cast<double>(b.y) -
            static_cast<double>(b.x) * static_cast<double>(a.y);
  }
  return area / 2.0;
}

std::vector<cam2d::XY> to_ring(const Path64& path) {
  std::vector<cam2d::XY> points;
  points.reserve(path.size());
  for (const Point64& point : path) {
    points.push_back(cam2d::XY{static_cast<double>(point.x) / kBooleanScale,
                               static_cast<double>(point.y) / kBooleanScale});
  }
  // Clipper2 closes paths implicitly; the ring contract repeats no closing
  // point.
  while (points.size() >= 2 &&
         std::abs(points.front().x - points.back().x) < 1e-12 &&
         std::abs(points.front().y - points.back().y) < 1e-12) {
    points.pop_back();
  }
  return points;
}

}  // namespace

std::vector<PcbRing> assemble_region_rings(const std::vector<PcbRing>& drawn) {
  std::vector<PcbRing> assembled;
  if (drawn.empty()) {
    return assembled;
  }

  // Polarity is applied in DRAW ORDER: %LPD adds to the accumulated region,
  // %LPC removes from it.  Clipper2's ClipType takes two operand sets, so the
  // accumulated result is folded through as the subject and each new ring as
  // the clip — one operation per polarity change, not per ring.
  Clipper64 clipper;
  Paths64 subject;
  bool have_result = false;

  const auto flush = [&](ClipType operation, const Paths64& operand) {
    if (operand.empty()) {
      return;
    }
    clipper.Clear();
    clipper.AddSubject(subject);
    clipper.AddClip(operand);
    Paths64 solution;
    clipper.Execute(operation, FillRule::NonZero, solution);
    subject = std::move(solution);
    have_result = true;
  };

  Paths64 pending_dark;
  Paths64 pending_clear;
  const auto flush_pending = [&]() {
    if (!pending_dark.empty()) {
      flush(ClipType::Union, pending_dark);
      pending_dark.clear();
    }
    if (!pending_clear.empty()) {
      flush(ClipType::Difference, pending_clear);
      pending_clear.clear();
    }
  };

  for (const PcbRing& ring : drawn) {
    if (ring.points.size() < 3) {
      continue;
    }
    Path64 path = to_path(ring.points);
    const double area = scaled_signed_area(path);
    if (std::abs(area) < 1.0) {
      continue;  // degenerate sliver
    }
    // Every drawn ring bounds a FILLED shape, so normalize them all to one
    // winding before clipping.  Clipper2 unions under the NonZero fill rule,
    // which reads orientation as inside/outside: mixing windings would make an
    // overlapping pad cancel itself instead of merging.  (The polygonizer keeps
    // the source's winding, which carries no meaning until polarity applies.)
    if (area < 0.0) {
      std::reverse(path.begin(), path.end());
    }
    if (ring.dark) {
      // A clear run in between must be applied first, in order.
      if (!pending_clear.empty()) {
        flush_pending();
      }
      pending_dark.push_back(std::move(path));
    } else {
      if (!pending_dark.empty()) {
        flush_pending();
      }
      pending_clear.push_back(std::move(path));
    }
  }
  flush_pending();

  if (!have_result && subject.empty()) {
    return assembled;
  }

  // Emit every boundary as its own contour: the cutter needs the outer
  // boundary and each hole as separate closed loops.  Orientation from
  // Clipper2 is preserved (positive = outer) so callers can tell them apart.
  assembled.reserve(subject.size());
  for (const Path64& path : subject) {
    const std::vector<cam2d::XY> points = to_ring(path);
    if (points.size() < 3) {
      continue;
    }
    PcbRing ring;
    ring.points = points;
    // The assembled region is what remains after dark and clear are applied,
    // so every boundary it leaves behind is material to cut.
    ring.dark = true;
    ring.line = 0;
    assembled.push_back(std::move(ring));
  }

  // Deterministic order: largest area first, then by position.  The laser
  // ordering pass reorders for travel afterwards; this only makes the result
  // reproducible run to run (and across platforms).
  std::stable_sort(assembled.begin(), assembled.end(),
                   [](const PcbRing& a, const PcbRing& b) {
                     const double area_a =
                         std::abs(cam2d::xy_signed_area(a.points));
                     const double area_b =
                         std::abs(cam2d::xy_signed_area(b.points));
                     if (std::abs(area_a - area_b) > 1e-9) {
                       return area_a > area_b;
                     }
                     return a.points.front().x < b.points.front().x;
                   });
  return assembled;
}

}  // namespace polysmith::core::pcb
