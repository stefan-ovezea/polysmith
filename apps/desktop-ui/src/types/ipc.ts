import type {
  ExtrudeProfileCommand,
  ExtrudeFaceCommand,
  UpdateExtrudeModeCommand,
  UpdateExtrudeTargetBodyCommand,
  UpdateExtrudeParametersCommand,
  UpdateExtrudeProfilesCommand,
  LoftProfilesCommand,
  UpdateLoftProfilesCommand,
  UpdateLoftRuledCommand,
  RevolveProfileCommand,
  UpdateRevolveProfileCommand,
  UpdateRevolveAxisCommand,
  UpdateRevolveAngleCommand,
  SweepProfileCommand,
  UpdateSweepProfileCommand,
  UpdateSweepPathCommand,
} from "./ipc/profileFeatureCommands";
import type {
  StartSketchOnPlaneCommand,
  StartSketchOnFaceCommand,
  AddSketchLineCommand,
  SetSketchLineConstructionCommand,
  SetSketchMidpointAnchorCommand,
  AddSketchAngleDimensionCommand,
  AddSketchDistanceDimensionCommand,
  AddSketchVertexDistanceDimensionCommand,
  AddSketchLineLengthDimensionCommand,
  AddSketchLineAngleDimensionCommand,
  AddSketchArcRadiusDimensionCommand,
  AddSketchArcLengthDimensionCommand,
  AddSketchCircleRadiusDimensionCommand,
  AddSketchPolygonRadiusDimensionCommand,
  SetSketchVertexLineAnchorCommand,
  AddSketchRectangleCommand,
  AddSketchCircleCommand,
  AddSketchPolygonCommand,
  AddSketchArcCommand,
  AddSketchFilletCommand,
  UpdateSketchFilletRadiusCommand,
  DeleteSketchFilletCommand,
  AddSketchChamferCommand,
  UpdateSketchChamferCommand,
  DeleteSketchChamferCommand,
  AddSketchEllipseCommand,
  AddSketchSlotCommand,
  AddSketchSplineCommand,
  UpdateSketchSlotCommand,
  AddSketchTextCommand,
  UpdateSketchTextCommand,
  DeleteSketchTextCommand,
  DeleteSketchDimensionCommand,
  ToggleSketchDimensionDrivenCommand,
  TrimSketchEntityCommand,
  ExtendSketchEntityCommand,
  OffsetSketchEntityCommand,
  TransformSketchEntitiesCommand,
  CreateLinearArrayCommand,
  CreateCircularArrayCommand,
  TrimPreviewCommand,
  DeleteSketchSelectionCommand,
  SetSketchToolCommand,
  UpdateSketchLineCommand,
  UpdateSketchVertexCommand,
  MoveSketchEntitiesCommand,
  SetSketchLineConstraintCommand,
  ClearSketchLineConstraintsCommand,
  SetSketchEqualLengthConstraintCommand,
  SetSketchPerpendicularConstraintCommand,
  StartMirrorPreviewCommand,
  UpdateMirrorPreviewAxisCommand,
  UpdateMirrorPreviewObjectsCommand,
  CommitMirrorPreviewCommand,
  CancelMirrorPreviewCommand,
  SetSketchTangentConstraintCommand,
  SetSketchParallelConstraintCommand,
  SetSketchCoincidentConstraintCommand,
  DeleteSketchCoincidentConstraintCommand,
  SetSketchVertexFixedCommand,
  UpdateSketchCircleCommand,
  UpdateSketchDimensionCommand,
  UpdateSketchDimensionLabelPositionCommand,
  UpdateSketchDimensionDisplayCommand,
  SelectSketchProfileCommand,
  SelectSketchEntityCommand,
  SelectSketchVertexCommand,
  SelectSketchDimensionCommand,
  FinishSketchCommand,
  ReenterSketchCommand,
  ClearSelectionCommand,
} from "./ipc/sketchCommands";
import type {
  AddBoxFeatureCommand,
  AddCylinderFeatureCommand,
  UpdateBoxFeatureCommand,
  UpdateCylinderFeatureCommand,
  CreatePluginFeatureCommand,
  UpdatePluginFeatureCommand,
  ConfirmPluginFeatureCommand,
  UpdateExtrudeDepthCommand,
  SetFeatureSuppressedCommand,
  RenameFeatureCommand,
  DeleteFeatureCommand,
  UndoCommand,
  RedoCommand,
  SetTimelineCursorCommand,
  SelectFeatureCommand,
  SelectReferenceCommand,
  SelectFaceCommand,
  SelectEdgeCommand,
  SelectVertexCommand,
  SetBodyColorCommand,
  SetFaceColorCommand,
  ClearBodyColorCommand,
  ClearFaceColorCommand,
  ClearAppearanceOverridesCommand,
  CreateFilletCommand,
  UpdateFilletRadiusCommand,
  UpdateFilletEdgesCommand,
  UpdateChamferEdgesCommand,
  CreateChamferCommand,
  UpdateChamferDistanceCommand,
  ConfirmFilletCommand,
  ConfirmChamferCommand,
  CreateShellCommand,
  UpdateShellThicknessCommand,
  ConfirmShellCommand,
  CreateOffsetPlaneCommand,
  CreateMidplaneCommand,
  CreateTangentPlaneCommand,
  CreateAnglePlaneCommand,
  CreateConstructionAxisCommand,
  CreateConstructionPointCommand,
  CreateHoleCommand,
  UpdateHoleParametersCommand,
  ConfirmHoleCommand,
  CreateHelixCommand,
  UpdateHelixParametersCommand,
  CreateThreadCommand,
  UpdateThreadParametersCommand,
  ConfirmThreadCommand,
  CreateFastenerCommand,
  UpdateFastenerParametersCommand,
  CreateMoveCommand,
  UpdateMoveParametersCommand,
  ConfirmMoveCommand,
  CreateBodyCopyCommand,
  UnlinkBodyCopyCommand,
  UpdateOffsetPlaneCommand,
  UpdateAnglePlaneCommand,
} from "./ipc/bodyFeatureCommands";
import type {
  CamCaptureFaceReferenceCommand,
  CamMachineSettingsSetCommand,
  CamWcsSetFaceCommand,
  CamSetupCreateCommand,
  CamSetupUpdateCommand,
  CamSetupGetCommand,
  CamStockSetCommand,
  CamStockGetCommand,
  CamToolAddCommand,
  CamToolUpdateCommand,
  CamToolDeleteCommand,
  CamToolListCommand,
  CamOperationCreateCommand,
  CamOperationUpdateCommand,
  CamOperationDeleteCommand,
  CamOperationSetScopeCommand,
  CamPostProcessorSetCommand,
  CamPostListCommand,
  CamPostImportCommand,
  CamOperationGenerateCommand,
  CamOperationPreviewCommand,
  CamExportGcodeCommand,
} from "./ipc/camCommands";
import type {
  FeatureEntry,
  SketchTool,
} from "./geometry/sketch";
import type { CamDocumentData, FaceAttestation } from "./geometry/cam";
import type { SelectionFilter, SelectionFilterUpdate } from "./selectionFilter";
import type {
  ViewportBoxPrimitive,
  ViewportCylinderPrimitive,
  ViewportHelixPrimitive,
  ViewportSketchArc,
  ViewportSketchCircle,
  ViewportSketchConstraint,
  ViewportSketchDimension,
  ViewportSketchEllipse,
  ViewportSketchSpline,
  ViewportSketchLine,
  ViewportSketchVertex,
  ViewportSketchPolygon,
  ViewportSketchProfile,
  ViewportPolygonExtrudePrimitive,
  ViewportReferenceAxis,
  ViewportReferencePlane,
  ViewportReferencePoint,
  ViewportSceneBounds,
  ViewportSolidFace,
} from "./viewport";

