import type { CrosshairMode } from "@/config";
import type {
  ArmedSketchConstraint,
  ConstraintType,
  DocumentState,
  MoveFeatureParameters,
  SelectionFilter,
  SlicerExportFormat,
  SketchTool,
  SolidFacePlaneFrame,
  ViewportState,
} from "@/types";
import type { ArcToolMode } from "./arcDraftPreview";
import type { CircleToolMode } from "./circleDraftPreview";
import type { DimensionToolMode } from "@/types";
import type { MoveGizmoDescriptor } from "./moveGizmo";
import type { RectangleToolMode } from "./rectangleDraftPreview";

export const ORTHO_FRUSTUM_HEIGHT = 220;
export const ORTHO_MIN_ZOOM = 0.02;
export const ORTHO_MAX_ZOOM = 500;
export const WHEEL_ZOOM_SPEED = 0.0012;
export const WHEEL_ZOOM_POINTER_PAN = 0.42;
export const CROSSHAIR_SIZE_FACTORS: Partial<Record<CrosshairMode, number>> = {
  "viewport-25": 0.25,
  "viewport-50": 0.5,
  "viewport-75": 0.75,
};
export const GRID_SNAP_SCREEN_DISTANCE_PX = 6;

// The armed drilling pick's report: a BODY reference (hole rim edge
// or cylindrical wall face — the caller captures an attestation via
// the core) or a bare world point (free pick on any other surface).
export type DrillPickTarget =
  | { mode: "point"; point: { x: number; y: number; z: number } }
  | { mode: "face"; id: string }
  | { mode: "edge"; id: string };

export type PolygonToolMode = "circumscribed" | "inscribed" | "edge";

export interface SketchSelection {
  entityIds: string[];
  vertexIds: string[];
  profileIds: string[];
}

