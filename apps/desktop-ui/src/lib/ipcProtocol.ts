import type {
  SketchTool,
  CoreCommand,
  CoreMessage,
  DocumentState,
  DocumentExportResult,
  ErrorEvent,
  ExtrudeAdvancedParameters,
  ExtrudeFeatureParameters,
  ExtrudeMode,
  FastenerFeatureParameters,
  HelixFeatureParameters,
  HoleFeatureParameters,
  MoveFeatureParameters,
  ThreadFeatureParameters,
  ViewportState,
} from "@/types";

import { coreMessageSchema } from "./schemas/ipcSchema";

export function parseCoreMessage(input: unknown): CoreMessage {
  return coreMessageSchema.parse(input) as CoreMessage;
}

export function makePingCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "ping",
    payload: {},
  };
}

export function makeCreateDocumentCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "create_document",
    payload: {},
  };
}

export function makeGetDocumentStateCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "get_document_state",
    payload: {},
  };
}

export function makeGetSessionStateCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "get_session_state",
    payload: {},
  };
}

export function makeGetViewportStateCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "get_viewport_state",
    payload: {},
  };
}

export function makeExportDocumentCommand(filePath: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "export_document",
    payload: {
      file_path: filePath,
    },
  };
}

export function makeExportDocumentStlCommand(filePath: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "export_document_stl",
    payload: {
      file_path: filePath,
    },
  };
}

export function makeExportBodyStlCommand(
  filePath: string,
  bodyId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "export_body_stl",
    payload: {
      file_path: filePath,
      body_id: bodyId,
    },
  };
}

export function makeSaveDocumentCommand(filePath: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "save_document",
    payload: {
      file_path: filePath,
    },
  };
}

export function makeLoadDocumentCommand(filePath: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "load_document",
    payload: {
      file_path: filePath,
    },
  };
}

export function makeProjectFaceIntoSketchCommand(faceId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "project_face_into_sketch",
    payload: {
      face_id: faceId,
    },
  };
}

export function makeProjectProfileIntoSketchCommand(
  profileId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "project_profile_into_sketch",
    payload: {
      profile_id: profileId,
    },
  };
}

export function makeProjectEdgeIntoSketchCommand(edgeId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "project_edge_into_sketch",
    payload: {
      edge_id: edgeId,
    },
  };
}

export function makeProjectVertexIntoSketchCommand(
  vertexId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "project_vertex_into_sketch",
    payload: {
      vertex_id: vertexId,
    },
  };
}

export function makeAddBoxFeatureCommand(
  width: number,
  height: number,
  depth: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_box_feature",
    payload: {
      width,
      height,
      depth,
    },
  };
}

export function makeAddCylinderFeatureCommand(
  radius: number,
  height: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_cylinder_feature",
    payload: {
      radius,
      height,
    },
  };
}

export function makeUpdateBoxFeatureCommand(
  featureId: string,
  width: number,
  height: number,
  depth: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_box_feature",
    payload: {
      feature_id: featureId,
      width,
      height,
      depth,
    },
  };
}

export function makeUpdateCylinderFeatureCommand(
  featureId: string,
  radius: number,
  height: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_cylinder_feature",
    payload: {
      feature_id: featureId,
      radius,
      height,
    },
  };
}

export function makeSetFeatureSuppressedCommand(
  featureId: string,
  suppressed: boolean,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_feature_suppressed",
    payload: {
      feature_id: featureId,
      suppressed,
    },
  };
}

export function makeUpdateExtrudeDepthCommand(
  featureId: string,
  depth: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_extrude_depth",
    payload: {
      feature_id: featureId,
      depth,
    },
  };
}

export function makeRenameFeatureCommand(
  featureId: string,
  name: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "rename_feature",
    payload: {
      feature_id: featureId,
      name,
    },
  };
}

export function makeDeleteFeatureCommand(featureId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "delete_feature",
    payload: {
      feature_id: featureId,
    },
  };
}

export function makeUndoCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "undo",
    payload: {},
  };
}

export function makeRedoCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "redo",
    payload: {},
  };
}

export function makeSetTimelineCursorCommand(
  includedActionCount: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_timeline_cursor",
    payload: {
      included_action_count: includedActionCount,
    },
  };
}

