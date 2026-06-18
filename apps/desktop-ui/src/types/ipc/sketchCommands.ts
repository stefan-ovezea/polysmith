import type { SketchTool } from "../geometry/sketch";
import type { PlaneFrame } from "../geometry/primitives";

export interface StartSketchOnPlaneCommand {
  id: string;
  type: "start_sketch_on_plane";
  payload: {
    reference_id: string;
  };
}

export interface StartSketchOnFaceCommand {
  id: string;
  type: "start_sketch_on_face";
  payload: {
    face_id: string;
    plane_frame: PlaneFrame;
  };
}

export interface AddSketchLineCommand {
  id: string;
  type: "add_sketch_line";
  payload: {
    start_x: number;
    start_y: number;
    end_x: number;
    end_y: number;
    is_construction: boolean;
  };
}

export interface SetSketchLineConstructionCommand {
  id: string;
  type: "set_sketch_line_construction";
  payload: {
    line_id: string;
    is_construction: boolean;
  };
}

export interface SetSketchMidpointAnchorCommand {
  id: string;
  type: "set_sketch_midpoint_anchor";
  payload: {
    point_id: string;
    // Empty string clears any existing anchor for the point.
    host_line_id: string;
  };
}

export interface AddSketchAngleDimensionCommand {
  id: string;
  type: "add_sketch_angle_dimension";
  payload: {
    first_line_id: string;
    second_line_id: string;
    value?: number;
  };
}

export interface AddSketchDistanceDimensionCommand {
  id: string;
  type: "add_sketch_distance_dimension";
  payload: {
    first_entity_id: string;
    second_entity_id: string;
  };
}

export interface AddSketchPointDistanceDimensionCommand {
  id: string;
  type: "add_sketch_point_distance_dimension";
  payload: {
    point_a_id: string;
    point_b_id: string;
  };
}

export interface AddSketchLineLengthDimensionCommand {
  id: string;
  type: "add_sketch_line_length_dimension";
  payload: {
    line_id: string;
  };
}

export interface AddSketchLineAngleDimensionCommand {
  id: string;
  type: "add_sketch_line_angle_dimension";
  payload: {
    line_id: string;
  };
}

export interface AddSketchCircleRadiusDimensionCommand {
  id: string;
  type: "add_sketch_circle_radius_dimension";
  payload: {
    circle_id: string;
  };
}

export interface AddSketchPolygonRadiusDimensionCommand {
  id: string;
  type: "add_sketch_polygon_radius_dimension";
  payload: {
    polygon_id: string;
  };
}

export interface SetSketchPointLineAnchorCommand {
  id: string;
  type: "set_sketch_point_line_anchor";
  payload: {
    point_id: string;
    // Empty string clears any existing anchor for the point.
    host_line_id: string;
    // Parametric position along the host line, clamped to [0, 1] by
    // the core. 0 = host start, 1 = host end.
    t: number;
  };
}

export interface AddSketchRectangleCommand {
  id: string;
  type: "add_sketch_rectangle";
  payload: {
    start_x: number;
    start_y: number;
    end_x: number;
    end_y: number;
    is_construction: boolean;
  };
}

export interface AddSketchCircleCommand {
  id: string;
  type: "add_sketch_circle";
  payload: {
    center_x: number;
    center_y: number;
    radius: number;
    is_construction: boolean;
  };
}

export interface AddSketchPolygonCommand {
  id: string;
  type: "add_sketch_polygon";
  payload: {
    sides: number;
    mode: string;
    start_x: number;
    start_y: number;
    end_x: number;
    end_y: number;
    is_construction: boolean;
  };
}

// Add an arc to the active sketch. The third anchor's interpretation
// depends on `mode`:
//   - "three_point": (start, end, anchor) where anchor lies on the
//     arc and fixes the bulge. Center is the circumcenter of the
//     three points.
//   - "center_start_end": anchor is the center; radius derives from
//     |center - start|, and the end point is snapped onto the
//     resulting circle.
export interface AddSketchArcCommand {
  id: string;
  type: "add_sketch_arc";
  payload: {
    start_x: number;
    start_y: number;
    end_x: number;
    end_y: number;
    anchor_x: number;
    anchor_y: number;
    mode: "three_point" | "center_start_end";
    is_construction: boolean;
  };
}

// Round a corner shared by two sketch lines into a tangent arc.
// `corner_point_id` must be an endpoint of both lines; the v1 core
// rejects mismatches and oversized radii with a structured error.
export interface AddSketchFilletCommand {
  id: string;
  type: "add_sketch_fillet";
  payload: {
    corner_point_id: string;
    line_a_id: string;
    line_b_id: string;
    radius: number;
  };
}

export interface UpdateSketchFilletRadiusCommand {
  id: string;
  type: "update_sketch_fillet_radius";
  payload: {
    fillet_id: string;
    radius: number;
  };
}

export interface DeleteSketchFilletCommand {
  id: string;
  type: "delete_sketch_fillet";
  payload: {
    fillet_id: string;
  };
}

export interface DeleteSketchDimensionCommand {
  id: string;
  type: "delete_sketch_dimension";
  payload: {
    dimension_id: string;
  };
}

export interface TrimSketchEntityCommand {
  id: string;
  type: "trim_sketch_entity";
  payload: {
    entity_id: string;
    click_x: number;
    click_y: number;
  };
}

