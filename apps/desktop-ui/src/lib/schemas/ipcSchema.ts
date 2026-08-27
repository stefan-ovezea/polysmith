import { z } from "zod";

import { documentStateSchema } from "./ipc/documentStateSchema";
import { viewportStateSchema } from "./ipc/viewportStateSchema";

const sessionStateSchema = z.object({
  document_count: z.number(),
  has_active_document: z.boolean(),
  active_document_id: z.string().nullable(),
  can_undo: z.boolean(),
  can_redo: z.boolean(),
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

const documentExportedEventSchema = z.object({
  id: z.string(),
  type: z.literal("document_exported"),
  payload: z.object({
    file_path: z.string(),
    format: z.enum(["step", "stl", "dxf", "iges"]),
    exported_feature_count: z.number(),
  }),
});

const documentSavedEventSchema = z.object({
  id: z.string(),
  type: z.literal("document_saved"),
  payload: z.object({
    file_path: z.string(),
  }),
});

const logEventSchema = z.object({
  type: z.literal("log"),
  payload: z.object({
    level: z.enum(["debug", "info", "warn", "error"]),
    source: z.string(),
    message: z.string(),
    timestamp: z.string(),
  }),
});

const errorEventSchema = z.object({
  id: z.string().optional(),
  type: z.literal("error"),
  payload: z.object({
    code: z.string(),
    message: z.string(),
  }),
});

const trimPreviewResultEventSchema = z.object({
  id: z.string(),
  type: z.literal("trim_preview_result"),
  payload: z.union([
    z.object({
      entity_id: z.string(),
      entity_kind: z.enum(["line", "circle", "arc"]),
      hovered_index: z.number(),
      // Document revision the preview was computed against; the UI
      // echoes it back so a stale preview can never drive a trim.
      revision: z.number(),
      full_circle: z.boolean().optional(),
      full_arc: z.boolean().optional(),
      segments: z.array(z.object({
        start: z.tuple([z.number(), z.number()]).optional(),
        end: z.tuple([z.number(), z.number()]).optional(),
        param_start: z.number().optional(),
        param_end: z.number().optional(),
      })).optional(),
    }),
    z.null(),
  ]),
});

export const coreMessageSchema = z.union([
  helloEventSchema,
  pongEventSchema,
  documentCreatedEventSchema,
  documentStateEventSchema,
  sessionStateEventSchema,
  viewportStateEventSchema,
  documentExportedEventSchema,
  documentSavedEventSchema,
  logEventSchema,
  trimPreviewResultEventSchema,
  errorEventSchema,
]);

