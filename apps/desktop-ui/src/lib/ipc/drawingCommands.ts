import type {
  AnnotationExtension,
  CoreCommand,
  Drawing,
  DrawingAnnotationKind,
  DrawingSheet,
  DrawingView,
  SectionDefinition,
  TitleBlock,
} from "@/types";

// Drawing command factories — every command replies with a
// `document_state` event (errors reply with an `error` event).
// Payloads are the serialized target-schema structs from
// types/geometry/drawing.ts.

export function makeDrawingCreateCommand(drawing: Drawing): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_create",
    payload: drawing,
  };
}

export function makeDrawingDeleteCommand(drawingId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_delete",
    payload: { drawing_id: drawingId },
  };
}

export function makeDrawingSetActiveCommand(drawingId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_set_active",
    payload: { drawing_id: drawingId },
  };
}

export function makeDrawingSheetCreateCommand(
  drawingId: string,
  sheet: DrawingSheet,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_sheet_create",
    payload: { drawing_id: drawingId, sheet },
  };
}

export function makeDrawingSheetDeleteCommand(
  drawingId: string,
  sheetId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_sheet_delete",
    payload: { drawing_id: drawingId, sheet_id: sheetId },
  };
}

export function makeDrawingSheetUpdateCommand(
  drawingId: string,
  sheetId: string,
  settings: {
    paper_size: "A0" | "A1" | "A2" | "A3" | "A4";
    orientation: "portrait" | "landscape";
    projection_angle: "first_angle" | "third_angle";
    name: string;
  },
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_sheet_update",
    payload: { drawing_id: drawingId, sheet_id: sheetId, ...settings },
  };
}

export function makeDrawingViewCreateCommand(
  drawingId: string,
  sheetId: string,
  view: DrawingView,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_view_create",
    payload: { drawing_id: drawingId, sheet_id: sheetId, view },
  };
}

export function makeDrawingViewUpdateCommand(
  drawingId: string,
  view: DrawingView,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_view_update",
    payload: { drawing_id: drawingId, view },
  };
}

export function makeDrawingViewDeleteCommand(
  drawingId: string,
  viewId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_view_delete",
    payload: { drawing_id: drawingId, view_id: viewId },
  };
}

/** Non-mutating live preview of an uncommitted view definition —
 *  the core replies with drawing_view_preview_result carrying the
 *  ghost geometry (the same vocabulary as a committed sheet view). */
export function makeDrawingViewPreviewCommand(
  drawingId: string,
  sheetId: string,
  view: DrawingView,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_view_preview",
    payload: { drawing_id: drawingId, sheet_id: sheetId, view },
  };
}

export function makeDrawingViewMoveCommand(
  drawingId: string,
  viewId: string,
  sheetPosition: [number, number],
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_view_move",
    payload: {
      drawing_id: drawingId,
      view_id: viewId,
      sheet_position: sheetPosition,
    },
  };
}

export function makeDrawingSectionUpdateCommand(
  drawingId: string,
  viewId: string,
  section: SectionDefinition,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_section_update",
    payload: { drawing_id: drawingId, view_id: viewId, section },
  };
}

export function makeDrawingTitleBlockUpdateCommand(
  drawingId: string,
  sheetId: string,
  titleBlock: TitleBlock,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_title_block_update",
    payload: { drawing_id: drawingId, sheet_id: sheetId, title_block: titleBlock },
  };
}

export function makeDrawingExportCommand(params: {
  drawingId: string;
  sheetId: string;
  format: "svg" | "dxf" | "pdf";
  filePath: string;
  dxfMode?: "geometry" | "annotated";
}): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_export",
    payload: {
      drawing_id: params.drawingId,
      sheet_id: params.sheetId,
      format: params.format,
      file_path: params.filePath,
      ...(params.dxfMode ? { dxf_mode: params.dxfMode } : {}),
    },
  };
}

// ── Dimensions (P6) ───────────────────────────────────────────────

export function makeDrawingDimensionCreateCommand(params: {
  drawingId: string;
  viewId: string;
  dimType: "linear" | "angular" | "radius" | "diameter";
  pick: [number, number];
  pick2?: [number, number];
  annotationId?: string;
}): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_dimension_create",
    payload: {
      drawing_id: params.drawingId,
      view_id: params.viewId,
      dim_type: params.dimType,
      pick: params.pick,
      ...(params.pick2 ? { pick_2: params.pick2 } : {}),
      ...(params.annotationId ? { annotation_id: params.annotationId } : {}),
    },
  };
}

export function makeDrawingDimensionUpdateCommand(params: {
  drawingId: string;
  annotationId: string;
  textOverride?: string;
  prefix?: string;
  arrowFlip?: boolean;
  textOffset?: [number, number];
}): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_dimension_update",
    payload: {
      drawing_id: params.drawingId,
      annotation_id: params.annotationId,
      ...(params.textOverride !== undefined
        ? { text_override: params.textOverride }
        : {}),
      ...(params.prefix !== undefined ? { prefix: params.prefix } : {}),
      ...(params.arrowFlip !== undefined
        ? { arrow_flip: params.arrowFlip }
        : {}),
      ...(params.textOffset !== undefined
        ? { text_offset: params.textOffset }
        : {}),
    },
  };
}

export function makeDrawingDimensionDeleteCommand(
  drawingId: string,
  annotationId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_dimension_delete",
    payload: { drawing_id: drawingId, annotation_id: annotationId },
  };
}

// ── Templates (CREATE DRAWING dialog) ──────────────────────────────