export * from "./ipc/bodyFeatureCommands";

export * from "./ipc/profileFeatureCommands";
export * from "./ipc/sketchCommands";
export * from "./ipc/camCommands";

// CAM data — mirrors polysmith::core::CamDocumentData (cam_types.h).
// Detailed CAM types live in types/geometry/cam.ts.
export { type CamDocumentData } from "./geometry/cam";

export interface DocumentState {
  document_id: string;
  name: string;
  units: string;
  revision: number;
  selected_feature_id: string | null;
  selected_reference_id: string | null;
  selected_face_id: string | null;
  selected_edge_ids: string[];
  selected_vertex_ids: string[];
  active_sketch_plane_id: string | null;
  active_sketch_face_id: string | null;
  active_sketch_feature_id: string | null;
  active_sketch_tool: SketchTool | null;
  selected_sketch_vertex_id: string | null;
  selected_sketch_entity_id: string | null;
  selected_sketch_vertex_ids: string[];
  selected_sketch_entity_ids: string[];
  selected_sketch_dimension_id: string | null;
  selected_sketch_profile_id: string | null;
  selected_sketch_profile_ids: string[];
  timeline_cursor: number | null;
  feature_history: FeatureEntry[];
  parameters: ParameterEntry[];
  appearance: DocumentAppearance;
  cam: CamDocumentData;
}

