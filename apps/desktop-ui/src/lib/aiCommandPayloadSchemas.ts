import { z } from "zod";

const emptyPayload = z.object({}).strict();
const numberField = z.number().finite();
const stringField = z.string();
const booleanField = z.boolean();
const stringArray = z.array(stringField);
const vector3Schema = z
  .object({ x: numberField, y: numberField, z: numberField })
  .strict();
const planeFrameSchema = z
  .object({
    origin: vector3Schema,
    x_axis: vector3Schema,
    y_axis: vector3Schema,
    normal: vector3Schema,
  })
  .strict();
const moveParametersSchema = z
  .object({
    target_body_id: stringField.optional(),
    translation_x: numberField.optional(),
    translation_y: numberField.optional(),
    translation_z: numberField.optional(),
    rotation_x_degrees: numberField.optional(),
    rotation_y_degrees: numberField.optional(),
    rotation_z_degrees: numberField.optional(),
    is_pending: booleanField.optional(),
  })
  .strict();

export const commandPayloadSchemas = {
  ping: emptyPayload,
  create_document: emptyPayload,
  get_document_state: emptyPayload,
  get_session_state: emptyPayload,
  get_viewport_state: emptyPayload,
  export_document: z.object({ file_path: stringField }).strict(),
  export_document_stl: z.object({ file_path: stringField }).strict(),
  export_body_stl: z
    .object({ file_path: stringField, body_id: stringField })
    .strict(),
  save_document: z.object({ file_path: stringField }).strict(),
  load_document: z.object({ file_path: stringField }).strict(),
  project_face_into_sketch: z.object({ face_id: stringField }).strict(),
  project_profile_into_sketch: z.object({ profile_id: stringField }).strict(),
  project_edge_into_sketch: z.object({ edge_id: stringField }).strict(),
  project_vertex_into_sketch: z.object({ vertex_id: stringField }).strict(),
  add_box_feature: z
    .object({
      width: numberField,
      height: numberField,
      depth: numberField,
    })
    .strict(),
  add_cylinder_feature: z
    .object({ radius: numberField, height: numberField })
    .strict(),
  update_box_feature: z
    .object({
      feature_id: stringField,
      width: numberField,
      height: numberField,
      depth: numberField,
    })
    .strict(),
  update_cylinder_feature: z
    .object({
      feature_id: stringField,
      radius: numberField,
      height: numberField,
    })
    .strict(),
  update_extrude_depth: z
    .object({ feature_id: stringField, depth: numberField })
    .strict(),
  set_feature_suppressed: z
    .object({ feature_id: stringField, suppressed: booleanField })
    .strict(),
  rename_feature: z
    .object({ feature_id: stringField, name: stringField })
    .strict(),
  delete_feature: z.object({ feature_id: stringField }).strict(),
  undo: emptyPayload,
  redo: emptyPayload,
  set_timeline_cursor: z
    .object({ included_action_count: z.number().int().min(0) })
    .strict(),
  select_feature: z.object({ feature_id: stringField }).strict(),
  select_reference: z.object({ reference_id: stringField }).strict(),
  select_face: z.object({ face_id: stringField }).strict(),
  select_edge: z
    .object({ edge_id: stringField, additive: booleanField })
    .strict(),
  select_vertex: z
    .object({ vertex_id: stringField, additive: booleanField })
    .strict(),
  create_fillet: z
    .object({ edge_ids: stringArray, radius: numberField })
    .strict(),
  update_fillet_radius: z
    .object({ feature_id: stringField, radius: numberField })
    .strict(),
  update_fillet_edges: z
    .object({ feature_id: stringField, edge_ids: stringArray })
    .strict(),
  update_chamfer_edges: z
    .object({ feature_id: stringField, edge_ids: stringArray })
    .strict(),
  create_chamfer: z
    .object({ edge_ids: stringArray, distance: numberField })
    .strict(),
  update_chamfer_distance: z
    .object({ feature_id: stringField, distance: numberField })
    .strict(),
  confirm_fillet: z.object({ feature_id: stringField }).strict(),
  confirm_chamfer: z.object({ feature_id: stringField }).strict(),
  create_shell: z
    .object({ face_id: stringField, thickness: numberField })
    .strict(),
  update_shell_thickness: z
    .object({ feature_id: stringField, thickness: numberField })
    .strict(),
  confirm_shell: z.object({ feature_id: stringField }).strict(),
  create_offset_plane: z
    .object({ source_plane_id: stringField, offset: numberField })
    .strict(),
  create_midplane: z
    .object({ source_plane_ids: z.tuple([stringField, stringField]) })
    .strict(),
  create_tangent_plane: z
    .object({ source_face_id: stringField })
    .strict(),
  create_angle_plane: z
    .object({
      source_plane_id: stringField,
      source_axis_id: stringField,
      angle_degrees: numberField,
    })
    .strict(),
  create_construction_axis: z.object({ source_id: stringField }).strict(),
  create_construction_point: z.object({ source_id: stringField }).strict(),
  create_hole: z
    .object({
      face_id: stringField,
      center_x: numberField,
      center_y: numberField,
      center_z: numberField,
      hole_type: z.enum(["simple", "counterbore", "countersink", "spotface"]).optional(),
      extent_type: z.enum(["blind", "through_all"]).optional(),
      diameter: numberField.optional(),
      depth: numberField.optional(),
      standard: z.enum(["custom", "metric", "imperial"]).optional(),
      standard_size: stringField.optional(),
      hole_fit: z.enum(["clearance", "tap_drill", "threaded"]).optional(),
      thread_enabled: booleanField.optional(),
      thread_spec: stringField.optional(),
      thread_pitch: numberField.optional(),
      major_diameter: numberField.optional(),
      minor_diameter: numberField.optional(),
      thread_representation: z.enum(["cosmetic", "modeled"]).optional(),
    })
    .strict(),
  update_hole_parameters: z
    .object({ feature_id: stringField, parameters: z.record(z.string(), z.unknown()) })
    .strict(),
  confirm_hole: z.object({ feature_id: stringField }).strict(),
  create_helix: z
    .object({
      axis_source_id: stringField,
      radius: numberField.optional(),
      pitch: numberField.optional(),
      height: numberField.optional(),
      handedness: z.enum(["left", "right"]).optional(),
      start_angle_degrees: numberField.optional(),
    })
    .strict(),
  update_helix_parameters: z
    .object({ feature_id: stringField, parameters: z.record(z.string(), z.unknown()) })
    .strict(),
  create_thread: z.record(z.string(), z.unknown()),
  update_thread_parameters: z
    .object({ feature_id: stringField, parameters: z.record(z.string(), z.unknown()) })
    .strict(),
  confirm_thread: z.object({ feature_id: stringField }).strict(),
  create_fastener: z.record(z.string(), z.unknown()),
  update_fastener_parameters: z
    .object({ feature_id: stringField, parameters: z.record(z.string(), z.unknown()) })
    .strict(),
  create_move: z
    .object({
      target_body_id: stringField,
      parameters: moveParametersSchema.optional(),
      translation_x: numberField.optional(),
      translation_y: numberField.optional(),
      translation_z: numberField.optional(),
      rotation_x_degrees: numberField.optional(),
      rotation_y_degrees: numberField.optional(),
      rotation_z_degrees: numberField.optional(),
    })
    .strict(),
  update_move_parameters: z
    .object({ feature_id: stringField, parameters: moveParametersSchema })
    .strict(),
  confirm_move: z.object({ feature_id: stringField }).strict(),
  create_body_copy: z
    .object({
      source_body_id: stringField,
      copy_mode: z.enum(["linked", "standalone"]).optional(),
    })
    .strict(),
  unlink_body_copy: z.object({ feature_id: stringField }).strict(),
  update_offset_plane: z
    .object({ feature_id: stringField, offset: numberField })
    .strict(),
  update_angle_plane: z
    .object({ feature_id: stringField, angle_degrees: numberField })
    .strict(),
  start_sketch_on_plane: z.object({ reference_id: stringField }).strict(),
  start_sketch_on_face: z
    .object({ face_id: stringField, plane_frame: planeFrameSchema })
    .strict(),
  add_sketch_line: z
    .object({
      start_x: numberField,
      start_y: numberField,
      end_x: numberField,
      end_y: numberField,
      is_construction: booleanField,
    })
    .strict(),
  set_sketch_line_construction: z
    .object({ line_id: stringField, is_construction: booleanField })
    .strict(),
  set_sketch_midpoint_anchor: z
    .object({ point_id: stringField, host_line_id: stringField })
    .strict(),
  add_sketch_angle_dimension: z
    .object({ first_line_id: stringField, second_line_id: stringField })
    .strict(),
  add_sketch_distance_dimension: z
    .object({ first_entity_id: stringField, second_entity_id: stringField })
    .strict(),
  set_sketch_point_line_anchor: z
    .object({
      point_id: stringField,
      host_line_id: stringField,
      t: numberField,
    })
    .strict(),
  add_sketch_rectangle: z
    .object({
      start_x: numberField,
      start_y: numberField,
      end_x: numberField,
      end_y: numberField,
      is_construction: booleanField,
    })
    .strict(),
  add_sketch_circle: z
    .object({
      center_x: numberField,
      center_y: numberField,
      radius: numberField,
      is_construction: booleanField,
    })
    .strict(),
  add_sketch_arc: z
    .object({
      start_x: numberField,
      start_y: numberField,
      end_x: numberField,
      end_y: numberField,
      anchor_x: numberField,
      anchor_y: numberField,
      mode: z.enum(["three_point", "center_start_end"]),
      is_construction: booleanField,
    })
    .strict(),
  add_sketch_fillet: z
    .object({
      corner_point_id: stringField,
      line_a_id: stringField,
      line_b_id: stringField,
      radius: numberField,
    })
    .strict(),
  update_sketch_fillet_radius: z
    .object({ fillet_id: stringField, radius: numberField })
    .strict(),
  delete_sketch_fillet: z.object({ fillet_id: stringField }).strict(),
  delete_sketch_selection: z
    .object({
      entity_ids: stringArray,
      point_ids: stringArray,
      profile_ids: stringArray,
    })
    .strict(),
  set_sketch_tool: z
    .object({
      tool: z.enum([
        "select",
        "line",
        "rectangle",
        "circle",
        "arc",
        "fillet",
        "project",
        "dimension",
      ]),
    })
    .strict(),
  update_sketch_line: z
    .object({
      line_id: stringField,
      start_x: numberField,
      start_y: numberField,
      end_x: numberField,
      end_y: numberField,
    })
    .strict(),
  update_sketch_point: z
    .object({ point_id: stringField, x: numberField, y: numberField })
    .strict(),
  set_sketch_line_constraint: z
    .object({
      line_id: stringField,
      constraint: z.enum(["none", "horizontal", "vertical"]),
    })
    .strict(),
  clear_sketch_line_constraints: z
    .object({ line_id: stringField })
    .strict(),
  set_sketch_equal_length_constraint: z
    .object({ line_id: stringField, other_line_id: stringField })
    .strict(),
  set_sketch_perpendicular_constraint: z
    .object({ line_id: stringField, other_line_id: stringField })
    .strict(),
  start_mirror_preview: emptyPayload,
  update_mirror_preview_axis: z
    .object({ axis_line_id: stringField })
    .strict(),
  update_mirror_preview_objects: z
    .object({ object_ids: stringArray })
    .strict(),
  commit_mirror_preview: emptyPayload,
  cancel_mirror_preview: emptyPayload,
  set_sketch_tangent_constraint: z
    .object({ line_id: stringField, circle_id: stringField })
    .strict(),
  set_sketch_parallel_constraint: z
    .object({ line_id: stringField, other_line_id: stringField })
    .strict(),
  set_sketch_coincident_constraint: z
    .object({ point_id: stringField, other_point_id: stringField })
    .strict(),
  set_sketch_point_fixed: z
    .object({ point_id: stringField, is_fixed: booleanField })
    .strict(),
  update_sketch_circle: z
    .object({
      circle_id: stringField,
      center_x: numberField,
      center_y: numberField,
      radius: numberField,
    })
    .strict(),
  update_sketch_dimension: z
    .object({ dimension_id: stringField, value: numberField })
    .strict(),
  update_sketch_dimension_label_position: z
    .object({
      dimension_id: stringField,
      label_x: numberField,
      label_y: numberField,
    })
    .strict(),
  select_sketch_profile: z
    .object({ profile_id: stringField, additive: booleanField.optional() })
    .strict(),
  extrude_profile: z
    .object({
      profile_id: stringField.optional(),
      profile_ids: stringArray.optional(),
      open_entity_ids: stringArray.optional(),
      depth: numberField,
      mode: z.enum(["new_body", "join", "cut", "intersect"]).optional(),
      target_body_id: stringField.optional(),
      parameters: z.record(z.string(), z.unknown()).optional(),
    })
    .strict()
    .refine((payload) => payload.profile_id || payload.profile_ids?.length || payload.open_entity_ids?.length, {
      message: "extrude_profile requires profile_id, profile_ids, or open_entity_ids",
    }),
  extrude_face: z
    .object({
      face_id: stringField,
      depth: numberField,
      mode: z.enum(["new_body", "join", "cut", "intersect"]).optional(),
      target_body_id: stringField.optional(),
      parameters: z.record(z.string(), z.unknown()).optional(),
    })
    .strict(),
  update_extrude_mode: z
    .object({
      feature_id: stringField,
      mode: z.enum(["new_body", "join", "cut", "intersect"]),
    })
    .strict(),
  update_extrude_parameters: z
    .object({
      feature_id: stringField,
      parameters: z.record(z.string(), z.unknown()),
    })
    .strict(),
  update_extrude_target_body: z
    .object({ feature_id: stringField, target_body_id: stringField.optional() })
    .strict(),
  update_extrude_profiles: z
    .object({ feature_id: stringField, profile_ids: stringArray })
    .strict(),
  loft_profiles: z
    .object({ profile_ids: stringArray, ruled: booleanField.optional() })
    .strict()
    .refine((payload) => payload.profile_ids.length >= 2, {
      message: "loft_profiles requires at least two profile_ids",
    }),
  update_loft_profiles: z
    .object({ feature_id: stringField, profile_ids: stringArray })
    .strict()
    .refine((payload) => payload.profile_ids.length >= 2, {
      message: "update_loft_profiles requires at least two profile_ids",
    }),
  update_loft_ruled: z
    .object({ feature_id: stringField, ruled: booleanField })
    .strict(),
  revolve_profile: z
    .object({
      profile_id: stringField,
      axis_entity_id: stringField,
      angle_degrees: numberField.optional(),
    })
    .strict(),
  update_revolve_profile: z
    .object({ feature_id: stringField, profile_id: stringField })
    .strict(),
  update_revolve_axis: z
    .object({ feature_id: stringField, axis_entity_id: stringField })
    .strict(),
  update_revolve_angle: z
    .object({ feature_id: stringField, angle_degrees: numberField })
    .strict(),
  sweep_profile: z
    .object({ profile_id: stringField, path_entity_id: stringField })
    .strict(),
  update_sweep_profile: z
    .object({ feature_id: stringField, profile_id: stringField })
    .strict(),
  update_sweep_path: z
    .object({ feature_id: stringField, path_entity_id: stringField })
    .strict(),
  select_sketch_entity: z
    .object({ entity_id: stringField, additive: booleanField })
    .strict(),
  select_sketch_point: z
    .object({ point_id: stringField, additive: booleanField })
    .strict(),
  select_sketch_dimension: z.object({ dimension_id: stringField }).strict(),
  finish_sketch: emptyPayload,
  reenter_sketch: z.object({ feature_id: stringField }).strict(),
  clear_selection: emptyPayload,
} satisfies Record<string, z.ZodTypeAny>;

export type AiCommandType = keyof typeof commandPayloadSchemas;
