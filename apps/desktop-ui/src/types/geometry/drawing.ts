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

// ── View ─────────────────────────────────────────────────────────

export type DrawingViewKind = "projection" | "section" | "axonometric";

export interface DrawingView {
  view_id: string;
  kind: DrawingViewKind;
  /**
   * "front" | "top" | "right" | "bottom" | "left" | "back" — UI
   * convenience only; the refresh derives the frame.  Empty for
   * custom frames / sections / axonometric views.
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
  | "leader";

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
  /** Second attachment (distance/angular between two edges). */
  witness_2?: SourceEdgeWitness;
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

// ── Drawing ───────────────────────────────────────────────────────

export interface Drawing {
  drawing_id: string;
  name: string;
  sheets: DrawingSheet[];
  views: DrawingView[];
  annotations: Annotation[];
}

// ── Document-level drawing data ───────────────────────────────────

export interface DrawingDocumentData {
  drawings: Drawing[];
  active_drawing_id: string | null;
  selected_view_id: string | null;
  selected_annotation_id: string | null;
  /** ISO 129-1: decimal comma. */
  decimal_separator: string;
}
