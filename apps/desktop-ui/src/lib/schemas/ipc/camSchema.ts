// Zod schema for the CAM section of the document state — mirrors
// polysmith::core::CamDocumentData (native/cad-core/src/core/cam/
// cam_types.h).  The core always emits the full typed shape, but every
// field carries a default (matching the C++ struct defaults) and the
// objects are .passthrough() so documents saved before a field existed
// — or hand-crafted test payloads — still parse.  A CAM parse failure
// would take down the whole document, so leniency is the priority.

import { z } from "zod";

const vec3Schema = z.tuple([z.number(), z.number(), z.number()]);

const bounds3DSchema = z
  .object({
    min_x: z.number().default(0),
    min_y: z.number().default(0),
    min_z: z.number().default(0),
    max_x: z.number().default(0),
    max_y: z.number().default(0),
    max_z: z.number().default(0),
  })
  .passthrough();

// ── TNP-Safe References ───────────────────────────────────────────

const faceAttestationSchema = z
  .object({
    bounds: bounds3DSchema.default({
      min_x: 0, min_y: 0, min_z: 0, max_x: 0, max_y: 0, max_z: 0,
    }),
    area: z.number().default(0),
    normal: vec3Schema.default([0, 0, 1]),
    sample_points: z.array(vec3Schema).default([]),
  })
  .passthrough();

const edgeAttestationSchema = z
  .object({
    start_point: vec3Schema.default([0, 0, 0]),
    end_point: vec3Schema.default([0, 0, 0]),
    length: z.number().default(0),
    tangent: vec3Schema.default([1, 0, 0]),
    adjacent_face_normals: z.array(vec3Schema).optional(),
  })
  .passthrough();

const sketchProfileAttestationSchema = z
  .object({
    sketch_feature_id: z.string().default(""),
    profile_id: z.string().default(""),
    center_x: z.number().default(0),
    center_y: z.number().default(0),
    area: z.number().default(0),
    min_x: z.number().default(0),
    min_y: z.number().default(0),
    max_x: z.number().default(0),
    max_y: z.number().default(0),
    boundary_edge_kinds: z.array(z.string()).default([]),
    inner_loop_count: z.number().default(0),
    source_circle_id: z.string().optional(),
  })
  .passthrough();

const geometryReferenceSchema = z
  .object({
    persistent_id: z.string().default(""),
    attestation: z
      .union([
        faceAttestationSchema,
        sketchProfileAttestationSchema,
        edgeAttestationSchema,
        z.object({}).passthrough(),
      ])
      .nullable()
      .optional(),
    fallback_strategy: z
      .enum(["warn_user", "require_user", "fail_operation", "auto_resolve"])
      .default("warn_user"),
  })
  .passthrough();

// ── Setup & Machine ───────────────────────────────────────────────

const stockDefinitionSchema = z
  .object({
    type: z
      .enum(["bounding_box", "cylinder", "from_solid", "from_mesh"])
      .default("bounding_box"),
    origin: vec3Schema.optional(),
    size: vec3Schema.optional(),
    diameter: z.number().optional(),
    length: z.number().optional(),
    margin: z.number().default(3),
    solid_reference: geometryReferenceSchema.optional(),
    mesh_reference: z.string().optional(),
  })
  .passthrough();

const machineAxesSchema = z
  .object({
    x: z.number().default(500),
    y: z.number().default(400),
    z: z.number().default(300),
    a: z.number().optional(),
    b: z.number().optional(),
    c: z.number().optional(),
  })
  .passthrough();

const wcsOriginSchema = z
  .object({
    feature_id: z.string().default(""),
    face_reference: geometryReferenceSchema.optional(),
    position: vec3Schema.optional(),
  })
  .passthrough();

const camSetupSchema = z
  .object({
    setup_id: z.string().default(""),
    name: z.string().default("Setup"),
    machine_type: z.string().default("3_axis_mill"),
    machine_axes: machineAxesSchema.default({ x: 500, y: 400, z: 300 }),
    stock: stockDefinitionSchema.default({ type: "bounding_box", margin: 3 }),
    wcs_origin: wcsOriginSchema.default({ feature_id: "" }),
    safety_height: z.number().default(50),
    retract_height: z.number().default(5),
    units: z.string().default("mm"),
  })
  .passthrough();