export function makeSelectFeatureCommand(featureId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "select_feature",
    payload: {
      feature_id: featureId,
    },
  };
}

export function makeSelectReferenceCommand(referenceId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "select_reference",
    payload: {
      reference_id: referenceId,
    },
  };
}

export function makeSelectFaceCommand(faceId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "select_face",
    payload: {
      face_id: faceId,
    },
  };
}

export function makeSelectEdgeCommand(
  edgeId: string,
  additive: boolean,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "select_edge",
    payload: {
      edge_id: edgeId,
      additive,
    },
  };
}

export function makeSelectVertexCommand(
  vertexId: string,
  additive: boolean,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "select_vertex",
    payload: {
      vertex_id: vertexId,
      additive,
    },
  };
}

export function makeSetBodyColorCommand(
  bodyId: string,
  color: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_body_color",
    payload: {
      body_id: bodyId,
      color,
    },
  };
}

export function makeSetFaceColorCommand(
  faceId: string,
  color: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_face_color",
    payload: {
      face_id: faceId,
      color,
    },
  };
}

export function makeClearBodyColorCommand(bodyId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "clear_body_color",
    payload: {
      body_id: bodyId,
    },
  };
}

export function makeClearFaceColorCommand(faceId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "clear_face_color",
    payload: {
      face_id: faceId,
    },
  };
}

export function makeClearAppearanceOverridesCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "clear_appearance_overrides",
    payload: {},
  };
}

export function makeCreateFilletCommand(
  edgeIds: readonly string[],
  radius: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "create_fillet",
    payload: {
      edge_ids: [...edgeIds],
      radius,
    },
  };
}

export function makeUpdateFilletEdgesCommand(
  featureId: string,
  edgeIds: readonly string[],
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_fillet_edges",
    payload: {
      feature_id: featureId,
      edge_ids: [...edgeIds],
    },
  };
}

export function makeUpdateChamferEdgesCommand(
  featureId: string,
  edgeIds: readonly string[],
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_chamfer_edges",
    payload: {
      feature_id: featureId,
      edge_ids: [...edgeIds],
    },
  };
}

export function makeUpdateFilletRadiusCommand(
  featureId: string,
  radius: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_fillet_radius",
    payload: {
      feature_id: featureId,
      radius,
    },
  };
}

export function makeCreateChamferCommand(
  edgeIds: readonly string[],
  distance: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "create_chamfer",
    payload: {
      edge_ids: [...edgeIds],
      distance,
    },
  };
}

export function makeUpdateChamferDistanceCommand(
  featureId: string,
  distance: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_chamfer_distance",
    payload: {
      feature_id: featureId,
      distance,
    },
  };
}

export function makeConfirmFilletCommand(featureId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "confirm_fillet",
    payload: { feature_id: featureId },
  };
}

export function makeConfirmChamferCommand(featureId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "confirm_chamfer",
    payload: { feature_id: featureId },
  };
}

export function makeCreateShellCommand(
  faceId: string,
  thickness: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "create_shell",
    payload: {
      face_id: faceId,
      thickness,
    },
  };
}

export function makeUpdateShellThicknessCommand(
  featureId: string,
  thickness: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_shell_thickness",
    payload: {
      feature_id: featureId,
      thickness,
    },
  };
}

export function makeConfirmShellCommand(featureId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "confirm_shell",
    payload: { feature_id: featureId },
  };
}

// Create a parametric offset construction plane. The source plane id
// can be one of the three origin planes ("ref-plane-xy/yz/xz"), an
// existing construction plane's feature id, or a planar body face id
// ("<body_id>:face:<index>"). The core resolves the source's frame,
// slides it along the normal by `offset`, and stores the result as a
// new `construction_plane` feature.
export function makeCreateOffsetPlaneCommand(
  sourcePlaneId: string,
  offset: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "create_offset_plane",
    payload: {
      source_plane_id: sourcePlaneId,
      offset,
    },
  };
}

export function makeCreateMidplaneCommand(
  sourcePlaneIds: [string, string],
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "create_midplane",
    payload: {
      source_plane_ids: sourcePlaneIds,
    },
  };
}

