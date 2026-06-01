#include "core/snap_engine.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace polysmith::core {
namespace {

// Compute the distance between two sketch-plane points.
double point_distance(double x1, double y1, double x2, double y2) {
  const double dx = x2 - x1;
  const double dy = y2 - y1;
  return std::sqrt(dx * dx + dy * dy);
}

// Find all endpoint snaps on lines.
void collect_line_endpoint_candidates(
    const SketchFeatureParameters& sketch,
    double cursor_x,
    double cursor_y,
    double tolerance,
    const SelectionFilter& filter,
    std::vector<SnapCandidate>& candidates) {
  for (const auto& line : sketch.lines) {
    if (line.is_construction && !filter.select_construction) {
      continue;
    }
    // Start point
    {
      const double d = point_distance(cursor_x, cursor_y, line.start_x, line.start_y);
      if (d <= tolerance) {
        candidates.push_back(SnapCandidate{
            .kind = "endpoint",
            .entity_id = line.id,
            .point_id = line.start_point_id,
            .local_x = line.start_x,
            .local_y = line.start_y,
            .distance = d,
            .label = "Endpoint",
        });
      }
    }
    // End point
    {
      const double d = point_distance(cursor_x, cursor_y, line.end_x, line.end_y);
      if (d <= tolerance) {
        candidates.push_back(SnapCandidate{
            .kind = "endpoint",
            .entity_id = line.id,
            .point_id = line.end_point_id,
            .local_x = line.end_x,
            .local_y = line.end_y,
            .distance = d,
            .label = "Endpoint",
        });
      }
    }
  }
}

// Find all endpoint snaps on arcs.
void collect_arc_endpoint_candidates(
    const SketchFeatureParameters& sketch,
    double cursor_x,
    double cursor_y,
    double tolerance,
    const SelectionFilter& filter,
    std::vector<SnapCandidate>& candidates) {
  for (const auto& arc : sketch.arcs) {
    if (arc.is_construction && !filter.select_construction) {
      continue;
    }
    {
      const double d = point_distance(cursor_x, cursor_y, arc.start_x, arc.start_y);
      if (d <= tolerance) {
        candidates.push_back(SnapCandidate{
            .kind = "endpoint",
            .entity_id = arc.id,
            .point_id = arc.start_point_id,
            .local_x = arc.start_x,
            .local_y = arc.start_y,
            .distance = d,
            .label = "Endpoint",
        });
      }
    }
    {
      const double d = point_distance(cursor_x, cursor_y, arc.end_x, arc.end_y);
      if (d <= tolerance) {
        candidates.push_back(SnapCandidate{
            .kind = "endpoint",
            .entity_id = arc.id,
            .point_id = arc.end_point_id,
            .local_x = arc.end_x,
            .local_y = arc.end_y,
            .distance = d,
            .label = "Endpoint",
        });
      }
    }
  }
}

// Find midpoint snaps on lines.
void collect_line_midpoint_candidates(
    const SketchFeatureParameters& sketch,
    double cursor_x,
    double cursor_y,
    double tolerance,
    const SelectionFilter& filter,
    std::vector<SnapCandidate>& candidates) {
  for (const auto& line : sketch.lines) {
    if (line.is_construction && !filter.select_construction) {
      continue;
    }
    const double mx = (line.start_x + line.end_x) / 2.0;
    const double my = (line.start_y + line.end_y) / 2.0;
    const double d = point_distance(cursor_x, cursor_y, mx, my);
    if (d <= tolerance) {
      candidates.push_back(SnapCandidate{
          .kind = "midpoint",
          .entity_id = line.id,
          .point_id = "",
          .local_x = mx,
          .local_y = my,
          .distance = d,
          .label = "Midpoint",
          .param_t = 0.5,
      });
    }
  }
}

// Find center snaps on circles.
void collect_circle_center_candidates(
    const SketchFeatureParameters& sketch,
    double cursor_x,
    double cursor_y,
    double tolerance,
    const SelectionFilter& filter,
    std::vector<SnapCandidate>& candidates) {
  for (const auto& circle : sketch.circles) {
    if (circle.is_construction && !filter.select_construction) {
      continue;
    }
    const double d = point_distance(cursor_x, cursor_y, circle.center_x, circle.center_y);
    if (d <= tolerance) {
      candidates.push_back(SnapCandidate{
          .kind = "center",
          .entity_id = circle.id,
          .point_id = "",
          .local_x = circle.center_x,
          .local_y = circle.center_y,
          .distance = d,
          .label = "Center",
      });
    }
  }
}

// Find center snaps on polygons.
void collect_polygon_center_candidates(
    const SketchFeatureParameters& sketch,
    double cursor_x,
    double cursor_y,
    double tolerance,
    const SelectionFilter& filter,
    std::vector<SnapCandidate>& candidates) {
  for (const auto& poly : sketch.polygons) {
    if (poly.is_construction && !filter.select_construction) {
      continue;
    }
    const double d = point_distance(cursor_x, cursor_y, poly.center_x, poly.center_y);
    if (d <= tolerance) {
      candidates.push_back(SnapCandidate{
          .kind = "center",
          .entity_id = poly.id,
          .point_id = "",
          .local_x = poly.center_x,
          .local_y = poly.center_y,
          .distance = d,
          .label = "Center",
      });
    }
  }
}

// Find center snaps on arcs.
void collect_arc_center_candidates(
    const SketchFeatureParameters& sketch,
    double cursor_x,
    double cursor_y,
    double tolerance,
    const SelectionFilter& filter,
    std::vector<SnapCandidate>& candidates) {
  for (const auto& arc : sketch.arcs) {
    if (arc.is_construction && !filter.select_construction) {
      continue;
    }
    const double d = point_distance(cursor_x, cursor_y, arc.center_x, arc.center_y);
    if (d <= tolerance) {
      candidates.push_back(SnapCandidate{
          .kind = "center",
          .entity_id = arc.id,
          .point_id = "",
          .local_x = arc.center_x,
          .local_y = arc.center_y,
          .distance = d,
          .label = "Center",
      });
    }
  }
}

// Find nearest (body) snaps on lines — any point along the line segment
// within tolerance.
void collect_nearest_candidates(
    const SketchFeatureParameters& sketch,
    double cursor_x,
    double cursor_y,
    double tolerance,
    const SelectionFilter& filter,
    std::vector<SnapCandidate>& candidates) {
  for (const auto& line : sketch.lines) {
    if (line.is_construction && !filter.select_construction) {
      continue;
    }
    // Project cursor onto the infinite line, clamp to segment.
    const double dx = line.end_x - line.start_x;
    const double dy = line.end_y - line.start_y;
    const double len_sq = dx * dx + dy * dy;
    if (len_sq < 1e-12) continue;
    double t = ((cursor_x - line.start_x) * dx + (cursor_y - line.start_y) * dy) / len_sq;
    t = std::max(0.0, std::min(1.0, t));
    const double px = line.start_x + t * dx;
    const double py = line.start_y + t * dy;
    const double d = point_distance(cursor_x, cursor_y, px, py);
    if (d <= tolerance) {
      candidates.push_back(SnapCandidate{
          .kind = "nearest",
          .entity_id = line.id,
          .point_id = "",
          .local_x = px,
          .local_y = py,
          .distance = d,
          .label = "Nearest",
          .param_t = t,
      });
    }
  }
}

// Find nearest snaps on circles — project cursor radially onto the
// circle's circumference. The nearest point is the intersection of
// the ray center→cursor with the circle edge.
void collect_circle_nearest_candidates(
    const SketchFeatureParameters& sketch,
    double cursor_x,
    double cursor_y,
    double tolerance,
    const SelectionFilter& filter,
    std::vector<SnapCandidate>& candidates) {
  for (const auto& circle : sketch.circles) {
    if (circle.is_construction && !filter.select_construction) continue;
    const double dx = cursor_x - circle.center_x;
    const double dy = cursor_y - circle.center_y;
    const double dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 1e-12) continue;
    const double nx = circle.center_x + (dx / dist) * circle.radius;
    const double ny = circle.center_y + (dy / dist) * circle.radius;
    const double d = point_distance(cursor_x, cursor_y, nx, ny);
    if (d <= tolerance) {
      candidates.push_back(SnapCandidate{
          .kind = "nearest",
          .entity_id = circle.id,
          .point_id = "",
          .local_x = nx,
          .local_y = ny,
          .distance = d,
          .label = "Nearest",
      });
    }
  }
}