// ── Tool Library ──────────────────────────────────────────────────

const toolEntrySchema = z
  .object({
    tool_id: z.string().default(""),
    name: z.string().default(""),
    type: z.string().default("endmill_flat"),
    diameter_mm: z.number().default(6),
    corner_radius_mm: z.number().default(0),
    flute_length_mm: z.number().default(20),
    overall_length_mm: z.number().default(60),
    shank_diameter_mm: z.number().default(6),
    material: z.string().default("carbide"),
    coating: z.string().optional(),
    coolant_through: z.boolean().default(false),
    max_spindle_rpm: z.number().default(15000),
    default_feedrate_mm_per_min: z.number().default(1000),
    default_plunge_feedrate_mm_per_min: z.number().default(500),
    default_stepdown_mm: z.number().default(1),
    default_stepover_percent: z.number().default(50),
  })
  .passthrough();

// ── Operations ────────────────────────────────────────────────────

// Single source of truth for laser defaults — the UI spreads this
// instead of carrying a parallel constants block.
export const laserCutParametersSchema = z
  .object({
    mode: z.enum(["cut", "score", "engrave"]).default("cut"),
    power_percent: z.number().default(85),
    speed_mm_per_s: z.number().optional(),
    passes: z.number().default(1),
    dynamic_power: z.boolean().default(true),
    air_assist: z.boolean().default(false),
    kerf_width_mm: z.number().default(0.15),
    kerf_side: z.enum(["auto", "outside", "inside", "none"]).default("auto"),
    lead_in_mm: z.number().default(2),
    lead_out_mm: z.number().default(2),
    lead_in_style: z.enum(["line", "arc"]).default("line"),
    lead_out_style: z.enum(["line", "arc"]).default("line"),
    lead_in_angle_deg: z.number().default(0),
    lead_out_angle_deg: z.number().default(0),
    overcut_mm: z.number().default(0),
    pierce_dwell_seconds: z.number().default(0.1),
    pierce_position: z
      .enum(["auto", "lead_start", "nearest_centroid"])
      .default("auto"),
    // Lead-side angle about the loop centroid; null = pierce_position
    // rules.  The core omits the field when unset, so default(null).
    pierce_angle_deg: z.number().nullable().default(null),
    tabs_enabled: z.boolean().default(false),
    tab_width_mm: z.number().default(0.5),
    tab_spacing_mm: z.number().default(20),
    tab_power_percent: z.number().default(0),
    tabs_on_holes: z.boolean().default(false),
    engrave_style: z.enum(["line", "fill"]).default("line"),
    line_spacing_mm: z.number().default(0.1),
    fill_angle_deg: z.number().default(0),
    fill_bidirectional: z.boolean().default(true),
    material_thickness_mm: z.number().default(3),
    cut_plane_offset_mm: z.number().default(0),
    // Polyline chords per full circle; 0 = auto chord tolerance.
    arc_segments_per_circle: z.number().int().min(0).max(360).default(0),
    cut_order: z
      .enum(["inner_first", "nearest_neighbor", "by_area"])
      .default("inner_first"),
  })
  .passthrough();

// Machine settings + test patterns (test_pattern is referenced by
// camOperationParametersSchema below — must be declared first).
export const laserMachineSettingsSchema = z
  .object({
    work_area_x_mm: z.number().default(400),
    work_area_y_mm: z.number().default(400),
    pointer_offset_x_mm: z.number().default(0),
    pointer_offset_y_mm: z.number().default(0),
  })
  .passthrough();

export const laserTestPatternParametersSchema = z
  .object({
    pattern: z
      .enum(["engrave_grid", "cut_grid", "kerf_gauge"])
      .default("engrave_grid"),
    power_min_percent: z.number().default(10),
    power_max_percent: z.number().default(100),
    power_steps: z.number().default(5),
    speed_min_mm_per_s: z.number().default(5),
    speed_max_mm_per_s: z.number().default(50),
    speed_steps: z.number().default(5),
    cell_size_mm: z.number().default(10),
    cell_spacing_mm: z.number().default(5),
    start_x_mm: z.number().default(5),
    start_y_mm: z.number().default(5),
    line_spacing_mm: z.number().default(0.1),
    kerf_width_mm: z.number().default(0.15),
    power_percent: z.number().default(85),
    speed_mm_per_s: z.number().default(10),
    cell_labels: z.boolean().default(true),
  })
  .passthrough();

