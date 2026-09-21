// Drawing command payload contracts — mirroring the core's drawing_*
// commands (native/cad-core/src/app/impl/drawing_commands.inc).
// Every mutator replies with a `document_state` event; errors reply
// with an `error` event `{code, message}`.

import type {
  AnnotationExtension,
  Drawing,
  DrawingSheet,
  DrawingView,
  SectionDefinition,
  TitleBlock,
} from "../geometry/drawing";

export interface DrawingCreateCommand {
  id: string;
  type: "drawing_create";
  payload: Drawing;
}

export interface DrawingDeleteCommand {
  id: string;
  type: "drawing_delete";
  payload: { drawing_id: string };
}

export interface DrawingSetActiveCommand {
  id: string;
  type: "drawing_set_active";
  payload: { drawing_id: string };
}

export interface DrawingSheetCreateCommand {
  id: string;
  type: "drawing_sheet_create";
  payload: { drawing_id: string; sheet: DrawingSheet };
}

export interface DrawingSheetDeleteCommand {
  id: string;
  type: "drawing_sheet_delete";
  payload: { drawing_id: string; sheet_id: string };
}

export interface DrawingSheetUpdateCommand {
  id: string;
  type: "drawing_sheet_update";
  payload: {
    drawing_id: string;
    sheet_id: string;
    paper_size: "A0" | "A1" | "A2" | "A3" | "A4";
    orientation: "portrait" | "landscape";
    projection_angle: "first_angle" | "third_angle";
    name: string;
  };
}

export interface DrawingViewCreateCommand {
  id: string;
  type: "drawing_view_create";
  payload: { drawing_id: string; sheet_id: string; view: DrawingView };
}

export interface DrawingViewUpdateCommand {
  id: string;
  type: "drawing_view_update";
  payload: { drawing_id: string; view: DrawingView };
}

export interface DrawingViewDeleteCommand {
  id: string;
  type: "drawing_view_delete";
  payload: { drawing_id: string; view_id: string };
}

export interface DrawingViewMoveCommand {
  id: string;
  type: "drawing_view_move";
  payload: {
    drawing_id: string;
    view_id: string;
    sheet_position: [number, number];
  };
}

export interface DrawingViewPreviewCommand {
  id: string;
  type: "drawing_view_preview";
  payload: { drawing_id: string; sheet_id: string; view: DrawingView };
}

export interface DrawingSectionUpdateCommand {
  id: string;
  type: "drawing_section_update";
  payload: {
    drawing_id: string;
    view_id: string;
    section: SectionDefinition;
  };
}

export interface DrawingTitleBlockUpdateCommand {
  id: string;
  type: "drawing_title_block_update";
  payload: {
    drawing_id: string;
    sheet_id: string;
    title_block: TitleBlock;
  };
}

// ── Export (P8/P9) ─────────────────────────────────────────────────

export type DrawingExportFormat = "svg" | "dxf" | "pdf";

/// DXF entity fidelity: "geometry" (default, exploded entities) or
/// "annotated" (real DIMENSION + HATCH entities).
export type DrawingDxfMode = "geometry" | "annotated";

export interface DrawingExportCommand {
  id: string;
  type: "drawing_export";
  payload: {
    drawing_id: string;
    sheet_id: string;
    format: DrawingExportFormat;
    file_path: string;
    dxf_mode?: DrawingDxfMode;
  };
}

// ── Dimensions (P6) ───────────────────────────────────────────────

export type DrawingDimensionKind =
  | "linear"
  | "angular"
  | "radius"
  | "diameter";

export interface DrawingDimensionCreateCommand {
  id: string;
  type: "drawing_dimension_create";
  payload: {
    drawing_id: string;
    view_id: string;
    dim_type: DrawingDimensionKind;
    /** Pick in SHEET-mm — the core resolves it against the view's
     *  current projection and mints the witness. */
    pick: [number, number];
    /** Second pick for distance/angular dimensions. */
    pick_2?: [number, number];
    /** Set to repair an existing annotation (witness + kind
     *  re-captured, cosmetics kept). */
    annotation_id?: string;
  };
}

export interface DrawingDimensionUpdateCommand {
  id: string;
  type: "drawing_dimension_update";
  payload: {
    drawing_id: string;
    annotation_id: string;
    text_override?: string;
    prefix?: string;
    arrow_flip?: boolean;
    text_offset?: [number, number];
  };
}

export interface DrawingDimensionDeleteCommand {
  id: string;
  type: "drawing_dimension_delete";
  payload: { drawing_id: string; annotation_id: string };
}

export interface DrawingDimensionPreviewCommand {
  id: string;
  type: "drawing_dimension_preview";
  payload: {
    drawing_id: string;
    view_id: string;
    dim_type: DrawingDimensionKind;
    pick: [number, number];
    pick_2?: [number, number];
  };
}

// ── Templates (CREATE DRAWING dialog) ──────────────────────────────

export interface DrawingTemplateSaveCommand {
  id: string;
  type: "drawing_template_save";
  payload: {
    file_path: string;
    template: { name: string; sheets: DrawingSheet[] };
  };
}

