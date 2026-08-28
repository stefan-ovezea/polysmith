// CAM type definitions — mirroring native/cad-core/src/core/cam/cam_types.h
//
// These are the canonical target-schema CAM types.  The DocumentState
// interface in ipc.ts holds a CamDocumentData container; the detailed
// shapes below are used when working with individual CAM entities.

// ══════════════════════════════════════════════════════════════════
//  TNP-Safe References
// ══════════════════════════════════════════════════════════════════

export interface Bounds3D {
  min_x: number; min_y: number; min_z: number;
  max_x: number; max_y: number; max_z: number;
}

export interface FaceAttestation {
  bounds: Bounds3D;
  area: number;
  normal: [number, number, number];
  sample_points: Array<[number, number, number]>;
}

export interface EdgeAttestation {
  start_point: [number, number, number];
  end_point: [number, number, number];
  length: number;
  tangent: [number, number, number];
  adjacent_face_normals?: Array<[number, number, number]>;
}

// Witness data to re-identify a sketch profile region after sketch
// edits. Profile ids churn because the core recomputes the whole region
// list; the geometry is the stable identity. All coordinates are
// sketch-local.
export interface SketchProfileAttestation {
  sketch_feature_id: string;
  profile_id: string;
  center_x: number;
  center_y: number;
  area: number;
  min_x: number;
  min_y: number;
  max_x: number;
  max_y: number;
  boundary_edge_kinds: string[];
  inner_loop_count: number;
  source_circle_id?: string;
}

export interface GeometryReference {
  persistent_id: string;
  attestation: FaceAttestation | EdgeAttestation | SketchProfileAttestation;
  fallback_strategy: "warn_user" | "require_user" | "fail_operation" | "auto_resolve";
}

// ══════════════════════════════════════════════════════════════════
//  Setup & Machine
// ══════════════════════════════════════════════════════════════════

export type StockType = "bounding_box" | "cylinder" | "from_solid" | "from_mesh";

export interface StockDefinition {
  type: StockType;
  origin?: [number, number, number];
  size?: [number, number, number];
  diameter?: number;
  length?: number;
  margin: number;
  solid_reference?: GeometryReference;
  mesh_reference?: string;
}

export type MachineType =
  | "3_axis_mill" | "4_axis_mill" | "5_axis_mill"
  | "lathe_2_axis" | "lathe_live_tooling"
  | "laser" | "plasma" | "printer";

export interface MachineAxes {
  x: number; y: number; z: number;
  a?: number; b?: number; c?: number;
}

export interface WcsOrigin {
  feature_id: string;
  face_reference: GeometryReference;
  /** Last-resolved machine origin, refreshed by the CAM dependency
   *  pass so the post-processor needs no face resolution at export. */
  position?: [number, number, number];
}

export interface CamSetup {
  setup_id: string;
  name: string;
  machine_type: MachineType;
  machine_axes: MachineAxes;
  stock: StockDefinition;
  wcs_origin: WcsOrigin;
  safety_height: number;
  retract_height: number;
  units: "mm" | "inch";
}

// ══════════════════════════════════════════════════════════════════
//  Tool Library
// ══════════════════════════════════════════════════════════════════

export type ToolType =
  | "endmill_flat" | "endmill_ball" | "endmill_bull"
  | "drill" | "facemill" | "chamfer" | "threadmill"
  | "turning_insert" | "laser" | "plasma";

export interface ToolEntry {
  tool_id: string;
  name: string;
  type: ToolType;
  diameter_mm: number;
  corner_radius_mm: number;
  flute_length_mm: number;
  overall_length_mm: number;
  shank_diameter_mm: number;
  material: "carbide" | "hss" | "diamond" | "ceramic" | "other";
  coating?: "tin" | "ticn" | "alticn" | "dlc" | "none";
  coolant_through: boolean;
  max_spindle_rpm: number;
  default_feedrate_mm_per_min: number;
  default_plunge_feedrate_mm_per_min: number;
  default_stepdown_mm: number;
  default_stepover_percent: number;
}

// ══════════════════════════════════════════════════════════════════
//  Operations
// ══════════════════════════════════════════════════════════════════

export type CamOperationType =
  | "face_milling" | "pocket_2d" | "contour_2d" | "slot"
  | "drilling" | "adaptive_clearing" | "parallel_3d"
  | "contour_3d" | "chamfer" | "thread_milling" | "engrave"
  | "laser_cut";

export type CamOperationStatus =
  | "pending" | "generated" | "needs_regenerate" | "error" | "deleted";

export interface CamGeometryReferences {
  machining_regions: GeometryReference[];
  avoidance_regions: GeometryReference[];
  guide_curves: GeometryReference[];
  check_surfaces: GeometryReference[];
}

export interface CamPointLocation {
  vertex_id: string;
  position: [number, number, number];
  surface_normal: [number, number, number];
  hole_diameter?: number;
  sample_face: GeometryReference;
  fallback_strategy: "warn_user" | "require_user" | "fail_operation";
}

