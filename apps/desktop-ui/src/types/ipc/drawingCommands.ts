// Drawing command payload contracts — mirroring the core's drawing_*
// commands (native/cad-core/src/app/impl/drawing_commands.inc).
// Every mutator replies with a `document_state` event; errors reply
// with an `error` event `{code, message}`.

import type {
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