export interface DocumentAppearance {
  body_colors: Array<{
    body_id: string;
    color: string;
  }>;
  face_colors: Array<{
    face_id: string;
    owner_body_id: string;
    signature: string;
    color: string;
  }>;
}

export interface SessionState {
  document_count: number;
  has_active_document: boolean;
  active_document_id: string | null;
  can_undo: boolean;
  can_redo: boolean;
}

export interface SnapCandidateEntry {
  kind: string;
  entity_id: string;
  vertex_id: string;
  local_x: number;
  local_y: number;
  label: string;
}

export interface ViewportToolpathPoint {
  x: number;
  y: number;
  z: number;
  is_rapid: boolean;
  /** The pierce dwell point (laser on + dwell > 0) — rendered as a marker. */
  pierce: boolean;
}

export interface ViewportToolpathPrimitive {
  id: string;
  label: string;
  points: ViewportToolpathPoint[];
}

export interface ViewportState {
  has_active_document: boolean;
  boxes: ViewportBoxPrimitive[];
  cylinders: ViewportCylinderPrimitive[];
  polygon_extrudes: ViewportPolygonExtrudePrimitive[];
  solid_faces: ViewportSolidFace[];
  reference_planes: ViewportReferencePlane[];
  reference_axes: ViewportReferenceAxis[];
  reference_points: ViewportReferencePoint[];
  helices: ViewportHelixPrimitive[];
  sketch_lines: ViewportSketchLine[];
  sketch_circles: ViewportSketchCircle[];
  sketch_ellipses: ViewportSketchEllipse[];
  sketch_splines: ViewportSketchSpline[];
  sketch_polygons: ViewportSketchPolygon[];
  sketch_arcs: ViewportSketchArc[];
  sketch_vertices: ViewportSketchVertex[];
  sketch_dimensions: ViewportSketchDimension[];
  sketch_constraints: ViewportSketchConstraint[];
  sketch_profiles: ViewportSketchProfile[];
  meshes: ViewportMeshPrimitive[];
  cut_previews: ViewportCutPreview[];
  bodies: ViewportBodySummary[];
  edges: ViewportEdgePrimitive[];
  vertices: ViewportVertexPrimitive[];
  toolpaths: ViewportToolpathPrimitive[];
  scene_width: number;
  scene_height: number;
  scene_depth: number;
  scene_bounds: ViewportSceneBounds;
  dof_statuses: Array<{
    entity_id: string;
    entity_kind: string;
    total_dof: number;
    consumed_dof: number;
    status: "under" | "full" | "over";
  }>;
  solver_dofs: number;
  solver_conflicting_count: number;
  solver_redundant_count: number;
  snap_candidates: SnapCandidateEntry[];
  selection_filter: SelectionFilter;
}