export function makeCreateTangentPlaneCommand(
  sourceFaceId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "create_tangent_plane",
    payload: {
      source_face_id: sourceFaceId,
    },
  };
}

export function makeCreateAnglePlaneCommand(
  sourcePlaneId: string,
  sourceAxisId: string,
  angleDegrees: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "create_angle_plane",
    payload: {
      source_plane_id: sourcePlaneId,
      source_axis_id: sourceAxisId,
      angle_degrees: angleDegrees,
    },
  };
}

export function makeCreateConstructionAxisCommand(sourceId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "create_construction_axis",
    payload: {
      source_id: sourceId,
    },
  };
}

export function makeCreateConstructionPointCommand(sourceId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "create_construction_point",
    payload: {
      source_id: sourceId,
    },
  };
}

export function makeCreateHoleCommand(
  faceId: string,
  center: { x: number; y: number; z: number },
  parameters: Partial<HoleFeatureParameters> = {},
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "create_hole",
    payload: {
      ...parameters,
      face_id: faceId,
      center_x: center.x,
      center_y: center.y,
      center_z: center.z,
    },
  };
}

export function makeUpdateHoleParametersCommand(
  featureId: string,
  parameters: Partial<HoleFeatureParameters>,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_hole_parameters",
    payload: {
      feature_id: featureId,
      parameters,
    },
  };
}

export function makeConfirmHoleCommand(featureId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "confirm_hole",
    payload: {
      feature_id: featureId,
    },
  };
}

export function makeCreateHelixCommand(
  axisSourceId: string,
  parameters: Partial<HelixFeatureParameters> = {},
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "create_helix",
    payload: {
      ...parameters,
      axis_source_id: axisSourceId,
    },
  };
}

export function makeUpdateHelixParametersCommand(
  featureId: string,
  parameters: Partial<HelixFeatureParameters>,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_helix_parameters",
    payload: {
      feature_id: featureId,
      parameters,
    },
  };
}

export function makeCreateThreadCommand(
  parameters: Partial<ThreadFeatureParameters>,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "create_thread",
    payload: parameters,
  };
}

export function makeUpdateThreadParametersCommand(
  featureId: string,
  parameters: Partial<ThreadFeatureParameters>,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_thread_parameters",
    payload: {
      feature_id: featureId,
      parameters,
    },
  };
}

export function makeConfirmThreadCommand(featureId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "confirm_thread",
    payload: {
      feature_id: featureId,
    },
  };
}

export function makeCreateFastenerCommand(
  parameters: Partial<FastenerFeatureParameters> = {},
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "create_fastener",
    payload: parameters,
  };
}

export function makeUpdateFastenerParametersCommand(
  featureId: string,
  parameters: Partial<FastenerFeatureParameters>,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_fastener_parameters",
    payload: {
      feature_id: featureId,
      parameters,
    },
  };
}

export function makeCreateMoveCommand(
  targetBodyId: string,
  parameters: Partial<MoveFeatureParameters> = {},
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "create_move",
    payload: {
      ...parameters,
      target_body_id: targetBodyId,
    },
  };
}

export function makeUpdateMoveParametersCommand(
  featureId: string,
  parameters: Partial<MoveFeatureParameters>,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_move_parameters",
    payload: {
      feature_id: featureId,
      parameters,
    },
  };
}

export function makeConfirmMoveCommand(featureId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "confirm_move",
    payload: {
      feature_id: featureId,
    },
  };
}

export function makeCreateBodyCopyCommand(
  sourceBodyId: string,
  copyMode: "linked" | "standalone" = "linked",
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "create_body_copy",
    payload: {
      source_body_id: sourceBodyId,
      copy_mode: copyMode,
    },
  };
}

export function makeUnlinkBodyCopyCommand(featureId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "unlink_body_copy",
    payload: {
      feature_id: featureId,
    },
  };
}

export function makeUpdateOffsetPlaneCommand(
  featureId: string,
  offset: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_offset_plane",
    payload: {
      feature_id: featureId,
      offset,
    },
  };
}

export function makeUpdateAnglePlaneCommand(
  featureId: string,
  angleDegrees: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_angle_plane",
    payload: {
      feature_id: featureId,
      angle_degrees: angleDegrees,
    },
  };
}

