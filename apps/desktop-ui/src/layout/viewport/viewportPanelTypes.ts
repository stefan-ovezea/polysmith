import type { CrosshairMode } from "@/config";
import type {
  ArmedSketchConstraint,
  ConstraintType,
  DocumentState,
  MoveFeatureParameters,
  SelectionFilter,
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

export type PolygonToolMode = "circumscribed" | "inscribed" | "edge";

export interface SketchSelection {
  entityIds: string[];
  pointIds: string[];
  profileIds: string[];
}

export interface ViewportPanelProps {
  status: "idle" | "starting" | "connected" | "error" | "stopped";
  document: DocumentState | null;
  viewport: ViewportState | null;
  showStock?: boolean;
  wcsOrientation?: string;
  onSnapshotCaptureReady?: (capture: (() => string | null) | null) => void;
  onSelectPrimitive: (primitiveId: string) => Promise<void>;
  onSelectReference: (referenceId: string) => Promise<void>;
  onSelectFace: (faceId: string) => Promise<void>;
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
    lineAId: string,
    lineBId: string,
  ) => Promise<void>;
  onSelectSketchEntity: (entityId: string, additive: boolean) => Promise<void>;
  onBatchSelectEntities: (
    entityIds: string[],
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
  ) => Promise<void>;
  onDeleteSketchSelection: (selection?: SketchSelection) => Promise<void>;
  onDeleteSketchDimension: (dimensionId: string) => Promise<void>;
  onToggleSketchDimensionDriven: (dimensionId: string) => Promise<void>;
  onSetSketchLineConstruction: (
    lineId: string,
    isConstruction: boolean,
  ) => Promise<void>;
  onAddSketchPointDistanceDimension: (
    pointAId: string,
    pointBId: string,
    axis?: "x" | "y",
  ) => Promise<void>;
  onUpdateSketchDimensionDisplay: (
    dimensionId: string,
    displayAs: string,
  ) => Promise<void>;
  onSetSketchTool: (tool: SketchTool) => Promise<void>;
  onUpdateSketchPoint: (
    pointId: string,
    x: number,
    y: number,
  ) => Promise<void>;
  onFinishSketch: () => Promise<void>;
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
  onUnlinkBodyCopy?: (featureId: string) => Promise<void> | void;
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
