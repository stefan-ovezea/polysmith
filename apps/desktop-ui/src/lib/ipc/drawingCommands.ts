import type {
  CoreCommand,
  Drawing,
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
