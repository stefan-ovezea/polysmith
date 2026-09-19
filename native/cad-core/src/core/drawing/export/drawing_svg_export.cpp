#include "core/drawing/export/drawing_export.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace polysmith::core {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kEllipseSegments = 48;

/// Sheet coordinates are mathematical (y up); SVG is y down with the
/// origin top-left — flip y so the drawing reads upright.
double svg_y(double y, double height_mm) { return height_mm - y; }

std::string fmt(double v) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(3) << v;
  return out.str();
}

/// A partial ellipse (start..end angle along the major axis) — SVG
/// has no native partial-ellipse primitive, so tessellate to a
/// polyline in the core (deterministic, same fidelity as the
/// viewport).
std::string ellipse_polyline_points(const SheetPrimitive& p,
                                    double height_mm) {
  const double cx = p.center.value()[0];
  const double cy = p.center.value()[1];
  const double mx = p.major_dir.value()[0];
  const double my = p.major_dir.value()[1];
  const double major = p.major_radius.value();
  const double minor = p.minor_radius.value();
  const double sweep = p.end_angle - p.start_angle;
  const int steps = std::max(2, static_cast<int>(
      std::ceil(std::abs(sweep) / (2.0 * kPi) * kEllipseSegments)));
  std::ostringstream out;
  for (int i = 0; i <= steps; ++i) {
    const double a = p.start_angle + sweep * i / steps;
    const double x = cx + mx * major * std::cos(a) - my * minor * std::sin(a);
    const double y = cy + my * major * std::cos(a) + mx * minor * std::sin(a);
    out << fmt(x) << "," << fmt(svg_y(y, height_mm))
        << (i < steps ? " " : "");
  }
  return out.str();
}

}  // namespace

ExportResult export_sheet_as_svg(const SheetPrimitiveStream& stream,
                                 const std::string& file_path) {
  std::ostringstream out;
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""
      << fmt(stream.width_mm) << "mm\" height=\"" << fmt(stream.height_mm)
      << "mm\" viewBox=\"0 0 " << fmt(stream.width_mm) << " "
      << fmt(stream.height_mm) << "\">\n";
  out << "<rect width=\"" << fmt(stream.width_mm) << "\" height=\""
      << fmt(stream.height_mm) << "\" fill=\"#ffffff\"/>\n";

  int exported_count = 0;
  for (const auto& p : stream.primitives) {
    switch (p.kind[0]) {
      case 'l': {  // line (view geometry, dashes, glyphs, furniture)
        out << "<path d=\"M " << fmt(p.p0[0]) << ","
            << fmt(svg_y(p.p0[1], stream.height_mm)) << " L " << fmt(p.p1[0])
            << "," << fmt(svg_y(p.p1[1], stream.height_mm))
            << "\" fill=\"none\" stroke=\"#000000\" stroke-width=\""
            << fmt(p.style.width_mm) << "\"/>\n";
        break;
      }
      case 'c': {  // circle_arc
        // Sheet arcs sweep CCW in MATH coordinates (y up); after the
        // y-flip the same arc runs counter-clockwise on screen, which
        // is SVG sweep-flag 0 (sweep 1 is the positive-angle
        // direction in SVG's y-down space = clockwise on screen).
        const double large =
            std::abs(p.end_angle - p.start_angle) > kPi ? 1.0 : 0.0;
        out << "<path d=\"M " << fmt(p.p0[0]) << ","
            << fmt(svg_y(p.p0[1], stream.height_mm)) << " A "
            << fmt(p.radius.value()) << "," << fmt(p.radius.value())
            << " 0 " << large << ",0 " << fmt(p.p1[0]) << ","
            << fmt(svg_y(p.p1[1], stream.height_mm))
            << "\" fill=\"none\" stroke=\"#000000\" stroke-width=\""
            << fmt(p.style.width_mm) << "\"/>\n";
        break;
      }
      case 'e': {  // ellipse_arc — tessellated (see helper).
        out << "<path d=\"M "
            << ellipse_polyline_points(p, stream.height_mm)
            << "\" fill=\"none\" stroke=\"#000000\" stroke-width=\""
            << fmt(p.style.width_mm) << "\"/>\n";
        break;
      }
      case 'f': {  // filled_poly (dimension/section arrowheads)
        out << "<polygon points=\"";
        for (size_t i = 0; i < p.points.size(); ++i) {
          out << fmt(p.points[i][0]) << ","
              << fmt(svg_y(p.points[i][1], stream.height_mm))
              << (i + 1 < p.points.size() ? " " : "");
        }
        out << "\" fill=\"#000000\" stroke=\"none\"/>\n";
        break;
      }
      default:
        continue;  // unknown kind — skip defensively
    }
    ++exported_count;
  }
  // Text records stay DATA — the glyph primitives above render them
  // (see drawing_export.h).  The DXF backend is the one that emits
  // real text.
  out << "</svg>\n";

  std::ofstream file(file_path, std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot write SVG file: " + file_path);
  }
  file << out.str();
  file.close();
  if (!file.good()) {
    throw std::runtime_error("Cannot write SVG file: " + file_path);
  }
  return ExportResult{file_path, "svg", exported_count};
}

}  // namespace polysmith::core
