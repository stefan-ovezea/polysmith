import type {
  FastenerFeatureParameters,
  HelixFeatureParameters,
  HoleFeatureParameters,
  MoveFeatureParameters,
  ThreadFeatureParameters,
} from "../geometry/3d";

export interface AddBoxFeatureCommand {
  id: string;
  type: "add_box_feature";
  payload: {
    width: number;
    height: number;
    depth: number;
  };
}

export interface AddCylinderFeatureCommand {
  id: string;
  type: "add_cylinder_feature";
  payload: {
    radius: number;
    height: number;
  };
}

export interface UpdateBoxFeatureCommand {
  id: string;
  type: "update_box_feature";
  payload: {
    feature_id: string;
    width: number;
    height: number;
    depth: number;
  };
}

export interface UpdateCylinderFeatureCommand {
  id: string;
  type: "update_cylinder_feature";
  payload: {
    feature_id: string;
    radius: number;
    height: number;
  };
}

export interface UpdateExtrudeDepthCommand {
  id: string;
  type: "update_extrude_depth";
  payload: {
    feature_id: string;
    depth: number;
  };
}

export interface SetFeatureSuppressedCommand {
  id: string;
  type: "set_feature_suppressed";
  payload: {
    feature_id: string;
    suppressed: boolean;
  };
}

export interface RenameFeatureCommand {
  id: string;
  type: "rename_feature";
  payload: {
    feature_id: string;
    name: string;
  };
}

export interface DeleteFeatureCommand {
  id: string;
  type: "delete_feature";
  payload: {
    feature_id: string;
  };
}

export interface UndoCommand {
  id: string;
  type: "undo";
  payload: Record<string, never>;
}

export interface RedoCommand {
  id: string;
  type: "redo";
  payload: Record<string, never>;
}

export interface SetTimelineCursorCommand {
  id: string;
  type: "set_timeline_cursor";
  payload: {
    included_action_count: number;
  };
}

export interface SelectFeatureCommand {
  id: string;
  type: "select_feature";
  payload: {
    feature_id: string;
  };
}

export interface SelectReferenceCommand {
  id: string;
  type: "select_reference";
  payload: {
    reference_id: string;
  };
}

export interface SelectFaceCommand {
  id: string;
  type: "select_face";
  payload: {
    face_id: string;
  };
}

export interface SelectEdgeCommand {
  id: string;
  type: "select_edge";
  payload: {
    edge_id: string;
    // When true, the edge is toggled into the existing edge
    // selection set (shift-click). When false / omitted, it replaces
    // the previous edge selection.
    additive: boolean;
  };
}

export interface SelectVertexCommand {
  id: string;
  type: "select_vertex";
  payload: {
    // Mirrors SelectEdgeCommand: shift-click toggles into the
    // multi-vertex set; plain click replaces.
    additive: boolean;
    vertex_id: string;
  };
}

export interface SetBodyColorCommand {
  id: string;
  type: "set_body_color";
  payload: {
    body_id: string;
    color: string;
  };
}

export interface SetFaceColorCommand {
  id: string;
  type: "set_face_color";
  payload: {
    face_id: string;
    color: string;
  };
}

export interface ClearBodyColorCommand {
  id: string;
  type: "clear_body_color";
  payload: {
    body_id: string;
  };
}

export interface ClearFaceColorCommand {
  id: string;
  type: "clear_face_color";
  payload: {
    face_id: string;
  };
}

export interface ClearAppearanceOverridesCommand {
  id: string;
  type: "clear_appearance_overrides";
  payload: Record<string, never>;
}

export interface CreateFilletCommand {
  id: string;
  type: "create_fillet";
  payload: {
    // One or more edges (must all share the same owner body — the
    // core rejects mixed-body selections).
    edge_ids: string[];
    radius: number;
  };
}

export interface UpdateFilletRadiusCommand {
  id: string;
  type: "update_fillet_radius";
  payload: {
    feature_id: string;
    radius: number;
  };
}

export interface UpdateFilletEdgesCommand {
  id: string;
  type: "update_fillet_edges";
  payload: {
    feature_id: string;
    edge_ids: string[];
  };
}

export interface UpdateChamferEdgesCommand {
  id: string;
  type: "update_chamfer_edges";
  payload: {
    feature_id: string;
    edge_ids: string[];
  };
}

export interface CreateChamferCommand {
  id: string;
  type: "create_chamfer";
  payload: {
    edge_ids: string[];
    distance: number;
  };
}

export interface UpdateChamferDistanceCommand {
  id: string;
  type: "update_chamfer_distance";
  payload: {
    feature_id: string;
    distance: number;
  };
}

export interface ConfirmFilletCommand {
  id: string;
  type: "confirm_fillet";
  payload: {
    feature_id: string;
  };
}

