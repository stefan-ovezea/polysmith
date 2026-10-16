#include "core/pcb/gerber_polygonize.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace polysmith::core::pcb {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTiny = 1e-12;

cam2d::XY add(const cam2d::XY& a, const cam2d::XY& b) {
  return cam2d::XY{a.x + b.x, a.y + b.y};
}

cam2d::XY sub(const cam2d::XY& a, const cam2d::XY& b) {
  return cam2d::XY{a.x - b.x, a.y - b.y};
}

double length(const cam2d::XY& p) {
  return std::hypot(p.x, p.y);
}

cam2d::XY rotate(const cam2d::XY& p, double radians) {
  if (radians == 0.0) {
    return p;
  }
  const double c = std::cos(radians);
  const double s = std::sin(radians);
  return cam2d::XY{p.x * c - p.y * s, p.x * s + p.y * c};
}

/// Chords per full circle so the sagitta never exceeds `tolerance`.
int circle_segments(double radius, double tolerance) {
  if (radius <= tolerance || tolerance <= 0.0) {
    return 8;
  }
  double ratio = 1.0 - tolerance / radius;
  ratio = std::max(-1.0, std::min(1.0, ratio));
  const double half_angle = std::acos(ratio);
  if (half_angle <= kTiny) {
    return 512;
  }
  const int segments = static_cast<int>(std::ceil(kPi / half_angle));
  return std::max(8, std::min(512, segments));
}

std::vector<cam2d::XY> circle_points(const cam2d::XY& center, double radius,
                                     double tolerance, double rotation = 0.0) {
  std::vector<cam2d::XY> points;
  if (radius <= kTiny) {
    return points;
  }
  const int segments = circle_segments(radius, tolerance);
  points.reserve(static_cast<size_t>(segments));
  for (int index = 0; index < segments; ++index) {
    const double angle = rotation + 2.0 * kPi * index / segments;
    points.push_back(add(center, cam2d::XY{radius * std::cos(angle),
                                           radius * std::sin(angle)}));
  }
  return points;
}

/// Stadium between two points (the swept shape of a round aperture).
std::vector<cam2d::XY> capsule_points(const cam2d::XY& start, const cam2d::XY& end,
                                      double radius, double tolerance) {
  const cam2d::XY span = sub(end, start);
  const double span_length = length(span);
  if (span_length <= kTiny) {
    return circle_points(start, radius, tolerance);
  }
  const cam2d::XY unit{span.x / span_length, span.y / span_length};
  const cam2d::XY normal{-unit.y, unit.x};
  const double phi = std::atan2(normal.y, normal.x);
  const int steps = std::max(4, circle_segments(radius, tolerance) / 2 + 2);

  std::vector<cam2d::XY> points;
  points.reserve(static_cast<size_t>(2 * steps + 2));
  // Cap around `end`, sweeping from +normal round to -normal.
  for (int index = 0; index <= steps; ++index) {
    const double angle = phi - kPi * index / steps;
    points.push_back(add(end, cam2d::XY{radius * std::cos(angle),
                                        radius * std::sin(angle)}));
  }
  // Cap around `start`, sweeping from -normal back to +normal.
  for (int index = 0; index <= steps; ++index) {
    const double angle = phi + kPi - kPi * index / steps;
    points.push_back(add(start, cam2d::XY{radius * std::cos(angle),
                                          radius * std::sin(angle)}));
  }
  return points;
}

std::vector<cam2d::XY> rectangle_points(const cam2d::XY& center, double width,
                                        double height, double rotation) {
  const double half_width = width / 2.0;
  const double half_height = height / 2.0;
  std::vector<cam2d::XY> points;
  points.reserve(4);
  for (const cam2d::XY& corner : {cam2d::XY{-half_width, -half_height},
                                  cam2d::XY{half_width, -half_height},
                                  cam2d::XY{half_width, half_height},
                                  cam2d::XY{-half_width, half_height}}) {
    points.push_back(add(center, rotate(corner, rotation)));
  }
  return points;
}