export type ClearingStrategy = "zigzag" | "one_way" | "offset" | "adaptive" | "spiral";
export type DrillingCycleType =
  | "g81_standard" | "g82_dwell" | "g83_peck"
  | "g73_high_speed_peck" | "g84_tap" | "g85_bore" | "g87_back_bore";

// Laser cutting parameters (only meaningful when type == "laser_cut").
// Follows the per-type optional block pattern of the other strategy
// fields so the operation struct stays one unified shape.
export interface LaserCutParameters {
  kerf_width_mm: number;         // cut width compensation, both sides
  lead_in_mm: number;            // straight lead-in length
  lead_out_mm: number;           // straight lead-out length
  pierce_dwell_seconds: number;  // G4 dwell after pierce
  power_percent: number;         // 0..100
  passes: number;                // repeat contour (same path in v1)
  mode: "cut" | "score" | "engrave";
  material_thickness_mm: number;
  cut_plane_offset_mm: number;   // cut-plane Z relative to sketch plane
  dynamic_power: boolean;        // true -> M4 (power scales with feed)
}

export interface CamOperationParameters {
  spindle_rpm: number;
  feedrate_mm_per_min: number;
  plunge_feedrate_mm_per_min: number;
  stepdown_mm?: number;
  stepover_percent?: number;
  stock_allowance_mm: number;
  cutting_direction: "climb" | "conventional" | "mixed";
  finish_pass: boolean;
  multiple_passes: boolean;
  strategy?: ClearingStrategy;
  cycle_type?: DrillingCycleType;
  hole_depth_mm?: number;
  peck_depth_mm?: number;
  dwell_seconds?: number;
  engagement_angle_deg?: number;
  zigzag_angle_deg?: number;     // for face milling
  laser?: LaserCutParameters;    // for laser_cut
  coolant: "off" | "flood" | "mist" | "through_tool";
}

export interface CamOperationDependencies {
  parent_operation_ids: string[];
  requires_operation_id: string | null;
  use_stock_from_previous: boolean;
}

export interface ExternalStorage {
  format: "binary" | "json" | "gcode";
  file_reference: string;
  compressed: boolean;
  size_bytes?: number;
}

export interface ToolpathCache {
  toolpath_id: string;
  status: "up_to_date" | "needs_regenerate" | "generating" | "error";
  generated_at?: string;
  bounds?: Bounds3D;
  total_length_mm?: number;
  estimated_time_seconds?: number;
  num_moves?: number;
  num_rapids?: number;
  num_feeds?: number;
  external_storage?: ExternalStorage;
}

export interface CamOperation {
  op_id: string;
  name: string;
  type: CamOperationType;
  enabled: boolean;
  tool_id: string;
  geometry_references: CamGeometryReferences;
  point_locations: CamPointLocation[];
  parameters: CamOperationParameters;
  dependencies: CamOperationDependencies;
  toolpath_cache?: ToolpathCache;
  status: CamOperationStatus;
  /** Human-readable degrade info (the CAM analogue of
   *  FeatureEntry::dependency_warning). Empty when healthy. */
  status_message: string;
}

// Payload for `cam_operation_create`: a serialized CamOperation whose
// op_id is optional (the core assigns "cam-op-N") and whose geometry
// references may be omitted entirely — when machining_regions is empty
// (or absent) the core captures witness references from the document's
// selected sketch profiles automatically.
export type CamOperationPayload = Omit<
  CamOperation,
  "op_id" | "geometry_references"
> & {
  op_id?: string;
  geometry_references?: CamGeometryReferences;
};

// ══════════════════════════════════════════════════════════════════
//  Post-Processor
// ══════════════════════════════════════════════════════════════════

export type PostProcessorType =
  | "fanuc" | "linuxcnc" | "mach3" | "mach4"
  | "grbl" | "marlin" | "custom";

export interface PostProcessorOptions {
  add_line_numbers: boolean;
  use_arcs: boolean;
  absolute_coordinates: boolean;
  tool_change_mcode: number;
  spindle_start_mcode: number;
  coolant_mcode_on: number;
  coolant_mcode_off: number;
  decimal_places: number;
  separate_rapids?: boolean;
  header_string?: string;
  footer_string?: string;
}

export interface PostProcessor {
  type: PostProcessorType;
  filename: string;
  options: PostProcessorOptions;
}

// ══════════════════════════════════════════════════════════════════
//  Simulation
// ══════════════════════════════════════════════════════════════════

export interface CollisionReport {
  op_id: string;
  tool_id: string;
  position: [number, number, number];
  severity: "warning" | "error" | "critical";
  message: string;
}

export interface SimulationData {
  stock_after_op_id?: string;
  stock_mesh_reference?: string;
  last_verification?: string;
  collisions_detected: boolean;
  collision_report: CollisionReport[];
}

// ══════════════════════════════════════════════════════════════════
//  Document Container
// ══════════════════════════════════════════════════════════════════

export interface CamDocumentData {
  setups: CamSetup[];
  tool_library: ToolEntry[];
  operations: CamOperation[];
  post_processor: PostProcessor | null;
  simulation: SimulationData | null;
}
