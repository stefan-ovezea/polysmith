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

const camExportGcodeTextResultEventSchema = z.object({
  id: z.string(),
  type: z.literal("cam_export_gcode_text_result"),
  payload: z.object({
    text: z.string(),
    format: z.string(),
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

// Generate-only completion event (never preview) — see
// CamGenerationResultEvent in types/ipc.ts. Lenient on the message and
// list fields: the core always sends them, defaults keep a bad payload
// from dropping the whole event.
const camGenerationResultEventSchema = z.object({
  id: z.string(),
  type: z.literal("cam_generation_result"),
  payload: z.object({
    op_id: z.string(),
    ok: z.boolean(),
    error_message: z.string().default(""),
    warnings: z.array(z.string()).default([]),
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

const camMachineListResultEventSchema = z.object({
  id: z.string(),
  type: z.literal("cam_machine_list_result"),
  payload: z.object({
    machines: z.array(
      z.object({
        name: z.string(),
        machine_type: z.string(),
        post_processor: z.object({
          type: z.string(),
          filename: z.string(),
        }),
        work_area_x_mm: z.number(),
        work_area_y_mm: z.number(),
        pointer_offset_x_mm: z.number(),
        pointer_offset_y_mm: z.number(),
        // GRBL workspace prefs (sent by the core since cam_types.h
        // grew them — kept in sync with to_payload(MachineDefinition)).
        jog_step_mm: z.number(),
        jog_feed_mm_per_min: z.number(),
        homing_enabled: z.boolean(),
        pointer_power_percent: z.number(),
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

// Reply to cam_capture_edge_reference — see CamEdgeAttestationResultEvent
// in types/ipc.ts. Payload mirrors make_cam_edge_attestation_event
// (native/cad-core/src/app/impl/cam_commands.inc).  The circle witness
// keys are optional — they exist only for full-circle edges.
const camEdgeAttestationResultEventSchema = z.object({
  id: z.string(),
  type: z.literal("cam_edge_attestation_result"),
  payload: z.object({
    persistent_id: z.string(),
    attestation: z.object({
      start_point: z.tuple([z.number(), z.number(), z.number()]),
      end_point: z.tuple([z.number(), z.number(), z.number()]),
      length: z.number(),
      tangent: z.tuple([z.number(), z.number(), z.number()]),
      adjacent_face_normals: z
        .array(z.tuple([z.number(), z.number(), z.number()]))
        .optional(),
      center: z.tuple([z.number(), z.number(), z.number()]).optional(),
      axis: z.tuple([z.number(), z.number(), z.number()]).optional(),
      radius: z.number().optional(),
    }),
  }),
});

// Reply to cam_capture_point — see CamAttestationResultEvent in
// types/ipc.ts. Payload mirrors make_cam_attestation_event
// (native/cad-core/src/app/impl/cam_commands.inc).
const camAttestationResultEventSchema = z.object({
  id: z.string(),
  type: z.literal("cam_attestation_result"),
  payload: z.object({
    persistent_id: z.string(),
    attestation: z.object({
      point: z.tuple([z.number(), z.number(), z.number()]),
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
  camExportGcodeTextResultEventSchema,
  documentSavedEventSchema,
  logEventSchema,
  trimPreviewResultEventSchema,
  camGenerationProgressEventSchema,
  camGenerationResultEventSchema,
  camPostListResultEventSchema,
  camMachineListResultEventSchema,
  camFaceAttestationResultEventSchema,
  camEdgeAttestationResultEventSchema,
  camAttestationResultEventSchema,
  errorEventSchema,
]);