export interface ConfirmChamferCommand {
  id: string;
  type: "confirm_chamfer";
  payload: {
    feature_id: string;
  };
}

export interface CreateShellCommand {
  id: string;
  type: "create_shell";
  payload: {
    face_id: string;
    thickness: number;
  };
}

export interface UpdateShellThicknessCommand {
  id: string;
  type: "update_shell_thickness";
  payload: {
    feature_id: string;
    thickness: number;
  };
}

export interface ConfirmShellCommand {
  id: string;
  type: "confirm_shell";
  payload: {
    feature_id: string;
  };
}

// Create a parametric offset construction plane. `source_plane_id`
// must resolve to a plane the core knows about (origin plane,
// existing construction plane, sketch profile id, or
// "<body_id>:face:<index>" face id).
export interface CreateOffsetPlaneCommand {
  id: string;
  type: "create_offset_plane";
  payload: {
    source_plane_id: string;
    offset: number;
  };
}

export interface CreateMidplaneCommand {
  id: string;
  type: "create_midplane";
  payload: {
    source_plane_ids: [string, string];
  };
}

export interface CreateTangentPlaneCommand {
  id: string;
  type: "create_tangent_plane";
  payload: {
    source_face_id: string;
  };
}

export interface CreateAnglePlaneCommand {
  id: string;
  type: "create_angle_plane";
  payload: {
    source_plane_id: string;
    source_axis_id: string;
    angle_degrees: number;
  };
}

export interface CreateConstructionAxisCommand {
  id: string;
  type: "create_construction_axis";
  payload: {
    source_id: string;
  };
}

export interface CreateConstructionPointCommand {
  id: string;
  type: "create_construction_point";
  payload: {
    source_id: string;
  };
}

export interface CreateHoleCommand {
  id: string;
  type: "create_hole";
  payload: Partial<HoleFeatureParameters> & {
    face_id: string;
    center_x: number;
    center_y: number;
    center_z: number;
  };
}

export interface UpdateHoleParametersCommand {
  id: string;
  type: "update_hole_parameters";
  payload: {
    feature_id: string;
    parameters: Partial<HoleFeatureParameters>;
  };
}

export interface ConfirmHoleCommand {
  id: string;
  type: "confirm_hole";
  payload: {
    feature_id: string;
  };
}

export interface CreateHelixCommand {
  id: string;
  type: "create_helix";
  payload: Partial<HelixFeatureParameters> & {
    axis_source_id: string;
  };
}

export interface UpdateHelixParametersCommand {
  id: string;
  type: "update_helix_parameters";
  payload: {
    feature_id: string;
    parameters: Partial<HelixFeatureParameters>;
  };
}

export interface CreateThreadCommand {
  id: string;
  type: "create_thread";
  payload: Partial<ThreadFeatureParameters>;
}

export interface UpdateThreadParametersCommand {
  id: string;
  type: "update_thread_parameters";
  payload: {
    feature_id: string;
    parameters: Partial<ThreadFeatureParameters>;
  };
}

export interface ConfirmThreadCommand {
  id: string;
  type: "confirm_thread";
  payload: {
    feature_id: string;
  };
}

export interface CreateFastenerCommand {
  id: string;
  type: "create_fastener";
  payload: Partial<FastenerFeatureParameters>;
}

export interface UpdateFastenerParametersCommand {
  id: string;
  type: "update_fastener_parameters";
  payload: {
    feature_id: string;
    parameters: Partial<FastenerFeatureParameters>;
  };
}

export interface CreateMoveCommand {
  id: string;
  type: "create_move";
  payload: Partial<MoveFeatureParameters> & {
    target_body_id: string;
  };
}

export interface UpdateMoveParametersCommand {
  id: string;
  type: "update_move_parameters";
  payload: {
    feature_id: string;
    parameters: Partial<MoveFeatureParameters>;
  };
}

export interface ConfirmMoveCommand {
  id: string;
  type: "confirm_move";
  payload: {
    feature_id: string;
  };
}

export interface CreateBodyCopyCommand {
  id: string;
  type: "create_body_copy";
  payload: {
    source_body_id: string;
    copy_mode?: "linked" | "standalone";
  };
}

export interface UnlinkBodyCopyCommand {
  id: string;
  type: "unlink_body_copy";
  payload: {
    feature_id: string;
  };
}

// Live-edit an existing construction plane's offset. The core
// re-derives the cached frame from the source's current frame, so
// chained planes / face-source planes update correctly.
export interface UpdateOffsetPlaneCommand {
  id: string;
  type: "update_offset_plane";
  payload: {
    feature_id: string;
    offset: number;
  };
}

export interface UpdateAnglePlaneCommand {
  id: string;
  type: "update_angle_plane";
  payload: {
    feature_id: string;
    angle_degrees: number;
  };
}
