import type { CoreCommand, SketchEntityPlaneFrame, SketchTool } from "@/types";

export type SketchPlaneFramePayload = SketchEntityPlaneFrame;

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
  planeFrame: SketchPlaneFramePayload,
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


export function makeUpdateSketchVertexCommand(
  vertexId: string,
  x: number,
  y: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_sketch_vertex",
    payload: {
      vertex_id: vertexId,
      x,
      y,
    },
  };
}

export function makeMoveSketchEntitiesCommand(params: {
  entityIds: string[];
  dx: number;
  dy: number;
  centerX: number;
  centerY: number;
  angleDeg: number;
}): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "move_sketch_entities",
    payload: {
      entity_ids: params.entityIds,
      dx: params.dx,
      dy: params.dy,
      center_x: params.centerX,
      center_y: params.centerY,
      angle_deg: params.angleDeg,
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
  vertexId: string,
  otherVertexId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_coincident_constraint",
    payload: {
      vertex_id: vertexId,
      other_vertex_id: otherVertexId,
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


export function makeSetSketchVertexFixedCommand(
  vertexId: string,
  isFixed: boolean,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_vertex_fixed",
    payload: {
      vertex_id: vertexId,
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
  vertexId: string,
  hostLineId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_midpoint_anchor",
    payload: {
      vertex_id: vertexId,
      host_line_id: hostLineId,
    },
  };
}


export function makeAddSketchAngleDimensionCommand(
  firstLineId: string,
  secondLineId: string,
  value?: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_angle_dimension",
    payload: {
      first_line_id: firstLineId,
      second_line_id: secondLineId,
      ...(value !== undefined ? { value } : {}),
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

export function makeAddSketchLineAngleDimensionCommand(
  lineId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_line_angle_dimension",
    payload: {
      line_id: lineId,
    },
  };
}


export function makeAddSketchVertexDistanceDimensionCommand(
  vertexAId: string,
  vertexBId: string,
  axis?: "x" | "y",
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_vertex_distance_dimension",
    payload: {
      vertex_a_id: vertexAId,
      vertex_b_id: vertexBId,
      ...(axis ? { axis } : {}),
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


export function makeAddSketchArcRadiusDimensionCommand(
  arcId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_arc_radius_dimension",
    payload: {
      arc_id: arcId,
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


export function makeSetSketchVertexLineAnchorCommand(
  vertexId: string,
  hostLineId: string,
  t: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "set_sketch_vertex_line_anchor",
    payload: {
      vertex_id: vertexId,
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


export function makeSelectSketchVertexCommand(
  vertexId: string,
  additive = false,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "select_sketch_vertex",
    payload: {
      vertex_id: vertexId,
      additive,
    },
  };
}


// Sketch fillet — round a corner shared by two sketch lines into a
// tangent arc. The corner is identified by the sketch point id
// shared by both lines. v1 fillets are line-line only; line-arc
// and arc-arc remain follow-ups.
export function makeAddSketchFilletCommand(
  cornerVertexId: string,
  lineAId: string,
  lineBId: string,
  radius: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_fillet",
    payload: {
      corner_vertex_id: cornerVertexId,
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

// Sketch chamfer command factories (mirror the fillet trio with two
// distances).
export function makeAddSketchChamferCommand(
  cornerVertexId: string,
  lineAId: string,
  lineBId: string,
  distanceA: number,
  distanceB: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_chamfer",
    payload: {
      corner_vertex_id: cornerVertexId,
      line_a_id: lineAId,
      line_b_id: lineBId,
      distance_a: distanceA,
      distance_b: distanceB,
    },
  };
}

export function makeUpdateSketchChamferCommand(
  chamferId: string,
  distanceA: number,
  distanceB: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_sketch_chamfer",
    payload: {
      chamfer_id: chamferId,
      distance_a: distanceA,
      distance_b: distanceB,
    },
  };
}

export function makeDeleteSketchChamferCommand(
  chamferId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "delete_sketch_chamfer",
    payload: {
      chamfer_id: chamferId,
    },
  };
}

// Sketch ellipse / slot command factories.
export function makeAddSketchEllipseCommand(
  centerX: number,
  centerY: number,
  axisAX: number,
  axisAY: number,
  axisBX: number,
  axisBY: number,
  isConstruction: boolean,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_ellipse",
    payload: {
      center_x: centerX,
      center_y: centerY,
      axis_a_x: axisAX,
      axis_a_y: axisAY,
      axis_b_x: axisBX,
      axis_b_y: axisBY,
      is_construction: isConstruction,
    },
  };
}

export function makeAddSketchSlotCommand(
  centerX: number,
  centerY: number,
  length: number,
  radius: number,
  rotation: number,
  isConstruction: boolean,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_slot",
    payload: {
      center_x: centerX,
      center_y: centerY,
      length,
      radius,
      rotation,
      is_construction: isConstruction,
    },
  };
}

export function makeUpdateSketchSlotCommand(
  slotId: string,
  centerX: number,
  centerY: number,
  length: number,
  radius: number,
  rotation: number,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_sketch_slot",
    payload: {
      slot_id: slotId,
      center_x: centerX,
      center_y: centerY,
      length,
      radius,
      rotation,
    },
  };
}


// Sketch text command factories. The core expands every text entry
// into plain glyph lines on recompute, so these commands only carry
// the text parameters. `anchor_x` / `anchor_y` are required; every
// other field falls back to the core default when omitted.
export function makeAddSketchTextCommand(params: {
  text?: string;
  font_path?: string;
  height_mm?: number;
  angle_deg?: number;
  anchor_x: number;
  anchor_y: number;
  h_align?: "left" | "center" | "right";
  v_align?: "top" | "middle" | "bottom";
  char_spacing?: number;
}): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "add_sketch_text",
    payload: {
      anchor_x: params.anchor_x,
      anchor_y: params.anchor_y,
      ...(params.text !== undefined ? { text: params.text } : {}),
      ...(params.font_path !== undefined ? { font_path: params.font_path } : {}),
      ...(params.height_mm !== undefined ? { height_mm: params.height_mm } : {}),
      ...(params.angle_deg !== undefined ? { angle_deg: params.angle_deg } : {}),
      ...(params.h_align !== undefined ? { h_align: params.h_align } : {}),
      ...(params.v_align !== undefined ? { v_align: params.v_align } : {}),
      ...(params.char_spacing !== undefined
        ? { char_spacing: params.char_spacing }
        : {}),
    },
  };
}


// Only the keys actually present in `patch` are included in the
// payload; the core merges them over the stored record.
export function makeUpdateSketchTextCommand(
  textId: string,
  patch: {
    text?: string;
    font_path?: string;
    height_mm?: number;
    angle_deg?: number;
    anchor_x?: number;
    anchor_y?: number;
    h_align?: "left" | "center" | "right";
    v_align?: "top" | "middle" | "bottom";
    char_spacing?: number;
    path_entity_id?: string | null;
    path_offset?: number;
  },
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "update_sketch_text",
    payload: {
      text_id: textId,
      ...(patch.text !== undefined ? { text: patch.text } : {}),
      ...(patch.font_path !== undefined ? { font_path: patch.font_path } : {}),
      ...(patch.height_mm !== undefined
        ? { height_mm: patch.height_mm }
        : {}),
      ...(patch.angle_deg !== undefined ? { angle_deg: patch.angle_deg } : {}),
      ...(patch.anchor_x !== undefined ? { anchor_x: patch.anchor_x } : {}),
      ...(patch.anchor_y !== undefined ? { anchor_y: patch.anchor_y } : {}),
      ...(patch.h_align !== undefined ? { h_align: patch.h_align } : {}),
      ...(patch.v_align !== undefined ? { v_align: patch.v_align } : {}),
      ...(patch.char_spacing !== undefined
        ? { char_spacing: patch.char_spacing }
        : {}),
      ...(patch.path_entity_id !== undefined
        ? { path_entity_id: patch.path_entity_id }
        : {}),
      ...(patch.path_offset !== undefined
        ? { path_offset: patch.path_offset }
        : {}),
    },
  };
}


export function makeDeleteSketchTextCommand(textId: string): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "delete_sketch_text",
    payload: {
      text_id: textId,
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


export function makeToggleSketchDimensionDrivenCommand(
  dimensionId: string,
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "toggle_sketch_dimension_driven",
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
  vertexIds: readonly string[],
  profileIds: readonly string[],
): CoreCommand {
  return {
    id: crypto.randomUUID(),
    type: "delete_sketch_selection",
    payload: {
      entity_ids: [...entityIds],
      vertex_ids: [...vertexIds],
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
