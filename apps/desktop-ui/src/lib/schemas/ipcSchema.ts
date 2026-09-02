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
    format: z.enum(["step", "stl", "dxf", "iges", "gcode"]),
    exported_feature_count: z.number(),
  }),
});

const camGenerationProgressEventSchema = z.object({
  id: z.string(),
  type: z.literal("cam_generation_progress"),
  payload: z.object({
    op_id: z.string(),
    percent: z.number(),
  }),
});

const camPostListResultEventSchema = z.object({
  id: z.string(),
  type: z.literal("cam_post_list_result"),
  payload: z.object({
    posts: z.array(
      z.object({
        name: z.string(),
        path: z.string(),
      }),
    ),
  }),
});

// Reply to cam_capture_face_reference — see CamFaceAttestationResultEvent
// in types/ipc.ts. Payload mirrors make_cam_face_attestation_event
// (native/cad-core/src/app/impl/cam_commands.inc).
const camFaceAttestationResultEventSchema = z.object({
  id: z.string(),
  type: z.literal("cam_face_attestation_result"),
  payload: z.object({
    persistent_id: z.string(),
    attestation: z.object({
      area: z.number(),
      bounds: z.object({
        min_x: z.number(),
        min_y: z.number(),
        min_z: z.number(),
        max_x: z.number(),
        max_y: z.number(),
        max_z: z.number(),
      }),
      normal: z.tuple([z.number(), z.number(), z.number()]),
      sample_points: z.array(z.tuple([z.number(), z.number(), z.number()])),
    }),
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
      entity_kind: z.enum(["line", "circle", "arc", "ellipse", "spline"]),
      hovered_index: z.number(),
      // Document revision the preview was computed against; the UI
      // echoes it back so a stale preview can never drive a trim.
      revision: z.number(),
      full_circle: z.boolean().optional(),
      full_arc: z.boolean().optional(),
      full_ellipse: z.boolean().optional(),
      full_spline: z.boolean().optional(),
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
  camGenerationProgressEventSchema,
  camPostListResultEventSchema,
  camFaceAttestationResultEventSchema,
  errorEventSchema,
]);