export function makeStartSketchOnPlaneCommand(
  referenceId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "start_sketch_on_plane",
    payload: {
      reference_id: referenceId,
    },
  };
}

export function makeStartSketchOnFaceCommand(
  faceId: string,
  planeFrame: {
    origin: { x: number; y: number; z: number };
    x_axis: { x: number; y: number; z: number };
    y_axis: { x: number; y: number; z: number };
    normal: { x: number; y: number; z: number };
  },
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "start_sketch_on_face",
    payload: {
      face_id: faceId,
      plane_frame: planeFrame,
    },
  };
}

export function makeSetSketchToolCommand(tool: SketchTool): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_tool",
    payload: {
      tool,
    },
  };
}

export function makeUpdateSketchLineCommand(
  lineId: string,
  startX: number,
  startY: number,
  endX: number,
  endY: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_sketch_line",
    payload: {
      line_id: lineId,
      start_x: startX,
      start_y: startY,
      end_x: endX,
      end_y: endY,
    },
  };
}

export function makeUpdateSketchPointCommand(
  pointId: string,
  x: number,
  y: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_sketch_point",
    payload: {
      point_id: pointId,
      x,
      y,
    },
  };
}

export function makeSetSketchLineConstraintCommand(
  lineId: string,
  constraint: "none" | "horizontal" | "vertical",
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_line_constraint",
    payload: {
      line_id: lineId,
      constraint,
    },
  };
}

export function makeClearSketchLineConstraintsCommand(
  lineId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "clear_sketch_line_constraints",
    payload: {
      line_id: lineId,
    },
  };
}

export function makeSetSketchEqualLengthConstraintCommand(
  lineId: string,
  otherLineId: string | null,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_equal_length_constraint",
    payload: {
      line_id: lineId,
      other_line_id: otherLineId ?? "none",
    },
  };
}

export function makeSetSketchPerpendicularConstraintCommand(
  lineId: string,
  otherLineId: string | null,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_perpendicular_constraint",
    payload: {
      line_id: lineId,
      other_line_id: otherLineId ?? "none",
    },
  };
}

// Mirror tool lifecycle factories. All five mirror the C++ ops in
// `core/sketch_feature.h`. Start opens an empty pending preview;
// the two `update_*` ops drive the live preview as the user
// edits the panel; commit/cancel finish the action.
export function makeStartMirrorPreviewCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "start_mirror_preview",
    payload: {},
  };
}

export function makeUpdateMirrorPreviewAxisCommand(
  axisLineId: string | null,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_mirror_preview_axis",
    // The C++ side treats an empty string as "no axis" (clears the
    // preview), so a null/absent UI state maps to "".
    payload: { axis_line_id: axisLineId ?? "" },
  };
}

export function makeUpdateMirrorPreviewObjectsCommand(
  objectIds: string[],
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_mirror_preview_objects",
    payload: { object_ids: objectIds },
  };
}

export function makeCommitMirrorPreviewCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "commit_mirror_preview",
    payload: {},
  };
}

export function makeCancelMirrorPreviewCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cancel_mirror_preview",
    payload: {},
  };
}

export function makeSetSketchTangentConstraintCommand(
  lineId: string,
  circleId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_tangent_constraint",
    payload: {
      line_id: lineId,
      circle_id: circleId,
    },
  };
}

export function makeSetSketchParallelConstraintCommand(
  lineId: string,
  otherLineId: string | null,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_parallel_constraint",
    payload: {
      line_id: lineId,
      other_line_id: otherLineId ?? "none",
    },
  };
}

export function makeSetSketchCoincidentConstraintCommand(
  pointId: string,
  otherPointId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_coincident_constraint",
    payload: {
      point_id: pointId,
      other_point_id: otherPointId,
    },
  };
}

export function makeSetSketchPointFixedCommand(
  pointId: string,
  isFixed: boolean,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_point_fixed",
    payload: {
      point_id: pointId,
      is_fixed: isFixed,
    },
  };
}

