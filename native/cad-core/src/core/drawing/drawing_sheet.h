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
  /// "line" | "circle_arc" | "ellipse_arc" | "filled_poly"
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
  /// Polygon vertices (sheet-mm) for "filled_poly" — closed filled
  /// paths: dimension arrowheads today, any solid fill later.
  std::vector<std::array<double, 2>> points;
  SheetLineStyle style;
  /// Why the primitive exists: "view_geometry" | "hatch" |
  /// "cutting_plane" | "frame" | "centring_mark" | "grid_ref" |
  /// "projection_symbol" | "title_block" | "dimension" |
  /// "section_label" | "text_glyph" — the backends and the UI can
  /// filter on it (e.g. the DXF layer split).
  std::string purpose = "view_geometry";
  /// "visible" | "hidden" — kept for coloring even though dash
  /// patterns are already applied.
  std::string line_class = "visible";
  /// Cutting-plane traces only: the sibling section's label + sight
  /// direction (unit 2D vector) — the flatten emits the A–A labels
  /// and arrows from these.
  std::string section_label;
  std::optional<std::array<double, 2>> trace_sight_dir;
};

/// A text record in the flattened stream (sheet-mm).  Kept as DATA
/// rather than glyph geometry so the DXF backend can emit a real
/// DRW_Text.  P7 additionally emits vector glyph line primitives
/// ("text_glyph") for every record — the viewport and the PDF/SVG
/// backends render those, the DXF backend renders the DATA.
struct SheetText {
  std::string text;
  /// Anchor position (sheet-mm) — the anchor is the text CENTER.
  std::array<double, 2> position = {0.0, 0.0};
  /// ISO 3098 letter height in mm (dimension numerals 3.5).
  double height_mm = 3.5;
  double angle_deg = 0.0;
  /// "left" | "center" | "right"
  std::string h_align = "center";
  /// Why the text exists: "dimension" | "title_block" |
  /// "section_label".
  std::string purpose = "dimension";
  /// true when the dimension is degraded (last-known value shown).
  bool stale = false;
};

/// Semantic dimension record (sheet-mm) — the P9 annotated-DXF
/// backend emits a real DIMENSION entity from this instead of the
/// exploded graphics.  Populated by the graphics builder alongside
/// the primitives (same computation, one source of truth); the
/// geometry-mode backends ignore it.
struct SheetDimension {
  /// "linear" | "radius" | "diameter" | "angular"
  std::string kind = "linear";
  /// Dimension-line location (code 10) — for radius/diameter: the
  /// center; for angular: second line 2-2 point.
  std::array<double, 2> def_point = {0.0, 0.0};
  /// Text middle point (code 11).
  std::array<double, 2> text_point = {0.0, 0.0};
  /// Definition points 1/2 (codes 13/14).
  std::array<double, 2> def1 = {0.0, 0.0};
  std::array<double, 2> def2 = {0.0, 0.0};
  /// Arc point for radius/diameter (code 15); angular second line 1.
  std::optional<std::array<double, 2>> arc_point;
  /// Angular arc location (code 16).
  std::optional<std::array<double, 2>> dim_point;
  /// Radial/diametric leader length (code 40).
  std::optional<double> leader_length;
  /// The formatted measurement text (code 1).
  std::string text;
  /// Dimension style name (code 3).
  std::string style = "POLYSMITH_ISO";
};

/// One hatch region boundary in sheet-mm — the annotated-DXF backend
/// emits a HATCH entity (ANSI31 predefined pattern, boundary loops
/// decomposed to LINE edges) from this instead of the scanline
/// primitives.
struct SheetHatchRegion {
  std::vector<std::array<double, 2>> outer_loop;
  std::vector<std::vector<std::array<double, 2>>> holes;
  /// The scanline angle used for the line hatch too.
  double angle_deg = 45.0;
  /// Scanline spacing (the DXF pattern scale derives from it).
  double spacing_mm = 3.0;
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
  std::vector<SheetText> texts;
  /// Semantic records (P9): the annotated-DXF backend consumes these
  /// instead of the exploded graphics (dimension primitives/texts,
  /// hatch scanlines).  Always populated; geometry-mode backends
  /// ignore them.
  std::vector<SheetDimension> dimensions;
  std::vector<SheetHatchRegion> hatch_regions;
  /// The document's dimension decimal separator — the annotated-DXF
  /// DIMSTYLE dimdsep mirrors it.
  std::string decimal_separator = ",";
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

/// Geometry of a view that is NOT (yet) on the sheet — the Insert
/// View live preview.  Flattened exactly like a committed view
/// (coincidence priority + ISO 128-2 dashing + section labels),
/// memory-only, never cached.  Sibling sections of the drawing still
/// trace their cutting planes onto the preview; the preview's own
/// (uncommitted) section does not.
struct ViewPreviewGeometry {
  std::vector<SheetPrimitive> primitives;
  std::vector<SheetText> texts;
  /// Content bounds (sheet-mm) of the view geometry — nullopt when
  /// the projection produced no geometry.
  std::optional<std::array<double, 2>> min;
  std::optional<std::array<double, 2>> max;
  /// Dependency-degradation warning (missing body / unresolvable
  /// frame); the UI shows it and drops the preview graphics.
  std::string warning;
};

/// Projects + flattens a preview view definition.  Returns nullopt
/// when the drawing does not exist; a dependency problem yields a
/// geometry with `warning` set instead of a failure — the panel
/// shows the reason rather than a silent empty preview.
std::optional<ViewPreviewGeometry> preview_view_geometry(
    const DocumentState& document, const std::string& drawing_id,
    const DrawingView& def);

}  // namespace polysmith::core
