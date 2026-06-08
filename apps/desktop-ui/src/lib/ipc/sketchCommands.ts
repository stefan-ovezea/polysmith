import type { CoreCommand, SketchTool } from "@/types";

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
// `core/sketch/sketch_feature.h`. Start opens an empty pending preview;
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


export function makeCommitMirrorPreviewCommand(
  persistent: boolean = false,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "commit_mirror_preview",
    payload: { persistent },
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


export function makeDeleteSketchCoincidentConstraintCommand(
  constraintId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "delete_sketch_coincident_constraint",
    payload: {
      constraint_id: constraintId,
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
