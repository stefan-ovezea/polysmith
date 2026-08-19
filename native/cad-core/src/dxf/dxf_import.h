#pragma once

#include <map>
#include <string>
#include <vector>

namespace polysmith::core {

// Plane-local (sketch u,v) geometry extracted from a DXF file. All
// coordinates are raw DXF values scaled by the unit factor when the
// file declares $INSUNITS == 1 (inches → mm).
struct DxfImportLine {
  double x1;
  double y1;
  double x2;
  double y2;
};

struct DxfImportCircle {
  double cx;
  double cy;
  double r;
};

// Arc endpoints lie ON the circle, matching SketchArc's cached coords.
struct DxfImportArc {
  double sx;
  double sy;
  double ex;
  double ey;
  double cx;
  double cy;
  double r;
  bool ccw;
};

struct DxfImportPoint {
  double x;
  double y;
};

struct DxfImportResult {
  std::vector<DxfImportLine> lines;
  std::vector<DxfImportCircle> circles;
  std::vector<DxfImportArc> arcs;
  std::vector<DxfImportPoint> points;
  // Entities that were skipped because they are unsupported (TEXT,
  // HATCH, INSERT blocks, ...) or degenerate/unusable (zero-length
  // lines, sub-minimum radii, non-planar polylines). Degenerate
  // entities are skipped BEFORE they can reach add_sketch_* (which
  // throws on invalid geometry) so a hostile file degrades instead of
  // failing the import.
  int skipped_count = 0;
  // Per-kind breakdown for the structured log, e.g. "TEXT": 3.
  std::map<std::string, int> skipped_by_kind;
  // Human-readable approximations applied during import (spline /
  // ellipse tessellation, units scaling).
  std::vector<std::string> warnings;
  // True when $INSUNITS == 1 triggered a ×25.4 scale.
  bool units_scaled = false;

  int entity_count() const {
    return static_cast<int>(lines.size() + circles.size() + arcs.size() +
                            points.size());
  }
};

// Parses `file_path` with libdxfrw and extracts the 2D geometry subset
// Polysmith sketches support. Throws std::runtime_error when the file
// is unreadable or not a DXF file.
DxfImportResult import_dxf_geometry(const std::string& file_path);

}  // namespace polysmith::core
