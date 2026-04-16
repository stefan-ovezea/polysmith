export interface BoxFeatureParameters {
  width: number;
  height: number;
  depth: number;
}

export interface CylinderFeatureParameters {
  radius: number;
  height: number;
}

export interface SketchLineEntry {
  line_id: string;
  start_x: number;
  start_y: number;
  end_x: number;
  end_y: number;
  constraint_hint: "horizontal" | "vertical" | null;
}

export interface SketchCircleEntry {
  circle_id: string;
  center_x: number;
  center_y: number;
  radius: number;
}

export interface SketchFeatureParameters {
  plane_id: string;
  active_tool: "select" | "line" | "rectangle" | "circle";
  lines: SketchLineEntry[];
  circles: SketchCircleEntry[];
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
  plane_id: string;
  start: ViewportVector3;
  end: ViewportVector3;
  is_selected: boolean;
  constraint_hint: "horizontal" | "vertical" | null;
}

export interface ViewportSketchCircle {
  circle_id: string;
  plane_id: string;
  center: ViewportVector3;
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
  active_sketch_plane_id: string | null;
  active_sketch_feature_id: string | null;
  active_sketch_tool: "select" | "line" | "rectangle" | "circle" | null;
  selected_sketch_entity_id: string | null;
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
  reference_planes: ViewportReferencePlane[];
  reference_axes: ViewportReferenceAxis[];
  sketch_lines: ViewportSketchLine[];
  sketch_circles: ViewportSketchCircle[];
  scene_width: number;
  scene_height: number;
  scene_depth: number;
  scene_bounds: ViewportSceneBounds;
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

export interface StartSketchOnPlaneCommand {
  id: string;
  type: "start_sketch_on_plane";
  payload: {
    reference_id: string;
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

export interface SelectSketchEntityCommand {
  id: string;
  type: "select_sketch_entity";
  payload: {
    entity_id: string;
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
  | AddBoxFeatureCommand
  | AddCylinderFeatureCommand
  | UpdateBoxFeatureCommand
  | RenameFeatureCommand
  | DeleteFeatureCommand
  | UndoCommand
  | RedoCommand
  | SelectFeatureCommand
  | SelectReferenceCommand
  | StartSketchOnPlaneCommand
  | SetSketchToolCommand
  | AddSketchLineCommand
  | AddSketchRectangleCommand
  | AddSketchCircleCommand
  | SelectSketchEntityCommand
  | FinishSketchCommand
  | ClearSelectionCommand
  | ShutdownCommand;