const camOperationParametersSchema = z
  .object({
    spindle_rpm: z.number().default(8000),
    feedrate_mm_per_min: z.number().default(1200),
    plunge_feedrate_mm_per_min: z.number().default(600),
    stepdown_mm: z.number().optional(),
    stepover_percent: z.number().optional(),
    stock_allowance_mm: z.number().default(0.2),
    cutting_direction: z.string().default("climb"),
    finish_pass: z.boolean().default(false),
    multiple_passes: z.boolean().default(false),
    strategy: z.string().optional(),
    cycle_type: z.string().optional(),
    hole_depth_mm: z.number().optional(),
    peck_depth_mm: z.number().optional(),
    dwell_seconds: z.number().optional(),
    engagement_angle_deg: z.number().optional(),
    zigzag_angle_deg: z.number().optional(),
    laser: laserCutParametersSchema.optional(),
    test_pattern: laserTestPatternParametersSchema.optional(),
    coolant: z.string().default("off"),
  })
  .passthrough();

const camGeometryReferencesSchema = z
  .object({
    machining_regions: z.array(geometryReferenceSchema).default([]),
    avoidance_regions: z.array(geometryReferenceSchema).default([]),
    guide_curves: z.array(geometryReferenceSchema).default([]),
    check_surfaces: z.array(geometryReferenceSchema).default([]),
  })
  .passthrough();

const camOperationDependenciesSchema = z
  .object({
    parent_operation_ids: z.array(z.string()).default([]),
    requires_operation_id: z.string().nullable().optional(),
    use_stock_from_previous: z.boolean().default(false),
  })
  .passthrough();

const externalStorageSchema = z
  .object({
    format: z.string().default("binary"),
    file_reference: z.string().default(""),
    compressed: z.boolean().default(false),
    size_bytes: z.number().optional(),
  })
  .passthrough();

const toolpathCacheSchema = z
  .object({
    toolpath_id: z.string().default(""),
    status: z.string().default("up_to_date"),
    generated_at: z.string().optional(),
    bounds: bounds3DSchema.optional(),
    total_length_mm: z.number().optional(),
    estimated_time_seconds: z.number().optional(),
    num_moves: z.number().optional(),
    num_rapids: z.number().optional(),
    num_feeds: z.number().optional(),
    external_storage: externalStorageSchema.optional(),
  })
  .passthrough();

const camOperationSchema = z
  .object({
    op_id: z.string().default(""),
    name: z.string().default(""),
    type: z.string().default("face_milling"),
    enabled: z.boolean().default(true),
    setup_id: z.string().default(""),
    tool_id: z.string().default(""),
    geometry_references: camGeometryReferencesSchema.default({
      machining_regions: [],
      avoidance_regions: [],
      guide_curves: [],
      check_surfaces: [],
    }),
    parameters: camOperationParametersSchema.default({
      spindle_rpm: 8000,
      feedrate_mm_per_min: 1200,
      plunge_feedrate_mm_per_min: 600,
      stock_allowance_mm: 0.2,
      cutting_direction: "climb",
      finish_pass: false,
      multiple_passes: false,
      coolant: "off",
    }),
    dependencies: camOperationDependenciesSchema.default({
      parent_operation_ids: [],
      use_stock_from_previous: false,
    }),
    toolpath_cache: toolpathCacheSchema.nullable().optional(),
    status: z.string().default("pending"),
    status_message: z.string().default(""),
  })
  .passthrough();

// ── Post-Processor ────────────────────────────────────────────────

const postProcessorSchema = z
  .object({
    type: z.string().default("fanuc"),
    filename: z.string().default(""),
  })
  .passthrough();

// ── Simulation ────────────────────────────────────────────────────

// ── Document Container ────────────────────────────────────────────

export const camDocumentDataSchema = z
  .object({
    setups: z.array(camSetupSchema).default([]),
    tool_library: z.array(toolEntrySchema).default([]),
    operations: z.array(camOperationSchema).default([]),
    post_processor: postProcessorSchema.nullable().default(null),
    machine_settings: laserMachineSettingsSchema.nullable().default(null),
  })
  .passthrough();
