import { z } from "zod";
import type {
  CoreCommand,
  CoreMessage,
  DocumentState,
  ErrorEvent,
  ViewportState,
} from "../types/ipc";

const documentStateSchema = z.object({
  document_id: z.string(),
  name: z.string(),
  units: z.string(),
  revision: z.number(),
  selected_feature_id: z.string().nullable(),
  feature_history: z.array(
    z.object({
      feature_id: z.string(),
      kind: z.string(),
      name: z.string(),
      status: z.string(),
      parameters_summary: z.string(),
      box_parameters: z
        .object({
          width: z.number(),
          height: z.number(),
          depth: z.number(),
        })
        .nullable(),
    }),
  ),
});

const sessionStateSchema = z.object({
  document_count: z.number(),
  has_active_document: z.boolean(),
  active_document_id: z.string().nullable(),
  can_undo: z.boolean(),
  can_redo: z.boolean(),
});

const viewportStateSchema = z.object({
  has_active_document: z.boolean(),
  boxes: z.array(
    z.object({
      primitive_id: z.string(),
      label: z.string(),
      width: z.number(),
      height: z.number(),
      depth: z.number(),
      x_offset: z.number(),
      is_selected: z.boolean(),
    }),
  ),
  scene_width: z.number(),
  scene_height: z.number(),
  scene_depth: z.number(),
});

const helloEventSchema = z.object({
  type: z.literal("hello"),
  payload: z.object({
    service: z.string(),
    version: z.string(),
  }),
});

const pongEventSchema = z.object({
  id: z.string(),
  type: z.literal("pong"),
  payload: z.object({
    version: z.string(),
  }),
});

const documentCreatedEventSchema = z.object({
  id: z.string(),
  type: z.literal("document_created"),
  payload: documentStateSchema,
});

const documentStateEventSchema = z.object({
  id: z.string(),
  type: z.literal("document_state"),
  payload: documentStateSchema,
});

const sessionStateEventSchema = z.object({
  id: z.string(),
  type: z.literal("session_state"),
  payload: sessionStateSchema,
});

const viewportStateEventSchema = z.object({
  id: z.string(),
  type: z.literal("viewport_state"),
  payload: viewportStateSchema,
});

const errorEventSchema = z.object({
  id: z.string().optional(),
  type: z.literal("error"),
  payload: z.object({
    code: z.string(),
    message: z.string(),
  }),
});

const coreMessageSchema = z.union([
  helloEventSchema,
  pongEventSchema,
  documentCreatedEventSchema,
  documentStateEventSchema,
  sessionStateEventSchema,
  viewportStateEventSchema,
  errorEventSchema,
]);

export function parseCoreMessage(input: unknown): CoreMessage {
  return coreMessageSchema.parse(input) as CoreMessage;
}

export function makePingCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "ping",
    payload: {},
  };
}

export function makeCreateDocumentCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "create_document",
    payload: {},
  };
}

export function makeGetDocumentStateCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "get_document_state",
    payload: {},
  };
}

export function makeGetSessionStateCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "get_session_state",
    payload: {},
  };
}

export function makeGetViewportStateCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "get_viewport_state",
    payload: {},
  };
}

export function makeAddBoxFeatureCommand(
  width: number,
  height: number,
  depth: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_box_feature",
    payload: {
      width,
      height,
      depth,
    },
  };
}

export function makeUpdateBoxFeatureCommand(
  featureId: string,
  width: number,
  height: number,
  depth: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_box_feature",
    payload: {
      feature_id: featureId,
      width,
      height,
      depth,
    },
  };
}

export function makeRenameFeatureCommand(
  featureId: string,
  name: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "rename_feature",
    payload: {
      feature_id: featureId,
      name,
    },
  };
}

export function makeDeleteFeatureCommand(featureId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "delete_feature",
    payload: {
      feature_id: featureId,
    },
  };
}

export function makeUndoCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "undo",
    payload: {},
  };
}

export function makeRedoCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "redo",
    payload: {},
  };
}

export function makeSelectFeatureCommand(featureId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "select_feature",
    payload: {
      feature_id: featureId,
    },
  };
}

export function makeClearSelectionCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "clear_selection",
    payload: {},
  };
}

export function getDocumentFromMessage(message: CoreMessage): DocumentState | null {
  if (message.type === "document_created" || message.type === "document_state") {
    return message.payload;
  }

  return null;
}

export function getErrorFromMessage(message: CoreMessage): ErrorEvent | null {
  if (message.type === "error") {
    return message;
  }

  return null;
}

export function getViewportFromMessage(message: CoreMessage): ViewportState | null {
  if (message.type === "viewport_state") {
    return message.payload;
  }

  return null;
}