std::vector<cam2d::XY> obround_points(const cam2d::XY& center, double width,
                                      double height, double rotation,
                                      double tolerance) {
  std::vector<cam2d::XY> points;
  if (std::abs(width - height) <= kTiny) {
    points = circle_points(center, width / 2.0, tolerance);
  } else if (width > height) {
    const double offset = (width - height) / 2.0;
    points = capsule_points(cam2d::XY{center.x - offset, center.y},
                            cam2d::XY{center.x + offset, center.y}, height / 2.0,
                            tolerance);
  } else {
    const double offset = (height - width) / 2.0;
    points = capsule_points(cam2d::XY{center.x, center.y - offset},
                            cam2d::XY{center.x, center.y + offset}, width / 2.0,
                            tolerance);
  }
  if (rotation != 0.0) {
    for (cam2d::XY& point : points) {
      point = add(center, rotate(sub(point, center), rotation));
    }
  }
  return points;
}

std::vector<cam2d::XY> regular_polygon_points(const cam2d::XY& center,
                                              double diameter, int vertices,
                                              double rotation) {
  std::vector<cam2d::XY> points;
  if (vertices < 3 || diameter <= kTiny) {
    return points;
  }
  const double radius = diameter / 2.0;
  points.reserve(static_cast<size_t>(vertices));
  for (int index = 0; index < vertices; ++index) {
    const double angle = rotation + 2.0 * kPi * index / vertices;
    points.push_back(add(center, cam2d::XY{radius * std::cos(angle),
                                           radius * std::sin(angle)}));
  }
  return points;
}

/// Sweeps a rectangle aperture along a segment: the convex hull of the two
/// end rectangles.  Exact (no round caps) — that is what a rectangular
/// aperture draws.
std::vector<cam2d::XY> rectangle_sweep(const cam2d::XY& start, const cam2d::XY& end,
                                       double width, double height) {
  std::vector<cam2d::XY> corners;
  for (const cam2d::XY& center : {start, end}) {
    for (const cam2d::XY& corner : rectangle_points(center, width, height, 0.0)) {
      corners.push_back(corner);
    }
  }
  std::sort(corners.begin(), corners.end(), [](const cam2d::XY& a, const cam2d::XY& b) {
    return a.x < b.x || (a.x == b.x && a.y < b.y);
  });
  const auto cross = [](const cam2d::XY& o, const cam2d::XY& a, const cam2d::XY& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
  };
  std::vector<cam2d::XY> hull(2 * corners.size());
  size_t size = 0;
  for (const cam2d::XY& point : corners) {  // lower hull
    while (size >= 2 && cross(hull[size - 2], hull[size - 1], point) <= 0.0) {
      --size;
    }
    hull[size++] = point;
  }
  const size_t lower = size + 1;
  for (size_t index = corners.size(); index-- > 0;) {  // upper hull
    const cam2d::XY& point = corners[index];
    while (size >= lower && cross(hull[size - 2], hull[size - 1], point) <= 0.0) {
      --size;
    }
    hull[size++] = point;
  }
  hull.resize(size > 0 ? size - 1 : 0);
  return hull;
}

/// Samples a multi-quadrant arc (G75 semantics) from start to end around
/// the centre at start + (i, j).
std::vector<cam2d::XY> arc_points(const cam2d::XY& start, const cam2d::XY& end,
                                  double i, double j, bool clockwise,
                                  double tolerance, size_t* skipped) {
  std::vector<cam2d::XY> points;
  const cam2d::XY center{start.x + i, start.y + j};
  const double radius = length(cam2d::XY{i, j});
  if (radius <= kTiny) {
    if (skipped != nullptr) {
      ++(*skipped);
    }
    return points;
  }
  const double start_angle = std::atan2(start.y - center.y, start.x - center.x);
  const double end_angle = std::atan2(end.y - center.y, end.x - center.x);
  double sweep = end_angle - start_angle;
  const bool full_circle = length(sub(end, start)) <= tolerance * 0.5;
  if (full_circle) {
    sweep = clockwise ? -2.0 * kPi : 2.0 * kPi;
  } else if (clockwise) {
    while (sweep >= 0.0) {
      sweep -= 2.0 * kPi;
    }
  } else {
    while (sweep <= 0.0) {
      sweep += 2.0 * kPi;
    }
  }
  const int per_circle = circle_segments(radius, tolerance);
  const int steps = std::max(
      2, std::min(2048, static_cast<int>(
                            std::ceil(std::abs(sweep) / (2.0 * kPi) * per_circle))));
  points.reserve(static_cast<size_t>(steps) + 1);
  for (int index = 0; index <= steps; ++index) {
    const double angle = start_angle + sweep * index / steps;
    points.push_back(add(center, cam2d::XY{radius * std::cos(angle),
                                           radius * std::sin(angle)}));
  }
  return points;
}