export function makeUpdateSketchCircleCommand(
  circleId: string,
  centerX: number,
  centerY: number,
  radius: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_sketch_circle",
    payload: {
      circle_id: circleId,
      center_x: centerX,
      center_y: centerY,
      radius,
    },
  };
}

export function makeUpdateSketchDimensionCommand(
  dimensionId: string,
  value: number | string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_sketch_dimension",
    payload: {
      dimension_id: dimensionId,
      value,
    },
  };
}

export function makeUpdateSketchDimensionLabelPositionCommand(
  dimensionId: string,
  labelX: number,
  labelY: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_sketch_dimension_label_position",
    payload: {
      dimension_id: dimensionId,
      label_x: labelX,
      label_y: labelY,
    },
  };
}

export function makeSelectSketchProfileCommand(
  profileId: string,
  additive = false,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "select_sketch_profile",
    payload: {
      profile_id: profileId,
      additive,
    },
  };
}

export function makeExtrudeProfileCommand(
  profileIds: string | readonly string[],
  depth: number,
  mode: ExtrudeMode | null = null,
  targetBodyId: string | null = null,
  parameters: Partial<ExtrudeAdvancedParameters> | null = null,
): CoreCommand {
  const ids = Array.isArray(profileIds) ? [...profileIds] : [profileIds];
  return {
    id: crypto.randomUUID(),
    type: "extrude_profile",
    payload: {
      profile_id: ids[0],
      profile_ids: ids,
      depth,
      ...(mode ? { mode } : {}),
      ...(targetBodyId ? { target_body_id: targetBodyId } : {}),
      ...(parameters ? { parameters } : {}),
    },
  };
}

export function makeExtrudeOpenEntitiesCommand(
  entityIds: readonly string[],
  depth: number,
  mode: ExtrudeMode | null = null,
  targetBodyId: string | null = null,
  parameters: Partial<ExtrudeAdvancedParameters> | null = null,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "extrude_profile",
    payload: {
      open_entity_ids: [...entityIds],
      depth,
      ...(mode ? { mode } : {}),
      ...(targetBodyId ? { target_body_id: targetBodyId } : {}),
      ...(parameters ? { parameters } : {}),
    },
  };
}

export function makeExtrudeFaceCommand(
  faceId: string,
  depth: number,
  mode: ExtrudeMode | null = null,
  targetBodyId: string | null = null,
  parameters: Partial<ExtrudeAdvancedParameters> | null = null,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "extrude_face",
    payload: {
      face_id: faceId,
      depth,
      ...(mode ? { mode } : {}),
      ...(targetBodyId ? { target_body_id: targetBodyId } : {}),
      ...(parameters ? { parameters } : {}),
    },
  };
}

export function makeUpdateExtrudeModeCommand(
  featureId: string,
  mode: ExtrudeMode,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_extrude_mode",
    payload: {
      feature_id: featureId,
      mode,
    },
  };
}

export function makeUpdateExtrudeTargetBodyCommand(
  featureId: string,
  targetBodyId: string | null,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_extrude_target_body",
    payload: {
      feature_id: featureId,
      ...(targetBodyId ? { target_body_id: targetBodyId } : {}),
    },
  };
}

export function makeUpdateExtrudeParametersCommand(
  featureId: string,
  parameters: ExtrudeFeatureParameters,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_extrude_parameters",
    payload: {
      feature_id: featureId,
      parameters,
    },
  };
}

export function makeUpdateExtrudeProfilesCommand(
  featureId: string,
  profileIds: readonly string[],
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_extrude_profiles",
    payload: {
      feature_id: featureId,
      profile_ids: [...profileIds],
    },
  };
}

export function makeLoftProfilesCommand(
  profileIds: readonly string[],
  ruled = false,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "loft_profiles",
    payload: {
      profile_ids: [...profileIds],
      ruled,
    },
  };
}

export function makeUpdateLoftProfilesCommand(
  featureId: string,
  profileIds: readonly string[],
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_loft_profiles",
    payload: {
      feature_id: featureId,
      profile_ids: [...profileIds],
    },
  };
}

export function makeUpdateLoftRuledCommand(
  featureId: string,
  ruled: boolean,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_loft_ruled",
    payload: {
      feature_id: featureId,
      ruled,
    },
  };
}

