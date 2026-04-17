export interface BoxFeatureParameters {
  width: number;
  height: number;
  depth: number;
}

export interface CylinderFeatureParameters {
  radius: number;
  height: number;
}

export interface SketchProfilePoint {
  x: number;
  y: number;
}

export interface ExtrudeFeatureParameters {
  sketch_feature_id: string;
  profile_id: string;
  plane_id: string;
  plane_frame: {
    origin: ViewportVector3;
    x_axis: ViewportVector3;
    y_axis: ViewportVector3;
    normal: ViewportVector3;
  } | null;
  profile_kind: "rectangle" | "circle" | "polygon";
  start_x: number;
  start_y: number;
  width: number;
  height: number;
  radius: number;
  profile_points: SketchProfilePoint[];
  depth: number;
}

export interface SketchLineEntry {
  line_id: string;
  start_point_id: string;
  end_point_id: string;
  start_x: number;
  start_y: number;
  end_x: number;
  end_y: number;
  constraint: "horizontal" | "vertical" | null;
}

export interface SketchCircleEntry {
  circle_id: string;
  center_x: number;
  center_y: number;
  radius: number;
}

export interface SketchDimensionEntry {
  dimension_id: string;
  kind: "line_length" | "circle_radius";
  entity_id: string;
  value: number;
}

export interface SketchLineRelationEntry {
  relation_id: string;
  kind: "equal_length" | "perpendicular" | "parallel";
  first_line_id: string;
  second_line_id: string;
}

export interface SketchFeatureParameters {
  plane_id: string;
  plane_frame: {
    origin: ViewportVector3;
    x_axis: ViewportVector3;
    y_axis: ViewportVector3;
    normal: ViewportVector3;
  } | null;
  active_tool: "select" | "line" | "rectangle" | "circle";
  lines: SketchLineEntry[];
  circles: SketchCircleEntry[];
  dimensions: SketchDimensionEntry[];
  line_relations: SketchLineRelationEntry[];
}

export interface ViewportVector3 {
  x: number;
  y: number;
  z: number;
}

export interface FeatureEntry {
  feature_id: string;
  kind: string;
  name: string;
  status: string;
  parameters_summary: string;
  box_parameters: BoxFeatureParameters | null;
  cylinder_parameters: CylinderFeatureParameters | null;
  extrude_parameters: ExtrudeFeatureParameters | null;
  sketch_parameters: SketchFeatureParameters | null;
}

export interface ViewportPlaneSize {
  width: number;
  height: number;
}

export interface ViewportBoxPrimitive {
  primitive_id: string;
  label: string;
  width: number;
  height: number;
  depth: number;
  x_offset: number;
  center: ViewportVector3;
  is_selected: boolean;
}

export interface ViewportCylinderPrimitive {
  primitive_id: string;
  label: string;
  radius: number;
  height: number;
  x_offset: number;
  center: ViewportVector3;
  is_selected: boolean;
}

export interface ViewportPolygonExtrudePrimitive {
  primitive_id: string;
  label: string;
  plane_id: string;
  plane_frame: {
    origin: ViewportVector3;
    x_axis: ViewportVector3;
    y_axis: ViewportVector3;
    normal: ViewportVector3;
  } | null;
  profile_points: SketchProfilePoint[];
  depth: number;
  is_selected: boolean;
}

export interface ViewportSolidFace {
  face_id: string;
  owner_id: string;
  owner_kind: string;
  label: string;
  sketchability: string;
  center: ViewportVector3;
  normal: ViewportVector3;
  plane_frame: {
    origin: ViewportVector3;
    x_axis: ViewportVector3;
    y_axis: ViewportVector3;
    normal: ViewportVector3;
  };
  size: {
    width: number;
    height: number;
    radius: number;
  };
  is_selected: boolean;
}

export interface ViewportReferencePlane {
  reference_id: string;
  label: string;
  orientation: "xy" | "yz" | "xz";
  center: ViewportVector3;
  size: ViewportPlaneSize;
  is_selected: boolean;
  is_active_sketch_plane: boolean;
}

export interface ViewportReferenceAxis {
  reference_id: string;
  label: string;
  axis: "x" | "y" | "z";
  start: ViewportVector3;
  end: ViewportVector3;
}

