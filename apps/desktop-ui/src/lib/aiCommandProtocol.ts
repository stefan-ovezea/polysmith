import { z } from "zod";
import type { CoreCommand, DocumentState, ViewportState } from "@/types";

export type AiExecutableCommand = Exclude<CoreCommand, { type: "shutdown" }>;

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

const commandPayloadSchemas = {
  ping: emptyPayload,
  create_document: emptyPayload,
  get_document_state: emptyPayload,
  get_session_state: emptyPayload,
  get_viewport_state: emptyPayload,
  export_document: z.object({ file_path: stringField }).strict(),
  export_document_stl: z.object({ file_path: stringField }).strict(),
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

type AiCommandType = keyof typeof commandPayloadSchemas;

const modelCommandSchema = z
  .object({
    type: z.string(),
    payload: z.record(z.string(), z.unknown()),
  })
  .strict();

const modelEnvelopeSchema = z
  .object({
    message: z.string(),
    commands: z.array(modelCommandSchema),
    continue: z.boolean(),
  })
  .strict();

export interface AiCommandEnvelope {
  message: string;
  commands: AiExecutableCommand[];
  continue: boolean;
}

function issueCommandId(index: number) {
  if (typeof crypto !== "undefined" && "randomUUID" in crypto) {
    return crypto.randomUUID();
  }
  return `ai-command-${Date.now()}-${index}`;
}

export function parseAiCommandEnvelope(raw: string): AiCommandEnvelope {
  const trimmed = raw.trim();
  let parsed: unknown;
  try {
    parsed = JSON.parse(trimmed);
  } catch {
    throw new Error("AI response was not valid JSON.");
  }

  const envelope = modelEnvelopeSchema.parse(parsed);
  const commands = envelope.commands.map((command, index) => {
    if ("id" in command) {
      throw new Error("AI commands must not include ids.");
    }
    if (!isAiCommandType(command.type)) {
      throw new Error(`Unknown or unsupported AI command: ${command.type}`);
    }
    const payload = commandPayloadSchemas[command.type].parse(command.payload);
    return {
      id: issueCommandId(index),
      type: command.type,
      payload,
    } as AiExecutableCommand;
  });

  return {
    message: envelope.message,
    commands,
    continue: envelope.continue,
  };
}

export function commandPreviewLabel(command: CoreCommand) {
  return `${command.type} ${JSON.stringify(command.payload)}`;
}

const activeSketchRequiredCommands = new Set<string>([
  "add_sketch_line",
  "set_sketch_line_construction",
  "set_sketch_midpoint_anchor",
  "add_sketch_angle_dimension",
  "add_sketch_distance_dimension",
  "update_sketch_dimension_label_position",
  "set_sketch_point_line_anchor",
  "add_sketch_rectangle",
  "add_sketch_circle",
  "add_sketch_arc",
  "add_sketch_fillet",
  "update_sketch_fillet_radius",
  "delete_sketch_fillet",
  "delete_sketch_selection",
  "set_sketch_tool",
  "update_sketch_line",
  "update_sketch_point",
  "set_sketch_line_constraint",
  "clear_sketch_line_constraints",
  "resolve_draft_snap",
  "set_sketch_equal_length_constraint",
  "set_sketch_perpendicular_constraint",
  "start_mirror_preview",
  "update_mirror_preview_axis",
  "update_mirror_preview_objects",
  "commit_mirror_preview",
  "cancel_mirror_preview",
  "set_sketch_tangent_constraint",
  "set_sketch_parallel_constraint",
  "set_sketch_coincident_constraint",
  "set_sketch_point_fixed",
  "update_sketch_circle",
  "update_sketch_dimension",
  "update_sketch_dimension_label_position",
  "select_sketch_entity",
  "select_sketch_point",
  "select_sketch_dimension",
  "finish_sketch",
  "project_face_into_sketch",
  "project_profile_into_sketch",
  "project_edge_into_sketch",
  "project_vertex_into_sketch",
]);

const sketchCreationCommands = new Set<string>([
  "add_sketch_line",
  "add_sketch_rectangle",
  "add_sketch_circle",
  "add_sketch_arc",
]);

export function validateAiCommandBatchForState(
  commands: readonly AiExecutableCommand[],
  document: DocumentState | null,
  viewport: ViewportState | null,
) {
  let hasActiveSketch = Boolean(document?.active_sketch_feature_id);
  const knownProfileIds = new Set<string>();
  const knownBodyIds = new Set<string>();
  const knownPlanarFaceIds = new Set<string>();

  for (const feature of document?.feature_history ?? []) {
    for (const profile of feature.sketch_parameters?.profiles ?? []) {
      knownProfileIds.add(profile.profile_id);
    }
  }
  for (const profile of viewport?.sketch_profiles ?? []) {
    knownProfileIds.add(profile.profile_id);
  }
  for (const body of viewport?.bodies ?? []) {
    knownBodyIds.add(body.id);
  }
  for (const face of viewport?.solid_faces ?? []) {
    if (face.sketchability === "planar") {
      knownPlanarFaceIds.add(face.face_id);
    }
  }

  for (const command of commands) {
    if (activeSketchRequiredCommands.has(command.type) && !hasActiveSketch) {
      throw new Error(
        `${command.type} requires an active sketch. Start a sketch on a plane or face first.`,
      );
    }

    if (
      command.type === "start_sketch_on_plane" ||
      command.type === "start_sketch_on_face" ||
      command.type === "reenter_sketch"
    ) {
      hasActiveSketch = true;
    }

    if (command.type === "finish_sketch") {
      hasActiveSketch = false;
    }

    if (command.type === "extrude_profile") {
      const profileIds =
        command.payload.profile_ids ?? (command.payload.profile_id ? [command.payload.profile_id] : []);
      for (const profileId of profileIds) {
        if (!knownProfileIds.has(profileId)) {
          throw new Error(
            `extrude_profile references unknown profile "${profileId}". Draw geometry first, refresh state, then use the real profile_id.`,
          );
        }
      }
      if (
        command.payload.target_body_id &&
        !knownBodyIds.has(command.payload.target_body_id)
      ) {
        throw new Error(
          `extrude_profile references unknown target body "${command.payload.target_body_id}". Use a body id from viewport state.`,
        );
      }
    }

    if (command.type === "create_move") {
      if (!knownBodyIds.has(command.payload.target_body_id)) {
        throw new Error(
          `create_move references unknown body "${command.payload.target_body_id}". Use a body id from viewport state.`,
        );
      }
    }

    if (command.type === "create_body_copy") {
      if (!knownBodyIds.has(command.payload.source_body_id)) {
        throw new Error(
          `create_body_copy references unknown body "${command.payload.source_body_id}". Use a body id from viewport state.`,
        );
      }
    }

    if (
      command.type === "loft_profiles" ||
      command.type === "update_loft_profiles"
    ) {
      for (const profileId of command.payload.profile_ids) {
        if (!knownProfileIds.has(profileId)) {
          throw new Error(
            `${command.type} references unknown profile "${profileId}". Draw geometry first, refresh state, then use the real profile_id.`,
          );
        }
      }
    }

    if (
      command.type === "revolve_profile" ||
      command.type === "update_revolve_profile" ||
      command.type === "sweep_profile" ||
      command.type === "update_sweep_profile"
    ) {
      if (!knownProfileIds.has(command.payload.profile_id)) {
        throw new Error(
          `${command.type} references unknown profile "${command.payload.profile_id}". Draw geometry first, refresh state, then use the real profile_id.`,
        );
      }
    }

    if (
      command.type === "revolve_profile" ||
      command.type === "update_revolve_axis"
    ) {
      const knownSketchLineIds = collectKnownSketchLineIds(document);
      if (!knownSketchLineIds.has(command.payload.axis_entity_id)) {
        throw new Error(
          `${command.type} references unknown axis line "${command.payload.axis_entity_id}". Use a sketch line id from current state.`,
        );
      }
    }

    if (command.type === "project_profile_into_sketch") {
      if (!knownProfileIds.has(command.payload.profile_id)) {
        throw new Error(
          `project_profile_into_sketch references unknown profile "${command.payload.profile_id}". Use a profile id from current state.`,
        );
      }
    }

    if (command.type === "extrude_face") {
      if (!knownPlanarFaceIds.has(command.payload.face_id)) {
        throw new Error(
          `extrude_face references unknown or non-planar face "${command.payload.face_id}". Use a planar face id from viewport state.`,
        );
      }
      if (
        command.payload.target_body_id &&
        !knownBodyIds.has(command.payload.target_body_id)
      ) {
        throw new Error(
          `extrude_face references unknown target body "${command.payload.target_body_id}". Use a body id from viewport state.`,
        );
      }
    }
  }

  const createdSketchGeometry = commands.some((command) =>
    sketchCreationCommands.has(command.type),
  );
  const createdNonConstructionSketchGeometry = commands.some((command) => {
    if (!sketchCreationCommands.has(command.type)) {
      return false;
    }
    const payload = command.payload as { is_construction?: boolean };
    return payload.is_construction === false;
  });
  if (createdSketchGeometry && !createdNonConstructionSketchGeometry) {
    throw new Error(
      "Construction sketch geometry is ignored by profile detection. Use is_construction: false for geometry the user wants to extrude.",
    );
  }
}

export interface PreparedAiCommandBatch {
  commands: AiExecutableCommand[];
  continue: boolean;
  notices: string[];
}

export function prepareAiCommandBatchForState(
  commands: readonly AiExecutableCommand[],
  shouldContinue: boolean,
  document: DocumentState | null,
  viewport: ViewportState | null,
): PreparedAiCommandBatch {
  try {
    validateAiCommandBatchForState(commands, document, viewport);
    return {
      commands: [...commands],
      continue: shouldContinue,
      notices: [],
    };
  } catch (error) {
    let fallbackCommands = commands;
    const fallbackNotices: string[] = [];
    const activeSketchRepair = buildMissingActiveSketchRepair(commands, document);
    if (activeSketchRepair) {
      fallbackCommands = activeSketchRepair.commands;
      fallbackNotices.push(activeSketchRepair.notice);
      try {
        validateAiCommandBatchForState(fallbackCommands, document, viewport);
        return {
          commands: [...fallbackCommands],
          continue: shouldContinue,
          notices: fallbackNotices,
        };
      } catch (repairError) {
        error = repairError;
      }
    }

    const unknownProfileExtrudeIndex = findUnknownProfileExtrudeIndex(
      fallbackCommands,
      document,
      viewport,
    );
    if (unknownProfileExtrudeIndex > 0) {
      const runnableCommands = fallbackCommands.slice(0, unknownProfileExtrudeIndex);
      const createdNonConstructionSketchGeometry = runnableCommands.some(
        (command) => {
          if (!sketchCreationCommands.has(command.type)) {
            return false;
          }
          const payload = command.payload as { is_construction?: boolean };
          return payload.is_construction === false;
        },
      );
      if (createdNonConstructionSketchGeometry) {
        validateAiCommandBatchForState(runnableCommands, document, viewport);
        return {
          commands: [...runnableCommands],
          continue: true,
          notices: [
            ...fallbackNotices,
            "Deferred commands that need generated profile IDs. Run this batch first, then the assistant will continue with refreshed references.",
          ],
        };
      }
    }
    throw error;
  }
}

function buildMissingActiveSketchRepair(
  commands: readonly AiExecutableCommand[],
  document: DocumentState | null,
) {
  let hasActiveSketch = Boolean(document?.active_sketch_feature_id);
  for (const [index, command] of commands.entries()) {
    if (
      command.type === "start_sketch_on_plane" ||
      command.type === "start_sketch_on_face" ||
      command.type === "reenter_sketch"
    ) {
      hasActiveSketch = true;
    }

    if (activeSketchRequiredCommands.has(command.type) && !hasActiveSketch) {
      if (!sketchCreationCommands.has(command.type)) {
        return null;
      }

      const repairedCommands = [...commands];
      const insertedCommands: AiExecutableCommand[] = [];
      const createsDocumentBeforeSketch =
        Boolean(document) ||
        repairedCommands
          .slice(0, index)
          .some((candidate) => candidate.type === "create_document");

      if (!createsDocumentBeforeSketch) {
        insertedCommands.push({
          id: issueCommandId(-2),
          type: "create_document",
          payload: {},
        } as AiExecutableCommand);
      }

      insertedCommands.push({
        id: issueCommandId(-1),
        type: "start_sketch_on_plane",
        payload: { reference_id: "ref-plane-xy" },
      } as AiExecutableCommand);

      repairedCommands.splice(index, 0, ...insertedCommands);
      return {
        commands: repairedCommands,
        notice:
          "Added a missing XY sketch start before sketch geometry because no sketch was active.",
      };
    }

    if (command.type === "finish_sketch") {
      hasActiveSketch = false;
    }
    if (
      command.type === "sweep_profile" ||
      command.type === "update_sweep_path"
    ) {
      const knownSketchPathEntityIds = collectKnownSketchPathEntityIds(document);
      if (!knownSketchPathEntityIds.has(command.payload.path_entity_id)) {
        throw new Error(
          `${command.type} references unknown path entity "${command.payload.path_entity_id}". Use a sketch line or arc id from current state.`,
        );
      }
      hasActiveSketch = false;
    }
  }

  return null;
}

function findUnknownProfileExtrudeIndex(
  commands: readonly AiExecutableCommand[],
  document: DocumentState | null,
  viewport: ViewportState | null,
) {
  const knownProfileIds = collectKnownProfileIds(document, viewport);
  return commands.findIndex((command) => {
    if (
      command.type !== "extrude_profile" &&
      command.type !== "loft_profiles" &&
      command.type !== "revolve_profile" &&
      command.type !== "sweep_profile"
    ) {
      return false;
    }
    const profileIds =
      command.type === "extrude_profile"
        ? command.payload.profile_ids ??
          (command.payload.profile_id ? [command.payload.profile_id] : [])
        : command.type === "loft_profiles"
          ? command.payload.profile_ids
          : [command.payload.profile_id];
    return profileIds.some((profileId) => !knownProfileIds.has(profileId));
  });
}

function collectKnownProfileIds(
  document: DocumentState | null,
  viewport: ViewportState | null,
) {
  const knownProfileIds = new Set<string>();
  for (const feature of document?.feature_history ?? []) {
    for (const profile of feature.sketch_parameters?.profiles ?? []) {
      knownProfileIds.add(profile.profile_id);
    }
  }
  for (const profile of viewport?.sketch_profiles ?? []) {
    knownProfileIds.add(profile.profile_id);
  }
  return knownProfileIds;
}

function collectKnownSketchLineIds(document: DocumentState | null) {
  const knownLineIds = new Set<string>();
  for (const feature of document?.feature_history ?? []) {
    for (const line of feature.sketch_parameters?.lines ?? []) {
      knownLineIds.add(line.line_id);
    }
  }
  return knownLineIds;
}

function collectKnownSketchPathEntityIds(document: DocumentState | null) {
  const knownEntityIds = collectKnownSketchLineIds(document);
  for (const feature of document?.feature_history ?? []) {
    for (const arc of feature.sketch_parameters?.arcs ?? []) {
      knownEntityIds.add(arc.arc_id);
    }
  }
  return knownEntityIds;
}

export function buildAiWorkingReferences(
  document: DocumentState | null,
  viewport: ViewportState | null,
) {
  if (!document) {
    return ["No active document."];
  }
  const activeSketch = document.feature_history.find(
    (feature) => feature.feature_id === document.active_sketch_feature_id,
  );
  const activeSketchProfiles =
    activeSketch?.sketch_parameters?.profiles.map((profile) => profile.profile_id) ??
    [];
  const viewportProfileIds =
    viewport?.sketch_profiles.map((profile) => profile.profile_id) ?? [];
  const bodyIds = viewport?.bodies.map((body) => body.id) ?? [];
  const lines =
    activeSketch?.sketch_parameters?.lines.map((line) => line.line_id) ?? [];
  const circles =
    activeSketch?.sketch_parameters?.circles.map((circle) => circle.circle_id) ??
    [];

  return [
    `Document: ${document.document_id}`,
    `Active sketch: ${document.active_sketch_feature_id ?? "none"}`,
    `Active sketch plane: ${document.active_sketch_plane_id ?? "none"}`,
    `Sketch profiles: ${[...new Set([...activeSketchProfiles, ...viewportProfileIds])].join(", ") || "none"}`,
    `Bodies: ${bodyIds.join(", ") || "none"}`,
    `Selected faces: ${document.selected_face_id ?? "none"}`,
    `Selected edges: ${document.selected_edge_ids.join(", ") || "none"}`,
    `Sketch lines: ${lines.join(", ") || "none"}`,
    `Sketch circles: ${circles.join(", ") || "none"}`,
  ];
}

export function buildCadStateSummary(
  document: DocumentState | null,
  viewport: ViewportState | null,
) {
  if (!document) {
    return [
      "No active document is loaded.",
      "If the user asks to draw or model, start with create_document.",
    ].join("\n");
  }

  const activeSketch = document.feature_history.find(
    (feature) => feature.feature_id === document.active_sketch_feature_id,
  );
  const features = document.feature_history.map((feature) => ({
    id: feature.feature_id,
    kind: feature.kind,
    name: feature.name,
    status: feature.status,
  }));
  const sketchSummary = activeSketch?.sketch_parameters
    ? {
        feature_id: activeSketch.feature_id,
        plane_id: activeSketch.sketch_parameters.plane_id,
        lines: activeSketch.sketch_parameters.lines.map((line) => ({
          id: line.line_id,
          start_point_id: line.start_point_id,
          end_point_id: line.end_point_id,
          start: [line.start_x, line.start_y],
          end: [line.end_x, line.end_y],
          is_construction: line.is_construction,
        })),
        circles: activeSketch.sketch_parameters.circles.map((circle) => ({
          id: circle.circle_id,
          center: [circle.center_x, circle.center_y],
          radius: circle.radius,
          is_construction: circle.is_construction,
        })),
        arcs: activeSketch.sketch_parameters.arcs.map((arc) => ({
          id: arc.arc_id,
          start_point_id: arc.start_point_id,
          end_point_id: arc.end_point_id,
          center: [arc.center_x, arc.center_y],
          radius: arc.radius,
          is_construction: arc.is_construction,
        })),
        points: activeSketch.sketch_parameters.points.map((point) => ({
          id: point.point_id,
          kind: point.kind,
          position: [point.x, point.y],
          is_fixed: point.is_fixed,
        })),
        profiles: activeSketch.sketch_parameters.profiles.map((profile) => ({
          id: profile.profile_id,
          kind: profile.kind,
          point_count: profile.points.length,
          source_circle_id: profile.source_circle_id,
        })),
        dimensions: activeSketch.sketch_parameters.dimensions.map(
          (dimension) => ({
            id: dimension.dimension_id,
            kind: dimension.kind,
            entity_id: dimension.entity_id,
            value: dimension.value,
          }),
        ),
      }
    : null;

  const viewportSummary = viewport
    ? {
        reference_planes: viewport.reference_planes.map((plane) => ({
          id: plane.reference_id,
          label: plane.label,
          orientation: plane.orientation,
        })),
        bodies: viewport.bodies,
        solid_faces: viewport.solid_faces.slice(0, 24).map((face) => ({
          id: face.face_id,
          owner_id: face.owner_id,
          label: face.label,
          sketchability: face.sketchability,
          normal: face.normal,
          plane_frame: face.plane_frame,
        })),
        edges: viewport.edges.slice(0, 40).map((edge) => ({
          id: edge.id,
          owner_body_id: edge.owner_body_id,
          kind: edge.kind,
          length: edge.length,
        })),
        vertices: viewport.vertices.slice(0, 40).map((vertex) => ({
          id: vertex.id,
          owner_body_id: vertex.owner_body_id,
          position: vertex.position,
        })),
        sketch_profiles: viewport.sketch_profiles.map((profile) => ({
          id: profile.profile_id,
          plane_id: profile.plane_id,
          kind: profile.profile_kind,
          point_count: profile.profile_points.length,
          radius: profile.radius,
        })),
      }
    : null;

  return JSON.stringify(
    {
      document: {
        id: document.document_id,
        units: document.units,
        revision: document.revision,
        active_sketch_feature_id: document.active_sketch_feature_id,
        active_sketch_plane_id: document.active_sketch_plane_id,
        active_sketch_face_id: document.active_sketch_face_id,
        selected_feature_id: document.selected_feature_id,
        selected_face_id: document.selected_face_id,
        selected_edge_ids: document.selected_edge_ids,
        selected_vertex_ids: document.selected_vertex_ids,
        selected_sketch_profile_ids: document.selected_sketch_profile_ids,
        features,
        active_sketch: sketchSummary,
      },
      viewport: viewportSummary,
    },
    null,
    2,
  );
}

function isAiCommandType(type: string): type is AiCommandType {
  return type in commandPayloadSchemas;
}
