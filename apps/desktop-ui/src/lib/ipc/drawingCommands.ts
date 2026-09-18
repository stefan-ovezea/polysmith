import type { CoreCommand, Drawing, DrawingSheet } from "@/types";

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