export interface ViewportSketchLine {
  line_id: string;
  start_point_id: string;
  end_point_id: string;
  plane_id: string;
  start: ViewportVector3;
  end: ViewportVector3;
  is_selected: boolean;
  constraint: "horizontal" | "vertical" | null;
}

export interface ViewportSketchCircle {
  circle_id: string;
  plane_id: string;
  center: ViewportVector3;
  radius: number;
  is_selected: boolean;
}

export interface ViewportSketchDimension {
  dimension_id: string;
  plane_id: string;
  kind: "line_length" | "circle_radius";
  entity_id: string;
  label: string;
  is_selected: boolean;
  anchor_start: ViewportVector3;
  anchor_end: ViewportVector3;
  dimension_start: ViewportVector3;
  dimension_end: ViewportVector3;
  label_position: ViewportVector3;
}

export interface ViewportSketchConstraint {
  constraint_id: string;
  plane_id: string;
  kind: "horizontal" | "vertical" | "equal_length" | "perpendicular" | "parallel";
  entity_id: string;
  related_entity_id: string | null;
  label: string;
  is_selected: boolean;
  position: ViewportVector3;
}

export interface ViewportSketchProfile {
  profile_id: string;
  plane_id: string;
  plane_frame: {
    origin: ViewportVector3;
    x_axis: ViewportVector3;
    y_axis: ViewportVector3;
    normal: ViewportVector3;
  } | null;
  profile_kind: "polygon" | "circle";
  profile_points: SketchProfilePoint[];
  start_x: number;
  start_y: number;
  width: number;
  height: number;
  radius: number;
  is_selected: boolean;
}

export interface ViewportSceneBounds {
  center: ViewportVector3;
  size: ViewportVector3;
  max_dimension: number;
}

export interface DocumentState {
  document_id: string;
  name: string;
  units: string;
  revision: number;
  selected_feature_id: string | null;
  selected_reference_id: string | null;
  selected_face_id: string | null;
  active_sketch_plane_id: string | null;
  active_sketch_face_id: string | null;
  active_sketch_feature_id: string | null;
  active_sketch_tool: "select" | "line" | "rectangle" | "circle" | null;
  selected_sketch_entity_id: string | null;
  selected_sketch_dimension_id: string | null;
  selected_sketch_profile_id: string | null;
  feature_history: FeatureEntry[];
}

export interface SessionState {
  document_count: number;
  has_active_document: boolean;
  active_document_id: string | null;
  can_undo: boolean;
  can_redo: boolean;
}

export interface ViewportState {
  has_active_document: boolean;
  boxes: ViewportBoxPrimitive[];
  cylinders: ViewportCylinderPrimitive[];
  polygon_extrudes: ViewportPolygonExtrudePrimitive[];
  solid_faces: ViewportSolidFace[];
  reference_planes: ViewportReferencePlane[];
  reference_axes: ViewportReferenceAxis[];
  sketch_lines: ViewportSketchLine[];
  sketch_circles: ViewportSketchCircle[];
  sketch_dimensions: ViewportSketchDimension[];
  sketch_constraints: ViewportSketchConstraint[];
  sketch_profiles: ViewportSketchProfile[];
  scene_width: number;
  scene_height: number;
  scene_depth: number;
  scene_bounds: ViewportSceneBounds;
}

export interface DocumentExportResult {
  file_path: string;
  format: "step";
  exported_feature_count: number;
}

export interface BaseMessage {
  id?: string;
  type: string;
  payload?: unknown;
}

export interface HelloEvent extends BaseMessage {
  type: "hello";
  payload: {
    service: string;
    version: string;
  };
}

export interface PongEvent extends BaseMessage {
  type: "pong";
  id: string;
  payload: {
    version: string;
  };
}

export interface DocumentCreatedEvent extends BaseMessage {
  type: "document_created";
  id: string;
  payload: DocumentState;
}

export interface DocumentStateEvent extends BaseMessage {
  type: "document_state";
  id: string;
  payload: DocumentState;
}

export interface SessionStateEvent extends BaseMessage {
  type: "session_state";
  id: string;
  payload: SessionState;
}

