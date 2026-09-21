// ISO drawing type definitions — mirroring
// native/cad-core/src/core/drawing/drawing_types.h
//
// These are the canonical target-schema drawing types.  The
// DocumentState interface in ipc.ts holds a DrawingDocumentData
// container; the detailed shapes below are used when working with
// individual drawing entities.

export interface DrawingViewFrame {
  origin: [number, number, number];
  normal: [number, number, number];
  x_direction: [number, number, number];
}

// ── TNP-safe source edge witness ─────────────────────────────────

export interface SourceEdgeWitness {
  body_id: string;
  /** Hint: index in the body compile edge list. */
  src_edge_index: number;
  start_point: [number, number, number];
  end_point: [number, number, number];
  length: number;
  tangent: [number, number, number];
  /** "line" | "circle" | "ellipse" | "bspline" | "other" */
  curve_kind: string;
  // Circle witness — present only for circular edges (a closed rim has
  // start === end, so these identify it).
  center?: [number, number, number];
  axis?: [number, number, number];
  radius?: number;
  /** Source-parameter range the projected edge covers. */
  param_range: [number, number];
}

// ── Section definition ───────────────────────────────────────────

export interface SectionDefinition {
  cutting_plane_point: [number, number, number];
  cutting_plane_normal: [number, number, number];
  /** true = cut away material on the normal side (halfspace cut). */
  cut_away: boolean;
  label: string;
  /** ISO 128-3 §7: 45° preferred, 0.7–3 mm spacing. */
  hatch_angle_deg: number;
  hatch_spacing_mm: number;
  construction_plane_id?: string;
}

// ── Detail definition ────────────────────────────────────────────

/** ISO 128-3 §4.12 detail (enlarged feature) view: a circular window
 *  on a parent projection view, re-projected at a larger scale.
 *  center/radius are in the PARENT's view-mm (its projection plane). */
export interface DetailDefinition {
  parent_view_id: string;
  center: [number, number];
  radius: number;
  /** Capital letter minted by the UI at create time ("A", "B", …). */
  label: string;
}

// ── View ─────────────────────────────────────────────────────────

export type DrawingViewKind =
  | "projection"
  | "section"
  | "axonometric"
  | "detail";

export interface DrawingView {
  view_id: string;
  kind: DrawingViewKind;
  /**
   * "front" | "top" | "right" | "bottom" | "left" | "back" — UI
   * convenience only; the refresh derives the frame.  Empty for
   * custom frames / sections / axonometric / detail views.
   */
  standard_view: string;
  /** Resolved custom frame — authoritative when standard_view is empty. */
  custom_frame?: DrawingViewFrame;
  source_body_ids: string[];
  /** View scale (ISO 5455 series); 1.0 = 1:1. */
  scale: number;
  /** View origin on the sheet in sheet-mm. */
  sheet_position: [number, number];
  show_hidden: boolean;
  section?: SectionDefinition;
  /** Present only on kind "detail" views (parent + window). */
  detail?: DetailDefinition;
  /** Reference whose resolution failed — present only while degraded. */
  broken_ref?: string;
  /** Human-readable dependency warning while holding last-known state. */
  warning: string;
}

// ── Title block (ISO 7200) ───────────────────────────────────────

export interface TitleBlock {
  legal_owner: string;
  identification: string;
  date: string;
  title: string;
  approver: string;
  creator: string;
  document_type: string;
  /** Each row: zone / revision / description / date / approved. */
  revision_rows: Array<[string, string, string, string, string]>;
}

// ── Sheet ─────────────────────────────────────────────────────────

export type PaperSize = "A0" | "A1" | "A2" | "A3" | "A4";
export type SheetOrientation = "portrait" | "landscape";

export interface DrawingSheet {
  sheet_id: string;
  name: string;
  paper_size: PaperSize;
  orientation: SheetOrientation;
  /** Per-sheet projection-angle override (ISO 5456). */
  projection_angle: "first_angle" | "third_angle";
  /** Ordered ids into Drawing.views. */
  view_ids: string[];
  title_block: TitleBlock;
}

// ── Annotation ───────────────────────────────────────────────────

export type AnnotationKind =
  | "linear"
  | "aligned"
  | "angular"
  | "radius"
  | "diameter"
  | "ordinate"
  | "baseline"
  | "leader"
  // GEOMETRY / SYMBOLS / ANNOTATE tabs (R4+R5): content is the user
  // text (text_override), never a measured value.
  | "center_mark"
  | "centerline"
  | "edge_extension"
  | "leader_text"
  | "surface_finish"
  | "welding"
  | "tolerance_frame"
  | "datum"
  | "balloon";

/** Extension payload for future ISO dimensions (tolerance frames,
 *  datums, surface texture, TED, auxiliary) — ordered key/value
 *  pairs so no data model migration is needed when those land. */
export interface AnnotationExtension {
  kind:
    | "tolerance_frame"
    | "datum"
    | "surface_texture"
    | "ted"
    | "auxiliary";
  fields: Array<[string, string]>;
}