/// Drops consecutive duplicates and a repeated closing point.
void clean_ring(std::vector<cam2d::XY>& points, double tolerance) {
  std::vector<cam2d::XY> cleaned;
  cleaned.reserve(points.size());
  const double epsilon = std::max(tolerance * 0.01, 1e-9);
  for (const cam2d::XY& point : points) {
    if (!cleaned.empty() && length(sub(point, cleaned.back())) <= epsilon) {
      continue;
    }
    cleaned.push_back(point);
  }
  while (cleaned.size() >= 2 && length(sub(cleaned.front(), cleaned.back())) <= epsilon) {
    cleaned.pop_back();
  }
  points.swap(cleaned);
}

class Polygonizer {
 public:
  Polygonizer(const GerberFile& file, double tolerance)
      : file_(file), tolerance_(tolerance > 0.0 ? tolerance : kArcToleranceMm) {
    // Aperture and macro parameters are written in file units.
    unit_scale_ = file.format.unit == PcbUnit::Inch ? 25.4 : 1.0;
  }

  PcbLayerGeometry run() {
    PcbLayerGeometry geometry;
    bool in_region = false;
    std::vector<cam2d::XY> region_points;
    cam2d::XY current{0.0, 0.0};
    int region_line = 0;
    bool region_dark = true;
    int region_aperture = 0;

    const auto flush_region = [&]() {
      if (region_points.size() >= 3) {
        PcbRing ring;
        ring.points = region_points;
        ring.dark = region_dark;
        ring.aperture_code = region_aperture;
        ring.line = region_line;
        ring.from_region = true;
        push(geometry, ring);
      } else if (!region_points.empty()) {
        ++skipped_operations;
      }
      region_points.clear();
    };

    for (const GerberOperation& operation : file_.operations) {
      const cam2d::XY target{operation.x, operation.y};
      switch (operation.kind) {
        case GerberOperation::Kind::RegionBegin:
          in_region = true;
          region_points.clear();
          region_line = operation.line;
          region_dark = operation.dark;
          region_aperture = operation.aperture_code;
          current = target;
          break;
        case GerberOperation::Kind::RegionEnd:
          flush_region();
          in_region = false;
          current = target;
          break;
        case GerberOperation::Kind::Move:
          if (in_region) {
            // A new D02 inside a region starts another contour — the
            // previous one is a complete ring (this is how regions carry
            // holes).
            flush_region();
          }
          current = target;
          break;
        case GerberOperation::Kind::Line:
        case GerberOperation::Kind::Arc: {
          std::vector<cam2d::XY> segment;
          if (operation.kind == GerberOperation::Kind::Arc) {
            segment = arc_points(current, target, operation.i, operation.j,
                                 operation.clockwise, tolerance_, &skipped_operations);
          } else {
            segment.push_back(current);
            segment.push_back(target);
          }
          if (in_region) {
            if (region_points.empty()) {
              region_points.push_back(current);
            }
            for (const cam2d::XY& point : segment) {
              region_points.push_back(point);
            }
          } else {
            stroke(geometry, current, target, operation, segment);
          }
          current = target;
          break;
        }
        case GerberOperation::Kind::Flash:
          flash(geometry, target, operation);
          current = target;
          break;
      }
    }
    if (in_region) {
      flush_region();
      geometry.diagnostics.push_back(
          {region_line, "region never closed (missing G37) — contour kept"});
    }
    geometry.skipped_operations = skipped_operations;
    return geometry;
  }

 private:
  void push(PcbLayerGeometry& geometry, PcbRing& ring) {
    clean_ring(ring.points, tolerance_);
    if (ring.points.size() < 3) {
      ++skipped_operations;
      return;
    }
    for (const cam2d::XY& point : ring.points) {
      if (!geometry.bounds.valid) {
        geometry.bounds = GerberBounds{point.x, point.y, point.x, point.y, true};
      } else {
        geometry.bounds.min_x = std::min(geometry.bounds.min_x, point.x);
        geometry.bounds.min_y = std::min(geometry.bounds.min_y, point.y);
        geometry.bounds.max_x = std::max(geometry.bounds.max_x, point.x);
        geometry.bounds.max_y = std::max(geometry.bounds.max_y, point.y);
      }
    }
    geometry.rings.push_back(std::move(ring));
  }

  const GerberAperture* aperture(int code) const {
    const auto found = file_.apertures.find(code);
    return found == file_.apertures.end() ? nullptr : &found->second;
  }