// Find intersection snaps between sketch entities.
void collect_intersection_candidates(
    const SketchFeatureParameters& sketch,
    double cursor_x,
    double cursor_y,
    double tolerance,
    const SelectionFilter& filter,
    std::vector<SnapCandidate>& candidates) {
  // Line-line intersections
  for (size_t i = 0; i < sketch.lines.size(); ++i) {
    const auto& a = sketch.lines[i];
    if (a.is_construction && !filter.select_construction) continue;
    for (size_t j = i + 1; j < sketch.lines.size(); ++j) {
      const auto& b = sketch.lines[j];
      if (b.is_construction && !filter.select_construction) continue;
      const double a_dx = a.end_x - a.start_x;
      const double a_dy = a.end_y - a.start_y;
      const double b_dx = b.end_x - b.start_x;
      const double b_dy = b.end_y - b.start_y;
      const double denom = a_dx * b_dy - a_dy * b_dx;
      if (std::abs(denom) < 1e-12) continue;
      const double t = ((b.start_x - a.start_x) * b_dy - (b.start_y - a.start_y) * b_dx) / denom;
      const double u = ((b.start_x - a.start_x) * a_dy - (b.start_y - a.start_y) * a_dx) / denom;
      if (t < 0.0 || t > 1.0 || u < 0.0 || u > 1.0) continue;
      const double ix = a.start_x + t * a_dx;
      const double iy = a.start_y + t * a_dy;
      const double d = point_distance(cursor_x, cursor_y, ix, iy);
      if (d <= tolerance) {
        candidates.push_back(SnapCandidate{
            .kind = "intersection",
            .entity_id = a.id,
            .point_id = "",
            .local_x = ix,
            .local_y = iy,
            .distance = d,
            .label = "Intersection",
        });
      }
    }
  }

  // Line-arc intersections
  for (const auto& line : sketch.lines) {
    if (line.is_construction && !filter.select_construction) continue;
    for (const auto& arc : sketch.arcs) {
      if (arc.is_construction && !filter.select_construction) continue;
      const double dx = line.end_x - line.start_x;
      const double dy = line.end_y - line.start_y;
      const double len_sq = dx * dx + dy * dy;
      if (len_sq < 1e-12) continue;
      const double r = point_distance(arc.center_x, arc.center_y, arc.start_x, arc.start_y);
      const double fx = line.start_x - arc.center_x;
      const double fy = line.start_y - arc.center_y;
      const double a_val = dx * dx + dy * dy;
      const double b_val = 2.0 * (fx * dx + fy * dy);
      const double c_val = fx * fx + fy * fy - r * r;
      double disc = b_val * b_val - 4.0 * a_val * c_val;
      if (disc < 0) continue;
      disc = std::sqrt(disc);
      for (double sign : {-1.0, 1.0}) {
        const double t = (-b_val + sign * disc) / (2.0 * a_val);
        if (t < 0.0 || t > 1.0) continue;
        const double ix = line.start_x + t * dx;
        const double iy = line.start_y + t * dy;
        const double d = point_distance(cursor_x, cursor_y, ix, iy);
        if (d <= tolerance) {
          candidates.push_back(SnapCandidate{
              .kind = "intersection",
              .entity_id = line.id,
              .point_id = "",
              .local_x = ix,
              .local_y = iy,
              .distance = d,
              .label = "Intersection",
          });
        }
      }
    }
  }
}

