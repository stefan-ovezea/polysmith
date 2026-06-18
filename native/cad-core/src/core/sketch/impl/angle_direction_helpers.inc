#pragma once

// ── Unified angle-direction resolution ──────────────────────────────
// Every angle dimension between two lines needs to pick a pair of
// direction vectors (one from each line) radiating from the pivot —
// whether that pivot is a shared endpoint, an endpoint-on-segment, or
// an infinite-line intersection (virtual pivot).
//
// Two lines × two endpoint directions each = 4 candidate pairs.
// The "right" pair is the one whose unsigned angle best matches the
// stored / target value (or the cursor quadrant, in the UI preview).
//
// This helper is the single source of truth for that selection.  It
// is used by the update, sync, and viewport-primitive paths so the
// same quadrant is preserved everywhere.

#include <optional>

struct ResolvedAngleDirections {
  double a_dx = 1.0;       // unit direction from pivot along line A
  double a_dy = 0.0;
  double b_dx = 0.0;       // unit direction from pivot along line B
  double b_dy = 1.0;
  double signed_angle = 0.0; // atan2(cross(a,b), dot(a,b))
  int a_endpoint_idx = 0;    // which line-A endpoint was chosen (0 or 1)
  int b_endpoint_idx = 0;    // which line-B endpoint was chosen (0 or 1)
};

// If  target_abs_angle  is provided (in radians, always in [0, π]),
// the pair whose unsigned angle is closest to that value wins.
// Otherwise the  dir_away  pair (farther endpoint from pivot for each
// line) is used — this is the "default" internal angle.
//
// Reflex values (> π) must be normalised by the caller before being
// passed as  target_abs_angle.
inline ResolvedAngleDirections resolve_angle_directions(
    const SketchLine& line_a,
    const SketchLine& line_b,
    double pivot_x, double pivot_y,
    std::optional<double> target_abs_angle = std::nullopt) {

  const std::array<std::pair<double, double>, 2> a_ends = {{
      {line_a.start_x, line_a.start_y},
      {line_a.end_x, line_a.end_y},
  }};
  const std::array<std::pair<double, double>, 2> b_ends = {{
      {line_b.start_x, line_b.start_y},
      {line_b.end_x, line_b.end_y},
  }};

  // Unit vectors from pivot to each line endpoint.
  auto unit = [&](double x, double y) {
    double dx = x - pivot_x, dy = y - pivot_y;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-12) return std::pair<double, double>{1.0, 0.0};
    return std::pair<double, double>{dx / len, dy / len};
  };
  std::pair<double, double> a_units[2] = {
      unit(a_ends[0].first, a_ends[0].second),
      unit(a_ends[1].first, a_ends[1].second)};
  std::pair<double, double> b_units[2] = {
      unit(b_ends[0].first, b_ends[0].second),
      unit(b_ends[1].first, b_ends[1].second)};

  // dir_away default: unit vector from pivot to the *farther* endpoint.
  auto dir_away = [&](double e0x, double e0y,
                      double e1x, double e1y) -> std::pair<double, double> {
    double d0 = std::sqrt((e0x - pivot_x)*(e0x - pivot_x) +
                          (e0y - pivot_y)*(e0y - pivot_y));
    double d1 = std::sqrt((e1x - pivot_x)*(e1x - pivot_x) +
                          (e1y - pivot_y)*(e1y - pivot_y));
    double tx = d0 > d1 ? e0x : e1x;
    double ty = d0 > d1 ? e0y : e1y;
    double dx = tx - pivot_x, dy = ty - pivot_y;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-12) return {1.0, 0.0};
    return {dx / len, dy / len};
  };
  auto av_def = dir_away(a_ends[0].first, a_ends[0].second,
                          a_ends[1].first, a_ends[1].second);
  auto bv_def = dir_away(b_ends[0].first, b_ends[0].second,
                          b_ends[1].first, b_ends[1].second);

  ResolvedAngleDirections result;
  int best_ai = (a_units[0].first == av_def.first &&
                 a_units[0].second == av_def.second) ? 0 : 1;
  int best_bi = (b_units[0].first == bv_def.first &&
                 b_units[0].second == bv_def.second) ? 0 : 1;

  if (target_abs_angle.has_value()) {
    double best_diff = 1e9;
    const double target = *target_abs_angle;
    for (int ai = 0; ai < 2; ++ai) {
      for (int bi = 0; bi < 2; ++bi) {
        // Skip candidates whose endpoint sits at the pivot — the
        // fallback (1,0) can accidentally win the scoring and turn
        // an inter-line angle into a single-line angle from horizontal.
        double a_dist = std::sqrt(
            (a_ends[ai].first - pivot_x) * (a_ends[ai].first - pivot_x) +
            (a_ends[ai].second - pivot_y) * (a_ends[ai].second - pivot_y));
        double b_dist = std::sqrt(
            (b_ends[bi].first - pivot_x) * (b_ends[bi].first - pivot_x) +
            (b_ends[bi].second - pivot_y) * (b_ends[bi].second - pivot_y));
        if (a_dist < 1e-9 || b_dist < 1e-9) continue;

        double cross = a_units[ai].first * b_units[bi].second -
                       a_units[ai].second * b_units[bi].first;
        double dot   = a_units[ai].first * b_units[bi].first +
                       a_units[ai].second * b_units[bi].second;
        if (cross == 0.0 && dot == 0.0) continue;
        double measured = std::abs(std::atan2(cross, dot));
        double diff = std::abs(measured - target);
        // Prefer dir_away on ties so the default quadrant is stable.
        bool is_def =
            (a_units[ai].first == av_def.first &&
             a_units[ai].second == av_def.second) &&
            (b_units[bi].first == bv_def.first &&
             b_units[bi].second == bv_def.second);
        if (is_def) diff -= 1e-12;
        if (diff < best_diff) {
          best_diff = diff;
          best_ai = ai;
          best_bi = bi;
        }
      }
    }
  }

  result.a_dx = a_units[best_ai].first;
  result.a_dy = a_units[best_ai].second;
  result.b_dx = b_units[best_bi].first;
  result.b_dy = b_units[best_bi].second;
  result.a_endpoint_idx = best_ai;
  result.b_endpoint_idx = best_bi;
  result.signed_angle = std::atan2(
      result.a_dx * result.b_dy - result.a_dy * result.b_dx,
      result.a_dx * result.b_dx + result.a_dy * result.b_dy);
  return result;
}