  static double parameter(const GerberAperture& aperture, size_t index,
                          double fallback = 0.0) {
    return index < aperture.params.size() ? aperture.params[index] : fallback;
  }

  /// Strokes a segment with the current aperture.
  void stroke(PcbLayerGeometry& geometry, const cam2d::XY& start,
              const cam2d::XY& end, const GerberOperation& operation,
              const std::vector<cam2d::XY>& sampled_arc) {
    const GerberAperture* current = aperture(operation.aperture_code);
    if (current == nullptr) {
      ++skipped_operations;
      geometry.diagnostics.push_back(
          {operation.line, "stroke with no aperture in effect — skipped"});
      return;
    }
    std::vector<cam2d::XY> outline;
    switch (current->kind) {
      case ApertureKind::Circle:
        outline = capsule_points(start, end, parameter(*current, 0) * unit_scale_ / 2.0,
                                 tolerance_);
        break;
      case ApertureKind::Rectangle:
        outline = rectangle_sweep(start, end, parameter(*current, 0) * unit_scale_,
                                  parameter(*current, 1) * unit_scale_);
        break;
      case ApertureKind::Obround: {
        const double width = parameter(*current, 0) * unit_scale_;
        const double height = parameter(*current, 1) * unit_scale_;
        outline = capsule_points(start, end, std::min(width, height) / 2.0, tolerance_);
        geometry.diagnostics.push_back(
            {operation.line, "obround aperture stroke approximated as a round"
                             " stroke of its short dimension"});
        break;
      }
      case ApertureKind::Polygon: {
        outline = capsule_points(start, end, parameter(*current, 0) * unit_scale_ / 2.0,
                                 tolerance_);
        geometry.diagnostics.push_back(
            {operation.line, "polygon aperture stroke approximated as a round stroke"});
        break;
      }
      case ApertureKind::Macro:
        // Macro apertures are pad templates in practice; a stroked macro
        // would need the full Minkowski sum.
        ++skipped_operations;
        geometry.diagnostics.push_back(
            {operation.line, "macro aperture used as a stroke — skipped"});
        return;
    }
    if (outline.size() < 3 && sampled_arc.size() >= 2) {
      // Degenerate aperture: fall back to the centerline so the trace is
      // at least visible as geometry.
      outline = sampled_arc;
    }
    PcbRing ring;
    ring.points = std::move(outline);
    ring.dark = operation.dark;
    ring.aperture_code = operation.aperture_code;
    ring.line = operation.line;
    push(geometry, ring);
  }

  void flash(PcbLayerGeometry& geometry, const cam2d::XY& at,
             const GerberOperation& operation) {
    const GerberAperture* current = aperture(operation.aperture_code);
    if (current == nullptr) {
      ++skipped_operations;
      geometry.diagnostics.push_back(
          {operation.line, "flash with no aperture in effect — skipped"});
      return;
    }
    if (current->kind != ApertureKind::Macro) {
      std::vector<cam2d::XY> shape;
      switch (current->kind) {
        case ApertureKind::Circle:
          shape = circle_points(at, parameter(*current, 0) * unit_scale_ / 2.0,
                                tolerance_);
          break;
        case ApertureKind::Rectangle:
          shape = rectangle_points(at, parameter(*current, 0) * unit_scale_,
                                   parameter(*current, 1) * unit_scale_, 0.0);
          break;
        case ApertureKind::Obround:
          shape = obround_points(at, parameter(*current, 0) * unit_scale_,
                                 parameter(*current, 1) * unit_scale_, 0.0, tolerance_);
          break;
        case ApertureKind::Polygon:
          shape = regular_polygon_points(
              at, parameter(*current, 0) * unit_scale_,
              static_cast<int>(parameter(*current, 1, 3.0)),
              parameter(*current, 2) * kPi / 180.0);
          break;
        case ApertureKind::Macro:
          return;  // handled above
      }
      if (shape.size() < 3) {
        ++skipped_operations;
        return;
      }
      PcbRing ring;
      ring.points = std::move(shape);
      ring.dark = operation.dark;
      ring.aperture_code = operation.aperture_code;
      ring.line = operation.line;
      push(geometry, ring);
      return;
    }

    const auto macro = file_.macros.find(current->macro_name);
    if (macro == file_.macros.end()) {
      ++skipped_operations;
      geometry.diagnostics.push_back(
          {operation.line, "aperture macro '" + current->macro_name +
                               "' is used but never defined — flash skipped"});
      return;
    }
    std::vector<double> args = current->macro_args;
    for (double& argument : args) {
      argument *= unit_scale_;
    }
    std::vector<PcbDiagnostic> macro_diagnostics;
    const ResolvedMacro resolved =
        resolve_macro(macro->second, args, &macro_diagnostics, operation.line);
    geometry.diagnostics.insert(geometry.diagnostics.end(), macro_diagnostics.begin(),
                                macro_diagnostics.end());
    if (!resolved.supported) {
      ++skipped_operations;
      return;
    }
    for (const MacroPrimitive& primitive : resolved.primitives) {
      std::vector<cam2d::XY> shape = macro_shape(primitive, at, operation.line,
                                                 geometry.diagnostics);
      if (shape.size() < 3) {
        ++skipped_operations;
        continue;
      }
      PcbRing ring;
      ring.points = std::move(shape);
      // Exposure inside a macro is relative to the flash's polarity.
      ring.dark = operation.dark && primitive.exposure_dark;
      ring.aperture_code = operation.aperture_code;
      ring.line = operation.line;
      ring.macro_primitive = primitive.code;
      push(geometry, ring);
    }
  }

