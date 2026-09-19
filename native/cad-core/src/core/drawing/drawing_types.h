#pragma once

#include <array>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "core/cam/cam_types.h"  // FaceAttestation (silhouette provenance)

namespace polysmith::core {

// ══════════════════════════════════════════════════════════════════
//  ISO Drawing Workspace — document data model
// ══════════════════════════════════════════════════════════════════
//
// Everything here is persisted inside the .polysmith document (the
// "drawing" key of DocumentState, parallel to "cam").  Generated
// projection output (ProjectionResult) is declared at the bottom but
// is RUNTIME-ONLY: it lives in drawing_runtime and is never
// serialized — the same contract CAM toolpaths follow.

// ── View frame ────────────────────────────────────────────────────

/// A resolved projection frame in document space: the projection
/// plane passes through `origin`, `normal` is the view direction
/// (towards the viewer), and `x_direction` is the sheet +X axis.
struct DrawingViewFrame {
  std::array<double, 3> origin = {0.0, 0.0, 0.0};
  std::array<double, 3> normal = {0.0, 0.0, 1.0};
  std::array<double, 3> x_direction = {1.0, 0.0, 0.0};
};

// ── TNP-safe source edge witness ──────────────────────────────────

/// Witness data to re-identify a body edge after topology changes —
/// the drawing analogue of EdgeAttestation, plus the parameter range
/// the projection actually used.  Dimensions attach to THIS, never to
/// a projection ordinal or sheet coordinates (see the TNP mantra).
struct SourceEdgeWitness {
  std::string body_id;   // owning body/feature id
  int src_edge_index = -1;  // hint: index in the body compile edge list
  std::array<double, 3> start_point = {0.0, 0.0, 0.0};
  std::array<double, 3> end_point = {0.0, 0.0, 0.0};
  double length = 0.0;
  std::array<double, 3> tangent = {1.0, 0.0, 0.0};
  // "line" | "circle" | "ellipse" | "bspline" | "other"
  std::string curve_kind = "line";
  // Circle/arc witness — present only for circular edges (a closed rim
  // has start == end, so endpoints alone cannot identify it).
  std::optional<std::array<double, 3>> center;
  std::optional<std::array<double, 3>> axis;
  std::optional<double> radius;
  // Source-parameter range the projected edge covers.
  std::array<double, 2> param_range = {0.0, 0.0};
};

// ── Section definition ────────────────────────────────────────────

/// A section view cuts the model along a plane.  Sections reference
/// the MODEL, never another view — the base view is orientation only,
/// so the dependency graph stays a DAG rooted at model features.
struct SectionDefinition {
  std::array<double, 3> cutting_plane_point = {0.0, 0.0, 0.0};
  std::array<double, 3> cutting_plane_normal = {0.0, 0.0, 1.0};
  /// true = cut away material on the normal side (halfspace cut);
  /// false = section-only (no removal).
  bool cut_away = true;
  std::string label = "A";
  /// ISO 128-3 §7: thin hatching, 45° preferred (30°/60° allowed),
  /// spacing 0.7–3 mm.
  double hatch_angle_deg = 45.0;
  double hatch_spacing_mm = 3.0;
  /// Optional construction plane the cutting plane snaps to (the
  /// plane is resolved into point/normal at view creation).
  std::optional<std::string> construction_plane_id;
};

// ── View ──────────────────────────────────────────────────────────

/// "projection" | "section" | "axonometric"
using DrawingViewKind = std::string;

/// A single view on a sheet.  Views belong to exactly one sheet; the
/// sheet's view_ids list orders them.  Views carry real dependency
/// edges on (source_body_ids, section, sheet policy) and recompute
/// inside the single existing refresh pass.
struct DrawingView {
  std::string view_id;
  DrawingViewKind kind = "projection";
  /// "front" | "top" | "right" | "bottom" | "left" | "back" — UI
  /// convenience only; the refresh derives the frame.  Empty for
  /// custom frames / sections / axonometric views.
  std::string standard_view;
  /// Resolved custom frame — authoritative when standard_view is
  /// empty and kind != "section".
  std::optional<DrawingViewFrame> custom_frame;
  std::vector<std::string> source_body_ids;
  /// View scale (ISO 5455 series); 1.0 = 1:1.
  double scale = 1.0;
  /// View origin on the sheet in sheet-mm (bottom-left corner of the
  /// projected content bounding box anchor).
  std::array<double, 2> sheet_position = {0.0, 0.0};
  bool show_hidden = false;
  std::optional<SectionDefinition> section;
  /// Reference whose resolution failed (a body id, an edge witness
  /// body id, ...).  Present only while the view is degraded.
  std::optional<std::string> broken_ref;
  /// Human-readable dependency warning shown on the sheet while the
  /// view holds its last-known state.
  std::string warning;
};

// ── Title block (ISO 7200) ────────────────────────────────────────

/// The eight mandatory ISO 7200 fields.  Layout is P7; the DATA
/// round-trips from day one so documents saved early stay valid.
struct TitleBlock {
  std::string legal_owner;      // legal owner (company)
  std::string identification;   // identification number (drawing number)
  std::string date;             // date of issue
  std::string title;            // title
  std::string approver;         // approval person
  std::string creator;          // creator
  std::string document_type;    // document type
  /// Revision table rows (convention, not normative): each row is
  /// zone / revision / description / date / approved.
  std::vector<std::array<std::string, 5>> revision_rows;
};

// ── Sheet ─────────────────────────────────────────────────────────

/// ISO 5457 paper size: "A0" | "A1" | "A2" | "A3" | "A4".
using PaperSize = std::string;

/// "portrait" | "landscape" (A4 defaults to portrait, A0–A3 to
/// landscape).
using SheetOrientation = std::string;

/// A single drawing sheet.  Furniture (frame, centring marks, grid
/// refs, title block layout) is DERIVED from paper_size/orientation
/// at flatten time (P5) — stored here is the data that defines it.
struct DrawingSheet {
  std::string sheet_id;
  std::string name;
  PaperSize paper_size = "A4";
  SheetOrientation orientation = "portrait";
  /// Per-sheet projection-angle override (ISO 5456).  The drawing
  /// default is first-angle; the projection symbol is always shown.
  std::string projection_angle = "first_angle";
  /// Ordered ids into Drawing::views.
  std::vector<std::string> view_ids;
  TitleBlock title_block;
};

// ── Annotation ────────────────────────────────────────────────────

/// ISO 129-1 dimension kinds (the semantic vocabulary mirrors
/// XCAFDimTolObjects_DimensionType names so the STEP AP242/PMI door
/// stays open).
///   "linear" | "aligned" | "angular" | "radius" | "diameter" |
///   "ordinate" | "baseline" | "leader"
using AnnotationKind = std::string;

/// Extension payload for future ISO dimensions (tolerance frames
/// ISO 1101, datums ISO 5459, surface texture ISO 1302, TED,
/// auxiliary dimensions).  Stored as ordered key/value pairs so no
/// data model migration is needed when those land.
struct AnnotationExtension {
  std::string kind;  // "tolerance_frame" | "datum" | "surface_texture" |
                     // "ted" | "auxiliary"
  std::vector<std::array<std::string, 2>> fields;
};

/// A dimension or annotation attached to model topology — placement
/// on the sheet is derived at view regeneration, never stored.
struct Annotation {
  std::string annotation_id;
  AnnotationKind kind = "linear";
  std::string view_id;
  /// Persistent edge reference id minted by the core at pick time
  /// ("drawing-edge-N", the cam_capture_edge_reference precedent).
  std::string source_edge_id;
  SourceEdgeWitness witness;
  /// Second attachment (distance/angular between two edges).
  std::optional<SourceEdgeWitness> witness_2;
  /// User text override; empty = measured value.
  std::optional<std::string> text_override;
  /// Dimension prefix: "" | "⌀" | "R".  ⌀ is omittable only when the
  /// dimension is unambiguous; R is mandatory (ISO 129-1).
  std::string prefix;
  std::vector<AnnotationExtension> extensions;
  bool dependency_broken = false;
  /// Human-readable degradation message while the last-known value
  /// is shown.
  std::string warning;
  /// Cosmetic placement override (offset of the dimension text from
  /// its default position, sheet-mm).  Cosmetic edits never
  /// re-project.
  std::optional<std::array<double, 2>> text_offset;
  bool arrow_flip = false;
};

// ── Drawing ───────────────────────────────────────────────────────

/// A drawing = first-class document node: sheets, views, and
/// annotations, stored flat (sheets reference views by id — the CAM
/// setup/operation ownership precedent).
struct Drawing {
  std::string drawing_id;
  std::string name;
  std::vector<DrawingSheet> sheets;
  std::vector<DrawingView> views;
  std::vector<Annotation> annotations;
};

// ── Document-level drawing data ───────────────────────────────────

/// Everything the drawing workspace persists — the `drawing` member
/// of DocumentState.
struct DrawingDocumentData {
  std::vector<Drawing> drawings;
  std::optional<std::string> active_drawing_id;
  std::optional<std::string> selected_view_id;
  std::optional<std::string> selected_annotation_id;
  /// ISO 129-1: decimal comma.  One formatting knob, honored by
  /// every dimension renderer and export backend.
  std::string decimal_separator = ",";
};

// ══════════════════════════════════════════════════════════════════
//  Projection result — RUNTIME-ONLY (never serialized)
// ══════════════════════════════════════════════════════════════════

/// One projected edge with full provenance: the source edge witness
/// for model edges, a FaceAttestation for silhouettes (which have no
/// source edge).  Provenance is born with the record during HLR
/// extraction — no output↔entry matching heuristics.
struct ProjectedEdgeRecord {
  /// "visible" | "hidden"
  std::string line_class = "visible";
  /// "sharp" | "smooth" | "seam" | "outline"
  std::string curve_class = "sharp";
  /// "line" | "circle" | "ellipse" | "bspline" | "other"
  std::string curve_kind = "line";
  std::array<double, 2> p_start = {0.0, 0.0};
  std::array<double, 2> p_end = {0.0, 0.0};
  /// 2D parameters of the projected curve (HLRBRep::MakeEdge output
  /// parameterization — for circle/ellipse pieces these ARE the
  /// angular parameters, start_angle/end_angle below).
  double first_param = 0.0;
  double last_param = 0.0;
  // Renderable curve geometry in view space (populated for
  // circle/ellipse pieces; the flattened sheet stream needs exact
  // arcs, not chord faceting).
  std::optional<std::array<double, 2>> circle_center;
  std::optional<double> circle_radius;
  std::optional<std::array<double, 2>> ellipse_center;
  /// Unit major-axis direction of the ellipse in view space.
  std::optional<std::array<double, 2>> ellipse_major_dir;
  std::optional<double> ellipse_major_radius;
  std::optional<double> ellipse_minor_radius;
  /// Angular parameters (radians) for circle/ellipse pieces.
  double start_angle = 0.0;
  double end_angle = 0.0;
  /// Edge provenance: source edge witness, or silhouette face.
  std::variant<SourceEdgeWitness, FaceAttestation> source;
  /// Visible parameter intervals (partial visibility splits).
  std::vector<std::array<double, 2>> visible_intervals;
  /// Cutting-plane traces only (curve_class == "cutting_plane"): the
  /// sibling section's label, and the section's sight direction
  /// projected into this view plane (unit 2D vector).  Populated by
  /// the trace pass; the flatten emits the A–A labels + arrows from
  /// them (P7).
  std::string section_label;
  std::optional<std::array<double, 2>> trace_sight_dir;
};

/// One hatch region boundary in sheet space (section views).
struct HatchRegion {
  std::vector<std::array<double, 2>> outer_loop;
  std::vector<std::vector<std::array<double, 2>>> holes;
};

/// The complete immutable output of `project(sourceShape, viewFrame,
/// kind, options)` — cached in drawing_runtime keyed by (document,
/// view) and revision-validated.
struct ProjectionResult {
  std::vector<ProjectedEdgeRecord> edges;
  std::vector<HatchRegion> hatch_regions;
  /// Document revision the source shapes were compiled at.
  int source_revision = -1;
  /// true when sources are missing/broken and the result is the
  /// last-known state (shown with a warning, never silently
  /// substituted).
  bool stale = false;
};

}  // namespace polysmith::core
