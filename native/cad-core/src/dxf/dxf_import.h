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

// A DXF SPLINE that the sketch's own spline entity can represent EXACTLY:
// the model stores a clamped open-uniform B-spline whose poles are the
// control points (see spline_math.h), so a curve with matching degree and
// knots imports as a curve — not as tessellated lines.  Anything else
// (weights, custom knot spacing, higher degree) falls back to tessellation.
struct DxfImportSpline {
  std::vector<std::pair<double, double>> poles;
  int degree = 3;
  int line = 0;
};

// A FULL elliptical DXF ELLIPSE, exactly representable by SketchEllipse:
// `axis_a`/`axis_b` are endpoints relative to the centre (the model takes
// axis POINTS).  Partial ellipses fall back to tessellation for now.
struct DxfImportEllipse {
  double center_x = 0.0;
  double center_y = 0.0;
  double axis_a_x = 0.0;
  double axis_a_y = 0.0;
  double axis_b_x = 0.0;
  double axis_b_y = 0.0;
  int line = 0;
};

struct DxfImportResult {
  std::vector<DxfImportLine> lines;
  std::vector<DxfImportCircle> circles;
  std::vector<DxfImportArc> arcs;
  std::vector<DxfImportPoint> points;
  // Curves imported EXACTLY, as the sketch's own entities.  These are the
  // preferred outcome: tessellating a spline into lines is what produces
  // faceted extrusions, and the sketch model already carries B-splines and
  // ellipses through profile detection into the B-rep.
  std::vector<DxfImportSpline> splines;
  std::vector<DxfImportEllipse> ellipses;
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
  //
  // PER-KIND COUNTS, not per-entity: a Fusion sketch export has hundreds of
  // splines, and one message each flooded the Logs panel until the 500-entry
  // ring evicted everything before it — the user saw the counter appear to go
  // DOWN.  These are logged once, as a summary.
  int splines_tessellated = 0;
  int ellipses_tessellated = 0;
  std::vector<std::string> warnings;
  // True when $INSUNITS == 1 triggered a ×25.4 scale.
  bool units_scaled = false;

  int entity_count() const {
    return static_cast<int>(lines.size() + circles.size() + arcs.size() +
                            points.size() + splines.size() + ellipses.size());
  }
};

// Parses `file_path` with libdxfrw and extracts the 2D geometry subset
// Polysmith sketches support. Throws std::runtime_error when the file
// is unreadable or not a DXF file.
DxfImportResult import_dxf_geometry(const std::string& file_path);

}  // namespace polysmith::core