  std::vector<cam2d::XY> macro_shape(const MacroPrimitive& primitive,
                                     const cam2d::XY& at, int line,
                                     std::vector<PcbDiagnostic>& diagnostics) {
    const std::vector<double>& p = primitive.params;
    const auto value = [&p](size_t index) {
      return index < p.size() ? p[index] : 0.0;
    };
    const double rotation = p.empty() ? 0.0 : value(p.size() - 1) * kPi / 180.0;
    std::vector<cam2d::XY> points;
    switch (primitive.code) {
      case 1: {  // circle: diameter, x, y, [rotation]
        const cam2d::XY center = add(at, rotate(cam2d::XY{value(1), value(2)}, rotation));
        points = circle_points(center, value(0) / 2.0, tolerance_);
        break;
      }
      case 4: {  // outline: n, x1,y1 … xn,yn, rotation
        const int vertices = static_cast<int>(value(0));
        if (vertices < 3 || p.size() < static_cast<size_t>(1 + 2 * vertices + 1)) {
          diagnostics.push_back({line, "macro outline primitive has too few points"});
          break;
        }
        points.reserve(static_cast<size_t>(vertices));
        for (int index = 0; index < vertices; ++index) {
          const cam2d::XY corner{value(1 + 2 * index), value(2 + 2 * index)};
          points.push_back(add(at, rotate(corner, rotation)));
        }
        break;
      }
      case 5: {  // polygon: vertices, x, y, diameter, rotation
        const cam2d::XY center = add(at, rotate(cam2d::XY{value(1), value(2)}, rotation));
        points = regular_polygon_points(center, value(3), static_cast<int>(value(0)),
                                        rotation);
        break;
      }
      case 20: {  // vector line: width, x1,y1, x2,y2, rotation
        const cam2d::XY start =
            add(at, rotate(cam2d::XY{value(1), value(2)}, rotation));
        const cam2d::XY end = add(at, rotate(cam2d::XY{value(3), value(4)}, rotation));
        const cam2d::XY span = sub(end, start);
        const double span_length = length(span);
        if (span_length <= kTiny) {
          break;
        }
        const cam2d::XY normal{-span.y / span_length * value(0) / 2.0,
                               span.x / span_length * value(0) / 2.0};
        points = {add(start, normal), add(end, normal), sub(end, normal),
                  sub(start, normal)};
        break;
      }
      case 21: {  // rect: width, height, x, y, rotation
        const cam2d::XY center = add(at, rotate(cam2d::XY{value(2), value(3)}, rotation));
        points = rectangle_points(center, value(0), value(1), rotation);
        break;
      }
      case 22: {  // obround: width, height, x, y, rotation
        const cam2d::XY center = add(at, rotate(cam2d::XY{value(2), value(3)}, rotation));
        points = obround_points(center, value(0), value(1), rotation, tolerance_);
        break;
      }
      default:
        diagnostics.push_back({line, "unsupported macro primitive " +
                                         std::to_string(primitive.code)});
        break;
    }
    return points;
  }

  const GerberFile& file_;
  double tolerance_;
  double unit_scale_ = 1.0;
  size_t skipped_operations = 0;
};

}  // namespace

PcbLayerGeometry polygonize_gerber(const GerberFile& file, double tolerance_mm) {
  Polygonizer polygonizer(file, tolerance_mm);
  return polygonizer.run();
}

}  // namespace polysmith::core::pcb
