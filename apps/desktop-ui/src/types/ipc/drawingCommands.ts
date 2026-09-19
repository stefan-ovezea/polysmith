// Drawing command payload contracts — mirroring the core's drawing_*
// commands (native/cad-core/src/app/impl/drawing_commands.inc).
// Every mutator replies with a `document_state` event; errors reply
// with an `error` event `{code, message}`.

import type {
  Drawing,
  DrawingSheet,
  DrawingView,
  SectionDefinition,
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