export function makeDrawingTemplateSaveCommand(
  filePath: string,
  template: { name: string; sheets: DrawingSheet[] },
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_template_save",
    payload: { file_path: filePath, template },
  };
}

export function makeDrawingTemplateLoadCommand(
  filePath: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_template_load",
    payload: { file_path: filePath },
  };
}

export function makeDrawingDimensionPreviewCommand(params: {
  drawingId: string;
  viewId: string;
  dimType: "linear" | "angular" | "radius" | "diameter";
  pick: [number, number];
  pick2?: [number, number];
}): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_dimension_preview",
    payload: {
      drawing_id: params.drawingId,
      view_id: params.viewId,
      dim_type: params.dimType,
      pick: params.pick,
      ...(params.pick2 ? { pick_2: params.pick2 } : {}),
    },
  };
}

// ── Sheet notes (ANNOTATE → Text) ──────────────────────────────────

export function makeDrawingNoteCreateCommand(params: {
  drawingId: string;
  sheetId: string;
  text: string;
  position: [number, number];
  heightMm?: number;
  angleDeg?: number;
  hAlign?: "left" | "center" | "right";
}): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_note_create",
    payload: {
      drawing_id: params.drawingId,
      sheet_id: params.sheetId,
      text: params.text,
      position: params.position,
      ...(params.heightMm !== undefined
        ? { height_mm: params.heightMm }
        : {}),
      ...(params.angleDeg !== undefined ? { angle_deg: params.angleDeg } : {}),
      ...(params.hAlign !== undefined ? { h_align: params.hAlign } : {}),
    },
  };
}

export function makeDrawingNoteUpdateCommand(params: {
  drawingId: string;
  noteId: string;
  text?: string;
  position?: [number, number];
  heightMm?: number;
  angleDeg?: number;
  hAlign?: "left" | "center" | "right";
}): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_note_update",
    payload: {
      drawing_id: params.drawingId,
      note_id: params.noteId,
      ...(params.text !== undefined ? { text: params.text } : {}),
      ...(params.position !== undefined ? { position: params.position } : {}),
      ...(params.heightMm !== undefined
        ? { height_mm: params.heightMm }
        : {}),
      ...(params.angleDeg !== undefined ? { angle_deg: params.angleDeg } : {}),
      ...(params.hAlign !== undefined ? { h_align: params.hAlign } : {}),
    },
  };
}

export function makeDrawingNoteDeleteCommand(
  drawingId: string,
  noteId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_note_delete",
    payload: { drawing_id: drawingId, note_id: noteId },
  };
}

// ── Annotations (GEOMETRY / SYMBOLS / ANNOTATE tabs) ───────────────

export function makeDrawingAnnotationCreateCommand(params: {
  drawingId: string;
  viewId: string;
  kind: DrawingAnnotationKind;
  pick: [number, number];
  pick2?: [number, number];
  textOverride?: string;
  prefix?: string;
  extensions?: AnnotationExtension[];
  placement?: [number, number];
}): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_annotation_create",
    payload: {
      drawing_id: params.drawingId,
      view_id: params.viewId,
      kind: params.kind,
      pick: params.pick,
      ...(params.pick2 ? { pick_2: params.pick2 } : {}),
      ...(params.textOverride !== undefined
        ? { text_override: params.textOverride }
        : {}),
      ...(params.prefix !== undefined ? { prefix: params.prefix } : {}),
      ...(params.extensions !== undefined
        ? { extensions: params.extensions }
        : {}),
      ...(params.placement !== undefined
        ? { placement: params.placement }
        : {}),
    },
  };
}

export function makeDrawingAnnotationUpdateCommand(params: {
  drawingId: string;
  annotationId: string;
  textOverride?: string;
  prefix?: string;
  arrowFlip?: boolean;
  textOffset?: [number, number];
  extensions?: AnnotationExtension[];
  attachParam?: number;
}): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_annotation_update",
    payload: {
      drawing_id: params.drawingId,
      annotation_id: params.annotationId,
      ...(params.textOverride !== undefined
        ? { text_override: params.textOverride }
        : {}),
      ...(params.prefix !== undefined ? { prefix: params.prefix } : {}),
      ...(params.arrowFlip !== undefined
        ? { arrow_flip: params.arrowFlip }
        : {}),
      ...(params.textOffset !== undefined
        ? { text_offset: params.textOffset }
        : {}),
      ...(params.extensions !== undefined
        ? { extensions: params.extensions }
        : {}),
      ...(params.attachParam !== undefined
        ? { attach_param: params.attachParam }
        : {}),
    },
  };
}

export function makeDrawingAnnotationDeleteCommand(
  drawingId: string,
  annotationId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_annotation_delete",
    payload: { drawing_id: drawingId, annotation_id: annotationId },
  };
}

export function makeDrawingAnnotationPreviewCommand(params: {
  drawingId: string;
  viewId: string;
  kind: DrawingAnnotationKind;
  pick: [number, number];
  pick2?: [number, number];
  textOverride?: string;
  prefix?: string;
}): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "drawing_annotation_preview",
    payload: {
      drawing_id: params.drawingId,
      view_id: params.viewId,
      kind: params.kind,
      pick: params.pick,
      ...(params.pick2 ? { pick_2: params.pick2 } : {}),
      ...(params.textOverride !== undefined
        ? { text_override: params.textOverride }
        : {}),
      ...(params.prefix !== undefined ? { prefix: params.prefix } : {}),
    },
  };
}
