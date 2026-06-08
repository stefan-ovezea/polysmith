import type {
  CoreCommand,
  FastenerFeatureParameters,
  HelixFeatureParameters,
  HoleFeatureParameters,
  MoveFeatureParameters,
  ThreadFeatureParameters,
} from "@/types";

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