export function makeRevolveProfileCommand(
  profileId: string,
  axisEntityId: string,
  angleDegrees = 360,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "revolve_profile",
    payload: {
      profile_id: profileId,
      axis_entity_id: axisEntityId,
      angle_degrees: angleDegrees,
    },
  };
}

export function makeUpdateRevolveProfileCommand(
  featureId: string,
  profileId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_revolve_profile",
    payload: {
      feature_id: featureId,
      profile_id: profileId,
    },
  };
}

export function makeUpdateRevolveAxisCommand(
  featureId: string,
  axisEntityId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_revolve_axis",
    payload: {
      feature_id: featureId,
      axis_entity_id: axisEntityId,
    },
  };
}

export function makeUpdateRevolveAngleCommand(
  featureId: string,
  angleDegrees: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_revolve_angle",
    payload: {
      feature_id: featureId,
      angle_degrees: angleDegrees,
    },
  };
}

export function makeSweepProfileCommand(
  profileId: string,
  pathEntityId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "sweep_profile",
    payload: {
      profile_id: profileId,
      path_entity_id: pathEntityId,
    },
  };
}

export function makeUpdateSweepProfileCommand(
  featureId: string,
  profileId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_sweep_profile",
    payload: {
      feature_id: featureId,
      profile_id: profileId,
    },
  };
}

export function makeUpdateSweepPathCommand(
  featureId: string,
  pathEntityId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_sweep_path",
    payload: {
      feature_id: featureId,
      path_entity_id: pathEntityId,
    },
  };
}

export function makeAddSketchLineCommand(
  startX: number,
  startY: number,
  endX: number,
  endY: number,
  isConstruction = false,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_line",
    payload: {
      start_x: startX,
      start_y: startY,
      end_x: endX,
      end_y: endY,
      is_construction: isConstruction,
    },
  };
}

export function makeSetSketchLineConstructionCommand(
  lineId: string,
  isConstruction: boolean,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_line_construction",
    payload: {
      line_id: lineId,
      is_construction: isConstruction,
    },
  };
}

export function makeSetSketchMidpointAnchorCommand(
  pointId: string,
  hostLineId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_midpoint_anchor",
    payload: {
      point_id: pointId,
      host_line_id: hostLineId,
    },
  };
}

export function makeAddSketchAngleDimensionCommand(
  firstLineId: string,
  secondLineId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_angle_dimension",
    payload: {
      first_line_id: firstLineId,
      second_line_id: secondLineId,
    },
  };
}

export function makeAddSketchDistanceDimensionCommand(
  firstEntityId: string,
  secondEntityId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_distance_dimension",
    payload: {
      first_entity_id: firstEntityId,
      second_entity_id: secondEntityId,
    },
  };
}

export function makeAddSketchLineLengthDimensionCommand(
  lineId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_line_length_dimension",
    payload: {
      line_id: lineId,
    },
  };
}

export function makeAddSketchPointDistanceDimensionCommand(
  pointAId: string,
  pointBId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_point_distance_dimension",
    payload: {
      point_a_id: pointAId,
      point_b_id: pointBId,
    },
  };
}

export function makeAddSketchCircleRadiusDimensionCommand(
  circleId: string,
  displayAs?: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_circle_radius_dimension",
    payload: {
      circle_id: circleId,
      ...(displayAs !== undefined ? { display_as: displayAs } : {}),
    },
  };
}

export function makeAddSketchPolygonRadiusDimensionCommand(
  polygonId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_polygon_radius_dimension",
    payload: {
      polygon_id: polygonId,
    },
  };
}

export function makeSetSketchPointLineAnchorCommand(
  pointId: string,
  hostLineId: string,
  t: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_point_line_anchor",
    payload: {
      point_id: pointId,
      host_line_id: hostLineId,
      t,
    },
  };
}

export function makeAddSketchRectangleCommand(
  startX: number,
  startY: number,
  endX: number,
  endY: number,
  isConstruction = false,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_rectangle",
    payload: {
      start_x: startX,
      start_y: startY,
      end_x: endX,
      end_y: endY,
      is_construction: isConstruction,
    },
  };
}

