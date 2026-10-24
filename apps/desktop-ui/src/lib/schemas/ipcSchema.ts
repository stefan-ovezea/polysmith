import { z } from "zod";

import { drawingViewPreviewResultSchema } from "./ipc/drawingSchema";
import { documentStateSchema } from "./ipc/documentStateSchema";
import {
  viewportDrawingCurveShape,
  viewportDrawingTextShape,
  viewportStateSchema,
} from "./ipc/viewportStateSchema";

const sessionStateSchema = z.object({
  document_count: z.number(),
  has_active_document: z.boolean(),
  active_document_id: z.string().nullable(),
  can_undo: z.boolean(),
  can_redo: z.boolean(),
});

// Phase F: the geometry payloads (document_state / viewport_state) are 155 KB –
// 1.6 MB of JSON. A full zod walk of those arrays on every event is a real parse
// cost on the hot path, and the core is the source of truth for their shape. So
// the message ENVELOPE (id + type) is always validated, but the payload body is
// walked only in dev, where a malformed payload should surface loudly. Production
// keeps the payload as an opaque object and lets the store read it directly.
const geometryPayload = <T extends z.ZodTypeAny>(schema: T): z.ZodTypeAny =>
  import.meta.env.DEV ? schema : z.record(z.string(), z.unknown());

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
  payload: geometryPayload(documentStateSchema),
});

const documentStateEventSchema = z.object({
  id: z.string(),
  type: z.literal("document_state"),
  payload: geometryPayload(documentStateSchema),
});

const sessionStateEventSchema = z.object({
  id: z.string(),
  type: z.literal("session_state"),
  payload: sessionStateSchema,
});

const viewportStateEventSchema = z.object({
  id: z.string(),
  type: z.literal("viewport_state"),
  payload: geometryPayload(viewportStateSchema),
});