// Find quadrant snaps on circles (0°, 90°, 180°, 270°).
void collect_quadrant_candidates(
    const SketchFeatureParameters& sketch,
    double cursor_x,
    double cursor_y,
    double tolerance,
    const SelectionFilter& filter,
    std::vector<SnapCandidate>& candidates) {
  for (const auto& circle : sketch.circles) {
    if (circle.is_construction && !filter.select_construction) continue;
    const double cx = circle.center_x;
    const double cy = circle.center_y;
    const double r = circle.radius;
    const double quads[4][2] = {
        {cx + r, cy},
        {cx, cy + r},
        {cx - r, cy},
        {cx, cy - r},
    };
    for (const auto& q : quads) {
      const double d = point_distance(cursor_x, cursor_y, q[0], q[1]);
      if (d <= tolerance) {
        candidates.push_back(SnapCandidate{
            .kind = "quadrant",
            .entity_id = circle.id,
            .point_id = "",
            .local_x = q[0],
            .local_y = q[1],
            .distance = d,
            .label = "Quadrant",
        });
      }
    }
  }
}

// Find perpendicular foot snaps from cursor to lines.
void collect_perpendicular_candidates(
    const SketchFeatureParameters& sketch,
    double cursor_x,
    double cursor_y,
    double tolerance,
    const SelectionFilter& filter,
    std::vector<SnapCandidate>& candidates) {
  for (const auto& line : sketch.lines) {
    if (line.is_construction && !filter.select_construction) continue;
    const double dx = line.end_x - line.start_x;
    const double dy = line.end_y - line.start_y;
    const double len_sq = dx * dx + dy * dy;
    if (len_sq < 1e-12) continue;
    // Project cursor onto the infinite line (no segment clamp).
    // The distance check below ensures the foot is close to the cursor;
    // clamping to the segment would produce a non-perpendicular point
    // when the projection falls outside, defeating the purpose.
    const double t = ((cursor_x - line.start_x) * dx + (cursor_y - line.start_y) * dy) / len_sq;
    const double px = line.start_x + t * dx;
    const double py = line.start_y + t * dy;
    const double d = point_distance(cursor_x, cursor_y, px, py);
    if (d <= tolerance) {
      candidates.push_back(SnapCandidate{
          .kind = "perpendicular",
          .entity_id = line.id,
          .point_id = "",
          .local_x = px,
          .local_y = py,
          .distance = d,
          .label = "Perpendicular",
      });
    }
  }
}