export function makeAddSketchCircleCommand(
  centerX: number,
  centerY: number,
  radius: number,
  isConstruction = false,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_circle",
    payload: {
      center_x: centerX,
      center_y: centerY,
      radius,
      is_construction: isConstruction,
    },
  };
}

export function makeAddSketchPolygonCommand(
  sides: number,
  mode: string,
  startX: number,
  startY: number,
  endX: number,
  endY: number,
  isConstruction = false,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_polygon",
    payload: {
      sides,
      mode,
      start_x: startX,
      start_y: startY,
      end_x: endX,
      end_y: endY,
      is_construction: isConstruction,
    },
  };
}

// Build an `add_sketch_arc` command. `mode` is one of "three_point"
// (anchor lies on the arc and fixes the bulge) or "center_start_end"
// (anchor is the center; end is snapped onto the resulting circle).
// See `AddSketchArcCommand` in types/ipc.ts for the full contract.
export function makeAddSketchArcCommand(
  startX: number,
  startY: number,
  endX: number,
  endY: number,
  anchorX: number,
  anchorY: number,
  mode: "three_point" | "center_start_end",
  isConstruction = false,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_arc",
    payload: {
      start_x: startX,
      start_y: startY,
      end_x: endX,
      end_y: endY,
      anchor_x: anchorX,
      anchor_y: anchorY,
      mode,
      is_construction: isConstruction,
    },
  };
}

export function makeSelectSketchPointCommand(
  pointId: string,
  additive = false,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "select_sketch_point",
    payload: {
      point_id: pointId,
      additive,
    },
  };
}

// Sketch fillet — round a corner shared by two sketch lines into a
// tangent arc. The corner is identified by the sketch point id
// shared by both lines. v1 fillets are line-line only; line-arc
// and arc-arc remain follow-ups.
export function makeAddSketchFilletCommand(
  cornerPointId: string,
  lineAId: string,
  lineBId: string,
  radius: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_fillet",
    payload: {
      corner_point_id: cornerPointId,
      line_a_id: lineAId,
      line_b_id: lineBId,
      radius,
    },
  };
}

export function makeUpdateSketchFilletRadiusCommand(
  filletId: string,
  radius: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_sketch_fillet_radius",
    payload: {
      fillet_id: filletId,
      radius,
    },
  };
}

export function makeDeleteSketchFilletCommand(filletId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "delete_sketch_fillet",
    payload: {
      fillet_id: filletId,
    },
  };
}

export function makeDeleteSketchDimensionCommand(
  dimensionId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "delete_sketch_dimension",
    payload: {
      dimension_id: dimensionId,
    },
  };
}

export function makeUpdateSketchDimensionDisplayCommand(
  dimensionId: string,
  displayAs: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_sketch_dimension_display",
    payload: {
      dimension_id: dimensionId,
      display_as: displayAs,
    },
  };
}

export function makeUpdateSelectionFilterCommand(
  filter: {
    select_curves?: boolean;
    select_points?: boolean;
    select_construction?: boolean;
    select_constraints?: boolean;
    snap_endpoint?: boolean;
    snap_midpoint?: boolean;
    snap_center?: boolean;
    snap_intersection?: boolean;
    snap_nearest?: boolean;
    snap_quadrant?: boolean;
    snap_perpendicular?: boolean;
    snap_parallel?: boolean;
    snap_tangent?: boolean;
    snap_grid?: boolean;
    magnetic_pull?: boolean;
    tolerance_px?: number;
  },
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_selection_filter",
    payload: { ...filter },
  };
}

export function makeResolveDraftSnapCommand(
  cursorX: number,
  cursorY: number,
  startX: number,
  startY: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "resolve_draft_snap",
    payload: {
      cursor_x: cursorX,
      cursor_y: cursorY,
      start_x: startX,
      start_y: startY,
    },
  };
}

export function makeAddParameterCommand(
  name: string,
  expression: string,
  kind: "length" | "angle" = "length",
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_parameter",
    payload: { name, expression, kind },
  };
}

export function makeUpdateParameterCommand(
  name: string,
  expression: string,
  kind: "length" | "angle" = "length",
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_parameter",
    payload: { name, expression, kind },
  };
}

