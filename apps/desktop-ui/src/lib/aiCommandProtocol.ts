import { z } from "zod";
import type { CoreCommand, DocumentState, ViewportState } from "@/types";
import { commandPayloadSchemas, type AiCommandType } from "./aiCommandPayloadSchemas";

export type AiExecutableCommand = Exclude<CoreCommand, { type: "shutdown" }>;

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

// Small local models sometimes wrap the JSON envelope in markdown fences
// (```json ... ```) despite format:"json". Strip a single outer fence before
// parsing so those responses are still accepted.
function stripJsonFences(raw: string) {
  const trimmed = raw.trim();
  const fenceStart = /^```(?:json)?\s*\n/.exec(trimmed);
  if (!fenceStart) {
    return trimmed;
  }
  let content = trimmed.slice(fenceStart[0].length);
  const fenceEnd = /\n```\s*$/.exec(content);
  if (fenceEnd) {
    content = content.slice(0, fenceEnd.index);
  }
  return content.trim();
}

export function parseAiCommandEnvelope(raw: string): AiCommandEnvelope {
  let parsed: unknown;
  try {
    parsed = JSON.parse(stripJsonFences(raw));
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

// Formats a validation or core failure into a compact, model-readable string.
// A raw ZodError.toString() is far too noisy for a small local model to learn
// from; map issues to "<path>: <message>" lines instead.
export function formatAiCommandError(error: unknown): string {
  if (error instanceof z.ZodError) {
    return error.issues
      .map((issue) => `${issue.path.join(".")}: ${issue.message}`)
      .join("; ");
  }
  return String(error);
}

const activeSketchRequiredCommands = new Set<string>([
  "add_sketch_line",
  "add_sketch_rectangle",
  "add_sketch_circle",
  "add_sketch_polygon",
  "add_sketch_ellipse",
  "add_sketch_slot",
  "update_sketch_slot",
  "add_sketch_arc",
  "add_sketch_fillet",
  "update_sketch_fillet_radius",
  "delete_sketch_fillet",
  "add_sketch_chamfer",
  "update_sketch_chamfer",
  "delete_sketch_chamfer",
  "add_sketch_text",
  "update_sketch_text",
  "delete_sketch_text",
  "set_sketch_line_construction",
  "set_sketch_midpoint_anchor",
  "set_sketch_point_line_anchor",
  "add_sketch_angle_dimension",
  "add_sketch_distance_dimension",
  "add_sketch_line_length_dimension",
  "add_sketch_line_angle_dimension",
  "add_sketch_circle_radius_dimension",
  "add_sketch_arc_length_dimension",
  "add_sketch_arc_radius_dimension",
  "add_sketch_arc_angle_dimension",
  "add_sketch_polygon_radius_dimension",
  "add_sketch_vertex_distance_dimension",
  "update_sketch_dimension_label_position",
  "update_sketch_dimension_display",
  "toggle_sketch_dimension_driven",
  "delete_sketch_dimension",
  "delete_sketch_selection",
  "set_sketch_tool",
  "update_sketch_line",
  "update_sketch_vertex",
  "update_sketch_circle",
  "move_sketch_entities",
  "trim_sketch_entity",
  "extend_sketch_entity",
  "offset_sketch_entity",
  "transform_sketch_entities",
  "create_linear_array",
  "create_circular_array",
  "trim_preview",
  "set_sketch_line_constraint",
  "clear_sketch_line_constraints",
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
  "delete_sketch_coincident_constraint",
  "set_sketch_symmetric_constraint",
  "set_sketch_midpoint_constraint",
  "set_sketch_collinear_constraint",
  "set_sketch_tangent_pair_constraint",
  "set_sketch_vertex_fixed",
  "update_sketch_dimension",
  "select_sketch_entity",
  "select_sketch_vertex",
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
  "add_sketch_polygon",
  "add_sketch_ellipse",
  "add_sketch_slot",
  "update_sketch_slot",
  "add_sketch_arc",
]);

interface AiCommandValidationContext {
  hasActiveSketch: boolean;
  knownProfileIds: Set<string>;
  knownBodyIds: Set<string>;
  knownPlanarFaceIds: Set<string>;
  faceOwners: Map<string, string>;
}

function validateAiCommandBatchForState(
  commands: readonly AiExecutableCommand[],
  document: DocumentState | null,
  viewport: ViewportState | null,
) {
  const context = buildAiCommandValidationContext(document, viewport);
  for (const command of commands) {
    validateCommandForState(command, context, document);
  }

  if (createsOnlyConstructionSketchGeometry(commands)) {
    throw new Error(
      "Construction sketch geometry is ignored by profile detection. Use is_construction: false for geometry the user wants to extrude.",
    );
  }
}

function buildAiCommandValidationContext(
  document: DocumentState | null,
  viewport: ViewportState | null,
): AiCommandValidationContext {
  const knownProfileIds = collectKnownProfileIds(document, viewport);
  const knownBodyIds = new Set((viewport?.bodies ?? []).map((body) => body.id));
  const knownPlanarFaceIds = new Set(
    (viewport?.solid_faces ?? [])
      .filter((face) => face.sketchability === "planar")
      .map((face) => face.face_id),
  );
  const faceOwners = new Map(
    (viewport?.solid_faces ?? []).map((face) => [face.face_id, face.owner_id]),
  );

  return {
    hasActiveSketch: Boolean(document?.active_sketch_feature_id),
    knownProfileIds,
    knownBodyIds,
    knownPlanarFaceIds,
    faceOwners,
  };
}

function validateCommandForState(
  command: AiExecutableCommand,
  context: AiCommandValidationContext,
  document: DocumentState | null,
) {
  validateActiveSketchAvailability(command, context);
  updateActiveSketchLifecycle(command, context);
  validateProfileReferences(command, context);
  validateBodyReferences(command, context);
  validateSketchLineReferences(command, document);
  validateProjectProfileReference(command, context);
  validateExtrudeFaceReference(command, context);
}

function validateActiveSketchAvailability(
  command: AiExecutableCommand,
  context: AiCommandValidationContext,
) {
  if (activeSketchRequiredCommands.has(command.type) && !context.hasActiveSketch) {
    throw new Error(
      `${command.type} requires an active sketch. Start a sketch on a plane or face first.`,
    );
  }
}

function updateActiveSketchLifecycle(
  command: AiExecutableCommand,
  context: AiCommandValidationContext,
) {
  if (startsOrReentersSketch(command)) {
    context.hasActiveSketch = true;
  }
  if (command.type === "finish_sketch") {
    context.hasActiveSketch = false;
  }
}

function validateProfileReferences(
  command: AiExecutableCommand,
  context: AiCommandValidationContext,
) {
  if (command.type === "extrude_profile") {
    assertKnownProfiles({
      commandType: command.type,
      profileIds:
        command.payload.profile_ids ??
        (command.payload.profile_id ? [command.payload.profile_id] : []),
      knownProfileIds: context.knownProfileIds,
    });
    return;
  }

  if (
    command.type === "loft_profiles" ||
    command.type === "update_loft_profiles"
  ) {
    assertKnownProfiles({
      commandType: command.type,
      profileIds: command.payload.profile_ids,
      knownProfileIds: context.knownProfileIds,
    });
    return;
  }

  if (usesSingleProfileId(command)) {
    assertKnownProfiles({
      commandType: command.type,
      profileIds: [command.payload.profile_id],
      knownProfileIds: context.knownProfileIds,
    });
  }
}

function validateBodyReferences(
  command: AiExecutableCommand,
  context: AiCommandValidationContext,
) {
  if (command.type === "extrude_profile" || command.type === "extrude_face") {
    assertKnownOptionalTargetBody({
      commandType: command.type,
      bodyId: command.payload.target_body_id,
      knownBodyIds: context.knownBodyIds,
    });
    return;
  }
  if (command.type === "create_move") {
    assertKnownBody(command.type, command.payload.target_body_id, context);
    return;
  }
  if (
    command.type === "export_body_stl" ||
    command.type === "export_body_step"
  ) {
    assertKnownBody(command.type, command.payload.body_id, context);
    return;
  }
  if (command.type === "create_body_copy") {
    assertKnownBody(command.type, command.payload.source_body_id, context);
  }
}

function validateSketchLineReferences(
  command: AiExecutableCommand,
  document: DocumentState | null,
) {
  if (command.type !== "revolve_profile" && command.type !== "update_revolve_axis") {
    return;
  }
  const knownSketchLineIds = collectKnownSketchLineIds(document);
  if (!knownSketchLineIds.has(command.payload.axis_entity_id)) {
    throw new Error(
      `${command.type} references unknown axis line "${command.payload.axis_entity_id}". Use a sketch line id from current state.`,
    );
  }
}

function validateProjectProfileReference(
  command: AiExecutableCommand,
  context: AiCommandValidationContext,
) {
  if (
    command.type === "project_profile_into_sketch" &&
    !context.knownProfileIds.has(command.payload.profile_id)
  ) {
    throw new Error(
      `project_profile_into_sketch references unknown profile "${command.payload.profile_id}". Use a profile id from current state.`,
    );
  }
}

function validateExtrudeFaceReference(
  command: AiExecutableCommand,
  context: AiCommandValidationContext,
) {
  if (command.type !== "extrude_face") {
    return;
  }
  if (!context.knownPlanarFaceIds.has(command.payload.face_id)) {
    throw new Error(
      `extrude_face references unknown or non-planar face "${command.payload.face_id}". Use a planar face id from viewport state.`,
    );
  }
  // Extruding a face of an existing body with mode new_body creates a second,
  // coincident body (two overlapping solids — visually a seam). Join into the
  // owner body instead.
  const ownerId = context.faceOwners.get(command.payload.face_id);
  if (command.payload.mode === "new_body" && ownerId && context.knownBodyIds.has(ownerId)) {
    throw new Error(
      `extrude_face with mode "new_body" on a face of existing body "${ownerId}" would create a second overlapping body. Use mode "join" with target_body_id "${ownerId}".`,
    );
  }
}

function assertKnownProfiles({
  commandType,
  profileIds,
  knownProfileIds,
}: {
  commandType: string;
  profileIds: readonly string[];
  knownProfileIds: ReadonlySet<string>;
}) {
  for (const profileId of profileIds) {
    if (!knownProfileIds.has(profileId)) {
      throw new Error(
        `${commandType} references unknown profile "${profileId}". Draw geometry first, refresh state, then use the real profile_id.`,
      );
    }
  }
}

function assertKnownBody(
  commandType: string,
  bodyId: string,
  context: AiCommandValidationContext,
) {
  if (!context.knownBodyIds.has(bodyId)) {
    throw new Error(
      `${commandType} references unknown body "${bodyId}". Use a body id from viewport state.`,
    );
  }
}

function assertKnownOptionalTargetBody({
  commandType,
  bodyId,
  knownBodyIds,
}: {
  commandType: string;
  bodyId: string | null;
  knownBodyIds: ReadonlySet<string>;
}) {
  if (bodyId && !knownBodyIds.has(bodyId)) {
    throw new Error(
      `${commandType} references unknown target body "${bodyId}". Use a body id from viewport state.`,
    );
  }
}

function startsOrReentersSketch(command: AiExecutableCommand) {
  return (
    command.type === "start_sketch_on_plane" ||
    command.type === "start_sketch_on_face" ||
    command.type === "reenter_sketch"
  );
}

function usesSingleProfileId(
  command: AiExecutableCommand,
): command is Extract<
  AiExecutableCommand,
  {
    type:
      | "revolve_profile"
      | "update_revolve_profile"
      | "sweep_profile"
      | "update_sweep_profile";
  }
> {
  return (
    command.type === "revolve_profile" ||
    command.type === "update_revolve_profile" ||
    command.type === "sweep_profile" ||
    command.type === "update_sweep_profile"
  );
}

function createsOnlyConstructionSketchGeometry(
  commands: readonly AiExecutableCommand[],
) {
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
  return createdSketchGeometry && !createdNonConstructionSketchGeometry;
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
      "If the user asks to draw or model, begin the batch with create_document and include the sketch start and the requested geometry in the same batch.",
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
          start_vertex_id: line.start_vertex_id,
          end_vertex_id: line.end_vertex_id,
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
          start_vertex_id: arc.start_vertex_id,
          end_vertex_id: arc.end_vertex_id,
          center: [arc.center_x, arc.center_y],
          radius: arc.radius,
          is_construction: arc.is_construction,
        })),
        vertices: activeSketch.sketch_parameters.vertices.map((vtx) => ({
          id: vtx.vertex_id,
          kind: vtx.kind,
          position: [vtx.x, vtx.y],
          is_fixed: vtx.is_fixed,
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
