// Zod schema for the drawing section of the document state — mirrors
// polysmith::core::DrawingDocumentData (native/cad-core/src/core/
// drawing/drawing_types.h).  The core always emits the full typed
// shape, but every field carries a default (matching the C++ struct
// defaults) and the objects are .passthrough() so documents saved
// before a field existed — or hand-crafted test payloads — still
// parse.  A drawing parse failure would take down the whole document,
// so leniency is the priority (the camSchema convention).

import { z } from "zod";

import {
  viewportDrawingCurveShape,
  viewportDrawingTextShape,
} from "./viewportStateSchema";

const vec2Schema = z.tuple([z.number(), z.number()]);
const vec3Schema = z.tuple([z.number(), z.number(), z.number()]);

// ── View frame ────────────────────────────────────────────────────

const drawingViewFrameSchema = z
  .object({
    origin: vec3Schema.default([0, 0, 0]),
    normal: vec3Schema.default([0, 0, 1]),
    x_direction: vec3Schema.default([1, 0, 0]),
  })
  .passthrough();

// ── Source edge witness ───────────────────────────────────────────

const sourceEdgeWitnessSchema = z
  .object({
    body_id: z.string().default(""),
    src_edge_index: z.number().default(-1),
    start_point: vec3Schema.default([0, 0, 0]),
    end_point: vec3Schema.default([0, 0, 0]),
    length: z.number().default(0),
    tangent: vec3Schema.default([1, 0, 0]),
    curve_kind: z.string().default("line"),
    center: vec3Schema.optional(),
    axis: vec3Schema.optional(),
    radius: z.number().optional(),
    param_range: vec2Schema.default([0, 0]),
  })
  .passthrough();

// ── Section definition ────────────────────────────────────────────

const sectionDefinitionSchema = z
  .object({
    cutting_plane_point: vec3Schema.default([0, 0, 0]),
    cutting_plane_normal: vec3Schema.default([0, 0, 1]),
    cut_away: z.boolean().default(true),
    label: z.string().default("A"),
    hatch_angle_deg: z.number().default(45),
    hatch_spacing_mm: z.number().default(3),
    construction_plane_id: z.string().optional(),
  })
  .passthrough();

// ── Title block (ISO 7200) ────────────────────────────────────────

const titleBlockSchema = z
  .object({
    legal_owner: z.string().default(""),
    identification: z.string().default(""),
    date: z.string().default(""),
    title: z.string().default(""),
    approver: z.string().default(""),
    creator: z.string().default(""),
    document_type: z.string().default(""),
    revision_rows: z
      .array(
        z
          .tuple([
            z.string(),
            z.string(),
            z.string(),
            z.string(),
            z.string(),
          ])
          .catch(() => ["", "", "", "", ""] as [string, string, string, string, string]),
      )
      .default([]),
  })
  .passthrough();

// ── Sheet ─────────────────────────────────────────────────────────

export const drawingSheetSchema = z
  .object({
    sheet_id: z.string().default(""),
    name: z.string().default(""),
    paper_size: z.string().default("A4"),
    orientation: z.string().default("landscape"),
    projection_angle: z.string().default("first_angle"),
    view_ids: z.array(z.string()).default([]),
    title_block: titleBlockSchema.default({
      legal_owner: "",
      identification: "",
      date: "",
      title: "",
      approver: "",
      creator: "",
      document_type: "",
      revision_rows: [],
    }),
  })
  .passthrough();

// ── Template (setup-only drawing definition) ──────────────────────

export const drawingTemplateSchema = z.object({
  name: z.string(),
  sheets: z.array(drawingSheetSchema),
});

// ── Annotation ────────────────────────────────────────────────────

const annotationExtensionSchema = z
  .object({
    kind: z.string().default(""),
    fields: z
      .array(
        z
          .tuple([z.string(), z.string()])
          .catch(() => ["", ""] as [string, string]),
      )
      .default([]),
  })
  .passthrough();