// Find tangent snaps from cursor to circles/arcs.
void collect_tangent_candidates(
    const SketchFeatureParameters& sketch,
    double cursor_x,
    double cursor_y,
    double tolerance,
    const SelectionFilter& filter,
    std::vector<SnapCandidate>& candidates) {
  for (const auto& circle : sketch.circles) {
    if (circle.is_construction && !filter.select_construction) continue;
    const double dx = circle.center_x - cursor_x;
    const double dy = circle.center_y - cursor_y;
    const double d_sq = dx * dx + dy * dy;
    const double r_sq = circle.radius * circle.radius;
    if (d_sq <= r_sq + 1e-9) continue;
    const double d_val = std::sqrt(d_sq);
    const double tangent_len = std::sqrt(d_sq - r_sq);
    const double ux = dx / d_val;
    const double uy = dy / d_val;
    const double sin_theta = circle.radius / d_val;
    const double cos_theta = tangent_len / d_val;
    for (double sign : {-1.0, 1.0}) {
      const double rux = cos_theta * ux - sign * sin_theta * uy;
      const double ruy = sign * sin_theta * ux + cos_theta * uy;
      const double tx = circle.center_x - rux * circle.radius;
      const double ty = circle.center_y - ruy * circle.radius;
      const double dist = point_distance(cursor_x, cursor_y, tx, ty);
      if (dist <= tolerance) {
        candidates.push_back(SnapCandidate{
            .kind = "tangent",
            .entity_id = circle.id,
            .point_id = "",
            .local_x = tx,
            .local_y = ty,
            .distance = dist,
            .label = "Tangent",
        });
      }
    }
  }
}

// Find grid snap — round cursor to nearest grid intersection.
void collect_grid_candidates(
    double cursor_x,
    double cursor_y,
    double tolerance,
    const SelectionFilter& filter,
    std::vector<SnapCandidate>& candidates) {
  constexpr double kGridSpacing = 1.0;
  const double gx = std::round(cursor_x / kGridSpacing) * kGridSpacing;
  const double gy = std::round(cursor_y / kGridSpacing) * kGridSpacing;
  const double d = point_distance(cursor_x, cursor_y, gx, gy);
  if (d <= tolerance) {
    candidates.push_back(SnapCandidate{
        .kind = "grid",
        .entity_id = "",
        .point_id = "",
        .local_x = gx,
        .local_y = gy,
        .distance = d,
        .label = "Grid",
    });
  }
}

