#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace polysmith::core {

// ── Drawing sheet emission (viewport payload) ─────────────────────
//
// The drawing workspace draws sheets with their projected views.
// Curves are emitted in SHEET-mm (view space transformed by the
// view's scale + sheet position) so the UI renderer needs no drawing
// math.  P3 emits the projection edges + view metadata; the full
// flattened stream (frame, centring marks, grid refs, title block)
// lands in P5 on top of this same payload shape.

/// One exact curve on the sheet: line / circle arc / ellipse arc.
struct ViewportDrawingCurve {
  /// "line" | "circle" | "ellipse"
  std::string kind = "line";
  /// "visible" | "hidden"
  std::string line_class = "visible";
  /// "sharp" | "smooth" | "seam" | "outline" | "cutting_plane" |
  /// "hatch" (P4: type-H chain line traces from sibling sections,
  /// and scanline hatching on section views)
  std::string curve_class = "sharp";
  /// "view_geometry" | "hatch" | "cutting_plane" | "frame" |
  /// "centring_mark" | "grid_ref" | "projection_symbol" — from the
  /// P5 flattened stream (the furniture purposes render in the
  /// border color).
  std::string purpose = "view_geometry";
  /// ISO line-group width in mm (P5: dash patterns are already
  /// applied by the core flatten — the renderer draws continuous
  /// ribbons of this width only).
  double width_mm = 0.5;
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
};

/// One view on the sheet: label + degradation state (the curves
/// themselves are emitted flat on the sheet).
struct ViewportDrawingView {
  std::string view_id;
  std::string label;          // standard view name or "View N"
  double scale = 1.0;
  /// View origin on the sheet (sheet-mm).
  std::array<double, 2> origin = {0.0, 0.0};
  /// Content bounds (sheet-mm) — the UI fits the camera to the union.
  std::array<double, 2> min = {0.0, 0.0};
  std::array<double, 2> max = {0.0, 0.0};
  bool stale = false;
  std::string warning;        // dependency_warning text
};

/// One sheet with its curves and views.
struct ViewportDrawingSheet {
  std::string sheet_id;
  std::string drawing_id;
  std::string name;
  double width_mm = 210.0;
  double height_mm = 297.0;
  std::vector<ViewportDrawingCurve> curves;
  std::vector<ViewportDrawingView> views;
};

}  // namespace polysmith::core