export interface Annotation {
  annotation_id: string;
  kind: AnnotationKind;
  view_id: string;
  /** Persistent edge reference id minted by the core at pick time. */
  source_edge_id: string;
  witness: SourceEdgeWitness;
  /** Second attachment (distance/angular between two edges, or the
   *  second circle of a centerline). */
  witness_2?: SourceEdgeWitness;
  /** Point attachments (leader_text, surface_finish, welding,
   *  tolerance_frame, datum, balloon, edge-extension end choice):
   *  the attachment's param fraction (0..1) over the witness
   *  param_range — follows the edge across recomputes. */
  attach_param?: number;
  /** User text override; empty = measured value. */
  text_override?: string;
  /** "" | "⌀" | "R" — ⌀ omittable only when unambiguous. */
  prefix: string;
  extensions: AnnotationExtension[];
  dependency_broken: boolean;
  warning: string;
  /** Cosmetic placement override (sheet-mm); never re-projects. */
  text_offset?: [number, number];
  arrow_flip: boolean;
}

// ── Sheet note ─────────────────────────────────────────────────────

/** A free text note on a sheet (ANNOTATE → Text).  Not model-anchored:
 *  the position is direct sheet-mm state (anchor = text CENTER). */
export interface SheetNote {
  note_id: string;
  sheet_id: string;
  text: string;
  /** Anchor position (sheet-mm) — the text CENTER. */
  position: [number, number];
  /** ISO 3098 letter height in mm. */
  height_mm: number;
  angle_deg: number;
  /** "left" | "center" | "right" */
  h_align: string;
}

// ── Drawing ───────────────────────────────────────────────────────

export interface Drawing {
  drawing_id: string;
  name: string;
  sheets: DrawingSheet[];
  views: DrawingView[];
  annotations: Annotation[];
  /** Free sheet notes (ANNOTATE → Text). */
  notes: SheetNote[];
}

/** A setup-only drawing template (the CREATE DRAWING dialog's
 *  save/load format): sheets carry settings + title block, never ids,
 *  view ids, views or annotations — those are minted at create time. */
export interface DrawingTemplate {
  name: string;
  sheets: DrawingSheet[];
}

// ── Document-level drawing data ───────────────────────────────────

export interface DrawingDocumentData {
  drawings: Drawing[];
  active_drawing_id: string | null;
  selected_view_id: string | null;
  selected_annotation_id: string | null;
  /** Decimal separator for dimension values ("." or ","). */
  decimal_separator: string;
}

// ── Viewport emission (viewport_drawing_primitives.h) ─────────────

export interface ViewportDrawingCurve {
  /** "line" | "circle" | "ellipse" | "filled_poly" */
  kind: string;
  /** "visible" | "hidden" */
  line_class: string;
  /** The P5 flattened stream's purpose ("sharp" | "hatch" |
   *  "cutting_plane" | "frame" | "centring_mark" | "grid_ref" |
   *  "projection_symbol" | ...). */
  curve_class: string;
  /** "view_geometry" | "hatch" | "cutting_plane" | "frame" |
   *  "centring_mark" | "grid_ref" | "projection_symbol" |
   *  "dimension" — furniture purposes render in the border color. */
  purpose: string;
  /** ISO line-group width in mm — the core flatten has already
   *  applied dash patterns, so the renderer draws continuous ribbons
   *  of this width only. */
  width_mm: number;
  /** Endpoints in sheet-mm. */
  p0: [number, number];
  p1: [number, number];
  center?: [number, number];
  radius?: number;
  major_dir?: [number, number];
  major_radius?: number;
  minor_radius?: number;
  /** Angular parameters (radians) for circle/ellipse arcs. */
  start_angle: number;
  end_angle: number;
  /** Polygon vertices (sheet-mm) for "filled_poly" — closed filled
   *  paths (dimension arrowheads). */
  points?: Array<[number, number]>;
}

/** One text record on the sheet (P6: dimension values) — the
 *  renderer draws a canvas sprite; the DXF backend emits a real
 *  DRW_Text from the same record. */
export interface ViewportDrawingText {
  text: string;
  /** Anchor = text CENTER (sheet-mm). */
  position: [number, number];
  height_mm: number;
  angle_deg: number;
  /** "left" | "center" | "right" */
  h_align: string;
  purpose: string;
  /** Owning annotation — set only for dimension texts (the hit test
   *  maps a picked text back to its annotation). */
  annotation_id?: string;
  /** true when the dimension is degraded (last-known value shown). */
  stale: boolean;
}

export interface ViewportDrawingView {
  view_id: string;
  label: string;
  scale: number;
  origin: [number, number];
  min: [number, number];
  max: [number, number];
  stale: boolean;
  warning: string;
}

export interface ViewportDrawingSheet {
  sheet_id: string;
  drawing_id: string;
  name: string;
  width_mm: number;
  height_mm: number;
  curves: ViewportDrawingCurve[];
  views: ViewportDrawingView[];
  texts: ViewportDrawingText[];
}

/** drawing_view_preview_result — the core's ghost of an uncommitted
 *  view definition (same vocabulary as a committed sheet view). */
export interface DrawingViewPreviewPayload {
  drawing_id: string;
  sheet_id: string;
  curves: ViewportDrawingCurve[];
  texts: ViewportDrawingText[];
  view: ViewportDrawingView;
}