// Grid-line snap: lock to the nearest horizontal or vertical grid line.
void collect_grid_line_candidates(
    double cursor_x,
    double cursor_y,
    double tolerance,
    const SelectionFilter& filter,
    std::vector<SnapCandidate>& candidates) {
  constexpr double kGridSpacing = 1.0;
  const double gx = std::round(cursor_x / kGridSpacing) * kGridSpacing;
  const double gy = std::round(cursor_y / kGridSpacing) * kGridSpacing;
  // Distance to nearest vertical grid line (locked X)
  const double dx_vert = std::abs(cursor_x - gx);
  // Distance to nearest horizontal grid line (locked Y)
  const double dy_horiz = std::abs(cursor_y - gy);

  // Prefer the closer axis lock.
  if (dx_vert <= tolerance && dx_vert <= dy_horiz) {
    candidates.push_back(SnapCandidate{
        .kind = "grid_line",
        .entity_id = "",
        .point_id = "",
        .local_x = gx,
        .local_y = cursor_y,
        .distance = dx_vert,
        .label = "Grid Line",
    });
  } else if (dy_horiz <= tolerance) {
    candidates.push_back(SnapCandidate{
        .kind = "grid_line",
        .entity_id = "",
        .point_id = "",
        .local_x = cursor_x,
        .local_y = gy,
        .distance = dy_horiz,
        .label = "Grid Line",
    });
  }
}

// Polar snap: lock cursor to the nearest polar angle increment from a
// start point. Only active when a start point is provided (line drafting).
void collect_polar_candidates(
    double cursor_x,
    double cursor_y,
    double start_x,
    double start_y,
    double tolerance,
    const SelectionFilter& filter,
    std::vector<SnapCandidate>& candidates) {
  const double dx = cursor_x - start_x;
  const double dy = cursor_y - start_y;
  const double dist = std::sqrt(dx * dx + dy * dy);
  if (dist < 1e-9) return;

  const int angle_step = filter.polar_angle_degrees > 0
      ? filter.polar_angle_degrees
      : 15;
  const double angle_rad = std::atan2(dy, dx);
  // Snap angle to nearest increment
  const double step_rad = (angle_step * M_PI) / 180.0;
  const double snapped_rad = std::round(angle_rad / step_rad) * step_rad;

  const double sx = start_x + dist * std::cos(snapped_rad);
  const double sy = start_y + dist * std::sin(snapped_rad);
  const double d = std::hypot(cursor_x - sx, cursor_y - sy);
  if (d <= tolerance) {
    candidates.push_back(SnapCandidate{
        .kind = "polar",
        .entity_id = "",
        .point_id = "",
        .local_x = sx,
        .local_y = sy,
        .distance = d,
        .label = "Polar",
    });
  }
}

void collect_parallel_candidates(
    double cursor_x, double cursor_y,
    double start_x, double start_y,
    double tolerance,
    const SketchFeatureParameters& sketch,
    const SelectionFilter& filter,
    std::vector<SnapCandidate>& candidates) {
  const double dx = cursor_x - start_x;
  const double dy = cursor_y - start_y;
  const double dist = std::hypot(dx, dy);
  if (dist < 1e-9) return;
  const double cursor_angle = std::atan2(dy, dx);
  constexpr double kAngleThreshold = M_PI / 18.0;
  double best_angle_diff = kAngleThreshold;
  double best_angle = 0.0;
  std::string best_line_id;
  for (const auto& line : sketch.lines) {
    if (line.is_construction) continue;
    const double la = std::atan2(line.end_y - line.start_y, line.end_x - line.start_x);
    for (double dir : {la, la + M_PI}) {
      double ad = std::abs(cursor_angle - dir);
      if (ad > M_PI) ad = 2.0 * M_PI - ad;
      if (ad < best_angle_diff) { best_angle_diff = ad; best_angle = dir; best_line_id = line.id; }
    }
  }
  if (best_line_id.empty()) return;
  const double ca = std::cos(best_angle), sa = std::sin(best_angle);
  const double t = dx * ca + dy * sa;
  const double px = start_x + t * ca, py = start_y + t * sa;
  const double d = std::hypot(cursor_x - px, cursor_y - py);
  if (d <= tolerance) {
    candidates.push_back(SnapCandidate{.kind="parallel",.entity_id=best_line_id,.point_id="",.local_x=px,.local_y=py,.distance=d,.label="Parallel"});
  }
}