export interface ViewportMeshPrimitive {
  primitive_id: string;
  // Triangulated body geometry in world space, laid out as flat arrays
  // for direct upload to a three.js BufferGeometry.
  positions: number[];
  normals: number[];
  indices: number[];
  is_selected: boolean;
  appearance_color: string | null;
}

// Translucent red preview of the cutter volume for the currently-edited
// cut extrude. Emitted by the core only while the user is editing the
// cut (i.e. the corresponding feature is selected). Renders as a red
// translucent overlay so the user sees exactly which volume is about
// to be removed, mirroring common CAD workflow's behavior.
export interface ViewportCutPreview {
  id: string;
  positions: number[];
  normals: number[];
  indices: number[];
}

// Lightweight summary of every body present in the current document, in
// document order. Used by the extrude panel to populate the cut/join
// target picker with stable ids and human-readable labels.
export interface ViewportBodySummary {
  id: string;
  label: string;
  center: { x: number; y: number; z: number };
  size: { x: number; y: number; z: number };
  local_frame: {
    x_axis: { x: number; y: number; z: number };
    y_axis: { x: number; y: number; z: number };
    z_axis: { x: number; y: number; z: number };
  };
}

// Selectable edge of a body, expressed as a flat polyline that the
// renderer can hand straight to a THREE.Line. Edge ids are stable for
// a given body's topology so selection survives mode/depth tweaks.
export interface ViewportEdgePrimitive {
  id: string;
  owner_body_id: string;
  // "line" | "circle" | "curve" — informational only, the renderer
  // treats them all as polylines.
  kind: string;
  // Flat world-space samples: x0, y0, z0, x1, y1, z1, ...
  points: number[];
  // Exact length in millimetres, computed by OCCT in the core. Used
  // by the bottom-right Selection readout when a single edge is
  // selected.
  length: number;
  is_selected: boolean;
}

// Selectable vertex of a body. Same id stability story as edges.
export interface ViewportVertexPrimitive {
  id: string;
  owner_body_id: string;
  position: { x: number; y: number; z: number };
  is_selected: boolean;
}