const documentExportedEventSchema = z.object({
  id: z.string(),
  type: z.literal("document_exported"),
  payload: z.object({
    file_path: z.string(),
    // Model exports (step/stl/dxf/iges/gcode) plus the drawing-sheet
    // exports (svg/dxf/pdf — dxf_mode "annotated" still reports dxf).
    format: z.enum(["step", "stl", "dxf", "iges", "gcode", "svg", "pdf"]),
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

// Reply to pcb_capture_layer_reference — see PcbLayerAttestationResultEvent
// in types/ipc.ts. Payload mirrors make_pcb_layer_attestation_event
// (native/cad-core/src/app/impl/pcb_command_handlers.inc).
const pcbLayerAttestationResultEventSchema = z.object({
  id: z.string(),
  type: z.literal("pcb_layer_attestation_result"),
  payload: z.object({
    persistent_id: z.string(),
    attestation: z.object({
      layer_feature_id: z.string(),
      role: z.string().default(""),
      ring_count: z.number().default(0),
      min_x: z.number().default(0),
      min_y: z.number().default(0),
      max_x: z.number().default(0),
      max_y: z.number().default(0),
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

// Shared tool library events.  toolEntrySchema lives in
// ipc/camSchema.ts; a lenient inline shape keeps this file
// self-contained (the event carries full payload-shaped tools).
const camToolListResultEventSchema = z.object({
  id: z.string(),
  type: z.literal("cam_tool_list_result"),
  payload: z.object({
    tools: z.array(z.record(z.string(), z.unknown())),
  }),
});

const camToolParseResultEventSchema = z.object({
  id: z.string(),
  type: z.literal("cam_tool_parse_result"),
  payload: z.object({
    format: z.string(),
    tools: z.array(z.record(z.string(), z.unknown())),
    warnings: z.array(z.string()),
  }),
});

const camToolExportTextResultEventSchema = z.object({
  id: z.string(),
  type: z.literal("cam_tool_export_text_result"),
  payload: z.object({
    format: z.string(),
    text: z.string(),
  }),
});

// Reply to cam_tool_export_file (the core wrote the file itself).
const camToolExportFileResultEventSchema = z.object({
  id: z.string(),
  type: z.literal("cam_tool_export_file_result"),
  payload: z.object({
    file_path: z.string(),
    exported_count: z.number(),
  }),
});

// Drawing template file I/O replies (the CREATE DRAWING dialog).  The
// loaded template is lenient here — the hook re-parses it with the
// strict drawingTemplateSchema.
const drawingTemplateSaveResultEventSchema = z.object({
  id: z.string(),
  type: z.literal("drawing_template_save_result"),
  payload: z.object({
    file_path: z.string(),
  }),
});

const drawingTemplateLoadResultEventSchema = z.object({
  id: z.string(),
  type: z.literal("drawing_template_load_result"),
  payload: z.object({
    template: z.record(z.string(), z.unknown()),
  }),
});

const cornerTrimPreviewResultEventSchema = z.object({
  id: z.string(),
  type: z.literal("corner_trim_preview_result"),
  payload: z.union([
    z.object({
      entity_a_id: z.string(),
      entity_b_id: z.string(),
      valid: z.boolean(),
      // Document revision the preview was computed against.
      revision: z.number(),
      corner: z.tuple([z.number(), z.number()]).optional(),
      a: z.object({
        kind: z.enum(["line", "arc"]),
        start: z.tuple([z.number(), z.number()]),
        end: z.tuple([z.number(), z.number()]),
        center: z.tuple([z.number(), z.number()]).optional(),
        radius: z.number().optional(),
        ccw: z.boolean().optional(),
      }).optional(),
      b: z.object({
        kind: z.enum(["line", "arc"]),
        start: z.tuple([z.number(), z.number()]),
        end: z.tuple([z.number(), z.number()]),
        center: z.tuple([z.number(), z.number()]).optional(),
        radius: z.number().optional(),
        ccw: z.boolean().optional(),
      }).optional(),
    }),
    z.null(),
  ]),
});

// Non-mutating drawing dimension preview — the core resolves the pick
// and replies with value + graphics until Enter commits.  Lenient:
// an unresolved pick replies with only {error}.
const drawingDimensionPreviewEventSchema = z.object({
  id: z.string(),
  type: z.literal("drawing_dimension_preview"),
  payload: z.object({
    drawing_id: z.string(),
    view_id: z.string(),
    dim_type: z.string(),
    pick: z.tuple([z.number(), z.number()]),
    pick_2: z.tuple([z.number(), z.number()]).optional(),
    error: z.string().optional(),
    value: z.number().optional(),
    text_value: z.string().optional(),
    curves: z.array(viewportDrawingCurveShape).optional(),
    text: viewportDrawingTextShape.optional(),
  }),
});

// Non-mutating drawing annotation preview (GEOMETRY/SYMBOLS/ANNOTATE)
// — the dimension preview contract with `kind` instead of `dim_type`.
const drawingAnnotationPreviewEventSchema = z.object({
  id: z.string(),
  type: z.literal("drawing_annotation_preview"),
  payload: z.object({
    drawing_id: z.string(),
    view_id: z.string(),
    kind: z.string(),
    pick: z.tuple([z.number(), z.number()]),
    pick_2: z.tuple([z.number(), z.number()]).optional(),
    error: z.string().optional(),
    text_value: z.string().optional(),
    curves: z.array(viewportDrawingCurveShape).optional(),
    text: viewportDrawingTextShape.optional(),
  }),
});

// Non-mutating Insert View ghost — the core projects an uncommitted
// view definition and replies with its geometry + a view-shaped
// record (empty view_id marks the ghost; warning = degraded).
const drawingViewPreviewResultEventSchema = z.object({
  id: z.string(),
  type: z.literal("drawing_view_preview_result"),
  payload: drawingViewPreviewResultSchema,
});

// Selection-only reply for the select_* commands.  The document is unchanged,
// so the core sends the selection fields instead of re-serialising everything
// (a click used to cost 1,219 KB on a dense sketch).
export const selectionChangedEventSchema = z.object({
  id: z.string(),
  type: z.literal("selection_changed"),
  payload: z.object({
    revision: z.number(),
    selected_feature_id: z.string().nullable().optional(),
    selected_reference_id: z.string().nullable().optional(),
    selected_face_id: z.string().nullable().optional(),
    selected_edge_ids: z.array(z.string()).default([]),
    selected_vertex_ids: z.array(z.string()).default([]),
    selected_sketch_vertex_id: z.string().nullable().optional(),
    selected_sketch_entity_id: z.string().nullable().optional(),
    selected_sketch_vertex_ids: z.array(z.string()).default([]),
    selected_sketch_entity_ids: z.array(z.string()).default([]),
    selected_sketch_dimension_id: z.string().nullable().optional(),
    selected_sketch_text_id: z.string().nullable().optional(),
    selected_sketch_profile_id: z.string().nullable().optional(),
    selected_sketch_profile_ids: z.array(z.string()).default([]),
  }),
});

// Incremental sketch edit (wiki/IPC-Payload-Plan.md phase D1): the entities the
// command touched, by id. Rows are loosely typed on purpose - the core owns
// their shape and the store patches them field by field.
export const sketchEntitiesChangedEventSchema = z.object({
  id: z.string(),
  type: z.literal("sketch_entities_changed"),
  payload: z.object({
    revision: z.number(),
    feature_id: z.string(),
    lines: z.array(z.record(z.string(), z.unknown())).optional(),
    vertices: z.array(z.record(z.string(), z.unknown())).optional(),
    removed: z.array(z.string()).optional(),
    derived: z
      .object({
        profile_count: z.number().optional(),
        vertex_count: z.number().optional(),
      })
      .optional(),
  }),
});

// Incremental feature edit (wiki/IPC-Payload-Plan.md phase D2): one feature's
// stored parameters, patched by id. The feature body is loosely typed on purpose
// - the core owns its shape and the store replaces it field by field.
export const featureChangedEventSchema = z.object({
  id: z.string(),
  type: z.literal("feature_changed"),
  payload: z.object({
    revision: z.number(),
    feature_id: z.string(),
    feature: z.record(z.string(), z.unknown()),
    removed_feature_ids: z.array(z.string()).default([]),
  }),
});

// On-demand profile geometry (phase E1): the reply to get_sketch_profiles.
// Profiles are loosely typed — the store merges them by profile_id into the
// document's sketch_parameters.profiles, which owns their exact shape.
export const sketchProfilesEventSchema = z.object({
  id: z.string(),
  type: z.literal("sketch_profiles"),
  payload: z.object({
    feature_id: z.string(),
    profile_count: z.number(),
    profiles: z.array(z.record(z.string(), z.unknown())).default([]),
  }),
});

// S5: loose-end diagnostics (reply to get_sketch_diagnostics). Declared here or
// the parser rejects the reply and the request hangs (the documented zod trap).
export const sketchDiagnosticsEventSchema = z.object({
  id: z.string(),
  type: z.literal("sketch_diagnostics"),
  payload: z.object({
    feature_id: z.string(),
    open_count: z.number(),
    untied_count: z.number(),
    open: z
      .array(
        z.object({
          vertex_id: z.string(),
          x: z.number(),
          y: z.number(),
          owner_ids: z.array(z.string()).default([]),
        }),
      )
      .default([]),
    untied: z
      .array(
        z.object({
          vertex_a_id: z.string(),
          vertex_b_id: z.string(),
          gap: z.number(),
        }),
      )
      .default([]),
  }),
});

// S11: the read-only auto-constrain preview. Declared here or the parser rejects
// the reply and the request times out (the documented zod trap).
export const autoConstraintsPreviewEventSchema = z.object({
  id: z.string(),
  type: z.literal("auto_constraints_preview"),
  payload: z.object({
    feature_id: z.string(),
    candidate_count: z.number(),
    candidates: z
      .array(
        z.object({
          kind: z.string(),
          target_ids: z.array(z.string()).default([]),
          error: z.number(),
          reason: z.string().default(""),
        }),
      )
      .default([]),
    current_dofs: z.number(),
  }),
});

export const coreMessageSchema = z.union([
  sketchEntitiesChangedEventSchema,
  featureChangedEventSchema,
  sketchProfilesEventSchema,
  sketchDiagnosticsEventSchema,
  autoConstraintsPreviewEventSchema,
  helloEventSchema,
  pongEventSchema,
  documentCreatedEventSchema,
  documentStateEventSchema,
  selectionChangedEventSchema,
  sessionStateEventSchema,
  viewportStateEventSchema,
  documentExportedEventSchema,
  camExportGcodeTextResultEventSchema,
  documentSavedEventSchema,
  logEventSchema,
  trimPreviewResultEventSchema,
  cornerTrimPreviewResultEventSchema,
  camGenerationProgressEventSchema,
  camGenerationResultEventSchema,
  camPostListResultEventSchema,
  camMachineListResultEventSchema,
  camToolListResultEventSchema,
  camToolParseResultEventSchema,
  camToolExportTextResultEventSchema,
  camToolExportFileResultEventSchema,
  camFaceAttestationResultEventSchema,
  camEdgeAttestationResultEventSchema,
  camAttestationResultEventSchema,
  pcbLayerAttestationResultEventSchema,
  drawingDimensionPreviewEventSchema,
  drawingAnnotationPreviewEventSchema,
  drawingViewPreviewResultEventSchema,
  drawingTemplateSaveResultEventSchema,
  drawingTemplateLoadResultEventSchema,
  errorEventSchema,
]);