void collect_perpendicular_direction_candidates(
    double cursor_x, double cursor_y,
    double start_x, double start_y,
    double tolerance,
    const SketchFeatureParameters& sketch,
    const SelectionFilter& filter,
    std::vector<SnapCandidate>& candidates) {
  const double dx = cursor_x - start_x;
  const double dy = cursor_y - start_y;
  const double dist = std::hypot(dx, dy);
  if (dist < 1e-9) return;
  const double cursor_angle = std::atan2(dy, dx);
  constexpr double kAngleThreshold = M_PI / 18.0;
  double best_angle_diff = kAngleThreshold;
  double best_angle = 0.0;
  std::string best_line_id;
  for (const auto& line : sketch.lines) {
    if (line.is_construction) continue;
    const double la = std::atan2(line.end_y - line.start_y, line.end_x - line.start_x);
    for (double dir : {la + M_PI_2, la - M_PI_2, la + 3.0 * M_PI_2, la - 3.0 * M_PI_2}) {
      double ad = std::abs(cursor_angle - dir);
      if (ad > M_PI) ad = 2.0 * M_PI - ad;
      if (ad < best_angle_diff) { best_angle_diff = ad; best_angle = dir; best_line_id = line.id; }
    }
  }
  if (best_line_id.empty()) return;
  const double ca = std::cos(best_angle), sa = std::sin(best_angle);
  const double t = dx * ca + dy * sa;
  const double px = start_x + t * ca, py = start_y + t * sa;
  const double d = std::hypot(cursor_x - px, cursor_y - py);
  if (d <= tolerance) {
    candidates.push_back(SnapCandidate{.kind="perpendicular_direction",.entity_id=best_line_id,.point_id="",.local_x=px,.local_y=py,.distance=d,.label="Perpendicular"});
  }
}

void collect_axis_lock_candidates(
    double cursor_x, double cursor_y,
    double start_x, double start_y,
    double tolerance,
    const SketchFeatureParameters& sketch,
    const SelectionFilter& filter,
    std::vector<SnapCandidate>& candidates) {
  const double dx = cursor_x - start_x;
  const double dy = cursor_y - start_y;
  const double dist = std::hypot(dx, dy);
  if (dist < 1e-9) return;
  const double kSinThreshold = std::sin(3.0 * M_PI / 180.0);
  const double hratio = std::abs(dy) / dist;
  const double vratio = std::abs(dx) / dist;
  if (hratio < kSinThreshold) {
    const double ly = start_y;
    double best_dist = tolerance; SnapCandidate best; bool found = false;
    for (const auto& line : sketch.lines) {
      double ldy = line.end_y - line.start_y;
      if (std::abs(ldy) < 1e-9) continue;
      double t = (ly - line.start_y) / ldy;
      if (t < 0.0 || t > 1.0) continue;
      double ix = line.start_x + t * (line.end_x - line.start_x);
      double d = std::abs(ix - cursor_x);
      if (d < best_dist) {
        best_dist = d;
        best = SnapCandidate{.kind="axis_lock",.entity_id=line.id,.point_id="",.local_x=ix,.local_y=ly,.distance=d,.label="Horizontal"};
        found = true;
      }
    }
    if (found) candidates.push_back(best);
    else candidates.push_back(SnapCandidate{.kind="axis_lock",.entity_id="",.point_id="",.local_x=cursor_x,.local_y=ly,.distance=std::abs(cursor_y-ly),.label="Horizontal"});
  } else if (vratio < kSinThreshold) {
    const double lx = start_x;
    double best_dist = tolerance; SnapCandidate best; bool found = false;
    for (const auto& line : sketch.lines) {
      double ldx = line.end_x - line.start_x;
      if (std::abs(ldx) < 1e-9) continue;
      double t = (lx - line.start_x) / ldx;
      if (t < 0.0 || t > 1.0) continue;
      double iy = line.start_y + t * (line.end_y - line.start_y);
      double d = std::abs(iy - cursor_y);
      if (d < best_dist) {
        best_dist = d;
        best = SnapCandidate{.kind="axis_lock",.entity_id=line.id,.point_id="",.local_x=lx,.local_y=iy,.distance=d,.label="Vertical"};
        found = true;
      }
    }
    if (found) candidates.push_back(best);
    else candidates.push_back(SnapCandidate{.kind="axis_lock",.entity_id="",.point_id="",.local_x=lx,.local_y=cursor_y,.distance=std::abs(cursor_x-lx),.label="Vertical"});
  }
}

} // namespace