export interface TrimPreviewCommand {
  id: string;
  type: "trim_preview";
  payload: {
    entity_id: string;
    cursor_x: number;
    cursor_y: number;
  };
}

export interface DeleteSketchSelectionCommand {
  id: string;
  type: "delete_sketch_selection";
  payload: {
    entity_ids: string[];
    point_ids: string[];
    profile_ids: string[];
  };
}

export interface SetSketchToolCommand {
  id: string;
  type: "set_sketch_tool";
  payload: {
    tool: SketchTool;
  };
}

export interface UpdateSketchLineCommand {
  id: string;
  type: "update_sketch_line";
  payload: {
    line_id: string;
    start_x: number;
    start_y: number;
    end_x: number;
    end_y: number;
  };
}

export interface UpdateSketchPointCommand {
  id: string;
  type: "update_sketch_point";
  payload: {
    point_id: string;
    x: number;
    y: number;
  };
}

export interface SetSketchLineConstraintCommand {
  id: string;
  type: "set_sketch_line_constraint";
  payload: {
    line_id: string;
    constraint: "none" | "horizontal" | "vertical";
  };
}

export interface ClearSketchLineConstraintsCommand {
  id: string;
  type: "clear_sketch_line_constraints";
  payload: {
    line_id: string;
  };
}

export interface SetSketchEqualLengthConstraintCommand {
  id: string;
  type: "set_sketch_equal_length_constraint";
  payload: {
    line_id: string;
    other_line_id: string;
  };
}

export interface SetSketchPerpendicularConstraintCommand {
  id: string;
  type: "set_sketch_perpendicular_constraint";
  payload: {
    line_id: string;
    other_line_id: string;
  };
}

// Mirror tool — contextual modeling pending preview lifecycle. See
// `wiki/polysmith.wiki/Contextual-Modeling-Workflow.md` and
// `core/sketch/sketch_feature.h`.
export interface StartMirrorPreviewCommand {
  id: string;
  type: "start_mirror_preview";
  payload: Record<string, never>;
}

export interface UpdateMirrorPreviewAxisCommand {
  id: string;
  type: "update_mirror_preview_axis";
  payload: {
    // Empty string clears the axis (preview drops to no geometry).
    axis_line_id: string;
  };
}

export interface UpdateMirrorPreviewObjectsCommand {
  id: string;
  type: "update_mirror_preview_objects";
  payload: {
    object_ids: string[];
  };
}

export interface CommitMirrorPreviewCommand {
  id: string;
  type: "commit_mirror_preview";
  payload: {
    persistent?: boolean;
  };
}

export interface CancelMirrorPreviewCommand {
  id: string;
  type: "cancel_mirror_preview";
  payload: Record<string, never>;
}

export interface SetSketchTangentConstraintCommand {
  id: string;
  type: "set_sketch_tangent_constraint";
  payload: {
    line_id: string;
    // Empty string clears any existing tangent relation on the line.
    circle_id: string;
  };
}

export interface SetSketchParallelConstraintCommand {
  id: string;
  type: "set_sketch_parallel_constraint";
  payload: {
    line_id: string;
    other_line_id: string;
  };
}

export interface SetSketchCoincidentConstraintCommand {
  id: string;
  type: "set_sketch_coincident_constraint";
  payload: {
    point_id: string;
    other_point_id: string;
  };
}

export interface DeleteSketchCoincidentConstraintCommand {
  id: string;
  type: "delete_sketch_coincident_constraint";
  payload: {
    constraint_id: string;
  };
}

export interface SetSketchPointFixedCommand {
  id: string;
  type: "set_sketch_point_fixed";
  payload: {
    point_id: string;
    is_fixed: boolean;
  };
}

export interface UpdateSketchCircleCommand {
  id: string;
  type: "update_sketch_circle";
  payload: {
    circle_id: string;
    center_x: number;
    center_y: number;
    radius: number;
  };
}

export interface UpdateSketchDimensionCommand {
  id: string;
  type: "update_sketch_dimension";
  payload: {
    dimension_id: string;
    value: number | string;
  };
}

export interface UpdateSketchDimensionLabelPositionCommand {
  id: string;
  type: "update_sketch_dimension_label_position";
  payload: {
    dimension_id: string;
    label_x: number;
    label_y: number;
  };
}

export interface UpdateSketchDimensionDisplayCommand {
  id: string;
  type: "update_sketch_dimension_display";
  payload: {
    dimension_id: string;
    display_as: string;
  };
}

export interface SelectSketchProfileCommand {
  id: string;
  type: "select_sketch_profile";
  payload: {
    profile_id: string;
    additive?: boolean;
  };
}

export interface SelectSketchEntityCommand {
  id: string;
  type: "select_sketch_entity";
  payload: {
    entity_id: string;
    additive: boolean;
  };
}

export interface SelectSketchPointCommand {
  id: string;
  type: "select_sketch_point";
  payload: {
    point_id: string;
    additive: boolean;
  };
}

export interface SelectSketchDimensionCommand {
  id: string;
  type: "select_sketch_dimension";
  payload: {
    dimension_id: string;
  };
}

export interface FinishSketchCommand {
  id: string;
  type: "finish_sketch";
  payload: Record<string, never>;
}

export interface ReenterSketchCommand {
  id: string;
  type: "reenter_sketch";
  payload: {
    feature_id: string;
  };
}

export interface ClearSelectionCommand {
  id: string;
  type: "clear_selection";
  payload: Record<string, never>;
}