export interface DocumentExportResult {
  file_path: string;
  format: "step" | "stl" | "gcode";
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

// Emitted by the core while a CAM toolpath is being generated
// (cam_operation_generate / cam_operation_preview). The event echoes the
// command id, so it can arrive interleaved with the final
// document_state reply of the same command.
export interface CamGenerationProgressEvent extends BaseMessage {
  type: "cam_generation_progress";
  id: string;
  payload: {
    op_id: string;
    percent: number;
  };
}

// Reply to cam_capture_face_reference: the TNP-safe face witness
// captured by the core (never fabricated in the UI).
export interface CamFaceAttestationResultEvent extends BaseMessage {
  type: "cam_face_attestation_result";
  id: string;
  payload: {
    persistent_id: string;
    attestation: FaceAttestation;
  };
}

// Reply to cam_post_list / cam_post_import: every available post
// processor (built-ins + files in the user's posts directory).
export interface CamPostListResultEvent extends BaseMessage {
  type: "cam_post_list_result";
  id: string;
  payload: {
    posts: Array<{ name: string; path: string }>;
  };
}

export interface DocumentSavedEvent extends BaseMessage {
  type: "document_saved";
  id: string;
  payload: {
    file_path: string;
  };
}

export interface ErrorEvent extends BaseMessage {
  type: "error";
  id?: string;
  payload: {
    code: string;
    message: string;
  };
}

export type LogLevel = "debug" | "info" | "warn" | "error";

export interface LogEvent extends BaseMessage {
  type: "log";
  payload: LogEntry;
}

export interface TrimPreviewResultEvent {
  id: string;
  type: "trim_preview_result";
  payload: {
    entity_id: string;
    entity_kind: "line" | "circle" | "arc" | "ellipse" | "spline";
    hovered_index: number;
    /** Document revision the preview was computed against. */
    revision: number;
    full_circle?: boolean;
    full_arc?: boolean;
    full_ellipse?: boolean;
    full_spline?: boolean;
    segments?: Array<{
      start?: [number, number];
      end?: [number, number];
      param_start?: number;
      param_end?: number;
    }>;
  } | null;
}

export interface LogEntry {
  level: LogLevel;
  source: string;
  message: string;
  timestamp: string;
}

export type CoreMessage =
  | HelloEvent
  | PongEvent
  | DocumentCreatedEvent
  | DocumentStateEvent
  | SessionStateEvent
  | ViewportStateEvent
  | DocumentExportedEvent
  | DocumentSavedEvent
  | LogEvent
  | TrimPreviewResultEvent
  | CamGenerationProgressEvent
  | CamPostListResultEvent
  | CamFaceAttestationResultEvent
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

export interface ExportDocumentStlCommand {
  id: string;
  type: "export_document_stl";
  payload: {
    file_path: string;
  };
}

export interface ExportDocumentDxfCommand {
  id: string;
  type: "export_document_dxf";
  payload: {
    file_path: string;
  };
}

export interface ExportDocumentIgesCommand {
  id: string;
  type: "export_document_iges";
  payload: {
    file_path: string;
  };
}

export interface ExportBodyStlCommand {
  id: string;
  type: "export_body_stl";
  payload: {
    file_path: string;
    body_id: string;
  };
}

export interface SaveDocumentCommand {
  id: string;
  type: "save_document";
  payload: {
    file_path: string;
  };
}

export interface LoadDocumentCommand {
  id: string;
  type: "load_document";
  payload: {
    file_path: string;
  };
}

export interface ImportStlCommand {
  id: string;
  type: "import_stl";
  payload: {
    file_path: string;
    scale?: number;
  };
}

export interface ImportDxfCommand {
  id: string;
  type: "import_dxf";
  payload: {
    file_path: string;
    plane_id?: string;
  };
}

export interface ImportStepCommand {
  id: string;
  type: "import_step";
  payload: {
    file_path: string;
  };
}

export interface ImportIgesCommand {
  id: string;
  type: "import_iges";
  payload: {
    file_path: string;
  };
}

export interface ConvertMeshToBodyCommand {
  id: string;
  type: "convert_mesh_to_body";
  payload: {
    body_id: string;
  };
}

export interface DetachBodyProjectionsCommand {
  id: string;
  type: "detach_body_projections";
  payload: {
    body_id: string;
  };
}

export interface ProjectBodyIntoSketchCommand {
  id: string;
  type: "project_body_into_sketch";
  payload: {
    body_id: string;
    mode: "section" | "silhouette";
  };
}

export interface ProjectFaceIntoSketchCommand {
  id: string;
  type: "project_face_into_sketch";
  payload: {
    face_id: string;
  };
}

export interface ProjectProfileIntoSketchCommand {
  id: string;
  type: "project_profile_into_sketch";
  payload: {
    profile_id: string;
  };
}

export interface ProjectEdgeIntoSketchCommand {
  id: string;
  type: "project_edge_into_sketch";
  payload: {
    edge_id: string;
  };
}

export interface ProjectVertexIntoSketchCommand {
  id: string;
  type: "project_vertex_into_sketch";
  payload: {
    vertex_id: string;
  };
}

export interface ParameterEntry {
  name: string;
  expression: string;
  resolved_value: number;
  kind: "length" | "angle";
  has_error: boolean;
  error_message: string;
}

export interface AddParameterCommand {
  id: string;
  type: "add_parameter";
  payload: {
    name: string;
    expression: string;
    kind?: "length" | "angle";
  };
}

export interface UpdateParameterCommand {
  id: string;
  type: "update_parameter";
  payload: {
    name: string;
    expression: string;
    kind?: "length" | "angle";
  };
}

export interface DeleteParameterCommand {
  id: string;
  type: "delete_parameter";
  payload: {
    name: string;
  };
}

export interface UpdateSelectionFilterCommand {
  id: string;
  type: "update_selection_filter";
  payload: SelectionFilterUpdate;
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
  | ExportDocumentStlCommand
  | ExportDocumentDxfCommand
  | ExportDocumentIgesCommand
  | ExportBodyStlCommand
  | SaveDocumentCommand
  | LoadDocumentCommand
  | ImportStlCommand
  | ImportDxfCommand
  | ImportStepCommand
  | ImportIgesCommand
  | ConvertMeshToBodyCommand
  | CamMachineSettingsSetCommand
  | CamCaptureFaceReferenceCommand
  | CamWcsSetFaceCommand
  | CamSetupCreateCommand
  | CamSetupUpdateCommand
  | CamSetupGetCommand
  | CamStockSetCommand
  | CamStockGetCommand
  | CamToolAddCommand
  | CamToolUpdateCommand
  | CamToolDeleteCommand
  | CamToolListCommand
  | CamOperationCreateCommand
  | CamOperationUpdateCommand
  | CamOperationDeleteCommand
  | CamOperationSetScopeCommand
  | CamPostProcessorSetCommand
  | CamPostListCommand
  | CamPostImportCommand
  | CamOperationGenerateCommand
  | CamOperationPreviewCommand
  | CamExportGcodeCommand
  | DetachBodyProjectionsCommand
  | ProjectFaceIntoSketchCommand
  | ProjectProfileIntoSketchCommand
  | ProjectEdgeIntoSketchCommand
  | ProjectVertexIntoSketchCommand
  | ProjectBodyIntoSketchCommand
  | AddBoxFeatureCommand
  | AddCylinderFeatureCommand
  | UpdateBoxFeatureCommand
  | UpdateCylinderFeatureCommand
  | CreatePluginFeatureCommand
  | UpdatePluginFeatureCommand
  | ConfirmPluginFeatureCommand
  | UpdateExtrudeDepthCommand
  | UpdateExtrudeModeCommand
  | UpdateExtrudeTargetBodyCommand
  | UpdateExtrudeParametersCommand
  | UpdateExtrudeProfilesCommand
  | LoftProfilesCommand
  | UpdateLoftProfilesCommand
  | UpdateLoftRuledCommand
  | RevolveProfileCommand
  | UpdateRevolveProfileCommand
  | UpdateRevolveAxisCommand
  | UpdateRevolveAngleCommand
  | SweepProfileCommand
  | UpdateSweepProfileCommand
  | UpdateSweepPathCommand
  | RenameFeatureCommand
  | SetFeatureSuppressedCommand
  | DeleteFeatureCommand
  | UndoCommand
  | RedoCommand
  | SetTimelineCursorCommand
  | SelectFeatureCommand
  | SelectReferenceCommand
  | SelectFaceCommand
  | SelectEdgeCommand
  | SelectVertexCommand
  | SetBodyColorCommand
  | SetFaceColorCommand
  | ClearBodyColorCommand
  | ClearFaceColorCommand
  | ClearAppearanceOverridesCommand
  | CreateFilletCommand
  | UpdateFilletRadiusCommand
  | UpdateFilletEdgesCommand
  | ConfirmFilletCommand
  | CreateChamferCommand
  | UpdateChamferDistanceCommand
  | UpdateChamferEdgesCommand
  | ConfirmChamferCommand
  | CreateShellCommand
  | UpdateShellThicknessCommand
  | ConfirmShellCommand
  | CreateOffsetPlaneCommand
  | CreateMidplaneCommand
  | CreateTangentPlaneCommand
  | CreateAnglePlaneCommand
  | CreateConstructionAxisCommand
  | CreateConstructionPointCommand
  | CreateHoleCommand
  | UpdateHoleParametersCommand
  | ConfirmHoleCommand
  | CreateHelixCommand
  | UpdateHelixParametersCommand
  | CreateThreadCommand
  | UpdateThreadParametersCommand
  | ConfirmThreadCommand
  | CreateFastenerCommand
  | UpdateFastenerParametersCommand
  | CreateMoveCommand
  | UpdateMoveParametersCommand
  | ConfirmMoveCommand
  | CreateBodyCopyCommand
  | UnlinkBodyCopyCommand
  | UpdateOffsetPlaneCommand
  | UpdateAnglePlaneCommand
  | StartSketchOnPlaneCommand
  | StartSketchOnFaceCommand
  | SetSketchToolCommand
  | UpdateSketchLineCommand
  | UpdateSketchVertexCommand
  | MoveSketchEntitiesCommand
  | SetSketchLineConstraintCommand
  | ClearSketchLineConstraintsCommand
  | SetSketchEqualLengthConstraintCommand
  | SetSketchPerpendicularConstraintCommand
  | SetSketchTangentConstraintCommand
  | StartMirrorPreviewCommand
  | UpdateMirrorPreviewAxisCommand
  | UpdateMirrorPreviewObjectsCommand
  | CommitMirrorPreviewCommand
  | CancelMirrorPreviewCommand
  | SetSketchParallelConstraintCommand
  | SetSketchCoincidentConstraintCommand
  | DeleteSketchCoincidentConstraintCommand
  | SetSketchVertexFixedCommand
  | UpdateSketchCircleCommand
  | UpdateSketchDimensionCommand
  | UpdateSketchDimensionLabelPositionCommand
  | UpdateSketchDimensionDisplayCommand
  | SelectSketchProfileCommand
  | AddSketchDistanceDimensionCommand
  | AddSketchVertexDistanceDimensionCommand
  | ExtrudeProfileCommand
  | ExtrudeFaceCommand
  | AddSketchLineCommand
  | SetSketchLineConstructionCommand
  | SetSketchMidpointAnchorCommand
  | SetSketchVertexLineAnchorCommand
  | AddSketchAngleDimensionCommand
  | AddSketchLineLengthDimensionCommand
  | AddSketchLineAngleDimensionCommand
  | AddSketchArcRadiusDimensionCommand
  | AddSketchArcLengthDimensionCommand
  | AddSketchCircleRadiusDimensionCommand
  | AddSketchPolygonRadiusDimensionCommand
  | AddSketchRectangleCommand
  | AddSketchCircleCommand
  | AddSketchPolygonCommand
  | AddSketchArcCommand
  | AddSketchFilletCommand
  | UpdateSketchFilletRadiusCommand
  | DeleteSketchFilletCommand
  | AddSketchChamferCommand
  | UpdateSketchChamferCommand
  | DeleteSketchChamferCommand
  | AddSketchEllipseCommand
  | AddSketchSlotCommand
  | AddSketchSplineCommand
  | UpdateSketchSlotCommand
  | AddSketchTextCommand
  | UpdateSketchTextCommand
  | DeleteSketchTextCommand
  | DeleteSketchDimensionCommand
  | ToggleSketchDimensionDrivenCommand
  | TrimSketchEntityCommand
  | ExtendSketchEntityCommand
  | OffsetSketchEntityCommand
  | TransformSketchEntitiesCommand
  | CreateLinearArrayCommand
  | CreateCircularArrayCommand
  | TrimPreviewCommand
  | DeleteSketchSelectionCommand
  | SelectSketchVertexCommand
  | SelectSketchEntityCommand
  | SelectSketchDimensionCommand
  | FinishSketchCommand
  | ReenterSketchCommand
  | ClearSelectionCommand
  | AddParameterCommand
  | UpdateParameterCommand
  | DeleteParameterCommand
  | UpdateSelectionFilterCommand
  | ShutdownCommand;