export interface ViewportStateEvent extends BaseMessage {
  type: "viewport_state";
  id: string;
  payload: ViewportState;
}

export interface DocumentExportedEvent extends BaseMessage {
  type: "document_exported";
  id: string;
  payload: DocumentExportResult;
}

export interface ErrorEvent extends BaseMessage {
  type: "error";
  id?: string;
  payload: {
    code: string;
    message: string;
  };
}

export type CoreMessage =
  | HelloEvent
  | PongEvent
  | DocumentCreatedEvent
  | DocumentStateEvent
  | SessionStateEvent
  | ViewportStateEvent
  | DocumentExportedEvent
  | ErrorEvent;

export interface PingCommand {
  id: string;
  type: "ping";
  payload: Record<string, never>;
}

export interface CreateDocumentCommand {
  id: string;
  type: "create_document";
  payload: Record<string, never>;
}

export interface GetDocumentStateCommand {
  id: string;
  type: "get_document_state";
  payload: Record<string, never>;
}

export interface GetSessionStateCommand {
  id: string;
  type: "get_session_state";
  payload: Record<string, never>;
}

export interface GetViewportStateCommand {
  id: string;
  type: "get_viewport_state";
  payload: Record<string, never>;
}

export interface ExportDocumentCommand {
  id: string;
  type: "export_document";
  payload: {
    file_path: string;
  };
}

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
    plane_frame: {
      origin: ViewportVector3;
      x_axis: ViewportVector3;
      y_axis: ViewportVector3;
      normal: ViewportVector3;
    };
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
  };
}

export interface AddSketchCircleCommand {
  id: string;
  type: "add_sketch_circle";
  payload: {
    center_x: number;
    center_y: number;
    radius: number;
  };
}

export interface SetSketchToolCommand {
  id: string;
  type: "set_sketch_tool";
  payload: {
    tool: "select" | "line" | "rectangle" | "circle";
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

export interface SetSketchLineConstraintCommand {
  id: string;
  type: "set_sketch_line_constraint";
  payload: {
    line_id: string;
    constraint: "none" | "horizontal" | "vertical";
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
    value: number;
  };
}

export interface SelectSketchProfileCommand {
  id: string;
  type: "select_sketch_profile";
  payload: {
    profile_id: string;
  };
}

export interface ExtrudeProfileCommand {
  id: string;
  type: "extrude_profile";
  payload: {
    profile_id: string;
    depth: number;
  };
}

export interface SelectSketchEntityCommand {
  id: string;
  type: "select_sketch_entity";
  payload: {
    entity_id: string;
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

export interface ClearSelectionCommand {
  id: string;
  type: "clear_selection";
  payload: Record<string, never>;
}

export interface ShutdownCommand {
  type: "shutdown";
  payload?: Record<string, never>;
}

export type CoreCommand =
  | PingCommand
  | CreateDocumentCommand
  | GetDocumentStateCommand
  | GetSessionStateCommand
  | GetViewportStateCommand
  | ExportDocumentCommand
  | AddBoxFeatureCommand
  | AddCylinderFeatureCommand
  | UpdateBoxFeatureCommand
  | RenameFeatureCommand
  | DeleteFeatureCommand
  | UndoCommand
  | RedoCommand
  | SelectFeatureCommand
  | SelectReferenceCommand
  | SelectFaceCommand
  | StartSketchOnPlaneCommand
  | StartSketchOnFaceCommand
  | SetSketchToolCommand
  | UpdateSketchLineCommand
  | SetSketchLineConstraintCommand
  | SetSketchEqualLengthConstraintCommand
  | SetSketchPerpendicularConstraintCommand
  | SetSketchParallelConstraintCommand
  | SetSketchCoincidentConstraintCommand
  | UpdateSketchCircleCommand
  | UpdateSketchDimensionCommand
  | SelectSketchProfileCommand
  | ExtrudeProfileCommand
  | AddSketchLineCommand
  | AddSketchRectangleCommand
  | AddSketchCircleCommand
  | SelectSketchEntityCommand
  | SelectSketchDimensionCommand
  | FinishSketchCommand
  | ClearSelectionCommand
  | ShutdownCommand;