export interface ViewportPanelProps {
  status: "idle" | "starting" | "connected" | "error" | "stopped";
  document: DocumentState | null;
  viewport: ViewportState | null;
  showStock?: boolean;
  // Only the CAM workspace draws the generated toolpath — leaving CAM
  // must not leave the cut path over the CAD model.
  showCamToolpath?: boolean;
  // Drawing workspace: renders ONLY the drawing sheets (no model,
  // stock, toolpath or sketch objects) and fits the camera to the
  // sheet — the workspace-leak discipline.
  showDrawingSheet?: boolean;
  wcsOrientation?: string;
  // CAM setup the viewport renders (WCS marker, stock box, origin
  // snap candidates) — falls back to the first setup.
  activeCamSetupId?: string | null;
  onSnapshotCaptureReady?: (
    capture: (() => Promise<string | null>) | null,
  ) => void;
  onSelectPrimitive: (primitiveId: string) => Promise<void>;
  onSelectReference: (referenceId: string) => Promise<void>;
  onSelectFace: (faceId: string) => Promise<void>;
  // Armed origin pick: while true, every pointer-up places the stock
  // origin at the clicked point.  The click snaps (12 px) to sketch
  // points, body vertices, body edge midpoints, body face centers,
  // and stock-box top corners/midpoints — snapped geometry keeps its
  // 3D z.  With no snap target the click falls back to the bed plane
  // (z = 0) — LightBurn-style, works on sketch-only jobs too.
  // `null` = the click missed the bed entirely (grazing camera
  // angle) — the caller shows a hint instead of placing.
  originPickPointEnabled: boolean;
  onOriginPickPoint: (point: { x: number; y: number; z: number } | null) => void;
  // Armed WCS pick: while true, every pointer-up routes to the WCS
  // pick handler — body faces keep the TNP face anchor, stock faces
  // anchor to the stock face (or a snapped stock corner/edge as a
  // pinned point), anything else snaps like the origin pick or falls
  // back to the bed plane (z = 0).
  wcsPickPointEnabled: boolean;
  onWcsPickPoint: (point: { x: number; y: number; z: number } | null) => void;
  // Armed drilling pick: while true, every pointer-up reports a drill
  // target instead of running scene selection.  A click on a hole
  // (snap dot, rim edge, or cylindrical wall face) reports the BODY
  // reference — the caller captures a re-resolvable attestation;
  // every other click reports the world point (free pick).
  drillPickPointEnabled: boolean;
  onDrillPickPoint: (result: DrillPickTarget | null) => void;
  onSelectEdge: (edgeId: string, additive: boolean) => Promise<void>;
  onSelectVertex: (vertexId: string, additive: boolean) => Promise<void>;
  onStartSketch: (referenceId: string) => Promise<void>;
  onStartSketchOnFace: (
    faceId: string,
    planeFrame: SolidFacePlaneFrame,
  ) => Promise<void>;
  onAddSketchLine: (
    startX: number,
    startY: number,
    endX: number,
    endY: number,
    isConstruction: boolean,
  ) => Promise<void>;
  onSetSketchMidpointAnchor: (
    pointId: string,
    hostLineId: string,
  ) => Promise<void>;
  onSetSketchPointLineAnchor: (
    pointId: string,
    hostLineId: string,
    t: number,
  ) => Promise<void>;
  onAddSketchAngleDimension: (
    firstLineId: string,
    secondLineId: string,
    value?: number,
  ) => Promise<void>;
  onAddSketchDistanceDimension: (
    firstEntityId: string,
    secondEntityId: string,
  ) => Promise<void>;
  onAddSketchLineLengthDimension: (lineId: string) => Promise<void>;
  onAddSketchLineAngleDimension: (lineId: string) => Promise<void>;
  onAddSketchArcRadiusDimension: (arcId: string) => Promise<void>;
  onAddSketchArcLengthDimension: (arcId: string) => Promise<void>;
  onAddSketchCircleRadiusDimension: (
    circleId: string,
    displayAs?: string,
  ) => Promise<void>;
  onAddSketchPolygonRadiusDimension: (polygonId: string) => Promise<void>;
  onSetSketchLineConstraint: (
    lineId: string,
    constraint: "none" | "horizontal" | "vertical",
  ) => Promise<void>;
  onSetSketchPerpendicularConstraint: (
    lineId: string,
    otherLineId: string | null,
  ) => Promise<void>;
  onSetSketchTangentConstraint: (
    lineId: string,
    circleId: string,
  ) => Promise<void>;
  onSetSketchParallelConstraint: (
    lineId: string,
    otherLineId: string | null,
  ) => Promise<void>;
  onAddSketchRectangle: (
    startX: number,
    startY: number,
    endX: number,
    endY: number,
    isConstruction: boolean,
  ) => Promise<void>;
  onAddSketchCircle: (
    centerX: number,
    centerY: number,
    radius: number,
    isConstruction: boolean,
  ) => Promise<void>;
  onAddSketchCircleMode: (
    mode: string,
    isConstruction: boolean,
    inputs: {
      p1?: [number, number];
      p2?: [number, number];
      p3?: [number, number];
      lineAId?: string;
      lineBId?: string;
      lineCId?: string;
      hint?: [number, number];
    },
  ) => Promise<void>;
  onAddSketchArc: (
    startX: number,
    startY: number,
    endX: number,
    endY: number,
    anchorX: number,
    anchorY: number,
    mode: ArcToolMode,
    isConstruction: boolean,
  ) => Promise<void>;
  onAddSketchEllipse: (
    centerX: number,
    centerY: number,
    axisAX: number,
    axisAY: number,
    axisBX: number,
    axisBY: number,
    isConstruction: boolean,
  ) => Promise<void>;
  onAddSketchSpline: (
    points: Array<{ x: number; y: number }>,
    isConstruction: boolean,
  ) => Promise<void>;
  onAddSketchSlot: (
    centerX: number,
    centerY: number,
    length: number,
    radius: number,
    rotation: number,
    isConstruction: boolean,
  ) => Promise<void>;
  onAddSketchChamfer: (
    cornerPointId: string,
    lineAId: string,
    lineBId: string,
  ) => Promise<void>;
  arcToolMode: ArcToolMode;
  onSetArcToolMode: (mode: ArcToolMode) => void;
  rectangleToolMode: RectangleToolMode;
  onSetRectangleToolMode: (mode: RectangleToolMode) => void;
  circleToolMode: CircleToolMode;
  onSetCircleToolMode: (mode: CircleToolMode) => void;
  polygonToolMode: PolygonToolMode;
  onSetPolygonToolMode: (mode: PolygonToolMode) => void;
  dimensionToolMode: DimensionToolMode;
  onSetDimensionToolMode: (mode: DimensionToolMode) => void;
  onAddSketchPolygon: (
    sides: number,
    mode: string,
    startX: number,
    startY: number,
    endX: number,
    endY: number,
    isConstruction: boolean,
  ) => Promise<void>;
  onAddSketchFillet: (
    cornerPointId: string,
    entityAId: string,
    entityAKind: "line" | "arc",
    entityBId: string,
    entityBKind: "line" | "arc",
  ) => Promise<void>;
  // Sketch Text tool: place a new text anchored at the given
  // sketch-local point with core-default parameters. The App opens /
  // rebinds the floating Text panel when the document round-trip
  // lands with the new text id.
  onAddSketchText: (anchorX: number, anchorY: number) => Promise<void>;
  // Select-mode glyph pick: the clicked sketch entity is a text glyph
  // segment (`generated_by: "text:<id>"`). App opens the Text panel
  // bound to the owning text instead of selecting the raw line.
  onPickSketchText: (textId: string) => void;
  onPickSketchSlot: (slotId: string) => void;
  onPickSketchChamfer: (chamferId: string) => void;
  onExtendSketchEntity: (
    entityId: string,
    clickX: number,
    clickY: number,
  ) => Promise<void>;
  // Corner trim: two picked entities are trimmed/extended to their
  // virtual corner.
  onCornerTrimSketchEntities: (
    entityAId: string,
    entityBId: string,
    clickX: number,
    clickY: number,
  ) => Promise<void>;
  // Split: divides the clicked entity (two points for circles/full
  // ellipses).
  onSplitSketchEntity: (
    entityId: string,
    clickX: number,
    clickY: number,
    split2X: number,
    split2Y: number,
  ) => Promise<void>;
  // Drag-paint trim stroke: one batch, one undo entry.
  onTrimSketchStroke: (
    entries: ReadonlyArray<{
      entity_id: string;
      click_x: number;
      click_y: number;
    }>,
  ) => Promise<void>;
  onOffsetSketchEntity: (entityId: string) => Promise<void>;
  // Text-on-path picking: while armed, entity clicks bind the text
  // path instead of placing a new text.
  sketchTextPathPicking: boolean;
  onPickSketchTextPath: (entityId: string) => void;
  onSelectSketchEntity: (entityId: string, additive: boolean) => Promise<void>;
  // Plain sketch-point selection (select_sketch_vertex — NOT the 3D
  // select_vertex, whose ids have a different format).
  onSelectSketchPoint: (vertexId: string, additive: boolean) => Promise<void>;
  // Marquee selection: sketch-local corners + the screen drag
  // direction; the core resolves the exact entity set.
  onSelectSketchRect: (
    x1: number,
    y1: number,
    x2: number,
    y2: number,
    windowMode: boolean,
    additive: boolean,
  ) => Promise<void>;
  onPickSketchPoint: (
    pointId: string,
    kind: "endpoint" | "center" | "quadrant",
    additive: boolean,
  ) => Promise<void>;
  armedSketchConstraint: ArmedSketchConstraint;
  mirrorFocusedSlot: "objects" | "axis" | null;
  inactiveSketchEntityPickEnabled?: boolean;
  onPickInactiveSketchLine?: (lineId: string) => void | Promise<void>;
  onMirrorEntityPick: (
    entityId: string,
    entityKind: "line" | "circle",
  ) => Promise<void>;
  onCancelSketchConstraint: () => void;
  onClearSketchConstraint: (
    kind: ConstraintType,
    entityId: string,
    relatedEntityId: string | null,
  ) => Promise<void>;
  onSelectSketchDimension: (dimensionId: string) => Promise<void>;
  onUpdateSketchDimension: (
    dimensionId: string,
    value: number | string,
  ) => Promise<void>;
  onUpdateSketchDimensionLabelPosition: (
    dimensionId: string,
    labelX: number,
    labelY: number,
  ) => Promise<void>;
  onSelectSketchProfile: (profileId: string, additive: boolean) => Promise<void>;
  onTrimSketchEntity?: (
    entityId: string,
    clickX: number,
    clickY: number,
    segmentIndex?: number,
    expectedRevision?: number,
    previewId?: string,
  ) => Promise<void>;
  onDeleteSketchSelection: (selection?: SketchSelection) => Promise<void>;
  onConfirmDeleteSketchSelection: () => void;
  onDeleteSketchDimension: (dimensionId: string) => Promise<void>;
  // Undo-group bridge (D2): the dimension draft commit spans several
  // commands (entity add + auto-dim deletion + expression update) and
  // collapses them into ONE undo step. begin opens before the add;
  // end closes after the post-commit effects drain.
  onBeginUndoGroup: (name: string) => Promise<void>;
  onEndUndoGroup: () => Promise<void>;
  onToggleSketchDimensionDriven: (dimensionId: string) => Promise<void>;
  onSetSketchLineConstruction: (
    lineId: string,
    isConstruction: boolean,
  ) => Promise<void>;
  onAddSketchVertexDistanceDimension: (
    pointAId: string,
    pointBId: string,
    axis?: "x" | "y",
  ) => Promise<void>;
  onUpdateSketchDimensionDisplay: (
    dimensionId: string,
    displayAs: string,
  ) => Promise<void>;
  onSetSketchTool: (tool: SketchTool) => Promise<void>;
  onOpenTransformArray: () => void;
  // Array/transform center pick: while true the viewport routes the
  // next sketch-plane click through the snap machinery and reports the
  // sketch-local point instead of selecting.
  arrayCenterPicking: boolean;
  onArrayCenterPicked: (local: [number, number]) => void;
  onUpdateSketchPoint: (
    pointId: string,
    x: number,
    y: number,
  ) => Promise<void>;
  onMoveSketchEntities: (params: {
    entityIds: string[];
    dx: number;
    dy: number;
    centerX: number;
    centerY: number;
    angleDeg: number;
  }) => Promise<void>;
  onFinishSketch: () => Promise<void>;
  onClearSelection: () => Promise<void>;
  moveGizmo?: MoveGizmoDescriptor | null;
  onMoveGizmoChange?: (
    parameters: MoveFeatureParameters,
  ) => Promise<void> | void;
  onMoveBody?: (bodyId: string) => Promise<void> | void;
  onCopyBody?: (
    bodyId: string,
    copyMode: "linked" | "standalone",
  ) => Promise<void> | void;
  onExportBodyMesh?: (bodyId: string) => Promise<void> | void;
  onExportBodyStep?: (bodyId: string) => Promise<void> | void;
  onSendBodyToSlicer?: (
    bodyId: string,
    format: SlicerExportFormat,
  ) => Promise<void> | void;
  onUnlinkBodyCopy?: (featureId: string) => Promise<void> | void;
  // Right-click heal for projections: false = delete all projected
  // entities, true = keep them and drop only the live links. Wired
  // from App with the active sketch's feature id.
  onRemoveSketchProjections?: (
    keepGeometry: boolean,
  ) => Promise<void> | void;
  // Show the Remove / Unlink projections entries in the sketch
  // right-click menu (true when the active sketch has projections).
  showSketchProjectionActions?: boolean;
  hiddenFeatureIds?: ReadonlySet<string>;
  hiddenSketchPlaneIds?: ReadonlySet<string>;
  hideReferences?: boolean;
}

export interface ViewportGridVisibilityConfig {
  showGrid: boolean;
  showSketchGrid: boolean;
}

export type UpdateSelectionFilter = (
  patch: Partial<SelectionFilter>,
) => Promise<void>;
