#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "core/drawing/drawing_types.h"

namespace polysmith::core {

struct DocumentState;

// ══════════════════════════════════════════════════════════════════
//  Flattened sheet primitive stream (P5)
// ══════════════════════════════════════════════════════════════════
//
// One canonical flattened stream per sheet: view geometry transformed
// to sheet-mm with ISO 128-2 line styles APPLIED (dash patterns are
// computed HERE in the core, with the Annex-A corner rule — every
// segment starts with a dash, no segment ends with a gap), plus the
// ISO 5457 furniture (frame, centring marks, grid-reference ticks)
// and the ISO 5456-2 projection symbol.  The viewport emission
// consumes this stream today; the SVG/DXF/PDF backends consume the
// same stream in P8/P9 — consistency by construction.

struct SheetLineStyle {
  /// "continuous" | "dashed" | "chain"
  std::string line_type = "continuous";
  /// ISO line-group width in mm (0.5 thick / 0.25 thin at A3/A4;
  /// the frame is 0.7 per ISO 5457).
  double width_mm = 0.25;
};

struct SheetPrimitive {
  /// "line" | "circle_arc" | "ellipse_arc"
  std::string kind = "line";
  /// Endpoints (sheet-mm) — always populated.
  std::array<double, 2> p0 = {0.0, 0.0};
  std::array<double, 2> p1 = {0.0, 0.0};
  // Circle/ellipse geometry (sheet-mm), populated for those kinds.
  std::optional<std::array<double, 2>> center;
  std::optional<double> radius;                     // circle
  std::optional<std::array<double, 2>> major_dir;   // ellipse, unit
  std::optional<double> major_radius;               // ellipse
  std::optional<double> minor_radius;               // ellipse
  /// Angular parameters (radians) for circle/ellipse arcs.
  double start_angle = 0.0;
  double end_angle = 0.0;
  SheetLineStyle style;
  /// Why the primitive exists: "view_geometry" | "hatch" |
  /// "cutting_plane" | "frame" | "centring_mark" | "grid_ref" |
  /// "projection_symbol" — the backends and the UI can filter on it
  /// (e.g. the DXF layer split).
  std::string purpose = "view_geometry";
  /// "visible" | "hidden" — kept for coloring even though dash
  /// patterns are already applied.
  std::string line_class = "visible";
};

/// View metadata on the flattened sheet (content bounds in sheet-mm)
/// — the UI draws the view frames and labels from this.
struct SheetViewBounds {
  std::string view_id;
  std::string label;
  double scale = 1.0;
  /// View origin on the sheet (sheet-mm).
  std::array<double, 2> origin = {0.0, 0.0};
  /// Content bounds (sheet-mm).
  std::array<double, 2> min = {0.0, 0.0};
  std::array<double, 2> max = {0.0, 0.0};
  bool stale = false;
  std::string warning;
};

struct SheetPrimitiveStream {
  std::string sheet_id;
  std::string drawing_id;
  /// Trimmed sheet size in mm (orientation applied).
  double width_mm = 210.0;
  double height_mm = 297.0;
  std::vector<SheetPrimitive> primitives;
  std::vector<SheetViewBounds> views;
};

/// ISO 5457 trimmed sheet sizes (portrait): A0..A4.
std::array<double, 2> paper_size_mm(const std::string& paper_size);

/// Flattens one sheet of a drawing: furniture + every view on it,
/// in sheet-mm, deterministically ordered.  Views without a cached
/// projection (not yet computed / stale) contribute nothing but
/// their metadata.  Returns nullopt when the drawing or sheet does
/// not exist.
std::optional<SheetPrimitiveStream> flatten_sheet(
    const DocumentState& document, const std::string& drawing_id,
    const std::string& sheet_id);

}  // namespace polysmith::core