export interface DrawingTemplateLoadCommand {
  id: string;
  type: "drawing_template_load";
  payload: { file_path: string };
}

/** Reply payload of drawing_template_save. */
export interface DrawingTemplateSaveResultPayload {
  file_path: string;
}

/** Reply payload of drawing_template_load — the setup-only template
 *  (name + sheets with title blocks, ids empty). */
export interface DrawingTemplateLoadResultPayload {
  template: { name: string; sheets: DrawingSheet[] };
}

/** Reply payload of drawing_dimension_preview — the non-mutating
 *  core-computed preview (value + sheet-mm graphics). */
export interface DrawingDimensionPreviewPayload {
  drawing_id: string;
  view_id: string;
  dim_type: DrawingDimensionKind;
  pick: [number, number];
  pick_2?: [number, number];
  error?: string;
  value?: number;
  /** Display text (decimal separator + prefix applied). */
  text_value?: string;
  curves?: Array<{
    kind: string;
    line_class: string;
    curve_class: string;
    purpose: string;
    width_mm: number;
    p0: [number, number];
    p1: [number, number];
    center?: [number, number];
    radius?: number;
    start_angle: number;
    end_angle: number;
    points?: Array<[number, number]>;
  }>;
  text?: {
    text: string;
    position: [number, number];
    height_mm: number;
    angle_deg: number;
    h_align: string;
    purpose: string;
    stale: boolean;
  };
}

// ── Sheet notes (ANNOTATE → Text) ──────────────────────────────────

export interface DrawingNoteCreateCommand {
  id: string;
  type: "drawing_note_create";
  payload: {
    drawing_id: string;
    sheet_id: string;
    text: string;
    /** Anchor position in SHEET-mm (the text CENTER). */
    position: [number, number];
    height_mm?: number;
    angle_deg?: number;
    h_align?: "left" | "center" | "right";
  };
}

export interface DrawingNoteUpdateCommand {
  id: string;
  type: "drawing_note_update";
  payload: {
    drawing_id: string;
    note_id: string;
    text?: string;
    position?: [number, number];
    height_mm?: number;
    angle_deg?: number;
    h_align?: "left" | "center" | "right";
  };
}

export interface DrawingNoteDeleteCommand {
  id: string;
  type: "drawing_note_delete";
  payload: { drawing_id: string; note_id: string };
}

// ── Annotations (GEOMETRY / SYMBOLS / ANNOTATE tabs) ───────────────

export type DrawingAnnotationKind =
  | "leader_text"
  | "center_mark"
  | "centerline"
  | "edge_extension"
  | "surface_finish"
  | "welding"
  | "tolerance_frame"
  | "datum"
  | "balloon";

export interface DrawingAnnotationCreateCommand {
  id: string;
  type: "drawing_annotation_create";
  payload: {
    drawing_id: string;
    view_id: string;
    kind: DrawingAnnotationKind;
    /** Pick in SHEET-mm — the core resolves it against the view's
     *  current projection and mints the witness + attach_param. */
    pick: [number, number];
    /** Second pick (centerline). */
    pick_2?: [number, number];
    text_override?: string;
    prefix?: string;
    extensions?: AnnotationExtension[];
    /** The placement click (leader_text): the core computes
     *  text_offset = placement − default text anchor. */
    placement?: [number, number];
  };
}

export interface DrawingAnnotationUpdateCommand {
  id: string;
  type: "drawing_annotation_update";
  payload: {
    drawing_id: string;
    annotation_id: string;
    text_override?: string;
    prefix?: string;
    arrow_flip?: boolean;
    text_offset?: [number, number];
    extensions?: AnnotationExtension[];
    attach_param?: number;
  };
}

export interface DrawingAnnotationDeleteCommand {
  id: string;
  type: "drawing_annotation_delete";
  payload: { drawing_id: string; annotation_id: string };
}

export interface DrawingAnnotationPreviewCommand {
  id: string;
  type: "drawing_annotation_preview";
  payload: {
    drawing_id: string;
    view_id: string;
    kind: DrawingAnnotationKind;
    pick: [number, number];
    pick_2?: [number, number];
    text_override?: string;
    prefix?: string;
  };
}

/** Reply payload of drawing_annotation_preview — the non-mutating
 *  core-computed attachment graphics (the dimension preview contract
 *  with `kind` instead of `dim_type`). */
export interface DrawingAnnotationPreviewPayload {
  drawing_id: string;
  view_id: string;
  kind: DrawingAnnotationKind;
  pick: [number, number];
  pick_2?: [number, number];
  error?: string;
  /** Display text (prefix + text_override). */
  text_value?: string;
  curves?: Array<{
    kind: string;
    line_class: string;
    curve_class: string;
    purpose: string;
    width_mm: number;
    p0: [number, number];
    p1: [number, number];
    center?: [number, number];
    radius?: number;
    start_angle: number;
    end_angle: number;
    points?: Array<[number, number]>;
  }>;
  text?: {
    text: string;
    position: [number, number];
    height_mm: number;
    angle_deg: number;
    h_align: string;
    purpose: string;
    stale: boolean;
  };
}