export function makeDeleteParameterCommand(
  name: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "delete_parameter",
    payload: { name },
  };
}

export function makeTrimSketchEntityCommand(
  entityId: string,
  clickX: number,
  clickY: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "trim_sketch_entity",
    payload: {
      entity_id: entityId,
      click_x: clickX,
      click_y: clickY,
    },
  };
}

export function makeDeleteSketchSelectionCommand(
  entityIds: readonly string[],
  pointIds: readonly string[],
  profileIds: readonly string[],
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "delete_sketch_selection",
    payload: {
      entity_ids: [...entityIds],
      point_ids: [...pointIds],
      profile_ids: [...profileIds],
    },
  };
}

export function makeSelectSketchEntityCommand(
  entityId: string,
  additive = false,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "select_sketch_entity",
    payload: {
      entity_id: entityId,
      additive,
    },
  };
}

export function makeSelectSketchDimensionCommand(
  dimensionId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "select_sketch_dimension",
    payload: {
      dimension_id: dimensionId,
    },
  };
}

export function makeFinishSketchCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "finish_sketch",
    payload: {},
  };
}

export function makeReenterSketchCommand(featureId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "reenter_sketch",
    payload: {
      feature_id: featureId,
    },
  };
}

export function makeClearSelectionCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "clear_selection",
    payload: {},
  };
}

export function getDocumentFromMessage(
  message: CoreMessage,
): DocumentState | null {
  if (
    message.type === "document_created" ||
    message.type === "document_state"
  ) {
    return message.payload;
  }

  return null;
}

export function getErrorFromMessage(message: CoreMessage): ErrorEvent | null {
  if (message.type === "error") {
    return message;
  }

  return null;
}

export function getViewportFromMessage(
  message: CoreMessage,
): ViewportState | null {
  if (message.type === "viewport_state") {
    return message.payload;
  }

  return null;
}

export function getDocumentExportFromMessage(
  message: CoreMessage,
): DocumentExportResult | null {
  if (message.type === "document_exported") {
    return message.payload;
  }

  return null;
}

export function makeTrimPreviewCommand(
  entityId: string,
  cursorX: number,
  cursorY: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "trim_preview",
    payload: {
      entity_id: entityId,
      cursor_x: cursorX,
      cursor_y: cursorY,
    },
  };
}

// ── CAM ───────────────────────────────────────────────────────────

export function makeCamSetupCreateCommand(): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_setup_create",
    payload: {
      name: "Setup",
      machine_config: { machine_type: "3_axis" },
      stock: {
        width: 120,
        height: 120,
        depth: 20,
        offset_x: 5,
        offset_y: 5,
        offset_z: 5,
      },
      wcs_origin: { x: 0, y: 0, z: 0 },
    },
  } as unknown as CoreCommand;
}

export interface CamSetupUpdatePayload {
  stock?: { width: number; height: number; depth: number; offset_x: number; offset_y: number; offset_z: number };
  wcs_origin?: { x: number; y: number; z: number };
  safety_plane_z?: number;
  wcs_angle?: number;
}

export function makeCamSetupUpdateCommand(payload: CamSetupUpdatePayload): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_setup_update",
    payload,
  } as unknown as CoreCommand;
}

export function makeCamOperationCreateCommand(
  operationType: string,
  bodyId: string,
  faceIndex: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_operation_create",
    payload: {
      operation_type: operationType,
      name: operationType === "face_milling" ? "Face Mill" : operationType,
      tool_id: "endmill_6mm",
      face_reference: { body_id: bodyId, face_index: faceIndex },
      params: { depth: 0.5, stepover: 5, angle_deg: 0 },
    },
  } as unknown as CoreCommand;
}

export interface CamOperationUpdatePayload {
  operation_id: string;
  name?: string;
  tool_id?: string;
  params?: { depth: number; stepover: number; angle_deg: number };
}

export function makeCamOperationUpdateCommand(payload: CamOperationUpdatePayload): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_operation_update",
    payload,
  } as unknown as CoreCommand;
}

export function makeCamOperationDeleteCommand(operationId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "cam_operation_delete",
    payload: { operation_id: operationId },
  } as unknown as CoreCommand;
}