const annotationSchema = z
  .object({
    annotation_id: z.string().default(""),
    kind: z.string().default("linear"),
    view_id: z.string().default(""),
    source_edge_id: z.string().default(""),
    witness: sourceEdgeWitnessSchema.default({
      body_id: "",
      src_edge_index: -1,
      start_point: [0, 0, 0],
      end_point: [0, 0, 0],
      length: 0,
      tangent: [1, 0, 0],
      curve_kind: "line",
      param_range: [0, 0],
    }),
    witness_2: sourceEdgeWitnessSchema.optional(),
    attach_param: z.number().optional(),
    text_override: z.string().optional(),
    prefix: z.string().default(""),
    extensions: z.array(annotationExtensionSchema).default([]),
    dependency_broken: z.boolean().default(false),
    warning: z.string().default(""),
    text_offset: vec2Schema.optional(),
    arrow_flip: z.boolean().default(false),
  })
  .passthrough();

const sheetNoteSchema = z
  .object({
    note_id: z.string().default(""),
    sheet_id: z.string().default(""),
    text: z.string().default(""),
    position: vec2Schema.default([0, 0]),
    height_mm: z.number().default(3.5),
    angle_deg: z.number().default(0),
    h_align: z.string().default("center"),
  })
  .passthrough();

// ── Detail definition (ISO 128-3 §4.12 enlarged feature) ─────────

const detailDefinitionSchema = z
  .object({
    parent_view_id: z.string().default(""),
    center: vec2Schema.default([0, 0]),
    radius: z.number().default(1),
    label: z.string().default("A"),
  })
  .passthrough();

// ── View ──────────────────────────────────────────────────────────

const drawingViewSchema = z
  .object({
    view_id: z.string().default(""),
    kind: z.string().default("projection"),
    standard_view: z.string().default(""),
    custom_frame: drawingViewFrameSchema.optional(),
    source_body_ids: z.array(z.string()).default([]),
    scale: z.number().default(1),
    sheet_position: vec2Schema.default([0, 0]),
    show_hidden: z.boolean().default(false),
    section: sectionDefinitionSchema.optional(),
    detail: detailDefinitionSchema.optional(),
    broken_ref: z.string().optional(),
    warning: z.string().default(""),
  })
  .passthrough();

// ── Drawing ───────────────────────────────────────────────────────

const drawingSchema = z
  .object({
    drawing_id: z.string().default(""),
    name: z.string().default(""),
    sheets: z.array(drawingSheetSchema).default([]),
    views: z.array(drawingViewSchema).default([]),
    annotations: z.array(annotationSchema).default([]),
    notes: z.array(sheetNoteSchema).default([]),
  })
  .passthrough();

// ── Document-level drawing data ───────────────────────────────────

const drawingDocumentDataShape = z.object({
  drawings: z.array(drawingSchema).default([]),
  active_drawing_id: z.string().nullable().default(null),
  selected_view_id: z.string().nullable().default(null),
  selected_annotation_id: z.string().nullable().default(null),
  decimal_separator: z.string().default("."),
});

export const drawingDocumentDataSchema = drawingDocumentDataShape
  .passthrough()
  .catch(() => ({
    drawings: [],
    active_drawing_id: null,
    selected_view_id: null,
    selected_annotation_id: null,
    decimal_separator: ".",
  }));

// ── View preview result (drawing_view_preview_result event) ───────
//
// The core projects an UNCOMMITTED view definition and replies with
// the ghost geometry in the same vocabulary as a committed sheet view
// (curves/texts) plus a view record — an empty view_id marks the
// ghost, and a non-empty warning means the projection degraded.

export const drawingViewPreviewResultSchema = z.object({
  drawing_id: z.string(),
  sheet_id: z.string(),
  curves: z.array(viewportDrawingCurveShape).default([]),
  texts: z.array(viewportDrawingTextShape).default([]),
  view: z.object({
    view_id: z.string().default(""),
    label: z.string().default(""),
    scale: z.number().default(1),
    origin: z.tuple([z.number(), z.number()]).default([0, 0]),
    min: z.tuple([z.number(), z.number()]).default([0, 0]),
    max: z.tuple([z.number(), z.number()]).default([0, 0]),
    stale: z.boolean().default(false),
    warning: z.string().default(""),
  }),
});