std::optional<SnapCandidate> resolve_snap(
    double cursor_x,
    double cursor_y,
    const SketchFeatureParameters& sketch,
    const SelectionFilter& filter,
    double tolerance,
    std::optional<double> start_x,
    std::optional<double> start_y,
    const std::vector<std::string>& snap_priority) {
  std::vector<SnapCandidate> candidates;

  // Collect candidates based on active snap types in the filter.
  if (filter.snap_endpoint) {
    collect_line_endpoint_candidates(sketch, cursor_x, cursor_y, tolerance, filter, candidates);
    collect_arc_endpoint_candidates(sketch, cursor_x, cursor_y, tolerance, filter, candidates);
  }
  if (filter.snap_midpoint) {
    collect_line_midpoint_candidates(sketch, cursor_x, cursor_y, tolerance, filter, candidates);
  }
  if (filter.snap_center) {
    collect_circle_center_candidates(sketch, cursor_x, cursor_y, tolerance, filter, candidates);
    collect_polygon_center_candidates(sketch, cursor_x, cursor_y, tolerance, filter, candidates);
    collect_arc_center_candidates(sketch, cursor_x, cursor_y, tolerance, filter, candidates);
  }
  if (filter.snap_nearest) {
    collect_nearest_candidates(sketch, cursor_x, cursor_y, tolerance, filter, candidates);
    collect_circle_nearest_candidates(sketch, cursor_x, cursor_y, tolerance, filter, candidates);
  }
  if (filter.snap_intersection) {
    collect_intersection_candidates(sketch, cursor_x, cursor_y, tolerance, filter, candidates);
  }
  if (filter.snap_quadrant) {
    collect_quadrant_candidates(sketch, cursor_x, cursor_y, tolerance, filter, candidates);
  }
  if (filter.snap_perpendicular) {
    collect_perpendicular_candidates(sketch, cursor_x, cursor_y, tolerance, filter, candidates);
  }
  if (filter.snap_tangent) {
    collect_tangent_candidates(sketch, cursor_x, cursor_y, tolerance, filter, candidates);
  }
  if (filter.snap_grid) {
    collect_grid_candidates(cursor_x, cursor_y, tolerance, filter, candidates);
  }
  if (filter.snap_grid_line) {
    collect_grid_line_candidates(cursor_x, cursor_y, tolerance, filter, candidates);
  }
  if (filter.snap_polar && start_x.has_value() && start_y.has_value()) {
    collect_polar_candidates(cursor_x, cursor_y, *start_x, *start_y, tolerance, filter, candidates);
  }
  if (start_x.has_value() && start_y.has_value()) {
    collect_axis_lock_candidates(cursor_x, cursor_y, *start_x, *start_y, tolerance, sketch, filter, candidates);
    if (filter.snap_parallel) {
      collect_parallel_candidates(cursor_x, cursor_y, *start_x, *start_y, tolerance, sketch, filter, candidates);
    }
    if (filter.snap_perpendicular) {
      collect_perpendicular_direction_candidates(cursor_x, cursor_y, *start_x, *start_y, tolerance, sketch, filter, candidates);
    }
  }

  if (candidates.empty()) {
    return std::nullopt;
  }

  // Build a priority map.
  const auto& priority = snap_priority.empty() ? kDefaultSnapPriority : snap_priority;
  std::unordered_set<std::string> priority_set(
      priority.begin(), priority.end());

  // Assign priority rank. Unknown snap types get lowest priority.
  auto priority_rank = [&](const std::string& kind) -> int {
    for (size_t i = 0; i < priority.size(); ++i) {
      if (priority[i] == kind) return static_cast<int>(i);
    }
    return static_cast<int>(priority.size());
  };

  // Find the best candidate: smallest distance first, then highest priority
  // as a tiebreaker when two candidates are at identical distance.
  const SnapCandidate* best = nullptr;
  double best_dist = std::numeric_limits<double>::max();
  int best_rank = std::numeric_limits<int>::max();

  for (const auto& c : candidates) {
    int rank = priority_rank(c.kind);
    if (c.distance < best_dist || (c.distance == best_dist && rank < best_rank)) {
      best = &c;
      best_dist = c.distance;
      best_rank = rank;
    }
  }

  if (best) {
    return *best;
  }
  return std::nullopt;
}

} // namespace polysmith::core
