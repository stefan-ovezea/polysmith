import {
  useEffect,
  useMemo,
  useRef,
  useState,
  type MutableRefObject,
  type KeyboardEvent as ReactKeyboardEvent,
} from "react";
import { useTranslation } from "react-i18next";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import {
  applyTheme,
  formatHotkey,
  matchesHotkey,
  useAppConfig,
} from "@/config";
import { Checkbox, Dropdown, ToolbarTooltip } from "@/lib";
import type { CrosshairMode } from "@/config";
import { createViewportScene } from "@/lib";
import type {
  ArmedSketchConstraint,
  ConstraintType,
  DocumentState,
  SketchTool,
  ViewportState,
  SketchDimensionScene,
  SolidFacePlaneFrame,
  PrimitiveVisual,
  PrimitiveInteractionState,
  ReferencePlaneVisual,
  ReferencePlaneInteractionState,
  SolidFaceVisual,
  SolidFaceInteractionState,
  SketchProfileVisual,
  SketchProfileInteractionState,
  ViewportContextMenuState,
  SketchPreviewPoint,
  SketchProfileScene,
  SketchLineEntry,
  SketchCircleEntry,
  MoveFeatureParameters,
} from "@/types";
import type { SelectionFilter } from "./SelectionFilterPanel";
import {
  applyPrimitiveVisualState,
  applyReferencePlaneVisualState,
  applySketchProfileVisualState,
  applySolidFaceVisualState,
  buildPrimitiveObject,
  buildReferenceAxisObject,
  buildReferenceHelixObject,
  buildReferencePlaneObject,
  buildReferencePointObject,
  buildSketchArcObject,
  buildSketchCircleObject,
  buildSketchConstraintObject,
  buildSketchDimensionObject,
  buildSketchLineObject,
  buildSketchPointObject,
  buildSketchPolygonObject,
  buildSketchProfileObject,
  buildSolidFaceObject,
  buildCutPreviewObject,
  buildSceneEdgeObject,
  applyEdgeVisualColor,
  applyVertexVisualColor,
  disposeGroup,
  disposeMaterial,
  distanceBetweenPoints,
  frameCamera,
  frameCameraToSketchPlane,
  projectWorldPointToViewport,
  resolveSketchPlanePoint,
  SKETCH_PLANE_OFFSET,
  SKETCH_SNAP_DISTANCE,
  themeColor,
  toWorldPoint,
  buildViewCubeGroup,
  createViewCubeScene,
  createViewCubeCamera,
  getCubeViewportRect,
  isPointerInCubeArea,
  syncCubeCamera,
  updateSketchRotationArrows,
  raycastViewCube,
  getCubeHitTargetDirection,
  getQuantizedCubeUp,
  isCardinalCubeDirection,
  animateCameraTowardTarget,
  applyCubeHover,
  clearCubeHover,
  applyCubeDragOrbit,
  disposeViewCubeGroup,
  lineLineIntersectionTrim,
  lineCircleIntersectionTrim,
} from "@/utils";
import { parseDimensionInput, mmToDisplay } from "@/utils/units";
import type { ViewCubeHit } from "@/utils";

type DynamicGridRef = {
  key: string;
  group: THREE.Group;
};

type GridPlaneFrame = {
  origin: THREE.Vector3;
  xAxis: THREE.Vector3;
  yAxis: THREE.Vector3;
  normal: THREE.Vector3;
};
type GridPlaneBounds = {
  minU: number;
  maxU: number;
  minV: number;
  maxV: number;
};
interface MoveGizmoDescriptor {
  bodyId: string;
  center: { x: number; y: number; z: number };
  size: { x: number; y: number; z: number };
  localFrame: {
    x_axis: { x: number; y: number; z: number };
    y_axis: { x: number; y: number; z: number };
    z_axis: { x: number; y: number; z: number };
  };
  parameters: MoveFeatureParameters;
  disabled: boolean;
}
type MoveGizmoAxis = "x" | "y" | "z";
type MoveGizmoDragState = {
  kind: "translate" | "rotate" | "free";
  axis: MoveGizmoAxis | null;
  startClientX: number;
  startClientY: number;
  startAngle: number;
  center: THREE.Vector3;
  axes: Record<MoveGizmoAxis, THREE.Vector3>;
  handleLength: number;
  parameters: MoveFeatureParameters;
};
type ActiveSketchGridPlaneFrame = NonNullable<
  NonNullable<
    DocumentState["feature_history"][number]["sketch_parameters"]
  >["plane_frame"]
>;
type DraftDimensionTool = "line" | "rectangle" | "circle" | "polygon";
type DraftDimensionField = "length" | "width" | "diameter" | "radius" | "angle";
type DraftDimensionSession = {
  tool: DraftDimensionTool;
  start: [number, number];
  current: [number, number];
  values: Record<DraftDimensionField, string>;
  activeField: DraftDimensionField;
  lockedFields: Partial<Record<DraftDimensionField, boolean>>;
};
type ParameterSuggestion = {
  name: string;
  expression: string;
  kind: "length" | "angle";
  value: number;
};

function parameterTokenAtCursor(value: string, cursor: number) {
  const before = value.slice(0, cursor);
  const after = value.slice(cursor);
  const startMatch = before.match(/[A-Za-z_][A-Za-z0-9_]*$/);
  if (!startMatch) {
    return null;
  }
  const endMatch = after.match(/^[A-Za-z0-9_]*/);
  const start = cursor - startMatch[0].length;
  const end = cursor + (endMatch?.[0].length ?? 0);
  return { query: value.slice(start, cursor), start, end };
}

function fuzzyParameterScore(query: string, candidate: string) {
  const normalizedQuery = query.toLowerCase();
  const normalizedCandidate = candidate.toLowerCase();
  if (!normalizedQuery) {
    return 1;
  }
  if (normalizedCandidate === normalizedQuery) {
    return 1000;
  }
  if (normalizedCandidate.startsWith(normalizedQuery)) {
    return 800 - (normalizedCandidate.length - normalizedQuery.length);
  }
  if (normalizedCandidate.includes(normalizedQuery)) {
    return 600 - normalizedCandidate.indexOf(normalizedQuery);
  }

  let score = 0;
  let candidateIndex = 0;
  let previousMatch = -1;
  for (const char of normalizedQuery) {
    const found = normalizedCandidate.indexOf(char, candidateIndex);
    if (found < 0) {
      return 0;
    }
    score += previousMatch >= 0 && found === previousMatch + 1 ? 12 : 4;
    if (found === 0 || /[_\-\s]/.test(candidate[found - 1] ?? "")) {
      score += 8;
    }
    previousMatch = found;
    candidateIndex = found + 1;
  }
  return score - normalizedCandidate.length * 0.1;
}

function GridMiniIcon() {
  return (
    <svg
      aria-hidden="true"
      className="h-[18px] w-[18px]"
      viewBox="0 0 16 16"
      fill="none"
    >
      <path
        d="M3 2.5V13.5M8 2.5V13.5M13 2.5V13.5M2.5 3H13.5M2.5 8H13.5M2.5 13H13.5"
        stroke="currentColor"
        strokeWidth="1.35"
        strokeLinecap="round"
      />
    </svg>
  );
}

type DimensionLabelDragState = {
  dimensionId: string;
  startClientX: number;
  startClientY: number;
  startWorld: [number, number, number];
  startLabelPosition: [number, number, number];
  dragAxis: [number, number, number];
  hasMoved: boolean;
  isPlacement?: boolean;
  hitPart?: "label" | "geometry";
};

type DimensionRelationPreview = {
  kind:
    | "parallel_line_distance"
    | "line_angle"
    | "circle_line_distance"
    | "circle_center_distance";
  firstEntityId: string;
  targetEntityId: string;
};

const ANGLE_DIMENSION_MIN_RADIUS = 6;
const ANGLE_DIMENSION_MAX_RADIUS = 80;

const GRID_STEPS_MM = [
  0.1, 0.5, 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000,
];
const GRID_MIN_HALF_LINE_COUNT = 80;
const GRID_SKETCH_PADDING_MULTIPLIER = 2;
const GRID_WORLD_PADDING_MULTIPLIER = 6;
const GRID_MAJOR_EVERY = 10;
const GRID_CAMERA_SCALE = 40;
const SKETCH_GRID_BACK_OFFSET = 0.015;
const SKETCH_SCREEN_SPRITE_BASE_HEIGHT = 900;
const SKETCH_LABEL_SCREEN_SCALE = 0.72;
const SKETCH_CONSTRAINT_SCREEN_SIZE = 34;
const SKETCH_LABEL_COLLISION_PADDING = 6;
const ORTHO_FRUSTUM_HEIGHT = 220;
const ORTHO_MIN_ZOOM = 0.02;
const ORTHO_MAX_ZOOM = 500;
const WHEEL_ZOOM_SPEED = 0.0012;
const WHEEL_ZOOM_POINTER_PAN = 0.42;
const CROSSHAIR_SIZE_FACTORS: Partial<Record<CrosshairMode, number>> = {
  "viewport-25": 0.25,
  "viewport-50": 0.5,
  "viewport-75": 0.75,
};
const CARDINAL_VIEW_DOT_THRESHOLD = 0.985;
const DRAFT_DIMENSION_OFFSET_PX = 36;
const GRID_SNAP_SCREEN_DISTANCE_PX = 6;

interface ViewportPanelProps {
  status: "idle" | "starting" | "connected" | "error" | "stopped";
  document: DocumentState | null;
  viewport: ViewportState | null;
  onSnapshotCaptureReady?: (capture: (() => string | null) | null) => void;
  onSelectPrimitive: (primitiveId: string) => Promise<void>;
  onSelectReference: (referenceId: string) => Promise<void>;
  onSelectFace: (faceId: string) => Promise<void>;
  // `additive` is true when the user shift-clicked the edge: the
  // core toggles the edge into the existing selection rather than
  // replacing it. Other selection categories don't support multi
  // yet, so they keep their single-id callbacks.
  onSelectEdge: (edgeId: string, additive: boolean) => Promise<void>;
  // Same multi-select shape as `onSelectEdge`: shift-click toggles
  // the vertex into the existing vertex set (used by the bottom-right
  // Selection panel to show vertex-vertex distance), plain click
  // replaces.
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
  ) => Promise<void>;
  onAddSketchDistanceDimension: (
    firstEntityId: string,
    secondEntityId: string,
  ) => Promise<void>;
  onAddSketchLineLengthDimension: (lineId: string) => Promise<void>;
  onAddSketchCircleRadiusDimension: (circleId: string, displayAs?: string) => Promise<void>;
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
  // Add an arc using one of two creation modes:
  //   - "three_point": (start, end, anchor) where anchor lies on the
  //     arc and fixes the bulge.
  //   - "center_start_end": anchor is the center; the end is snapped
  //     onto the resulting circle in the core.
  // Both modes accept the same three (x, y) pairs in sketch-local
  // 2D coordinates; the ViewportPanel resolves them from world clicks.
  onAddSketchArc: (
    startX: number,
    startY: number,
    endX: number,
    endY: number,
    anchorX: number,
    anchorY: number,
    mode: "three_point" | "center_start_end",
    isConstruction: boolean,
  ) => Promise<void>;
  // Tool-level mode for the arc tool. Lifted out of ViewportPanel so
  // the SketchToolbar can render the segmented control. Defaults to
  // "three_point"; the toolbar updates it through `onSetArcToolMode`.
  arcToolMode: "three_point" | "center_start_end";
  onSetArcToolMode: (mode: "three_point" | "center_start_end") => void;
  // Rectangle creation mode — corner-to-corner, center-point, or 3-point.
  // Lifted from App.tsx so the viewport commit handler can compute
  // the rectangle corners differently per mode.
  rectangleToolMode: "corner_corner" | "center_point" | "three_point";
  // Circle creation mode — center+radius, 2-point, 3-point, or tangent.
  // Lifted from App.tsx so the viewport handler can compute the
  // circle geometry differently per mode.
  circleToolMode: "center_radius" | "two_point" | "three_point" | "tangent_two_lines" | "tangent_three_lines";
  onSetCircleToolMode: (mode: "center_radius" | "two_point" | "three_point" | "tangent_two_lines" | "tangent_three_lines") => void;
  onSetRectangleToolMode: (mode: "corner_corner" | "center_point" | "three_point") => void;
  polygonToolMode: "circumscribed" | "inscribed" | "edge";
  onSetPolygonToolMode: (mode: "circumscribed" | "inscribed" | "edge") => void;
  onAddSketchPolygon: (
    sides: number,
    mode: string,
    startX: number,
    startY: number,
    endX: number,
    endY: number,
    isConstruction: boolean,
  ) => Promise<void>;
  // Sketch fillet — fired when the user clicks an eligible corner
  // point under the Fillet tool. Eligible = sketch point shared by
  // exactly two non-construction sketch lines that are not already
  // filleted at this corner. The viewport only signals "user
  // clicked an eligible corner with these args"; App owns the
  // session radius (driven by the floating panel) and decides
  // what to do with the click. Mirrors the 3D edge-op flow where
  // the viewport reports edge picks and the panel owns the
  // numeric value being applied to all of them.
  onAddSketchFillet: (
    cornerPointId: string,
    lineAId: string,
    lineBId: string,
  ) => Promise<void>;
  onSelectSketchEntity: (entityId: string, additive: boolean) => Promise<void>;
  onBatchSelectEntities: (entityIds: string[], additive: boolean) => Promise<void>;
  onPickSketchPoint: (
    pointId: string,
    kind: "endpoint" | "center" | "quadrant",
    additive: boolean,
  ) => Promise<void>;
  armedSketchConstraint: ArmedSketchConstraint;
  // Which mirror tool slot is taking entity clicks. `null` when the
  // mirror tool isn't open. The mirror tool runs alongside the
  // armed-constraint flow but takes priority when active: a click
  // on a sketch entity is routed through `onMirrorEntityPick`
  // instead of the normal selection / armed-constraint paths.
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
  onDeleteSketchSelection: (
    selection?: {
      entityIds: string[];
      pointIds: string[];
      profileIds: string[];
    },
  ) => Promise<void>;
  onDeleteSketchDimension: (dimensionId: string) => Promise<void>;
  onAddSketchPointDistanceDimension: (
    pointAId: string,
    pointBId: string,
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
  onMoveGizmoChange?: (parameters: MoveFeatureParameters) => Promise<void> | void;
  onMoveBody?: (bodyId: string) => Promise<void> | void;
  onCopyBody?: (
    bodyId: string,
    copyMode: "linked" | "standalone",
  ) => Promise<void> | void;
  onUnlinkBodyCopy?: (featureId: string) => Promise<void> | void;
  hiddenFeatureIds?: ReadonlySet<string>;
  hiddenSketchPlaneIds?: ReadonlySet<string>;
  hideReferences?: boolean;
}

function snapGridCenter(value: number, spacing: number): number {
  return Math.round(value / spacing) * spacing;
}

function isGridMajorLine(value: number, spacing: number): boolean {
  const majorSpacing = spacing * GRID_MAJOR_EVERY;
  return (
    Math.abs(value / majorSpacing - Math.round(value / majorSpacing)) < 1e-5
  );
}

function pushGridLine(
  positions: number[],
  colors: number[],
  start: THREE.Vector3,
  end: THREE.Vector3,
  color: THREE.Color,
): void {
  positions.push(start.x, start.y, start.z, end.x, end.y, end.z);
  colors.push(color.r, color.g, color.b, color.r, color.g, color.b);
}

function buildDynamicGrid(
  frame: GridPlaneFrame,
  spacing: number,
  bounds: GridPlaneBounds,
  minorColor: THREE.Color,
  majorColor: THREE.Color,
  axisColor: THREE.Color,
  opacity: number,
): THREE.LineSegments {
  const positions: number[] = [];
  const colors: number[] = [];
  const start = new THREE.Vector3();
  const end = new THREE.Vector3();

  for (let u = bounds.minU; u <= bounds.maxU + spacing * 0.5; u += spacing) {
    const uColor =
      Math.abs(u) < spacing * 0.25
        ? axisColor
        : isGridMajorLine(u, spacing)
          ? majorColor
          : minorColor;

    start
      .copy(frame.origin)
      .addScaledVector(frame.xAxis, u)
      .addScaledVector(frame.yAxis, bounds.minV);
    end
      .copy(frame.origin)
      .addScaledVector(frame.xAxis, u)
      .addScaledVector(frame.yAxis, bounds.maxV);
    pushGridLine(positions, colors, start, end, uColor);
  }

  for (let v = bounds.minV; v <= bounds.maxV + spacing * 0.5; v += spacing) {
    const vColor =
      Math.abs(v) < spacing * 0.25
        ? axisColor
        : isGridMajorLine(v, spacing)
          ? majorColor
          : minorColor;

    start
      .copy(frame.origin)
      .addScaledVector(frame.xAxis, bounds.minU)
      .addScaledVector(frame.yAxis, v);
    end
      .copy(frame.origin)
      .addScaledVector(frame.xAxis, bounds.maxU)
      .addScaledVector(frame.yAxis, v);
    pushGridLine(positions, colors, start, end, vColor);
  }

  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute(
    "position",
    new THREE.Float32BufferAttribute(positions, 3),
  );
  geometry.setAttribute("color", new THREE.Float32BufferAttribute(colors, 3));
  const material = new THREE.LineBasicMaterial({
    vertexColors: true,
    transparent: true,
    opacity,
    depthWrite: false,
  });
  return new THREE.LineSegments(geometry, material);
}

function disposeDynamicGrid(grid: DynamicGridRef | null): void {
  if (!grid) {
    return;
  }
  disposeGroup(grid.group);
}

function moveGizmoAxisVector(
  axis: { x: number; y: number; z: number },
): THREE.Vector3 {
  const vector = new THREE.Vector3(axis.x, axis.y, axis.z);
  return vector.lengthSq() > 1.0e-12 ? vector.normalize() : new THREE.Vector3(1, 0, 0);
}

function moveGizmoAxes(
  gizmo: MoveGizmoDescriptor,
): Record<MoveGizmoAxis, THREE.Vector3> {
  return {
    x: moveGizmoAxisVector(gizmo.localFrame.x_axis),
    y: moveGizmoAxisVector(gizmo.localFrame.y_axis),
    z: moveGizmoAxisVector(gizmo.localFrame.z_axis),
  };
}

function orientObjectAlongAxis(object: THREE.Object3D, axis: THREE.Vector3) {
  object.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), axis);
}

function orientRingToAxis(object: THREE.Object3D, axis: THREE.Vector3) {
  object.quaternion.setFromUnitVectors(new THREE.Vector3(0, 0, 1), axis);
}

function buildMoveGizmoObject(gizmo: MoveGizmoDescriptor) {
  const group = new THREE.Group();
  const pickables: THREE.Object3D[] = [];
  const center = new THREE.Vector3(
    gizmo.center.x,
    gizmo.center.y,
    gizmo.center.z,
  );
  const axes = moveGizmoAxes(gizmo);
  const maxSize = Math.max(gizmo.size.x, gizmo.size.y, gizmo.size.z, 12);
  const handleLength = Math.min(Math.max(maxSize * 0.65, 18), 80);
  const ringRadius = handleLength * 0.55;
  const axisColors: Record<MoveGizmoAxis, string> = {
    x: themeColor("--color-axis-x", "#ff6b7a"),
    y: themeColor("--color-axis-y", "#2bd978"),
    z: themeColor("--color-axis-z", "#6db4ff"),
  };
  const handleRadius = Math.max(handleLength * 0.018, 0.28);

  const freeMaterial = new THREE.MeshBasicMaterial({
    color: themeColor("--color-primary-glow", "#00e5ff"),
    transparent: true,
    opacity: 0.9,
    depthTest: false,
    depthWrite: false,
  });
  const centerHandle = new THREE.Mesh(
    new THREE.SphereGeometry(Math.max(handleRadius * 3.2, 2.2), 20, 12),
    freeMaterial,
  );
  centerHandle.position.copy(center);
  centerHandle.renderOrder = 50;
  centerHandle.userData.moveGizmoHandle = { kind: "free" };
  group.add(centerHandle);
  pickables.push(centerHandle);

  (["x", "y", "z"] as const).forEach((axisKey) => {
    const axis = axes[axisKey];
    const color = axisColors[axisKey];
    const axisMaterial = new THREE.MeshBasicMaterial({
      color,
      transparent: true,
      opacity: 0.92,
      depthTest: false,
      depthWrite: false,
    });
    const shaft = new THREE.Mesh(
      new THREE.CylinderGeometry(handleRadius, handleRadius, handleLength, 12),
      axisMaterial,
    );
    orientObjectAlongAxis(shaft, axis);
    shaft.position.copy(center).addScaledVector(axis, handleLength * 0.5);
    shaft.renderOrder = 50;
    shaft.userData.moveGizmoHandle = { kind: "translate", axis: axisKey };
    group.add(shaft);
    pickables.push(shaft);

    const arrow = new THREE.Mesh(
      new THREE.ConeGeometry(handleRadius * 3.4, handleRadius * 9, 20),
      axisMaterial,
    );
    orientObjectAlongAxis(arrow, axis);
    arrow.position.copy(center).addScaledVector(axis, handleLength + handleRadius * 4.5);
    arrow.renderOrder = 50;
    arrow.userData.moveGizmoHandle = { kind: "translate", axis: axisKey };
    group.add(arrow);
    pickables.push(arrow);

    const ringMaterial = new THREE.MeshBasicMaterial({
      color,
      transparent: true,
      opacity: 0.72,
      depthTest: false,
      depthWrite: false,
    });
    const ring = new THREE.Mesh(
      new THREE.TorusGeometry(ringRadius, Math.max(handleRadius * 0.7, 0.18), 8, 72),
      ringMaterial,
    );
    orientRingToAxis(ring, axis);
    ring.position.copy(center);
    ring.renderOrder = 49;
    ring.userData.moveGizmoHandle = { kind: "rotate", axis: axisKey };
    group.add(ring);
    pickables.push(ring);
  });

  return { group, pickables, handleLength };
}

function getSketchGridFrame(
  planeId: string,
  planeFrame: ActiveSketchGridPlaneFrame | null,
): GridPlaneFrame {
  if (planeFrame) {
    const normal = new THREE.Vector3(
      planeFrame.normal.x,
      planeFrame.normal.y,
      planeFrame.normal.z,
    ).normalize();
    return {
      origin: new THREE.Vector3(
        planeFrame.origin.x,
        planeFrame.origin.y,
        planeFrame.origin.z,
      ).addScaledVector(normal, -SKETCH_GRID_BACK_OFFSET),
      xAxis: new THREE.Vector3(
        planeFrame.x_axis.x,
        planeFrame.x_axis.y,
        planeFrame.x_axis.z,
      ).normalize(),
      yAxis: new THREE.Vector3(
        planeFrame.y_axis.x,
        planeFrame.y_axis.y,
        planeFrame.y_axis.z,
      ).normalize(),
      normal,
    };
  }

  if (planeId === "ref-plane-yz") {
    return {
      origin: new THREE.Vector3(
        SKETCH_PLANE_OFFSET - SKETCH_GRID_BACK_OFFSET,
        0,
        0,
      ),
      xAxis: new THREE.Vector3(0, 1, 0),
      yAxis: new THREE.Vector3(0, 0, 1),
      normal: new THREE.Vector3(1, 0, 0),
    };
  }

  if (planeId === "ref-plane-xz") {
    return {
      origin: new THREE.Vector3(
        0,
        0,
        SKETCH_PLANE_OFFSET - SKETCH_GRID_BACK_OFFSET,
      ),
      xAxis: new THREE.Vector3(1, 0, 0),
      yAxis: new THREE.Vector3(0, 1, 0),
      normal: new THREE.Vector3(0, 0, 1),
    };
  }

  return {
    origin: new THREE.Vector3(
      0,
      SKETCH_PLANE_OFFSET - SKETCH_GRID_BACK_OFFSET,
      0,
    ),
    xAxis: new THREE.Vector3(1, 0, 0),
    yAxis: new THREE.Vector3(0, 0, 1),
    normal: new THREE.Vector3(0, 1, 0),
  };
}

function projectPointToGridFrame(point: THREE.Vector3, frame: GridPlaneFrame) {
  const relative = point.clone().sub(frame.origin);
  return {
    u: relative.dot(frame.xAxis),
    v: relative.dot(frame.yAxis),
  };
}

function worldPointToSketchLocal(
  world: [number, number, number],
  planeId: string | null,
  planeFrame: ActiveSketchGridPlaneFrame | null,
): [number, number] | null {
  if (!planeId) {
    return null;
  }
  if (planeFrame) {
    const point = new THREE.Vector3(...world);
    const origin = new THREE.Vector3(
      planeFrame.origin.x,
      planeFrame.origin.y,
      planeFrame.origin.z,
    );
    const xAxis = new THREE.Vector3(
      planeFrame.x_axis.x,
      planeFrame.x_axis.y,
      planeFrame.x_axis.z,
    ).normalize();
    const yAxis = new THREE.Vector3(
      planeFrame.y_axis.x,
      planeFrame.y_axis.y,
      planeFrame.y_axis.z,
    ).normalize();
    const delta = point.sub(origin);
    return [delta.dot(xAxis), delta.dot(yAxis)];
  }
  if (planeId === "ref-plane-xy") {
    return [world[0], world[2]];
  }
  if (planeId === "ref-plane-yz") {
    return [world[1], world[2]];
  }
  return [world[0], world[1]];
}

function fallbackGridBounds(
  center: { u: number; v: number },
  spacing: number,
): GridPlaneBounds {
  const extent = spacing * GRID_MIN_HALF_LINE_COUNT;
  return {
    minU: snapGridCenter(center.u - extent, spacing),
    maxU: snapGridCenter(center.u + extent, spacing),
    minV: snapGridCenter(center.v - extent, spacing),
    maxV: snapGridCenter(center.v + extent, spacing),
  };
}

function getGridViewBounds(
  camera: THREE.OrthographicCamera,
  frame: GridPlaneFrame,
  spacing: number,
  fallbackCenter: { u: number; v: number },
  paddingMultiplier: number,
): GridPlaneBounds {
  const plane = new THREE.Plane().setFromNormalAndCoplanarPoint(
    frame.normal.clone().normalize(),
    frame.origin,
  );
  const rayDirection = new THREE.Vector3();
  camera.getWorldDirection(rayDirection);
  const corners = [
    [-1, -1],
    [1, -1],
    [-1, 1],
    [1, 1],
  ] as const;
  const projectedCorners: Array<{ u: number; v: number }> = [];

  for (const [x, y] of corners) {
    const rayOrigin = new THREE.Vector3(x, y, -1).unproject(camera);
    const denominator = plane.normal.dot(rayDirection);
    if (Math.abs(denominator) > 1e-6) {
      const t = -(rayOrigin.dot(plane.normal) + plane.constant) / denominator;
      const hit = rayOrigin.clone().addScaledVector(rayDirection, t);
      projectedCorners.push(projectPointToGridFrame(hit, frame));
    }
  }

  if (projectedCorners.length < 2) {
    return fallbackGridBounds(fallbackCenter, spacing);
  }

  const minU = Math.min(...projectedCorners.map((point) => point.u));
  const maxU = Math.max(...projectedCorners.map((point) => point.u));
  const minV = Math.min(...projectedCorners.map((point) => point.v));
  const maxV = Math.max(...projectedCorners.map((point) => point.v));
  const spanU = Math.max(maxU - minU, spacing);
  const spanV = Math.max(maxV - minV, spacing);
  const minPadding = spacing * GRID_MIN_HALF_LINE_COUNT;
  const paddingU = Math.max(spanU * paddingMultiplier, minPadding);
  const paddingV = Math.max(spanV * paddingMultiplier, minPadding);

  return {
    minU: Math.floor((minU - paddingU) / spacing) * spacing,
    maxU: Math.ceil((maxU + paddingU) / spacing) * spacing,
    minV: Math.floor((minV - paddingV) / spacing) * spacing,
    maxV: Math.ceil((maxV + paddingV) / spacing) * spacing,
  };
}

function getOrthographicViewHeight(camera: THREE.OrthographicCamera): number {
  return (camera.top - camera.bottom) / Math.max(camera.zoom, 0.0001);
}

function selectOrthographicGridSpacing(
  camera: THREE.OrthographicCamera,
): number {
  const desiredSpacing = Math.max(
    getOrthographicViewHeight(camera) / GRID_CAMERA_SCALE,
    GRID_STEPS_MM[0],
  );
  return (
    GRID_STEPS_MM.find((spacing) => spacing >= desiredSpacing) ??
    GRID_STEPS_MM[GRID_STEPS_MM.length - 1]
  );
}

function nearestCardinalAxis(viewOffset: THREE.Vector3): THREE.Vector3 | null {
  const candidates = [
    new THREE.Vector3(1, 0, 0),
    new THREE.Vector3(-1, 0, 0),
    new THREE.Vector3(0, 1, 0),
    new THREE.Vector3(0, -1, 0),
    new THREE.Vector3(0, 0, 1),
    new THREE.Vector3(0, 0, -1),
  ];
  let best: THREE.Vector3 | null = null;
  let bestDot = -1;
  for (const candidate of candidates) {
    const dot = viewOffset.dot(candidate);
    if (dot > bestDot) {
      best = candidate;
      bestDot = dot;
    }
  }
  return best && bestDot >= CARDINAL_VIEW_DOT_THRESHOLD ? best : null;
}

function getCardinalGridFrame(viewOffset: THREE.Vector3): GridPlaneFrame | null {
  const axis = nearestCardinalAxis(viewOffset);
  if (!axis) {
    return null;
  }

  if (Math.abs(axis.x) > 0.5) {
    return {
      origin: new THREE.Vector3(0, 0, 0),
      xAxis: new THREE.Vector3(0, 1, 0),
      yAxis: new THREE.Vector3(0, 0, 1),
      normal: axis,
    };
  }

  if (Math.abs(axis.z) > 0.5) {
    return {
      origin: new THREE.Vector3(0, 0, 0),
      xAxis: new THREE.Vector3(1, 0, 0),
      yAxis: new THREE.Vector3(0, 1, 0),
      normal: axis,
    };
  }

  return {
    origin: new THREE.Vector3(0, 0, 0),
    xAxis: new THREE.Vector3(1, 0, 0),
    yAxis: new THREE.Vector3(0, 0, 1),
    normal: axis,
  };
}

function isDraftDimensionTool(tool: SketchTool): tool is DraftDimensionTool {
  return tool === "line" || tool === "rectangle" || tool === "circle" || tool === "polygon";
}

function isDrawableSketchTool(
  tool: SketchTool | null,
): tool is DraftDimensionTool | "arc" | "polygon" {
  return (
    tool === "line" ||
    tool === "rectangle" ||
    tool === "circle" ||
    tool === "arc" ||
    tool === "polygon"
  );
}

function sketchToolLabelKey(tool: DraftDimensionTool | "arc" | "polygon"): string {
  if (tool === "line") {
    return "toolbar.line";
  }
  if (tool === "rectangle") {
    return "toolbar.rectangle";
  }
  if (tool === "circle") {
    return "toolbar.circle";
  }
  if (tool === "arc") {
    return "toolbar.arc";
  }
  return "toolbar.polygon";
}

function formatDraftDimension(value: number): string {
  return Math.max(Math.abs(value), 0).toFixed(2);
}

function draftSessionValues(
  tool: DraftDimensionTool,
  start: [number, number],
  current: [number, number],
): Record<DraftDimensionField, string> {
  const width = current[0] - start[0];
  const length = current[1] - start[1];
  const radius = distanceBetweenPoints(start, current);
  // Angle from positive X axis in sketch coordinates. Negated so that
  // positive angles go CCW on screen (sketch Y points down in viewport).
  // Display shows absolute value; the sign is inferred from cursor
  // position when the user types.
  const lineAngleDeg =
    -Math.atan2(current[1] - start[1], current[0] - start[0]) *
    (180 / Math.PI);
  const lineAngle =
    tool === "line" ? Math.abs(lineAngleDeg).toFixed(2) : "0";
  return {
    length:
      tool === "line"
        ? formatDraftDimension(radius)
        : formatDraftDimension(length),
    width: formatDraftDimension(width),
    diameter: formatDraftDimension(radius * 2),
    radius: formatDraftDimension(radius),
    angle: lineAngle,
  };
}

function draftSessionFields(tool: DraftDimensionTool): DraftDimensionField[] {
  if (tool === "rectangle") {
    return ["width", "length"];
  }
  if (tool === "circle") {
    return ["diameter"];
  }
  if (tool === "polygon") {
    return ["radius"];
  }
  if (tool === "line") {
    return ["length", "angle"];
  }
  return ["length"];
}

function applyDraftDimensionFieldValue(
  session: DraftDimensionSession,
  field: DraftDimensionField,
  rawValue: string,
  lockField = true,
): DraftDimensionSession {
  const numeric = Number(rawValue);
  const nextValues = { ...session.values, [field]: rawValue };
  // Angles may be negative or zero — only reject NaN / Infinity.
  if (field === "angle") {
    if (!Number.isFinite(numeric)) {
      return {
        ...session,
        values: nextValues,
        activeField: field,
        lockedFields: lockField
          ? {...session.lockedFields, [field]: true}
          : session.lockedFields,
      };
    }
  } else if (!Number.isFinite(numeric) || numeric <= 0) {
    return {
      ...session,
      values: nextValues,
      activeField: field,
      lockedFields: lockField
        ? {...session.lockedFields, [field]: true}
        : session.lockedFields,
    };
  }

  const dx = session.current[0] - session.start[0];
  const dy = session.current[1] - session.start[1];
  const signX = dx < 0 ? -1 : 1;
  const signY = dy < 0 ? -1 : 1;
  let current = session.current;
  if (field === "angle") {
    // Angle is in degrees; convert to radians. Preserve the sign from
    // the current draft direction (atan2 of dy/dx) so the line keeps
    // its original orientation — the user types an unsigned value and
    // the core infers ± from cursor position.
    // Preserve the current length (or locked length) and rotate the
    // endpoint around the start.
    const currentRad = Math.atan2(dy, dx);
    const sign = currentRad >= 0 ? 1 : -1;
    const radians = sign * numeric * (Math.PI / 180);
    const currentLength = Math.hypot(dx, dy) || 1;
    const lockedLength = session.lockedFields.length
      ? Number(session.values.length)
      : NaN;
    const useLength =
      Number.isFinite(lockedLength) && lockedLength > 0
        ? lockedLength
        : currentLength;
    current = [
      session.start[0] + Math.cos(radians) * useLength,
      session.start[1] + Math.sin(radians) * useLength,
    ];
  } else if (session.tool === "rectangle") {
    current = [
      field === "width" ? session.start[0] + signX * numeric : current[0],
      field === "length" ? session.start[1] + signY * numeric : current[1],
    ];
  } else if (session.tool === "circle") {
    const radius = numeric / 2;
    const length = Math.hypot(dx, dy) || 1;
    current = [
      session.start[0] + (dx / length) * radius,
      session.start[1] + (dy / length) * radius,
    ];
  } else {
    const length = Math.hypot(dx, dy) || 1;
    current = [
      session.start[0] + (dx / length) * numeric,
      session.start[1] + (dy / length) * numeric,
    ];
  }

  return {
    ...session,
    current,
    values: {
      ...draftSessionValues(session.tool, session.start, current),
      [field]: rawValue,
    },
    activeField: field,
    lockedFields: lockField
      ? {...session.lockedFields, [field]: true}
      : session.lockedFields,
  };
}

function updateDraftSessionCurrent(
  session: DraftDimensionSession,
  current: [number, number],
): DraftDimensionSession {
  let next: DraftDimensionSession = {
    ...session,
    current,
    values: draftSessionValues(session.tool, session.start, current),
  };

  for (const field of draftSessionFields(session.tool)) {
    if (!session.lockedFields[field]) {
      continue;
    }
    const lockedValue = Number(session.values[field]);
    // Angles can be zero or negative — only reject NaN/Infinity.
    if (field === "angle") {
      if (!Number.isFinite(lockedValue)) {
        next.values[field] = session.values[field];
        continue;
      }
    } else if (!Number.isFinite(lockedValue) || lockedValue <= 0) {
      next.values[field] = session.values[field];
      continue;
    }
    next = applyDraftDimensionFieldValue(
      {...next, values: {...next.values, [field]: session.values[field]}},
      field,
      session.values[field],
      false,
    );
  }

  return {
    ...next,
    activeField: session.activeField,
    values: {
      ...next.values,
      ...Object.fromEntries(
        Object.entries(session.lockedFields)
          .filter(([, locked]) => locked)
          .map(([field]) => [
            field,
            session.values[field as DraftDimensionField],
          ]),
      ),
    },
  };
}

export function ViewportPanel({
  status,
  document,
  viewport,
  onSnapshotCaptureReady,
  onSelectPrimitive,
  onSelectReference,
  onSelectFace,
  onSelectEdge,
  onSelectVertex,
  onStartSketch,
  onStartSketchOnFace,
  onAddSketchLine,
  onSetSketchMidpointAnchor,
  onSetSketchPointLineAnchor,
  onAddSketchAngleDimension,
  onAddSketchDistanceDimension,
  onAddSketchLineLengthDimension,
  onAddSketchCircleRadiusDimension,
  onAddSketchPolygonRadiusDimension,
  onSetSketchLineConstraint,
  onSetSketchPerpendicularConstraint,
  onSetSketchTangentConstraint,
  onAddSketchRectangle,
  onAddSketchCircle,
  onAddSketchArc,
  arcToolMode,
  onSetArcToolMode,
  rectangleToolMode,
  onSetRectangleToolMode,
  circleToolMode,
  onSetCircleToolMode,
  polygonToolMode,
  onSetPolygonToolMode,
  onAddSketchPolygon,
  onAddSketchFillet,
  onSelectSketchEntity,
  onBatchSelectEntities,
  onPickSketchPoint,
  armedSketchConstraint,
  mirrorFocusedSlot,
  inactiveSketchEntityPickEnabled = false,
  onPickInactiveSketchLine,
  onMirrorEntityPick,
  onCancelSketchConstraint,
  onClearSketchConstraint,
  onSelectSketchDimension,
  onUpdateSketchDimension,
  onUpdateSketchDimensionLabelPosition,
  onSelectSketchProfile,
  onTrimSketchEntity,
  onDeleteSketchSelection,
  onDeleteSketchDimension,
  onAddSketchPointDistanceDimension,
  onUpdateSketchDimensionDisplay,
  onSetSketchTool,
  onUpdateSketchPoint,
  onFinishSketch,
  moveGizmo = null,
  onMoveGizmoChange,
  onMoveBody,
  onCopyBody,
  onUnlinkBodyCopy,
  hiddenFeatureIds,
  hiddenSketchPlaneIds,
  hideReferences,
}: ViewportPanelProps) {
  const { config, activeTheme, updateConfig } = useAppConfig();
  const { t: translate } = useTranslation();
  const [showReferencePlanes, setShowReferencePlanes] = useState(true);
  const showViewportGrid = config.viewport.showGrid;
  const showSketchGrid = config.viewport.showSketchGrid;
  const [trimDebugInfo, setTrimDebugInfo] = useState<string | null>(null);
  const [contextMenu, setContextMenu] =
    useState<ViewportContextMenuState | null>(null);
  const [sketchSnapLabel, setSketchSnapLabel] = useState<string | null>(null);
  // Floating constraint-preview badge tracked relative to the
  // viewport container. Shown next to the cursor whenever the snap
  // resolver is producing a midpoint or perpendicular snap so the
  // user sees *which* constraint the next click would auto-create
  // (CAD convention). `kind` controls the glyph; `x`/`y` are
  // container-local pixel offsets so the overlay scrolls with the
  // viewport.
  // First line picked while the dimension tool is armed. After a
  // line click we wait for a *second* line click to know whether the
  // user wants a length dim (same line clicked again) or an angle
  // dim (different line). Cleared when the dim tool exits or when a
  // dimension is created. Stored as a ref so the click handler reads
  // the latest value without re-attaching listeners.
  const dimensionToolFirstLineRef = useRef<string | null>(null);
  const [dimensionToolFirstLine, setDimensionToolFirstLine] = useState<
    string | null
  >(null);
  // First point picked in point_distance dimension mode.
  const dimensionToolFirstPointRef = useRef<{
    id: string;
    x: number;
    y: number;
  } | null>(null);
  const [constraintPreview, setConstraintPreview] = useState<{
    kind:
      | "midpoint"
      | "perpendicular"
      | "on_line"
      | "horizontal"
      | "vertical"
      | "tangent";
    x: number;
    y: number;
  } | null>(null);
  const [crosshairPointer, setCrosshairPointer] = useState<{
    x: number;
    y: number;
  } | null>(null);
  const [viewportSize, setViewportSize] = useState({ width: 1, height: 1 });
  // Whether the next drawable sketch entity will be flagged as
  // construction geometry. The core owns the resulting CAD state;
  // this UI state is only the pending tool option sent with the
  // add_* IPC command.
  const [sketchToolConstruction, setSketchToolConstruction] = useState(false);
  const sketchToolConstructionRef = useRef(false);
  const [polygonSides, setPolygonSides] = useState(6);
  const polygonSidesRef = useRef(6);
  // Held while the user holds the wireframe-toggle key (Tab) during a
  // pending fillet/chamfer panel session. Reveals every ghost edge
  // so the user can see and click the original sharp edges that
  // were hidden by default to keep the rounded preview readable.
  // Kept as a ref because the keydown/keyup handlers repaint edge
  // materials directly (no React state read). Painting goes through
  // `paintEdgeMaterials` which reads this ref.
  const revealGhostEdgesRef = useRef(false);
  const [dimensionDraftValue, setDimensionDraftValue] = useState("");
  const [dimensionSuggestionIndex, setDimensionSuggestionIndex] = useState(0);
  const [draftSuggestionState, setDraftSuggestionState] = useState<{
    field: DraftDimensionField;
    index: number;
  } | null>(null);
  const [isDimensionEditorOpen, setIsDimensionEditorOpen] = useState(false);
  const [dimensionLabelPositions, setDimensionLabelPositions] = useState<
    Record<string, [number, number, number]>
  >({});
  const dimensionLabelPositionsRef = useRef<
    Record<string, [number, number, number]>
  >({});
  const [draftDimensionSession, setDraftDimensionSession] =
    useState<DraftDimensionSession | null>(null);
  const pendingCircleDimensionPlacementRef = useRef<{
    fromCircleCount: number;
    center: [number, number];
    end: [number, number];
  } | null>(null);
  const hostRef = useRef<HTMLDivElement | null>(null);
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const dimensionEditorRef = useRef<HTMLFormElement | null>(null);
  const dimensionInputRef = useRef<HTMLInputElement | null>(null);
  const dimensionInputSelectionLockedRef = useRef(false);
  const dimensionExpressionTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const rendererRef = useRef<THREE.WebGLRenderer | null>(null);
  const sceneRef = useRef<THREE.Scene | null>(null);
  const cameraRef = useRef<THREE.OrthographicCamera | null>(null);
  const controlsRef = useRef<OrbitControls | null>(null);
  const contentGroupRef = useRef<THREE.Group | null>(null);
  const referenceGroupRef = useRef<THREE.Group | null>(null);
  const sketchGroupRef = useRef<THREE.Group | null>(null);
  const previewLineRef = useRef<THREE.Line | null>(null);
  const previewCircleRef = useRef<THREE.LineLoop | null>(null);
  const previewDimensionRef = useRef<{
    line: THREE.Object3D;
    label: THREE.Sprite;
  } | null>(null);
  // Mirrors `previewLineRef` / `previewCircleRef` for the arc tool.
  // Carries the dashed in-progress arc preview rendered while the
  // user is between clicks 2 and 3 (or, in center+start+end mode, a
  // dashed circle while between clicks 1 and 2).
  const previewArcRef = useRef<THREE.Line | null>(null);
  const trimSegmentHighlightRef = useRef<THREE.Line | null>(null);
  const trimArcHighlightRef = useRef<THREE.Line | null>(null);
  const draftDimGroupRef = useRef<THREE.Group | null>(null);
  /** Reusable scene object for draft dimension lines (create once, update positions in-place). */
  const draftDimSceneObjRef = useRef<{
    lines: THREE.LineSegments;
  } | null>(null);
  const draftArcTestRef = useRef<THREE.LineSegments | null>(null);
  // Projected screen positions for draft dimension labels, updated
  // every frame by the render loop so React can position inputs.
  const draftDimScreenPositionsRef = useRef<
    Partial<Record<DraftDimensionField, { x: number; y: number }>>
  >({});
  const viewCubeGroupRef = useRef<THREE.Group | null>(null);
  const viewCubeSceneRef = useRef<THREE.Scene | null>(null);
  const viewCubeCameraRef = useRef<THREE.OrthographicCamera | null>(null);
  const viewCubeRaycasterRef = useRef<THREE.Raycaster | null>(null);
  const viewCubeHoveredRef = useRef<ViewCubeHit>(null);
  const viewCubeAnimatingRef = useRef(false);
  const viewCubeAnimStartRef = useRef(0);
  const viewCubeAnimStartPosRef = useRef(new THREE.Vector3());
  const viewCubeAnimTargetPosRef = useRef(new THREE.Vector3());
  const viewCubeAnimStartUpRef = useRef(new THREE.Vector3(0, 1, 0));
  const viewCubeAnimTargetUpRef = useRef(new THREE.Vector3(0, 1, 0));
  const viewCubeDraggingRef = useRef(false);
  const viewCubeDragStartRef = useRef<{ x: number; y: number } | null>(null);
  const lineDraftStartRef = useRef<[number, number] | null>(null);
  // Track click timing and position for double-click detection during
  // line drafting. Two clicks <300ms apart at the same location break
  // the chain and start an independent line on the next click.
  const lastPointerDownTimeRef = useRef(0);
  const lastPointerDownPosRef = useRef<{ x: number; y: number } | null>(null);
const chainBreakRequestedRef = useRef(false);
// 2D sketch-plane angle (radians) of the last committed line segment,
// used as the reference for the next chained line's angle arc.
// null for the first / independent line (defaults to horizontal, 0 rad).
const previousLineAngleRef = useRef<number | null>(null);
const currentGridSpacingRef = useRef(10);
  const draftDimensionSessionRef = useRef<DraftDimensionSession | null>(null);
  const draftDimensionInputRefs = useRef<
    Partial<Record<DraftDimensionField, HTMLInputElement | null>>
  >({});
  /** Set while the user is actively typing into a draft field. Prevents
   *  the display-unit reconversion from overwriting partial input like
   *  "2." (which would round-trip through mm and lose the decimal). */
  const draftFieldFocusedRef = useRef<DraftDimensionField | null>(null);
  /** Raw user-typed input preserved during editing so the round-trip
   *  through mm doesn't drop the decimal from partial values like "2.". */
  const draftRawInputRef = useRef<Partial<Record<DraftDimensionField, string>>>({});
  const draftParameterExpressionRef = useRef<
    Partial<Record<DraftDimensionField, string>>
  >({});
  const draftStartedOnPointerDownRef = useRef(false);
  const previousReferencePlaneVisibilityRef = useRef<boolean | null>(null);
  const primitiveVisualsRef = useRef(new Map<string, PrimitiveVisual>());
  const primitiveStatesRef = useRef(
    new Map<string, PrimitiveInteractionState>(),
  );
  const referencePlaneVisualsRef = useRef(
    new Map<string, ReferencePlaneVisual>(),
  );
  const referencePlaneStatesRef = useRef(
    new Map<string, ReferencePlaneInteractionState>(),
  );
  const solidFaceVisualsRef = useRef(new Map<string, SolidFaceVisual>());
  const solidFaceStatesRef = useRef(
    new Map<string, SolidFaceInteractionState>(),
  );
  const worldGridRef = useRef<DynamicGridRef | null>(null);
  const sketchGridRef = useRef<DynamicGridRef | null>(null);
  const meshesRef = useRef<THREE.Mesh[]>([]);
  const referencePlaneMeshesRef = useRef<THREE.Mesh[]>([]);
  const sketchEntityObjectsRef = useRef<Array<THREE.Line | THREE.LineLoop>>([]);
  const sketchEntityObjectByIdRef = useRef(
    new Map<string, THREE.Line | THREE.LineLoop>(),
  );
  const sketchDimensionObjectsRef = useRef<Array<THREE.Object3D>>([]);
  const sketchConstraintObjectsRef = useRef<Array<THREE.Object3D>>([]);
  const sketchPointObjectsRef = useRef<THREE.Mesh[]>([]);
  const sketchPointObjectByIdRef = useRef(new Map<string, THREE.Mesh>());
  const sketchProfileObjectsRef = useRef<THREE.Group[]>([]);
  const sketchProfileVisualsRef = useRef(new Map<string, SketchProfileVisual>());
  const sketchProfileStatesRef = useRef(
    new Map<string, SketchProfileInteractionState>(),
  );
  const faceMeshesRef = useRef<THREE.Mesh[]>([]);
  // Body edges materialized as THREE.Line objects. Raycasting against
  // these (with a small `params.Line.threshold`) drives edge picking
  // for the upcoming fillet/chamfer features. Edges are checked before
  // faces in the pick chain because they sit ON the faces and would
  // otherwise be visually occluded.
  const edgeLineObjectsRef = useRef<THREE.Line[]>([]);
  // Body vertices materialized as small sphere meshes. Raycast first so
  // a vertex picks ahead of any edge or face that lies underneath.
  const vertexObjectsRef = useRef<THREE.Mesh[]>([]);
  // Translucent red overlay meshes for in-progress cut extrudes. Built
  // from `cut_previews` and rendered without participating in raycasts.
  const cutPreviewObjectsRef = useRef<THREE.Mesh[]>([]);
  const moveGizmoObjectsRef = useRef<THREE.Object3D[]>([]);
  const moveGizmoDragRef = useRef<MoveGizmoDragState | null>(null);
  const moveGizmoRef = useRef<MoveGizmoDescriptor | null>(moveGizmo);
  const moveGizmoChangeRef = useRef(onMoveGizmoChange);
  const moveBodyRef = useRef(onMoveBody);
  const copyBodyRef = useRef(onCopyBody);
  const unlinkBodyCopyRef = useRef(onUnlinkBodyCopy);
  const pendingMoveGizmoParametersRef = useRef<MoveFeatureParameters | null>(
    null,
  );
  const pendingMoveGizmoFrameRef = useRef<number | null>(null);
  const lastGeometryKeyRef = useRef("");
  const selectPrimitiveRef = useRef(onSelectPrimitive);
  const selectReferenceRef = useRef(onSelectReference);
  const selectFaceRef = useRef(onSelectFace);
  const selectEdgeRef = useRef(onSelectEdge);
  const selectVertexRef = useRef(onSelectVertex);
  const startSketchRef = useRef(onStartSketch);
  const startSketchOnFaceRef = useRef(onStartSketchOnFace);
  const addSketchLineRef = useRef(onAddSketchLine);
  const addSketchRectangleRef = useRef(onAddSketchRectangle);
  const addSketchCircleRef = useRef(onAddSketchCircle);
  
  const addSketchArcRef = useRef(onAddSketchArc);
  // --- Rectangle selection drag state ---
  interface SelectionDrag {
    startX: number;
    startY: number;
    currentX: number;
    currentY: number;
    active: boolean;
  }
  const selectionDragRef = useRef<SelectionDrag | null>(null);

  // Endpoint drag state — active when the user grabs a sketch
  // line endpoint in Select mode and drags it to a new position.
  interface EndpointDrag {
    pointId: string;
    startClientX: number;
    startClientY: number;
    startLocalX: number;
    startLocalY: number;
    hasMoved: boolean;
  }
  const endpointDragRef = useRef<EndpointDrag | null>(null);

  const [selectionRect, setSelectionRect] = useState<{
    left: number; top: number; width: number; height: number;
    visible: boolean;
    direction: "window" | "crossing";
  } | null>(null);

  const arcToolModeRef = useRef(arcToolMode);
  const rectangleToolModeRef = useRef(rectangleToolMode);
  const circleToolModeRef = useRef(circleToolMode);
  const polygonToolModeRef = useRef(polygonToolMode);
  const addSketchPolygonRef = useRef(onAddSketchPolygon);
  const addSketchFilletRef = useRef(onAddSketchFillet);
  // Arc placement requires three clicks. The first click goes through
  // `lineDraftStartRef` (shared with line/rect/circle to keep the
  // start-snap pipeline uniform); the second click lands here and
  // captures the end point so the third click can resolve to the
  // anchor (interior point or center, depending on `arcToolMode`).
  // Cleared after every committed arc and whenever the user switches
  // away from the arc tool.
  const arcSecondPointRef = useRef<[number, number] | null>(null);
  const rectSecondPointRef = useRef<[number, number] | null>(null);
  const circleSecondPointRef = useRef<[number, number] | null>(null);
  const selectSketchEntityRef = useRef(onSelectSketchEntity);
  const pickInactiveSketchLineRef = useRef(onPickInactiveSketchLine);
  const inactiveSketchEntityPickEnabledRef = useRef(
    inactiveSketchEntityPickEnabled,
  );
  const pickSketchPointRef = useRef(onPickSketchPoint);
  const updateSketchPointRef = useRef(onUpdateSketchPoint);
  const selectSketchDimensionRef = useRef(onSelectSketchDimension);
  const updateSketchDimensionRef = useRef(onUpdateSketchDimension);
  const updateSketchDimensionLabelPositionRef = useRef(
    onUpdateSketchDimensionLabelPosition,
  );
  const selectSketchProfileRef = useRef(onSelectSketchProfile);
  const trimSketchEntityRef = useRef(onTrimSketchEntity);
  const deleteSketchSelectionRef = useRef(onDeleteSketchSelection);
  const deleteSketchDimensionRef = useRef(onDeleteSketchDimension);
  const addSketchPointDistanceDimensionRef = useRef(
    onAddSketchPointDistanceDimension,
  );
  const updateSketchDimensionDisplayRef = useRef(
    onUpdateSketchDimensionDisplay,
  );
  const selectedSketchDimensionRef = useRef<SketchDimensionScene | null>(null);
  const displayedSketchDimensionsRef = useRef<SketchDimensionScene[]>([]);
  const dimensionLabelDragRef = useRef<DimensionLabelDragState | null>(null);
  const dimensionRelationPreviewRef =
    useRef<DimensionRelationPreview | null>(null);
  const dimensionRelationPreviewLabelRef =
    useRef<[number, number, number] | null>(null);
  const pendingRelationPlacementLabelRef =
    useRef<[number, number, number] | null>(null);
  const pendingRelationPlacementMatchRef =
    useRef<DimensionRelationPreview | null>(null);
  const pendingRelationPlacementRetryRef = useRef<number | null>(null);
  const hiddenRelationPreviewDimensionIdsRef = useRef<Set<string>>(new Set());
  const pendingDimensionPlacementRef = useRef(false);
  // The dimension ID that was just created (before the IPC round-trip).
  // Used to delete it on Escape even before the response arrives.
  const pendingDimensionIdRef = useRef<string | null>(null);
  // The entity that was just dimensioned (the source of the pending dimension).
  // Used by the regroup path: if user clicks a different entity, delete the
  // pending dimension and create a two-entity/point dimension instead.
  const pendingDimSourceEntityIdRef = useRef<string | null>(null);
  const dimensionPlacementOriginalPositionRef = useRef<
    [number, number, number] | null
  >(null);
  const dimensionEditOriginalValueRef = useRef<{
    dimensionId: string;
    value: number;
    expression: string;
  } | null>(null);
  const lastPointerEventRef = useRef<PointerEvent | null>(null);
  const isDimensionEditorOpenRef = useRef(false);
  const suppressNextDimensionEditorOpenRef = useRef(false);
  useEffect(() => {
    isDimensionEditorOpenRef.current = isDimensionEditorOpen;
  }, [isDimensionEditorOpen]);
  const setSketchToolRef = useRef(onSetSketchTool);
  const armedSketchConstraintRef = useRef(armedSketchConstraint);
  const mirrorFocusedSlotRef = useRef(mirrorFocusedSlot);
  const mirrorEntityPickRef = useRef(onMirrorEntityPick);
  const cancelSketchConstraintRef = useRef(onCancelSketchConstraint);
  const clearSketchConstraintRef = useRef(onClearSketchConstraint);
  /** Selected constraint for deletion on Delete key. */
  interface SelectedConstraint {
    kind: ConstraintType;
    entityId: string;
    relatedEntityId: string | null;
  }
  const [selectedConstraint, setSelectedConstraint] = useState<SelectedConstraint | null>(null);
  const selectedConstraintRef = useRef(selectedConstraint);
  selectedConstraintRef.current = selectedConstraint;
  const activeSketchToolRef = useRef<SketchTool>("select");
  const sketchSnapCandidatesRef = useRef<
    Array<{
      local: [number, number];
      label: string;
      kind?:
        | "midpoint"
        | "endpoint"
        | "center"
        | "intersection"
        | "nearest"
        | "tangent";
      hostLineId?: string;
      // Parametric position along the host line for sub-segment
      // midpoint candidates (and 0.5 for whole-line midpoints).
      tValue?: number;
      endpointHostLineId?: string;
    }>
  >([]);
  // Track host line ids for midpoint snaps that were committed during
  // a line draft. The first click of a line stores the start's host
  // (if any); the second click stores the end's host. After the
  // resulting `add_sketch_line` IPC settles, the post-add effect
  // reads the new line's start_point_id / end_point_id and dispatches
  // `set_sketch_midpoint_anchor` for each side that snapped to a
  // midpoint. The line count baseline at dispatch time guards against
  // misattributing the anchor to a later line.
  const pendingMidpointAnchorRef = useRef<{
    fromLineCount: number;
    startHostLineId: string | null;
    endHostLineId: string | null;
  } | null>(null);
  const draftStartMidpointHostRef = useRef<string | null>(null);
  // Host line id under the *start* point of the active draft. When
  // set, `resolveSnappedSketchPoint` enables perpendicular-foot snap
  // — projecting the cursor onto the perpendicular ray from the
  // start, in the direction normal to the host line.
  const draftStartEndpointHostRef = useRef<string | null>(null);
  // Pending perpendicular-constraint state, keyed against the line
  // count baseline for the same reasons as the midpoint anchor
  // pending state above. The post-add effect dispatches
  // `set_sketch_perpendicular_constraint` once the new line lands.
  const pendingPerpendicularConstraintRef = useRef<{
    fromLineCount: number;
    hostLineId: string;
  } | null>(null);
  // Pending point-on-line anchor state. Captured at click-time when
  // either end of the just-committed draft snapped to a line body.
  // The post-add effect dispatches one `set_sketch_point_line_anchor`
  // per side once the new line lands. Same baseline-on-line-count
  // guard as the other pending refs.
  const pendingPointLineAnchorRef = useRef<{
    fromLineCount: number;
    startHost: { lineId: string; t: number } | null;
    endHost: { lineId: string; t: number } | null;
  } | null>(null);
  // Mirror of `draftStartMidpointHostRef` for the line-body snap.
  // Holds the host line + t at the time the start was committed so
  // the *next* click (which only sees the end's snap) can still
  // attribute the start-side anchor to the correct host.
  const draftStartLineBodyHostRef = useRef<{
    lineId: string;
    t: number;
  } | null>(null);
  // Latest line count for the active sketch, mirrored as a ref so the
  // pointer handler (which captures stale closures) can baseline new
  // lines for the post-add midpoint-anchor effect.
  const sketchLineCountRef = useRef(0);
  // Stable ref to `onSetSketchMidpointAnchor` so the post-add effect
  // can issue the IPC without remounting on every re-render.
  const setSketchMidpointAnchorRef = useRef(onSetSketchMidpointAnchor);
  const setSketchPointLineAnchorRef = useRef(onSetSketchPointLineAnchor);
  const addSketchAngleDimensionRef = useRef(onAddSketchAngleDimension);
  const addSketchDistanceDimensionRef = useRef(onAddSketchDistanceDimension);
  const addSketchLineLengthDimensionRef = useRef(
    onAddSketchLineLengthDimension,
  );
  const addSketchCircleRadiusDimensionRef = useRef(
    onAddSketchCircleRadiusDimension,
  );
  const addSketchPolygonRadiusDimensionRef = useRef(
    onAddSketchPolygonRadiusDimension,
  );
  const setSketchPerpendicularConstraintRef = useRef(
    onSetSketchPerpendicularConstraint,
  );
  const setSketchLineConstraintRef = useRef(onSetSketchLineConstraint);
  const setSketchTangentConstraintRef = useRef(onSetSketchTangentConstraint);
  // Track Alt key for object snap override (invert all snap toggles
  // while held). Updated by keydown/keyup listeners below.
  const altHeldRef = useRef(false);

  // Captured at click-time when the resolved sketch point indicates
  // the line's end snapped to a circle tangent. The post-add effect
  // dispatches `set_sketch_tangent_constraint` so the relation
  // sticks. Same baseline-on-line-count guard as the other refs.
  const pendingTangentConstraintRef = useRef<{
    fromLineCount: number;
    circleId: string;
  } | null>(null);
  // Set at click-time when the resolved sketch point indicates an
  // axis lock; the post-add effect dispatches `set_sketch_line_constraint`
  // for the just-added line. Same baseline-on-line-count guard as
  // the other pending refs to avoid mis-attribution if the line
  // count ticks twice between commit and refresh.
  const pendingAxisConstraintRef = useRef<{
    fromLineCount: number;
    kind: "horizontal" | "vertical";
  } | null>(null);
  // Pending dimension deletion after a sketch entity commit. Set by
  // `commitDraftDimensionSession` when the user dragged without typing
  // (lockedFields is empty for the relevant field). The post-add effect
  // below reads this and calls `onDeleteSketchDimension` once the new
  // entity lands, removing the auto-dimension the core created.
  const pendingDimensionDeletionRef = useRef<{
    shouldDeleteLine: boolean;
    shouldDeleteCircle: boolean;
    shouldDeletePolygon: boolean;
    shouldDeleteRectangle: boolean;
    shouldDeleteLineAngle: boolean;
  } | null>(null);
  const pendingDraftDimensionExpressionsRef = useRef<{
    tool: DraftDimensionTool;
    fromLineCount: number;
    fromCircleCount: number;
    fromPolygonCount: number;
    expressions: Partial<Record<DraftDimensionField, string>>;
  } | null>(null);
  // Snapshot of the sketch feature's lines for the post-add effect to
  // index into. Same pattern as the count ref above.
  const sketchLinesRef = useRef<
    NonNullable<typeof sketchFeature>["sketch_parameters"] | null
  >(null);
  // Bodies whose edges are picked against a stable pre-fillet topology
  // because a fillet / chamfer feature targeting them is in its
  // pending panel session. Used to flag those edges as ghosts in the
  // scene, which the renderer hides by default and reveals when the
  // user holds Tab. Recomputed alongside the scene so it stays in
  // sync with `feature_history`.
  const pendingEdgeOpBodyIds = useMemo(() => {
    const result = new Set<string>();
    if (!document) {
      return result;
    }
    for (const feature of document.feature_history) {
      const params =
        feature.fillet_parameters ?? feature.chamfer_parameters ?? null;
      if (params && params.is_pending && params.target_body_id) {
        result.add(params.target_body_id);
      }
    }
    return result;
  }, [document]);
  const sceneData = useMemo(
    () =>
      viewport?.has_active_document
        ? createViewportScene(viewport, {
            hiddenFeatureIds,
            hiddenSketchPlaneIds,
            hideReferences,
            pendingEdgeOpBodyIds,
            document,
          })
        : null,
    [
      viewport,
      hiddenFeatureIds,
      hiddenSketchPlaneIds,
      hideReferences,
      pendingEdgeOpBodyIds,
      document,
    ],
  );
  const sceneDataRef = useRef(sceneData);
  useEffect(() => {
    sceneDataRef.current = sceneData;
  }, [sceneData]);
  useEffect(() => {
    applyTheme(activeTheme);
  }, [activeTheme]);
  const hasActiveDocument = Boolean(viewport?.has_active_document);
  const activeSketchPlaneId = document?.active_sketch_plane_id ?? null;
  const activeSketchTool = document?.active_sketch_tool ?? "select";
  const sketchFeature = useMemo(
    () =>
      document?.feature_history.find(
        (feature) => feature.feature_id === document.active_sketch_feature_id,
      ) ?? null,
    [document],
  );
  const activeSketchPlaneFrame =
    sketchFeature?.sketch_parameters?.plane_frame ?? null;
  useEffect(() => {
    const pending = pendingCircleDimensionPlacementRef.current;
    const sketch = sketchFeature?.sketch_parameters;
    if (!pending || !sketch || sketch.circles.length <= pending.fromCircleCount) {
      return;
    }

    const circle =
      sketch.circles[pending.fromCircleCount] ??
      sketch.circles[sketch.circles.length - 1];
    if (!circle) {
      return;
    }
    const radius = distanceBetweenPoints(pending.center, pending.end);
    if (radius <= 1e-6) {
      pendingCircleDimensionPlacementRef.current = null;
      return;
    }
    const dx = pending.end[0] - pending.center[0];
    const dy = pending.end[1] - pending.center[1];
    const length = Math.hypot(dx, dy);
    if (length <= 1e-6) {
      pendingCircleDimensionPlacementRef.current = null;
      return;
    }
    const labelLocal: [number, number] = [
      pending.center[0] + (dx / length) * (radius + 4),
      pending.center[1] + (dy / length) * (radius + 4),
    ];
    setDimensionLabelPositions((current) => ({
      ...current,
      [`dim-circle-${circle.circle_id}`]: toWorldPoint(
        sketch.plane_id,
        labelLocal,
        sketch.plane_frame,
      ),
    }));
    pendingCircleDimensionPlacementRef.current = null;
  }, [sketchFeature]);
  // Post-commit dimension deletion for drag-only shapes that have no
  // typed value (Fusion 360 behavior). When the user commits a shape
  // by dragging without typing into a draft dimension field, the core
  // still creates an auto-dimension — we delete it here.
  useEffect(() => {
    const pending = pendingDimensionDeletionRef.current;
    if (!pending) {
      return;
    }
    const sketch = sketchFeature?.sketch_parameters;
    if (!sketch) {
      pendingDimensionDeletionRef.current = null;
      return;
    }
    console.warn("[dim-delete] pending", {
      tool: pending.shouldDeleteLine ? "line" : pending.shouldDeleteCircle ? "circle" : pending.shouldDeletePolygon ? "polygon" : pending.shouldDeleteRectangle ? "rectangle" : "none",
      sketchLines: sketch.lines.length,
      sketchCircles: sketch.circles.length,
      sketchPolygons: sketch.polygons?.length ?? 0,
      dimIds: sketch.dimensions.map(d => d.dimension_id),
    });
    // Use the last entity instead of fromLineCount to avoid race
    // conditions when React hasn't re-rendered between rapid commits.
    if (pending.shouldDeleteLine && sketch.lines.length > 0) {
      const line = sketch.lines[sketch.lines.length - 1];
      if (line && !line.is_construction) {
        void deleteSketchDimensionRef.current(`dim-line-${line.line_id}`);
      }
    }
    if (pending.shouldDeleteLineAngle && sketch.lines.length > 0) {
      const line = sketch.lines[sketch.lines.length - 1];
      if (line && !line.is_construction) {
        void deleteSketchDimensionRef.current(
          `dim-line-angle-${line.line_id}`,
        );
      }
    }
    if (pending.shouldDeleteCircle && sketch.circles.length > 0) {
      const circle = sketch.circles[sketch.circles.length - 1];
      if (circle && !circle.is_construction) {
        void deleteSketchDimensionRef.current(
          `dim-circle-${circle.circle_id}`,
        );
      }
    }
    if (pending.shouldDeletePolygon && (sketch.polygons?.length ?? 0) > 0) {
      const polygon = sketch.polygons?.[(sketch.polygons?.length ?? 1) - 1];
      if (polygon && !polygon.is_construction) {
        void deleteSketchDimensionRef.current(
          `dim-polygon-${polygon.polygon_id}`,
        );
      }
    }
    if (pending.shouldDeleteRectangle && sketch.lines.length >= 4) {
      for (let i = sketch.lines.length - 4; i < sketch.lines.length; i++) {
        const line = sketch.lines[i];
        if (line && !line.is_construction) {
          void deleteSketchDimensionRef.current(`dim-line-${line.line_id}`);
        }
      }
    }
    pendingDimensionDeletionRef.current = null;
  }, [sketchFeature]);
  useEffect(() => {
    const pending = pendingDraftDimensionExpressionsRef.current;
    const sketch = sketchFeature?.sketch_parameters;
    if (!pending || !sketch) {
      return;
    }

    const updateDimensionExpression = (dimensionId: string, expression?: string) => {
      if (!expression) {
        return;
      }
      void updateSketchDimensionRef.current(dimensionId, expression).catch(() => {});
    };

    if (pending.tool === "line") {
      if (sketch.lines.length <= pending.fromLineCount) {
        return;
      }
      const line =
        sketch.lines[pending.fromLineCount] ??
        sketch.lines[sketch.lines.length - 1];
      if (line) {
        updateDimensionExpression(
          `dim-line-${line.line_id}`,
          pending.expressions.length,
        );
        updateDimensionExpression(
          `dim-line-angle-${line.line_id}`,
          pending.expressions.angle,
        );
      }
    } else if (pending.tool === "rectangle") {
      if (sketch.lines.length < pending.fromLineCount + 4) {
        return;
      }
      const topLine = sketch.lines[pending.fromLineCount];
      const rightLine = sketch.lines[pending.fromLineCount + 1];
      if (topLine) {
        updateDimensionExpression(
          `dim-line-${topLine.line_id}`,
          pending.expressions.width,
        );
      }
      if (rightLine) {
        updateDimensionExpression(
          `dim-line-${rightLine.line_id}`,
          pending.expressions.length,
        );
      }
    } else if (pending.tool === "circle") {
      if (sketch.circles.length <= pending.fromCircleCount) {
        return;
      }
      const circle =
        sketch.circles[pending.fromCircleCount] ??
        sketch.circles[sketch.circles.length - 1];
      if (circle) {
        updateDimensionExpression(
          `dim-circle-${circle.circle_id}`,
          pending.expressions.diameter,
        );
      }
    } else if (pending.tool === "polygon") {
      const polygons = sketch.polygons ?? [];
      if (polygons.length <= pending.fromPolygonCount) {
        return;
      }
      const polygon =
        polygons[pending.fromPolygonCount] ?? polygons[polygons.length - 1];
      if (polygon) {
        updateDimensionExpression(
          `dim-polygon-${polygon.polygon_id}`,
          pending.expressions.radius,
        );
      }
    }

    pendingDraftDimensionExpressionsRef.current = null;
  }, [sketchFeature]);
  const selectedPrimitiveLabel = useMemo(() => {
    const selectedBox = viewport?.boxes.find((box) => box.is_selected);
    if (selectedBox) {
      return selectedBox.label;
    }

    const selectedCylinder = viewport?.cylinders.find(
      (cylinder) => cylinder.is_selected,
    );
    if (selectedCylinder) {
      return selectedCylinder.label;
    }

    const selectedPolygonExtrude = viewport?.polygon_extrudes.find(
      (primitive) => primitive.is_selected,
    );
    return selectedPolygonExtrude?.label ?? null;
  }, [viewport]);
  const selectedReference = useMemo(
    () =>
      viewport?.reference_planes.find(
        (referencePlane) => referencePlane.is_selected,
      ) ?? null,
    [viewport],
  );
  // Live "quick measurement" readout for the bottom-right Selection
  // panel, mirroring common CAD workflow's behavior where a single edge shows its
  // length and two vertices show their straight-line distance. Edge
  // length is computed by the core (BRepGProp) and shipped on the
  // viewport edge primitive; vertex distance is a trivial Euclidean
  // calc on world-space positions the core already gave us, so we do
  // it inline rather than round-tripping a `measure` command. Anything
  // outside those two cases returns null so the row is hidden.
  const measurementText = useMemo(() => {
    if (!document || !viewport) {
      return null;
    }
    if (document.selected_edge_ids.length === 1) {
      const edge = viewport.edges.find(
        (entry) => entry.id === document.selected_edge_ids[0],
      );
      if (edge) {
        return `Length: ${edge.length.toFixed(2)} mm`;
      }
    }
    if (document.selected_vertex_ids.length === 2) {
      const [aId, bId] = document.selected_vertex_ids;
      const a = viewport.vertices.find((entry) => entry.id === aId);
      const b = viewport.vertices.find((entry) => entry.id === bId);
      if (a && b) {
        const dx = a.position.x - b.position.x;
        const dy = a.position.y - b.position.y;
        const dz = a.position.z - b.position.z;
        const distance = Math.sqrt(dx * dx + dy * dy + dz * dz);
        return `Distance: ${distance.toFixed(2)} mm`;
      }
    }
    return null;
  }, [document, viewport]);
  function angleDimensionFrame(dimension: SketchDimensionScene) {
    const sketch = sketchFeature?.sketch_parameters;
    const coreDimension = sketch?.dimensions.find(
      (candidate) => candidate.dimension_id === dimension.dimensionId,
    );
    if (
      sketch &&
      coreDimension?.kind === "angle" &&
      coreDimension.secondary_entity_id
    ) {
      const lineA = sketch.lines.find(
        (line) => line.line_id === coreDimension.entity_id,
      );
      const lineB = sketch.lines.find(
        (line) => line.line_id === coreDimension.secondary_entity_id,
      );
      if (lineA && lineB) {
        const aEnds: Array<{
          pointId: string;
          local: [number, number];
        }> = [
          {pointId: lineA.start_point_id, local: [lineA.start_x, lineA.start_y]},
          {pointId: lineA.end_point_id, local: [lineA.end_x, lineA.end_y]},
        ];
        const bEnds: Array<{
          pointId: string;
          local: [number, number];
        }> = [
          {pointId: lineB.start_point_id, local: [lineB.start_x, lineB.start_y]},
          {pointId: lineB.end_point_id, local: [lineB.end_x, lineB.end_y]},
        ];
        let aPivotIndex = -1;
        let bPivotIndex = -1;
        for (let aIndex = 0; aIndex < aEnds.length && aPivotIndex < 0; aIndex++) {
          for (let bIndex = 0; bIndex < bEnds.length; bIndex++) {
            const samePointId = aEnds[aIndex].pointId === bEnds[bIndex].pointId;
            const dx = aEnds[aIndex].local[0] - bEnds[bIndex].local[0];
            const dy = aEnds[aIndex].local[1] - bEnds[bIndex].local[1];
            if (samePointId || Math.hypot(dx, dy) <= 0.05) {
              aPivotIndex = aIndex;
              bPivotIndex = bIndex;
              break;
            }
          }
        }

        if (aPivotIndex >= 0 && bPivotIndex >= 0) {
          const pivotLocal = aEnds[aPivotIndex].local;
          const aOther = aEnds[1 - aPivotIndex].local;
          const bOther = bEnds[1 - bPivotIndex].local;
          const aDx = aOther[0] - pivotLocal[0];
          const aDy = aOther[1] - pivotLocal[1];
          const bDx = bOther[0] - pivotLocal[0];
          const bDy = bOther[1] - pivotLocal[1];
          const aLength = Math.hypot(aDx, aDy);
          const bLength = Math.hypot(bDx, bDy);
          if (aLength > 1e-8 && bLength > 1e-8) {
            const pivot = new THREE.Vector3(
              ...toWorldPoint(dimension.planeId, pivotLocal, sketch.plane_frame),
            );
            const aUnitPoint = new THREE.Vector3(
              ...toWorldPoint(
                dimension.planeId,
                [pivotLocal[0] + aDx / aLength, pivotLocal[1] + aDy / aLength],
                sketch.plane_frame,
              ),
            );
            const bUnitPoint = new THREE.Vector3(
              ...toWorldPoint(
                dimension.planeId,
                [pivotLocal[0] + bDx / bLength, pivotLocal[1] + bDy / bLength],
                sketch.plane_frame,
              ),
            );
            const startUnit = aUnitPoint.sub(pivot).normalize();
            const endUnit = bUnitPoint.sub(pivot).normalize();
            const bisector = startUnit.clone().add(endUnit);
            if (bisector.lengthSq() > 1e-8) {
              const anchorRadius = Math.max(
                0.1,
                new THREE.Vector3(...dimension.anchorStart).distanceTo(pivot),
              );
              const dimensionRadius = Math.max(
                anchorRadius + 1,
                new THREE.Vector3(...dimension.dimensionStart).distanceTo(pivot),
              );
              return {
                pivot,
                startUnit,
                endUnit,
                bisector: bisector.normalize(),
                anchorRadius,
                dimensionRadius,
              };
            }
          }
        }
      }
    }

    // Fallback: use the core-provided arc centre when line data isn't
    // available.  anchorStart == dimensionStart for angle dims (both
    // sit at the arc radius), so the anchor-based pivot computation
    // below would always fail.
    if (dimension.arcCenter) {
      const pivot = new THREE.Vector3(...dimension.arcCenter);
      const startUnit = new THREE.Vector3(...dimension.dimensionStart)
        .sub(pivot).normalize();
      const endUnit = new THREE.Vector3(...dimension.dimensionEnd)
        .sub(pivot).normalize();
      const bisector = startUnit.clone().add(endUnit);
      if (bisector.lengthSq() > 1e-8) {
        const anchorRadius = new THREE.Vector3(...dimension.anchorEnd)
          .distanceTo(pivot);
        const dimensionRadius = new THREE.Vector3(...dimension.dimensionStart)
          .distanceTo(pivot);
        return {
          pivot,
          startUnit,
          endUnit,
          bisector: bisector.normalize(),
          anchorRadius,
          dimensionRadius,
        };
      }
    }
    return null;
  }

  const displayedSketchDimensions = useMemo(() => {
    if (!sceneData) {
      return [];
    }
    return sceneData.sketchDimensions.map((dimension) => {
      const labelPosition = dimensionLabelPositions[dimension.dimensionId];
      if (!labelPosition) {
        return dimension;
      }
      const originalLabel = new THREE.Vector3(...dimension.labelPosition);
      const nextLabel = new THREE.Vector3(...labelPosition);
      let offset = nextLabel.sub(originalLabel);
      if (dimension.kind === "angle" || dimension.kind === "line_angle") {
        const frame = angleDimensionFrame(dimension);
        if (frame) {
          // Angle dimensions store a control point for the arc radius;
          // the actual text label is derived on the angle bisector, same
          // as the relation ghost preview.
          const dimensionRadius = Math.max(
            ANGLE_DIMENSION_MIN_RADIUS,
            Math.min(
              nextLabel.distanceTo(frame.pivot),
              ANGLE_DIMENSION_MAX_RADIUS,
            ),
          );
          const labelRadius = Math.max(3.0, dimensionRadius * 0.42);
          const toTuple = (point: THREE.Vector3): [number, number, number] => [
            point.x,
            point.y,
            point.z,
          ];
          return {
            ...dimension,
            anchorStart: toTuple(
              frame.pivot
                .clone()
                .add(frame.startUnit.clone().multiplyScalar(frame.anchorRadius)),
            ),
            anchorEnd: toTuple(
              frame.pivot
                .clone()
                .add(frame.endUnit.clone().multiplyScalar(frame.anchorRadius)),
            ),
            dimensionStart: toTuple(
              frame.pivot
                .clone()
                .add(frame.startUnit.clone().multiplyScalar(dimensionRadius)),
            ),
            dimensionEnd: toTuple(
              frame.pivot
                .clone()
                .add(frame.endUnit.clone().multiplyScalar(dimensionRadius)),
            ),
            labelPosition: toTuple(
              frame.pivot
                .clone()
                .add(frame.bisector.clone().multiplyScalar(labelRadius)),
            ),
          };
        }
      }
      if (dimension.kind !== "angle" && dimension.kind !== "line_angle") {
        if (dimension.kind === "circle_radius") {
          const center = new THREE.Vector3(...dimension.dimensionStart)
            .add(new THREE.Vector3(...dimension.dimensionEnd))
            .multiplyScalar(0.5);
          const radius =
            new THREE.Vector3(...dimension.dimensionStart).distanceTo(
              new THREE.Vector3(...dimension.dimensionEnd),
            ) * 0.5;
          const direction = new THREE.Vector3(...labelPosition).sub(center);
          const planeNormal = getSketchGridFrame(
            dimension.planeId,
            activeSketchPlaneFrame,
          ).normal;
          direction.addScaledVector(planeNormal, -direction.dot(planeNormal));
          if (direction.lengthSq() > 1e-8 && radius > 1e-8) {
            direction.normalize();
            const start = center
              .clone()
              .add(direction.clone().multiplyScalar(-radius));
            const end = center
              .clone()
              .add(direction.clone().multiplyScalar(radius));
            const toTuple = (point: THREE.Vector3): [number, number, number] => [
              point.x,
              point.y,
              point.z,
            ];
            return {
              ...dimension,
              anchorStart: toTuple(start),
              anchorEnd: toTuple(end),
              dimensionStart: toTuple(start),
              dimensionEnd: toTuple(end),
              labelPosition,
            };
          }
        }
        const extensionAxis = new THREE.Vector3(
          ...dimension.dimensionStart,
        ).sub(new THREE.Vector3(...dimension.anchorStart));
        const dimensionDirection = new THREE.Vector3(
          ...dimension.dimensionEnd,
        ).sub(new THREE.Vector3(...dimension.dimensionStart));
        const placementAxis =
          extensionAxis.lengthSq() > 1e-8
            ? extensionAxis.normalize()
            : getSketchGridFrame(
                dimension.planeId,
                activeSketchPlaneFrame,
              ).normal
                .cross(dimensionDirection)
                .normalize();
        if (placementAxis.lengthSq() > 1e-8) {
          offset = placementAxis.multiplyScalar(offset.dot(placementAxis));
        }
      }
      const shiftPoint = (point: [number, number, number]) => {
        const shifted = new THREE.Vector3(...point).add(offset);
        return [shifted.x, shifted.y, shifted.z] as [number, number, number];
      };
      const shiftedLabel = shiftPoint(dimension.labelPosition);
      if (
        dimension.kind === "line_line_distance" ||
        dimension.kind === "circle_line_distance"
      ) {
        return {
          ...dimension,
          dimensionStart: shiftPoint(dimension.dimensionStart),
          dimensionEnd: shiftPoint(dimension.dimensionEnd),
          labelPosition: shiftedLabel,
        };
      }
      return {
        ...dimension,
        dimensionStart: shiftPoint(dimension.dimensionStart),
        dimensionEnd: shiftPoint(dimension.dimensionEnd),
        labelPosition: shiftedLabel,
      };
    });
  }, [activeSketchPlaneFrame, dimensionLabelPositions, sceneData]);
  useEffect(() => {
    displayedSketchDimensionsRef.current = displayedSketchDimensions;
  }, [displayedSketchDimensions]);
  const selectedSketchDimension = useMemo(
    () =>
      document?.selected_sketch_dimension_id
        ? (displayedSketchDimensions.find(
            (dimension) =>
              dimension.dimensionId === document.selected_sketch_dimension_id,
          ) ?? null)
        : null,
    [displayedSketchDimensions, document?.selected_sketch_dimension_id],
  );
  const selectedSketchDimensionValue = useMemo(
    () =>
      document?.selected_sketch_dimension_id && sketchFeature?.sketch_parameters
        ? (sketchFeature.sketch_parameters.dimensions.find(
            (dimension) =>
              dimension.dimension_id === document.selected_sketch_dimension_id,
          )?.value ?? null)
        : null,
    [document?.selected_sketch_dimension_id, sketchFeature],
  );
  const selectedSketchDimensionExpression = useMemo(
    () =>
      document?.selected_sketch_dimension_id && sketchFeature?.sketch_parameters
        ? (sketchFeature.sketch_parameters.dimensions.find(
            (dimension) =>
              dimension.dimension_id === document.selected_sketch_dimension_id,
          )?.expression ?? "")
        : "",
    [document?.selected_sketch_dimension_id, sketchFeature],
  );
  const getParameterSuggestions = (
    value: string,
    cursor: number,
    isAngleDimension: boolean,
  ): ParameterSuggestion[] => {
    if (!document?.parameters.length) {
      return [];
    }
    const token = parameterTokenAtCursor(value, cursor);
    if (!token) {
      return [];
    }
    const normalizedQuery = token.query.toLowerCase();
    if (
      document.parameters.some(
        (parameter) =>
          !parameter.has_error &&
          parameter.name.toLowerCase() === normalizedQuery,
      )
    ) {
      return [];
    }
    return document.parameters
      .filter((parameter) => !parameter.has_error)
      .filter((parameter) =>
        isAngleDimension ? parameter.kind === "angle" : parameter.kind !== "angle",
      )
      .map((parameter) => ({
        parameter,
        score: fuzzyParameterScore(token.query, parameter.name),
      }))
      .filter((entry) => entry.score > 0)
      .sort((left, right) => right.score - left.score)
      .slice(0, 6)
      .map(({ parameter }) => ({
        name: parameter.name,
        expression: parameter.expression,
        kind: parameter.kind,
        value: parameter.resolved_value,
      }));
  };
  const dimensionParameterSuggestions = useMemo<ParameterSuggestion[]>(() => {
    if (!selectedSketchDimension) {
      return [];
    }
    const cursor =
      dimensionInputRef.current?.selectionStart ?? dimensionDraftValue.length;
    const isAngleDimension =
      selectedSketchDimension.kind === "angle" ||
      selectedSketchDimension.kind === "line_angle";
    return getParameterSuggestions(
      dimensionDraftValue,
      cursor,
      isAngleDimension,
    );
  }, [
    dimensionDraftValue,
    document?.parameters,
    selectedSketchDimension,
  ]);
  useEffect(() => {
    setDimensionSuggestionIndex(0);
  }, [dimensionDraftValue, selectedSketchDimension?.dimensionId]);
  /** Stable DOF map ref — updated on every viewport change, read by
   *  paintSketchEntityMaterials so hover never sees an empty map. */
  const dofMapRef = useRef<Map<string, "full" | "over">>(new Map());
  useEffect(() => {
    const map = new Map<string, "full" | "over">();
    for (const ds of (viewport?.dof_statuses ?? [])) {
      if (ds.status === "full" || ds.status === "over") {
        map.set(ds.entity_id, ds.status);
      }
    }
    dofMapRef.current = map;
  }, [viewport?.dof_statuses]);

  /** DOF status for the currently selected sketch entity, if any. */
  const selectedEntityDof = useMemo(() => {
    const id = document?.selected_sketch_entity_id;
    const statuses = viewport?.dof_statuses;
    if (!id || !statuses) return null;
    return statuses.find((s) => s.entity_id === id) ?? null;
  }, [document?.selected_sketch_entity_id, viewport?.dof_statuses]);

  const sketchSnapCandidates = useMemo(() => {
    // When the C++ core emits snap_candidates, build from those.
    // Otherwise fall back to the legacy TS-side build.
    const coreCandidates = viewport?.snap_candidates;
    if (!sketchFeature?.sketch_parameters) {
      return [];
    }

    type Candidate = {
      local: [number, number];
      label: string;
      kind?:
        | "midpoint"
        | "endpoint"
        | "center"
        | "intersection"
        | "nearest"
        | "tangent";
      hostLineId?: string;
      tValue?: number;
      endpointHostLineId?: string;
    };
    const params = sketchFeature.sketch_parameters;
    const candidates: Candidate[] = [{ local: [0, 0], label: translate("snap.origin") }];

    if (coreCandidates && coreCandidates.length > 0) {
      // Build from C++ snap_candidates, gated by the core's SelectionFilter.
      for (const sc of coreCandidates) {
        switch (sc.kind) {
          case "endpoint":
            candidates.push({
              local: [sc.local_x, sc.local_y],
              label: sc.label,
              kind: "endpoint",
              endpointHostLineId: sc.entity_id || undefined,
            });
            break;
          case "midpoint":
            candidates.push({
              local: [sc.local_x, sc.local_y],
              label: sc.label,
              kind: "midpoint",
              hostLineId: sc.entity_id || undefined,
              tValue: 0.5,
            });
            break;
          case "center":
          default:
            candidates.push({
              local: [sc.local_x, sc.local_y],
              label: sc.label,
            });
            break;
        }
      }
      return candidates;
    }

    // Legacy fallback: build candidates from raw sketch entities.
    // (preserved for when the core hasn't been rebuilt yet)
    for (const line of params.lines) {
      candidates.push({
        local: [line.start_x, line.start_y],
        label:
          line.constraint === "horizontal" || line.constraint === "vertical"
            ? translate("snap.constrainedLine", {
                constraint:
                  line.constraint === "horizontal"
                    ? translate("toolbar.horizontal")
                    : translate("toolbar.vertical"),
              })
            : translate("snap.lineEndpoint"),
        kind: "endpoint",
        endpointHostLineId: line.line_id,
      });
      candidates.push({
        local: [line.end_x, line.end_y],
        label: translate("snap.lineEndpoint"),
        kind: "endpoint",
        endpointHostLineId: line.line_id,
      });

      // Build the list of "split" t-values for this line. A line is
      // split by any point anchored on it (whole-line midpoint
      // anchor at t=0.5, or arbitrary-t point-line anchor). Sorting
      // them and inserting the t=0 / t=1 endpoints yields a sequence
      // of sub-segments; the midpoint of each sub-segment becomes
      // its own snap candidate. With no splits, this collapses back
      // to the classic whole-line midpoint at t=0.5.
      const splitTs: number[] = [];
      for (const anchor of params.midpoint_anchors) {
        if (anchor.line_id === line.line_id) {
          splitTs.push(0.5);
        }
      }
      for (const anchor of params.point_line_anchors ?? []) {
        if (anchor.line_id === line.line_id) {
          splitTs.push(anchor.t);
        }
      }
      // Dedupe + sort. Two anchors at the same t shouldn't generate
      // a zero-length sub-segment with degenerate snap candidates.
      const uniqueTs = Array.from(new Set([0, ...splitTs, 1])).sort(
        (a, b) => a - b,
      );
      for (let i = 0; i < uniqueTs.length - 1; i++) {
        const tMid = (uniqueTs[i] + uniqueTs[i + 1]) / 2;
        // Skip degenerate sub-segments — same-position anchors leave
        // adjacent t values equal up to floating-point noise.
        if (uniqueTs[i + 1] - uniqueTs[i] < 1e-9) {
          continue;
        }
        const isWholeLine = uniqueTs.length === 2;
        const dx = line.end_x - line.start_x;
        const dy = line.end_y - line.start_y;
        candidates.push({
          local: [line.start_x + tMid * dx, line.start_y + tMid * dy],
          // The label deliberately avoids ids — see AGENTS.md UI
          // copy rules. "Midpoint" reads naturally for both whole-
          // line and sub-segment cases.
          label: isWholeLine
            ? translate("snap.midpoint")
            : translate("snap.subSegmentMidpoint"),
          kind: "midpoint",
          hostLineId: line.line_id,
          tValue: tMid,
        });
      }
    }
    for (const circle of params.circles) {
      candidates.push({
        local: [circle.center_x, circle.center_y],
        label: translate("snap.circleCenter"),
      });
    }
    return candidates;
  }, [sketchFeature, translate, viewport?.snap_candidates]);
  const activeSketchPlaneIdRef = useRef(activeSketchPlaneId);
  const activeSketchPlaneFrameRef = useRef(activeSketchPlaneFrame);
  const showViewportGridRef = useRef(showViewportGrid);
  const showSketchGridRef = useRef(showSketchGrid);
  const documentRef = useRef(document);
  useEffect(() => {
    activeSketchPlaneIdRef.current = activeSketchPlaneId;
    activeSketchPlaneFrameRef.current = activeSketchPlaneFrame;
  }, [activeSketchPlaneId, activeSketchPlaneFrame]);
  useEffect(() => {
    showViewportGridRef.current = showViewportGrid;
  }, [showViewportGrid]);
  useEffect(() => {
    showSketchGridRef.current = showSketchGrid;
  }, [showSketchGrid]);
  useEffect(() => {
    documentRef.current = document;
  }, [document]);
  useEffect(() => {
    draftDimensionSessionRef.current = draftDimensionSession;
  }, [draftDimensionSession]);
  useEffect(() => {
    if (!draftDimensionSession) {
      return;
    }
    renderDraftPreview(draftDimensionSession);
  }, [draftDimensionSession, sketchToolConstruction]);

  function toggleGridVisibility(kind: "viewport" | "sketch") {
    updateConfig((current) => ({
      ...current,
      viewport: {
        ...current.viewport,
        showGrid:
          kind === "viewport"
            ? !current.viewport.showGrid
            : current.viewport.showGrid,
        showSketchGrid:
          kind === "sketch"
            ? !current.viewport.showSketchGrid
            : current.viewport.showSketchGrid,
      },
    }));
  }

  useEffect(() => {
    function isTypingTarget(target: EventTarget | null) {
      return (
        target instanceof HTMLInputElement ||
        target instanceof HTMLTextAreaElement ||
        target instanceof HTMLSelectElement ||
        (target instanceof HTMLElement && target.isContentEditable)
      );
    }

    function handleKeyDown(event: KeyboardEvent) {
      if (
        isTypingTarget(event.target) ||
        !matchesHotkey(event, config.hotkeys.viewport.toggleGrid)
      ) {
        return;
      }
      event.preventDefault();
      updateConfig((current) => {
        const isSketchMode = Boolean(activeSketchPlaneIdRef.current);
        return {
          ...current,
          viewport: {
            ...current.viewport,
            showGrid: isSketchMode
              ? current.viewport.showGrid
              : !current.viewport.showGrid,
            showSketchGrid: isSketchMode
              ? !current.viewport.showSketchGrid
              : current.viewport.showSketchGrid,
          },
        };
      });
    }

    window.addEventListener("keydown", handleKeyDown);
    return () => {
      window.removeEventListener("keydown", handleKeyDown);
    };
  }, [config.hotkeys.viewport.toggleGrid, updateConfig]);

  // Track Alt key for object snap override.
  useEffect(() => {
    function handleAltDown(e: KeyboardEvent) {
      if (e.key === "Alt") altHeldRef.current = true;
    }
    function handleAltUp(e: KeyboardEvent) {
      if (e.key === "Alt") altHeldRef.current = false;
    }
    window.addEventListener("keydown", handleAltDown);
    window.addEventListener("keyup", handleAltUp);
    return () => {
      window.removeEventListener("keydown", handleAltDown);
      window.removeEventListener("keyup", handleAltUp);
    };
  }, []);

  useEffect(() => {
    const scene = sceneRef.current;
    const cubeScene = viewCubeSceneRef.current;

    if (cubeScene) {
      const previousCubeGroup = viewCubeGroupRef.current;
      if (previousCubeGroup) {
        cubeScene.remove(previousCubeGroup);
        disposeViewCubeGroup(previousCubeGroup);
      }

      const nextCubeGroup = buildViewCubeGroup();
      cubeScene.add(nextCubeGroup);
      viewCubeGroupRef.current = nextCubeGroup;
      viewCubeHoveredRef.current = null;
    }

    if (scene) {
      const worldGrid = worldGridRef.current;
      if (worldGrid) {
        scene.remove(worldGrid.group);
        disposeDynamicGrid(worldGrid);
        worldGridRef.current = null;
      }

      const sketchGrid = sketchGridRef.current;
      if (sketchGrid) {
        scene.remove(sketchGrid.group);
        disposeDynamicGrid(sketchGrid);
        sketchGridRef.current = null;
      }
    }

    syncPrimitiveVisuals();
    syncReferencePlaneVisuals();
    syncSolidFaceVisuals();
    syncSketchProfileVisuals();
    paintEdgeMaterials(hoveredEdgeIdRef.current);
    paintVertexMaterials(hoveredVertexIdRef.current);
    paintSketchEntityMaterials();
    paintSketchPointMaterials();
    paintDofStatusColors();
  }, [activeTheme.id]);

  // Update constraint badge highlights whenever selection changes.
  useEffect(() => {
    for (const obj of sketchConstraintObjectsRef.current) {
      const conEntityId =
        obj.userData.sketchConstraintEntityId as string | undefined;
      const conKind =
        obj.userData.sketchConstraintKind as string | undefined;
      const isSelected =
        selectedConstraint !== null &&
        conEntityId === selectedConstraint.entityId &&
        conKind === selectedConstraint.kind;
      if (obj instanceof THREE.Sprite && obj.material instanceof THREE.SpriteMaterial) {
        if (isSelected) {
          obj.material.color.set(0x60e0ff); // bright cyan
          obj.scale.set(7.5, 7.5, 1);
        } else {
          obj.material.color.set(0xffffff);
          obj.scale.set(6, 6, 1);
        }
      }
    }
  }, [selectedConstraint]);

  function clearPreviewLine() {
    const previewLine = previewLineRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!previewLine || !sketchGroup) {
      return;
    }

    sketchGroup.remove(previewLine);
    previewLine.geometry.dispose();
    disposeMaterial(previewLine.material);
    previewLineRef.current = null;
  }

  function clearPreviewCircle() {
    const previewCircle = previewCircleRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!previewCircle || !sketchGroup) {
      return;
    }

    sketchGroup.remove(previewCircle);
    previewCircle.geometry.dispose();
    disposeMaterial(previewCircle.material);
    previewCircleRef.current = null;
  }

  function clearPreviewArc() {
    const previewArc = previewArcRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!previewArc || !sketchGroup) {
      return;
    }

    sketchGroup.remove(previewArc);
    previewArc.geometry.dispose();
    disposeMaterial(previewArc.material);
    previewArcRef.current = null;
  }

  function clearTrimSegmentHighlight() {
    const hl = trimSegmentHighlightRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!hl || !sketchGroup) return;
    sketchGroup.remove(hl);
    hl.geometry.dispose();
    disposeMaterial(hl.material);
    trimSegmentHighlightRef.current = null;
  }
  function clearTrimArcHighlight() {
    const hl = trimArcHighlightRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!hl || !sketchGroup) return;
    sketchGroup.remove(hl);
    hl.geometry.dispose();
    disposeMaterial(hl.material);
    trimArcHighlightRef.current = null;
  }

  function updateTrimSegmentHighlight(
    _lineId: string,
    segments: Array<{ sx: number; sy: number; sz: number; ex: number; ey: number; ez: number }>,
    hoveredSegIdx: number,
  ) {
    clearTrimSegmentHighlight();
    if (hoveredSegIdx < 0 || hoveredSegIdx >= segments.length) return;
    const seg = segments[hoveredSegIdx];
    const sketchGroup = sketchGroupRef.current;
    if (!sketchGroup) return;

    const material = new THREE.LineBasicMaterial({
      color: 0xff3333,
      transparent: true,
      opacity: 0.9,
      linewidth: 3,
      depthTest: false,
    });
    const points = [
      new THREE.Vector3(seg.sx, seg.sy, seg.sz),
      new THREE.Vector3(seg.ex, seg.ey, seg.ez),
    ];
    const geometry = new THREE.BufferGeometry().setFromPoints(points);
    const hl = new THREE.Line(geometry, material);
    hl.renderOrder = 8; // above sketch entities (7)
    trimSegmentHighlightRef.current = hl;
    sketchGroup.add(hl);
  }

  function updateTrimArcHighlight(worldPts: Array<[number, number, number]>) {
    clearTrimArcHighlight();
    if (worldPts.length < 2) return;
    const sketchGroup = sketchGroupRef.current;
    if (!sketchGroup) return;
    const material = new THREE.LineBasicMaterial({
      color: 0xff3333, transparent: true, opacity: 0.9,
      linewidth: 3, depthTest: false,
    });
    const geometry = new THREE.BufferGeometry().setFromPoints(
      worldPts.map(([x, y, z]) => new THREE.Vector3(x, y, z)),
    );
    const hl = new THREE.Line(geometry, material);
    hl.renderOrder = 8;
    trimArcHighlightRef.current = hl;
    sketchGroup.add(hl);
  }

  function clearPreviewDimension() {
    const previewDimension = previewDimensionRef.current;
    const sketchGroup = sketchGroupRef.current;
    dimensionRelationPreviewRef.current = null;
    dimensionRelationPreviewLabelRef.current = null;
    restoreRelationPreviewHiddenDimensions();
    if (!previewDimension || !sketchGroup) {
      return;
    }

    sketchGroup.remove(previewDimension.line);
    sketchGroup.remove(previewDimension.label);
    // `buildSketchDimensionObject` now returns a Group that contains
    // LineSegments + optional arrow Mesh + optional dashed ref Line.
    // Traverse its children to dispose each sub-object's resources.
    previewDimension.line.traverse((child) => {
      if (
        child instanceof THREE.Mesh ||
        child instanceof THREE.LineSegments ||
        child instanceof THREE.Line
      ) {
        child.geometry.dispose();
        disposeMaterial(child.material);
      }
    });
    const labelMaterial = previewDimension.label.material;
    if (labelMaterial instanceof THREE.SpriteMaterial) {
      labelMaterial.map?.dispose();
    }
    disposeMaterial(labelMaterial);
    previewDimensionRef.current = null;
  }

  function setSketchDimensionObjectVisibility(
    dimensionId: string,
    visible: boolean,
  ) {
    for (const object of sketchDimensionObjectsRef.current) {
      let matches = object.userData.sketchDimensionId === dimensionId;
      if (!matches) {
        object.traverse((child) => {
          if (child.userData.sketchDimensionId === dimensionId) {
            matches = true;
          }
        });
      }
      if (matches) {
        object.visible = visible;
      }
    }
  }

  function hideRelationPreviewDimension(dimensionId: string | null) {
    if (!dimensionId) {
      return;
    }
    hiddenRelationPreviewDimensionIdsRef.current.add(dimensionId);
    setSketchDimensionObjectVisibility(dimensionId, false);
  }

  function restoreRelationPreviewHiddenDimensions() {
    for (const dimensionId of hiddenRelationPreviewDimensionIdsRef.current) {
      setSketchDimensionObjectVisibility(dimensionId, true);
    }
    hiddenRelationPreviewDimensionIdsRef.current.clear();
  }

  function clearDraftDimGroup() {
    const group = draftDimGroupRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!group || !sketchGroup) {
      return;
    }
    sketchGroup.remove(group);
    group.traverse((child) => {
      if (child instanceof THREE.Mesh || child instanceof THREE.LineSegments || child instanceof THREE.Line) {
        child.geometry.dispose();
        disposeMaterial(child.material);
      }
    });
    draftDimGroupRef.current = null;

    // Also clean up the reusable scene object
    const sceneObj = draftDimSceneObjRef.current;
    if (sceneObj && sketchGroup) {
      sketchGroup.remove(sceneObj.lines);
      sceneObj.lines.geometry.dispose();
      disposeMaterial(sceneObj.lines.material);
      draftDimSceneObjRef.current = null;
    }
    const arcTest = draftArcTestRef.current;
    if (arcTest && sketchGroup) {
      sketchGroup.remove(arcTest);
      arcTest.geometry.dispose();
      disposeMaterial(arcTest.material);
      draftArcTestRef.current = null;
    }
  }


  /** Returns the snapped point and a boolean indicating whether grid
   *  snap actually fired. When gridSnap is disabled, returns raw point
   *  with snapped = false. */
  function snapRawPointToGrid(
    rawPoint: {
      local: [number, number];
      world: [number, number, number];
    },
    worldUnitsPerPixel: number,
    gridSnapEnabled: boolean,
  ): { point: typeof rawPoint; snapped: boolean } {
    const spacing = currentGridSpacingRef.current;
    if (!gridSnapEnabled) {
      return { point: rawPoint, snapped: false };
    }
    if (!Number.isFinite(spacing) || spacing <= 0) {
      return { point: rawPoint, snapped: false };
    }
    const threshold = worldUnitsPerPixel * GRID_SNAP_SCREEN_DISTANCE_PX;
    const nearestX = Math.round(rawPoint.local[0] / spacing) * spacing;
    const nearestY = Math.round(rawPoint.local[1] / spacing) * spacing;
    const local: [number, number] = [
      Math.abs(rawPoint.local[0] - nearestX) <= threshold
        ? nearestX
        : rawPoint.local[0],
      Math.abs(rawPoint.local[1] - nearestY) <= threshold
        ? nearestY
        : rawPoint.local[1],
    ];
    if (local[0] === rawPoint.local[0] && local[1] === rawPoint.local[1]) {
      return { point: rawPoint, snapped: false };
    }
    return {
      point: {
        local,
        world: toWorldPoint(
          activeSketchPlaneId ?? "ref-plane-xy",
          local,
          activeSketchPlaneFrame,
        ),
      },
      snapped: true,
    };
  }

  function createDraftDimensionSession(
    tool: DraftDimensionTool,
    start: [number, number],
    current: [number, number],
  ): DraftDimensionSession {
    const fields = draftSessionFields(tool);
    return {
      tool,
      start,
      current,
      values: draftSessionValues(tool, start, current),
      activeField: fields[0],
      lockedFields: {},
    };
  }

  function clearDraftDimensionSession() {
    Object.values(draftDimensionInputRefs.current).forEach((input) => {
      input?.blur();
    });
    clearDraftDimGroup();
    setDraftDimensionSession(null);
    draftDimensionSessionRef.current = null;
    draftFieldFocusedRef.current = null;
    draftRawInputRef.current = {};
    draftParameterExpressionRef.current = {};
    previousLineAngleRef.current = null;
    setDraftSuggestionState(null);
  }

  // Centralized helper: schedule deletion of auto-dimensions after a
  // shape commit when the user dragged/clicked without typing a value.
  // Call this BEFORE clearing the draft session so lockedFields are
  // still available. Pass the tool and optionally a pre-captured
  // session (for chained-line paths where the session is about to be
  // replaced).
  function scheduleDimensionDeletion(
    tool: DraftDimensionTool,
    preCapturedSession?: DraftDimensionSession | null,
  ) {
    const session = preCapturedSession ?? draftDimensionSessionRef.current;
    pendingDimensionDeletionRef.current = {
      shouldDeleteLine:
        tool === "line" && !session?.lockedFields.length,
      shouldDeleteCircle:
        tool === "circle" && !session?.lockedFields.diameter,
      shouldDeletePolygon:
        tool === "polygon" && !session?.lockedFields.radius,
      shouldDeleteRectangle:
        tool === "rectangle" &&
        !session?.lockedFields.width &&
        !session?.lockedFields.length,
      shouldDeleteLineAngle:
        tool === "line" && !session?.lockedFields.angle,
    };
  }

  function scheduleDraftDimensionExpressionUpdate(tool: DraftDimensionTool) {
    const entries = Object.entries(draftParameterExpressionRef.current).filter(
      ([, expression]) => expression.trim().length > 0,
    );
    if (entries.length === 0) {
      pendingDraftDimensionExpressionsRef.current = null;
      return;
    }
    pendingDraftDimensionExpressionsRef.current = {
      tool,
      fromLineCount: sketchLineCountRef.current,
      fromCircleCount: sketchFeature?.sketch_parameters?.circles.length ?? 0,
      fromPolygonCount: sketchFeature?.sketch_parameters?.polygons?.length ?? 0,
      expressions: Object.fromEntries(entries) as Partial<
        Record<DraftDimensionField, string>
      >,
    };
    draftParameterExpressionRef.current = {};
  }

  function suppressDimensionEditorAfterSketchCommit() {
    suppressNextDimensionEditorOpenRef.current = true;
    dimensionInputRef.current?.blur();
    setIsDimensionEditorOpen(false);
  }

  // Look up a dimension's display_as preference from the document state.
  // Falls back to "" (diameter display) when the dimension isn't found
  // or the field is absent (backward compat with older documents).
  function resolveDimensionDisplayAs(dimensionId: string): string {
    const sketch = sketchLinesRef.current;
    if (!sketch) return "";
    const dim = sketch.dimensions.find(
      (d) => d.dimension_id === dimensionId,
    );
    return dim?.display_as ?? "";
  }

  // --- Dimension tool action helpers (shared by entity + sketch-point paths) ---

  function dimCreateCircle(entityId: string, displayAs: string) {
    const dimensionId = `dim-circle-${entityId}`;
    pendingDimensionIdRef.current = dimensionId;
    pendingDimSourceEntityIdRef.current = entityId;
    pendingDimensionPlacementRef.current = true;
    // Stage for possible two-entity follow-up pick while the diameter
    // placement is active.
    dimensionToolFirstLineRef.current = entityId;
    setDimensionToolFirstLine(entityId);
    void addSketchCircleRadiusDimensionRef
      .current(entityId, displayAs)
      .catch(() => {
        pendingDimensionIdRef.current = null;
        pendingDimSourceEntityIdRef.current = null;
        pendingDimensionPlacementRef.current = false;
        dimensionToolFirstLineRef.current = null;
        setDimensionToolFirstLine(null);
      });
  }

  function dimSelectCircle(entityId: string) {
    const dimensionId = `dim-circle-${entityId}`;
    handleDimensionClick(dimensionId);
    // Stage for possible two-entity follow-up pick
    dimensionToolFirstLineRef.current = entityId;
    setDimensionToolFirstLine(entityId);
  }

  function dimCreateLine(entityId: string) {
    const dimensionId = `dim-line-${entityId}`;
    pendingDimensionIdRef.current = dimensionId;
    pendingDimSourceEntityIdRef.current = entityId;
    pendingDimensionPlacementRef.current = true;
    // Stage for possible two-entity follow-up pick (angle / distance).
    dimensionToolFirstLineRef.current = entityId;
    setDimensionToolFirstLine(entityId);
    void addSketchLineLengthDimensionRef
      .current(entityId)
      .catch(() => {
        pendingDimensionIdRef.current = null;
        pendingDimSourceEntityIdRef.current = null;
        pendingDimensionPlacementRef.current = false;
        dimensionToolFirstLineRef.current = null;
        setDimensionToolFirstLine(null);
      });
  }

  function dimSelectLine(entityId: string) {
    const dimensionId = `dim-line-${entityId}`;
    handleDimensionClick(dimensionId);
    // Stage for possible two-entity follow-up pick
    dimensionToolFirstLineRef.current = entityId;
    setDimensionToolFirstLine(entityId);
  }

  function dimCreatePolygon(entityId: string) {
    const dimensionId = `dim-polygon-${entityId}`;
    pendingDimensionIdRef.current = dimensionId;
    pendingDimSourceEntityIdRef.current = entityId;
    pendingDimensionPlacementRef.current = true;
    void addSketchPolygonRadiusDimensionRef
      .current(entityId)
      .catch(() => {
        pendingDimensionIdRef.current = null;
        pendingDimSourceEntityIdRef.current = null;
        pendingDimensionPlacementRef.current = false;
      });
  }

  function dimSelectPolygon(entityId: string) {
    const dimensionId = `dim-polygon-${entityId}`;
    handleDimensionClick(dimensionId);
    // Stage for possible two-entity follow-up pick
    dimensionToolFirstLineRef.current = entityId;
    setDimensionToolFirstLine(entityId);
  }

  function dimCreateAngleOrDistance(
    firstEntityId: string,
    secondEntityId: string,
  ) {
    if (
      firstEntityId.startsWith("line-") &&
      sketchLinesShareEndpoint(firstEntityId, secondEntityId)
    ) {
      pendingDimensionPlacementRef.current = true;
      pendingDimSourceEntityIdRef.current = null;
      void addSketchAngleDimensionRef
        .current(firstEntityId, secondEntityId)
        .catch(() => {
          pendingDimensionPlacementRef.current = false;
          pendingRelationPlacementLabelRef.current = null;
          pendingRelationPlacementMatchRef.current = null;
        });
    } else {
      pendingDimensionPlacementRef.current = true;
      pendingDimSourceEntityIdRef.current = null;
      void addSketchDistanceDimensionRef
        .current(firstEntityId, secondEntityId)
        .catch(() => {
          pendingDimensionPlacementRef.current = false;
          pendingRelationPlacementLabelRef.current = null;
          pendingRelationPlacementMatchRef.current = null;
        });
    }
  }

  function unaryDimensionIdForEntity(entityId: string) {
    if (entityId.startsWith("line-")) {
      return `dim-line-${entityId}`;
    }
    if (entityId.startsWith("circle-")) {
      return `dim-circle-${entityId}`;
    }
    if (entityId.startsWith("polygon-")) {
      return `dim-polygon-${entityId}`;
    }
    return null;
  }

  function readDimensionPreviewFilter() {
    const localFilter = readLocalFilter();
    const base = localFilter ?? {
      select_curves: true,
      select_construction: false,
      snap_intersection: true,
      snap_nearest: true,
      snap_parallel: true,
    };
    if (!localFilter || !altHeldRef.current) {
      return base;
    }
    return {
      ...base,
      select_curves: !localFilter.select_curves,
      select_construction: !localFilter.select_construction,
      snap_intersection: !localFilter.snap_intersection,
      snap_nearest: !localFilter.snap_nearest,
      snap_parallel: !localFilter.snap_parallel,
    };
  }

  function lineLength(line: SketchLineEntry) {
    return Math.hypot(line.end_x - line.start_x, line.end_y - line.start_y);
  }

  function lineDirection(line: SketchLineEntry): [number, number] | null {
    const length = lineLength(line);
    if (length <= 1e-9) {
      return null;
    }
    return [(line.end_x - line.start_x) / length, (line.end_y - line.start_y) / length];
  }

  function midpointOfLine(line: SketchLineEntry): [number, number] {
    return [(line.start_x + line.end_x) / 2, (line.start_y + line.end_y) / 2];
  }

  function distanceToLineSegment(
    point: [number, number],
    line: SketchLineEntry,
  ) {
    const dx = line.end_x - line.start_x;
    const dy = line.end_y - line.start_y;
    const lenSq = dx * dx + dy * dy;
    if (lenSq <= 1e-12) {
      return {
        distance: Math.hypot(point[0] - line.start_x, point[1] - line.start_y),
        local: [line.start_x, line.start_y] as [number, number],
        t: 0,
      };
    }
    const t = Math.max(
      0,
      Math.min(
        1,
        ((point[0] - line.start_x) * dx + (point[1] - line.start_y) * dy) /
          lenSq,
      ),
    );
    const local: [number, number] = [
      line.start_x + t * dx,
      line.start_y + t * dy,
    ];
    return {
      distance: Math.hypot(point[0] - local[0], point[1] - local[1]),
      local,
      t,
    };
  }

  function distanceToCircleEdge(
    point: [number, number],
    circle: SketchCircleEntry,
  ) {
    const dx = point[0] - circle.center_x;
    const dy = point[1] - circle.center_y;
    const centerDistance = Math.hypot(dx, dy);
    return Math.abs(centerDistance - circle.radius);
  }

  function areLinesParallel(first: SketchLineEntry, second: SketchLineEntry) {
    const firstDir = lineDirection(first);
    const secondDir = lineDirection(second);
    if (!firstDir || !secondDir) {
      return false;
    }
    return Math.abs(firstDir[0] * secondDir[1] - firstDir[1] * secondDir[0]) <
      0.03;
  }

  function sharedLineEndpoint(
    first: SketchLineEntry,
    second: SketchLineEntry,
  ): [number, number] | null {
    if (first.start_point_id === second.start_point_id) {
      return [first.start_x, first.start_y];
    }
    if (first.start_point_id === second.end_point_id) {
      return [first.start_x, first.start_y];
    }
    if (first.end_point_id === second.start_point_id) {
      return [first.end_x, first.end_y];
    }
    if (first.end_point_id === second.end_point_id) {
      return [first.end_x, first.end_y];
    }
    return null;
  }

  function lineDirectionAwayFromPoint(
    line: SketchLineEntry,
    point: [number, number],
  ): [number, number] | null {
    const startDistance = Math.hypot(line.start_x - point[0], line.start_y - point[1]);
    const endDistance = Math.hypot(line.end_x - point[0], line.end_y - point[1]);
    const target =
      startDistance > endDistance
        ? [line.start_x, line.start_y]
        : [line.end_x, line.end_y];
    const dx = target[0] - point[0];
    const dy = target[1] - point[1];
    const length = Math.hypot(dx, dy);
    if (length <= 1e-9) {
      return null;
    }
    return [dx / length, dy / length];
  }

  function formatAngleLabel(radians: number) {
    const degrees = Math.abs((radians * 180) / Math.PI);
    return `${formatDraftDimension(degrees)}°`;
  }

  function createParallelLineDistancePreview(
    first: SketchLineEntry,
    second: SketchLineEntry,
    cursor: [number, number],
  ): SketchDimensionScene | null {
    const dir = lineDirection(first);
    if (!dir) {
      return null;
    }
    const normal: [number, number] = [-dir[1], dir[0]];
    const firstMid = midpointOfLine(first);
    const secondMid = midpointOfLine(second);
    const signedDistance =
      (secondMid[0] - firstMid[0]) * normal[0] +
      (secondMid[1] - firstMid[1]) * normal[1];
    if (Math.abs(signedDistance) <= 1e-6) {
      return null;
    }
    if (signedDistance < 0) {
      normal[0] *= -1;
      normal[1] *= -1;
    }

    const projection =
      (cursor[0] - firstMid[0]) * dir[0] + (cursor[1] - firstMid[1]) * dir[1];
    const anchorStart: [number, number] = [
      firstMid[0] + projection * dir[0],
      firstMid[1] + projection * dir[1],
    ];
    const distance = Math.abs(signedDistance);
    const anchorEnd: [number, number] = [
      anchorStart[0] + normal[0] * distance,
      anchorStart[1] + normal[1] * distance,
    ];
    const labelLocal: [number, number] = [
      (anchorStart[0] + anchorEnd[0]) / 2,
      (anchorStart[1] + anchorEnd[1]) / 2,
    ];
    const planeId = activeSketchPlaneIdRef.current ?? "ref-plane-xy";
    return {
      dimensionId: "preview-dim-line-line-distance",
      planeId,
      kind: "line_line_distance",
      entityId: first.line_id,
      label: `${formatDraftDimension(distance)} mm`,
      rawValue: distance,
      unitSuffix: "mm",
      isSelected: false,
      anchorStart: toWorldPoint(planeId, anchorStart, activeSketchPlaneFrameRef.current),
      anchorEnd: toWorldPoint(planeId, anchorEnd, activeSketchPlaneFrameRef.current),
      dimensionStart: toWorldPoint(planeId, anchorStart, activeSketchPlaneFrameRef.current),
      dimensionEnd: toWorldPoint(planeId, anchorEnd, activeSketchPlaneFrameRef.current),
      labelPosition: toWorldPoint(planeId, labelLocal, activeSketchPlaneFrameRef.current),
    };
  }

  function createLineAnglePreview(
    first: SketchLineEntry,
    second: SketchLineEntry,
    cursor: [number, number],
  ): SketchDimensionScene | null {
    const pivot = sharedLineEndpoint(first, second);
    if (!pivot) {
      return null;
    }
    const firstDir = lineDirectionAwayFromPoint(first, pivot);
    const secondDir = lineDirectionAwayFromPoint(second, pivot);
    if (!firstDir || !secondDir) {
      return null;
    }
    const dot = Math.max(
      -1,
      Math.min(1, firstDir[0] * secondDir[0] + firstDir[1] * secondDir[1]),
    );
    const angle = Math.acos(dot);
    const cursorRadius = Math.hypot(cursor[0] - pivot[0], cursor[1] - pivot[1]);
    const radius = Math.max(
      ANGLE_DIMENSION_MIN_RADIUS,
      Math.min(cursorRadius, ANGLE_DIMENSION_MAX_RADIUS),
    );
    const dimensionStart: [number, number] = [
      pivot[0] + firstDir[0] * radius,
      pivot[1] + firstDir[1] * radius,
    ];
    const dimensionEnd: [number, number] = [
      pivot[0] + secondDir[0] * radius,
      pivot[1] + secondDir[1] * radius,
    ];
    const anchorRadius = Math.min(lineLength(first), lineLength(second), radius + 2);
    const anchorStart: [number, number] = [
      pivot[0] + firstDir[0] * anchorRadius,
      pivot[1] + firstDir[1] * anchorRadius,
    ];
    const anchorEnd: [number, number] = [
      pivot[0] + secondDir[0] * anchorRadius,
      pivot[1] + secondDir[1] * anchorRadius,
    ];
    const bisector: [number, number] = [
      firstDir[0] + secondDir[0],
      firstDir[1] + secondDir[1],
    ];
    const bisectorLength = Math.hypot(bisector[0], bisector[1]);
    if (bisectorLength <= 1e-9) {
      return null;
    }
    bisector[0] /= bisectorLength;
    bisector[1] /= bisectorLength;
    const labelLocal: [number, number] = [
      pivot[0] + bisector[0] * Math.max(3, radius * 0.42),
      pivot[1] + bisector[1] * Math.max(3, radius * 0.42),
    ];
    const planeId = activeSketchPlaneIdRef.current ?? "ref-plane-xy";
    return {
      dimensionId: "preview-dim-line-angle",
      planeId,
      kind: "line_angle",
      entityId: first.line_id,
      label: formatAngleLabel(angle),
      rawValue: angle,
      unitSuffix: "°",
      isSelected: false,
      anchorStart: toWorldPoint(planeId, anchorStart, activeSketchPlaneFrameRef.current),
      anchorEnd: toWorldPoint(planeId, anchorEnd, activeSketchPlaneFrameRef.current),
      dimensionStart: toWorldPoint(planeId, dimensionStart, activeSketchPlaneFrameRef.current),
      dimensionEnd: toWorldPoint(planeId, dimensionEnd, activeSketchPlaneFrameRef.current),
      labelPosition: toWorldPoint(planeId, labelLocal, activeSketchPlaneFrameRef.current),
      arcCenter: toWorldPoint(planeId, pivot, activeSketchPlaneFrameRef.current),
      arcRadius: radius,
      arcStartAngle: Math.atan2(firstDir[1], firstDir[0]),
      arcEndAngle: Math.atan2(secondDir[1], secondDir[0]),
      arcCcw: true,
    };
  }

  function createCircleLineDistancePreview(
    line: SketchLineEntry,
    circle: SketchCircleEntry,
  ): SketchDimensionScene | null {
    const closest = distanceToLineSegment([circle.center_x, circle.center_y], line);
    const dx = circle.center_x - closest.local[0];
    const dy = circle.center_y - closest.local[1];
    const centerDistance = Math.hypot(dx, dy);
    if (centerDistance <= 1e-9) {
      return null;
    }
    const nx = dx / centerDistance;
    const ny = dy / centerDistance;
    const circleEdge: [number, number] = [
      circle.center_x - nx * circle.radius,
      circle.center_y - ny * circle.radius,
    ];
    const distance = Math.max(0, centerDistance - circle.radius);
    const labelLocal: [number, number] = [
      (closest.local[0] + circleEdge[0]) / 2,
      (closest.local[1] + circleEdge[1]) / 2,
    ];
    const planeId = activeSketchPlaneIdRef.current ?? "ref-plane-xy";
    return {
      dimensionId: "preview-dim-circle-line-distance",
      planeId,
      kind: "circle_line_distance",
      entityId: line.line_id,
      label: `${formatDraftDimension(distance)} mm`,
      rawValue: distance,
      unitSuffix: "mm",
      isSelected: false,
      anchorStart: toWorldPoint(planeId, closest.local, activeSketchPlaneFrameRef.current),
      anchorEnd: toWorldPoint(planeId, circleEdge, activeSketchPlaneFrameRef.current),
      dimensionStart: toWorldPoint(planeId, closest.local, activeSketchPlaneFrameRef.current),
      dimensionEnd: toWorldPoint(planeId, circleEdge, activeSketchPlaneFrameRef.current),
      labelPosition: toWorldPoint(planeId, labelLocal, activeSketchPlaneFrameRef.current),
    };
  }

  function createCircleCenterDistancePreview(
    first: SketchCircleEntry,
    second: SketchCircleEntry,
  ): SketchDimensionScene | null {
    const dx = second.center_x - first.center_x;
    const dy = second.center_y - first.center_y;
    const distance = Math.hypot(dx, dy);
    if (distance <= 1e-9) {
      return null;
    }
    const start: [number, number] = [first.center_x, first.center_y];
    const end: [number, number] = [second.center_x, second.center_y];
    const labelLocal: [number, number] = [
      (first.center_x + second.center_x) / 2,
      (first.center_y + second.center_y) / 2,
    ];
    const planeId = activeSketchPlaneIdRef.current ?? "ref-plane-xy";
    return {
      dimensionId: "preview-dim-circle-center-distance",
      planeId,
      kind: "circle_center_distance",
      entityId: first.circle_id,
      label: `${formatDraftDimension(distance)} mm`,
      rawValue: distance,
      unitSuffix: "mm",
      isSelected: false,
      anchorStart: toWorldPoint(planeId, start, activeSketchPlaneFrameRef.current),
      anchorEnd: toWorldPoint(planeId, end, activeSketchPlaneFrameRef.current),
      dimensionStart: toWorldPoint(planeId, start, activeSketchPlaneFrameRef.current),
      dimensionEnd: toWorldPoint(planeId, end, activeSketchPlaneFrameRef.current),
      labelPosition: toWorldPoint(planeId, labelLocal, activeSketchPlaneFrameRef.current),
    };
  }

  function renderDimensionRelationPreview(
    relation: DimensionRelationPreview,
    dimension: SketchDimensionScene,
  ) {
    clearPreviewDimension();
    const sketchGroup = sketchGroupRef.current;
    if (!sketchGroup) {
      dimensionRelationPreviewRef.current = null;
      return;
    }
    const preview = buildSketchDimensionObject(dimension, config.displayUnits, {
      variant: "muted-preview",
      pickable: false,
    });
    hideRelationPreviewDimension(unaryDimensionIdForEntity(relation.firstEntityId));
    previewDimensionRef.current = preview;
    dimensionRelationPreviewRef.current = relation;
    dimensionRelationPreviewLabelRef.current = dimension.labelPosition;
    sketchGroup.add(preview.line);
    sketchGroup.add(preview.label);
  }

  function updateDimensionRelationPreview(cursor: [number, number]) {
    clearPreviewDimension();
    dimensionRelationPreviewRef.current = null;
    const firstEntityId = dimensionToolFirstLineRef.current;
    const params = sketchLinesRef.current;
    if (!firstEntityId || !params || activeSketchToolRef.current !== "dimension") {
      return null;
    }
    const filter = readDimensionPreviewFilter();
    if (!filter.select_curves) {
      return null;
    }

    const firstLine = params.lines.find((line) => line.line_id === firstEntityId);
    const firstCircle = params.circles.find(
      (circle) => circle.circle_id === firstEntityId,
    );
    const allowConstruction = Boolean(filter.select_construction);
    let best:
      | {
          score: number;
          relation: DimensionRelationPreview;
          dimension: SketchDimensionScene;
        }
      | null = null;

    if (firstLine && (!firstLine.is_construction || allowConstruction)) {
      for (const line of params.lines) {
        if (line.line_id === firstLine.line_id) {
          continue;
        }
        if (line.is_construction && !allowConstruction) {
          continue;
        }
        const hit = distanceToLineSegment(cursor, line);
        if (hit.distance > SKETCH_SNAP_DISTANCE) {
          continue;
        }
        let relation: DimensionRelationPreview | null = null;
        let dimension: SketchDimensionScene | null = null;
        if (filter.snap_intersection && sharedLineEndpoint(firstLine, line)) {
          relation = {
            kind: "line_angle",
            firstEntityId,
            targetEntityId: line.line_id,
          };
          dimension = createLineAnglePreview(firstLine, line, cursor);
        } else if (filter.snap_parallel && areLinesParallel(firstLine, line)) {
          relation = {
            kind: "parallel_line_distance",
            firstEntityId,
            targetEntityId: line.line_id,
          };
          dimension = createParallelLineDistancePreview(firstLine, line, cursor);
        }
        if (!relation || !dimension) {
          continue;
        }
        if (!best || hit.distance < best.score) {
          best = { score: hit.distance, relation, dimension };
        }
      }

      if (filter.snap_nearest) {
        for (const circle of params.circles) {
          if (circle.is_construction && !allowConstruction) {
            continue;
          }
          const distance = distanceToCircleEdge(cursor, circle);
          if (distance > SKETCH_SNAP_DISTANCE) {
            continue;
          }
          const dimension = createCircleLineDistancePreview(firstLine, circle);
          if (!dimension) {
            continue;
          }
          const relation: DimensionRelationPreview = {
            kind: "circle_line_distance",
            firstEntityId,
            targetEntityId: circle.circle_id,
          };
          if (!best || distance < best.score) {
            best = { score: distance, relation, dimension };
          }
        }
      }
    } else if (
      firstCircle &&
      (!firstCircle.is_construction || allowConstruction) &&
      filter.snap_nearest
    ) {
      for (const line of params.lines) {
        if (line.is_construction && !allowConstruction) {
          continue;
        }
        const hit = distanceToLineSegment(cursor, line);
        if (hit.distance > SKETCH_SNAP_DISTANCE) {
          continue;
        }
        const dimension = createCircleLineDistancePreview(line, firstCircle);
        if (!dimension) {
          continue;
        }
        const relation: DimensionRelationPreview = {
          kind: "circle_line_distance",
          firstEntityId,
          targetEntityId: line.line_id,
        };
        if (!best || hit.distance < best.score) {
          best = { score: hit.distance, relation, dimension };
        }
      }
      for (const circle of params.circles) {
        if (circle.circle_id === firstCircle.circle_id) {
          continue;
        }
        if (circle.is_construction && !allowConstruction) {
          continue;
        }
        const distance = distanceToCircleEdge(cursor, circle);
        if (distance > SKETCH_SNAP_DISTANCE) {
          continue;
        }
        const dimension = createCircleCenterDistancePreview(firstCircle, circle);
        if (!dimension) {
          continue;
        }
        const relation: DimensionRelationPreview = {
          kind: "circle_center_distance",
          firstEntityId,
          targetEntityId: circle.circle_id,
        };
        if (!best || distance < best.score) {
          best = { score: distance, relation, dimension };
        }
      }
    }

    if (!best) {
      return null;
    }
    renderDimensionRelationPreview(best.relation, best.dimension);
    return best.relation;
  }

  function commitDimensionRelationPreview() {
    const relation = dimensionRelationPreviewRef.current;
    if (!relation) {
      return false;
    }
    const pointerEvent = lastPointerEventRef.current;
    const renderer = rendererRef.current;
    const camera = cameraRef.current;
    const sketchPlaneId = activeSketchPlaneIdRef.current;
    const clickedSketchPoint =
      pointerEvent && renderer && camera && sketchPlaneId
        ? resolveSketchPlanePoint(
            pointerEvent,
            renderer,
            camera,
            sketchPlaneId,
            activeSketchPlaneFrameRef.current,
          )
        : null;
    pendingRelationPlacementLabelRef.current =
      relation.kind === "line_angle"
        ? clickedSketchPoint?.world ?? dimensionRelationPreviewLabelRef.current
        : clickedSketchPoint?.world ?? dimensionRelationPreviewLabelRef.current;
    pendingRelationPlacementMatchRef.current = relation;
    clearPreviewDimension();
    dimensionRelationPreviewRef.current = null;
    const unaryDimensionId = unaryDimensionIdForEntity(relation.firstEntityId);
    if (unaryDimensionId) {
      void deleteSketchDimensionRef.current(unaryDimensionId);
    }
    pendingDimensionIdRef.current = null;
    pendingDimSourceEntityIdRef.current = null;
    pendingDimensionPlacementRef.current = false;
    dimensionLabelDragRef.current = null;
    dimensionPlacementOriginalPositionRef.current = null;
    controlsRef.current && (controlsRef.current.enabled = true);
    setCanvasCursor("");
    dimensionToolFirstLineRef.current = null;
    setDimensionToolFirstLine(null);
    dimensionToolFirstPointRef.current = null;
    dimCreateAngleOrDistance(relation.firstEntityId, relation.targetEntityId);
    schedulePendingRelationPlacementRetry();
    return true;
  }

  function selectedDimensionMatchesPendingRelation(
    dimension: SketchDimensionScene,
    relation: DimensionRelationPreview,
  ) {
    const params = sketchLinesRef.current;
    const coreDimension = params?.dimensions.find(
      (candidate) => candidate.dimension_id === dimension.dimensionId,
    );
    if (!coreDimension) {
      return false;
    }
    const isSamePair =
      (coreDimension.entity_id === relation.firstEntityId &&
        coreDimension.secondary_entity_id === relation.targetEntityId) ||
      (coreDimension.entity_id === relation.targetEntityId &&
        coreDimension.secondary_entity_id === relation.firstEntityId);
    if (!isSamePair) {
      return false;
    }
    if (relation.kind === "parallel_line_distance") {
      return coreDimension.kind === "line_line_distance";
    }
    if (relation.kind === "line_angle") {
      return coreDimension.kind === "angle";
    }
    if (relation.kind === "circle_center_distance") {
      return coreDimension.kind === "circle_center_distance";
    }
    return coreDimension.kind === "circle_line_distance";
  }

  function pendingRelationDimension(
    relation: DimensionRelationPreview,
    dimensions: SketchDimensionScene[],
  ) {
    const params = sketchLinesRef.current;
    if (!params) {
      return null;
    }
    const coreDimension = params.dimensions.find((candidate) => {
      const isSamePair =
        (candidate.entity_id === relation.firstEntityId &&
          candidate.secondary_entity_id === relation.targetEntityId) ||
        (candidate.entity_id === relation.targetEntityId &&
          candidate.secondary_entity_id === relation.firstEntityId);
      if (!isSamePair) {
        return false;
      }
      if (relation.kind === "parallel_line_distance") {
        return candidate.kind === "line_line_distance";
      }
      if (relation.kind === "line_angle") {
        return candidate.kind === "angle";
      }
      if (relation.kind === "circle_center_distance") {
        return candidate.kind === "circle_center_distance";
      }
      return candidate.kind === "circle_line_distance";
    });
    if (!coreDimension) {
      return null;
    }
    return (
      dimensions.find(
        (dimension) => dimension.dimensionId === coreDimension.dimension_id,
      ) ?? null
    );
  }

  function stopPendingRelationPlacementRetry() {
    if (pendingRelationPlacementRetryRef.current !== null) {
      window.cancelAnimationFrame(pendingRelationPlacementRetryRef.current);
      pendingRelationPlacementRetryRef.current = null;
    }
  }

  function startPendingRelationPlacementIfReady() {
    if (
      !pendingDimensionPlacementRef.current ||
      activeSketchToolRef.current !== "dimension"
    ) {
      return true;
    }
    const relation = pendingRelationPlacementMatchRef.current;
    if (!relation) {
      return true;
    }
    const placementDimension = pendingRelationDimension(
      relation,
      displayedSketchDimensionsRef.current,
    );
    if (!placementDimension) {
      return false;
    }
    pendingRelationPlacementMatchRef.current = null;
    pendingDimensionIdRef.current = null;
    pendingDimensionPlacementRef.current = false;
    pendingDimSourceEntityIdRef.current = null;
    beginDimensionPlacement(placementDimension);
    return true;
  }

  function schedulePendingRelationPlacementRetry() {
    stopPendingRelationPlacementRetry();
    let attempts = 0;
    const tick = () => {
      if (startPendingRelationPlacementIfReady()) {
        pendingRelationPlacementRetryRef.current = null;
        return;
      }
      attempts += 1;
      if (attempts >= 90) {
        pendingRelationPlacementRetryRef.current = null;
        return;
      }
      pendingRelationPlacementRetryRef.current = window.requestAnimationFrame(tick);
    };
    pendingRelationPlacementRetryRef.current = window.requestAnimationFrame(tick);
  }

  function dimCreatePointDistance(pointAId: string, pointBId: string) {
    pendingDimensionIdRef.current =
      `dim-point-distance-${pointAId}-${pointBId}`;
    pendingDimensionPlacementRef.current = true;
    pendingDimSourceEntityIdRef.current = null;
    void addSketchPointDistanceDimensionRef
      .current(pointAId, pointBId)
      .catch(() => {
        pendingDimensionIdRef.current = null;
        pendingDimensionPlacementRef.current = false;
      });
  }

  function isProjectedCircleDimension(dimensionId: string) {
    const sketch = sketchFeature?.sketch_parameters;
    if (!sketch) {
      return false;
    }
    const dimension = sketch.dimensions.find(
      (candidate) => candidate.dimension_id === dimensionId,
    );
    if (!dimension || dimension.kind !== "circle_radius") {
      return false;
    }
    return sketch.projections.some((projection) =>
      projection.generated_circle_ids.includes(dimension.entity_id),
    );
  }

  // Track which dimension was last clicked, so a second click on the
  // same dimension opens the editor (click to select, re-click to edit).
  const lastClickedDimensionRef = useRef<string | null>(null);

  function handleDimensionClick(dimensionId: string) {
    if (isProjectedCircleDimension(dimensionId)) {
      void selectSketchDimensionRef.current(dimensionId);
      return;
    }

    // Check BOTH the store (accurate after IPC round-trip) AND the
    // local ref (accurate for rapid re-clicks before IPC completes).
    const isAlreadySelected =
      selectedSketchDimension?.dimensionId === dimensionId ||
      lastClickedDimensionRef.current === dimensionId;

    if (isAlreadySelected) {
      // Second click on the already-selected dimension → open editor
      suppressNextDimensionEditorOpenRef.current = false;
      setIsDimensionEditorOpen(true);
    } else {
      // First click → select it (highlight), no editor
      suppressNextDimensionEditorOpenRef.current = true;
      setIsDimensionEditorOpen(false);
      void selectSketchDimensionRef.current(dimensionId);
    }

    lastClickedDimensionRef.current = dimensionId;
  }

  function dimensionDisplayValue(
    dimension: SketchDimensionScene,
    coreValue: number,
  ) {
    if (dimension.kind === "angle" || dimension.kind === "line_angle") {
      return coreValue * (180 / Math.PI);
    }
    if (dimension.kind === "circle_radius") {
      // Per-dimension display_as controls radius vs diameter display.
      // "" (default) = diameter, "radius" = show raw radius.
      const displayAs = resolveDimensionDisplayAs(dimension.dimensionId);
      return displayAs === "radius" ? coreValue : coreValue * 2;
    }
    return coreValue;
  }

  function dimensionCoreValue(
    dimension: SketchDimensionScene,
    displayValue: number,
  ) {
    if (dimension.kind === "angle" || dimension.kind === "line_angle") {
      return displayValue * (Math.PI / 180);
    }
    if (dimension.kind === "circle_radius") {
      // Per-dimension display_as controls radius vs diameter conversion.
      const displayAs = resolveDimensionDisplayAs(dimension.dimensionId);
      return displayAs === "radius" ? displayValue : displayValue / 2;
    }
    return displayValue;
  }

  function formattedDimensionDisplayValue(
    dimension: SketchDimensionScene,
    coreValue: number,
  ) {
    const displayVal = dimensionDisplayValue(dimension, coreValue);
    // Convert mm to user's display unit for non-angle dimensions
    const isAngleKind = dimension.kind === "angle" ||
      dimension.kind === "line_angle";
    const adjusted =
      !isAngleKind
        ? mmToDisplay(displayVal, config.displayUnits)
        : displayVal;
    return String(parseFloat(adjusted.toFixed(2)));
  }

  function setCanvasCursor(cursor: string) {
    const canvas = rendererRef.current?.domElement as
      | HTMLCanvasElement
      | undefined;
    if (canvas) {
      canvas.style.cursor = cursor;
    }
  }

  function getDimensionPlacementAxis(dimension: SketchDimensionScene) {
    if (dimension.kind === "angle" || dimension.kind === "line_angle") {
      return angleDimensionFrame(dimension)?.bisector ?? null;
    }

    const extensionAxis = new THREE.Vector3(...dimension.dimensionStart).sub(
      new THREE.Vector3(...dimension.anchorStart),
    );
    if (extensionAxis.lengthSq() > 1e-8) {
      return extensionAxis.normalize();
    }

    const sketchPlaneId = activeSketchPlaneIdRef.current;
    const dimensionDirection = new THREE.Vector3(...dimension.dimensionEnd).sub(
      new THREE.Vector3(...dimension.dimensionStart),
    );
    if (!sketchPlaneId || dimensionDirection.lengthSq() <= 1e-8) {
      return null;
    }

    const planeNormal = getSketchGridFrame(
      sketchPlaneId,
      activeSketchPlaneFrameRef.current,
    ).normal;
    const placementAxis = planeNormal.cross(dimensionDirection).normalize();
    return placementAxis.lengthSq() > 1e-8 ? placementAxis : null;
  }

  function circleDimensionLabelNearPoint(
    dimension: SketchDimensionScene,
    worldPoint: [number, number, number],
  ): [number, number, number] | null {
    if (dimension.kind !== "circle_radius") {
      return null;
    }
    const center = new THREE.Vector3(...dimension.dimensionStart)
      .add(new THREE.Vector3(...dimension.dimensionEnd))
      .multiplyScalar(0.5);
    const radius =
      new THREE.Vector3(...dimension.dimensionStart).distanceTo(
        new THREE.Vector3(...dimension.dimensionEnd),
      ) * 0.5;
    const direction = new THREE.Vector3(...worldPoint).sub(center);
    const planeNormal = getSketchGridFrame(
      dimension.planeId,
      activeSketchPlaneFrameRef.current,
    ).normal;
    direction.addScaledVector(planeNormal, -direction.dot(planeNormal));
    if (direction.lengthSq() <= 1e-8 || radius <= 1e-8) {
      return null;
    }
    const position = center.add(
      direction.normalize().multiplyScalar(radius + 4),
    );
    return [position.x, position.y, position.z];
  }

  function angleDimensionArcControlNearPoint(
    dimension: SketchDimensionScene,
    worldPoint: [number, number, number],
  ): [number, number, number] | null {
    const frame = angleDimensionFrame(dimension);
    if (!frame) {
      return null;
    }
    const cursor = new THREE.Vector3(...worldPoint);
    const radius = Math.max(
      ANGLE_DIMENSION_MIN_RADIUS,
      Math.min(cursor.distanceTo(frame.pivot), ANGLE_DIMENSION_MAX_RADIUS),
    );
    const direction = cursor.sub(frame.pivot);
    if (direction.lengthSq() <= 1e-8) {
      direction.copy(frame.bisector);
    } else {
      direction.normalize();
    }
    const control = frame.pivot.clone().add(direction.multiplyScalar(radius));
    return [control.x, control.y, control.z];
  }

  function setDimensionLabelPosition(
    dimensionId: string,
    position: [number, number, number],
  ) {
    dimensionLabelPositionsRef.current = {
      ...dimensionLabelPositionsRef.current,
      [dimensionId]: position,
    };
    setDimensionLabelPositions((current) => ({
      ...current,
      [dimensionId]: position,
    }));
  }

  function persistDimensionLabelPosition(
    dimensionId: string,
    position: [number, number, number] | undefined,
  ) {
    if (!position) {
      return;
    }
    const labelLocal = worldPointToSketchLocal(
      position,
      activeSketchPlaneIdRef.current,
      activeSketchPlaneFrameRef.current,
    );
    if (!labelLocal) {
      return;
    }
    void updateSketchDimensionLabelPositionRef.current(
      dimensionId,
      labelLocal[0],
      labelLocal[1],
    );
  }

  function finishDimensionPlacement() {
    const dimensionDrag = dimensionLabelDragRef.current;
    if (!dimensionDrag?.isPlacement) {
      return false;
    }
    persistDimensionLabelPosition(
      dimensionDrag.dimensionId,
      dimensionLabelPositionsRef.current[dimensionDrag.dimensionId],
    );
    clearPreviewDimension();
    dimensionLabelDragRef.current = null;
    dimensionPlacementOriginalPositionRef.current = null;
    controlsRef.current && (controlsRef.current.enabled = true);
    setCanvasCursor("");
    return true;
  }

  function cancelDimensionPlacement() {
    const dimensionDrag = dimensionLabelDragRef.current;
    if (!dimensionDrag?.isPlacement) {
      return false;
    }
    clearPreviewDimension();
    const originalPosition = dimensionPlacementOriginalPositionRef.current;
    if (originalPosition) {
      setDimensionLabelPositions((current) => ({
        ...current,
        [dimensionDrag.dimensionId]: originalPosition,
      }));
    }
    dimensionLabelDragRef.current = null;
    dimensionPlacementOriginalPositionRef.current = null;
    controlsRef.current && (controlsRef.current.enabled = true);
    setCanvasCursor("");
    return true;
  }

  function beginDimensionPlacement(dimension: SketchDimensionScene) {
    const renderer = rendererRef.current;
    const camera = cameraRef.current;
    const sketchPlaneId = activeSketchPlaneIdRef.current;
    const controls = controlsRef.current;
    const pointerEvent = lastPointerEventRef.current;
    if (!renderer || !camera || !sketchPlaneId || !controls || !pointerEvent) {
      return;
    }
    const sketchPoint = resolveSketchPlanePoint(
      pointerEvent,
      renderer,
      camera,
      sketchPlaneId,
      activeSketchPlaneFrameRef.current,
    );
    if (!sketchPoint) {
      return;
    }
    const originalPosition = dimension.labelPosition;
    const circlePosition =
      dimension.kind === "circle_radius"
        ? circleDimensionLabelNearPoint(dimension, sketchPoint.world)
        : null;
    const dragAxis =
      dimension.kind === "circle_radius"
        ? new THREE.Vector3(0, 0, 0)
        : getDimensionPlacementAxis(dimension);
    if (dimension.kind !== "circle_radius" && !dragAxis) {
      return;
    }
    const anglePosition =
      dimension.kind === "angle" || dimension.kind === "line_angle"
        ? angleDimensionArcControlNearPoint(dimension, sketchPoint.world)
        : null;
    const relationPosition =
      dimension.kind === "line_line_distance" ||
      dimension.kind === "circle_line_distance" ||
      dimension.kind === "circle_center_distance" ||
      dimension.kind === "angle" ||
      dimension.kind === "line_angle"
        ? pendingRelationPlacementLabelRef.current
        : null;
    pendingRelationPlacementLabelRef.current = null;
    const nextPosition =
      relationPosition ??
      circlePosition ??
      anglePosition ??
      (() => {
        if (!dragAxis) {
          return originalPosition;
        }
        const originalPositionVector = new THREE.Vector3(...originalPosition);
        const pointerDelta = new THREE.Vector3(...sketchPoint.world).sub(
          originalPositionVector,
        );
        const nextPositionVector = originalPositionVector
          .clone()
          .add(dragAxis.clone().multiplyScalar(pointerDelta.dot(dragAxis)));
        return [
          nextPositionVector.x,
          nextPositionVector.y,
          nextPositionVector.z,
        ] as [number, number, number];
      })();
    dimensionPlacementOriginalPositionRef.current = dimension.labelPosition;
    setDimensionLabelPosition(dimension.dimensionId, nextPosition);
    dimensionLabelDragRef.current = {
      dimensionId: dimension.dimensionId,
      startClientX: pointerEvent.clientX,
      startClientY: pointerEvent.clientY,
      startWorld: sketchPoint.world,
      startLabelPosition: nextPosition,
      dragAxis: dragAxis ? [dragAxis.x, dragAxis.y, dragAxis.z] : [0, 0, 0],
      hasMoved: true,
      isPlacement: true,
    };
    controls.enabled = false;
    setCanvasCursor("grabbing");
  }

  function sketchLinesShareEndpoint(firstLineId: string, secondLineId: string) {
    const params = sketchLinesRef.current;
    const first = params?.lines.find((line) => line.line_id === firstLineId);
    const second = params?.lines.find((line) => line.line_id === secondLineId);
    if (!first || !second) {
      return false;
    }
    return (
      first.start_point_id === second.start_point_id ||
      first.start_point_id === second.end_point_id ||
      first.end_point_id === second.start_point_id ||
      first.end_point_id === second.end_point_id
    );
  }

  function cancelActiveSketchDraft() {
    if (armedSketchConstraintRef.current) {
      cancelSketchConstraintRef.current();
      return;
    }
    lineDraftStartRef.current = null;
    arcSecondPointRef.current = null;
    rectSecondPointRef.current = null;
    circleSecondPointRef.current = null;
    clearPreviewLine();
    clearPreviewCircle();
    clearPreviewArc();
    clearPreviewDimension();
    clearDraftDimensionSession();
    setSketchSnapLabel(null);
    setConstraintPreview(null);
    void setSketchToolRef.current("select");
  }

  function makeDraftLineMaterial() {
    if (sketchToolConstructionRef.current) {
      return new THREE.LineDashedMaterial({
        color: themeColor("--color-tertiary-plane-edge", "#ffe784"),
        transparent: true,
        opacity: 0.72,
        dashSize: 1,
        gapSize: 0.6,
      });
    }
    return new THREE.LineBasicMaterial({
      color: themeColor("--color-tertiary-plane-edge", "#ffe784"),
      transparent: true,
      opacity: 0.88,
    });
  }

  function renderDraftPreview(session: DraftDimensionSession) {
    const sketchGroup = sketchGroupRef.current;
    if (!sketchGroup || !activeSketchPlaneId) {
      return;
    }

    clearPreviewLine();
    clearPreviewCircle();
    clearPreviewArc();
    clearPreviewDimension();
    const [sx, sy] = session.start;
    const [ex, ey] = session.current;

    if (session.tool === "rectangle") {
      const rectMode = rectangleToolModeRef.current;
      if (rectMode === "three_point") {
        const rectSecondPoint = rectSecondPointRef.current;
        if (!rectSecondPoint) {
          // Click 2 still pending — dashed first-edge hint.
          const preview = new THREE.Line(
            new THREE.BufferGeometry().setFromPoints([
              new THREE.Vector3(
                ...toWorldPoint(activeSketchPlaneId, [sx, sy], activeSketchPlaneFrame),
              ),
              new THREE.Vector3(
                ...toWorldPoint(activeSketchPlaneId, [ex, ey], activeSketchPlaneFrame),
              ),
            ]),
            new THREE.LineDashedMaterial({
              color: themeColor("--color-tertiary-plane-edge", "#ffe784"),
              transparent: true,
              opacity: 0.65,
              dashSize: 1,
              gapSize: 0.6,
            }),
          );
          preview.computeLineDistances();
          previewLineRef.current = preview;
          sketchGroup.add(preview);
          return;
        }
        // Click 2 captured: compute 4 corners from (start, second, current).
        const [p1x, p1y] = [sx, sy];
        const [p2x, p2y] = rectSecondPoint;
        const [p3x, p3y] = [ex, ey];
        const dx = p2x - p1x;
        const dy = p2y - p1y;
        const edgeLen = Math.hypot(dx, dy);
        if (edgeLen >= 1e-9) {
          const nx = -dy / edgeLen;
          const ny = dx / edgeLen;
          const offset = nx * (p3x - p1x) + ny * (p3y - p1y);
          const c1x = p1x + nx * offset;
          const c1y = p1y + ny * offset;
          const c2x = p2x + nx * offset;
          const c2y = p2y + ny * offset;
          const corners: Array<[number, number]> = [
            [p1x, p1y],
            [p2x, p2y],
            [c2x, c2y],
            [c1x, c1y],
            [p1x, p1y],
          ];
          const preview = new THREE.Line(
            new THREE.BufferGeometry().setFromPoints(
              corners.map(
                (corner) =>
                  new THREE.Vector3(
                    ...toWorldPoint(activeSketchPlaneId, corner, activeSketchPlaneFrame),
                  ),
              ),
            ),
            makeDraftLineMaterial(),
          );
          if (sketchToolConstructionRef.current) {
            preview.computeLineDistances();
          }
          previewLineRef.current = preview;
          sketchGroup.add(preview);
        }
        return;
      }
      // corner_corner / center_point: full-rectangle draft.
      const isCenterPoint = rectMode === "center_point";
      const corners: Array<[number, number]> = isCenterPoint
        ? [
            [2 * sx - ex, 2 * sy - ey],
            [ex, 2 * sy - ey],
            [ex, ey],
            [2 * sx - ex, ey],
            [2 * sx - ex, 2 * sy - ey],
          ]
        : [
            [sx, sy],
            [ex, sy],
            [ex, ey],
            [sx, ey],
            [sx, sy],
          ];
      const preview = new THREE.Line(
        new THREE.BufferGeometry().setFromPoints(
          corners.map(
            (corner) =>
              new THREE.Vector3(
                ...toWorldPoint(activeSketchPlaneId, corner, activeSketchPlaneFrame),
              ),
          ),
        ),
        makeDraftLineMaterial(),
      );
      if (sketchToolConstructionRef.current) {
        preview.computeLineDistances();
      }
      previewLineRef.current = preview;
      sketchGroup.add(preview);
      return;
    }

    if (session.tool === "circle") {
      const circleMode = circleToolModeRef.current;
      if (circleMode === "two_point") {
        // Two-point (diameter) circle: center at midpoint, radius = dist/2.
        const dist = distanceBetweenPoints(session.start, session.current);
        if (dist <= 0.001) {
          return;
        }
        const cx = (session.start[0] + session.current[0]) / 2;
        const cy = (session.start[1] + session.current[1]) / 2;
        const radius = dist / 2;
        const preview = buildSketchCircleObject(
          {
            circleId: "preview-2pt-circle",
            planeId: activeSketchPlaneId,
            planeFrame: activeSketchPlaneFrame,
            center: toWorldPoint(
              activeSketchPlaneId,
              [cx, cy],
              activeSketchPlaneFrame,
            ),
            radius,
            isSelected: false,
            isConstruction: sketchToolConstructionRef.current,
            isPreview: false,
            isProjected: false,
          },
          activeSketchPlaneFrame,
        );
        previewCircleRef.current = preview;
        sketchGroup.add(preview);
        return;
      }
      if (circleMode === "three_point") {
        // Three-point circle: two-phase preview matching handlePointerMove.
        const circleSecondPoint = circleSecondPointRef.current;
        if (!circleSecondPoint) {
          // Click 2 still pending — dashed chord hint.
          const preview = new THREE.Line(
            new THREE.BufferGeometry().setFromPoints([
              new THREE.Vector3(
                ...toWorldPoint(activeSketchPlaneId, session.start, activeSketchPlaneFrame),
              ),
              new THREE.Vector3(
                ...toWorldPoint(activeSketchPlaneId, session.current, activeSketchPlaneFrame),
              ),
            ]),
            new THREE.LineDashedMaterial({
              color: themeColor("--color-tertiary-plane-edge", "#ffe784"),
              transparent: true,
              opacity: 0.65,
              dashSize: 1,
              gapSize: 0.6,
            }),
          );
          preview.computeLineDistances();
          previewLineRef.current = preview;
          sketchGroup.add(preview);
        } else {
          // Click 2 captured: cursor (session.current) is the third point.
          const [p1x, p1y] = session.start;
          const [p2x, p2y] = circleSecondPoint;
          const [p3x, p3y] = session.current;
          const d = 2 * (p1x * (p2y - p3y) + p2x * (p3y - p1y) + p3x * (p1y - p2y));
          if (Math.abs(d) > 1e-9) {
            const ux =
              ((p1x*p1x + p1y*p1y)*(p2y - p3y) + (p2x*p2x + p2y*p2y)*(p3y - p1y) + (p3x*p3x + p3y*p3y)*(p1y - p2y)) / d;
            const uy =
              ((p1x*p1x + p1y*p1y)*(p3x - p2x) + (p2x*p2x + p2y*p2y)*(p1x - p3x) + (p3x*p3x + p3y*p3y)*(p2x - p1x)) / d;
            const radius = Math.hypot(p1x - ux, p1y - uy);
            if (radius >= 1e-3) {
              const preview = buildSketchCircleObject(
                {
                  circleId: "preview-3pt-circle",
                  planeId: activeSketchPlaneId,
                  planeFrame: activeSketchPlaneFrame,
                  center: toWorldPoint(activeSketchPlaneId, [ux, uy], activeSketchPlaneFrame),
                  radius,
                  isSelected: false,
                  isConstruction: sketchToolConstructionRef.current,
                  isPreview: true,
                  isProjected: false,
                },
                activeSketchPlaneFrame,
              );
              previewCircleRef.current = preview;
              sketchGroup.add(preview);
            }
          }
        }
        return;
      }
      // center_radius: existing draft circle from center.
      const radius = distanceBetweenPoints(session.start, session.current);
      if (radius <= 0.001) {
        return;
      }
      const preview = buildSketchCircleObject(
        {
          circleId: "preview-circle",
          planeId: activeSketchPlaneId,
          planeFrame: activeSketchPlaneFrame,
          center: toWorldPoint(
            activeSketchPlaneId,
            session.start,
            activeSketchPlaneFrame,
          ),
          radius,
          isSelected: false,
          isConstruction: sketchToolConstructionRef.current,
          isPreview: false,
          isProjected: false,
        },
        activeSketchPlaneFrame,
      );
      previewCircleRef.current = preview;
      sketchGroup.add(preview);
      renderCircleDraftDimension(sketchGroup, session.start, session.current);
      return;
    }

    // Rectangle and circle tools are handled above with early returns;
    // only the line tool reaches this fallback.
    const localPoints: Array<[number, number]> = [session.start, session.current];
    const preview = new THREE.Line(
      new THREE.BufferGeometry().setFromPoints(
        localPoints.map(
          (point) =>
            new THREE.Vector3(
              ...toWorldPoint(activeSketchPlaneId, point, activeSketchPlaneFrame),
            ),
        ),
      ),
      makeDraftLineMaterial(),
    );
    if (sketchToolConstructionRef.current) {
      preview.computeLineDistances();
    }
    previewLineRef.current = preview;
    sketchGroup.add(preview);
  }

  function renderCircleDraftDimension(
    sketchGroup: THREE.Group,
    center: [number, number],
    edge: [number, number],
  ) {
    if (!activeSketchPlaneId) {
      return;
    }
    const radius = distanceBetweenPoints(center, edge);
    const dx = edge[0] - center[0];
    const dy = edge[1] - center[1];
    const length = Math.hypot(dx, dy);
    if (radius <= 0.001 || length <= 1e-6) {
      return;
    }

    const ux = dx / length;
    const uy = dy / length;
    const dimensionStartLocal: [number, number] = [
      center[0] - ux * radius,
      center[1] - uy * radius,
    ];
    const dimensionEndLocal: [number, number] = [
      center[0] + ux * radius,
      center[1] + uy * radius,
    ];
    const labelLocal: [number, number] = [
      center[0] + ux * (radius + 4),
      center[1] + uy * (radius + 4),
    ];
    const draftDimension = buildSketchDimensionObject({
      dimensionId: "preview-circle-diameter",
      planeId: activeSketchPlaneId,
      kind: "circle_radius",
      entityId: "preview-circle",
      label: `D ${formatDraftDimension(radius * 2)} mm`,
      rawValue: radius * 2,
      unitSuffix: "mm",
      isSelected: false,
      anchorStart: toWorldPoint(
        activeSketchPlaneId,
        dimensionStartLocal,
        activeSketchPlaneFrame,
      ),
      anchorEnd: toWorldPoint(
        activeSketchPlaneId,
        dimensionEndLocal,
        activeSketchPlaneFrame,
      ),
      dimensionStart: toWorldPoint(
        activeSketchPlaneId,
        dimensionStartLocal,
        activeSketchPlaneFrame,
      ),
      dimensionEnd: toWorldPoint(
        activeSketchPlaneId,
        dimensionEndLocal,
        activeSketchPlaneFrame,
      ),
      labelPosition: toWorldPoint(
        activeSketchPlaneId,
        labelLocal,
        activeSketchPlaneFrame,
      ),
    });
    previewDimensionRef.current = draftDimension;
    sketchGroup.add(draftDimension.line);
    sketchGroup.add(draftDimension.label);
  }

  function updateDraftSessionFromPoint(point: [number, number]) {
    const session = draftDimensionSessionRef.current;
    if (!session) {
      return;
    }
    const next = updateDraftSessionCurrent(session, point);
    draftDimensionSessionRef.current = next;
    setDraftDimensionSession(next);
    if (!next.lockedFields[next.activeField]) {
      focusDraftField(next.activeField);
    }
  }

  function applyDraftDimensionField(
    session: DraftDimensionSession,
    field: DraftDimensionField,
    rawValue: string,
    lockField = true,
  ): DraftDimensionSession {
    return applyDraftDimensionFieldValue(session, field, rawValue, lockField);
  }

  async function commitDraftDimensionSession(
    session = draftDimensionSessionRef.current,
  ) {
    if (!session) {
      return;
    }
    const [startX, startY] = session.start;
    const [endX, endY] = session.current;
    clearPreviewLine();
    clearPreviewCircle();
    clearPreviewArc();
    clearPreviewDimension();
    lineDraftStartRef.current = null;
    scheduleDimensionDeletion(session.tool, session);
    scheduleDraftDimensionExpressionUpdate(session.tool);
    clearDraftDimensionSession();
    suppressDimensionEditorAfterSketchCommit();
    rendererRef.current?.domElement.focus();

    if (session.tool === "rectangle") {
      if (rectangleToolModeRef.current === "three_point") {
        // 3-point mode can't commit from drag; handled in snap handler.
        return;
      }
      const rectStartX =
        rectangleToolModeRef.current === "center_point"
          ? 2 * startX - endX
          : startX;
      const rectStartY =
        rectangleToolModeRef.current === "center_point"
          ? 2 * startY - endY
          : startY;
      await addSketchRectangleRef.current(
        rectStartX,
        rectStartY,
        endX,
        endY,
        sketchToolConstructionRef.current,
      );
      return;
    }
    if (session.tool === "circle") {
      const circleMode = circleToolModeRef.current;
      let cx = startX;
      let cy = startY;
      let r = distanceBetweenPoints(session.start, session.current);
      if (circleMode === "two_point") {
        // 2-point circle: start/end are diameter endpoints
        cx = (startX + endX) / 2;
        cy = (startY + endY) / 2;
        r = distanceBetweenPoints(session.start, session.current) / 2;
      }
      // 3-point and tangent modes can't commit from a 2-click drag
      if (circleMode === "three_point" || circleMode === "tangent_two_lines" || circleMode === "tangent_three_lines") {
        return;
      }
      pendingCircleDimensionPlacementRef.current = {
        fromCircleCount: sketchFeature?.sketch_parameters?.circles.length ?? 0,
        center: [cx, cy],
        end: session.current,
      };
      await addSketchCircleRef.current(
        cx,
        cy,
        r,
        sketchToolConstructionRef.current,
      );
      return;
    }
    if (session.tool === "polygon") {
      void addSketchPolygonRef.current(
        polygonSidesRef.current,
        polygonToolModeRef.current,
        startX,
        startY,
        endX,
        endY,
        sketchToolConstructionRef.current,
      );
      return;
    }
    await addSketchLineRef.current(
      startX,
      startY,
      endX,
      endY,
      sketchToolConstructionRef.current,
    );
  }

  /** Read the selection filter from localStorage synchronously. */
  function readLocalFilter(): SelectionFilter | null {
    try {
      const raw = localStorage.getItem("polysmith-selection-filter");
      if (raw) return JSON.parse(raw) as SelectionFilter;
    } catch { /* ignore */ }
    return null;
  }

  function resolveSnappedSketchPoint(rawPoint: {
    local: [number, number];
    world: [number, number, number];
  }) {
    // Read filter from localStorage (instant, no IPC round trip).
    const localFilter = readLocalFilter();
    const effectiveFilter: typeof localFilter = localFilter && altHeldRef.current
      ? {
          ...localFilter,
          select_curves: !localFilter.select_curves,
          select_points: !localFilter.select_points,
          select_construction: !localFilter.select_construction,
          select_constraints: !localFilter.select_constraints,
          snap_endpoint: !localFilter.snap_endpoint,
          snap_midpoint: !localFilter.snap_midpoint,
          snap_center: !localFilter.snap_center,
          snap_intersection: !localFilter.snap_intersection,
          snap_nearest: !localFilter.snap_nearest,
          snap_quadrant: !localFilter.snap_quadrant,
          snap_perpendicular: !localFilter.snap_perpendicular,
          snap_parallel: !localFilter.snap_parallel,
          snap_tangent: !localFilter.snap_tangent,
          snap_grid: !localFilter.snap_grid,
          snap_grid_line: !localFilter.snap_grid_line,
          snap_polar: !localFilter.snap_polar,
          magnetic_pull: !localFilter.magnetic_pull,
        }
      : localFilter;

    const gridSnapEnabled = effectiveFilter ? effectiveFilter.snap_grid : true;
    const perpEnabled = effectiveFilter ? effectiveFilter.snap_perpendicular : true;

    // Apply grid snap first — all subsequent geometric snaps resolve
    // against grid-aligned coordinates so the user gets both.
    const worldUnitsPerPixel =
      cameraRef.current && rendererRef.current
        ? getOrthographicViewHeight(cameraRef.current) /
            Math.max(rendererRef.current.domElement.clientHeight, 1)
        : 1;
    const gridResult = snapRawPointToGrid(rawPoint, worldUnitsPerPixel, gridSnapEnabled);
    let gridDidSnap = gridResult.snapped;
    if (gridResult.snapped) {
      rawPoint.local = gridResult.point.local;
      rawPoint.world = gridResult.point.world;
    }

    let closestCandidate:
      | (typeof sketchSnapCandidatesRef.current)[number]
      | null = null;
    let closestDistance = Number.POSITIVE_INFINITY;

    for (const candidate of sketchSnapCandidatesRef.current) {
      // Gate on the selection filter when available.
      if (effectiveFilter) {
        const allowed = (
          (candidate.kind === "endpoint" && effectiveFilter.snap_endpoint) ||
          (candidate.kind === "midpoint" && effectiveFilter.snap_midpoint) ||
          (candidate.kind === "center" && effectiveFilter.snap_center) ||
          (candidate.kind === "intersection" && effectiveFilter.snap_intersection) ||
          (candidate.kind === "nearest" && effectiveFilter.snap_nearest) ||
          (candidate.kind === "tangent" && effectiveFilter.snap_tangent) ||
          (!candidate.kind) // unknown kinds pass unfiltered
        );
        if (!allowed) continue;
      }
      const distance = distanceBetweenPoints(rawPoint.local, candidate.local);
      if (distance < closestDistance) {
        closestDistance = distance;
        closestCandidate = candidate;
      }
    }

    // Endpoint / midpoint candidates win immediately (high priority).
    // Center, quadrant, intersection, and unknown kinds yield to
    // dynamic snaps (tangent, perpendicular, parallel) below.
    if (closestCandidate && closestDistance <= SKETCH_SNAP_DISTANCE) {
      if (closestCandidate.kind === "endpoint" ||
          closestCandidate.kind === "midpoint") {
        return {
          local: closestCandidate.local,
          world: toWorldPoint(
            activeSketchPlaneId ?? "ref-plane-xy",
            closestCandidate.local,
            activeSketchPlaneFrame,
          ),
          snapLabel: closestCandidate.label,
          snapMidpointHostLineId:
            closestCandidate.kind === "midpoint"
              ? (closestCandidate.hostLineId ?? null)
              : null,
          snapMidpointT:
            closestCandidate.kind === "midpoint"
              ? (closestCandidate.tValue ?? null)
              : null,
          snapPerpendicularHostLineId: null,
          snapEndpointHostLineId:
            closestCandidate.kind === "endpoint"
              ? (closestCandidate.endpointHostLineId ?? null)
              : null,
        } satisfies SketchPreviewPoint;
      }
      // Non-endpoint/midpoint candidates: don't return yet —
      // let dynamic snaps (tangent, perpendicular) compete.
    }

    const startPoint = lineDraftStartRef.current;
    const params = sketchLinesRef.current;

    // General perpendicular-to-line snap — REMOVED. The on-line snap
    // below handles line body placement correctly with a narrow
    // activation window (like midpoint). The perpendicular-foot snap
    // above handles the "start on host line" case. No separate
    // general perpendicular needed.
    if (false && localFilter?.snap_perpendicular && startPoint && params) {
      let bestPerpSnap: {
        local: [number, number];
        distance: number;
        lineId: string;
      } | null = null;
      for (const line of params.lines) {
        if (line.is_construction) continue;
        const dx = line.end_x - line.start_x;
        const dy = line.end_y - line.start_y;
        const lenSq = dx * dx + dy * dy;
        if (lenSq < 1e-12) continue;
        let t =
          ((rawPoint.local[0] - line.start_x) * dx +
           (rawPoint.local[1] - line.start_y) * dy) /
          lenSq;
        t = Math.max(0, Math.min(1, t));
        const px = line.start_x + t * dx;
        const py = line.start_y + t * dy;
        const d = Math.hypot(rawPoint.local[0] - px, rawPoint.local[1] - py);
        if (d > SKETCH_SNAP_DISTANCE) continue;
        if (!bestPerpSnap || d < bestPerpSnap.distance) {
          bestPerpSnap = { local: [px, py], distance: d, lineId: line.line_id };
        }
      }
      if (bestPerpSnap) {
        return {
          local: bestPerpSnap.local,
          world: toWorldPoint(
            activeSketchPlaneId ?? "ref-plane-xy",
            bestPerpSnap.local,
            activeSketchPlaneFrame,
          ),
          snapLabel: translate("snap.perpendicular"),
          snapMidpointHostLineId: null,
          snapPerpendicularHostLineId: bestPerpSnap.lineId,
          snapEndpointHostLineId: null,
        } satisfies SketchPreviewPoint;
      }
    }

    // Dynamic perpendicular-foot snap. Active only on the second
    // click of a line draft, when the start lay on an existing line
    // (`draftStartEndpointHostRef` is set). Project the cursor onto
    // the ray rooted at the start, normal to the host line. If the
    // cursor is within snap distance of that ray, snap to the foot.
    const perpHostId = draftStartEndpointHostRef.current;
    if (effectiveFilter?.snap_perpendicular && startPoint && perpHostId && params) {
      const hostLine = params.lines.find((line) => line.line_id === perpHostId);
      if (hostLine) {
        const dx = hostLine.end_x - hostLine.start_x;
        const dy = hostLine.end_y - hostLine.start_y;
        const lengthSquared = dx * dx + dy * dy;
        if (lengthSquared > 1e-12) {
          const length = Math.sqrt(lengthSquared);
          // Perpendicular direction (rotate host line direction by
          // +90°). Normalized so we can project cleanly.
          const perpX = -dy / length;
          const perpY = dx / length;
          const dxFromStart = rawPoint.local[0] - startPoint[0];
          const dyFromStart = rawPoint.local[1] - startPoint[1];
          const t = dxFromStart * perpX + dyFromStart * perpY;
          const footX = startPoint[0] + t * perpX;
          const footY = startPoint[1] + t * perpY;
          const distanceFromRay = Math.hypot(
            rawPoint.local[0] - footX,
            rawPoint.local[1] - footY,
          );
          if (distanceFromRay <= SKETCH_SNAP_DISTANCE) {
            return {
              local: [footX, footY] as [number, number],
              world: toWorldPoint(
                activeSketchPlaneId ?? "ref-plane-xy",
                [footX, footY],
                activeSketchPlaneFrame,
              ),
              snapLabel: translate("snap.perpendicular"),
              snapMidpointHostLineId: null,
              snapPerpendicularHostLineId: perpHostId,
              snapEndpointHostLineId: null,
            } satisfies SketchPreviewPoint;
          }
        }
      }
    }

    // Tangent snap: while drafting a line from outside a circle, the
    // cursor sticks to whichever of the two tangent points it's
    // nearest. Given start S, center C, radius r and d = |C - S|,
    // the tangent points sit at distance L = sqrt(d² - r²) from S
    // along directions ±θ off S→C, where sin θ = r/d. Skipped when
    // the start lies inside or on the circle (no real tangent
    // exists). Higher priority than axis-lock so an explicit
    // "draw to this circle" gesture wins over a passive axis hint.
    if (effectiveFilter?.snap_tangent !== false && startPoint && params) {
      let bestTangentSnap: {
        local: [number, number];
        distance: number;
        circleId: string;
      } | null = null;
      for (const circle of params.circles) {
        const dx = circle.center_x - startPoint[0];
        const dy = circle.center_y - startPoint[1];
        const dSquared = dx * dx + dy * dy;
        const rSquared = circle.radius * circle.radius;
        if (dSquared <= rSquared + 1e-9) {
          continue;
        }
        const d = Math.sqrt(dSquared);
        const tangentLength = Math.sqrt(dSquared - rSquared);
        const ux = dx / d;
        const uy = dy / d;
        const vx = -uy;
        const vy = ux;
        const along = (tangentLength * tangentLength) / d;
        const perp = (tangentLength * circle.radius) / d;
        const candidates: Array<[number, number]> = [
          [
            startPoint[0] + along * ux + perp * vx,
            startPoint[1] + along * uy + perp * vy,
          ],
          [
            startPoint[0] + along * ux - perp * vx,
            startPoint[1] + along * uy - perp * vy,
          ],
        ];
        for (const [tx, ty] of candidates) {
          const distance = Math.hypot(
            rawPoint.local[0] - tx,
            rawPoint.local[1] - ty,
          );
          if (distance > SKETCH_SNAP_DISTANCE) {
            continue;
          }
          if (!bestTangentSnap || distance < bestTangentSnap.distance) {
            bestTangentSnap = {
              local: [tx, ty],
              distance,
              circleId: circle.circle_id,
            };
          }
        }
      }
      if (bestTangentSnap) {
        return {
          local: bestTangentSnap.local,
          world: toWorldPoint(
            activeSketchPlaneId ?? "ref-plane-xy",
            bestTangentSnap.local,
            activeSketchPlaneFrame,
          ),
          snapLabel: translate("snap.tangent"),
          snapMidpointHostLineId: null,
          snapPerpendicularHostLineId: null,
          snapEndpointHostLineId: null,
          snapTangentCircleId: bestTangentSnap.circleId,
        } satisfies SketchPreviewPoint;
      }
    }

    // Axis lock: while a draft is in progress, if the segment from
    // start → cursor lies within ~3° of the world horizontal or
    // vertical axis, pull the off-axis coordinate onto the start so
    // the line lands flat. We check H first (CAD convention),
    // and gate on a minimum draft length so the lock doesn't fight
    // the user during the first few pixels of motion. Threshold uses
    // sin(3°) ≈ 0.0523 against `|orthogonal| / hypot`. Higher
    // priority than line-body snap by default, but we co-snap to a
    // crossed line when the locked position is within snap distance
    // of one — that way an axis-locked stroke that meets a host
    // line at right angles still gets a coincident anchor on commit
    // (the user's "go straight down to the bottom side" gesture).
    if (startPoint) {
      const ax = rawPoint.local[0] - startPoint[0];
      const ay = rawPoint.local[1] - startPoint[1];
      const hypot = Math.hypot(ax, ay);
      const minDraftLength = SKETCH_SNAP_DISTANCE * 1.5;
      if (hypot >= minDraftLength) {
        const sinThreshold = Math.sin((3 * Math.PI) / 180);
        const horizontalRatio = Math.abs(ay) / hypot;
        const verticalRatio = Math.abs(ax) / hypot;

        const buildAxisLockSnap = (
          axis: "horizontal" | "vertical",
          lockedLocal: [number, number],
        ): SketchPreviewPoint => {
          // After the axis lock fixes the off-axis coordinate, look
          // for a line the lock ray actually crosses (within the
          // line's segment) and within snap distance of the locked
          // position. We use a proper ray/segment intersection
          // rather than perpendicular projection so a line parallel
          // to the lock axis (e.g. the rectangle's other vertical
          // side under a vertical lock) doesn't generate a phantom
          // coincident anchor — those lines have no real crossing.
          const linesParamsForLock = sketchLinesRef.current;
          let crossedLine: {
            intersectionLocal: [number, number];
            lineId: string;
            t: number;
          } | null = null;
          if (linesParamsForLock) {
            let bestDistance = SKETCH_SNAP_DISTANCE;
            for (const line of linesParamsForLock.lines) {
              const dx = line.end_x - line.start_x;
              const dy = line.end_y - line.start_y;
              // Vertical lock crosses lines with non-zero dx;
              // horizontal lock crosses lines with non-zero dy.
              // Skip lines that are parallel to the lock axis
              // because the ray either misses them entirely or
              // overlaps them (no single intersection point).
              const denom = axis === "vertical" ? dx : dy;
              if (Math.abs(denom) <= 1e-9) {
                continue;
              }
              const t =
                axis === "vertical"
                  ? (startPoint[0] - line.start_x) / dx
                  : (startPoint[1] - line.start_y) / dy;
              if (t < 0 || t > 1) {
                continue;
              }
              const ix = line.start_x + t * dx;
              const iy = line.start_y + t * dy;
              // Distance is along the lock axis only — `lockedLocal`
              // shares the off-axis coordinate with the crossing.
              const distance =
                axis === "vertical"
                  ? Math.abs(iy - lockedLocal[1])
                  : Math.abs(ix - lockedLocal[0]);
              if (distance > bestDistance) {
                continue;
              }
              bestDistance = distance;
              crossedLine = {
                intersectionLocal: [ix, iy],
                lineId: line.line_id,
                t,
              };
            }
          }

          if (crossedLine) {
            return {
              local: crossedLine.intersectionLocal,
              world: toWorldPoint(
                activeSketchPlaneId ?? "ref-plane-xy",
                crossedLine.intersectionLocal,
                activeSketchPlaneFrame,
              ),
              snapLabel:
                axis === "horizontal"
                  ? translate("toolbar.horizontal")
                  : translate("toolbar.vertical"),
              snapMidpointHostLineId: null,
              snapPerpendicularHostLineId: null,
              snapEndpointHostLineId: null,
              snapLineBodyHostLineId: crossedLine.lineId,
              snapLineBodyT: crossedLine.t,
              snapAxisLock: axis,
            } satisfies SketchPreviewPoint;
          }

          return {
            local: lockedLocal,
            world: toWorldPoint(
              activeSketchPlaneId ?? "ref-plane-xy",
              lockedLocal,
              activeSketchPlaneFrame,
            ),
            snapLabel:
              axis === "horizontal"
                ? translate("toolbar.horizontal")
                : translate("toolbar.vertical"),
            snapMidpointHostLineId: null,
            snapPerpendicularHostLineId: null,
            snapEndpointHostLineId: null,
            snapAxisLock: axis,
          } satisfies SketchPreviewPoint;
        };

        if (horizontalRatio < sinThreshold) {
          return buildAxisLockSnap("horizontal", [
            rawPoint.local[0],
            startPoint[1],
          ]);
        }
        if (verticalRatio < sinThreshold) {
          return buildAxisLockSnap("vertical", [
            startPoint[0],
            rawPoint.local[1],
          ]);
        }
      }
    }

    // Parallel snap: when `snap_parallel` is on and a start point
    // exists, lock the cursor to the nearest existing line's
    // direction. Finds the line whose angle is closest to the
    // cursor ray from the start, then projects the cursor onto
    // that direction. Allows both forward and reverse parallel
    // (0 and 180 degrees relative).
    if (effectiveFilter?.snap_parallel && startPoint && params) {
      const cursorDx = rawPoint.local[0] - startPoint[0];
      const cursorDy = rawPoint.local[1] - startPoint[1];
      const cursorDist = Math.hypot(cursorDx, cursorDy);
      if (cursorDist > SKETCH_SNAP_DISTANCE * 1.5) {
        const cursorAngle = Math.atan2(cursorDy, cursorDx);
        const threshold = Math.PI / 18;
        let bestParallel: {
          local: [number, number];
          distance: number;
        } | null = null;
        for (const line of params.lines) {
          if (line.is_construction) continue;
          const lineAngle = Math.atan2(
            line.end_y - line.start_y,
            line.end_x - line.start_x,
          );
          for (const dir of [lineAngle, lineAngle + Math.PI]) {
            let angleDiff = Math.abs(cursorAngle - dir);
            if (angleDiff > Math.PI) angleDiff = 2 * Math.PI - angleDiff;
            if (angleDiff > threshold) continue;
            const cosA = Math.cos(dir);
            const sinA = Math.sin(dir);
            const t = cursorDx * cosA + cursorDy * sinA;
            const px = startPoint[0] + t * cosA;
            const py = startPoint[1] + t * sinA;
            const d = Math.hypot(rawPoint.local[0] - px, rawPoint.local[1] - py);
            if (d <= SKETCH_SNAP_DISTANCE) {
              if (!bestParallel || d < bestParallel.distance) {
                bestParallel = { local: [px, py], distance: d };
              }
            }
          }
        }
        if (bestParallel) {
          return {
            local: bestParallel.local,
            world: toWorldPoint(
              activeSketchPlaneId ?? "ref-plane-xy",
              bestParallel.local,
              activeSketchPlaneFrame,
            ),
            snapLabel: translate("snap.parallel"),
            snapMidpointHostLineId: null,
            snapPerpendicularHostLineId: null,
            snapEndpointHostLineId: null,
          } satisfies SketchPreviewPoint;
        }
      }
    }

    // Fallback: if a lower-priority static candidate (center, quadrant,
    // intersection, etc.) matched but dynamic snaps didn't fire, return
    // the static candidate now.
    if (closestCandidate && closestDistance <= SKETCH_SNAP_DISTANCE) {
      return {
        local: closestCandidate.local,
        world: toWorldPoint(
          activeSketchPlaneId ?? "ref-plane-xy",
          closestCandidate.local,
          activeSketchPlaneFrame,
        ),
        snapLabel: closestCandidate.label,
        snapMidpointHostLineId: null,
        snapPerpendicularHostLineId: null,
        snapEndpointHostLineId: null,
      } satisfies SketchPreviewPoint;
    }

    // Line-body snap: when no point candidate matched, project the
    // cursor onto the closest sketch line segment (clamped to the
    // segment's interior). This lets the user start or end a draft
    // anywhere on an existing line, not just at its endpoints or
    // midpoint. Lower priority than every point candidate above
    // because endpoint / midpoint snaps should win when the cursor
    // is genuinely close to those features.
    const linesParams = sketchLinesRef.current;
    if (linesParams) {
      let bestLineSnap: {
        local: [number, number];
        distance: number;
        lineId: string;
        t: number;
      } | null = null;
      for (const line of linesParams.lines) {
        const dx = line.end_x - line.start_x;
        const dy = line.end_y - line.start_y;
        const lengthSquared = dx * dx + dy * dy;
        if (lengthSquared <= 1e-12) {
          continue;
        }
        // Parametric projection clamped to [0, 1] so we never snap
        // past the segment's endpoints.
        const t = Math.max(
          0,
          Math.min(
            1,
            ((rawPoint.local[0] - line.start_x) * dx +
              (rawPoint.local[1] - line.start_y) * dy) /
              lengthSquared,
          ),
        );
        const px = line.start_x + t * dx;
        const py = line.start_y + t * dy;
        const distance = Math.hypot(
          rawPoint.local[0] - px,
          rawPoint.local[1] - py,
        );
        if (distance > SKETCH_SNAP_DISTANCE) {
          continue;
        }
        if (!bestLineSnap || distance < bestLineSnap.distance) {
          bestLineSnap = {
            local: [px, py],
            distance,
            lineId: line.line_id,
            t,
          };
        }
      }
      if (bestLineSnap) {
        return {
          local: bestLineSnap.local,
          world: toWorldPoint(
            activeSketchPlaneId ?? "ref-plane-xy",
            bestLineSnap.local,
            activeSketchPlaneFrame,
          ),
          snapLabel: translate("snap.onLine"),
          snapMidpointHostLineId: null,
          snapPerpendicularHostLineId: null,
          snapEndpointHostLineId: null,
          snapLineBodyHostLineId: bestLineSnap.lineId,
          snapLineBodyT: bestLineSnap.t,
        } satisfies SketchPreviewPoint;
      }
    }

    // Circle-nearest snap: project cursor onto the nearest circle's
    // circumference. The nearest point on the circle is the radial
    // projection of the cursor onto the circle edge. Distance check:
    // |dist(cursor, center) - radius| <= SKETCH_SNAP_DISTANCE.
    if (effectiveFilter?.snap_nearest !== false && params) {
      let bestCircleSnap: {
        local: [number, number];
        distance: number;
        circleId: string;
      } | null = null;
      for (const circle of params.circles) {
        const dx = rawPoint.local[0] - circle.center_x;
        const dy = rawPoint.local[1] - circle.center_y;
        const distToCenter = Math.hypot(dx, dy);
        if (distToCenter < 1e-9) continue;
        const nx = circle.center_x + (dx / distToCenter) * circle.radius;
        const ny = circle.center_y + (dy / distToCenter) * circle.radius;
        const d = Math.hypot(rawPoint.local[0] - nx, rawPoint.local[1] - ny);
        if (d > SKETCH_SNAP_DISTANCE) continue;
        if (!bestCircleSnap || d < bestCircleSnap.distance) {
          bestCircleSnap = { local: [nx, ny], distance: d, circleId: circle.circle_id };
        }
      }
      if (bestCircleSnap) {
        return {
          local: bestCircleSnap.local,
          world: toWorldPoint(
            activeSketchPlaneId ?? "ref-plane-xy",
            bestCircleSnap.local,
            activeSketchPlaneFrame,
          ),
          snapLabel: translate("snap.onCircle"),
          snapMidpointHostLineId: null,
          snapPerpendicularHostLineId: null,
          snapEndpointHostLineId: null,
        } satisfies SketchPreviewPoint;
      }
    }

    return {
      ...rawPoint,
      snapLabel: gridDidSnap ? translate("snap.grid") : null,
      snapMidpointHostLineId: null,
      snapPerpendicularHostLineId: null,
      snapEndpointHostLineId: null,
    } satisfies SketchPreviewPoint;
  }

  function syncPrimitiveVisuals() {
    for (const [primitiveId, visual] of primitiveVisualsRef.current.entries()) {
      const state = primitiveStatesRef.current.get(primitiveId);
      if (!state) {
        continue;
      }

      applyPrimitiveVisualState(visual, state);
    }
  }

  function syncReferencePlaneVisuals() {
    for (const [
      referenceId,
      visual,
    ] of referencePlaneVisualsRef.current.entries()) {
      const state = referencePlaneStatesRef.current.get(referenceId);
      if (!state) {
        continue;
      }

      applyReferencePlaneVisualState(visual, state);
    }
  }

  function syncSolidFaceVisuals() {
    for (const [faceId, visual] of solidFaceVisualsRef.current.entries()) {
      const state = solidFaceStatesRef.current.get(faceId);
      if (!state) {
        continue;
      }

      applySolidFaceVisualState(visual, state);
    }
  }

  function syncSketchProfileVisuals() {
    for (const [
      profileId,
      visual,
    ] of sketchProfileVisualsRef.current.entries()) {
      const state = sketchProfileStatesRef.current.get(profileId);
      if (!state) {
        continue;
      }

      applySketchProfileVisualState(visual, state);
    }
  }

  function setHoveredFace(faceId: string | null) {
    let changed = false;

    for (const [id, state] of solidFaceStatesRef.current.entries()) {
      const nextHovered = id === faceId;
      if (state.isHovered !== nextHovered) {
        solidFaceStatesRef.current.set(id, {
          ...state,
          isHovered: nextHovered,
        });
        changed = true;
      }
    }

    if (changed) {
      syncSolidFaceVisuals();
    }
  }

  function setHoveredSketchProfile(profileId: string | null) {
    let changed = false;

    for (const [id, state] of sketchProfileStatesRef.current.entries()) {
      const nextHovered = id === profileId;
      if (state.isHovered !== nextHovered) {
        sketchProfileStatesRef.current.set(id, {
          ...state,
          isHovered: nextHovered,
        });
        changed = true;
      }
    }

    if (changed) {
      syncSketchProfileVisuals();
    }
  }

  const hoveredSketchEntityIdRef = useRef<string | null>(null);
  function paintSketchEntityMaterials() {
    // Use the stable ref (updated from useMemo on viewport change)
    // so hover never sees an empty DOF map when viewport is stale.
    const dofMap = dofMapRef.current;
    for (const object of sketchEntityObjectsRef.current) {
      const id = object.userData.sketchEntityId as string | undefined;
      const isSelected = object.userData.isSelected === true;
      const isProjected = object.userData.sketchEntityIsProjected === true;
      const isHovered =
        id !== undefined && id === hoveredSketchEntityIdRef.current;
      const material = object.material as
        | THREE.LineBasicMaterial
        | THREE.LineDashedMaterial;
      if (isSelected) {
        material.color.set(themeColor("--color-primary-edge-active", "#c3f5ff"));
      } else if (isHovered) {
        material.color.set(themeColor("--color-tertiary-plane-edge-hover", "#fff2b2"));
      } else if (isProjected) {
        material.color.set(themeColor("--cad-sketch-projected", "#ff4fd8"));
      } else if (id && dofMap.has(id)) {
        const status = dofMap.get(id)!;
        if (status === "full") {
          material.color.set(0x8899aa);
        } else {
          material.color.set(0xff4444);
        }
      } else {
        material.color.set(themeColor("--color-tertiary-plane-fill", "#fff7c0"));
      }
      material.opacity = isSelected || isHovered ? 1 : 0.98;
      material.linewidth = isSelected ? 3 : isHovered ? 2.5 : 1;
    }
  }

  function setHoveredSketchEntity(entityId: string | null) {
    if (hoveredSketchEntityIdRef.current === entityId) {
      return;
    }
    hoveredSketchEntityIdRef.current = entityId;
    paintSketchEntityMaterials();
    paintDofStatusColors();
  }

  const hoveredSketchPointIdRef = useRef<string | null>(null);

  /** No-op — DOF colors are now applied in paintSketchEntityMaterials directly. */
  function paintDofStatusColors() {}

  function paintSketchPointMaterials() {
    for (const mesh of sketchPointObjectsRef.current) {
      const id = mesh.userData.sketchPointId as string | undefined;
      const kind = mesh.userData.sketchPointKind as string | undefined;
      const isSelected = mesh.userData.isSelected === true;
      const isHovered =
        id !== undefined && id === hoveredSketchPointIdRef.current;
      const material = mesh.material as THREE.MeshBasicMaterial;
      material.color.set(
        isSelected
          ? themeColor("--color-primary-edge-active", "#c3f5ff")
          : isHovered
            ? themeColor("--color-tertiary-plane-edge-hover", "#fff2b2")
            : kind === "center" || kind === "projected" || kind === "quadrant"
              ? themeColor("--color-axis-z", "#6db4ff")
              : themeColor("--color-tertiary-plane-edge", "#ffe784"),
      );
      material.opacity = isSelected || isHovered ? 1 : 0.95;
      const scale = isSelected ? 1.35 : isHovered ? 1.25 : 1;
      mesh.scale.setScalar(scale);
    }
  }

  function setHoveredSketchPoint(pointId: string | null) {
    if (hoveredSketchPointIdRef.current === pointId) {
      return;
    }
    hoveredSketchPointIdRef.current = pointId;
    paintSketchPointMaterials();
  }

  function setHoveredPrimitive(primitiveId: string | null) {
    let changed = false;

    for (const [id, state] of primitiveStatesRef.current.entries()) {
      const nextHovered = id === primitiveId;
      if (state.isHovered !== nextHovered) {
        primitiveStatesRef.current.set(id, {
          ...state,
          isHovered: nextHovered,
        });
        changed = true;
      }
    }

    if (changed) {
      syncPrimitiveVisuals();
    }
  }

  // Hover state for body edges / vertices. Unlike face / primitive
  // hover (which keeps a per-object state map and re-runs the visual
  // helper en masse), edges and vertices are simple THREE objects
  // built once per geometry rebuild — so we recolor materials in
  // place. `userData.isSelected` was stashed at build time so we can
  // resolve the (selected, hovered) tuple per object without reading
  // the document state here.
  const hoveredEdgeIdRef = useRef<string | null>(null);
  // Recolor every edge from its userData (selection / ghost flags) plus
  // the current hover id and the live ghost-reveal flag. Single source
  // of truth so hover changes and Tab-toggle changes don't have to
  // duplicate the visual logic.
  function paintEdgeMaterials(hoveredId: string | null) {
    const revealGhost = revealGhostEdgesRef.current;
    for (const line of edgeLineObjectsRef.current) {
      const id = line.userData.edgeId as string | undefined;
      const isSelected = line.userData.isSelected === true;
      const isGhost = line.userData.isGhost === true;
      const isHovered = id !== undefined && id === hoveredId;
      const material = line.material as THREE.LineBasicMaterial;
      applyEdgeVisualColor(material, {
        isSelected,
        isHovered,
        isGhost,
        revealGhost,
      });
    }
  }
  function setHoveredEdge(edgeId: string | null) {
    if (hoveredEdgeIdRef.current === edgeId) {
      return;
    }
    hoveredEdgeIdRef.current = edgeId;
    paintEdgeMaterials(edgeId);
  }

  const hoveredVertexIdRef = useRef<string | null>(null);
  function paintVertexMaterials(hoveredId: string | null) {
    for (const mesh of vertexObjectsRef.current) {
      const id = mesh.userData.vertexId as string | undefined;
      const isSelected = mesh.userData.isSelected === true;
      const isHovered = id !== undefined && id === hoveredId;
      const material = mesh.material as THREE.MeshBasicMaterial;
      applyVertexVisualColor(material, { isSelected, isHovered });
    }
  }

  function setHoveredVertex(vertexId: string | null) {
    if (hoveredVertexIdRef.current === vertexId) {
      return;
    }
    hoveredVertexIdRef.current = vertexId;
    paintVertexMaterials(vertexId);
  }

  function setHoveredReference(referenceId: string | null) {
    let changed = false;

    for (const [id, state] of referencePlaneStatesRef.current.entries()) {
      const nextHovered = id === referenceId;
      if (state.isHovered !== nextHovered) {
        referencePlaneStatesRef.current.set(id, {
          ...state,
          isHovered: nextHovered,
        });
        changed = true;
      }
    }

    if (changed) {
      syncReferencePlaneVisuals();
    }
  }

  useEffect(() => {
    selectPrimitiveRef.current = onSelectPrimitive;
    selectReferenceRef.current = onSelectReference;
    selectFaceRef.current = onSelectFace;
    moveBodyRef.current = onMoveBody;
    copyBodyRef.current = onCopyBody;
    unlinkBodyCopyRef.current = onUnlinkBodyCopy;
    selectEdgeRef.current = onSelectEdge;
    selectVertexRef.current = onSelectVertex;
    startSketchRef.current = onStartSketch;
    startSketchOnFaceRef.current = onStartSketchOnFace;
    addSketchLineRef.current = onAddSketchLine;
    addSketchRectangleRef.current = onAddSketchRectangle;
    addSketchCircleRef.current = onAddSketchCircle;
    addSketchArcRef.current = onAddSketchArc;
    arcToolModeRef.current = arcToolMode;
    rectangleToolModeRef.current = rectangleToolMode;
    circleToolModeRef.current = circleToolMode;
    polygonToolModeRef.current = polygonToolMode;
    polygonSidesRef.current = polygonSides;
    addSketchPolygonRef.current = onAddSketchPolygon;
    addSketchFilletRef.current = onAddSketchFillet;
    selectSketchEntityRef.current = onSelectSketchEntity;
    pickInactiveSketchLineRef.current = onPickInactiveSketchLine;
    inactiveSketchEntityPickEnabledRef.current =
      inactiveSketchEntityPickEnabled;
    pickSketchPointRef.current = onPickSketchPoint;
    updateSketchPointRef.current = onUpdateSketchPoint;
    selectSketchDimensionRef.current = onSelectSketchDimension;
    updateSketchDimensionRef.current = onUpdateSketchDimension;
    updateSketchDimensionLabelPositionRef.current =
      onUpdateSketchDimensionLabelPosition;
    addSketchPointDistanceDimensionRef.current =
      onAddSketchPointDistanceDimension;
    updateSketchDimensionDisplayRef.current =
      onUpdateSketchDimensionDisplay;
    selectSketchProfileRef.current = onSelectSketchProfile;
    trimSketchEntityRef.current = onTrimSketchEntity;
    deleteSketchSelectionRef.current = onDeleteSketchSelection;
    setSketchToolRef.current = onSetSketchTool;
    armedSketchConstraintRef.current = armedSketchConstraint;
    mirrorFocusedSlotRef.current = mirrorFocusedSlot;
    mirrorEntityPickRef.current = onMirrorEntityPick;
    cancelSketchConstraintRef.current = onCancelSketchConstraint;
    clearSketchConstraintRef.current = onClearSketchConstraint;
    moveGizmoRef.current = moveGizmo;
    moveGizmoChangeRef.current = onMoveGizmoChange;
    moveBodyRef.current = onMoveBody;
    copyBodyRef.current = onCopyBody;
    unlinkBodyCopyRef.current = onUnlinkBodyCopy;
  }, [
    onSelectPrimitive,
    onSelectReference,
    onSelectFace,
    onSelectEdge,
    onSelectVertex,
    onStartSketch,
    onStartSketchOnFace,
    onAddSketchLine,
    onAddSketchRectangle,
    onAddSketchCircle,
    onAddSketchArc,
    arcToolMode,
    rectangleToolMode,
    circleToolMode,
    polygonToolMode,
    polygonSides,
    onAddSketchPolygon,
    onAddSketchFillet,
    onSelectSketchEntity,
    onPickInactiveSketchLine,
    inactiveSketchEntityPickEnabled,
    onPickSketchPoint,
    onSelectSketchDimension,
    onUpdateSketchDimension,
    onUpdateSketchDimensionLabelPosition,
    onSelectSketchProfile,
    onDeleteSketchSelection,
    onSetSketchTool,
    onUpdateSketchPoint,
    armedSketchConstraint,
    mirrorFocusedSlot,
    onMirrorEntityPick,
    onCancelSketchConstraint,
    onClearSketchConstraint,
    moveGizmo,
    onMoveGizmoChange,
    onMoveBody,
    onCopyBody,
    onUnlinkBodyCopy,
  ]);

  function flushMoveGizmoChange(parameters: MoveFeatureParameters) {
    pendingMoveGizmoParametersRef.current = parameters;
    if (pendingMoveGizmoFrameRef.current !== null) {
      return;
    }
    pendingMoveGizmoFrameRef.current = window.requestAnimationFrame(() => {
      pendingMoveGizmoFrameRef.current = null;
      const next = pendingMoveGizmoParametersRef.current;
      pendingMoveGizmoParametersRef.current = null;
      if (next) {
        void moveGizmoChangeRef.current?.(next);
      }
    });
  }

  function moveGizmoScreenAngle(
    event: PointerEvent,
    center: THREE.Vector3,
    camera: THREE.Camera,
    renderer: THREE.WebGLRenderer,
  ) {
    const projectedCenter = projectWorldPointToViewport(
      [center.x, center.y, center.z],
      camera,
      renderer,
    );
    if (!projectedCenter) {
      return 0;
    }
    const rect = renderer.domElement.getBoundingClientRect();
    return Math.atan2(
      event.clientY - rect.top - projectedCenter.y,
      event.clientX - rect.left - projectedCenter.x,
    );
  }

  function moveGizmoTranslationDelta(
    event: PointerEvent,
    drag: MoveGizmoDragState,
    axis: MoveGizmoAxis,
    camera: THREE.Camera,
    renderer: THREE.WebGLRenderer,
  ) {
    const center = projectWorldPointToViewport(
      [drag.center.x, drag.center.y, drag.center.z],
      camera,
      renderer,
    );
    const endpoint = drag.center.clone().addScaledVector(
      drag.axes[axis],
      drag.handleLength,
    );
    const projectedEndpoint = projectWorldPointToViewport(
      [endpoint.x, endpoint.y, endpoint.z],
      camera,
      renderer,
    );
    if (!center || !projectedEndpoint) {
      return 0;
    }
    const axisScreen = {
      x: projectedEndpoint.x - center.x,
      y: projectedEndpoint.y - center.y,
    };
    const axisScreenLength = Math.hypot(axisScreen.x, axisScreen.y);
    if (axisScreenLength <= 1.0e-6) {
      return 0;
    }
    const dragScreen = {
      x: event.clientX - drag.startClientX,
      y: event.clientY - drag.startClientY,
    };
    const projectedPixels =
      (dragScreen.x * axisScreen.x + dragScreen.y * axisScreen.y) /
      axisScreenLength;
    return (projectedPixels / axisScreenLength) * drag.handleLength;
  }

  function moveGizmoFreeDelta(
    event: PointerEvent,
    drag: MoveGizmoDragState,
    camera: THREE.Camera,
    renderer: THREE.WebGLRenderer,
  ) {
    const dx = event.clientX - drag.startClientX;
    const dy = event.clientY - drag.startClientY;
    const cameraDirection = new THREE.Vector3();
    camera.getWorldDirection(cameraDirection);
    const cameraRight = new THREE.Vector3()
      .crossVectors(cameraDirection, camera.up)
      .normalize();
    const cameraUp = camera.up.clone().normalize();
    const worldUnitsPerPixel =
      camera instanceof THREE.OrthographicCamera
        ? (camera.top - camera.bottom) / camera.zoom / renderer.domElement.clientHeight
        : drag.handleLength / Math.max(renderer.domElement.clientHeight, 1);
    return new THREE.Vector3()
      .addScaledVector(cameraRight, dx * worldUnitsPerPixel)
      .addScaledVector(cameraUp, -dy * worldUnitsPerPixel);
  }

  useEffect(() => {
    activeSketchToolRef.current = activeSketchTool;
    sketchSnapCandidatesRef.current = sketchSnapCandidates;
  }, [activeSketchTool, sketchSnapCandidates]);

  useEffect(() => {
    setCrosshairPointer(null);
  }, [activeSketchPlaneId, activeSketchTool, config.viewport.crosshair]);

  useEffect(() => {
    setSketchMidpointAnchorRef.current = onSetSketchMidpointAnchor;
  }, [onSetSketchMidpointAnchor]);

  useEffect(() => {
    setSketchPointLineAnchorRef.current = onSetSketchPointLineAnchor;
  }, [onSetSketchPointLineAnchor]);

  useEffect(() => {
    addSketchAngleDimensionRef.current = onAddSketchAngleDimension;
    addSketchDistanceDimensionRef.current = onAddSketchDistanceDimension;
    addSketchLineLengthDimensionRef.current = onAddSketchLineLengthDimension;
    addSketchCircleRadiusDimensionRef.current =
      onAddSketchCircleRadiusDimension;
    addSketchPolygonRadiusDimensionRef.current =
      onAddSketchPolygonRadiusDimension;
  }, [
    onAddSketchAngleDimension,
    onAddSketchDistanceDimension,
    onAddSketchLineLengthDimension,
    onAddSketchCircleRadiusDimension,
    onAddSketchPolygonRadiusDimension,
  ]);

  useEffect(() => {
    setSketchPerpendicularConstraintRef.current =
      onSetSketchPerpendicularConstraint;
  }, [onSetSketchPerpendicularConstraint]);

  useEffect(() => {
    setSketchLineConstraintRef.current = onSetSketchLineConstraint;
  }, [onSetSketchLineConstraint]);

  useEffect(() => {
    setSketchTangentConstraintRef.current = onSetSketchTangentConstraint;
  }, [onSetSketchTangentConstraint]);

  // Post-add midpoint-anchor dispatch. When `add_sketch_line` settles
  // and the sketch's lines vector has grown, look at the just-added
  // (last) line and issue `set_sketch_midpoint_anchor` for whichever
  // endpoint(s) snapped to a midpoint host. The pending state is
  // captured at click-time (so the host id stays consistent even if
  // intervening edits re-render) and cleared as soon as we apply it.
  useEffect(() => {
    const params = sketchFeature?.sketch_parameters ?? null;
    sketchLinesRef.current = params;
    const newCount = params?.lines.length ?? 0;
    const previousCount = sketchLineCountRef.current;
    sketchLineCountRef.current = newCount;

    const pending = pendingMidpointAnchorRef.current;
    const pendingPerp = pendingPerpendicularConstraintRef.current;
    const pendingLine = pendingPointLineAnchorRef.current;
    const pendingAxis = pendingAxisConstraintRef.current;
    const pendingTangent = pendingTangentConstraintRef.current;
    if (!params) {
      return;
    }
    if (
      !pending &&
      !pendingPerp &&
      !pendingLine &&
      !pendingAxis &&
      !pendingTangent
    ) {
      return;
    }

    // All pending kinds use the same matching rule: the line count
    // must have grown by exactly one past the baseline. If it
    // didn't, drop every pending (they're stale).
    const baseline =
      pending?.fromLineCount ??
      pendingPerp?.fromLineCount ??
      pendingLine?.fromLineCount ??
      pendingAxis?.fromLineCount ??
      pendingTangent?.fromLineCount ??
      -1;
    if (newCount !== baseline + 1) {
      if (newCount !== previousCount) {
        pendingMidpointAnchorRef.current = null;
        pendingPerpendicularConstraintRef.current = null;
        pendingPointLineAnchorRef.current = null;
        pendingAxisConstraintRef.current = null;
        pendingTangentConstraintRef.current = null;
      }
      return;
    }

    pendingMidpointAnchorRef.current = null;
    pendingPerpendicularConstraintRef.current = null;
    pendingPointLineAnchorRef.current = null;
    pendingAxisConstraintRef.current = null;
    pendingTangentConstraintRef.current = null;
    const newLine = params.lines[params.lines.length - 1];
    if (!newLine) {
      return;
    }
    if (pending?.startHostLineId) {
      void setSketchMidpointAnchorRef.current(
        newLine.start_point_id,
        pending.startHostLineId,
      );
    }
    if (pending?.endHostLineId) {
      void setSketchMidpointAnchorRef.current(
        newLine.end_point_id,
        pending.endHostLineId,
      );
    }
    if (pendingPerp) {
      void setSketchPerpendicularConstraintRef.current(
        newLine.line_id,
        pendingPerp.hostLineId,
      );
    }
    // Point-on-line anchors. Per side, only fire when that side
    // wasn't already claimed by a midpoint anchor — the midpoint
    // anchor is a more specific relation and should win.
    if (pendingLine?.startHost && !pending?.startHostLineId) {
      void setSketchPointLineAnchorRef.current(
        newLine.start_point_id,
        pendingLine.startHost.lineId,
        pendingLine.startHost.t,
      );
    }
    if (pendingLine?.endHost && !pending?.endHostLineId) {
      void setSketchPointLineAnchorRef.current(
        newLine.end_point_id,
        pendingLine.endHost.lineId,
        pendingLine.endHost.t,
      );
    }
    // Axis lock takes precedence at most once per draft. Skip it if
    // a perpendicular constraint already fired for this line — the
    // two would conflict in the solver (perp's own conflict check
    // throws). Otherwise dispatch H/V as a sticky line constraint.
    if (pendingAxis && !pendingPerp) {
      void setSketchLineConstraintRef.current(
        newLine.line_id,
        pendingAxis.kind,
      );
    }
    // Tangent relation. Skipped when the new line already has a
    // perpendicular host or an axis lock — both fully determine the
    // line's direction and would over-constrain the tangent. The
    // user's explicit perp/axis intent wins; tangent is only the
    // implicit "snapped to a circle" outcome.
    if (pendingTangent && !pendingPerp && !pendingAxis) {
      void setSketchTangentConstraintRef.current(
        newLine.line_id,
        pendingTangent.circleId,
      );
    }
  }, [sketchFeature]);

  useEffect(() => {
    sketchToolConstructionRef.current = sketchToolConstruction;
  }, [sketchToolConstruction]);

  // Auto-clear the construction toggle when the user leaves drawable
  // sketch tools so the option doesn't silently apply next time.
  useEffect(() => {
    if (!isDrawableSketchTool(activeSketchTool)) {
      setSketchToolConstruction(false);
    }
  }, [activeSketchTool]);

  useEffect(() => {
    selectedSketchDimensionRef.current = selectedSketchDimension;
  }, [selectedSketchDimension]);

  useEffect(() => {
    dimensionLabelPositionsRef.current = dimensionLabelPositions;
  }, [dimensionLabelPositions]);

  useEffect(() => {
    if (selectedSketchDimensionValue === null) {
      setDimensionDraftValue("");
      dimensionEditOriginalValueRef.current = null;
      return;
    }
    if (!selectedSketchDimension) {
      return;
    }
    const originalValue = dimensionEditOriginalValueRef.current;
    if (originalValue?.dimensionId !== selectedSketchDimension.dimensionId) {
      dimensionEditOriginalValueRef.current = {
        dimensionId: selectedSketchDimension.dimensionId,
        value: selectedSketchDimensionValue,
        expression: selectedSketchDimensionExpression,
      };
    }
    if (window.document.activeElement === dimensionInputRef.current) {
      return;
    }

    // Round to 2 decimals and strip trailing zeros so 12.000000001 →
    // "12" and 3.4567 → "3.46", instead of leaking the full IEEE-754
    // representation into the input. `parseFloat` of a fixed-precision
    // string is the canonical way to drop trailing zeros without
    // building a regex.
    setDimensionDraftValue(
      selectedSketchDimensionExpression.trim().length > 0
        ? selectedSketchDimensionExpression
        : formattedDimensionDisplayValue(
            selectedSketchDimension,
            selectedSketchDimensionValue,
          ),
    );
  }, [
    selectedSketchDimensionValue,
    selectedSketchDimensionExpression,
    document?.selected_sketch_dimension_id,
    selectedSketchDimension,
    selectedSketchDimension?.dimensionId,
    selectedSketchDimension?.kind,
  ]);

  useEffect(() => {
    if (
      !pendingDimensionPlacementRef.current ||
      activeSketchTool !== "dimension"
    ) {
      return;
    }
    const pendingRelation = pendingRelationPlacementMatchRef.current;
    let placementDimension = selectedSketchDimension;
    if (pendingRelation) {
      if (!startPendingRelationPlacementIfReady()) {
        return;
      }
      stopPendingRelationPlacementRetry();
      return;
    }
    if (!placementDimension) {
      return;
    }
    pendingDimensionIdRef.current = null;
    pendingDimensionPlacementRef.current = false;
    pendingDimSourceEntityIdRef.current = null;
    beginDimensionPlacement(placementDimension);
  }, [activeSketchTool, displayedSketchDimensions, selectedSketchDimension]);

  useEffect(() => {
    if (!selectedSketchDimension) {
      setIsDimensionEditorOpen(false);
      dimensionInputSelectionLockedRef.current = false;
      return;
    }

    if (isProjectedCircleDimension(selectedSketchDimension.dimensionId)) {
      dimensionInputRef.current?.blur();
      setIsDimensionEditorOpen(false);
      dimensionInputSelectionLockedRef.current = false;
      return;
    }

    if (suppressNextDimensionEditorOpenRef.current) {
      suppressNextDimensionEditorOpenRef.current = false;
      dimensionInputRef.current?.blur();
      setIsDimensionEditorOpen(false);
      return;
    }

    dimensionInputSelectionLockedRef.current = true;
    setIsDimensionEditorOpen(true);
  }, [selectedSketchDimension?.dimensionId]);

  useEffect(() => {
    if (!isDimensionEditorOpen || !selectedSketchDimension) {
      return;
    }

    const input = dimensionInputRef.current;
    if (!input) {
      return;
    }

    input.focus();
    input.select();
  }, [isDimensionEditorOpen, selectedSketchDimension?.dimensionId]);

  useEffect(() => {
    const host = hostRef.current;
    const canvas = canvasRef.current;

    if (!host || !canvas) {
      return undefined;
    }

    const renderer = new THREE.WebGLRenderer({
      canvas,
      antialias: true,
      alpha: true,
      preserveDrawingBuffer: true,
    });
    const scene = new THREE.Scene();
    const camera = new THREE.OrthographicCamera(
      -ORTHO_FRUSTUM_HEIGHT / 2,
      ORTHO_FRUSTUM_HEIGHT / 2,
      ORTHO_FRUSTUM_HEIGHT / 2,
      -ORTHO_FRUSTUM_HEIGHT / 2,
      -10000,
      10000,
    );
    const controls = new OrbitControls(camera, renderer.domElement);
    const contentGroup = new THREE.Group();
    const referenceGroup = new THREE.Group();
    const sketchGroup = new THREE.Group();
    const raycaster = new THREE.Raycaster();
    const pointer = new THREE.Vector2();
    let pointerDown: { x: number; y: number } | null = null;
    let frameId = 0;

    rendererRef.current = renderer;
    sceneRef.current = scene;
    cameraRef.current = camera;
    controlsRef.current = controls;
    contentGroupRef.current = contentGroup;
    referenceGroupRef.current = referenceGroup;
    sketchGroupRef.current = sketchGroup;

    renderer.setPixelRatio(window.devicePixelRatio);
    scene.add(contentGroup);
    scene.add(referenceGroup);
    scene.add(sketchGroup);
    // Neutral studio lighting so MeshStandardMaterial bodies render as
    // true contextual modeling gray. The previous cyan-tinted ambient + key
    // + rim lights were leaking cyan into the body fill, which made
    // the new gray material look like the old translucent cyan even
    // after the material itself was switched to opaque.
    scene.add(new THREE.AmbientLight(0xffffff, 0.85));

    const keyLight = new THREE.DirectionalLight(0xffffff, 0.95);
    keyLight.position.set(1.2, 1.8, 1.4);
    scene.add(keyLight);

    const fillLight = new THREE.DirectionalLight(0xffffff, 0.45);
    fillLight.position.set(-1.5, 0.8, -1.1);
    scene.add(fillLight);

    onSnapshotCaptureReady?.(() => {
      renderer.render(scene, camera);
      const source = renderer.domElement;
      if (source.width === 0 || source.height === 0) {
        return null;
      }
      const thumbnail = window.document.createElement("canvas");
      thumbnail.width = 240;
      thumbnail.height = 150;
      const context = thumbnail.getContext("2d");
      if (!context) {
        return null;
      }
      const thumbnailBackground =
        window
          .getComputedStyle(host)
          .getPropertyValue("--cad-project-thumbnail-bg")
          .trim();
      if (thumbnailBackground) {
        context.fillStyle = thumbnailBackground;
        context.fillRect(0, 0, thumbnail.width, thumbnail.height);
      }
      context.drawImage(source, 0, 0, thumbnail.width, thumbnail.height);
      return thumbnail.toDataURL("image/png");
    });

    controls.enableDamping = false;
    controls.enableZoom = false;
    controls.screenSpacePanning = true;
    controls.minDistance = 24;
    controls.maxDistance = 6000;
    controls.mouseButtons.LEFT = null;
    controls.mouseButtons.MIDDLE = THREE.MOUSE.PAN;
    controls.mouseButtons.RIGHT = null;
    controls.addEventListener("start", () => {
      canvas.classList.add("cad-viewport-canvas-dragging");
    });
    controls.addEventListener("end", () => {
      canvas.classList.remove("cad-viewport-canvas-dragging");
    });

    // -- view cube setup -------------------------------------------------
    const cubeGroup = buildViewCubeGroup();
    const cubeScene = createViewCubeScene(cubeGroup);
    const cubeCamera = createViewCubeCamera();
    const cubeRaycaster = new THREE.Raycaster();
    viewCubeGroupRef.current = cubeGroup;
    viewCubeSceneRef.current = cubeScene;
    viewCubeCameraRef.current = cubeCamera;
    viewCubeRaycasterRef.current = cubeRaycaster;

    function ensureDynamicGrid(
      ref: MutableRefObject<DynamicGridRef | null>,
      key: string,
      buildLine: () => THREE.LineSegments,
    ) {
      const current = ref.current;
      if (current?.key === key) {
        return;
      }

      if (current) {
        scene.remove(current.group);
        disposeDynamicGrid(current);
      }

      const group = new THREE.Group();
      group.add(buildLine());
      scene.add(group);
      ref.current = { key, group };
    }

    function clearDynamicGrid(ref: MutableRefObject<DynamicGridRef | null>) {
      const current = ref.current;
      if (!current) {
        return;
      }
      scene.remove(current.group);
      disposeDynamicGrid(current);
      ref.current = null;
    }

    function updateDynamicGrids() {
      if (!sceneDataRef.current) {
        clearDynamicGrid(worldGridRef);
        clearDynamicGrid(sketchGridRef);
        return;
      }

      const spacing = selectOrthographicGridSpacing(camera);
      currentGridSpacingRef.current = spacing;
      const viewOffset = new THREE.Vector3()
        .copy(camera.position)
        .sub(controls.target)
        .normalize();
      const cardinalFrame = getCardinalGridFrame(viewOffset);
      const worldFrame: GridPlaneFrame = cardinalFrame ?? {
        origin: new THREE.Vector3(0, 0, 0),
        xAxis: new THREE.Vector3(1, 0, 0),
        yAxis: new THREE.Vector3(0, 0, 1),
        normal: new THREE.Vector3(0, 1, 0),
      };

      const sketchPlaneId = activeSketchPlaneIdRef.current;
      if (!sketchPlaneId) {
        if (!showViewportGridRef.current) {
          clearDynamicGrid(worldGridRef);
          clearDynamicGrid(sketchGridRef);
          return;
        }

        const worldCenter = projectPointToGridFrame(controls.target, worldFrame);
        const worldBounds = getGridViewBounds(
          camera,
          worldFrame,
          spacing,
          worldCenter,
          GRID_WORLD_PADDING_MULTIPLIER,
        );
        ensureDynamicGrid(
          worldGridRef,
          `world:${
            cardinalFrame ? "cardinal" : "floor"
          }:${spacing}:${worldBounds.minU}:${worldBounds.maxU}:${worldBounds.minV}:${worldBounds.maxV}`,
          () => {
            const worldGrid = buildDynamicGrid(
              worldFrame,
              spacing,
              worldBounds,
              new THREE.Color(themeColor("--color-cad-grid", "#3f4648")),
              new THREE.Color(themeColor("--color-cad-grid-axis", "#5e696c")),
              new THREE.Color(themeColor("--color-cad-grid-axis", "#7a7a7c")),
              0.34,
            );
            worldGrid.renderOrder = -10;
            return worldGrid;
          },
        );
        clearDynamicGrid(sketchGridRef);
        return;
      }

      clearDynamicGrid(worldGridRef);
      if (!showSketchGridRef.current) {
        clearDynamicGrid(sketchGridRef);
        return;
      }

      const sketchFrame = getSketchGridFrame(
        sketchPlaneId,
        activeSketchPlaneFrameRef.current,
      );
      const sketchCenter = projectPointToGridFrame(controls.target, sketchFrame);
      const sketchBounds = getGridViewBounds(
        camera,
        sketchFrame,
        spacing,
        sketchCenter,
        GRID_SKETCH_PADDING_MULTIPLIER,
      );
      ensureDynamicGrid(
        sketchGridRef,
        `sketch:${sketchPlaneId}:${spacing}:${sketchBounds.minU}:${sketchBounds.maxU}:${sketchBounds.minV}:${sketchBounds.maxV}`,
        () => {
          const sketchGrid = buildDynamicGrid(
            sketchFrame,
            spacing,
            sketchBounds,
            new THREE.Color(themeColor("--cad-sketch-grid", "#2a383b")),
            new THREE.Color(themeColor("--cad-sketch-grid-axis", "#46585d")),
            new THREE.Color(
              themeColor("--cad-sketch-grid-center-axis", "#7a8a8f"),
            ),
            0.48,
          );
          sketchGrid.renderOrder = -9;
          return sketchGrid;
        },
      );
    }

    function updateScreenSpaceSketchSprites() {
      const viewportHeight = Math.max(renderer.domElement.clientHeight, 1);
      const viewportScale = Math.min(
        Math.max(viewportHeight / SKETCH_SCREEN_SPRITE_BASE_HEIGHT, 0.82),
        1.18,
      );
      const worldUnitsPerPixel =
        getOrthographicViewHeight(camera) / viewportHeight;
      const cameraRight = new THREE.Vector3()
        .setFromMatrixColumn(camera.matrixWorld, 0)
        .normalize();
      const cameraUp = new THREE.Vector3()
        .setFromMatrixColumn(camera.matrixWorld, 1)
        .normalize();
      const dimensionRects: Array<{
        center: { x: number; y: number };
        width: number;
        height: number;
      }> = [];

      function updateSpriteScale(object: THREE.Object3D, scale: number) {
        const sprite = object as THREE.Sprite;
        const screenSize = sprite.userData.screenSize as
          | { width: number; height: number }
          | undefined;
        if (!screenSize || !sprite.isSprite) {
          return null;
        }

        const basePosition = sprite.userData.basePosition as
          | [number, number, number]
          | null
          | undefined;
        if (basePosition) {
          sprite.position.set(...basePosition);
        }
        sprite.scale.set(
          screenSize.width * scale * viewportScale * worldUnitsPerPixel,
          screenSize.height * scale * viewportScale * worldUnitsPerPixel,
          1,
        );
        const dimensionStart = sprite.userData.dimensionStart as
          | [number, number, number]
          | undefined;
        const dimensionEnd = sprite.userData.dimensionEnd as
          | [number, number, number]
          | undefined;
        const labelOffsetPixels = 5;
        if (dimensionStart && dimensionEnd && sprite.material) {
          const material = sprite.material as THREE.SpriteMaterial;
          if (
            sprite.userData.dimensionKind === "angle" ||
            sprite.userData.dimensionKind === "line_angle"
          ) {
            material.rotation = 0;
            return null;
          }
          const start = projectWorldPointToViewport(
            dimensionStart,
            camera,
            renderer,
          );
          const end = projectWorldPointToViewport(dimensionEnd, camera, renderer);
          if (start && end) {
            const dx = end.x - start.x;
            const dy = end.y - start.y;
            let angle = Math.atan2(dy, dx);
            if (angle > Math.PI / 2) {
              angle -= Math.PI;
            } else if (angle < -Math.PI / 2) {
              angle += Math.PI;
            }
            material.rotation = -angle;
            const lineLength = Math.hypot(dx, dy);
            if (lineLength > 1e-6) {
              const midpoint = {
                x: (start.x + end.x) / 2,
                y: (start.y + end.y) / 2,
              };
              const labelCenter = projectWorldPointToViewport(
                [sprite.position.x, sprite.position.y, sprite.position.z],
                camera,
                renderer,
              );
              let normalX = -dy / lineLength;
              let normalY = dx / lineLength;
              if (
                labelCenter &&
                (labelCenter.x - midpoint.x) * normalX +
                  (labelCenter.y - midpoint.y) * normalY <
                  0
              ) {
                normalX = -normalX;
                normalY = -normalY;
              }
              sprite.position
                .addScaledVector(
                  cameraRight,
                  normalX * labelOffsetPixels * worldUnitsPerPixel,
                )
                .addScaledVector(
                  cameraUp,
                  -normalY * labelOffsetPixels * worldUnitsPerPixel,
                );
            }
          } else {
            material.rotation = 0;
          }
        }

        const center = projectWorldPointToViewport(
          [sprite.position.x, sprite.position.y, sprite.position.z],
          camera,
          renderer,
        );
        if (!center) {
          return null;
        }
        return {
          center,
          width: screenSize.width * scale * viewportScale,
          height: screenSize.height * scale * viewportScale,
        };
      }

      for (const object of sketchDimensionObjectsRef.current) {
        const rect = updateSpriteScale(object, SKETCH_LABEL_SCREEN_SCALE);
        if (rect) {
          dimensionRects.push(rect);
        }
      }

      for (const object of sketchConstraintObjectsRef.current) {
        const rect = updateSpriteScale(
          object,
          SKETCH_CONSTRAINT_SCREEN_SIZE / 42,
        );
        if (!rect) {
          continue;
        }
        const sprite = object as THREE.Sprite;
        for (const dimensionRect of dimensionRects) {
          const dx = rect.center.x - dimensionRect.center.x;
          const dy = rect.center.y - dimensionRect.center.y;
          const overlapX =
            (rect.width + dimensionRect.width) / 2 +
            SKETCH_LABEL_COLLISION_PADDING -
            Math.abs(dx);
          const overlapY =
            (rect.height + dimensionRect.height) / 2 +
            SKETCH_LABEL_COLLISION_PADDING -
            Math.abs(dy);
          if (overlapX <= 0 || overlapY <= 0) {
            continue;
          }

          const moveX =
            overlapX < overlapY
              ? (dx >= 0 ? 1 : -1) * overlapX
              : 0;
          const moveY =
            overlapX < overlapY
              ? 0
              : (dy >= 0 ? 1 : -1) * overlapY;
          sprite.position
            .addScaledVector(cameraRight, moveX * worldUnitsPerPixel)
            .addScaledVector(cameraUp, -moveY * worldUnitsPerPixel);
          rect.center.x += moveX;
          rect.center.y += moveY;
        }
      }
    }

    function resizeRenderer() {
      const width = Math.max(host?.clientWidth ?? 0, 1);
      const height = Math.max(host?.clientHeight ?? 0, 1);
      setViewportSize({ width, height });
      renderer.setSize(width, height, false);
      const aspect = width / height;
      camera.left = (-ORTHO_FRUSTUM_HEIGHT * aspect) / 2;
      camera.right = (ORTHO_FRUSTUM_HEIGHT * aspect) / 2;
      camera.top = ORTHO_FRUSTUM_HEIGHT / 2;
      camera.bottom = -ORTHO_FRUSTUM_HEIGHT / 2;
      camera.updateProjectionMatrix();
    }

    function worldPointAtPointer(event: WheelEvent) {
      const rect = renderer.domElement.getBoundingClientRect();
      const ndcX = ((event.clientX - rect.left) / rect.width) * 2 - 1;
      const ndcY = -((event.clientY - rect.top) / rect.height) * 2 + 1;
      camera.updateMatrixWorld();
      return new THREE.Vector3(ndcX, ndcY, 0).unproject(camera);
    }

    function handleWheel(event: WheelEvent) {
      event.preventDefault();

      const before = worldPointAtPointer(event);
      const deltaMultiplier =
        event.deltaMode === WheelEvent.DOM_DELTA_LINE
          ? 16
          : event.deltaMode === WheelEvent.DOM_DELTA_PAGE
            ? renderer.domElement.clientHeight
            : 1;
      const zoomFactor = Math.exp(
        -event.deltaY * deltaMultiplier * WHEEL_ZOOM_SPEED,
      );
      const nextZoom = THREE.MathUtils.clamp(
        camera.zoom * zoomFactor,
        ORTHO_MIN_ZOOM,
        ORTHO_MAX_ZOOM,
      );

      if (Math.abs(nextZoom - camera.zoom) < 1e-6) {
        return;
      }

      camera.zoom = nextZoom;
      camera.updateProjectionMatrix();

      const after = worldPointAtPointer(event);
      const pointerShift = before
        .sub(after)
        .multiplyScalar(WHEEL_ZOOM_POINTER_PAN);
      camera.position.add(pointerShift);
      controls.target.add(pointerShift);
      controls.update();
    }

    function renderDraftDimensions() {
      const session = draftDimensionSessionRef.current;
      const sketchGroup = sketchGroupRef.current;
      const camera = cameraRef.current;
      if (!session || !sketchGroup || !camera) {
        clearDraftDimGroup();
        return;
      }

      // Clear previous frame's geometry
      clearDraftDimGroup();

      const [sx, sy] = session.start;
      const [ex, ey] = session.current;
      const dx = ex - sx;
      const dy = ey - sy;
      const len = Math.hypot(dx, dy);
      if (len < 0.001) return;

      // Current implementation handles line tool only.
      // Rectangle, circle, polygon tools are left working as before
      // via the existing HTML input overlay.
      if (session.tool !== "line") {
        draftDimScreenPositionsRef.current = {};
        return;
      }

      const planeId = activeSketchPlaneId ?? "ref-plane-xy";
      const planeFrame = activeSketchPlaneFrame;

      // Convert 2D sketch coords to 3D world
      const sw = toWorldPoint(planeId, [sx, sy], planeFrame);
      const ew = toWorldPoint(planeId, [ex, ey], planeFrame);

      const group = new THREE.Group();
      group.renderOrder = 6;

      // --- Helper: add line segments to a collection ---
      const allSegs: THREE.Vector3[] = [];
      const addSeg = (a: [number, number, number], b: [number, number, number]) => {
        allSegs.push(new THREE.Vector3(a[0], a[1], a[2]));
        allSegs.push(new THREE.Vector3(b[0], b[1], b[2]));
      };

      // Filled arrow triangle collection
      const arrowPositions: number[] = [];
      const arrowIndices: number[] = [];
      const addFilledArrow = (
        tip: THREE.Vector3,
        inward: THREE.Vector3,
        perp: THREE.Vector3,
      ) => {
        const base = tip.clone().add(inward.clone().multiplyScalar(ARROW_LEN));
        const side = perp.clone().multiplyScalar(ARROW_W);
        const idx = arrowPositions.length / 3;
        arrowPositions.push(tip.x, tip.y, tip.z);
        arrowPositions.push(base.x + side.x, base.y + side.y, base.z + side.z);
        arrowPositions.push(base.x - side.x, base.y - side.y, base.z - side.z);
        arrowIndices.push(idx, idx + 1, idx + 2);
      };

      // --- Compute sketch plane normal ---
      let pNormal: THREE.Vector3;
      if (planeFrame) {
        const nx = Array.isArray(planeFrame.normal) ? planeFrame.normal[0] : planeFrame.normal.x;
        const ny = Array.isArray(planeFrame.normal) ? planeFrame.normal[1] : planeFrame.normal.y;
        const nz = Array.isArray(planeFrame.normal) ? planeFrame.normal[2] : planeFrame.normal.z;
        pNormal = new THREE.Vector3(nx, ny, nz);
      } else {
        pNormal = new THREE.Vector3(0, 1, 0);
      }

      // Line direction and perpendicular (in sketch plane)
      const sVec = new THREE.Vector3(sw[0], sw[1], sw[2]);
      const eVec = new THREE.Vector3(ew[0], ew[1], ew[2]);
      const lineDir = eVec.clone().sub(sVec).normalize();
      const perpDir = new THREE.Vector3().crossVectors(lineDir, pNormal).normalize();

      // Flip perpDir toward camera
      const toCam = new THREE.Vector3().copy(camera.position).sub(sVec).normalize();
      if (perpDir.dot(toCam) < 0) perpDir.negate();

      // Zoom-aware dimension offset (~30 px on screen)
      const viewH = getOrthographicViewHeight(camera);
      const vpH = rendererRef.current?.domElement.height ?? 600;
      const zoomDimOffset = Math.max(4, 30 * viewH / vpH);
      const ARROW_LEN = 1.5;
      const ARROW_W = 0.27;

      // --- Length dimension ---
      const dimLabelPos: [number, number, number] = [0, 0, 0];
      let angleRad = 0; // hoisted for label position computation below
      let arcMidWorldLabel: [number, number, number] = [0, 0, 0];
      if (session.tool === "line") {
        const dimS = new THREE.Vector3(sw[0], sw[1], sw[2]).add(perpDir.clone().multiplyScalar(-2 * zoomDimOffset));
        const dimE = new THREE.Vector3(ew[0], ew[1], ew[2]).add(perpDir.clone().multiplyScalar(-2 * zoomDimOffset));
        const dimDir = dimE.clone().sub(dimS);

        // Extension lines
        addSeg(sw, [dimS.x, dimS.y, dimS.z]);
        addSeg(ew, [dimE.x, dimE.y, dimE.z]);

        if (dimDir.lengthSq() > 0.001) {
          const dimDirN = dimDir.clone().normalize();

          // Dimension line
          addSeg([dimS.x, dimS.y, dimS.z], [dimE.x, dimE.y, dimE.z]);

          // Arrowheads (filled triangles)
          addFilledArrow(dimS, dimDirN, perpDir);
          addFilledArrow(dimE, dimDirN.clone().negate(), perpDir);

          // Label position = midpoint of dimension line
          const mid = dimS.clone().add(dimE).multiplyScalar(0.5);
          dimLabelPos[0] = mid.x;
          dimLabelPos[1] = mid.y;
          dimLabelPos[2] = mid.z;
        }

        // --- Dotted extension + reference line + angle arc ---
        angleRad = Math.atan2(dy, dx);
        const lineLen = Math.hypot(dx, dy);

        // Determine reference angle: previous chained line, or horizontal (0)
        const refAngle = previousLineAngleRef.current ?? 0;

        // Compute relative display angle (shorter arc direction)
        let displayAngle = angleRad - refAngle;
        while (displayAngle > Math.PI) displayAngle -= 2 * Math.PI;
        while (displayAngle < -Math.PI) displayAngle += 2 * Math.PI;
        // Negate so arc and length dimension occupy opposite sides.
        // Preserves angle magnitude, just flips the sweep direction.
        // Arc sweeps the natural shorter direction (CCW for positive angles).

        // --- Angle arc centered at line start, sweeping refAngle → angleRad ---
        // Zoom-aware cap so the arc stays ~480 px on screen.
        const zoomCap = Math.max(20, 480 * viewH / vpH);
        const arcRadius = Math.max(8, Math.min(lineLen, zoomCap));

        // --- Dotted reference line from start point along reference angle ---
        // Extend to the arc radius so it meets the dimension arc exactly.
        {
          const rsw = toWorldPoint(planeId, [sx, sy], planeFrame);
          const rex = sx + arcRadius * Math.cos(refAngle);
          const rey = sy + arcRadius * Math.sin(refAngle);
          const rew = toWorldPoint(planeId, [rex, rey], planeFrame);
          const refGeom = new THREE.BufferGeometry().setFromPoints([
            new THREE.Vector3(rsw[0], rsw[1], rsw[2]),
            new THREE.Vector3(rew[0], rew[1], rew[2]),
          ]);
          const refMat = new THREE.LineDashedMaterial({
            color: new THREE.Color(0x8feaf7), transparent: true,
            opacity: 0.40, dashSize: 2, gapSize: 2, depthTest: false,
          });
          const refLine = new THREE.Line(refGeom, refMat);
          refLine.computeLineDistances();
          refLine.renderOrder = 7;
          group.add(refLine);
        }
        const arcSweep = displayAngle;
        const arcSegments = 24;
        let prevArc: THREE.Vector3 | null = null;
        let arcStartWorldPt: [number, number, number] = [0, 0, 0];
        let arcEndWorldPt: [number, number, number] = [0, 0, 0];
        for (let i = 0; i <= arcSegments; i++) {
          const a = refAngle + arcSweep * (i / arcSegments);
          const lx = sx + arcRadius * Math.cos(a);
          const ly = sy + arcRadius * Math.sin(a);
          const wp = toWorldPoint(planeId, [lx, ly], planeFrame);
          const p = new THREE.Vector3(wp[0], wp[1], wp[2]);
          if (i === 0) arcStartWorldPt = wp;
          if (i === arcSegments) arcEndWorldPt = wp;
          if (prevArc) addSeg([prevArc.x, prevArc.y, prevArc.z], [p.x, p.y, p.z]);
          prevArc = p;
        }

        // Filled arrowheads at arc ends
        const addArcArrow = (tipWorld: [number, number, number], tipAngle: number) => {
          const tip = new THREE.Vector3(tipWorld[0], tipWorld[1], tipWorld[2]);
          const tlx = sx + arcRadius * Math.cos(tipAngle) - arcRadius * Math.sin(tipAngle);
          const tly = sy + arcRadius * Math.sin(tipAngle) + arcRadius * Math.cos(tipAngle);
          const tw = toWorldPoint(planeId, [tlx, tly], planeFrame);
          const tanDir = new THREE.Vector3(tw[0], tw[1], tw[2]).sub(tip).normalize();
          const radDir = tip.clone().sub(sVec).normalize();
          addFilledArrow(tip, tanDir, radDir);
        };
        addArcArrow(arcStartWorldPt, refAngle);
        addArcArrow(arcEndWorldPt, angleRad);

        // Arc midpoint for label positioning
        const labelAngle = refAngle + arcSweep / 2;
        // For near-horizontal lines (|displayAngle| < 20°), a label that
        // rides in front of the rubber band can trap the mouse pointer —
        // the cursor lands on the HTML input instead of the canvas,
        // tracking stops, and both line and label freeze.  Always offset
        // perpendicular (above/below) to the line in this case, like
        // other CAD tools do.
        const angleDeg = Math.abs(displayAngle) * 180 / Math.PI;
        let labelOnPerp = angleDeg < 20 && lineLen > 0.001;
        if (labelOnPerp) {
          const lineUx = dx / lineLen;
          const lineUy = dy / lineLen;
          // Place the angle window on the opposite side from the
          // length dimension.  For a downward line the length dim
          // already sits below, so angle goes above.
          const perpFlip = dy >= 0 ? -1 : 1;
          const perpUx = -lineUy * perpFlip;
          const perpUy = lineUx * perpFlip;
          arcMidWorldLabel = toWorldPoint(planeId, [
            sx + arcRadius * lineUx + perpUx * 2.0 * zoomDimOffset,
            sy + arcRadius * lineUy + perpUy * 2.0 * zoomDimOffset,
          ], planeFrame);
        } else {
          arcMidWorldLabel = toWorldPoint(planeId, [
            sx + arcRadius * Math.cos(labelAngle),
            sy + arcRadius * Math.sin(labelAngle),
          ], planeFrame);
        }
      }

      // (diagnostic line removed)

      // Build line geometry
      const segCount = allSegs.length;
      if (segCount > 0) {
        const positions = new Float32Array(segCount * 3);
        for (let i = 0; i < segCount; i++) {
          positions[i * 3] = allSegs[i].x;
          positions[i * 3 + 1] = allSegs[i].y;
          positions[i * 3 + 2] = allSegs[i].z;
        }
        const geom = new THREE.BufferGeometry();
        geom.setAttribute("position", new THREE.BufferAttribute(positions, 3));
        const mat = new THREE.LineBasicMaterial({
          color: new THREE.Color(0x8feaf7),
          transparent: true,
          opacity: 0.78,
          depthTest: false,
        });
        const line = new THREE.LineSegments(geom, mat);
        line.renderOrder = 6;
        group.add(line);
      }

      // Build filled arrow mesh
      if (arrowIndices.length > 0) {
        const arrowGeom = new THREE.BufferGeometry();
        arrowGeom.setAttribute(
          "position",
          new THREE.Float32BufferAttribute(arrowPositions, 3),
        );
        arrowGeom.setIndex(arrowIndices);
        const arrowMat = new THREE.MeshBasicMaterial({
          color: new THREE.Color(0x8feaf7),
          transparent: true,
          opacity: 0.78,
          depthTest: false,
          side: THREE.DoubleSide,
        });
        const arrowMesh = new THREE.Mesh(arrowGeom, arrowMat);
        arrowMesh.renderOrder = 6;
        group.add(arrowMesh);
      }

      // Store computed label positions for HTML input overlay
      draftDimScreenPositionsRef.current = {};
      if (session.tool === "line") {
        const theRenderer = rendererRef.current;
        if (theRenderer) {
          // Length label at dimension line midpoint
          const lenLabelProj = projectWorldPointToViewport(
            dimLabelPos, camera, theRenderer,
          );
          if (lenLabelProj) {
            draftDimScreenPositionsRef.current.length = lenLabelProj;
          }
          // Angle label at arc midpoint
          const angleLabelProj = projectWorldPointToViewport(
            arcMidWorldLabel, camera, theRenderer,
          );
          if (angleLabelProj) {
            draftDimScreenPositionsRef.current.angle = angleLabelProj;
          }
        }
      }

      sketchGroup.add(group);
      draftDimGroupRef.current = group;
    }

    function render() {
      controls.update();
      updateDynamicGrids();
      updateScreenSpaceSketchSprites();
      try {
        renderDraftDimensions();
      } catch (err) {
        console.warn("renderDraftDimensions error:", err);
        clearDraftDimGroup();
      }

      renderer.render(scene, camera);
      renderViewCube();

      const editor = dimensionEditorRef.current;
      const dimension = selectedSketchDimensionRef.current;
      const isOpen = isDimensionEditorOpenRef.current;
      if (!editor || !dimension || !isOpen) {
        if (editor) {
          editor.style.opacity = "0";
        }
        return;
      }

      const projectedPosition = projectWorldPointToViewport(
        dimension.labelPosition,
        camera,
        renderer,
      );

      if (!projectedPosition) {
        editor.style.opacity = "0";
        return;
      }

      editor.style.opacity = "1";
      editor.style.transform = `translate(${projectedPosition.x}px, ${projectedPosition.y}px) translate(-50%, -50%)`;
    }


    function segmentCrossesRect(
      x1: number, y1: number, x2: number, y2: number,
      rect: { x1: number; y1: number; x2: number; y2: number },
    ): boolean {
      // Check if segment intersects any of the 4 rect edges
      return (
        segmentsIntersect(x1, y1, x2, y2, rect.x1, rect.y1, rect.x2, rect.y1) ||
        segmentsIntersect(x1, y1, x2, y2, rect.x2, rect.y1, rect.x2, rect.y2) ||
        segmentsIntersect(x1, y1, x2, y2, rect.x2, rect.y2, rect.x1, rect.y2) ||
        segmentsIntersect(x1, y1, x2, y2, rect.x1, rect.y2, rect.x1, rect.y1)
      );
    }

    function segmentsIntersect(
      ax1: number, ay1: number, ax2: number, ay2: number,
      bx1: number, by1: number, bx2: number, by2: number,
    ): boolean {
      const d1 = cross2D(ax1, ay1, ax2, ay2, bx1, by1);
      const d2 = cross2D(ax1, ay1, ax2, ay2, bx2, by2);
      const d3 = cross2D(bx1, by1, bx2, by2, ax1, ay1);
      const d4 = cross2D(bx1, by1, bx2, by2, ax2, ay2);
      if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
          ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0))) return true;
      if (d1 === 0 && pointOnSegment(ax1, ay1, ax2, ay2, bx1, by1)) return true;
      if (d2 === 0 && pointOnSegment(ax1, ay1, ax2, ay2, bx2, by2)) return true;
      if (d3 === 0 && pointOnSegment(bx1, by1, bx2, by2, ax1, ay1)) return true;
      if (d4 === 0 && pointOnSegment(bx1, by1, bx2, by2, ax2, ay2)) return true;
      return false;
    }

    function cross2D(x1: number, y1: number, x2: number, y2: number, x3: number, y3: number): number {
      return (x2 - x1) * (y3 - y1) - (y2 - y1) * (x3 - x1);
    }

    function pointOnSegment(
      x1: number, y1: number, x2: number, y2: number, px: number, py: number,
    ): boolean {
      return (
        Math.min(x1, x2) <= px && px <= Math.max(x1, x2) &&
        Math.min(y1, y2) <= py && py <= Math.max(y1, y2)
      );
    }

    function boxesIntersect(
      ax1: number, ay1: number, ax2: number, ay2: number,
      bx1: number, by1: number, bx2: number, by2: number,
    ): boolean {
      return !(ax2 < bx1 || bx2 < ax1 || ay2 < by1 || by2 < ay1);
    }


    async function performRectangleSelect(drag: SelectionDrag, additive: boolean) {
      if (!activeSketchPlaneIdRef.current) return;

      const camera = cameraRef.current;
      const renderer = rendererRef.current;
      if (!camera || !renderer) return;

      // Convert drag rectangle from viewport coords (clientX/Y) to
      // canvas-relative coords so they match projectWorldPointToViewport
      // output (which is relative to the renderer's DOM element).
      const canvasRect = renderer.domElement.getBoundingClientRect();
      const rect = {
        x1: Math.min(drag.startX, drag.currentX) - canvasRect.left,
        y1: Math.min(drag.startY, drag.currentY) - canvasRect.top,
        x2: Math.max(drag.startX, drag.currentX) - canvasRect.left,
        y2: Math.max(drag.startY, drag.currentY) - canvasRect.top,
      };
      const isWindow = drag.currentX >= drag.startX; // L→R = window

      // Collect visible sketch entities from sceneData
      const { sketchLines, sketchCircles, sketchArcs, sketchPolygons, sketchPoints } =
        sceneDataRef.current ?? {};

      const insideRect = (px: number, py: number) =>
        px >= rect.x1 && px <= rect.x2 && py >= rect.y1 && py <= rect.y2;

      const selected: string[] = [];
      console.warn("[SEL]", isWindow ? "win" : "cross",
        "L:", sketchLines?.length ?? 0,
        "C:", sketchCircles?.length ?? 0,
        "P:", sketchPoints?.length ?? 0);

      // Test lines (skip preview and construction)
      if (sketchLines) {
        for (const line of sketchLines) {
          if (line.isPreview || line.isConstruction) continue;
          const s = projectWorldPointToViewport(
            [line.start[0], line.start[1], line.start[2]],
            camera,
            renderer,
          );
          const e = projectWorldPointToViewport(
            [line.end[0], line.end[1], line.end[2]],
            camera,
            renderer,
          );
          if (!s || !e) continue;
          if (isWindow) {
            if (insideRect(s.x, s.y) && insideRect(e.x, e.y))
              selected.push(line.lineId);
          } else {
            if (insideRect(s.x, s.y) || insideRect(e.x, e.y) ||
                segmentCrossesRect(s.x, s.y, e.x, e.y, rect))
              selected.push(line.lineId);
          }
        }
      }

      // Test circles
      if (sketchCircles) {
        for (const c of sketchCircles) {
          const center = projectWorldPointToViewport(
            [c.center[0], c.center[1], c.center[2]],
            camera,
            renderer,
          );
          if (!center) continue;
          // Simple bounding-box test
          const radius = c.radius; // in world units, approximate px radius
          const right = projectWorldPointToViewport(
            [c.center[0] + radius, c.center[1], c.center[2]],
            camera,
            renderer,
          );
          const approxRadius = right ? Math.abs(right.x - center.x) : 0;
          const bx1 = center.x - approxRadius, by1 = center.y - approxRadius;
          const bx2 = center.x + approxRadius, by2 = center.y + approxRadius;
          if (isWindow) {
            if (bx1 >= rect.x1 && bx2 <= rect.x2 && by1 >= rect.y1 && by2 <= rect.y2)
              selected.push(c.circleId);
          } else {
            if (boxesIntersect(bx1, by1, bx2, by2, rect.x1, rect.y1, rect.x2, rect.y2))
              selected.push(c.circleId);
          }
        }
      }

      // Test arcs
      if (sketchArcs) {
        for (const arc of sketchArcs) {
          if (arc.isConstruction) continue;
          const start = projectWorldPointToViewport(
            [arc.start[0], arc.start[1], arc.start[2]], camera, renderer);
          const end = projectWorldPointToViewport(
            [arc.end[0], arc.end[1], arc.end[2]], camera, renderer);
          if (!start || !end) continue;
          // Bounding box: start, end + 4 quadrant extremes of parent circle
          const cx = arc.center[0], cy = arc.center[1];
          const r = Math.hypot(arc.start[0] - cx, arc.start[1] - cy);
          const extremes: Array<[number, number]> = [
            [cx + r, cy], [cx - r, cy], [cx, cy + r], [cx, cy - r]];
          let bx1 = Math.min(start.x, end.x), by1 = Math.min(start.y, end.y);
          let bx2 = Math.max(start.x, end.x), by2 = Math.max(start.y, end.y);
          for (const [wx, wy] of extremes) {
            const p = projectWorldPointToViewport(
              [wx, wy, arc.start[2]], camera, renderer);
            if (p) {
              bx1 = Math.min(bx1, p.x); by1 = Math.min(by1, p.y);
              bx2 = Math.max(bx2, p.x); by2 = Math.max(by2, p.y);
            }
          }
          if (isWindow) {
            if (insideRect(start.x, start.y) && insideRect(end.x, end.y))
              selected.push(arc.arcId);
          } else {
            if (insideRect(start.x, start.y) || insideRect(end.x, end.y) ||
                boxesIntersect(bx1, by1, bx2, by2,
                  rect.x1, rect.y1, rect.x2, rect.y2))
              selected.push(arc.arcId);
          }
        }
      }

      // Apply selection via IPC
      // Batch selection via dedicated callback
      if (selected.length > 0) {
        onBatchSelectEntities(selected, additive);
      }
    }

    function isFacingCardinalCubeFace() {
      const viewOffset = new THREE.Vector3()
        .copy(camera.position)
        .sub(controls.target)
        .normalize();
      return isCardinalCubeDirection(viewOffset);
    }

    function rotateCameraAroundCurrentView(direction: -1 | 1) {
      const viewOffset = new THREE.Vector3()
        .copy(camera.position)
        .sub(controls.target);
      if (viewOffset.lengthSq() < 1e-6) {
        return;
      }
      const axis = viewOffset.clone().normalize();

      const angle = direction * (Math.PI / 2);
      viewCubeAnimStartPosRef.current.copy(camera.position);
      viewCubeAnimTargetPosRef.current.copy(camera.position);
      viewCubeAnimStartUpRef.current.copy(camera.up).normalize();
      viewCubeAnimTargetUpRef.current
        .copy(camera.up)
        .applyAxisAngle(axis, angle)
        .normalize();
      viewCubeAnimStartRef.current = performance.now();
      viewCubeAnimatingRef.current = true;
      controls.enabled = false;
    }

    function renderViewCube() {
      const cubeGroup = viewCubeGroupRef.current;
      const cubeScene = viewCubeSceneRef.current;
      const cubeCam = viewCubeCameraRef.current;
      if (!cubeGroup || !cubeScene || !cubeCam) return;

      // Sync cube camera to main view direction
      syncCubeCamera(camera, controls.target, cubeCam);
      updateSketchRotationArrows(
        cubeGroup,
        cubeCam,
        isFacingCardinalCubeFace(),
      );

      // Camera animation tick
      if (viewCubeAnimatingRef.current) {
        const done = animateCameraTowardTarget(
          camera,
          controls,
          viewCubeAnimStartPosRef.current,
          viewCubeAnimTargetPosRef.current,
          viewCubeAnimStartRef.current,
          performance.now(),
          viewCubeAnimStartUpRef.current,
          viewCubeAnimTargetUpRef.current,
        );
        if (done) {
          viewCubeAnimatingRef.current = false;
          controls.enabled = true;
        }
      }

      const dpr = renderer.getPixelRatio();
      const w = renderer.domElement.width;
      const h = renderer.domElement.height;
      const rect = getCubeViewportRect(w, h, dpr);

      const oldAutoClear = renderer.autoClear;
      const oldViewport = new THREE.Vector4();
      const oldScissor = new THREE.Vector4();
      renderer.getViewport(oldViewport);
      renderer.getScissor(oldScissor);

      renderer.autoClear = false;
      renderer.setViewport(rect.x, rect.y, rect.width, rect.height);
      renderer.setScissor(rect.x, rect.y, rect.width, rect.height);
      renderer.setScissorTest(true);
      renderer.clear(true, true, false);
      renderer.render(cubeScene, cubeCam);

      renderer.setViewport(oldViewport);
      renderer.setScissor(oldScissor);
      renderer.setScissorTest(false);
      renderer.autoClear = oldAutoClear;
    }

    function pickVisibleSketchLineScreenSpace(
      event: PointerEvent,
      maxDistancePx = 16,
    ) {
      const lines = sceneDataRef.current?.sketchLines ?? [];
      const rect = renderer.domElement.getBoundingClientRect();
      const pointerPx = {
        x: event.clientX - rect.left,
        y: event.clientY - rect.top,
      };
      const toScreen = (point: readonly [number, number, number]) => {
        const projected = new THREE.Vector3(...point).project(camera);
        if (
          projected.z < -1 ||
          projected.z > 1 ||
          !Number.isFinite(projected.x) ||
          !Number.isFinite(projected.y)
        ) {
          return null;
        }
        return {
          x: ((projected.x + 1) * 0.5) * rect.width,
          y: ((-projected.y + 1) * 0.5) * rect.height,
        };
      };
      const pointSegmentDistance = (
        point: { x: number; y: number },
        start: { x: number; y: number },
        end: { x: number; y: number },
      ) => {
        const dx = end.x - start.x;
        const dy = end.y - start.y;
        const lengthSq = dx * dx + dy * dy;
        if (lengthSq <= 1.0e-9) {
          return Math.hypot(point.x - start.x, point.y - start.y);
        }
        const t = Math.max(
          0,
          Math.min(
            1,
            ((point.x - start.x) * dx + (point.y - start.y) * dy) / lengthSq,
          ),
        );
        return Math.hypot(
          point.x - (start.x + dx * t),
          point.y - (start.y + dy * t),
        );
      };

      let best: { lineId: string; distance: number } | null = null;
      for (const line of lines) {
        if (line.isPreview) {
          continue;
        }
        const start = toScreen(line.start);
        const end = toScreen(line.end);
        if (!start || !end) {
          continue;
        }
        const distance = pointSegmentDistance(pointerPx, start, end);
        if (distance <= maxDistancePx && (!best || distance < best.distance)) {
          best = { lineId: line.lineId, distance };
        }
      }
      return best?.lineId ?? null;
    }

    function intersectSceneTargets(event: PointerEvent) {
      const rect = renderer.domElement.getBoundingClientRect();
      pointer.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
      pointer.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;

      raycaster.setFromCamera(pointer, camera);
      raycaster.params.Line = { threshold: 1.75 };

      function pointInPolygon2d(
        point: [number, number],
        polygon: Array<[number, number]>,
      ) {
        if (polygon.length < 3) {
          return false;
        }
        let inside = false;
        for (
          let index = 0, previous = polygon.length - 1;
          index < polygon.length;
          previous = index, index += 1
        ) {
          const currentPoint = polygon[index];
          const previousPoint = polygon[previous];
          const crosses =
            currentPoint[1] > point[1] !== previousPoint[1] > point[1] &&
            point[0] <
              ((previousPoint[0] - currentPoint[0]) *
                (point[1] - currentPoint[1])) /
                (previousPoint[1] - currentPoint[1]) +
                currentPoint[0];
          if (crosses) {
            inside = !inside;
          }
        }
        return inside;
      }

      function profileArea(profile: SketchProfileScene) {
        if (profile.profileKind === "circle") {
          return Math.PI * profile.radius * profile.radius;
        }
        const polygonArea = (points: Array<[number, number]>) => {
          let area = 0;
          for (let index = 0; index < points.length; index += 1) {
            const current = points[index];
            const next = points[(index + 1) % points.length];
            area += current[0] * next[1] - next[0] * current[1];
          }
          return Math.abs(area * 0.5);
        };
        return (
          polygonArea(profile.profilePoints) -
          profile.innerLoops.reduce((sum, loop) => sum + polygonArea(loop), 0)
        );
      }

      function profileLocalPoint(profile: SketchProfileScene) {
        if (profile.planeFrame) {
          const origin = new THREE.Vector3(
            profile.planeFrame.origin[0],
            profile.planeFrame.origin[1],
            profile.planeFrame.origin[2],
          );
          const normal = new THREE.Vector3(
            profile.planeFrame.normal[0],
            profile.planeFrame.normal[1],
            profile.planeFrame.normal[2],
          );
          const xAxis = new THREE.Vector3(
            profile.planeFrame.xAxis[0],
            profile.planeFrame.xAxis[1],
            profile.planeFrame.xAxis[2],
          );
          const yAxis = new THREE.Vector3(
            profile.planeFrame.yAxis[0],
            profile.planeFrame.yAxis[1],
            profile.planeFrame.yAxis[2],
          );
          const renderOrigin = origin.clone().addScaledVector(
            normal,
            SKETCH_PLANE_OFFSET,
          );
          const plane = new THREE.Plane().setFromNormalAndCoplanarPoint(
            normal,
            renderOrigin,
          );
          const hitPoint = new THREE.Vector3();
          const hit = raycaster.ray.intersectPlane(plane, hitPoint);
          if (!hit) {
            return null;
          }
          const relative = hitPoint.sub(renderOrigin);
          return [relative.dot(xAxis), relative.dot(yAxis)] as [number, number];
        }

        const plane =
          profile.planeId === "ref-plane-xy"
            ? new THREE.Plane(new THREE.Vector3(0, 1, 0), -SKETCH_PLANE_OFFSET)
            : profile.planeId === "ref-plane-yz"
              ? new THREE.Plane(
                  new THREE.Vector3(1, 0, 0),
                  -SKETCH_PLANE_OFFSET,
                )
              : new THREE.Plane(
                  new THREE.Vector3(0, 0, 1),
                  -SKETCH_PLANE_OFFSET,
                );
        const hitPoint = new THREE.Vector3();
        const hit = raycaster.ray.intersectPlane(plane, hitPoint);
        if (!hit) {
          return null;
        }
        if (profile.planeId === "ref-plane-xy") {
          return [hitPoint.x, hitPoint.z] as [number, number];
        }
        if (profile.planeId === "ref-plane-yz") {
          return [hitPoint.y, hitPoint.z] as [number, number];
        }
        return [hitPoint.x, hitPoint.y] as [number, number];
      }

      function containsProfilePoint(
        profile: SketchProfileScene,
        point: [number, number],
      ) {
        if (profile.profileKind === "circle") {
          const dx = point[0] - profile.start[0];
          const dy = point[1] - profile.start[1];
          return dx * dx + dy * dy <= profile.radius * profile.radius;
        }
        if (!pointInPolygon2d(point, profile.profilePoints)) {
          return false;
        }
        return !profile.innerLoops.some((loop) => pointInPolygon2d(point, loop));
      }

      function pickSketchProfileId() {
        const profiles = sceneDataRef.current?.sketchProfiles ?? [];
        const profileById = new Map(
          profiles.map((profile) => [profile.profileId, profile]),
        );
        const profileObjectHits = raycaster
          .intersectObjects(sketchProfileObjectsRef.current, true)
          .map((hit) => {
            const profileId = hit.object.userData.sketchProfileId;
            if (typeof profileId !== "string") {
              return null;
            }
            const profile = profileById.get(profileId);
            if (!profile) {
              return null;
            }
            return {
              profileId,
              area: profileArea(profile),
            };
          })
          .filter(
            (hit): hit is { profileId: string; area: number } => hit !== null,
          );
        if (profileObjectHits.length > 0) {
          profileObjectHits.sort((left, right) => left.area - right.area);
          return profileObjectHits[0].profileId;
        }

        const hits = profiles
          .map((profile) => {
            const point = profileLocalPoint(profile);
            if (!point || !containsProfilePoint(profile, point)) {
              return null;
            }
            return {
              profileId: profile.profileId,
              area: profileArea(profile),
            };
          })
          .filter(
            (hit): hit is { profileId: string; area: number } => hit !== null,
          );
        if (hits.length === 0) {
          return null;
        }
        hits.sort((left, right) => left.area - right.area);
        return hits[0].profileId;
      }

      if (activeSketchPlaneId) {
        // Dimension tool: check points and entities before dimension
        // labels so dimension visuals don't occlude endpoint picks
        // during point-to-point or two-entity workflows.
        const checkDimensionsLast =
          activeSketchToolRef.current === "dimension";

        if (checkDimensionsLast) {
          const [sketchPointHitPre] = raycaster.intersectObjects(
            sketchPointObjectsRef.current,
            false,
          );
          const sketchPointIdPre =
            sketchPointHitPre?.object.userData.sketchPointId;
          if (typeof sketchPointIdPre === "string") {
            return {
              kind: "sketch_point" as const,
              id: sketchPointIdPre,
              pointKind: sketchPointHitPre.object.userData.sketchPointKind,
            };
          }

          const [sketchEntityHitPre] = raycaster.intersectObjects(
            sketchEntityObjectsRef.current,
            false,
          );
          const sketchEntityIdPre =
            sketchEntityHitPre?.object.userData.sketchEntityId;
          if (typeof sketchEntityIdPre === "string") {
            const hitPoint = sketchEntityHitPre!.point;
            return {
              kind: "sketch_entity" as const,
              id: sketchEntityIdPre,
              entityKind:
                sketchEntityHitPre.object.userData.sketchEntityKind,
              isProjected:
                sketchEntityHitPre.object.userData
                  .sketchEntityIsProjected === true,
              worldPoint: [hitPoint.x, hitPoint.y, hitPoint.z] as [
                number,
                number,
                number,
              ],
            };
          }
        }

        const [sketchDimensionHit] = raycaster.intersectObjects(
          sketchDimensionObjectsRef.current,
          true,
        );
        const sketchDimensionId =
          sketchDimensionHit?.object.userData.sketchDimensionId;
        if (typeof sketchDimensionId === "string") {
          const part =
            sketchDimensionHit?.object.userData.sketchDimensionPart === "label"
              ? "label"
              : "geometry";
          return {
            kind: "sketch_dimension" as const,
            id: sketchDimensionId,
            part,
          };
        }

        const [sketchConstraintHit] = raycaster.intersectObjects(
          sketchConstraintObjectsRef.current,
          true, // recursive — catches sprites inside any group
        );
        const sketchConstraintId =
          sketchConstraintHit?.object.userData.sketchConstraintId;
        if (typeof sketchConstraintId === "string") {
          return {
            kind: "sketch_constraint" as const,
            id: sketchConstraintId,
            constraintKind:
              sketchConstraintHit.object.userData
                .sketchConstraintKind as ConstraintType,
            entityId:
              sketchConstraintHit.object.userData.sketchConstraintEntityId,
            relatedEntityId:
              sketchConstraintHit.object.userData
                .sketchConstraintRelatedEntityId ?? null,
          };
        }

        if (!checkDimensionsLast) {
          const [sketchPointHit] = raycaster.intersectObjects(
            sketchPointObjectsRef.current,
            false,
          );
          const sketchPointId =
            sketchPointHit?.object.userData.sketchPointId;
          if (typeof sketchPointId === "string") {
            return {
              kind: "sketch_point" as const,
              id: sketchPointId,
              pointKind: sketchPointHit.object.userData.sketchPointKind,
            };
          }

          const [sketchEntityHit] = raycaster.intersectObjects(
            sketchEntityObjectsRef.current,
            false,
          );
          const sketchEntityId =
            sketchEntityHit?.object.userData.sketchEntityId;
          if (typeof sketchEntityId === "string") {
            const hitPoint = sketchEntityHit!.point;
            return {
              kind: "sketch_entity" as const,
              id: sketchEntityId,
              entityKind:
                sketchEntityHit.object.userData.sketchEntityKind ?? null,
              isProjected:
                sketchEntityHit.object.userData
                  .sketchEntityIsProjected === true,
              worldPoint: [hitPoint.x, hitPoint.y, hitPoint.z] as const,
            };
          }
        }

        const profileId = pickSketchProfileId();
        if (profileId) {
          return { kind: "sketch_profile" as const, id: profileId };
        }
      }

      // Outside sketch mode, sketch profiles are still pickable so the
      // user can re-select a closed profile (e.g. after exiting sketch
      // mode) and run Extrude on it. We check profiles BEFORE the
      // body-side hits because a profile usually sits on a sketch plane
      // that may coincide with a body face, and the user's intent in
      // clicking a profile-tinted region is overwhelmingly "extrude
      // this", not "select the underlying face". Profiles whose owning
      // sketch was hidden (e.g. auto-hide-after-extrude) won't have a
      // mesh in the ref array, so they naturally drop out of the pick.
      if (inactiveSketchEntityPickEnabledRef.current) {
        const sketchLineId = pickVisibleSketchLineScreenSpace(event, 16);
        if (sketchLineId) {
          return {
            kind: "sketch_entity" as const,
            id: sketchLineId,
            entityKind: "line",
            isProjected: false,
            worldPoint: [0, 0, 0] as const,
          };
        }
        const [sketchEntityHit] = raycaster.intersectObjects(
          sketchEntityObjectsRef.current,
          false,
        );
        const sketchEntityId = sketchEntityHit?.object.userData.sketchEntityId;
        const sketchEntityKind =
          sketchEntityHit?.object.userData.sketchEntityKind;
        const sketchEntityIsProjected =
          sketchEntityHit?.object.userData.sketchEntityIsProjected === true;
        if (
          typeof sketchEntityId === "string" &&
          sketchEntityKind === "line"
        ) {
          const hitPoint = sketchEntityHit!.point;
          return {
            kind: "sketch_entity" as const,
            id: sketchEntityId,
            entityKind: "line",
            isProjected: sketchEntityIsProjected,
            worldPoint: [hitPoint.x, hitPoint.y, hitPoint.z] as const,
          };
        }
      }

      {
        const profileId = pickSketchProfileId();
        if (profileId) {
          return { kind: "sketch_profile" as const, id: profileId };
        }
      }

      if (inactiveSketchEntityPickEnabledRef.current) {
        const [sketchEntityHit] = raycaster.intersectObjects(
          sketchEntityObjectsRef.current,
          false,
        );
        const sketchEntityId = sketchEntityHit?.object.userData.sketchEntityId;
        const sketchEntityKind =
          sketchEntityHit?.object.userData.sketchEntityKind;
        const sketchEntityIsProjected =
          sketchEntityHit?.object.userData.sketchEntityIsProjected === true;
        if (typeof sketchEntityId === "string") {
          const hitPoint = sketchEntityHit!.point;
          return {
            kind: "sketch_entity" as const,
            id: sketchEntityId,
            entityKind:
              typeof sketchEntityKind === "string" ? sketchEntityKind : null,
            isProjected: sketchEntityIsProjected,
            worldPoint: [hitPoint.x, hitPoint.y, hitPoint.z] as const,
          };
        }
      }

      const [referenceHit] = raycaster.intersectObjects(
        referencePlaneMeshesRef.current,
        false,
      );
      const referenceId = referenceHit?.object.userData.referenceId;
      if (typeof referenceId === "string") {
        return { kind: "reference" as const, id: referenceId };
      }

      // Vertices are checked first: they're the smallest pick targets
      // but always visible (renderOrder = 2, depthTest = false), and
      // they sit on top of every edge / face, so prioritizing them
      // matches the visual stacking the user sees.
      const [vertexHit] = raycaster.intersectObjects(
        vertexObjectsRef.current,
        false,
      );
      const vertexId = vertexHit?.object.userData.vertexId;
      if (typeof vertexId === "string") {
        return { kind: "vertex" as const, id: vertexId };
      }

      // Edges are checked before faces because they sit ON the faces
      // and would be hidden by the face fill if checked after.
      const previousLineThreshold = raycaster.params.Line?.threshold ?? 1;
      if (raycaster.params.Line) {
        // Generous pick tolerance so hover lights up edges without
        // the user having to land on a pixel-perfect line. Click
        // accuracy isn't hurt here because edges are checked before
        // faces in this same chain — if both hit, edge wins.
        raycaster.params.Line.threshold = 1.2;
      }
      const [edgeHit] = raycaster.intersectObjects(
        edgeLineObjectsRef.current,
        false,
      );
      if (raycaster.params.Line) {
        raycaster.params.Line.threshold = previousLineThreshold;
      }
      const edgeId = edgeHit?.object.userData.edgeId;
      if (typeof edgeId === "string") {
        return { kind: "edge" as const, id: edgeId };
      }

      const [faceHit] = raycaster.intersectObjects(
        faceMeshesRef.current,
        false,
      );
      const faceId = faceHit?.object.userData.faceId;
      if (typeof faceId === "string") {
        return { kind: "face" as const, id: faceId };
      }

      const [primitiveHit] = raycaster.intersectObjects(
        meshesRef.current,
        false,
      );
      const primitiveId = primitiveHit?.object.userData.primitiveId;
      return typeof primitiveId === "string"
        ? { kind: "primitive" as const, id: primitiveId }
        : null;
    }

    function handlePointerDown(event: PointerEvent) {
      // Clear selected constraint on any pointer-down (except on the
      // constraint badge itself, which sets it in the click path above).
      setSelectedConstraint(null);
      lastPointerEventRef.current = event;
      setContextMenu(null);

      if (dimensionLabelDragRef.current?.isPlacement) {
        // Capture the pointer so the subsequent pointerup reaches the
        // canvas handler even when the cursor is over the dimension
        // editor input — the editor must not steal the commit click.
        renderer.domElement.setPointerCapture(event.pointerId);
        pointerDown = { x: event.clientX, y: event.clientY };
        return;
      }

      if (event.button === 1) {
        controls.mouseButtons.MIDDLE =
          event.ctrlKey || event.metaKey ? THREE.MOUSE.ROTATE : THREE.MOUSE.PAN;
        return;
      }

      if (event.button !== 0) {
        pointerDown = null;
        return;
      }

      // Double-click detection during line drafting: two clicks <300ms
      // apart at the same location break the chain so the next click
      // starts a fresh independent line.
      const now = performance.now();
      const prevTime = lastPointerDownTimeRef.current;
      const prevPos = lastPointerDownPosRef.current;
      lastPointerDownTimeRef.current = now;
      lastPointerDownPosRef.current = { x: event.clientX, y: event.clientY };
      if (
        prevTime > 0 &&
        now - prevTime < 300 &&
        prevPos &&
        Math.abs(event.clientX - prevPos.x) < 6 &&
        Math.abs(event.clientY - prevPos.y) < 6 &&
        lineDraftStartRef.current !== null &&
        isDraftDimensionTool(activeSketchToolRef.current)
      ) {
        chainBreakRequestedRef.current = true;
      } else {
        chainBreakRequestedRef.current = false;
      }

      // Cube-area drag start
      if (
        isPointerInCubeArea(
          event,
          renderer.domElement.getBoundingClientRect(),
          renderer.getPixelRatio(),
        )
      ) {
        viewCubeDraggingRef.current = true;
        viewCubeDragStartRef.current = { x: event.clientX, y: event.clientY };
        controls.enabled = false;
        pointerDown = null;
        return;
      }

      const activeMoveGizmo = moveGizmoRef.current;
      if (
        activeMoveGizmo &&
        !activeMoveGizmo.disabled &&
        moveGizmoObjectsRef.current.length > 0
      ) {
        const rect = renderer.domElement.getBoundingClientRect();
        pointer.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
        pointer.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;
        raycaster.setFromCamera(pointer, camera);
        const [gizmoHit] = raycaster.intersectObjects(
          moveGizmoObjectsRef.current,
          false,
        );
        const handle = gizmoHit?.object.userData.moveGizmoHandle as
          | { kind: "translate"; axis: MoveGizmoAxis }
          | { kind: "rotate"; axis: MoveGizmoAxis }
          | { kind: "free" }
          | undefined;
        if (handle) {
          const center = new THREE.Vector3(
            activeMoveGizmo.center.x,
            activeMoveGizmo.center.y,
            activeMoveGizmo.center.z,
          );
          moveGizmoDragRef.current = {
            kind: handle.kind,
            axis: handle.kind === "free" ? null : handle.axis,
            startClientX: event.clientX,
            startClientY: event.clientY,
            startAngle:
              handle.kind === "rotate"
                ? moveGizmoScreenAngle(event, center, camera, renderer)
                : 0,
            center,
            axes: moveGizmoAxes(activeMoveGizmo),
            handleLength: Math.min(
              Math.max(
                Math.max(
                  activeMoveGizmo.size.x,
                  activeMoveGizmo.size.y,
                  activeMoveGizmo.size.z,
                  12,
                ) * 0.65,
                18,
              ),
              80,
            ),
            parameters: activeMoveGizmo.parameters,
          };
          controls.enabled = false;
          renderer.domElement.setPointerCapture(event.pointerId);
          (renderer.domElement as HTMLCanvasElement).style.cursor = "grabbing";
          pointerDown = null;
          return;
        }
      }

      pointerDown = { x: event.clientX, y: event.clientY };
      // --- Select mode: endpoint drag OR rectangle selection ---
      if (activeSketchToolRef.current === "select") {
        const selHit = intersectSceneTargets(event);

        // Endpoint drag on sketch points (non-fixed endpoints only).
        if (
          selHit?.kind === "sketch_point" &&
          activeSketchPlaneIdRef.current
        ) {
          const params = sketchLinesRef.current;
          if (params) {
            const point = params.points?.find((p) => p.point_id === selHit.id);
            console.warn(
              "[endpoint-drag] pointerDown hit sketch_point:",
              selHit.id,
              "kind:",
              selHit.pointKind,
              "is_fixed:",
              point?.is_fixed,
            );
            if (point && !point.is_fixed) {
              const rawPoint = resolveSketchPlanePoint(
                event,
                renderer,
                camera,
                activeSketchPlaneIdRef.current,
                activeSketchPlaneFrameRef.current,
              );
              if (rawPoint) {
                console.warn(
                  "[endpoint-drag] starting drag for point:",
                  selHit.id,
                  "at local:",
                  rawPoint.local,
                );
                endpointDragRef.current = {
                  pointId: selHit.id,
                  startClientX: event.clientX,
                  startClientY: event.clientY,
                  startLocalX: rawPoint.local[0],
                  startLocalY: rawPoint.local[1],
                  hasMoved: false,
                };
                controls.enabled = false;
                renderer.domElement.setPointerCapture(event.pointerId);
                (renderer.domElement as HTMLCanvasElement).style.cursor =
                  "grabbing";
                pointerDown = null;
                return;
              }
            }
          }
        }

        // Rectangle selection on empty space.
        if (
          !selHit &&
          selHit?.kind !== "sketch_dimension"
        ) {
          selectionDragRef.current = {
            startX: event.clientX,
            startY: event.clientY,
            currentX: event.clientX,
            currentY: event.clientY,
            active: true,
          };
          controls.enabled = false;
          return;
        }
      }

      renderer.domElement.setPointerCapture(event.pointerId);
      if (
        activeSketchPlaneIdRef.current &&
        (activeSketchToolRef.current === "select" ||
          activeSketchToolRef.current === "dimension")
      ) {
        const hit = intersectSceneTargets(event);
        if (hit?.kind === "sketch_dimension") {
          suppressNextDimensionEditorOpenRef.current = true;
          setIsDimensionEditorOpen(false);
          void selectSketchDimensionRef.current(hit.id);
          const dimension = displayedSketchDimensionsRef.current.find(
            (entry) => entry.dimensionId === hit.id,
          );
          const sketchPoint = resolveSketchPlanePoint(
            event,
            renderer,
            camera,
            activeSketchPlaneIdRef.current,
            activeSketchPlaneFrameRef.current,
          );
          if (dimension && sketchPoint) {
            const dragAxis =
              dimension.kind === "circle_radius"
                ? new THREE.Vector3(0, 0, 0)
                : getDimensionPlacementAxis(dimension);
            if (dimension.kind !== "circle_radius" && !dragAxis) {
              return;
            }
            dimensionLabelDragRef.current = {
              dimensionId: hit.id,
              hitPart: hit.part === "label" ? "label" : "geometry",
              startClientX: event.clientX,
              startClientY: event.clientY,
              startWorld: sketchPoint.world,
              startLabelPosition: dimension.labelPosition,
              dragAxis: dragAxis
                ? [dragAxis.x, dragAxis.y, dragAxis.z]
                : [0, 0, 0],
              hasMoved: false,
            };
            controls.enabled = false;
            (renderer.domElement as HTMLCanvasElement).style.cursor = "grabbing";
            return;
          }
        }
      }
      if (
        activeSketchPlaneIdRef.current &&
        isDraftDimensionTool(activeSketchToolRef.current) &&
        !lineDraftStartRef.current
      ) {
        const rawPoint = resolveSketchPlanePoint(
          event,
          renderer,
          camera,
          activeSketchPlaneIdRef.current,
          activeSketchPlaneFrameRef.current,
        );
        if (!rawPoint) {
          return;
        }
        const sketchPoint = resolveSnappedSketchPoint(rawPoint);
        lineDraftStartRef.current = sketchPoint.local;
        draftStartedOnPointerDownRef.current = true;
        const session = createDraftDimensionSession(
          activeSketchToolRef.current,
          sketchPoint.local,
          sketchPoint.local,
        );
        draftDimensionSessionRef.current = session;
        setDraftDimensionSession(session);
        focusDraftField(session.activeField);
      }

    }

    function handlePointerMove(event: PointerEvent) {

      // --- Rectangle selection drag tracking ---
      if (selectionDragRef.current?.active) {
        selectionDragRef.current.currentX = event.clientX;
        selectionDragRef.current.currentY = event.clientY;
        const d = selectionDragRef.current;
        const x1 = Math.min(d.startX, d.currentX);
        const y1 = Math.min(d.startY, d.currentY);
        const w = Math.abs(d.currentX - d.startX);
        const h = Math.abs(d.currentY - d.startY);
        const direction = d.currentX >= d.startX ? "window" : "crossing";
        setSelectionRect({ left: x1, top: y1, width: w, height: h, visible: w > 3 || h > 3, direction });
        return;
      }

      lastPointerEventRef.current = event;
      // -- cube-area interaction ---------------------------------------
      const cubeDpr = renderer.getPixelRatio();
      const cubeCanvasRect = renderer.domElement.getBoundingClientRect();
      const inCube = isPointerInCubeArea(event, cubeCanvasRect, cubeDpr);
      if (
        activeSketchPlaneIdRef.current &&
        activeSketchToolRef.current !== "select" &&
        activeSketchToolRef.current !== "project" &&
        !inCube
      ) {
        setCrosshairPointer({
          x: event.clientX - cubeCanvasRect.left,
          y: event.clientY - cubeCanvasRect.top,
        });
      } else {
        setCrosshairPointer(null);
      }

      if (viewCubeDraggingRef.current) {
        if (viewCubeDragStartRef.current) {
          const deltaX = event.clientX - viewCubeDragStartRef.current.x;
          const deltaY = event.clientY - viewCubeDragStartRef.current.y;
          viewCubeDragStartRef.current = { x: event.clientX, y: event.clientY };
          applyCubeDragOrbit(camera, controls, deltaX, deltaY, 0.005);
        }
        return;
      }

      const moveGizmoDrag = moveGizmoDragRef.current;
      if (moveGizmoDrag) {
        const next: MoveFeatureParameters = { ...moveGizmoDrag.parameters };
        if (moveGizmoDrag.kind === "translate" && moveGizmoDrag.axis) {
          const delta = moveGizmoTranslationDelta(
            event,
            moveGizmoDrag,
            moveGizmoDrag.axis,
            camera,
            renderer,
          );
          if (moveGizmoDrag.axis === "x") {
            next.translation_x = moveGizmoDrag.parameters.translation_x + delta;
          } else if (moveGizmoDrag.axis === "y") {
            next.translation_y = moveGizmoDrag.parameters.translation_y + delta;
          } else {
            next.translation_z = moveGizmoDrag.parameters.translation_z + delta;
          }
        } else if (moveGizmoDrag.kind === "free") {
          const worldDelta = moveGizmoFreeDelta(
            event,
            moveGizmoDrag,
            camera,
            renderer,
          );
          next.translation_x =
            moveGizmoDrag.parameters.translation_x +
            worldDelta.dot(moveGizmoDrag.axes.x);
          next.translation_y =
            moveGizmoDrag.parameters.translation_y +
            worldDelta.dot(moveGizmoDrag.axes.y);
          next.translation_z =
            moveGizmoDrag.parameters.translation_z +
            worldDelta.dot(moveGizmoDrag.axes.z);
        } else if (moveGizmoDrag.axis) {
          const angle = moveGizmoScreenAngle(
            event,
            moveGizmoDrag.center,
            camera,
            renderer,
          );
          let deltaDegrees =
            ((angle - moveGizmoDrag.startAngle) * 180) / Math.PI;
          if (deltaDegrees > 180) {
            deltaDegrees -= 360;
          } else if (deltaDegrees < -180) {
            deltaDegrees += 360;
          }
          if (moveGizmoDrag.axis === "x") {
            next.rotation_x_degrees =
              moveGizmoDrag.parameters.rotation_x_degrees + deltaDegrees;
          } else if (moveGizmoDrag.axis === "y") {
            next.rotation_y_degrees =
              moveGizmoDrag.parameters.rotation_y_degrees + deltaDegrees;
          } else {
            next.rotation_z_degrees =
              moveGizmoDrag.parameters.rotation_z_degrees + deltaDegrees;
          }
        }
        flushMoveGizmoChange(next);
        return;
      }

      if (inCube) {
        const cubeGroup = viewCubeGroupRef.current;
        const cubeCam = viewCubeCameraRef.current;
        const cubeRaycaster = viewCubeRaycasterRef.current;
        if (cubeGroup && cubeCam && cubeRaycaster) {
          const canvasWidth = renderer.domElement.width;
          const canvasHeight = renderer.domElement.height;
          const rect = getCubeViewportRect(canvasWidth, canvasHeight, cubeDpr);
          // Pointer in GL coords (origin at bottom-left)
          const glX = (event.clientX - cubeCanvasRect.left) * cubeDpr;
          const glY = canvasHeight - (event.clientY - cubeCanvasRect.top) * cubeDpr;
          const ndcX = ((glX - rect.x) / rect.width) * 2 - 1;
          const ndcY = ((glY - rect.y) / rect.height) * 2 - 1;
          cubeRaycaster.setFromCamera(new THREE.Vector2(ndcX, ndcY), cubeCam);
          const hit = raycastViewCube(cubeRaycaster, cubeGroup);
          applyCubeHover(cubeGroup, hit);
          viewCubeHoveredRef.current = hit;
          (renderer.domElement as HTMLCanvasElement).style.cursor = hit
            ? "pointer"
            : "";
        }
        return;
      }

      // Clear cube hover when pointer leaves cube area
      if (viewCubeHoveredRef.current) {
        const cubeGroup = viewCubeGroupRef.current;
        if (cubeGroup) {
          clearCubeHover(cubeGroup);
        }
        viewCubeHoveredRef.current = null;
        (renderer.domElement as HTMLCanvasElement).style.cursor = "";
      }

      const dimensionDrag = dimensionLabelDragRef.current;
      if (dimensionDrag && activeSketchPlaneIdRef.current) {
        const sketchPoint = resolveSketchPlanePoint(
          event,
          renderer,
          camera,
          activeSketchPlaneIdRef.current,
          activeSketchPlaneFrameRef.current,
        );
        if (!sketchPoint) {
          return;
        }
        if (dimensionDrag.isPlacement) {
          if (updateDimensionRelationPreview(sketchPoint.local)) {
            // When a relation snap is active, the ghost relation dimension
            // is the preview. Do not keep moving the temporary unary
            // dimension label underneath it; that causes the two previews
            // to fight and flicker.
            dimensionDrag.hasMoved = true;
            return;
          }
        }
        const dx = event.clientX - dimensionDrag.startClientX;
        const dy = event.clientY - dimensionDrag.startClientY;
        if (Math.abs(dx) > 4 || Math.abs(dy) > 4) {
          dimensionDrag.hasMoved = true;
        }
        const draggedDimension = displayedSketchDimensionsRef.current.find(
          (dimension) => dimension.dimensionId === dimensionDrag.dimensionId,
        );
        if (
          draggedDimension?.kind === "angle" ||
          draggedDimension?.kind === "line_angle"
        ) {
          const nextPosition = angleDimensionArcControlNearPoint(
            draggedDimension,
            sketchPoint.world,
          );
          if (nextPosition) {
            setDimensionLabelPosition(dimensionDrag.dimensionId, nextPosition);
          }
          return;
        }
        if (draggedDimension?.kind === "circle_radius") {
          const nextPosition = circleDimensionLabelNearPoint(
            draggedDimension,
            sketchPoint.world,
          );
          if (nextPosition) {
            setDimensionLabelPosition(dimensionDrag.dimensionId, nextPosition);
          }
          return;
        }
        const dragAxis = new THREE.Vector3(...dimensionDrag.dragAxis);
        const worldDelta = new THREE.Vector3(
          sketchPoint.world[0] - dimensionDrag.startWorld[0],
          sketchPoint.world[1] - dimensionDrag.startWorld[1],
          sketchPoint.world[2] - dimensionDrag.startWorld[2],
        );
        const constrainedDelta = dragAxis.multiplyScalar(
          worldDelta.dot(dragAxis),
        );
        const nextPositionVector = new THREE.Vector3(
          ...dimensionDrag.startLabelPosition,
        ).add(constrainedDelta);
        const nextPosition: [number, number, number] = [
          nextPositionVector.x,
          nextPositionVector.y,
          nextPositionVector.z,
        ];
        setDimensionLabelPosition(dimensionDrag.dimensionId, nextPosition);
        return;
      }

      // --- Endpoint drag tracking ---
      const endpointDrag = endpointDragRef.current;
      if (endpointDrag && activeSketchPlaneIdRef.current) {
        const rawPoint = resolveSketchPlanePoint(
          event,
          renderer,
          camera,
          activeSketchPlaneIdRef.current,
          activeSketchPlaneFrameRef.current,
        );
        if (rawPoint) {
          const sketchPoint = resolveSnappedSketchPoint(rawPoint);
          const dx = event.clientX - endpointDrag.startClientX;
          const dy = event.clientY - endpointDrag.startClientY;
          if (Math.abs(dx) > 3 || Math.abs(dy) > 3) {
            endpointDrag.hasMoved = true;
          }
          setSketchSnapLabel(sketchPoint.snapLabel);

          // Convert sketch-local position to world space.
          const frame = activeSketchPlaneFrameRef.current;
          const toWorld = (lx: number, ly: number): THREE.Vector3 => {
            if (frame) {
              const origin = new THREE.Vector3(
                frame.origin.x, frame.origin.y, frame.origin.z,
              );
              const xAxis = new THREE.Vector3(
                frame.x_axis.x, frame.x_axis.y, frame.x_axis.z,
              );
              const yAxis = new THREE.Vector3(
                frame.y_axis.x, frame.y_axis.y, frame.y_axis.z,
              );
              return origin
                .clone()
                .addScaledVector(xAxis, lx)
                .addScaledVector(yAxis, ly);
            }
            // Fallback for ref-plane sketches (e.g. ref-plane-xy).
            return new THREE.Vector3(lx, 0, ly);
          };

          const draggedWorld = toWorld(
            sketchPoint.local[0],
            sketchPoint.local[1],
          );

          // Move the dragged point mesh.
          const pointObjects = sketchPointObjectsRef.current;
          for (const obj of pointObjects) {
            if (obj.userData.sketchPointId === endpointDrag.pointId) {
              obj.position.copy(draggedWorld);
              break;
            }
          }

          // A single point ID may be shared by multiple lines (e.g. a
          // rectangle corner).  Update every line that references the
          // dragged point so all connected geometry rubber-bands together.
          const params = sketchLinesRef.current;
          if (params) {
            const connectedLines = params.lines.filter(
              (l) =>
                l.start_point_id === endpointDrag.pointId ||
                l.end_point_id === endpointDrag.pointId,
            );
            for (const ownerLine of connectedLines) {
              const isStart =
                ownerLine.start_point_id === endpointDrag.pointId;
              const fixedLocal: [number, number] = isStart
                ? [ownerLine.end_x, ownerLine.end_y]
                : [ownerLine.start_x, ownerLine.start_y];
              const fixedWorld = toWorld(fixedLocal[0], fixedLocal[1]);

              const lineObj = sketchEntityObjectByIdRef.current.get(
                ownerLine.line_id,
              );
              if (
                lineObj &&
                lineObj instanceof THREE.Line &&
                lineObj.geometry.attributes.position
              ) {
                const pos = lineObj.geometry.attributes.position
                  .array as Float32Array;
                if (isStart) {
                  pos[0] = draggedWorld.x;
                  pos[1] = draggedWorld.y;
                  pos[2] = draggedWorld.z;
                  pos[3] = fixedWorld.x;
                  pos[4] = fixedWorld.y;
                  pos[5] = fixedWorld.z;
                } else {
                  pos[0] = fixedWorld.x;
                  pos[1] = fixedWorld.y;
                  pos[2] = fixedWorld.z;
                  pos[3] = draggedWorld.x;
                  pos[4] = draggedWorld.y;
                  pos[5] = draggedWorld.z;
                }
                lineObj.geometry.attributes.position.needsUpdate = true;
                // Dashed lines need per-vertex distances recomputed.
                if (
                  lineObj.material instanceof
                  THREE.LineDashedMaterial
                ) {
                  lineObj.computeLineDistances();
                }
              }
            }
          }
        }
        return;
      }

      if (activeSketchPlaneId) {
        if (activeSketchToolRef.current === "select") {
          clearPreviewLine();
          clearPreviewCircle();
          clearPreviewArc();
          clearPreviewDimension();
          setSketchSnapLabel(null);
          setConstraintPreview(null);
          clearDraftDimensionSession();
          const hit = intersectSceneTargets(event);
          setHoveredReference(null);
          setHoveredPrimitive(null);
          setHoveredFace(null);
          setHoveredEdge(null);
          setHoveredVertex(null);
          setHoveredSketchPoint(hit?.kind === "sketch_point" ? hit.id : null);
          setHoveredSketchEntity(
            hit?.kind === "sketch_entity" ? hit.id : null,
          );
          setHoveredSketchProfile(
            hit?.kind === "sketch_profile" ? hit.id : null,
          );
          return;
        }

        // Project tool is a picker, not a draftsman: skip every
        // draft preview / snap-label resolution and run the body
        // face / edge / vertex hover highlight directly so the
        // user sees what they're about to pick. We can't fall
        // through to the bottom of this function because the
        // line-draft branches below `return` early before we get
        // there — instead we mirror the same hover dispatch here
        // and bail out.
        if (activeSketchToolRef.current === "project") {
          clearPreviewLine();
          clearPreviewCircle();
          clearPreviewArc();
          clearPreviewDimension();
          setSketchSnapLabel(null);
          setConstraintPreview(null);
          clearDraftDimensionSession();
          const projectHit = intersectSceneTargets(event);
          setHoveredReference(null);
          setHoveredPrimitive(null);
          setHoveredSketchProfile(null);
          setHoveredSketchPoint(null);
          setHoveredSketchEntity(null);
          setHoveredFace(projectHit?.kind === "face" ? projectHit.id : null);
          setHoveredEdge(projectHit?.kind === "edge" ? projectHit.id : null);
          setHoveredVertex(
            projectHit?.kind === "vertex" ? projectHit.id : null,
          );
          return;
        }

        // Trim tool hover: highlight the segment under the cursor in red.
        if (activeSketchToolRef.current === "trim") {
          clearPreviewLine();
          clearPreviewCircle();
          clearPreviewArc();
          clearPreviewDimension();
          setSketchSnapLabel(null);
          setConstraintPreview(null);
          clearDraftDimensionSession();
          const trimHit = intersectSceneTargets(event);
          setHoveredSketchEntity(
            trimHit?.kind === "sketch_entity" ? trimHit.id : null,
          );
          setHoveredSketchProfile(null);
          setHoveredSketchPoint(null);
          setHoveredFace(null);
          setHoveredEdge(null);
          setHoveredVertex(null);

          const scn = sceneDataRef.current;
          if (
            !scn ||
            !trimHit ||
            trimHit.kind !== "sketch_entity" ||
            (trimHit.entityKind !== "line" && trimHit.entityKind !== "circle" && trimHit.entityKind !== "arc")
          ) {
            clearTrimSegmentHighlight();
            return;
          }

          // Compute the 2D sketch-local position of the cursor.
          const rawPt = resolveSketchPlanePoint(
            event,
            renderer,
            camera,
            activeSketchPlaneId,
            activeSketchPlaneFrame,
          );
          if (!rawPt) {
            clearTrimSegmentHighlight();
            clearTrimArcHighlight();
            return;
          }
          const [mx, my] = rawPt.local;

          const pid = activeSketchPlaneId;
          const frame = activeSketchPlaneFrameRef.current;

          const toLocal = (px: number, py: number, pz: number): [number, number] => {
            if (frame) {
              const dx = px - frame.origin.x;
              const dy = py - frame.origin.y;
              const dz = pz - frame.origin.z;
              return [
                dx * frame.x_axis.x + dy * frame.x_axis.y + dz * frame.x_axis.z,
                dx * frame.y_axis.x + dy * frame.y_axis.y + dz * frame.y_axis.z,
              ];
            }
            if (pid === "ref-plane-xy") return [px, pz];
            if (pid === "ref-plane-yz") return [py, pz];
            return [px, py];
          };
          const toWorld = (ux: number, uy: number): [number, number, number] => {
            if (frame) {
              return [
                frame.origin.x + ux * frame.x_axis.x + uy * frame.y_axis.x,
                frame.origin.y + ux * frame.x_axis.y + uy * frame.y_axis.y,
                frame.origin.z + ux * frame.x_axis.z + uy * frame.y_axis.z,
              ];
            }
            if (pid === "ref-plane-xy") return [ux, 0, uy];
            if (pid === "ref-plane-yz") return [0, ux, uy];
            return [ux, uy, 0];
          };

          // Line hover
          if (trimHit.entityKind === "line") {
          const lineData = scn.sketchLines.find((l) => l.lineId === trimHit.id);
          if (!lineData) {
            clearTrimSegmentHighlight();
            clearTrimArcHighlight();
            return;
          }

          // Convert 3D endpoints to 2D sketch-local.
          const wx = lineData.start[0], wy = lineData.start[1], wz = lineData.start[2];
          const lx = lineData.end[0], ly = lineData.end[1], lz = lineData.end[2];

          const [ax2, ay2] = toLocal(wx, wy, wz);
          const [bx2, by2] = toLocal(lx, ly, lz);

          // Collect intersection parameters along the line.
          const ts: number[] = [];

          // Line-line intersections.
          for (const other of scn.sketchLines) {
            if (other.lineId === lineData.lineId || other.isConstruction) continue;
            const [ox2, oy2] = toLocal(other.start[0], other.start[1], other.start[2]);
            const [ox3, oy3] = toLocal(other.end[0], other.end[1], other.end[2]);
            const t = lineLineIntersectionTrim(ax2, ay2, bx2, by2, ox2, oy2, ox3, oy3);
            if (t !== null) ts.push(t);
          }

          // Line-circle intersections.
          for (const circle of scn.sketchCircles) {
            if (circle.isConstruction) continue;
            const [cx, cy] = toLocal(circle.center[0], circle.center[1], circle.center[2]);
            const lens = lineCircleIntersectionTrim(ax2, ay2, bx2, by2, cx, cy, circle.radius);
            for (const t of lens) ts.push(t);
          }

          if (ts.length === 0) {
            // No intersections — the entire line is one segment.
            const [wsx, wsy, wsz] = toWorld(ax2, ay2);
            const [wex, wey, wez] = toWorld(bx2, by2);
            updateTrimSegmentHighlight(lineData.lineId, [
              { sx: wsx, sy: wsy, sz: wsz, ex: wex, ey: wey, ez: wez },
            ], 0);
            return;
          }

          console.log("[trim_hover] line=", lineData.lineId,
            "ts_raw=", ts.map((t: number) => t.toFixed(3)),
            "n_lines=", scn.sketchLines.length,
            "n_circles=", scn.sketchCircles.length);
          // Sort and deduplicate.
          ts.sort((a, b) => a - b);
          const deduped: number[] = [ts[0]];
          for (let i = 1; i < ts.length; i++) {
            if (ts[i] - deduped[deduped.length - 1] > 0.01) {
              deduped.push(ts[i]);
            }
          }

          // Build segments.
          const segs: Array<{ sx: number; sy: number; sz: number; ex: number; ey: number; ez: number }> = [];
          // First segment: 0 → deduped[0]
          {
            const u = 0, v = deduped[0];
            const sx = ax2 + u * (bx2 - ax2), sy = ay2 + u * (by2 - ay2);
            const ex = ax2 + v * (bx2 - ax2), ey = ay2 + v * (by2 - ay2);
            const [wsx, wsy, wsz] = toWorld(sx, sy);
            const [wex, wey, wez] = toWorld(ex, ey);
            segs.push({ sx: wsx, sy: wsy, sz: wsz, ex: wex, ey: wey, ez: wez });
          }
          // Middle segments.
          for (let i = 0; i + 1 < deduped.length; i++) {
            const u = deduped[i], v = deduped[i + 1];
            const sx = ax2 + u * (bx2 - ax2), sy = ay2 + u * (by2 - ay2);
            const ex = ax2 + v * (bx2 - ax2), ey = ay2 + v * (by2 - ay2);
            const [wsx, wsy, wsz] = toWorld(sx, sy);
            const [wex, wey, wez] = toWorld(ex, ey);
            segs.push({ sx: wsx, sy: wsy, sz: wsz, ex: wex, ey: wey, ez: wez });
          }
          // Last segment: deduped[last] → 1
          {
            const u = deduped[deduped.length - 1], v = 1;
            const sx = ax2 + u * (bx2 - ax2), sy = ay2 + u * (by2 - ay2);
            const ex = ax2 + v * (bx2 - ax2), ey = ay2 + v * (by2 - ay2);
            const [wsx, wsy, wsz] = toWorld(sx, sy);
            const [wex, wey, wez] = toWorld(ex, ey);
            segs.push({ sx: wsx, sy: wsy, sz: wsz, ex: wex, ey: wey, ez: wez });
          }

          // Determine which segment the cursor is on.
          const abx = bx2 - ax2, aby = by2 - ay2;
          const abLenSq = abx * abx + aby * aby;
          let clickT = 0;
          if (abLenSq > 1e-6) {
            clickT = ((mx - ax2) * abx + (my - ay2) * aby) / abLenSq;
            clickT = Math.max(0, Math.min(1, clickT));
          }
          let hoveredIdx = -1;
          // Check segments in order: first, middle (loop), last
          if (clickT >= 0 - 1e-10 && clickT <= deduped[0] + 1e-10) {
            hoveredIdx = 0;
          } else {
            for (let i = 0; i + 1 < deduped.length; i++) {
              if (clickT >= deduped[i] - 1e-10 && clickT <= deduped[i + 1] + 1e-10) {
                hoveredIdx = i + 1;
                break;
              }
            }
            if (hoveredIdx < 0 && clickT >= deduped[deduped.length - 1] - 1e-10) {
              hoveredIdx = deduped.length; // last segment
            }
          }

          updateTrimSegmentHighlight(lineData.lineId, segs, hoveredIdx);
          return;
        }

        // Circle hover: highlight the arc segment under the cursor.
        if (trimHit.entityKind === "circle") {
          clearTrimSegmentHighlight();
          const circleData = scn.sketchCircles.find((c) => c.circleId === trimHit.id);
          if (!circleData) { clearTrimArcHighlight(); return; }

          const [clx, cly] = toLocal(circleData.center[0], circleData.center[1], circleData.center[2]);
          const cursorAngle = Math.atan2(my - cly, mx - clx);
          const wrap = (a: number) => { while (a < 0) a += 2*Math.PI; while (a >= 2*Math.PI) a -= 2*Math.PI; return a; };
          const cAngle = wrap(cursorAngle);
          const angles: number[] = [];

          for (const other of scn.sketchLines) {
            if (other.isConstruction) continue;
            const [ox, oy] = toLocal(other.start[0], other.start[1], other.start[2]);
            const [ox2, oy2] = toLocal(other.end[0], other.end[1], other.end[2]);
            const ts = lineCircleIntersectionTrim(ox, oy, ox2, oy2, clx, cly, circleData.radius);
            for (const t of ts) {
              const ix = ox + t * (ox2 - ox), iy = oy + t * (oy2 - oy);
              angles.push(wrap(Math.atan2(iy - cly, ix - clx)));
            }
          }
          for (const other of scn.sketchCircles) {
            if (other.circleId === circleData.circleId || other.isConstruction) continue;
            const [ocx, ocy] = toLocal(other.center[0], other.center[1], other.center[2]);
            const dx = ocx - clx, dy = ocy - cly;
            const d = Math.hypot(dx, dy);
            const r1 = circleData.radius, r2 = other.radius;
            if (d > r1 + r2 + 0.01 || d < Math.abs(r1 - r2) - 0.01) continue;
            const a = (r1 * r1 - r2 * r2 + d * d) / (2 * d);
            const hSq = r1 * r1 - a * a;
            const h = hSq <= 0 ? 0 : Math.sqrt(hSq);
            const px = clx + a * dx / d, py = cly + a * dy / d;
            const hx = -dy * h / d, hy = dx * h / d;
            angles.push(wrap(Math.atan2(py + hy - cly, px + hx - clx)));
            if (hSq > 1e-12) angles.push(wrap(Math.atan2(py - hy - cly, px - hx - clx)));
          }

          if (angles.length === 0) {
            // No intersections — full circle as one segment.
            const pts: Array<[number, number, number]> = [];
            for (let i = 0; i <= 48; i++) {
              const a = (i / 48) * 2 * Math.PI;
              const [wx, wy, wz] = toWorld(clx + circleData.radius * Math.cos(a), cly + circleData.radius * Math.sin(a));
              pts.push([wx, wy, wz]);
            }
            updateTrimArcHighlight(pts);
            return;
          }

          angles.sort((a, b) => a - b);
          const deduped = [angles[0]];
          for (let i = 1; i < angles.length; i++)
            if (angles[i] - deduped[deduped.length-1] > 0.01) deduped.push(angles[i]);
          const N = deduped.length;
          const full = 2 * Math.PI;

          let hoveredSeg = -1;
          for (let i = 0; i < N; i++) {
            const s = deduped[i];
            const e = deduped[(i+1)%N];
            const ee = e <= s ? e + full : e;
            let ta = cAngle;
            if (e <= s && ta < s) ta += full;
            if (ta >= s - 1e-10 && ta <= ee + 1e-10) { hoveredSeg = i; break; }
          }
          if (hoveredSeg < 0) { clearTrimArcHighlight(); return; }

          const s = deduped[hoveredSeg];
          const e = deduped[(hoveredSeg+1)%N];
          const ee = e <= s ? e + full : e;
          const pts: Array<[number, number, number]> = [];
          const steps = 48;
          for (let i = 0; i <= steps; i++) {
            const a = s + (ee - s) * (i / steps);
            const [wx, wy, wz] = toWorld(
              clx + circleData.radius * Math.cos(a),
              cly + circleData.radius * Math.sin(a));
            pts.push([wx, wy, wz]);
          }
          updateTrimArcHighlight(pts);
          return;
        }

        // Arc hover: like circle but constrained to the arc's sweep.
        if (trimHit.entityKind === "arc") {
          clearTrimSegmentHighlight();
          const arcData = scn.sketchArcs.find((a) => a.arcId === trimHit.id);
          if (!arcData) { clearTrimArcHighlight(); return; }

          const [clx, cly] = toLocal(arcData.center[0], arcData.center[1], arcData.center[2]);
          const [asx, asy] = toLocal(arcData.start[0], arcData.start[1], arcData.start[2]);
          const [aex, aey] = toLocal(arcData.end[0], arcData.end[1], arcData.end[2]);
          const aStart = Math.atan2(asy - cly, asx - clx);
          const aEnd = Math.atan2(aey - cly, aex - clx);
          const ccw = arcData.ccw;

          const cursorAngle = Math.atan2(my - cly, mx - clx);
          const wrap = (a: number) => { while (a < 0) a += 2*Math.PI; while (a >= 2*Math.PI) a -= 2*Math.PI; return a; };
          const full = 2 * Math.PI;
          let cAngle = wrap(cursorAngle);
          if (ccw) { if (cAngle < wrap(aStart)) cAngle += full; }
          else { if (cAngle > wrap(aStart)) cAngle -= full; }

          const angles: number[] = [];
          for (const other of scn.sketchLines) {
            if (other.isConstruction) continue;
            const [ox, oy] = toLocal(other.start[0], other.start[1], other.start[2]);
            const [ox2, oy2] = toLocal(other.end[0], other.end[1], other.end[2]);
            const ts = lineCircleIntersectionTrim(ox, oy, ox2, oy2, clx, cly, arcData.radius);
            for (const t of ts) {
              const ix = ox + t * (ox2 - ox), iy = oy + t * (oy2 - oy);
              let ai = wrap(Math.atan2(iy - cly, ix - clx));
              if (ccw) { if (ai < wrap(aStart)) ai += full; }
              else { if (ai > wrap(aStart)) ai -= full; }
              if ((ccw && ai >= wrap(aStart) - 1e-10 && ai <= (wrap(aEnd) <= wrap(aStart) ? wrap(aEnd) + full : wrap(aEnd)) + 1e-10) ||
                  (!ccw && ai <= wrap(aStart) + 1e-10 && ai >= (wrap(aEnd) >= wrap(aStart) ? wrap(aEnd) - full : wrap(aEnd)) - 1e-10))
                angles.push(ai);
            }
          }
          for (const other of scn.sketchCircles) {
            if (other.isConstruction) continue;
            const [ocx, ocy] = toLocal(other.center[0], other.center[1], other.center[2]);
            const dx = ocx - clx, dy = ocy - cly;
            const d = Math.hypot(dx, dy);
            if (d > arcData.radius + other.radius + 0.01 || d < Math.abs(arcData.radius - other.radius) - 0.01) continue;
            const a = (arcData.radius * arcData.radius - other.radius * other.radius + d * d) / (2 * d);
            const hSq = arcData.radius * arcData.radius - a * a;
            const h = hSq <= 0 ? 0 : Math.sqrt(hSq);
            const px = clx + a * dx / d, py = cly + a * dy / d;
            const hx = -dy * h / d, hy = dx * h / d;
            for (const [ix, iy] of [[px + hx, py + hy], [px - hx, py - hy]] as [number, number][]) {
              let ai = wrap(Math.atan2(iy - cly, ix - clx));
              if (ccw) { if (ai < wrap(aStart)) ai += full; }
              else { if (ai > wrap(aStart)) ai -= full; }
              if ((ccw && ai >= wrap(aStart) - 1e-10 && ai <= (wrap(aEnd) <= wrap(aStart) ? wrap(aEnd) + full : wrap(aEnd)) + 1e-10) ||
                  (!ccw && ai <= wrap(aStart) + 1e-10 && ai >= (wrap(aEnd) >= wrap(aStart) ? wrap(aEnd) - full : wrap(aEnd)) - 1e-10))
                angles.push(ai);
            }
          }

          if (angles.length === 0) {
            const pts: Array<[number, number, number]> = [];
            const steps = 48;
            for (let i = 0; i <= steps; i++) {
              const ang = aStart + (ccw ? 1 : -1) * (i / steps) * Math.abs(ccw ? (wrap(aEnd) <= wrap(aStart) ? wrap(aEnd) + full - wrap(aStart) : wrap(aEnd) - wrap(aStart)) : (wrap(aStart) - (wrap(aEnd) >= wrap(aStart) ? wrap(aEnd) - full : wrap(aEnd))));
              const [wx, wy, wz] = toWorld(clx + arcData.radius * Math.cos(ang), cly + arcData.radius * Math.sin(ang));
              pts.push([wx, wy, wz]);
            }
            updateTrimArcHighlight(pts);
            return;
          }

          angles.sort((a, b) => ccw ? a - b : b - a);
          const deduped = [angles[0]];
          for (let i = 1; i < angles.length; i++)
            if (Math.abs(angles[i] - deduped[deduped.length-1]) > 0.01) deduped.push(angles[i]);

          let hoveredSeg = -1;
          const N = deduped.length;
          const segs = [wrap(aStart), ...deduped, ccw ? (wrap(aEnd) <= wrap(aStart) ? wrap(aEnd) + full : wrap(aEnd)) : (wrap(aEnd) >= wrap(aStart) ? wrap(aEnd) - full : wrap(aEnd))];
          for (let i = 0; i <= N; i++) {
            const s = segs[i], e = segs[i+1];
            let ta = cAngle;
            if (ccw) { if (ta < s) ta += full; }
            else { if (ta > s) ta -= full; }
            if (ta >= s - 1e-10 && ta <= e + 1e-10) { hoveredSeg = i; break; }
          }
          if (hoveredSeg < 0) { clearTrimArcHighlight(); return; }

          const s = segs[hoveredSeg], e = segs[hoveredSeg+1];
          const pts: Array<[number, number, number]> = [];
          const steps = 48;
          for (let i = 0; i <= steps; i++) {
            const ang = s + (e - s) * (i / steps);
            const [wx, wy, wz] = toWorld(clx + arcData.radius * Math.cos(ang), cly + arcData.radius * Math.sin(ang));
            pts.push([wx, wy, wz]);
          }
          updateTrimArcHighlight(pts);
          return;
        }
        }

        setHoveredSketchProfile(null);
        setHoveredSketchPoint(null);
        setHoveredSketchEntity(null);
        const draftStart = lineDraftStartRef.current;
        const rawPoint = resolveSketchPlanePoint(
          event,
          renderer,
          camera,
          activeSketchPlaneId,
          activeSketchPlaneFrame,
        );
        if (!rawPoint) {
          return;
        }

        const sketchPoint = resolveSnappedSketchPoint(rawPoint);
        setSketchSnapLabel(sketchPoint.snapLabel);
        if (
          isDraftDimensionTool(activeSketchToolRef.current) &&
          draftDimensionSessionRef.current
        ) {
          updateDraftSessionFromPoint(sketchPoint.local);
        }
        const draftPreviewLocal =
          isDraftDimensionTool(activeSketchToolRef.current) &&
          draftDimensionSessionRef.current
            ? draftDimensionSessionRef.current.current
            : sketchPoint.local;
        const draftPreviewWorld = toWorldPoint(
          activeSketchPlaneId,
          draftPreviewLocal,
          activeSketchPlaneFrame,
        );

        // Hover-time constraint preview. The badge sits next to the
        // cursor in viewport-local coordinates so it stays glued to
        // the pointer regardless of canvas size or scroll. Pixel
        // offsets are taken from `getBoundingClientRect` rather than
        // `clientX/Y` directly because the canvas can be inset
        // inside the viewport panel (toolbar, side panels, etc.).
        const canvasRect = renderer.domElement.getBoundingClientRect();
        const previewX = event.clientX - canvasRect.left;
        const previewY = event.clientY - canvasRect.top;
        if (sketchPoint.snapMidpointHostLineId) {
          // Whole-line midpoint snaps render the "M" glyph; sub-
          // segment midpoints commit as parametric anchors and so
          // get the same "/" point-on-line glyph as a generic
          // line-body snap, keeping the visual language aligned
          // with what's actually being created on commit.
          const isWhole =
            sketchPoint.snapMidpointT !== null &&
            sketchPoint.snapMidpointT !== undefined &&
            Math.abs(sketchPoint.snapMidpointT - 0.5) < 1e-9;
          setConstraintPreview({
            kind: isWhole ? "midpoint" : "on_line",
            x: previewX,
            y: previewY,
          });
        } else if (sketchPoint.snapPerpendicularHostLineId) {
          setConstraintPreview({
            kind: "perpendicular",
            x: previewX,
            y: previewY,
          });
        } else if (
          sketchPoint.snapLabel &&
          sketchPoint.snapLabel.startsWith("On ")
        ) {
          setConstraintPreview({
            kind: "on_line",
            x: previewX,
            y: previewY,
          });
        } else if (sketchPoint.snapAxisLock) {
          setConstraintPreview({
            kind: sketchPoint.snapAxisLock,
            x: previewX,
            y: previewY,
          });
        } else if (sketchPoint.snapTangentCircleId) {
          setConstraintPreview({
            kind: "tangent",
            x: previewX,
            y: previewY,
          });
        } else {
          setConstraintPreview(null);
        }

        if (!draftStart) {
          setHoveredPrimitive(null);
          setHoveredReference(null);
          return;
        }

        const sketchGroupRefValue = sketchGroupRef.current;
        if (!sketchGroupRefValue) {
          return;
        }

        clearPreviewLine();
        clearPreviewCircle();
        clearPreviewArc();
        clearPreviewDimension();
        if (activeSketchToolRef.current === "arc") {
          // Arc preview branches on the user's progress through the
          // three-click sequence and the active arc creation mode.
          // Goal: the dashed preview should always show the *exact*
          // arc the next click will commit (or, for the early
          // single-click states, an unambiguous hint about what is
          // currently locked in).
          const arcSecondPoint = arcSecondPointRef.current;
          const cursor = sketchPoint.local;
          const arcMode = arcToolModeRef.current;

          // Build a SketchArcScene-shaped preview from a 2D
          // (center, radius, start, end, ccw) tuple. The
          // `buildSketchArcObject` renderer projects coords back
          // through the plane frame so we can hand it world-space
          // coords directly.
          const buildArcPreview = (
            centerLocal: [number, number],
            radius: number,
            startLocal: [number, number],
            endLocal: [number, number],
            ccw: boolean,
          ) => {
            if (radius < 1e-3) {
              return null;
            }
            return buildSketchArcObject(
              {
                arcId: "preview-arc",
                startPointId: "preview-arc-start",
                endPointId: "preview-arc-end",
                planeId: activeSketchPlaneId,
                planeFrame: activeSketchPlaneFrame,
                center: toWorldPoint(
                  activeSketchPlaneId,
                  centerLocal,
                  activeSketchPlaneFrame,
                ),
                radius,
                start: toWorldPoint(
                  activeSketchPlaneId,
                  startLocal,
                  activeSketchPlaneFrame,
                ),
                end: toWorldPoint(
                  activeSketchPlaneId,
                  endLocal,
                  activeSketchPlaneFrame,
                ),
                ccw,
                isSelected: false,
                isConstruction: sketchToolConstructionRef.current,
                isPreview: true,
                isProjected: false,
              },
              activeSketchPlaneFrame,
            );
          };

          if (arcMode === "three_point") {
            if (!arcSecondPoint) {
              // Click 2 still pending. We don't know enough to draw
              // an arc yet, so render a dashed line from start to
              // cursor as a chord-of-the-future-end hint. Mirrors
              // the existing "line preview while drafting" cue but
              // dashed so it reads as not-yet-an-arc.
              const preview = new THREE.Line(
                new THREE.BufferGeometry().setFromPoints([
                  new THREE.Vector3(
                    ...toWorldPoint(
                      activeSketchPlaneId,
                      draftStart,
                      activeSketchPlaneFrame,
                    ),
                  ),
                  new THREE.Vector3(...sketchPoint.world),
                ]),
                new THREE.LineDashedMaterial({
                  color: themeColor("--color-tertiary-plane-edge", "#ffe784"),
                  transparent: true,
                  opacity: 0.65,
                  dashSize: 1,
                  gapSize: 0.6,
                }),
              );
              preview.computeLineDistances();
              previewArcRef.current = preview;
              sketchGroupRefValue.add(preview);
            } else {
              // Click 2 captured: cursor is the bulge anchor. Solve
              // the circumcircle of (start, cursor, end) — same
              // formula as `DocumentManager::add_sketch_arc` so the
              // preview lands exactly on what we'll commit. Falls
              // through to a no-preview state when the three points
              // are colinear (degenerate arc).
              const [sx, sy] = draftStart;
              const [ex, ey] = arcSecondPoint;
              const [ax, ay] = cursor;
              const d = 2 * (sx * (ey - ay) + ex * (ay - sy) + ax * (sy - ey));
              if (Math.abs(d) > 1e-9) {
                const s2 = sx * sx + sy * sy;
                const e2 = ex * ex + ey * ey;
                const a2 = ax * ax + ay * ay;
                const cx =
                  (s2 * (ey - ay) + e2 * (ay - sy) + a2 * (sy - ey)) / d;
                const cy =
                  (s2 * (ax - ex) + e2 * (sx - ax) + a2 * (ex - sx)) / d;
                const radius = Math.hypot(sx - cx, sy - cy);
                const cross = (ax - sx) * (ey - sy) - (ay - sy) * (ex - sx);
                const ccw = cross > 0;
                const preview = buildArcPreview(
                  [cx, cy],
                  radius,
                  [sx, sy],
                  [ex, ey],
                  ccw,
                );
                if (preview) {
                  previewArcRef.current = preview;
                  sketchGroupRefValue.add(preview);
                }
              }
            }
          } else {
            // center_start_end mode.
            if (!arcSecondPoint) {
              // Click 2 still pending. Center is locked; draw a
              // dashed full circle of radius |cursor - center| so
              // the user can see the radius they're about to
              // commit. Once they click, the start position will be
              // the cursor's current position.
              const radius = Math.hypot(
                cursor[0] - draftStart[0],
                cursor[1] - draftStart[1],
              );
              if (radius >= 1e-3) {
                const preview = buildSketchCircleObject(
                  {
                    circleId: "preview-arc-circle",
                    planeId: activeSketchPlaneId,
                    planeFrame: activeSketchPlaneFrame,
                    center: toWorldPoint(
                      activeSketchPlaneId,
                      draftStart,
                      activeSketchPlaneFrame,
                    ),
                    radius,
                    isSelected: false,
                    isConstruction: sketchToolConstructionRef.current,
                    // Mark as preview so it renders dashed +
                    // translucent. (LineLoop is fine here even
                    // though the ref is typed as Line — the
                    // dispose path only touches geometry/material.)
                    isPreview: true,
                    isProjected: false,
                  },
                  activeSketchPlaneFrame,
                );
                previewArcRef.current = preview as unknown as THREE.Line;
                sketchGroupRefValue.add(preview);
              }
            } else {
              // Click 2 captured (= start). Cursor is the end; we
              // snap it onto the circle established by
              // |center → start| just like the core does, then
              // render the arc.
              const cx = draftStart[0];
              const cy = draftStart[1];
              const sx = arcSecondPoint[0];
              const sy = arcSecondPoint[1];
              const radius = Math.hypot(sx - cx, sy - cy);
              const endDx = cursor[0] - cx;
              const endDy = cursor[1] - cy;
              const endLen = Math.hypot(endDx, endDy);
              if (radius >= 1e-3 && endLen >= 1e-3) {
                const finalEx = cx + (endDx * radius) / endLen;
                const finalEy = cy + (endDy * radius) / endLen;
                const cross =
                  (sx - cx) * (finalEy - cy) - (sy - cy) * (finalEx - cx);
                const ccw = cross > 0;
                const preview = buildArcPreview(
                  [cx, cy],
                  radius,
                  [sx, sy],
                  [finalEx, finalEy],
                  ccw,
                );
                if (preview) {
                  previewArcRef.current = preview;
                  sketchGroupRefValue.add(preview);
                }
              }
            }
          }
          return;
        }
        if (activeSketchToolRef.current === "circle") {
          const circleMode = circleToolModeRef.current;
          if (circleMode === "three_point") {
            // 3-point circle: two-phase preview.
            // Click 1 locked (draftStart); click 2 captured by circleSecondPointRef.
            const circleSecondPoint = circleSecondPointRef.current;
            const cursor = draftPreviewLocal;
            if (!circleSecondPoint) {
              // Click 2 still pending — dashed chord hint from first point.
              const preview = new THREE.Line(
                new THREE.BufferGeometry().setFromPoints([
                  new THREE.Vector3(
                    ...toWorldPoint(
                      activeSketchPlaneId,
                      draftStart,
                      activeSketchPlaneFrame,
                    ),
                  ),
                  new THREE.Vector3(...draftPreviewWorld),
                ]),
                new THREE.LineDashedMaterial({
                  color: themeColor("--color-tertiary-plane-edge", "#ffe784"),
                  transparent: true,
                  opacity: 0.65,
                  dashSize: 1,
                  gapSize: 0.6,
                }),
              );
              preview.computeLineDistances();
              previewLineRef.current = preview;
              sketchGroupRefValue.add(preview);
            } else {
              // Click 2 captured: cursor is the third point. Compute
              // circumcircle of (draftStart, secondPoint, cursor).
              const [p1x, p1y] = draftStart;
              const [p2x, p2y] = circleSecondPoint;
              const [p3x, p3y] = cursor;
              const d = 2 * (p1x * (p2y - p3y) + p2x * (p3y - p1y) + p3x * (p1y - p2y));
              if (Math.abs(d) > 1e-9) {
                const ux =
                  ((p1x * p1x + p1y * p1y) * (p2y - p3y) +
                   (p2x * p2x + p2y * p2y) * (p3y - p1y) +
                   (p3x * p3x + p3y * p3y) * (p1y - p2y)) / d;
                const uy =
                  ((p1x * p1x + p1y * p1y) * (p3x - p2x) +
                   (p2x * p2x + p2y * p2y) * (p1x - p3x) +
                   (p3x * p3x + p3y * p3y) * (p2x - p1x)) / d;
                const radius = Math.hypot(p1x - ux, p1y - uy);
                if (radius >= 1e-3) {
                  const preview = buildSketchCircleObject(
                    {
                      circleId: "preview-3pt-circle",
                      planeId: activeSketchPlaneId,
                      planeFrame: activeSketchPlaneFrame,
                      center: toWorldPoint(
                        activeSketchPlaneId,
                        [ux, uy],
                        activeSketchPlaneFrame,
                      ),
                      radius,
                      isSelected: false,
                      isConstruction: sketchToolConstructionRef.current,
                      isPreview: true,
                      isProjected: false,
                    },
                    activeSketchPlaneFrame,
                  );
                  previewCircleRef.current = preview;
                  sketchGroupRefValue.add(preview);
                }
              }
            }
          } else if (circleMode === "two_point") {
            // Two-point circle: center at midpoint of (click, cursor),
            // radius = half the distance. Same math as handlePointerUp.
            const p1x = draftStart[0];
            const p1y = draftStart[1];
            const p2x = draftPreviewLocal[0];
            const p2y = draftPreviewLocal[1];
            const dist = distanceBetweenPoints(draftStart, draftPreviewLocal);
            if (dist > 0.001) {
              const cx = (p1x + p2x) / 2;
              const cy = (p1y + p2y) / 2;
              const radius = dist / 2;
              const preview = buildSketchCircleObject(
                {
                  circleId: "preview-2pt-circle",
                  planeId: activeSketchPlaneId,
                  planeFrame: activeSketchPlaneFrame,
                  center: toWorldPoint(
                    activeSketchPlaneId,
                    [cx, cy],
                    activeSketchPlaneFrame,
                  ),
                  radius,
                  isSelected: false,
                  isConstruction: sketchToolConstructionRef.current,
                  isPreview: false,
                  isProjected: false,
                },
                activeSketchPlaneFrame,
              );
              previewCircleRef.current = preview;
              sketchGroupRefValue.add(preview);
            }
          } else {
            // center_radius: existing draft circle from center.
            const radius = distanceBetweenPoints(draftStart, draftPreviewLocal);
            if (radius > 0.001) {
              // Pass the active sketch's plane frame so the perimeter is
              // projected onto the actual sketch plane. Without it the
              // perimeter falls back to the legacy ref-plane axis
              // mapping which disagrees with the center's projection
              // for arbitrary planes (face-based sketches), and the
              // circle reads as perpendicular to the sketch plane.
              const preview = buildSketchCircleObject(
                {
                  circleId: "preview-circle",
                  planeId: activeSketchPlaneId,
                  planeFrame: activeSketchPlaneFrame,
                  center: toWorldPoint(
                    activeSketchPlaneId,
                    draftStart,
                    activeSketchPlaneFrame,
                  ),
                  radius,
                  isSelected: false,
                  isConstruction: sketchToolConstructionRef.current,
                  // The line/circle draft preview (drawn while the
                  // user is dragging out a new circle) is not a
                  // tool-generated preview entity; the renderer's
                  // dashed-translucent path is gated on `isPreview`,
                  // so leaving it false keeps the existing draft
                  // styling intact.
                  isPreview: false,
                  isProjected: false,
                },
                activeSketchPlaneFrame,
              );
              previewCircleRef.current = preview;
              sketchGroupRefValue.add(preview);
              renderCircleDraftDimension(
                sketchGroupRefValue,
                draftStart,
                draftPreviewLocal,
              );
            }
          }
        } else if (activeSketchToolRef.current === "rectangle") {
          const rectMode = rectangleToolModeRef.current;
          if (rectMode === "three_point") {
            // 3-point rectangle: two-phase preview.
            const rectSecondPoint = rectSecondPointRef.current;
            const cursor = draftPreviewLocal;
            if (!rectSecondPoint) {
              // Click 2 still pending — dashed first-edge hint.
              const preview = new THREE.Line(
                new THREE.BufferGeometry().setFromPoints([
                  new THREE.Vector3(
                    ...toWorldPoint(
                      activeSketchPlaneId,
                      draftStart,
                      activeSketchPlaneFrame,
                    ),
                  ),
                  new THREE.Vector3(...draftPreviewWorld),
                ]),
                new THREE.LineDashedMaterial({
                  color: themeColor("--color-tertiary-plane-edge", "#ffe784"),
                  transparent: true,
                  opacity: 0.65,
                  dashSize: 1,
                  gapSize: 0.6,
                }),
              );
              preview.computeLineDistances();
              previewLineRef.current = preview;
              sketchGroupRefValue.add(preview);
            } else {
              // Click 2 captured. Compute the 4 corners from
              // (draftStart, secondPoint, cursor-as-offset).
              const [p1x, p1y] = draftStart;
              const [p2x, p2y] = rectSecondPoint;
              const [p3x, p3y] = cursor;
              const dx = p2x - p1x;
              const dy = p2y - p1y;
              const edgeLen = Math.hypot(dx, dy);
              if (edgeLen >= 1e-9) {
                const nx = -dy / edgeLen;
                const ny = dx / edgeLen;
                const offset = nx * (p3x - p1x) + ny * (p3y - p1y);
                const cx = p1x + nx * offset;
                const cy = p1y + ny * offset;
                const c2x = p2x + nx * offset;
                const c2y = p2y + ny * offset;
                const corners: Array<[number, number]> = [
                  [p1x, p1y],
                  [p2x, p2y],
                  [c2x, c2y],
                  [cx, cy],
                  [p1x, p1y],
                ];
                const worldCorners = corners.map(
                  (corner) =>
                    new THREE.Vector3(
                      ...toWorldPoint(
                        activeSketchPlaneId,
                        corner,
                        activeSketchPlaneFrame,
                      ),
                    ),
                );
                const preview = new THREE.Line(
                  new THREE.BufferGeometry().setFromPoints(worldCorners),
                  makeDraftLineMaterial(),
                );
                if (sketchToolConstructionRef.current) {
                  preview.computeLineDistances();
                }
                previewLineRef.current = preview;
                sketchGroupRefValue.add(preview);
              }
            }
          } else {
            // corner_corner / center_point: full-rectangle draft.
            const [sx, sy] = draftStart;
            const [ex, ey] = draftPreviewLocal;
            const isCenterPoint = rectMode === "center_point";
            const corners: Array<[number, number]> = isCenterPoint
              ? [
                  [2 * sx - ex, 2 * sy - ey],
                  [ex, 2 * sy - ey],
                  [ex, ey],
                  [2 * sx - ex, ey],
                  [2 * sx - ex, 2 * sy - ey],
                ]
              : [
                  [sx, sy],
                  [ex, sy],
                  [ex, ey],
                  [sx, ey],
                  [sx, sy],
                ];
            const worldCorners = corners.map(
              (corner) =>
                new THREE.Vector3(
                  ...toWorldPoint(
                    activeSketchPlaneId,
                    corner,
                    activeSketchPlaneFrame,
                  ),
                ),
            );
            const preview = new THREE.Line(
              new THREE.BufferGeometry().setFromPoints(worldCorners),
              makeDraftLineMaterial(),
            );
            if (sketchToolConstructionRef.current) {
              preview.computeLineDistances();
            }
            previewLineRef.current = preview;
            sketchGroupRefValue.add(preview);
          }
        } else {
          const preview = new THREE.Line(
            new THREE.BufferGeometry().setFromPoints([
              new THREE.Vector3(
                ...toWorldPoint(
                  activeSketchPlaneId,
                  draftStart,
                  activeSketchPlaneFrame,
                ),
              ),
              new THREE.Vector3(...draftPreviewWorld),
            ]),
            makeDraftLineMaterial(),
          );
          if (sketchToolConstructionRef.current) {
            preview.computeLineDistances();
          }
          previewLineRef.current = preview;
          sketchGroupRefValue.add(preview);
        }
        return;
      }

      const hit = intersectSceneTargets(event);
      if (hit?.kind === "sketch_dimension" || hit?.kind === "sketch_entity") {
        setHoveredReference(null);
        setHoveredPrimitive(null);
        setHoveredSketchProfile(null);
        setHoveredSketchPoint(null);
        setHoveredSketchEntity(hit?.kind === "sketch_entity" ? hit.id : null);
        setHoveredFace(null);
        setHoveredEdge(null);
        setHoveredVertex(null);
        return;
      }
      setHoveredReference(hit?.kind === "reference" ? hit.id : null);
      setHoveredSketchProfile(hit?.kind === "sketch_profile" ? hit.id : null);
      setHoveredSketchPoint(hit?.kind === "sketch_point" ? hit.id : null);
      setHoveredSketchEntity(null);
      setHoveredFace(hit?.kind === "face" ? hit.id : null);
      // Edges and vertices are mutually exclusive with each other (the
      // raycaster prioritizes vertex over edge over face), so only one
      // of these will resolve to a real id at a time.
      setHoveredEdge(hit?.kind === "edge" ? hit.id : null);
      setHoveredVertex(hit?.kind === "vertex" ? hit.id : null);
      // Suppress primitive hover while a face under the same primitive is
      // hovered so the visual highlight reads as a face hover, not a body
      // hover.
      setHoveredPrimitive(hit?.kind === "primitive" && hit.id ? hit.id : null);
    }

    function handlePointerLeave() {
      pointerDown = null;
      if (moveGizmoDragRef.current) {
        moveGizmoDragRef.current = null;
        controls.enabled = true;
      }
      if (!dimensionLabelDragRef.current?.isPlacement) {
        dimensionLabelDragRef.current = null;
        controls.enabled = true;
      }
      (renderer.domElement as HTMLCanvasElement).style.cursor = "";
      setSketchSnapLabel(null);
      setConstraintPreview(null);
      setCrosshairPointer(null);
      setHoveredSketchProfile(null);
      setHoveredSketchPoint(null);
      setHoveredSketchEntity(null);
      if (!activeSketchPlaneId) {
        setHoveredReference(null);
        setHoveredPrimitive(null);
        setHoveredFace(null);
        setHoveredEdge(null);
        setHoveredVertex(null);
      }
      if (viewCubeGroupRef.current) {
        clearCubeHover(viewCubeGroupRef.current);
      }
      viewCubeHoveredRef.current = null;
    }

    function handlePointerUp(event: PointerEvent) {

      // --- Rectangle selection finalize ---
      if (selectionDragRef.current?.active) {
        const d = selectionDragRef.current;
        const w = Math.abs(d.currentX - d.startX);
        const h = Math.abs(d.currentY - d.startY);
        if (w > 3 || h > 3) {
          void performRectangleSelect(d, event.shiftKey);
        }
        selectionDragRef.current = null;
        setSelectionRect(null);
        controls.enabled = true;
        return;
      }

      lastPointerEventRef.current = event;
      if (event.button === 1) {
        controls.mouseButtons.MIDDLE = THREE.MOUSE.PAN;
        pointerDown = null;
        return;
      }

      if (event.button !== 0) {
        pointerDown = null;
        return;
      }
      if (renderer.domElement.hasPointerCapture(event.pointerId)) {
        renderer.domElement.releasePointerCapture(event.pointerId);
      }

      if (moveGizmoDragRef.current) {
        moveGizmoDragRef.current = null;
        controls.enabled = true;
        (renderer.domElement as HTMLCanvasElement).style.cursor = "";
        pointerDown = null;
        return;
      }

      const dimensionDrag = dimensionLabelDragRef.current;
      if (dimensionDrag) {
        if (dimensionDrag.isPlacement) {
          if (commitDimensionRelationPreview()) {
            setIsDimensionEditorOpen(false);
            pointerDown = null;
            return;
          }
          finishDimensionPlacement();
          setIsDimensionEditorOpen(false);
          // Fall through to entity handling so two-pick workflows
          // (angle, distance) can process the second click.
        }
        // finishDimensionPlacement nulls the ref; only clean up if
        // the drag is still active (non-placement label drag).
        if (dimensionLabelDragRef.current) {
          if (dimensionDrag.hasMoved) {
            persistDimensionLabelPosition(
              dimensionDrag.dimensionId,
              dimensionLabelPositionsRef.current[dimensionDrag.dimensionId],
            );
          }
          dimensionLabelDragRef.current = null;
          controls.enabled = true;
          (renderer.domElement as HTMLCanvasElement).style.cursor = "";
          pointerDown = null;
          if (!dimensionDrag.hasMoved) {
            if (dimensionDrag.hitPart === "label") {
              suppressNextDimensionEditorOpenRef.current = false;
              dimensionInputSelectionLockedRef.current = true;
              void selectSketchDimensionRef.current(dimensionDrag.dimensionId);
              setIsDimensionEditorOpen(true);
            } else {
              suppressNextDimensionEditorOpenRef.current = true;
              setIsDimensionEditorOpen(false);
              void selectSketchDimensionRef.current(dimensionDrag.dimensionId);
            }
          }
          return;
        }
      }

      // --- Endpoint drag finalize ---
      if (endpointDragRef.current) {
        const drag = endpointDragRef.current;
        const dx = event.clientX - drag.startClientX;
        const dy = event.clientY - drag.startClientY;

        if (Math.abs(dx) > 4 || Math.abs(dy) > 4) {
          // Committed drag — resolve final snapped position and update.
          const rawPoint = resolveSketchPlanePoint(
            event,
            renderer,
            camera,
            activeSketchPlaneIdRef.current,
            activeSketchPlaneFrameRef.current,
          );
          if (rawPoint) {
            const sketchPoint = resolveSnappedSketchPoint(rawPoint);
            // Diagnostic: log the owning line's data before commit.
            const params = sketchLinesRef.current;
            if (params) {
              const ownerLine = params.lines.find(
                (l) =>
                  l.start_point_id === drag.pointId ||
                  l.end_point_id === drag.pointId,
              );
              if (ownerLine) {
                const isStart =
                  ownerLine.start_point_id === drag.pointId;
                console.warn(
                  "[endpoint-drag] dragging",
                  isStart ? "start" : "end",
                  "of line",
                  ownerLine.line_id,
                );
                console.warn(
                  "[endpoint-drag]   fixed endpoint:",
                  isStart
                    ? [ownerLine.end_x, ownerLine.end_y]
                    : [ownerLine.start_x, ownerLine.start_y],
                );
                console.warn(
                  "[endpoint-drag]   dragged to:",
                  sketchPoint.local,
                );
                // Check for constraints / relations on this line.
                if (ownerLine.constraint) {
                  console.warn(
                    "[endpoint-drag]   line constraint:",
                    ownerLine.constraint,
                  );
                }
                if (params.line_relations) {
                  const rels = params.line_relations.filter(
                    (r) =>
                      r.first_line_id === ownerLine.line_id ||
                      r.second_line_id === ownerLine.line_id,
                  );
                  if (rels.length > 0) {
                    console.warn(
                      "[endpoint-drag]   line relations:",
                      rels.map((r) => r.kind),
                    );
                  }
                }
                // Check for dimensions on this line.
                if (params.dimensions) {
                  const dims = params.dimensions.filter(
                    (d) => d.entity_id === ownerLine.line_id,
                  );
                  if (dims.length > 0) {
                    console.warn(
                      "[endpoint-drag]   dimensions:",
                      dims.map(
                        (d) =>
                          `${d.kind}=${d.value} driven=${d.driven}`,
                      ),
                    );
                  }
                }
              }
            }
            void updateSketchPointRef.current(
              drag.pointId,
              sketchPoint.local[0],
              sketchPoint.local[1],
            );
          }
        } else {
          // Click (no significant movement) — fall through to the
          // existing click-to-select logic so the user can still
          // select the point normally.
          console.warn(
            "[endpoint-drag] no movement, falling through to click select:",
            drag.pointId,
          );
        }

        endpointDragRef.current = null;
        controls.enabled = true;
        (renderer.domElement as HTMLCanvasElement).style.cursor = "";
        setSketchSnapLabel(null);

        if (Math.abs(dx) > 4 || Math.abs(dy) > 4) {
          // Drag was committed — consume the event.
          pointerDown = null;
          return;
        }
        // Fall through: treat as a click.
      }

      // -- cube-area click ---------------------------------------------
      if (viewCubeDraggingRef.current) {
        viewCubeDraggingRef.current = false;
        const dragStart = viewCubeDragStartRef.current;
        viewCubeDragStartRef.current = null;

        // If it was a click (minimal drag), snap camera
        if (
          dragStart &&
          Math.abs(event.clientX - dragStart.x) <= 4 &&
          Math.abs(event.clientY - dragStart.y) <= 4
        ) {
          const cubeGroup = viewCubeGroupRef.current;
          const cubeCam = viewCubeCameraRef.current;
          const cubeRaycaster = viewCubeRaycasterRef.current;
          if (cubeGroup && cubeCam && cubeRaycaster) {
            const cubeDpr = renderer.getPixelRatio();
            const cubeCanvasRect = renderer.domElement.getBoundingClientRect();
            const canvasWidth = renderer.domElement.width;
            const canvasHeight = renderer.domElement.height;
            const rect = getCubeViewportRect(canvasWidth, canvasHeight, cubeDpr);
            const glX = (event.clientX - cubeCanvasRect.left) * cubeDpr;
            const glY = canvasHeight - (event.clientY - cubeCanvasRect.top) * cubeDpr;
            const ndcX = ((glX - rect.x) / rect.width) * 2 - 1;
            const ndcY = ((glY - rect.y) / rect.height) * 2 - 1;
            cubeRaycaster.setFromCamera(new THREE.Vector2(ndcX, ndcY), cubeCam);
            const hit = raycastViewCube(cubeRaycaster, cubeGroup);
            if (hit) {
              if (hit.type === "rotation_arrow") {
                rotateCameraAroundCurrentView(-hit.direction as -1 | 1);
                return;
              }
              const direction = getCubeHitTargetDirection(hit);
              const targetUp = getQuantizedCubeUp(direction, camera.up);
              const distance = camera.position.distanceTo(controls.target);
              const targetPos = controls.target.clone().add(
                direction.multiplyScalar(distance),
              );
              viewCubeAnimStartPosRef.current.copy(camera.position);
              viewCubeAnimTargetPosRef.current.copy(targetPos);
              viewCubeAnimStartUpRef.current.copy(camera.up).normalize();
              viewCubeAnimTargetUpRef.current.copy(targetUp);
              viewCubeAnimStartRef.current = performance.now();
              viewCubeAnimatingRef.current = true;
              // controls.enabled stays false (already set on pointerDown)
            } else {
              controls.enabled = true;
            }
          }
        }
        return;
      }

      if (!pointerDown) {
        return;
      }

      const deltaX = Math.abs(event.clientX - pointerDown.x);
      const deltaY = Math.abs(event.clientY - pointerDown.y);
      pointerDown = null;

      if (draftStartedOnPointerDownRef.current) {
        draftStartedOnPointerDownRef.current = false;

        if (deltaX > 4 || deltaY > 4) {
          const field = draftDimensionSessionRef.current?.activeField;
          if (field) {
            window.requestAnimationFrame(() => {
              draftDimensionInputRefs.current[field]?.focus();
              draftDimensionInputRefs.current[field]?.select();
            });
          }
        }
        return;
      }

      if (deltaX > 4 || deltaY > 4) {
        return;
      }

      if (activeSketchPlaneId) {
        const hit = intersectSceneTargets(event);
        const additiveSelection =
          event.shiftKey || event.ctrlKey || event.metaKey;
        if (
          activeSketchToolRef.current === "project" &&
          hit?.kind === "sketch_profile"
        ) {
          void selectSketchProfileRef.current(hit.id, false);
          return;
        }
        // Trim tool. Intercept entity clicks. Uses the 3D world-space
        // hit point from the raycaster, then projects it onto the sketch
        // plane frame to get consistent sketch-local coordinates that
        // match the core's line storage.
        if (
          activeSketchToolRef.current === "trim" &&
          hit?.kind === "sketch_entity" &&
          (hit.entityKind === "line" || hit.entityKind === "circle" || hit.entityKind === "arc") &&
          "worldPoint" in hit
        ) {
          const wp = hit.worldPoint as readonly [number, number, number];
          const frame = activeSketchPlaneFrameRef.current;
          // Use the same sketch-local convention as resolveSketchPlanePoint
          // so coordinates land in the same space as the line data.
          let localX: number, localY: number;
          if (frame) {
            const dx = wp[0] - frame.origin.x;
            const dy = wp[1] - frame.origin.y;
            const dz = wp[2] - frame.origin.z;
            localX = dx * frame.x_axis.x + dy * frame.x_axis.y + dz * frame.x_axis.z;
            localY = dx * frame.y_axis.x + dy * frame.y_axis.y + dz * frame.y_axis.z;
          } else if (activeSketchPlaneId === "ref-plane-xy") {
            localX = wp[0];
            localY = wp[2];
          } else if (activeSketchPlaneId === "ref-plane-yz") {
            localX = wp[1];
            localY = wp[2];
          } else {
            localX = wp[0];
            localY = wp[1];
          }

          console.log("[trim] hit_world=[", wp[0].toFixed(1), wp[1].toFixed(1), wp[2].toFixed(1),
                      "] local=[", localX.toFixed(1), localY.toFixed(1), "]");

          void trimSketchEntityRef.current?.(
            hit.id,
            localX,
            localY,
          );
          return;
        }

        if (activeSketchToolRef.current === "select") {
          // Mirror tool takes priority over the rest of the
          // selection / armed-constraint flow when one of its
          // slots is focused. We route line and circle hits to
          // the parent's mirror handler; everything else falls
          // through (so the user can still rotate the camera,
          // edit dimensions, etc. with the panel open).
          if (
            mirrorFocusedSlotRef.current &&
            hit?.kind === "sketch_entity" &&
            !hit.isProjected &&
            (hit.entityKind === "line" || hit.entityKind === "circle")
          ) {
            void mirrorEntityPickRef.current(hit.id, hit.entityKind);
            return;
          }

          if (
            armedSketchConstraintRef.current &&
            hit?.kind === "sketch_entity" &&
            !hit.isProjected &&
            hit.entityKind === "line"
          ) {
            void selectSketchEntityRef.current(hit.id, false);
            return;
          }

          if (hit?.kind === "sketch_point") {
            void pickSketchPointRef.current(
              hit.id,
              hit.pointKind,
              additiveSelection,
            );
            return;
          }

          if (hit?.kind === "sketch_dimension") {
            handleDimensionClick(hit.id);
            return;
          }

          if (hit?.kind === "sketch_constraint") {
            setSelectedConstraint({
              kind: hit.constraintKind,
              entityId: hit.entityId,
              relatedEntityId: hit.relatedEntityId,
            });
            // Flash the entity this constraint sits on
            paintSketchEntityMaterials();
            paintSketchPointMaterials();
            // Highlight will update via the useEffect below
            return;
          }

          if (
            inactiveSketchEntityPickEnabledRef.current &&
            hit?.kind === "sketch_entity" &&
            (hit.entityKind === "line" || hit.entityKind === "arc")
          ) {
            void selectSketchEntityRef.current(hit.id, false);
            return;
          }

          if (hit?.kind === "sketch_profile") {
            void selectSketchProfileRef.current(
              hit.id,
              event.shiftKey || event.ctrlKey || event.metaKey,
            );
            return;
          }

          if (hit?.kind === "sketch_entity") {
            void selectSketchEntityRef.current(hit.id, additiveSelection);
          }
          return;
        }

        // Dimension tool. Circle picks open the diameter editor.
        // Line picks are staged: the first line opens its length dim,
        // and a second different line creates/selects an angle dim.
        // Enter confirms the focused input; Escape cancels via the
        // global sketch key path.
        if (activeSketchToolRef.current === "dimension") {
          if (hit?.kind === "sketch_dimension") {
            dimensionToolFirstLineRef.current = null;
            setDimensionToolFirstLine(null);
            dimensionToolFirstPointRef.current = null;
            handleDimensionClick(hit.id);
            return;
          }
          // --- Regroup check ---
          // If a single-entity dimension was just created (pending) and the
          // user clicks a different entity, delete the pending dim and stage
          // the source entity for a two-entity/point dimension.
          if (
            pendingDimensionPlacementRef.current &&
            pendingDimSourceEntityIdRef.current
          ) {
            const pendingSourceId = pendingDimSourceEntityIdRef.current;
            let clickedEntityId: string | null = null;
            let clickedIsPoint = false;
            if (hit?.kind === "sketch_entity") {
              clickedEntityId = hit.id;
            } else if (hit?.kind === "sketch_point") {
              clickedIsPoint = true;
              const lm = hit.id.match(/^point-(line-\d+)/);
              if (lm) { clickedEntityId = lm[1]; }
              if (!clickedEntityId) {
                const cm = hit.id.match(/^point-(circle-\d+)/);
                if (cm) { clickedEntityId = cm[1]; }
              }
              if (!clickedEntityId) {
                const pm = hit.id.match(/^point-(polygon-\d+)/);
                if (pm) { clickedEntityId = pm[1]; }
              }
            }
            if (clickedEntityId && clickedEntityId !== pendingSourceId) {
              // Regroup: delete the hastily-created dimension
              const dimId = pendingDimensionIdRef.current;
              if (dimId) {
                void deleteSketchDimensionRef.current(dimId);
              }
              pendingDimensionIdRef.current = null;
              pendingDimSourceEntityIdRef.current = null;
              pendingDimensionPlacementRef.current = false;
              // Stage the source entity so the two-entity handler below
              // picks it up as the "first pick" in a two-entity flow.
              dimensionToolFirstLineRef.current = pendingSourceId;
              setDimensionToolFirstLine(pendingSourceId);
              // If the click was on a sketch point, route through the
              // point-distance path instead of entity handler.
              if (clickedIsPoint) {
                dimensionToolFirstPointRef.current = { id: hit!.id, x: 0, y: 0 };
              }
              // Fall through to the entity handler below which will see
              // firstEntityId != clickedEntityId and call
              // dimCreateAngleOrDistance / dimCreatePointDistance.
            }
          }
          // Point_distance mode: on the *second* click of a two-pick
          // sequence, restore the staged entity so the existing handler
          // below processes angle / parallel-distance.  First clicks
          // fall through untouched so the existing handler creates the
          // unary dimension (line length / circle radius) and stages the
          // entity for a possible follow-up pick.
          if (
            hit?.kind === "sketch_entity" || hit?.kind === "sketch_point"
          ) {
            let entityId: string | null = null;
            if (hit.kind === "sketch_entity") {
              entityId = hit.id;
            } else if (hit.kind === "sketch_point") {
              const match = hit.id.match(/^point-(line-\d+)/);
              if (match) entityId = match[1];
            }
            if (!entityId) {
              // Point didn't resolve to a known entity — for
              // circle-center points, try the circle pattern.
              if (hit?.kind === "sketch_point") {
                const circleMatch = hit.id.match(/^point-(circle-\d+)/);
                if (circleMatch) entityId = circleMatch[1];
              }
              if (!entityId) return;
            }
            const firstEntityId = dimensionToolFirstLineRef.current;
            if (firstEntityId) {
              if (firstEntityId === entityId) {
                dimensionToolFirstLineRef.current = null;
                setDimensionToolFirstLine(null);
                return;
              }
              dimensionToolFirstLineRef.current = firstEntityId;
              setDimensionToolFirstLine(firstEntityId);
              // Fall through to sketch_entity handler.
            }
            // First click with no staged entity: fall through to the
            // existing handler which creates the unary dimension and
            // stages the entity.
          }
          if (hit?.kind === "sketch_entity") {
            if (hit.isProjected) {
              return;
            }
            const firstEntityId = dimensionToolFirstLineRef.current;
            const firstPoint = dimensionToolFirstPointRef.current;
            if (firstEntityId && firstEntityId !== hit.id) {
              dimensionToolFirstLineRef.current = null;
              setDimensionToolFirstLine(null);
              // Delete the single-entity dimension created for the first
              // pick so it doesn't persist alongside the two-entity dim.
              // Dimension IDs carry the entity kind prefix twice
              // (e.g. dim-line-line-0, dim-circle-circle-1).
              const firstDimId =
                firstEntityId.startsWith("line-")
                  ? `dim-line-${firstEntityId}`
                  : firstEntityId.startsWith("circle-")
                    ? `dim-circle-${firstEntityId}`
                    : firstEntityId.startsWith("polygon-")
                      ? `dim-polygon-${firstEntityId}`
                      : null;
              if (firstDimId) {
                void deleteSketchDimensionRef.current(firstDimId);
              }
              // When the first pick was a point (line endpoint, circle
              // center, etc.), create a point-to-point distance to the
              // second entity's reference point instead of an
              // entity-to-entity distance.
              if (firstPoint) {
                dimensionToolFirstPointRef.current = null;
                if (hit.entityKind === "circle") {
                  dimCreatePointDistance(
                      firstPoint.id, `point-circle-${hit.id}-center`);
                  return;
                }
                if (hit.entityKind === "polygon") {
                  dimCreatePointDistance(
                      firstPoint.id, `point-polygon-${hit.id}-center`);
                  return;
                }
                // For line bodies, fall through to entity-to-entity
                // (e.g. line_line_distance or angle).
              }
              dimCreateAngleOrDistance(firstEntityId, hit.id);
              return;
            }
            // First click on this entity (or re-click on the staged one).
            // When a point was staged from the same line (user clicked
            // one endpoint, now re-clicks the line body), treat it as a
            // line-length dimension instead of creating a duplicate.
            if (firstPoint && hit.entityKind === "line") {
              const m = firstPoint.id.match(/^point-(line-\d+)/);
              if (m && m[1] === hit.id) {
                dimensionToolFirstLineRef.current = null;
                setDimensionToolFirstLine(null);
                dimensionToolFirstPointRef.current = null;
                dimCreateLine(hit.id);
                return;
              }
            }
            // Re-click on a staged circle with no radius dimension yet:
            // create the radius dimension now.
            if (firstPoint && firstEntityId === hit.id &&
                hit.entityKind === "circle") {
              const exists =
                sketchLinesRef.current?.dimensions.some(
                  (d) => d.dimension_id === `dim-circle-${hit.id}`,
                ) ?? false;
              if (!exists) {
                dimensionToolFirstLineRef.current = null;
                setDimensionToolFirstLine(null);
                dimensionToolFirstPointRef.current = null;
                dimCreateCircle(hit.id, "");
                return;
              }
            }
            // Create or select the unary dimension, stage for two-entity flow.
            if (hit.entityKind === "circle") {
              const exists =
                sketchLinesRef.current?.dimensions.some(
                  (d) => d.dimension_id === `dim-circle-${hit.id}`,
                ) ?? false;
              if (exists) {
                // Already has a radius dimension — select it and
                // stage for two-entity flow.
                dimSelectCircle(hit.id);
              } else {
                // First circle click creates the diameter dimension, then
                // stages the circle for relation previews while the label is
                // being placed.
                dimCreateCircle(hit.id, "");
              }
              return;
            }
            // Polygon
            if (hit.entityKind === "polygon") {
              const exists =
                sketchLinesRef.current?.dimensions.some(
                  (d) => d.dimension_id === `dim-polygon-${hit.id}`,
                ) ?? false;
              if (exists) {
                dimSelectPolygon(hit.id);
              } else {
                dimCreatePolygon(hit.id);
              }
              return;
            }
            // Line
            const exists =
              sketchLinesRef.current?.dimensions.some(
                (d) => d.dimension_id === `dim-line-${hit.id}`,
              ) ?? false;
            if (exists) {
              dimSelectLine(hit.id);
            } else {
              dimCreateLine(hit.id);
            }
            return;
          }
          // Sketch-point hit: route to point-to-point distance or entity
          // dimension based on whether a first point is already staged.
          // Without this, clicking near a shared chained-line endpoint
          // hits the point sphere before the line, and the dimension
          // tool silently ignores it.
          if (hit?.kind === "sketch_point") {
            const firstPoint = dimensionToolFirstPointRef.current;
            if (firstPoint && firstPoint.id !== hit.id) {
              // Two-point flow. If both points belong to the same
              // line (user clicked both endpoints), create a
              // line-length dimension. Otherwise point-to-point.
              const resolveLine = (pid: string) => {
                const m = pid.match(/^point-(line-\d+)/);
                return m ? m[1] : null;
              };
              const lineA = resolveLine(firstPoint.id);
              const lineB = resolveLine(hit.id);
              if (lineA && lineA === lineB) {
                dimensionToolFirstLineRef.current = null;
                setDimensionToolFirstLine(null);
                dimensionToolFirstPointRef.current = null;
                dimCreateLine(lineA);
                return;
              }
              dimensionToolFirstLineRef.current = null;
              setDimensionToolFirstLine(null);
              dimensionToolFirstPointRef.current = null;
              dimCreatePointDistance(firstPoint.id, hit.id);
              return;
            }
            // First point pick — stage it, then resolve to an entity
            // so the next click on an entity body works.
            dimensionToolFirstPointRef.current = {
              id: hit.id, x: 0, y: 0,
            };
            // Resolve point → entity for the unary-dimension path
            let resolvedId: string | null = null;
            const lm = hit.id.match(/^point-(line-\d+)/);
            if (lm) { resolvedId = lm[1]; }
            const cm = hit.id.match(/^point-(circle-\d+)/);
            if (cm) { resolvedId = cm[1]; }
            const pm = hit.id.match(/^point-(polygon-\d+)/);
            if (pm) { resolvedId = pm[1]; }
            if (!resolvedId) {
              dimensionToolFirstLineRef.current = null;
              setDimensionToolFirstLine(null);
              return;
            }
            // Stage the resolved entity so the next click can create
            // a two-entity dimension. Don't create a single-entity dim yet
            // — point clicks always stage and wait for the next action.
            dimensionToolFirstLineRef.current = resolvedId;
            setDimensionToolFirstLine(resolvedId);
            return;
          }
          // Click in empty space: accept pending dimension if one is open,
          // or cancel staging if a two-entity pick is in progress.
          if (pendingDimensionPlacementRef.current) {
            // Accept the current dimension as-is — it stays visible.
            // Clear pending refs so the regroup path doesn't fire on next click.
            pendingDimensionIdRef.current = null;
            pendingDimSourceEntityIdRef.current = null;
            pendingDimensionPlacementRef.current = false;
          }
          dimensionToolFirstLineRef.current = null;
          setDimensionToolFirstLine(null);
          dimensionToolFirstPointRef.current = null;
          return;
        }

        // Sketch fillet tool. Clicks must land on a sketch point that
        // is shared by exactly two non-construction sketch lines and
        // is not already filleted. Anything else is a no-op (the
        // user can switch back to Select if they want to inspect).
        // Eligibility is resolved against the *current* sketch
        // snapshot in `sketchLinesRef`, not the document store, so
        // it stays in sync with what the user just sees.
        if (activeSketchToolRef.current === "fillet") {
          const filletRawPoint = resolveSketchPlanePoint(
            event,
            renderer,
            camera,
            activeSketchPlaneId,
            activeSketchPlaneFrame,
          );
          if (!filletRawPoint) {
            return;
          }
          const filletSnapped = resolveSnappedSketchPoint(filletRawPoint);
          const params = sketchLinesRef.current;
          if (!params) {
            return;
          }

          // Find the sketch point id that matches the snapped click
          // location within `kCoincidentTolerance` (same fudge the
          // core uses). We don't rely on `endpointHostLineId` from
          // the snap candidate because a corner is by definition
          // shared by *two* lines and the snap only carries one
          // host id.
          const tolerance = 0.05;
          const cornerPoint = params.points.find(
            (point) =>
              Math.hypot(
                point.x - filletSnapped.local[0],
                point.y - filletSnapped.local[1],
              ) <= tolerance,
          );
          if (!cornerPoint) {
            return;
          }

          // Lines that reference this point as an endpoint and are
          // not construction-only (construction lines don't
          // contribute to closed loops, so filleting them would
          // produce nothing useful).
          const incidentLines = params.lines.filter(
            (line) =>
              !line.is_construction &&
              (line.start_point_id === cornerPoint.point_id ||
                line.end_point_id === cornerPoint.point_id),
          );
          if (incidentLines.length !== 2) {
            return;
          }

          // Reject corners that already host a fillet referencing
          // either of the same lines — same guard the core enforces
          // in `add_sketch_fillet`. Surfaced here too so the click
          // doesn't bother round-tripping a doomed command.
          const alreadyFilleted = (params.fillets ?? []).some((fillet) =>
            incidentLines.some(
              (line) =>
                (fillet.line_a_id === line.line_id ||
                  fillet.line_b_id === line.line_id) &&
                (fillet.trim_a_point_id === cornerPoint.point_id ||
                  fillet.trim_b_point_id === cornerPoint.point_id),
            ),
          );
          if (alreadyFilleted) {
            return;
          }

          // The session radius lives in App's panel state; the
          // viewport only signals which corner was picked. App
          // dispatches `add_sketch_fillet` with whatever radius
          // the panel is showing at click time.
          void addSketchFilletRef.current(
            cornerPoint.point_id,
            incidentLines[0].line_id,
            incidentLines[1].line_id,
          );
          return;
        }

        // Modal Project tool. Clicks must land on a body face,
        // edge, or vertex; we dispatch the matching selection
        // callback (App.tsx intercepts those while the tool is
        // active and turns them into `project_*_into_sketch`
        // commands). Empty-space clicks and sketch-entity clicks
        // are no-ops so the user can't accidentally start drafting
        // a line: per the user's explicit request, the only valid
        // action while Project is armed is "pick something to
        // project". The tool stays armed across clicks.
        if (activeSketchToolRef.current === "project") {
          if (hit?.kind === "vertex") {
            void selectVertexRef.current(hit.id, /*additive=*/ false);
            return;
          }
          if (hit?.kind === "edge") {
            void selectEdgeRef.current(hit.id, /*additive=*/ false);
            return;
          }
          if (hit?.kind === "face") {
            void selectFaceRef.current(hit.id);
            return;
          }
          // No body geometry under the cursor — swallow the click
          // so we don't drop into the line-draft branch below.
          return;
        }

        // Trim tool: only sketch-entity clicks (handled above)
        // produce a trim. Empty-space clicks and non-entity clicks
        // are no-ops so the tool stays armed.
        if (activeSketchToolRef.current === "trim") {
          return;
        }

        // In a draft tool (line / rectangle / circle), clicks on an
        // existing line / dimension MUST NOT select. They should
        // fall through to the plane-projection path so the snap
        // resolver can use the entity as a snap source (line body,
        // endpoint, midpoint). Selection is reserved for the select
        // tool. Dimensions in draft mode are simply ignored — the
        // user can press S to switch back to select if they want to
        // edit one.
        const rawPoint = resolveSketchPlanePoint(
          event,
          renderer,
          camera,
          activeSketchPlaneId,
          activeSketchPlaneFrame,
        );
        if (!rawPoint) {
          return;
        }
        const sketchPoint = resolveSnappedSketchPoint(rawPoint);
        setSketchSnapLabel(sketchPoint.snapLabel);

        if (!lineDraftStartRef.current) {
          lineDraftStartRef.current = sketchPoint.local;
          // Whole-line midpoint snaps (t==0.5) get a true
          // `midpoint_anchor`; sub-segment midpoints (t!=0.5,
          // e.g. quarter-line snaps after a sibling line has split
          // the host) get a parametric `point_line_anchor` instead
          // — same machinery as the line-body snap. The two
          // branches stay mutually exclusive: the snap resolver
          // can only return one host per click.
          const midpointHostLineId = sketchPoint.snapMidpointHostLineId;
          const midpointT = sketchPoint.snapMidpointT ?? null;
          const isWholeLineMidpoint =
            midpointHostLineId !== null &&
            midpointHostLineId !== undefined &&
            midpointT !== null &&
            Math.abs(midpointT - 0.5) < 1e-9;
          draftStartMidpointHostRef.current = isWholeLineMidpoint
            ? (midpointHostLineId ?? null)
            : null;
          // If the start snapped to an existing line's endpoint, arm
          // the perpendicular-foot snap for the rest of the draft.
          // (The midpoint and endpoint hosts are independent: a
          // single click can only be one or the other since they
          // come from distinct snap candidates.)
          draftStartEndpointHostRef.current =
            sketchPoint.snapEndpointHostLineId ?? null;
          // Line-body snap captures the host + parametric position
          // so the post-add effect can anchor the new line's start
          // point to the host once it lands. We funnel sub-segment
          // midpoint snaps through the same channel: they're just
          // discrete-t projections onto the line's body.
          if (
            !isWholeLineMidpoint &&
            midpointHostLineId &&
            midpointT !== null
          ) {
            draftStartLineBodyHostRef.current = {
              lineId: midpointHostLineId,
              t: midpointT,
            };
          } else {
            draftStartLineBodyHostRef.current =
              sketchPoint.snapLineBodyHostLineId &&
              typeof sketchPoint.snapLineBodyT === "number"
                ? {
                    lineId: sketchPoint.snapLineBodyHostLineId,
                    t: sketchPoint.snapLineBodyT,
                  }
                : null;
          }
          return;
        }

	        const [startX, startY] = lineDraftStartRef.current;
	        const committedEnd =
	          draftDimensionSessionRef.current &&
	          isDraftDimensionTool(activeSketchToolRef.current)
	            ? draftDimensionSessionRef.current.current
	            : sketchPoint.local;
	        // Prevent committing a degenerate zero-length line (e.g. when
	        // double-clicking at the same endpoint to break the chain).
	        if (Math.abs(committedEnd[0] - startX) < 0.01 &&
	            Math.abs(committedEnd[1] - startY) < 0.01) {
	          lineDraftStartRef.current = null;
	          clearDraftDimensionSession();
	          return;
	        }
	        clearPreviewLine();
	        clearPreviewCircle();
	        clearPreviewArc();
	        clearPreviewDimension();

        if (activeSketchToolRef.current === "arc") {
          // Three-click arc placement. The first click landed in the
          // `!lineDraftStartRef.current` branch and stored the
          // start point; the second click captures the end point and
          // returns; the third click reads the anchor and dispatches.
          if (!arcSecondPointRef.current) {
            arcSecondPointRef.current = sketchPoint.local;
            return;
          }
          const [secondX, secondY] = arcSecondPointRef.current;
          const [thirdX, thirdY] = sketchPoint.local;
          arcSecondPointRef.current = null;
          lineDraftStartRef.current = null;
          // Click order differs by mode. In `three_point` the user
          // clicks (start, end, point-on-arc), which lines up 1:1
          // with the IPC's (start, end, anchor) params. In
          // `center_start_end` the user clicks (center, start, end)
          // — center is the *third* IPC param, so we have to
          // permute. The preview rendering above already follows
          // the same per-mode interpretation; without the permute
          // here the committed arc lands on the opposite side from
          // what the preview showed because the core then treats
          // click 1 (center) as if it were the arc's start.
          const mode = arcToolModeRef.current;
          if (mode === "three_point") {
            void addSketchArcRef.current(
              startX,
              startY,
              secondX,
              secondY,
              thirdX,
              thirdY,
              mode,
              sketchToolConstructionRef.current,
            );
          } else {
            // center_start_end: IPC params are
            // (start=click2, end=click3, anchor=click1).
            void addSketchArcRef.current(
              secondX,
              secondY,
              thirdX,
              thirdY,
              startX,
              startY,
              mode,
              sketchToolConstructionRef.current,
            );
          }
          return;
        }

        if (activeSketchToolRef.current === "rectangle") {
          const mode = rectangleToolModeRef.current;
          // 3-point rectangle: click 1 = corner, click 2 = second corner
          // of first edge, click 3 = perpendicular offset point.
          if (mode === "three_point") {
            if (!rectSecondPointRef.current) {
              rectSecondPointRef.current = [committedEnd[0], committedEnd[1]];
              return;
            }
            const [secondX, secondY] = rectSecondPointRef.current;
            rectSecondPointRef.current = null;
            lineDraftStartRef.current = null;
            scheduleDimensionDeletion("rectangle");
            scheduleDraftDimensionExpressionUpdate("rectangle");
            clearDraftDimensionSession();
            suppressDimensionEditorAfterSketchCommit();
            const p1x = startX;
            const p1y = startY;
            const p2x = secondX;
            const p2y = secondY;
            const p3x = committedEnd[0];
            const p3y = committedEnd[1];
            const dx = p2x - p1x;
            const dy = p2y - p1y;
            const edgeLen = Math.hypot(dx, dy);
            if (edgeLen < 1e-9) return;
            const nx = -dy / edgeLen;
            const ny = dx / edgeLen;
            const offset = nx * (p3x - p1x) + ny * (p3y - p1y);
            const cx = p1x + nx * offset;
            const cy = p1y + ny * offset;
            const c2x = p2x + nx * offset;
            const c2y = p2y + ny * offset;
            const minX = Math.min(p1x, p2x, cx, c2x);
            const minY = Math.min(p1y, p2y, cy, c2y);
            const maxX = Math.max(p1x, p2x, cx, c2x);
            const maxY = Math.max(p1y, p2y, cy, c2y);
            void addSketchRectangleRef.current(
              minX,
              minY,
              maxX,
              maxY,
              sketchToolConstructionRef.current,
            );
            return;
          }
          lineDraftStartRef.current = null;
          scheduleDimensionDeletion("rectangle");
          scheduleDraftDimensionExpressionUpdate("rectangle");
          clearDraftDimensionSession();
          suppressDimensionEditorAfterSketchCommit();
          const rectStartX =
            mode === "center_point"
              ? 2 * startX - committedEnd[0]
              : startX;
          const rectStartY =
            mode === "center_point"
              ? 2 * startY - committedEnd[1]
              : startY;
          void addSketchRectangleRef.current(
            rectStartX,
            rectStartY,
            committedEnd[0],
            committedEnd[1],
            sketchToolConstructionRef.current,
          );
          return;
        }

        if (activeSketchToolRef.current === "circle") {
          const circleMode = circleToolModeRef.current;
          // 3-point circle: click 1 = point on circle, click 2 = point on
          // circle, click 3 = third point on circle → compute circumcenter.
          if (circleMode === "three_point") {
            if (!circleSecondPointRef.current) {
              circleSecondPointRef.current = [committedEnd[0], committedEnd[1]];
              return;
            }
            const [p2x, p2y] = circleSecondPointRef.current;
            circleSecondPointRef.current = null;
            lineDraftStartRef.current = null;
            scheduleDimensionDeletion("circle");
            scheduleDraftDimensionExpressionUpdate("circle");
            clearDraftDimensionSession();
            suppressDimensionEditorAfterSketchCommit();
            const p1x = startX;
            const p1y = startY;
            const p3x = committedEnd[0];
            const p3y = committedEnd[1];
            // Circumcenter from three points using perpendicular bisectors
            const d = 2 * (p1x * (p2y - p3y) + p2x * (p3y - p1y) + p3x * (p1y - p2y));
            if (Math.abs(d) < 1e-9) return; // collinear
            const ux = ((p1x*p1x + p1y*p1y)*(p2y - p3y) + (p2x*p2x + p2y*p2y)*(p3y - p1y) + (p3x*p3x + p3y*p3y)*(p1y - p2y)) / d;
            const uy = ((p1x*p1x + p1y*p1y)*(p3x - p2x) + (p2x*p2x + p2y*p2y)*(p1x - p3x) + (p3x*p3x + p3y*p3y)*(p2x - p1x)) / d;
            const radius = Math.hypot(ux - p1x, uy - p1y);
            void addSketchCircleRef.current(
              ux,
              uy,
              radius,
              sketchToolConstructionRef.current,
            );
            return;
          }
          // Tangent modes: reserved for future core support.
          if (circleMode === "tangent_two_lines" || circleMode === "tangent_three_lines") {
            return;
          }
          lineDraftStartRef.current = null;
          pendingCircleDimensionPlacementRef.current = {
            fromCircleCount:
              sketchFeature?.sketch_parameters?.circles.length ?? 0,
            center: [startX, startY],
            end: committedEnd,
          };
          scheduleDimensionDeletion("circle");
          scheduleDraftDimensionExpressionUpdate("circle");
          clearDraftDimensionSession();
          suppressDimensionEditorAfterSketchCommit();
          let cx = startX;
          let cy = startY;
          let radius = distanceBetweenPoints(
            [startX, startY],
            committedEnd,
          );
          if (circleMode === "two_point") {
            cx = (startX + committedEnd[0]) / 2;
            cy = (startY + committedEnd[1]) / 2;
            radius = distanceBetweenPoints(
              [startX, startY],
              committedEnd,
            ) / 2;
          }
          void addSketchCircleRef.current(
            cx,
            cy,
            radius,
            sketchToolConstructionRef.current,
          );
          return;
        }

        if (activeSketchToolRef.current === "polygon") {
          lineDraftStartRef.current = null;
          scheduleDimensionDeletion("polygon");
          scheduleDraftDimensionExpressionUpdate("polygon");
          clearDraftDimensionSession();
          suppressDimensionEditorAfterSketchCommit();
          void addSketchPolygonRef.current(
            polygonSidesRef.current,
            polygonToolModeRef.current,
            startX,
            startY,
            committedEnd[0],
            committedEnd[1],
            sketchToolConstructionRef.current,
          );
          return;
        }

        // Capture both endpoints' midpoint hosts before the draft
        // state advances, so the post-add effect can attach anchors
        // to the just-created line. The baseline line count anchors
        // the effect's match: it will fire only when the sketch's
        // line count grows past `fromLineCount` by 1. Whole-line
        // midpoints (t==0.5) → midpoint_anchor; sub-segment
        // midpoints → point_line_anchor (handled below as part of
        // the line-body anchor path).
        const startHostLineId = draftStartMidpointHostRef.current;
        const endMidpointT = sketchPoint.snapMidpointT ?? null;
        const endIsWholeLineMidpoint =
          sketchPoint.snapMidpointHostLineId &&
          endMidpointT !== null &&
          Math.abs(endMidpointT - 0.5) < 1e-9;
        const endHostLineId = endIsWholeLineMidpoint
          ? (sketchPoint.snapMidpointHostLineId ?? null)
          : null;
        if (startHostLineId || endHostLineId) {
          pendingMidpointAnchorRef.current = {
            fromLineCount: sketchLineCountRef.current,
            startHostLineId,
            endHostLineId,
          };
        }

        // Capture pending perpendicular constraint when the cursor
        // committed on the perpendicular ray of the start's host
        // line. The post-add effect dispatches the constraint once
        // the new line lands.
        const perpHostLineId = sketchPoint.snapPerpendicularHostLineId;
        if (perpHostLineId) {
          pendingPerpendicularConstraintRef.current = {
            fromLineCount: sketchLineCountRef.current,
            hostLineId: perpHostLineId,
          };
        }

        // Capture pending point-on-line anchor for either side that
        // landed on a line body OR a sub-segment midpoint. The
        // start-side host was stashed at the previous click (or
        // chained from the last commit); the end-side comes
        // straight from the current snap result. Sub-segment
        // midpoints (t!=0.5) ride this same channel so they
        // commit as parametric anchors.
        const endIsSubSegmentMidpoint =
          sketchPoint.snapMidpointHostLineId &&
          endMidpointT !== null &&
          !endIsWholeLineMidpoint;
        const endLineBodyHost = endIsSubSegmentMidpoint
          ? {
              lineId: sketchPoint.snapMidpointHostLineId as string,
              t: endMidpointT as number,
            }
          : sketchPoint.snapLineBodyHostLineId &&
              typeof sketchPoint.snapLineBodyT === "number"
            ? {
                lineId: sketchPoint.snapLineBodyHostLineId,
                t: sketchPoint.snapLineBodyT,
              }
            : null;
        const startLineBodyHost = draftStartLineBodyHostRef.current;
        if (startLineBodyHost || endLineBodyHost) {
          pendingPointLineAnchorRef.current = {
            fromLineCount: sketchLineCountRef.current,
            startHost: startLineBodyHost,
            endHost: endLineBodyHost,
          };
        }

        // Capture pending axis lock so the post-add effect can
        // dispatch a sticky `set_sketch_line_constraint` for the new
        // line. Skipped when the line also has a perpendicular host
        // — the solver rejects the combination.
        if (sketchPoint.snapAxisLock && !perpHostLineId) {
          pendingAxisConstraintRef.current = {
            fromLineCount: sketchLineCountRef.current,
            kind: sketchPoint.snapAxisLock,
          };
        }

        // Capture pending tangent relation when the cursor snapped
        // onto a circle's tangent point. Skipped when a perpendicular
        // host is also pending — they conflict on the line's end.
        if (sketchPoint.snapTangentCircleId && !perpHostLineId) {
          pendingTangentConstraintRef.current = {
            fromLineCount: sketchLineCountRef.current,
            circleId: sketchPoint.snapTangentCircleId,
          };
        }

        // The line tool keeps drafting from the just-clicked end so
        // the user can chain segments. When chainBreakRequested is set
        // (double-click detected) instead clear the start so the next
        // click begins a fresh independent line.
        if (chainBreakRequestedRef.current) {
          chainBreakRequestedRef.current = false;
          lineDraftStartRef.current = null;
          previousLineAngleRef.current = null;
          scheduleDraftDimensionExpressionUpdate("line");
          clearDraftDimensionSession();
        } else {
          lineDraftStartRef.current = committedEnd;
          // Store the 2D sketch angle of the committed segment so
          // the next chained line's angle arc references it instead of
          // the horizontal axis.
          const pdx = committedEnd[0] - startX;
          const pdy = committedEnd[1] - startY;
          if (Math.hypot(pdx, pdy) > 1e-6) {
            previousLineAngleRef.current = Math.atan2(pdy, pdx);
          }
          draftStartMidpointHostRef.current = endHostLineId;
          // Reset the perpendicular host: a fresh draft segment starts
          // from the just-clicked end. Only set it again if that end
          // happened to itself snap to an existing line's endpoint.
          draftStartEndpointHostRef.current =
            sketchPoint.snapEndpointHostLineId ?? null;
          // Same chaining for the line-body host: the next draft
          // segment's start is the end we just committed.
          draftStartLineBodyHostRef.current = endLineBodyHost;
          Object.values(draftDimensionInputRefs.current).forEach((input) => {
            input?.blur();
          });
          suppressDimensionEditorAfterSketchCommit();
          // Capture the old session's lockedFields before creating the
          // new chained session, so we know whether the user typed.
          const oldSession = draftDimensionSessionRef.current;
          scheduleDraftDimensionExpressionUpdate("line");
          const nextLineSession = createDraftDimensionSession(
            "line",
            committedEnd,
            committedEnd,
          );
          // Clear previous line's scene object so the next line gets a fresh one
          clearDraftDimGroup();
          draftDimensionSessionRef.current = nextLineSession;
          setDraftDimensionSession(nextLineSession);
          focusDraftField(nextLineSession.activeField);
          scheduleDimensionDeletion("line", oldSession);
        }
        void addSketchLineRef.current(
          startX,
          startY,
          committedEnd[0],
          committedEnd[1],
          sketchToolConstructionRef.current,
        );
        return;
      }

      if (
        inactiveSketchEntityPickEnabledRef.current &&
        pickInactiveSketchLineRef.current
      ) {
        const sketchLineId = pickVisibleSketchLineScreenSpace(event, 16);
        if (sketchLineId) {
          void pickInactiveSketchLineRef.current(sketchLineId);
          return;
        }
      }

      const hit = intersectSceneTargets(event);
      if (
        inactiveSketchEntityPickEnabledRef.current &&
        hit?.kind === "sketch_entity" &&
        (hit.entityKind === "line" || hit.entityKind === "arc")
      ) {
        void selectSketchEntityRef.current(hit.id, false);
        return;
      }
      if (hit?.kind === "sketch_profile") {
        // Profiles are pickable outside sketch mode so the user can
        // run Extrude on a closed profile without re-entering its
        // sketch (contextual modeling). Selection is a no-op on the core's
        // body picking path; the floating Extrude action consumes
        // `selected_sketch_profile_id` directly.
        void selectSketchProfileRef.current(
          hit.id,
          event.shiftKey || event.ctrlKey || event.metaKey,
        );
        return;
      }

      if (hit?.kind === "reference") {
        void selectReferenceRef.current(hit.id);
        return;
      }

      // Shift, Ctrl (Win/Linux) and Cmd (mac) all toggle the picked
      // entity into the existing multi-select set; plain click
      // replaces. We accept all three so the additive gesture matches
      // each OS's native file-manager / list conventions without
      // forcing the user to learn a tool-specific modifier.
      const additive = event.shiftKey || event.ctrlKey || event.metaKey;

      if (hit?.kind === "vertex") {
        void selectVertexRef.current(hit.id, additive);
        return;
      }

      if (hit?.kind === "edge") {
        void selectEdgeRef.current(hit.id, additive);
        return;
      }

      if (hit?.kind === "face") {
        void selectFaceRef.current(hit.id);
        return;
      }

      if (hit?.kind === "primitive") {
        void selectPrimitiveRef.current(hit.id);
      }
    }

    function handleContextMenu(event: MouseEvent) {
      event.preventDefault();

      // Use the ref so right-clicks after a batch select see the
      // latest document state even when the handler closure is stale.
      const doc = documentRef.current;

      if (activeSketchPlaneId) {
        const hit = intersectSceneTargets(event as PointerEvent);
        if (
          hit?.kind !== "sketch_entity" &&
          hit?.kind !== "sketch_point" &&
          hit?.kind !== "sketch_profile" &&
          hit?.kind !== "sketch_dimension" &&
          hit?.kind !== "sketch_constraint"
        ) {
          // Check if there's an active selection (from rectangle select or clicks)
          const selEntityIds = [
            ...(doc?.selected_sketch_entity_ids ?? []),
            ...(doc?.selected_sketch_entity_id
              ? [doc.selected_sketch_entity_id]
              : []),
          ];
          const selPointIds = [
            ...(doc?.selected_sketch_point_ids ?? []),
            ...(doc?.selected_sketch_point_id
              ? [doc.selected_sketch_point_id]
              : []),
          ];
          const hasSelection = selEntityIds.length > 0 || selPointIds.length > 0;
          if (hasSelection) {
            const rect = renderer.domElement.getBoundingClientRect();
            setContextMenu({
              x: event.clientX - rect.left,
              y: event.clientY - rect.top,
              referenceId: null,
              faceId: null,
              sketchDeleteSelection: {
                entityIds: [...new Set(selEntityIds)],
                pointIds: [...new Set(selPointIds)],
                profileIds: doc?.selected_sketch_profile_ids ?? [],
              },
            });
            return;
          }
          setContextMenu(null);
          return;
        }

        // Right-click on a sketch constraint: show Delete context menu
        if (hit?.kind === "sketch_constraint") {
          const rect = renderer.domElement.getBoundingClientRect();
          setContextMenu({
            x: event.clientX - rect.left,
            y: event.clientY - rect.top,
            referenceId: null,
            faceId: null,
            constraintKind: hit.constraintKind,
            constraintEntityId: hit.entityId,
            constraintRelatedEntityId: hit.relatedEntityId,
          });
          setSelectedConstraint({
            kind: hit.constraintKind,
            entityId: hit.entityId,
            relatedEntityId: hit.relatedEntityId,
          });
          return;
        }

        // Right-click on a sketch dimension: show Delete context menu
        if (hit?.kind === "sketch_dimension") {
          const rect = renderer.domElement.getBoundingClientRect();
          setContextMenu({
            x: event.clientX - rect.left,
            y: event.clientY - rect.top,
            referenceId: null,
            faceId: null,
            dimensionId: hit.id,
          });
          return;
        }

        const selectedEntityIds = [
          ...(doc?.selected_sketch_entity_ids ?? []),
          ...(doc?.selected_sketch_entity_id
            ? [doc.selected_sketch_entity_id]
            : []),
        ];
        const selectedPointIds = [
          ...(doc?.selected_sketch_point_ids ?? []),
          ...(doc?.selected_sketch_point_id
            ? [doc.selected_sketch_point_id]
            : []),
        ];
        const selectedProfileIds = [
          ...(doc?.selected_sketch_profile_ids ?? []),
          ...(doc?.selected_sketch_profile_id
            ? [doc.selected_sketch_profile_id]
            : []),
        ];
        const currentSelection = {
          entityIds: [...new Set(selectedEntityIds)],
          pointIds: [...new Set(selectedPointIds)],
          profileIds: [...new Set(selectedProfileIds)],
        };
        const clickedSelection =
          hit.kind === "sketch_entity"
            ? { entityIds: [hit.id], pointIds: [], profileIds: [] }
            : hit.kind === "sketch_point"
              ? { entityIds: [], pointIds: [hit.id], profileIds: [] }
              : { entityIds: [], pointIds: [], profileIds: [hit.id] };
        const clickedIsSelected =
          (hit.kind === "sketch_entity" &&
            currentSelection.entityIds.includes(hit.id)) ||
          (hit.kind === "sketch_point" &&
            currentSelection.pointIds.includes(hit.id)) ||
          (hit.kind === "sketch_profile" &&
            currentSelection.profileIds.includes(hit.id));
        const selection =
          clickedIsSelected &&
          (currentSelection.entityIds.length > 0 ||
            currentSelection.pointIds.length > 0 ||
            currentSelection.profileIds.length > 0)
            ? currentSelection
            : clickedSelection;
        const rect = renderer.domElement.getBoundingClientRect();
        setContextMenu({
          x: event.clientX - rect.left,
          y: event.clientY - rect.top,
          referenceId: null,
          faceId: null,
          sketchDeleteSelection: selection,
        });
        return;
      }

      function bodyIdFromTopologyId(id: string | null | undefined) {
        if (!id) {
          return null;
        }
        for (const marker of [":face:", ":edge:", ":vertex:"]) {
          const markerIndex = id.indexOf(marker);
          if (markerIndex > 0) {
            return id.slice(0, markerIndex);
          }
        }
        return null;
      }

      const hit = intersectSceneTargets(event as PointerEvent);
      if (
        hit?.kind !== "reference" &&
        hit?.kind !== "face" &&
        hit?.kind !== "primitive" &&
        hit?.kind !== "edge" &&
        hit?.kind !== "vertex"
      ) {
        setContextMenu(null);
        return;
      }

      const rect = renderer.domElement.getBoundingClientRect();
      const solidFace =
        hit.kind === "face"
          ? sceneDataRef.current?.solidFaces.find((face) => face.faceId === hit.id)
          : null;
      const bodyId =
        hit.kind === "primitive"
          ? hit.id
          : hit.kind === "face"
            ? (solidFace?.ownerId ?? bodyIdFromTopologyId(hit.id))
            : hit.kind === "edge" || hit.kind === "vertex"
              ? bodyIdFromTopologyId(hit.id)
              : null;
      setContextMenu({
        x: event.clientX - rect.left,
        y: event.clientY - rect.top,
        referenceId: hit.kind === "reference" ? hit.id : null,
        faceId: hit.kind === "face" ? hit.id : null,
        bodyId,
        sketchDeleteSelection: null,
      });
    }

    const resizeObserver = new ResizeObserver(() => {
      resizeRenderer();
      render();
    });

    resizeObserver.observe(host);
    function handleDoubleClick(event: MouseEvent) {
      if (activeSketchPlaneId) {
        return;
      }

      const hit = intersectSceneTargets(event as PointerEvent);
      if (hit?.kind !== "face") {
        return;
      }

      const solidFace = sceneDataRef.current?.solidFaces.find(
        (face) => face.faceId === hit.id,
      );
      if (!solidFace) {
        return;
      }

      void selectFaceRef.current(solidFace.faceId);
      void startSketchOnFaceRef.current(solidFace.faceId, solidFace.planeFrame);
    }

    renderer.domElement.addEventListener("pointerdown", handlePointerDown);
    renderer.domElement.addEventListener("pointermove", handlePointerMove);
    renderer.domElement.addEventListener("pointerleave", handlePointerLeave);
    renderer.domElement.addEventListener("pointerup", handlePointerUp);
    renderer.domElement.addEventListener("contextmenu", handleContextMenu);
    renderer.domElement.addEventListener("dblclick", handleDoubleClick);
    renderer.domElement.addEventListener("wheel", handleWheel, {
      passive: false,
    });
    resizeRenderer();

    const animate = () => {
      render();
      frameId = window.requestAnimationFrame(animate);
    };

    frameId = window.requestAnimationFrame(animate);

    return () => {
      onSnapshotCaptureReady?.(null);
      window.cancelAnimationFrame(frameId);
      if (pendingMoveGizmoFrameRef.current !== null) {
        window.cancelAnimationFrame(pendingMoveGizmoFrameRef.current);
        pendingMoveGizmoFrameRef.current = null;
      }
      pendingMoveGizmoParametersRef.current = null;
      resizeObserver.disconnect();
      renderer.domElement.removeEventListener("pointerdown", handlePointerDown);
      renderer.domElement.removeEventListener("pointermove", handlePointerMove);
      renderer.domElement.removeEventListener(
        "pointerleave",
        handlePointerLeave,
      );
      renderer.domElement.removeEventListener("pointerup", handlePointerUp);
      renderer.domElement.removeEventListener("contextmenu", handleContextMenu);
      renderer.domElement.removeEventListener("dblclick", handleDoubleClick);
      renderer.domElement.removeEventListener("wheel", handleWheel);
      controls.dispose();
      disposeGroup(contentGroup);
      disposeGroup(referenceGroup);
      disposeGroup(sketchGroup);
      if (viewCubeGroupRef.current) {
        disposeViewCubeGroup(viewCubeGroupRef.current);
        viewCubeGroupRef.current = null;
      }
      viewCubeSceneRef.current = null;
      viewCubeCameraRef.current = null;
      viewCubeRaycasterRef.current = null;
      renderer.dispose();
      disposeDynamicGrid(worldGridRef.current);
      disposeDynamicGrid(sketchGridRef.current);
      rendererRef.current = null;
      sceneRef.current = null;
      cameraRef.current = null;
      controlsRef.current = null;
      contentGroupRef.current = null;
      referenceGroupRef.current = null;
      sketchGroupRef.current = null;
      primitiveVisualsRef.current.clear();
      primitiveStatesRef.current.clear();
      referencePlaneVisualsRef.current.clear();
      referencePlaneStatesRef.current.clear();
      solidFaceVisualsRef.current.clear();
      solidFaceStatesRef.current.clear();
      sketchProfileVisualsRef.current.clear();
      sketchProfileStatesRef.current.clear();
      referencePlaneMeshesRef.current = [];
      sketchEntityObjectsRef.current = [];
      sketchEntityObjectByIdRef.current.clear();
      sketchDimensionObjectsRef.current = [];
      sketchConstraintObjectsRef.current = [];
      sketchPointObjectsRef.current = [];
      sketchPointObjectByIdRef.current.clear();
      sketchProfileObjectsRef.current = [];
      meshesRef.current = [];
      faceMeshesRef.current = [];
      edgeLineObjectsRef.current = [];
      vertexObjectsRef.current = [];
      cutPreviewObjectsRef.current = [];
      moveGizmoObjectsRef.current = [];
      moveGizmoDragRef.current = null;
      worldGridRef.current = null;
      sketchGridRef.current = null;
      previewLineRef.current = null;
      previewCircleRef.current = null;
      previewArcRef.current = null;
      lineDraftStartRef.current = null;
      lastGeometryKeyRef.current = "";
    };
  }, [activeSketchPlaneId]);

  useEffect(() => {
    const scene = sceneRef.current;
    const camera = cameraRef.current;
    const controls = controlsRef.current;
    const contentGroup = contentGroupRef.current;
    const referenceGroup = referenceGroupRef.current;
    const sketchGroup = sketchGroupRef.current;

    if (
      !scene ||
      !camera ||
      !controls ||
      !contentGroup ||
      !referenceGroup ||
      !sketchGroup
    ) {
      return;
    }

    disposeGroup(contentGroup);
    disposeGroup(referenceGroup);
    disposeGroup(sketchGroup);
    primitiveVisualsRef.current.clear();
    primitiveStatesRef.current.clear();
    referencePlaneVisualsRef.current.clear();
    referencePlaneStatesRef.current.clear();
    solidFaceVisualsRef.current.clear();
    solidFaceStatesRef.current.clear();
    sketchProfileVisualsRef.current.clear();
    sketchProfileStatesRef.current.clear();
    referencePlaneMeshesRef.current = [];
    sketchEntityObjectsRef.current = [];
    sketchDimensionObjectsRef.current = [];
    sketchConstraintObjectsRef.current = [];
    sketchPointObjectsRef.current = [];
    sketchProfileObjectsRef.current = [];
    meshesRef.current = [];
    faceMeshesRef.current = [];
    edgeLineObjectsRef.current = [];
    vertexObjectsRef.current = [];
    cutPreviewObjectsRef.current = [];
    moveGizmoObjectsRef.current = [];
    // Hovered ids reference disposed THREE objects after a rebuild;
    // null them out so the next pointermove cleanly re-applies hover.
    hoveredEdgeIdRef.current = null;
    hoveredVertexIdRef.current = null;
    hoveredSketchEntityIdRef.current = null;
    hoveredSketchPointIdRef.current = null;
    previewLineRef.current = null;
    previewCircleRef.current = null;
    previewArcRef.current = null;

    if (!sceneData) {
      lastGeometryKeyRef.current = "";
      return;
    }

    for (const primitive of sceneData.primitives) {
      const object = buildPrimitiveObject(primitive);
      meshesRef.current.push(object.mesh);
      primitiveVisualsRef.current.set(primitive.primitiveId, object.visual);
      primitiveStatesRef.current.set(primitive.primitiveId, {
        isSelected: primitive.isSelected,
        isHovered: false,
      });
      contentGroup.add(object.mesh);
      contentGroup.add(object.edges);
    }

    for (const reference of sceneData.references) {
      if (reference.kind === "reference_plane") {
        if (!showReferencePlanes) {
          continue;
        }

        const object = buildReferencePlaneObject(reference);
        referencePlaneMeshesRef.current.push(object.mesh);
        referencePlaneVisualsRef.current.set(
          reference.referenceId,
          object.visual,
        );
        referencePlaneStatesRef.current.set(reference.referenceId, {
          isSelected: reference.isSelected,
          isHovered: false,
          isActiveSketchPlane: reference.isActiveSketchPlane,
        });
        referenceGroup.add(object.mesh);
        referenceGroup.add(object.edges);
        continue;
      }

      if (reference.kind === "reference_axis") {
        const axisObject = buildReferenceAxisObject(reference);
        referenceGroup.add(axisObject.line);
      } else if (reference.kind === "reference_point") {
        const pointObject = buildReferencePointObject(reference);
        referenceGroup.add(pointObject.mesh);
      } else if (reference.kind === "reference_helix") {
        const helixObject = buildReferenceHelixObject(reference);
        referenceGroup.add(helixObject.line);
      }
    }

    for (const face of sceneData.solidFaces) {
      const faceObject = buildSolidFaceObject(face);
      faceMeshesRef.current.push(faceObject.mesh);
      solidFaceVisualsRef.current.set(face.faceId, faceObject.visual);
      solidFaceStatesRef.current.set(face.faceId, {
        isSelected: face.isSelected,
        isHovered: false,
      });
      contentGroup.add(faceObject.mesh);
    }

    for (const edge of sceneData.edges) {
      const edgeLine = buildSceneEdgeObject(edge);
      edgeLineObjectsRef.current.push(edgeLine);
      contentGroup.add(edgeLine);
    }

    for (const preview of sceneData.cutPreviews) {
      const cutPreviewMesh = buildCutPreviewObject(preview);
      cutPreviewObjectsRef.current.push(cutPreviewMesh);
      contentGroup.add(cutPreviewMesh);
    }

    if (moveGizmo && !moveGizmo.disabled) {
      const object = buildMoveGizmoObject(moveGizmo);
      moveGizmoObjectsRef.current = object.pickables;
      contentGroup.add(object.group);
    }

    for (const sketchLine of sceneData.sketchLines) {
      const sketchLineObject = buildSketchLineObject(sketchLine);
      sketchLineObject.userData.isSelected = sketchLine.isSelected;
      sketchEntityObjectsRef.current.push(sketchLineObject);
      sketchEntityObjectByIdRef.current.set(
        sketchLine.lineId,
        sketchLineObject,
      );
      sketchGroup.add(sketchLineObject);
    }

    for (const sketchCircle of sceneData.sketchCircles) {
      const frame =
        sketchCircle.planeFrame ??
        (activeSketchPlaneId &&
        sketchCircle.planeId === activeSketchPlaneId &&
        activeSketchPlaneFrame
          ? activeSketchPlaneFrame
          : null);
      const sketchCircleObject = buildSketchCircleObject(sketchCircle, frame);
      sketchCircleObject.userData.isSelected = sketchCircle.isSelected;
      sketchEntityObjectsRef.current.push(sketchCircleObject);
      sketchEntityObjectByIdRef.current.set(
        sketchCircle.circleId,
        sketchCircleObject,
      );
      sketchGroup.add(sketchCircleObject);
    }

    for (const sketchPolygon of sceneData.sketchPolygons) {
      const sketchPolygonObject = buildSketchPolygonObject(sketchPolygon);
      sketchPolygonObject.userData.isSelected = sketchPolygon.isSelected;
      sketchEntityObjectsRef.current.push(sketchPolygonObject);
      sketchEntityObjectByIdRef.current.set(
        sketchPolygon.polygonId,
        sketchPolygonObject,
      );
      sketchGroup.add(sketchPolygonObject);
    }

    for (const sketchArc of sceneData.sketchArcs) {
      const frame =
        sketchArc.planeFrame ??
        (activeSketchPlaneId &&
        sketchArc.planeId === activeSketchPlaneId &&
        activeSketchPlaneFrame
          ? activeSketchPlaneFrame
          : null);
      const sketchArcObject = buildSketchArcObject(sketchArc, frame);
      sketchArcObject.userData.isSelected = sketchArc.isSelected;
      sketchEntityObjectsRef.current.push(sketchArcObject);
      sketchEntityObjectByIdRef.current.set(sketchArc.arcId, sketchArcObject);
      sketchGroup.add(sketchArcObject);
    }

    for (const sketchDimension of displayedSketchDimensions) {
      const sketchDimensionObject = buildSketchDimensionObject(
        sketchDimension,
        config.displayUnits,
      );
      if (
        hiddenRelationPreviewDimensionIdsRef.current.has(
          sketchDimension.dimensionId,
        )
      ) {
        sketchDimensionObject.line.visible = false;
        sketchDimensionObject.label.visible = false;
      }
      sketchDimensionObjectsRef.current.push(sketchDimensionObject.line);
      sketchDimensionObjectsRef.current.push(sketchDimensionObject.label);
      sketchGroup.add(sketchDimensionObject.line);
      sketchGroup.add(sketchDimensionObject.label);
    }

    for (const sketchConstraint of sceneData.sketchConstraints) {
      if (sketchConstraint.kind === "fixed") {
        continue;
      }
      const sketchConstraintObject =
        buildSketchConstraintObject(sketchConstraint);
      sketchConstraintObjectsRef.current.push(sketchConstraintObject);
      sketchGroup.add(sketchConstraintObject);
    }
    // Highlight the currently selected constraint badge.
    const selCon = selectedConstraintRef.current;
    for (const obj of sketchConstraintObjectsRef.current) {
      const conEntityId =
        obj.userData.sketchConstraintEntityId as string | undefined;
      const conKind =
        obj.userData.sketchConstraintKind as string | undefined;
      const isSelected =
        selCon !== null &&
        conEntityId === selCon.entityId &&
        conKind === selCon.kind;
      if (obj instanceof THREE.Sprite && obj.material instanceof THREE.SpriteMaterial) {
        if (isSelected) {
          obj.material.color.set(0x60e0ff); // bright cyan
          obj.scale.set(7.5, 7.5, 1);
        } else {
          obj.material.color.set(0xffffff);
          obj.scale.set(6, 6, 1);
        }
      }
    }

    for (const sketchProfile of sceneData.sketchProfiles) {
      const sketchProfileObject = buildSketchProfileObject(sketchProfile);
      sketchProfileObjectsRef.current.push(sketchProfileObject.group);
      sketchProfileVisualsRef.current.set(
        sketchProfile.profileId,
        sketchProfileObject.visual,
      );
      sketchProfileStatesRef.current.set(sketchProfile.profileId, {
        isSelected: sketchProfile.isSelected,
        isHovered: false,
      });
      sketchGroup.add(sketchProfileObject.group);
    }

    for (const sketchPoint of sceneData.sketchPoints) {
      if (sketchPoint.kind === "quadrant") {
        continue;
      }
      const sketchPointObject = buildSketchPointObject(sketchPoint);
      sketchPointObject.userData.isSelected = sketchPoint.isSelected;
      sketchPointObjectsRef.current.push(sketchPointObject);
      sketchPointObjectByIdRef.current.set(
        sketchPoint.pointId,
        sketchPointObject,
      );
      sketchGroup.add(sketchPointObject);
    }

    syncPrimitiveVisuals();
    syncReferencePlaneVisuals();
    syncSolidFaceVisuals();
    syncSketchProfileVisuals();
    paintSketchEntityMaterials();
    paintSketchPointMaterials();
    paintDofStatusColors();

    if (sceneData.geometryKey !== lastGeometryKeyRef.current) {
      // Auto-frame the camera ONLY on the very first scene load (when
      // we haven't recorded any prior geometry key yet). On subsequent
      // rebuilds the geometry key flips for many reasons that have
      // nothing to do with the user's intended view — selection
      // state, hover/select highlight rebuilds, depth-preview ticks
      // during an extrude edit, etc. — so re-fitting every time
      // would yank the camera back to its initial pose any time the
      // user clicks a face. Sketch-mode framing is handled by a
      // separate effect (frameCameraToSketchPlane).
      const isFirstSceneLoad = lastGeometryKeyRef.current === "";
      if (isFirstSceneLoad && !activeSketchPlaneId) {
        frameCamera(
          camera,
          controls,
          sceneData.bounds.center,
          sceneData.bounds.maxDimension,
        );
      }

      lastGeometryKeyRef.current = sceneData.geometryKey;
    }
  }, [activeTheme.id, config.displayUnits, displayedSketchDimensions, moveGizmo, sceneData, showReferencePlanes]);

  useEffect(() => {
    lineDraftStartRef.current = null;
    arcSecondPointRef.current = null;
    rectSecondPointRef.current = null;
    circleSecondPointRef.current = null;
    clearPreviewLine();
    clearPreviewCircle();
    clearPreviewArc();
    clearPreviewDimension();
    setSketchSnapLabel(null);
    setConstraintPreview(null);
    clearDraftDimensionSession();
    cancelDimensionPlacement();
    pendingDimensionPlacementRef.current = false;
    // Reset the dimension tool's pending first-line on every tool
    // switch so it can't leak across tools or sketches.
    dimensionToolFirstLineRef.current = null;
    setDimensionToolFirstLine(null);
    dimensionToolFirstPointRef.current = null;
  }, [activeSketchPlaneId, activeSketchTool]);

  useEffect(() => {
    if (!activeSketchPlaneId) {
      return undefined;
    }

    function handleKeyDown(event: KeyboardEvent) {
      if (
        event.code === "Enter" &&
        dimensionLabelDragRef.current?.isPlacement
      ) {
        event.preventDefault();
        finishDimensionPlacement();
        return;
      }
      if (
        event.code === "Escape" &&
        dimensionLabelDragRef.current?.isPlacement
      ) {
        event.preventDefault();
        const dimId = dimensionLabelDragRef.current.dimensionId;
        dimensionLabelDragRef.current = null;
        dimensionPlacementOriginalPositionRef.current = null;
        if (controlsRef.current) controlsRef.current.enabled = true;
        setCanvasCursor("");
        void deleteSketchDimensionRef.current(dimId);
        return;
      }
      // Escape in the dimension tool: delete the just-placed
      // dimension.  We check refs (not closure-captured state) so
      // the handler works even when selectedSketchDimension hasn't
      // updated yet.  pendingDimensionIdRef is set before every IPC
      // dimension-create call; if that's null we fall back to
      // checking the label drag state.
      if (
        event.code === "Escape" &&
        activeSketchToolRef.current === "dimension"
      ) {
        const drag = dimensionLabelDragRef.current;
        const targetId = pendingDimensionIdRef.current ??
          (drag?.isPlacement ? drag.dimensionId : null);
        if (targetId) {
          event.preventDefault();
          clearPreviewDimension();
          pendingDimensionIdRef.current = null;
          pendingDimSourceEntityIdRef.current = null;
          pendingDimensionPlacementRef.current = false;
          dimensionLabelDragRef.current = null;
          dimensionPlacementOriginalPositionRef.current = null;
          if (controlsRef.current) controlsRef.current.enabled = true;
          setCanvasCursor("");
          void deleteSketchDimensionRef.current(targetId);
          return;
        }
      }
      const target = event.target;
      if (
        target instanceof HTMLInputElement ||
        target instanceof HTMLTextAreaElement ||
        target instanceof HTMLSelectElement ||
        (target instanceof HTMLElement && target.isContentEditable)
      ) {
        return;
      }

      if (event.code === "Escape") {
        event.preventDefault();
        setSelectedConstraint(null);
        cancelActiveSketchDraft();
        return;
      }

      if (event.code === "Delete" || event.code === "Backspace") {
        event.preventDefault();
        const sel = selectedConstraintRef.current;
        if (sel) {
          setSelectedConstraint(null);
          void clearSketchConstraintRef.current(
            sel.kind, sel.entityId, sel.relatedEntityId ?? undefined,
          );
        } else {
          // Read selection from document state directly (not via the
          // async store) to avoid a race condition where the user presses
          // Delete before the select_sketch_entity IPC response updates
          // the store's document state. Use selected_sketch_entity_ids
          // from the current document object (ref).
          const entityIds = document?.selected_sketch_entity_ids ?? [];
          const entityId = document?.selected_sketch_entity_id;
          const pointIds = document?.selected_sketch_point_ids ?? [];
          const profileIds = document?.selected_sketch_profile_ids ?? [];
          const allEntityIds = entityId
            ? entityIds.includes(entityId)
              ? entityIds
              : [...entityIds, entityId]
            : entityIds;
          if (allEntityIds.length > 0 || pointIds.length > 0 || profileIds.length > 0) {
            void deleteSketchSelectionRef.current({
              entityIds: allEntityIds,
              pointIds,
              profileIds,
            });
          } else {
            void deleteSketchSelectionRef.current();
          }
        }
        return;
      }

      if (matchesHotkey(event, config.hotkeys.sketchToolbar.line)) {
        event.preventDefault();
        void setSketchToolRef.current("line");
        return;
      }

      if (matchesHotkey(event, config.hotkeys.sketchToolbar.rectangle)) {
        event.preventDefault();
        void setSketchToolRef.current("rectangle");
        return;
      }

      if (matchesHotkey(event, config.hotkeys.sketchToolbar.circle)) {
        event.preventDefault();
        void setSketchToolRef.current("circle");
        return;
      }

      if (matchesHotkey(event, config.hotkeys.sketchToolbar.trim)) {
        event.preventDefault();
        void setSketchToolRef.current("trim");
        return;
      }

      // X toggles the construction flag while a drawable sketch tool
      // is armed.
      if (
        matchesHotkey(
          event,
          config.hotkeys.sketchToolbar.toggleConstruction,
        ) &&
        (isDraftDimensionTool(activeSketchToolRef.current) ||
          activeSketchToolRef.current === "arc")
      ) {
        event.preventDefault();
        setSketchToolConstruction((prev) => {
          const next = !prev;
          sketchToolConstructionRef.current = next;
          return next;
        });
        return;
      }

      // D arms the dimension tool (CAD convention). Clicking a
      // line or circle while armed opens its driving dimension's
      // inline editor.
      if (matchesHotkey(event, config.hotkeys.sketchToolbar.dimension)) {
        event.preventDefault();
        void setSketchToolRef.current("dimension");
        return;
      }
    }

    window.addEventListener("keydown", handleKeyDown);
    return () => {
      window.removeEventListener("keydown", handleKeyDown);
    };
  }, [activeSketchPlaneId, config.hotkeys.sketchToolbar]);

  // Tab toggles ghost-edge visibility while a fillet/chamfer panel is
  // open. The handler is mounted only when at least one body has a
  // pending edge-op feature so the key keeps its default browser
  // behavior in every other context. Hold to reveal, release to hide.
  useEffect(() => {
    if (pendingEdgeOpBodyIds.size === 0) {
      return;
    }
    function isTypingTarget(target: EventTarget | null) {
      return (
        target instanceof HTMLInputElement ||
        target instanceof HTMLTextAreaElement ||
        target instanceof HTMLSelectElement ||
        (target instanceof HTMLElement && target.isContentEditable)
      );
    }
    function setReveal(next: boolean) {
      if (revealGhostEdgesRef.current === next) {
        return;
      }
      revealGhostEdgesRef.current = next;
      paintEdgeMaterials(hoveredEdgeIdRef.current);
    }
    function handleKeyDown(event: KeyboardEvent) {
      if (event.code !== "Tab") {
        return;
      }
      if (isTypingTarget(event.target)) {
        return;
      }
      // Suppress the default Tab focus shuffle so the panel session
      // stays in control of the input the user just typed into.
      event.preventDefault();
      setReveal(true);
    }
    function handleKeyUp(event: KeyboardEvent) {
      if (event.code !== "Tab") {
        return;
      }
      setReveal(false);
    }
    function handleBlur() {
      // Window blur (e.g. user switched apps mid-hold) loses keyup
      // events; reset so the wireframe doesn't get stuck "on".
      setReveal(false);
    }
    window.addEventListener("keydown", handleKeyDown);
    window.addEventListener("keyup", handleKeyUp);
    window.addEventListener("blur", handleBlur);
    return () => {
      window.removeEventListener("keydown", handleKeyDown);
      window.removeEventListener("keyup", handleKeyUp);
      window.removeEventListener("blur", handleBlur);
      // Drop the reveal state so the next pending session starts
      // hidden by default.
      revealGhostEdgesRef.current = false;
    };
  }, [pendingEdgeOpBodyIds]);

  useEffect(() => {
    if (activeSketchPlaneId) {
      if (previousReferencePlaneVisibilityRef.current === null) {
        previousReferencePlaneVisibilityRef.current = showReferencePlanes;
      }

      if (showReferencePlanes) {
        setShowReferencePlanes(false);
      }
      return;
    }

    if (previousReferencePlaneVisibilityRef.current !== null) {
      setShowReferencePlanes(previousReferencePlaneVisibilityRef.current);
      previousReferencePlaneVisibilityRef.current = null;
    }
  }, [activeSketchPlaneId, showReferencePlanes]);

  const lastFramedSketchPlaneRef = useRef<string | null>(null);
  useEffect(() => {
    const camera = cameraRef.current;
    const controls = controlsRef.current;

    if (!camera || !controls) {
      return;
    }

    if (!activeSketchPlaneId) {
      lastFramedSketchPlaneRef.current = null;
      return;
    }

    if (lastFramedSketchPlaneRef.current === activeSketchPlaneId) {
      return;
    }

    if (!sceneData) {
      return;
    }

    lastFramedSketchPlaneRef.current = activeSketchPlaneId;

    frameCameraToSketchPlane(
      camera,
      controls,
      activeSketchPlaneId,
      activeSketchPlaneFrame,
      sceneData.bounds.maxDimension,
    );
  }, [activeSketchPlaneId, activeSketchPlaneFrame, sceneData]);

  async function handleCreateSketchFromContextMenu() {
    if (contextMenu?.referenceId) {
      setContextMenu(null);
      await selectReferenceRef.current(contextMenu.referenceId);
      await startSketchRef.current(contextMenu.referenceId);
      return;
    }

    if (!contextMenu?.faceId) {
      return;
    }

    setContextMenu(null);
    await selectFaceRef.current(contextMenu.faceId);

    const solidFace = sceneData?.solidFaces.find(
      (face) => face.faceId === contextMenu.faceId,
    );
    if (!solidFace) {
      return;
    }

    await startSketchOnFaceRef.current(solidFace.faceId, solidFace.planeFrame);
  }

  async function handleMoveBodyFromContextMenu() {
    const bodyId = contextMenu?.bodyId;
    if (!bodyId) {
      return;
    }
    setContextMenu(null);
    await moveBodyRef.current?.(bodyId);
  }

  async function handleCopyBodyFromContextMenu(copyMode: "linked" | "standalone") {
    const bodyId = contextMenu?.bodyId;
    if (!bodyId) {
      return;
    }
    setContextMenu(null);
    await copyBodyRef.current?.(bodyId, copyMode);
  }

  async function handleUnlinkBodyCopyFromContextMenu() {
    const bodyId = contextMenu?.bodyId;
    if (!bodyId) {
      return;
    }
    setContextMenu(null);
    await unlinkBodyCopyRef.current?.(bodyId);
  }

  async function handleDeleteSketchFromContextMenu() {
    const selection = contextMenu?.sketchDeleteSelection;
    if (!selection) {
      return;
    }
    setContextMenu(null);
    await deleteSketchSelectionRef.current(selection);
  }

  async function handleDeleteDimensionFromContextMenu() {
    const dimensionId = contextMenu?.dimensionId;
    if (!dimensionId) {
      return;
    }
    setContextMenu(null);
    setIsDimensionEditorOpen(false);
    await deleteSketchDimensionRef.current(dimensionId);
  }

  async function handleDeleteConstraintFromContextMenu() {
    const kind = contextMenu?.constraintKind;
    const entityId = contextMenu?.constraintEntityId;
    if (!kind || !entityId) {
      return;
    }
    setContextMenu(null);
    setSelectedConstraint(null);
    await clearSketchConstraintRef.current(
      kind as ConstraintType,
      entityId,
      contextMenu?.constraintRelatedEntityId ?? null,
    );
  }

  async function handleToggleDimensionDisplayFromContextMenu() {
    const dimensionId = contextMenu?.dimensionId;
    if (!dimensionId) return;
    const sketch = sketchLinesRef.current;
    if (!sketch) return;
    const dim = sketch.dimensions.find(
      (d) => d.dimension_id === dimensionId,
    );
    if (!dim || dim.kind !== "circle_radius") return;

    // Toggle: "" (diameter) → "radius" → "" (diameter)
    const newDisplayAs = dim.display_as === "radius" ? "" : "radius";
    setContextMenu(null);
    await updateSketchDimensionDisplayRef.current(dimensionId, newDisplayAs);
  }

  const lineCount = sketchFeature?.sketch_parameters?.lines.length ?? 0;
  const circleCount = sketchFeature?.sketch_parameters?.circles.length ?? 0;

  function isLinkedBodyCopy(bodyId: string | null | undefined) {
    if (!bodyId) {
      return false;
    }
    const feature = document?.feature_history.find(
      (entry) => entry.feature_id === bodyId,
    );
    return (
      feature?.kind === "body_copy" &&
      feature.body_copy_parameters?.copy_mode === "linked"
    );
  }

  async function handleSubmitDimensionEdit() {
    if (!selectedSketchDimension) {
      setIsDimensionEditorOpen(false);
      return;
    }

    const rawValue = dimensionDraftValue.trim();
    if (!rawValue) {
      setIsDimensionEditorOpen(false);
      return;
    }

    // If the value parses as a plain number, send it as a number
    // (backward compatible). Parse with display-unit conversion.
    // If it contains non-numeric characters (e.g. "width * 2"), send it
    // as a formula expression.
    // Angles are unitless (same in mm and inch) — skip displayToMm
    // and let dimensionCoreValue handle the degrees→radians conversion.
    const isAngle = selectedSketchDimension?.kind === "angle" ||
      selectedSketchDimension?.kind === "line_angle";
    let parsed: number | null;
    if (isAngle) {
      const normalized = rawValue.replace(",", ".");
      const p = parseFloat(normalized);
      parsed = isNaN(p) ? null : p;
    } else {
      parsed = parseDimensionInput(rawValue, config.displayUnits);
    }
    if (parsed !== null && parsed > 0) {
      await updateSketchDimensionRef.current(
        selectedSketchDimension.dimensionId,
        dimensionCoreValue(selectedSketchDimension, parsed),
      );
    } else {
      // Send as expression string — the core will evaluate it
      await updateSketchDimensionRef.current(
        selectedSketchDimension.dimensionId,
        rawValue,
      );
    }
    finishDimensionPlacement();
    dimensionEditOriginalValueRef.current = null;
    setIsDimensionEditorOpen(false);
  }

  function handleDimensionDraftChange(value: string) {
    setDimensionDraftValue(value);
    if (!selectedSketchDimension) {
      return;
    }
    const trimmed = value.trim();
    if (!trimmed) {
      return;
    }
    // Only send numeric values as live preview. Expressions
    // (parameter names, formulas) are held until Enter — partial
    // keystrokes like "t", "te" would otherwise flood the core
    // with "unknown parameter" errors before the name is complete.
    // Angles are unitless — skip displayToMm.
    const isAngle = selectedSketchDimension?.kind === "angle" ||
      selectedSketchDimension?.kind === "line_angle";
    let parsed: number | null;
    if (isAngle) {
      const normalized = trimmed.replace(",", ".");
      const p = parseFloat(normalized);
      parsed = isNaN(p) ? null : p;
    } else {
      parsed = parseDimensionInput(trimmed, config.displayUnits);
    }
    if (parsed !== null && parsed > 0) {
      void updateSketchDimensionRef.current(
        selectedSketchDimension.dimensionId,
        dimensionCoreValue(selectedSketchDimension, parsed),
      );
    }
    // Send parameter names / expressions with a 300ms debounce so the
    // core doesn't flood with partial parameter names ("t", "te", ...).
    else if (/[a-zA-Z_]/.test(trimmed)) {
      if (dimensionExpressionTimeoutRef.current !== null) {
        clearTimeout(dimensionExpressionTimeoutRef.current);
      }
      dimensionExpressionTimeoutRef.current = setTimeout(() => {
        dimensionExpressionTimeoutRef.current = null;
        void updateSketchDimensionRef.current(
          selectedSketchDimension.dimensionId,
          trimmed,
        ).catch(() => {});
      }, 300);
    }
  }

  function insertDimensionParameterSuggestion(name: string) {
    const input = dimensionInputRef.current;
    const cursor = input?.selectionStart ?? dimensionDraftValue.length;
    const token = parameterTokenAtCursor(dimensionDraftValue, cursor);
    const start = token?.start ?? cursor;
    const end = token?.end ?? cursor;
    const nextValue =
      dimensionDraftValue.slice(0, start) +
      name +
      dimensionDraftValue.slice(end);
    setDimensionDraftValue(nextValue);
    handleDimensionDraftChange(nextValue);
    window.requestAnimationFrame(() => {
      const nextCursor = start + name.length;
      input?.focus();
      input?.setSelectionRange(nextCursor, nextCursor);
    });
  }

  function cancelDimensionEdit() {
    // Cancel any pending debounced expression send
    if (dimensionExpressionTimeoutRef.current !== null) {
      clearTimeout(dimensionExpressionTimeoutRef.current);
      dimensionExpressionTimeoutRef.current = null;
    }
    const dimension = selectedSketchDimension;
    const originalValue = dimensionEditOriginalValueRef.current;
    cancelDimensionPlacement();
    if (dimension && originalValue?.dimensionId === dimension.dimensionId) {
      void updateSketchDimensionRef.current(
        dimension.dimensionId,
        originalValue.expression.trim().length > 0
          ? originalValue.expression
          : originalValue.value,
      );
      setDimensionLabelPositions((current) => {
        if (!(dimension.dimensionId in current)) {
          return current;
        }
        const next = { ...current };
        delete next[dimension.dimensionId];
        return next;
      });
      setDimensionDraftValue(
        originalValue.expression.trim().length > 0
          ? originalValue.expression
          : formattedDimensionDisplayValue(dimension, originalValue.value),
      );
    } else if (dimension && selectedSketchDimensionValue !== null) {
      setDimensionDraftValue(
        selectedSketchDimensionExpression.trim().length > 0
          ? selectedSketchDimensionExpression
          : formattedDimensionDisplayValue(dimension, selectedSketchDimensionValue),
      );
    } else {
      setDimensionDraftValue("");
    }
    dimensionEditOriginalValueRef.current = null;
    setIsDimensionEditorOpen(false);
  }

  function draftFieldScreenPosition(field: DraftDimensionField) {
    const session = draftDimensionSession;
    // For line tool, use the render-loop-computed screen positions from
    // the 3D dimension lines. These are updated every frame and match
    // the Three.js geometry drawn in renderDraftDimensions().
    if (session?.tool === "line") {
      const fromRef = draftDimScreenPositionsRef.current[field];
      if (fromRef) {
        return fromRef;
      }
      // Fall through to legacy path if the render loop hasn't computed
      // positions yet (e.g., first frame).
    }

    if (!cameraRef.current || !rendererRef.current) {
      return null;
    }
    if (!session) return null;

    const [sx, sy] = session.start;
    const [ex, ey] = session.current;
    let local: [number, number] = session.current;
    let offset: [number, number] = [0, -DRAFT_DIMENSION_OFFSET_PX];

    if (session.tool === "rectangle") {
      if (field === "width") {
        local = [(sx + ex) / 2, sy];
        offset = [0, -DRAFT_DIMENSION_OFFSET_PX];
      } else {
        local = [ex, (sy + ey) / 2];
        offset = [DRAFT_DIMENSION_OFFSET_PX, 0];
      }
    } else if (session.tool === "line") {
      if (field === "angle") {
        local = [sx, sy];
        offset = [0, -DRAFT_DIMENSION_OFFSET_PX];
      } else {
        local = [(sx + ex) / 2, (sy + ey) / 2];
        offset = [0, -DRAFT_DIMENSION_OFFSET_PX];
      }
    } else {
      local = session.start;
      offset = [0, -DRAFT_DIMENSION_OFFSET_PX];
    }

    const world = toWorldPoint(
      activeSketchPlaneId ?? "ref-plane-xy",
      local,
      activeSketchPlaneFrame,
    );
    const point = projectWorldPointToViewport(
      world,
      cameraRef.current,
      rendererRef.current,
    );
    if (!point) {
      return null;
    }
    return {
      x: point.x + offset[0],
      y: point.y + offset[1],
    };
  }

  function draftDisplayValue(rawValue: string): string {
    if (config.displayUnits === "mm") return rawValue;
    const num = Number(rawValue);
    if (!Number.isFinite(num) || num <= 0) return rawValue;
    const display = mmToDisplay(num, config.displayUnits);
    return String(parseFloat(display.toFixed(3)));
  }

  function handleDraftDimensionChange(
    field: DraftDimensionField,
    value: string,
  ) {
    const session = draftDimensionSessionRef.current;
    if (!session) {
      return;
    }
    // Preserve raw input during editing so partial values like "2."
    // don't lose the decimal when the round-trip through mm converts
    // them back to display.
    draftRawInputRef.current[field] = value;
    // Convert display-unit input to mm for internal storage
    const parsed = parseDimensionInput(value, config.displayUnits);
    let mmValue: string;
    if (parsed !== null) {
      mmValue = String(parsed);
      delete draftParameterExpressionRef.current[field];
    } else if (/[a-zA-Z_]/.test(value)) {
      draftParameterExpressionRef.current[field] = value.trim();
      // Try to resolve as a parameter name for live draft preview.
      // The draft dimension system is client-side, so we look up the
      // parameter in the current document state.  Angle parameters
      // store degrees, length parameters store mm — both match what
      // applyDraftDimensionFieldValue expects for their respective fields.
      const param = document?.parameters.find((p) => p.name === value.trim());
      if (param && !param.has_error && Number.isFinite(param.resolved_value) && param.resolved_value > 0) {
        mmValue = String(param.resolved_value);
      } else {
        mmValue = value;
      }
    } else {
      delete draftParameterExpressionRef.current[field];
      mmValue = value;
    }
    const next = applyDraftDimensionField(session, field, mmValue);
    draftDimensionSessionRef.current = next;
    // Clear all render-loop positions so both fields get fresh
    // fallback positions.  Changing the length also moves the angle
    // arc endpoint — clearing only the changed field leaves the
    // other stuck at its old screen position until the next frame.
    draftDimScreenPositionsRef.current = {};
    setDraftDimensionSession(next);
    setDraftSuggestionState({ field, index: 0 });
  }

  function getDraftFieldInputValue(
    session: DraftDimensionSession,
    field: DraftDimensionField,
  ) {
    if (
      draftFieldFocusedRef.current === field &&
      draftRawInputRef.current[field] !== undefined
    ) {
      return draftRawInputRef.current[field] ?? "";
    }
    const expression = draftParameterExpressionRef.current[field];
    if (expression && expression.trim().length > 0) {
      return expression;
    }
    return draftDisplayValue(session.values[field]);
  }

  function getDraftParameterSuggestions(
    field: DraftDimensionField,
    value: string,
  ) {
    const input = draftDimensionInputRefs.current[field];
    const cursor = input?.selectionStart ?? value.length;
    return getParameterSuggestions(value, cursor, field === "angle");
  }

  function insertDraftParameterSuggestion(
    field: DraftDimensionField,
    name: string,
  ) {
    const input = draftDimensionInputRefs.current[field];
    const currentValue = input?.value ?? draftRawInputRef.current[field] ?? "";
    const cursor = input?.selectionStart ?? currentValue.length;
    const token = parameterTokenAtCursor(currentValue, cursor);
    const start = token?.start ?? cursor;
    const end = token?.end ?? cursor;
    const nextValue =
      currentValue.slice(0, start) + name + currentValue.slice(end);
    handleDraftDimensionChange(field, nextValue);
    window.requestAnimationFrame(() => {
      const nextCursor = start + name.length;
      input?.focus();
      input?.setSelectionRange(nextCursor, nextCursor);
    });
  }

  function focusDraftField(field: DraftDimensionField) {
    window.requestAnimationFrame(() => {
      draftDimensionInputRefs.current[field]?.focus();
      draftDimensionInputRefs.current[field]?.select();
    });
  }

  function handleDraftDimensionKeyDown(
    event: ReactKeyboardEvent<HTMLInputElement>,
    field: DraftDimensionField,
  ) {
    const session = draftDimensionSessionRef.current;
    if (!session) {
      return;
    }
    const suggestions = getDraftParameterSuggestions(
      field,
      event.currentTarget.value,
    );
    if (
      suggestions.length > 0 &&
      (event.key === "ArrowDown" || event.key === "ArrowUp")
    ) {
      event.preventDefault();
      setDraftSuggestionState((current) => {
        const currentIndex =
          current?.field === field ? current.index : 0;
        const delta = event.key === "ArrowDown" ? 1 : -1;
        return {
          field,
          index:
            (currentIndex + delta + suggestions.length) %
            suggestions.length,
        };
      });
      return;
    }
    if (
      suggestions.length > 0 &&
      (event.key === "Tab" || event.key === "Enter")
    ) {
      event.preventDefault();
      const suggestionIndex =
        draftSuggestionState?.field === field
          ? draftSuggestionState.index
          : 0;
      const suggestion = suggestions[suggestionIndex] ?? suggestions[0];
      insertDraftParameterSuggestion(field, suggestion.name);
      return;
    }
    if (event.key === "Enter") {
      event.preventDefault();
      void commitDraftDimensionSession(session);
      void setSketchToolRef.current("select");
      return;
    }
    if (event.key === "Escape") {
      event.preventDefault();
      event.stopPropagation();
      cancelActiveSketchDraft();
      return;
    }
    if (event.key !== "Tab") {
      return;
    }
    event.preventDefault();
    const fields = draftSessionFields(session.tool);
    const index = fields.indexOf(field);
    const nextField =
      fields[
        (index + (event.shiftKey ? -1 : 1) + fields.length) % fields.length
      ];
    const next = { ...session, activeField: nextField };
    draftDimensionSessionRef.current = next;
    setDraftDimensionSession(next);
    focusDraftField(nextField);
  }

  const isSketchDrawingCursor =
    Boolean(activeSketchPlaneId) &&
    activeSketchTool !== "select" &&
    activeSketchTool !== "project";
  const crosshairMode = config.viewport.crosshair;
  const usesCrosshairGuide =
    crosshairMode === "viewport-25" ||
    crosshairMode === "viewport-50" ||
    crosshairMode === "viewport-75" ||
    crosshairMode === "infinite";
  const crosshairGuideSize =
    crosshairMode === "infinite"
      ? Math.max(viewportSize.width, viewportSize.height) * 2
      : viewportSize.height * (CROSSHAIR_SIZE_FACTORS[crosshairMode] ?? 0);
  const crosshairCanvasClass = isSketchDrawingCursor
    ? [
        "cad-viewport-canvas-drawing",
        usesCrosshairGuide ? "cad-viewport-canvas-drawing-guide" : "",
      ]
        .filter(Boolean)
        .join(" ")
    : "";
  const isSketchMode = Boolean(activeSketchPlaneId);

  return (
    <section className="relative flex h-full min-h-0 flex-col overflow-hidden">
      {showViewportGrid && !activeSketchPlaneId ? (
        <div className="pointer-events-none absolute inset-0 cad-grid-stage opacity-70" />
      ) : null}
      <div
        ref={hostRef}
        className="absolute inset-0 min-h-0 min-w-0 overflow-hidden rounded-[18px]"
      >
        {contextMenu ? (
          <div
            className="cad-context-menu absolute z-20 min-w-[160px] rounded-2xl p-1.5 backdrop-blur-xl"
            style={{
              left: contextMenu.x,
              top: contextMenu.y,
              transform: "translate(8px, 8px)",
            }}
          >
            {contextMenu.dimensionId ? (
              <>
                {(() => {
                  // Only show toggle for circle_radius dimensions
                  const sketch = sketchLinesRef.current;
                  if (!sketch) return null;
                  const dim = sketch.dimensions.find(
                    (d) => d.dimension_id === contextMenu.dimensionId,
                  );
                  if (!dim || dim.kind !== "circle_radius") return null;
                  const label =
                    dim.display_as === "radius"
                      ? "Show Diameter"
                      : "Show Radius";
                  return (
                    <button
                      type="button"
                      className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
                      onClick={handleToggleDimensionDisplayFromContextMenu}
                    >
                      {label}
                    </button>
                  );
                })()}
                <button
                  type="button"
                  className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
                  onClick={handleDeleteDimensionFromContextMenu}
                >
                  Delete
                </button>
              </>
            ) : contextMenu.constraintKind ? (
              <button
                type="button"
                className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
                onClick={handleDeleteConstraintFromContextMenu}
              >
                Delete Constraint
              </button>
            ) : contextMenu.sketchDeleteSelection ? (
              <button
                type="button"
                className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
                onClick={handleDeleteSketchFromContextMenu}
              >
                Delete
              </button>
            ) : (
              <>
                {contextMenu.bodyId ? (
                  <>
                    <button
                      type="button"
                      className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
                      onClick={handleMoveBodyFromContextMenu}
                    >
                      {translate("common.move")}
                    </button>
                    <div className="group/copy relative">
                      <button
                        type="button"
                        className="cad-context-menu-item flex w-full items-center justify-between gap-3 rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
                      >
                        <span>{translate("common.copy")}</span>
                        <span className="text-on-surface-dim">&gt;</span>
                      </button>
                      <div className="cad-context-menu invisible absolute left-full top-0 z-30 ml-1 min-w-[180px] rounded-2xl p-1.5 opacity-0 backdrop-blur-xl transition-opacity group-hover/copy:visible group-hover/copy:opacity-100">
                        <button
                          type="button"
                          className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
                          onClick={() => {
                            void handleCopyBodyFromContextMenu("linked");
                          }}
                        >
                          {translate("common.copyLinked")}
                        </button>
                        <button
                          type="button"
                          className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
                          onClick={() => {
                            void handleCopyBodyFromContextMenu("standalone");
                          }}
                        >
                          {translate("common.copyIndependent")}
                        </button>
                      </div>
                    </div>
                    {isLinkedBodyCopy(contextMenu.bodyId) ? (
                      <button
                        type="button"
                        className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
                        onClick={handleUnlinkBodyCopyFromContextMenu}
                      >
                        {translate("common.unlink")}
                      </button>
                    ) : null}
                  </>
                ) : null}
                {contextMenu.referenceId || contextMenu.faceId ? (
                  <button
                    type="button"
                    className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
                    onClick={handleCreateSketchFromContextMenu}
                  >
                    Create Sketch
                  </button>
                ) : null}
              </>
            )}
          </div>
        ) : null}
        <canvas
          ref={canvasRef}
          className={`cad-viewport-canvas absolute inset-0 h-full w-full ${crosshairCanvasClass}`}
        />
        {!isSketchMode ? (
          <div className="pointer-events-auto absolute bottom-4 left-1/2 z-20 -translate-x-1/2">
            <div className="cad-view-mini-toolbar flex items-center gap-1 px-1.5 py-1.5 backdrop-blur-xl">
              <ToolbarTooltip
                label={`${
                  showViewportGrid
                    ? translate("viewport.hideViewportGrid")
                    : translate("viewport.showViewportGrid")
                } (${formatHotkey(config.hotkeys.viewport.toggleGrid)})`}
              >
                <button
                  type="button"
                  className={
                    showViewportGrid
                      ? "cad-view-mini-button cad-view-mini-button-active"
                      : "cad-view-mini-button"
                  }
                  aria-label={
                    showViewportGrid
                      ? translate("viewport.hideViewportGrid")
                      : translate("viewport.showViewportGrid")
                  }
                  aria-pressed={showViewportGrid}
                  onClick={() => {
                    toggleGridVisibility("viewport");
                  }}
                >
                  <GridMiniIcon />
                </button>
              </ToolbarTooltip>
            </div>
          </div>
        ) : (
          <div className="pointer-events-auto absolute bottom-4 left-1/2 z-20 -translate-x-1/2">
            <div className="cad-view-mini-toolbar flex items-center gap-1 px-1.5 py-1.5 backdrop-blur-xl">
              <ToolbarTooltip
                label={`${
                  showSketchGrid
                    ? translate("viewport.hideSketchGrid")
                    : translate("viewport.showSketchGrid")
                } (${formatHotkey(config.hotkeys.viewport.toggleGrid)})`}
              >
                <button
                  type="button"
                  className={
                    showSketchGrid
                      ? "cad-view-mini-button cad-view-mini-button-active"
                      : "cad-view-mini-button"
                  }
                  aria-label={
                    showSketchGrid
                      ? translate("viewport.hideSketchGrid")
                      : translate("viewport.showSketchGrid")
                  }
                  aria-pressed={showSketchGrid}
                  onClick={() => {
                    toggleGridVisibility("sketch");
                  }}
                >
                  <GridMiniIcon />
                </button>
              </ToolbarTooltip>
            </div>
          </div>
        )}
        {isSketchDrawingCursor &&
        usesCrosshairGuide &&
        crosshairPointer &&
        crosshairGuideSize > 0 ? (
          <div
            className="cad-crosshair-guide"
            style={{
              left: crosshairPointer.x,
              top: crosshairPointer.y,
              width: crosshairGuideSize,
              height: crosshairGuideSize,
              transform: "translate(-50%, -50%)",
            }}
          />
        ) : null}
        {/* Selection rectangle overlay */}
        {selectionRect?.visible ? (
          <div
            className="pointer-events-none fixed z-30"
            style={{
              left: selectionRect.left + 'px',
              top: selectionRect.top + 'px',
              width: selectionRect.width + 'px',
              height: selectionRect.height + 'px',
              border: selectionRect.direction === "window"
                ? "1px solid var(--color-primary-edge-active, #4fc3f7)"
                : "1px dashed var(--color-destructive, #4caf50)",
              background: selectionRect.direction === "window"
                ? "rgba(79, 195, 247, 0.07)"
                : "rgba(76, 175, 80, 0.07)",
            }}
          />
        ) : null}

        {/*
          Cursor-following constraint preview badge. Only visible
          while a sketch tool is producing a midpoint, perpendicular,
          or on-line snap. The badge is offset 12px down-right from
          the cursor so it doesn't sit under the actual snap dot, and
          is `pointer-events-none` so it never steals clicks from the
          underlying canvas. The colors mirror the in-scene
          constraint-badge palette to keep the language consistent.
        */}
        {constraintPreview ? (
          <div
            className="pointer-events-none absolute z-30 flex h-5 w-5 items-center justify-center rounded-full border border-cyan-300/70 bg-slate-900/85 text-[10px] font-semibold text-cyan-200 shadow-md"
            style={{
              left: `${constraintPreview.x + 12}px`,
              top: `${constraintPreview.y + 12}px`,
            }}
          >
            {constraintPreview.kind === "midpoint"
              ? "M"
              : constraintPreview.kind === "perpendicular"
                ? "\u22a5"
                : constraintPreview.kind === "horizontal"
                  ? "H"
                  : constraintPreview.kind === "vertical"
                    ? "V"
                    : constraintPreview.kind === "tangent"
                      ? "T"
              : "/"}
          </div>
        ) : null}
        {draftDimensionSession
          ? draftSessionFields(draftDimensionSession.tool).map((field) => {
              const position = draftFieldScreenPosition(field);
              if (!position) {
                return null;
              }
              const inputValue = getDraftFieldInputValue(
                draftDimensionSession,
                field,
              );
              const suggestions = getDraftParameterSuggestions(
                field,
                inputValue,
              );
              const suggestionIndex =
                draftSuggestionState?.field === field
                  ? draftSuggestionState.index
                  : 0;
              return (
                <form
                  key={field}
                  className="pointer-events-auto absolute z-30 flex w-[120px] items-center rounded-md border px-2 py-1 backdrop-blur-md"
                  style={{
                    left: position.x,
                    top: position.y,
                    transform: "translate(-50%, -50%)",
                    opacity: 0.65,
                    background: "var(--cad-dimension-editor-bg)",
                    borderColor: "var(--cad-dimension-editor-border)",
                    boxShadow:
                      "0 4px 12px var(--cad-dimension-editor-shadow)",
                  }}
                  onSubmit={(event) => {
                    event.preventDefault();
                    void commitDraftDimensionSession();
                  }}
                >
                  <input
                    ref={(input) => {
                      draftDimensionInputRefs.current[field] = input;
                    }}
                    className="h-6 w-full bg-transparent text-center text-sm font-semibold text-on-surface tabular-nums outline-none"
                    value={inputValue}
                    inputMode="text"
                    onChange={(event) => {
                      handleDraftDimensionChange(field, event.target.value);
                    }}
                    onFocus={() => {
                      draftFieldFocusedRef.current = field;
                      const next = {
                        ...draftDimensionSession,
                        activeField: field,
                      };
                      draftDimensionSessionRef.current = next;
                      setDraftDimensionSession(next);
                      setDraftSuggestionState({ field, index: 0 });
                    }}
                    onBlur={() => {
                      draftFieldFocusedRef.current = null;
                      if (!draftParameterExpressionRef.current[field]) {
                        delete draftRawInputRef.current[field];
                      }
                    }}
                    onKeyDown={(event) => {
                      handleDraftDimensionKeyDown(event, field);
                    }}
                  />
                  {suggestions.length > 0 ? (
                    <div
                      className="absolute left-0 top-[calc(100%+0.35rem)] w-[220px] overflow-hidden rounded-lg border border-surface-high bg-surface-container py-1 text-left shadow-xl"
                      onMouseDown={(event) => event.preventDefault()}
                    >
                      {suggestions.map((suggestion, index) => (
                        <button
                          key={suggestion.name}
                          type="button"
                          className={`flex w-full items-center justify-between gap-3 px-3 py-1.5 text-xs ${
                            index === suggestionIndex
                              ? "bg-surface-bright text-on-surface"
                              : "text-on-surface-muted hover:bg-surface-high hover:text-on-surface"
                          }`}
                          onClick={() =>
                            insertDraftParameterSuggestion(field, suggestion.name)
                          }
                        >
                          <span className="min-w-0 truncate font-mono">
                            {suggestion.name}
                          </span>
                          <span className="shrink-0 text-[10px] uppercase tracking-[0.12em] text-on-surface-dim">
                            {suggestion.kind}
                          </span>
                        </button>
                      ))}
                    </div>
                  ) : null}
                </form>
              );
            })
          : null}
        {activeSketchPlaneId && isDrawableSketchTool(activeSketchTool) ? (
          <section className="pointer-events-auto cad-floating-panel absolute right-4 top-4 z-20 w-72 px-5 py-5">
            <p className="cad-kicker">{translate("common.sketchTool")}</p>
            <h2 className="cad-title mt-2">
              {isDrawableSketchTool(activeSketchTool)
                ? translate(sketchToolLabelKey(activeSketchTool))
                : translate("toolbar.line")}
            </h2>
            {activeSketchTool === "circle" && (
              <p className="text-xs text-on-surface/50 mt-1">
                {translate(`toolbar.circle${circleToolMode === "center_radius" ? "CenterRadius" : circleToolMode === "two_point" ? "TwoPoint" : circleToolMode === "three_point" ? "ThreePoint" : circleToolMode === "tangent_two_lines" ? "TangentTwoLines" : "TangentThreeLines"}`)}
              </p>
            )}
            {activeSketchTool === "rectangle" && (
              <p className="text-xs text-on-surface/50 mt-1">
                {translate(`toolbar.rectangle${rectangleToolMode === "corner_corner" ? "CornerCorner" : rectangleToolMode === "center_point" ? "CenterPoint" : "ThreePoint"}`)}
              </p>
            )}
            {activeSketchTool === "arc" && (
              <p className="text-xs text-on-surface/50 mt-1">
                {translate(arcToolMode === "three_point" ? "toolbar.arcThreePoint" : "toolbar.arcCenter")}
              </p>
            )}
            {activeSketchTool === "polygon" && (
              <p className="text-xs text-on-surface/50 mt-1">
                {translate(`toolbar.polygon${polygonToolMode === "circumscribed" ? "Circumscribed" : polygonToolMode === "inscribed" ? "Inscribed" : "Edge"}`)}
              </p>
            )}
            <div className="mt-5 flex flex-col gap-4">
              {isDrawableSketchTool(activeSketchTool) ? (
                <label className="flex items-center justify-between gap-4 text-sm text-on-surface">
                  <span>{translate("common.construction")}</span>
                  <Checkbox
                    checked={sketchToolConstruction}
                    ariaLabel={translate("common.construction")}
                    onCheckedChange={(checked) => {
                      sketchToolConstructionRef.current = checked;
                      setSketchToolConstruction(checked);
                    }}
                  />
                </label>
              ) : null}
              {activeSketchTool === "arc" ? (
                <div>
                  <p className="cad-kicker">{translate("viewport.mode")}</p>
                  <div className="mt-3 flex gap-2">
                    {[
                      { value: "three_point", label: translate("toolbar.arcThreePointTitle") },
                      { value: "center_start_end", label: translate("toolbar.arcCenter") },
                    ].map((option) => (
                      <button
                        key={option.value}
                        type="button"
                        className={
                          arcToolMode === option.value
                            ? "cad-action-primary flex-1"
                            : "cad-action-ghost flex-1"
                        }
                        onClick={() => {
                          onSetArcToolMode(
                            option.value as
                              | "three_point"
                              | "center_start_end",
                          );
                        }}
                      >
                        {option.label}
                      </button>
                    ))}
                  </div>
                </div>
              ) : null}
              {activeSketchTool === "circle" ? (
                <div>
                  <p className="cad-kicker">{translate("viewport.mode")}</p>
                  <div className="mt-3">
                    <Dropdown
                      value={circleToolMode}
                      options={[
                        { value: "center_radius", label: translate("toolbar.circleCenterRadius") },
                        { value: "two_point", label: translate("toolbar.circleTwoPoint") },
                        { value: "three_point", label: translate("toolbar.circleThreePoint") },
                        { value: "tangent_two_lines", label: translate("toolbar.circleTangentTwoLines") },
                        { value: "tangent_three_lines", label: translate("toolbar.circleTangentThreeLines") },
                      ]}
                      label={translate("viewport.mode")}
                      onChange={(value) => {
                        onSetCircleToolMode(value as
                          | "center_radius"
                          | "two_point"
                          | "three_point"
                          | "tangent_two_lines"
                          | "tangent_three_lines");
                      }}
                    />
                  </div>
                </div>
              ) : null}
              {activeSketchTool === "rectangle" ? (
                <div>
                  <p className="cad-kicker">{translate("viewport.mode")}</p>
                  <div className="mt-3">
                    <Dropdown
                      value={rectangleToolMode}
                      options={[
                        { value: "corner_corner", label: translate("toolbar.rectangleCornerCorner") },
                        { value: "center_point", label: translate("toolbar.rectangleCenterPoint") },
                        { value: "three_point", label: translate("toolbar.rectangleThreePoint") },
                      ]}
                      label={translate("viewport.mode")}
                      onChange={(value) => {
                        onSetRectangleToolMode(value as
                          | "corner_corner"
                          | "center_point"
                          | "three_point");
                      }}
                    />
                  </div>
                </div>
              ) : null}
              {activeSketchTool === "polygon" ? (
                <div>
                  <p className="cad-kicker">{translate("viewport.mode")}</p>
                  <div className="mt-3">
                    <Dropdown
                      value={polygonToolMode}
                      options={[
                        { value: "circumscribed", label: translate("toolbar.polygonCircumscribed") },
                        { value: "inscribed", label: translate("toolbar.polygonInscribed") },
                        { value: "edge", label: translate("toolbar.polygonEdge") },
                      ]}
                      label={translate("viewport.mode")}
                      onChange={(value) => {
                        onSetPolygonToolMode(value as
                          | "circumscribed"
                          | "inscribed"
                          | "edge");
                      }}
                    />
                    <div className="mt-3 flex items-center gap-2">
                      <span className="text-xs text-on-surface/60">Sides:</span>
                      <input
                        type="number"
                        min="3"
                        max="48"
                        step="1"
                        className="h-7 w-16 rounded-md border px-2 text-xs text-center tabular-nums bg-transparent"
                        style={{
                          border: "1px solid var(--cad-panel-border)",
                          color: "inherit",
                        }}
                        value={polygonSides}
                        onChange={(event) => {
                          const value = Math.max(3, Math.min(48, Number(event.target.value) || 3));
                          setPolygonSides(value);
                          polygonSidesRef.current = value;
                        }}
                      />
                    </div>
                  </div>
                </div>
              ) : null}
            </div>
          </section>
        ) : null}
        {/*
          Floating Dimension Tool hint panel. Active only while the
          dim tool is armed; updates from "click a line / circle" to
          "click second line for angle, or same line for length"
          after the first line is picked. Mirrors the Line Tool
          panel's placement so the user always knows where to look.
        */}
        {activeSketchPlaneId && activeSketchTool === "dimension" ? (
          <div className="cad-floating-panel pointer-events-auto absolute left-4 top-4 z-20 flex flex-col gap-1 px-3 py-2 text-xs">
            <p className="text-[10px] uppercase tracking-[0.18em] text-on-surface-dim">
              {translate("viewport.dimensionTool")}{" "}
              <span className="opacity-60">
                ({formatHotkey(config.hotkeys.sketchToolbar.dimension)})
              </span>
            </p>
            <p className="text-on-surface">
              {dimensionToolFirstLine === null ? (
                <>{translate("viewport.placeDimension")}</>
              ) : (
                <>
                  {translate("viewport.dimensionReady")}
                </>
              )}
            </p>
          </div>
        ) : null}
        {selectedSketchDimension &&
        activeSketchPlaneId &&
        isDimensionEditorOpen ? (
          <form
            ref={dimensionEditorRef}
            className="pointer-events-none absolute z-20 flex w-[172px] items-center gap-1 rounded-md border px-2 py-1 backdrop-blur-md"
            style={{
              left: 0,
              top: 0,
              opacity: 0,
              background: "var(--cad-dimension-editor-bg)",
              borderColor: "var(--cad-dimension-editor-border)",
              boxShadow: "0 4px 12px var(--cad-dimension-editor-shadow)",
            }}
            onSubmit={(event) => {
              event.preventDefault();
              void handleSubmitDimensionEdit();
            }}
          >
            <input
              ref={dimensionInputRef}
              className="h-6 min-w-0 flex-1 bg-transparent text-center text-sm font-medium text-on-surface tabular-nums outline-none pointer-events-none"
              type="text"
              inputMode="text"
              value={dimensionDraftValue}
              onChange={(event) => {
                dimensionInputSelectionLockedRef.current = false;
                handleDimensionDraftChange(event.target.value);
              }}
              onFocus={(event) => {
                if (dimensionInputSelectionLockedRef.current) {
                  event.currentTarget.select();
                }
              }}
              onKeyDown={(event) => {
                dimensionInputSelectionLockedRef.current = false;
                if (
                  dimensionParameterSuggestions.length > 0 &&
                  (event.key === "ArrowDown" || event.key === "ArrowUp")
                ) {
                  event.preventDefault();
                  setDimensionSuggestionIndex((current) => {
                    const delta = event.key === "ArrowDown" ? 1 : -1;
                    return (
                      current +
                      delta +
                      dimensionParameterSuggestions.length
                    ) % dimensionParameterSuggestions.length;
                  });
                  return;
                }
                if (
                  dimensionParameterSuggestions.length > 0 &&
                  (event.key === "Tab" || event.key === "Enter")
                ) {
                  event.preventDefault();
                  const suggestion =
                    dimensionParameterSuggestions[dimensionSuggestionIndex] ??
                    dimensionParameterSuggestions[0];
                  insertDimensionParameterSuggestion(suggestion.name);
                  return;
                }
                if (event.key === "Escape") {
                  event.preventDefault();
                  // During placement, the dimension hasn't been committed
                  // yet — delete it entirely, matching the global Escape
                  // handler's behaviour when the input isn't focused.
                  const drag = dimensionLabelDragRef.current;
                  if (drag?.isPlacement && drag.dimensionId) {
                    const dimId = drag.dimensionId;
                    dimensionLabelDragRef.current = null;
                    dimensionPlacementOriginalPositionRef.current = null;
                    if (controlsRef.current) controlsRef.current.enabled = true;
                    setCanvasCursor("");
                    setIsDimensionEditorOpen(false);
                    dimensionEditOriginalValueRef.current = null;
                    void deleteSketchDimensionRef.current(dimId);
                  } else {
                    cancelDimensionEdit();
                  }
                }
              }}
            />

            {dimensionParameterSuggestions.length > 0 ? (
              <div
                className="pointer-events-auto absolute left-0 top-[calc(100%+0.35rem)] w-[220px] overflow-hidden rounded-lg border border-surface-high bg-surface-container py-1 text-left shadow-xl"
                onMouseDown={(event) => event.preventDefault()}
              >
                {dimensionParameterSuggestions.map((suggestion, index) => (
                  <button
                    key={suggestion.name}
                    type="button"
                    className={`flex w-full items-center justify-between gap-3 px-3 py-1.5 text-xs ${
                      index === dimensionSuggestionIndex
                        ? "bg-surface-bright text-on-surface"
                        : "text-on-surface-muted hover:bg-surface-high hover:text-on-surface"
                    }`}
                    onClick={() =>
                      insertDimensionParameterSuggestion(suggestion.name)
                    }
                  >
                    <span className="min-w-0 truncate font-mono">
                      {suggestion.name}
                    </span>
                    <span className="shrink-0 text-[10px] uppercase tracking-[0.12em] text-on-surface-dim">
                      {suggestion.kind}
                    </span>
                  </button>
                ))}
              </div>
            ) : null}
          </form>
        ) : null}
        {!hasActiveDocument ? (
          <div
            className="absolute inset-0 flex items-center justify-center backdrop-blur-sm"
            style={{ background: "var(--cad-overlay-strong)" }}
          >
            <div className="text-center">
              <p className="cad-kicker">{translate("viewport.title")}</p>
              <p className="mt-4 text-sm text-on-surface-muted">
                {translate("viewport.noActiveDocument")}
              </p>
            </div>
          </div>
        ) : null}
        {status === "starting" ? (
          <div
            className="pointer-events-none absolute inset-0 flex items-center justify-center backdrop-blur-sm"
            style={{ background: "var(--cad-overlay-soft)" }}
          >
            <div className="cad-floating-panel flex min-w-[220px] items-center gap-4 px-5 py-4">
              <span className="cad-loader-spinner" aria-hidden="true" />
              <div>
                <p className="cad-kicker">{translate("viewport.coreStartup")}</p>
                <p className="mt-2 text-sm text-on-surface-muted">
                  {translate("viewport.startingCore")}
                </p>
              </div>
            </div>
          </div>
        ) : null}
        {hasActiveDocument ? (
          <>
            <div className="pointer-events-none absolute bottom-4 right-4 cad-floating-panel px-4 py-3 text-right">
              <p className="text-[11px] uppercase tracking-[0.18em] text-on-surface-dim">
                {translate("common.selection")}
              </p>
              <p className="mt-1 text-sm text-on-surface-muted">
                {selectedReference?.label ??
                  selectedPrimitiveLabel ??
                  translate("viewport.noSelection")}
              </p>
              {measurementText ? (
                <p className="mt-1 text-sm text-primary-soft">
                  {measurementText}
                </p>
              ) : null}
              <p className="mt-1 text-xs uppercase tracking-[0.14em] text-on-surface-dim">
                {/* Plane / face ids are internal — see
                    AGENTS.md UI Copy Rules. The status line just
                    reports the active tool and entity counts. */}
                {activeSketchPlaneId
                  ? translate("viewport.sketchStatus", {
                      tool: activeSketchTool,
                      lineCount,
                      linePlural: lineCount === 1 ? "" : "s",
                      circleCount,
                      circlePlural: circleCount === 1 ? "" : "s",
                    })
                  : translate("viewport.noActiveSketch")}
              </p>
              {activeSketchPlaneId ? (
                <>
                  <p className="mt-1 text-xs text-on-surface-dim">
                    {/* Status text — never embed internal ids; see
                        AGENTS.md "UI Copy Rules". The selection
                        details (entity / point / dimension /
                        profile) just acknowledge that something is
                        selected; specifics live in the floating
                        panels keyed off those selections. */}
                    {armedSketchConstraint
                      ? armedSketchConstraint.kind === "coincident"
                        ? armedSketchConstraint.firstPointId
                          ? translate("constraints.coincidentSecondPoint")
                          : translate("constraints.coincidentFirstPoint")
                        : armedSketchConstraint.kind === "equal_length" ||
                            armedSketchConstraint.kind === "perpendicular" ||
                            armedSketchConstraint.kind === "parallel"
                          ? armedSketchConstraint.firstLineId
                            ? translate("constraints.lineSecond", {
                                label:
                                  armedSketchConstraint.kind === "equal_length"
                                    ? translate("toolbar.equalLength")
                                    : armedSketchConstraint.kind ===
                                        "perpendicular"
                                      ? translate("toolbar.perpendicular")
                                      : translate("toolbar.parallel"),
                              })
                            : translate("constraints.lineFirst", {
                                label:
                                  armedSketchConstraint.kind === "equal_length"
                                    ? translate("toolbar.equalLength")
                                    : armedSketchConstraint.kind ===
                                        "perpendicular"
                                      ? translate("toolbar.perpendicular")
                                      : translate("toolbar.parallel"),
                              })
                          : translate("constraints.lineConstraint", {
                              kind: armedSketchConstraint.kind,
                            })
                      : document?.selected_sketch_entity_id
                        ? (
                            (document?.selected_sketch_dimension_id
                              ? translate("viewport.dimensionSelected")
                              : (selectedEntityDof
                                ? translate("viewport.entitySelectedDof", {
                                    entity: selectedEntityDof.entity_kind,
                                    dof: selectedEntityDof.total_dof,
                                    consumed: selectedEntityDof.consumed_dof,
                                    status: selectedEntityDof.status === "over"
                                      ? translate("viewport.dofOver")
                                      : selectedEntityDof.status === "full"
                                        ? translate("viewport.dofFull") : "",
                                  })
                                : translate("viewport.entitySelected"))))
                        : document?.selected_sketch_point_id
                          ? translate("viewport.pointSelected")
                          : document?.selected_sketch_profile_id
                            ? translate("viewport.profileSelected")
                            : selectedConstraint
                              ? translate("viewport.constraintSelected", { kind: selectedConstraint.kind })
                              : sketchSnapLabel
                              ? translate("viewport.snap", { label: sketchSnapLabel })
                              : activeSketchTool === "select"
                                ? translate("viewport.selectionMode")
                                : activeSketchTool === "project"
                                  ? translate("viewport.projectPrompt")
                                  : activeSketchTool === "line" &&
                                      lineDraftStartRef.current
                                    ? translate("viewport.lineChainActive")
                                    : translate("viewport.clickPlaceGeometry")}
                </p>
                  <button
                    type="button"
                    className="pointer-events-auto mt-3 ml-auto flex cad-ribbon-action cad-ribbon-action-primary"
                    disabled={status !== "connected"}
                    onClick={() => {
                      void onFinishSketch();
                    }}
                  >
                    {translate("toolbar.finishSketch")}
                  </button>
                </>
              ) : null}
            </div>
          </>
        ) : null}
      </div>
    </section>
  );
}
