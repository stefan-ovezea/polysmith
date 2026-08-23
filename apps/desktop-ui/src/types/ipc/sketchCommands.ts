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
    vertex_id: string;
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

export interface AddSketchVertexDistanceDimensionCommand {
  id: string;
  type: "add_sketch_vertex_distance_dimension";
  payload: {
    vertex_a_id: string;
    vertex_b_id: string;
    axis?: "x" | "y";
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

export interface AddSketchArcRadiusDimensionCommand {
  id: string;
  type: "add_sketch_arc_radius_dimension";
  payload: {
    arc_id: string;
  };
}

export interface AddSketchPolygonRadiusDimensionCommand {
  id: string;
  type: "add_sketch_polygon_radius_dimension";
  payload: {
    polygon_id: string;
  };
}

export interface SetSketchVertexLineAnchorCommand {
  id: string;
  type: "set_sketch_vertex_line_anchor";
  payload: {
    vertex_id: string;
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
    // Creation mode. "center_radius" (default) uses the center/radius
    // fields; the other modes resolve in the core from raw inputs:
    // two_point (p1/p2 = diameter endpoints), three_point (p1/p2/p3 =
    // circumcircle), tangent_two_lines / tangent_three_lines (line
    // ids + hint placement point).
    mode?: string;
    p1_x?: number;
    p1_y?: number;
    p2_x?: number;
    p2_y?: number;
    p3_x?: number;
    p3_y?: number;
    line_a_id?: string;
    line_b_id?: string;
    line_c_id?: string;
    hint_x?: number;
    hint_y?: number;
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
// `corner_vertex_id` must be an endpoint of both lines; the v1 core
// rejects mismatches and oversized radii with a structured error.
export interface AddSketchFilletCommand {
  id: string;
  type: "add_sketch_fillet";
  payload: {
    corner_vertex_id: string;
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

// Sketch chamfer (line-line). Same corner/line pair pattern as the
// fillet with two trim distances instead of a radius.
export interface AddSketchChamferCommand {
  id: string;
  type: "add_sketch_chamfer";
  payload: {
    corner_vertex_id: string;
    line_a_id: string;
    line_b_id: string;
    distance_a: number;
    distance_b: number;
  };
}

export interface UpdateSketchChamferCommand {
  id: string;
  type: "update_sketch_chamfer";
  payload: {
    chamfer_id: string;
    distance_a: number;
    distance_b: number;
  };
}

export interface DeleteSketchChamferCommand {
  id: string;
  type: "delete_sketch_chamfer";
  payload: {
    chamfer_id: string;
  };
}

// Sketch ellipse (center + 2 axis points). The core derives a / b /
// rotation from the two axis clicks; the axis points are fixed at
// creation.
export interface AddSketchEllipseCommand {
  id: string;
  type: "add_sketch_ellipse";
  payload: {
    center_x: number;
    center_y: number;
    axis_a_x: number;
    axis_a_y: number;
    axis_b_x: number;
    axis_b_y: number;
    is_construction?: boolean;
  };
}

// Sketch slot (straight stadium). `length` is the distance between
// the two arc centers and must stay >= 2 * radius; `rotation` is in
// radians.
export interface AddSketchSlotCommand {
  id: string;
  type: "add_sketch_slot";
  payload: {
    center_x: number;
    center_y: number;
    length: number;
    radius: number;
    rotation: number;
    is_construction?: boolean;
  };
}

export interface UpdateSketchSlotCommand {
  id: string;
  type: "update_sketch_slot";
  payload: {
    slot_id: string;
    center_x: number;
    center_y: number;
    length: number;
    radius: number;
    rotation: number;
  };
}

// Sketch text (Fusion-style). The core expands the entry into plain
// glyph lines tagged `generated_by: "text:<id>"` on every recompute;
// the commands only carry the text parameters. Missing optional fields
// fall back to the core defaults ("Text", height 10, angle 0, center /
// middle alignment, no char spacing, default font).
export interface AddSketchTextCommand {
  id: string;
  type: "add_sketch_text";
  payload: {
    text?: string;
    font_path?: string;
    height_mm?: number;
    angle_deg?: number;
    anchor_x: number;
    anchor_y: number;
    h_align?: "left" | "center" | "right";
    v_align?: "top" | "middle" | "bottom";
    char_spacing?: number;
  };
}

// Optional-field semantics: the core merges the patch over the stored
// record, so callers only include the keys that changed. The UI sends
// the FULL merged parameter set on every debounced edit.
export interface UpdateSketchTextCommand {
  id: string;
  type: "update_sketch_text";
  payload: {
    text_id: string;
    text?: string;
    font_path?: string;
    height_mm?: number;
    angle_deg?: number;
    anchor_x?: number;
    anchor_y?: number;
    h_align?: "left" | "center" | "right";
    v_align?: "top" | "middle" | "bottom";
    char_spacing?: number;
  };
}

export interface DeleteSketchTextCommand {
  id: string;
  type: "delete_sketch_text";
  payload: {
    text_id: string;
  };
}

export interface DeleteSketchDimensionCommand {
  id: string;
  type: "delete_sketch_dimension";
  payload: {
    dimension_id: string;
  };
}

export interface ToggleSketchDimensionDrivenCommand {
  id: string;
  type: "toggle_sketch_dimension_driven";
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

// Extend tool: extends a sketch line (infinite support) or arc (full
// circle) from the end nearest the click to the nearest intersection
// with another non-construction entity.
export interface ExtendSketchEntityCommand {
  id: string;
  type: "extend_sketch_entity";
  payload: {
    entity_id: string;
    click_x: number;
    click_y: number;
  };
}

// Offset tool: creates a new entity (parallel line / concentric
// circle / same-sweep arc) at a signed distance from the source.
export interface OffsetSketchEntityCommand {
  id: string;
  type: "offset_sketch_entity";
  payload: {
    entity_id: string;
    distance: number;
  };
}

// Transform tool: translate/rotate/scale a set of sketch entities
// around a center. `copy=true` creates exploded copies with fresh
// ids and leaves the originals; uniform scale only. The plain move
// command remains the scale-1 copy-false wrapper.
// Array tools: exploded copies (direct commit in v1 — the pending
// preview workflow is deferred; undo is the adjust path).
export interface CreateLinearArrayCommand {
  id: string;
  type: "create_linear_array";
  payload: {
    entity_ids: string[];
    dx: number;
    dy: number;
    count: number;
  };
}

export interface CreateCircularArrayCommand {
  id: string;
  type: "create_circular_array";
  payload: {
    entity_ids: string[];
    center_x: number;
    center_y: number;
    count: number;
    total_angle_deg: number;
  };
}

export interface TransformSketchEntitiesCommand {
  id: string;
  type: "transform_sketch_entities";
  payload: {
    entity_ids: string[];
    dx: number;
    dy: number;
    center_x: number;
    center_y: number;
    angle_deg: number;
    scale: number;
    copy: boolean;
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
    vertex_ids: string[];
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

export interface UpdateSketchVertexCommand {
  id: string;
  type: "update_sketch_vertex";
  payload: {
    vertex_id: string;
    x: number;
    y: number;
  };
}

export interface MoveSketchEntitiesCommand {
  id: string;
  type: "move_sketch_entities";
  payload: {
    entity_ids: string[];
    dx: number;
    dy: number;
    center_x: number;
    center_y: number;
    angle_deg: number;
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
    vertex_id: string;
    other_vertex_id: string;
  };
}

export interface DeleteSketchCoincidentConstraintCommand {
  id: string;
  type: "delete_sketch_coincident_constraint";
  payload: {
    constraint_id: string;
  };
}

export interface SetSketchVertexFixedCommand {
  id: string;
  type: "set_sketch_vertex_fixed";
  payload: {
    vertex_id: string;
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

export interface SelectSketchVertexCommand {
  id: string;
  type: "select_sketch_vertex";
  payload: {
    vertex_id: string;
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
