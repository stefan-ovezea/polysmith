import * as THREE from "three";

import type { SketchDimensionScene, SketchFeatureParameters } from "@/types";
import { resolveSketchPlanePoint, toWorldPoint } from "@/utils";
import type { ActiveSketchGridPlaneFrame } from "./grid";
import { getSketchGridFrame } from "./grid";
import {
  clampAngleRadius,
  type DimensionLabelDragState,
  type DimensionRelationPreview,
} from "./draftDimensions";

export interface AngleDimensionFrame {
  pivot: THREE.Vector3;
  startUnit: THREE.Vector3;
  endUnit: THREE.Vector3;
  bisector: THREE.Vector3;
  anchorRadius: number;
  dimensionRadius: number;
}

export interface CircleRadiusDimensionProjection {
  center: THREE.Vector3;
  radius: number;
  direction: THREE.Vector3;
}

export interface DimensionPlacementStart {
  originalPosition: [number, number, number];
  nextPosition: [number, number, number];
  isAngleKind: boolean;
  angleWorldPoint: [number, number, number] | null;
  dragState: DimensionLabelDragState;
}

interface MutableRef<T> {
  current: T;
}

type SketchDimensionHit = {
  kind: "sketch_dimension";
  id: string;
  part?: string;
};

type SketchLineEndpoint = {
  vertexId: string;
  local: [number, number];
};

function dimensionLabelDragHasMoved(
  event: PointerEvent,
  drag: DimensionLabelDragState,
  threshold = 4,
) {
  const dx = event.clientX - drag.startClientX;
  const dy = event.clientY - drag.startClientY;
  return Math.abs(dx) > threshold || Math.abs(dy) > threshold;
}

function angleDimensionDragRadius(
  worldPoint: [number, number, number],
  frame: AngleDimensionFrame,
) {
  return clampAngleRadius(
    new THREE.Vector3(...worldPoint).distanceTo(frame.pivot),
  );
}

export function buildAngleDimensionFrame({
  dimension,
  sketchParameters,
}: {
  dimension: SketchDimensionScene;
  sketchParameters: SketchFeatureParameters | null | undefined;
}): AngleDimensionFrame | null {
  const coreDimension = sketchParameters?.dimensions.find(
    (candidate) => candidate.dimension_id === dimension.dimensionId,
  );
  if (
    sketchParameters &&
    coreDimension?.kind === "angle" &&
    coreDimension.secondary_entity_id
  ) {
    const frame = angleDimensionFrameFromSketchLines({
      dimension,
      sketchParameters,
      primaryLineId: coreDimension.entity_id,
      secondaryLineId: coreDimension.secondary_entity_id,
    });
    if (frame) {
      return frame;
    }
  }

  return angleDimensionFrameFromArcCenter(dimension);
}

function angleDimensionFrameFromSketchLines({
  dimension,
  sketchParameters,
  primaryLineId,
  secondaryLineId,
}: {
  dimension: SketchDimensionScene;
  sketchParameters: SketchFeatureParameters;
  primaryLineId: string;
  secondaryLineId: string;
}): AngleDimensionFrame | null {
  const lineA = sketchParameters.lines.find(
    (line) => line.line_id === primaryLineId,
  );
  const lineB = sketchParameters.lines.find(
    (line) => line.line_id === secondaryLineId,
  );
  if (!lineA || !lineB) {
    return null;
  }

  const aEnds: SketchLineEndpoint[] = [
    { vertexId: lineA.start_vertex_id, local: [lineA.start_x, lineA.start_y] },
    { vertexId: lineA.end_vertex_id, local: [lineA.end_x, lineA.end_y] },
  ];
  const bEnds: SketchLineEndpoint[] = [
    { vertexId: lineB.start_vertex_id, local: [lineB.start_x, lineB.start_y] },
    { vertexId: lineB.end_vertex_id, local: [lineB.end_x, lineB.end_y] },
  ];

  const pivot = matchingLinePivot(aEnds, bEnds);
  if (!pivot) {
    return null;
  }

  const pivotLocal = aEnds[pivot.aIndex].local;
  const aOther = aEnds[1 - pivot.aIndex].local;
  const bOther = bEnds[1 - pivot.bIndex].local;
  return angleDimensionFrameFromLocalRays({
    dimension,
    planeFrame: sketchParameters.plane_frame,
    pivotLocal,
    aOther,
    bOther,
  });
}

function matchingLinePivot(
  aEnds: readonly SketchLineEndpoint[],
  bEnds: readonly SketchLineEndpoint[],
): { aIndex: number; bIndex: number } | null {
  for (let aIndex = 0; aIndex < aEnds.length; aIndex++) {
    for (let bIndex = 0; bIndex < bEnds.length; bIndex++) {
      const samePointId = aEnds[aIndex].vertexId === bEnds[bIndex].vertexId;
      const dx = aEnds[aIndex].local[0] - bEnds[bIndex].local[0];
      const dy = aEnds[aIndex].local[1] - bEnds[bIndex].local[1];
      if (samePointId || Math.hypot(dx, dy) <= 0.05) {
        return { aIndex, bIndex };
      }
    }
  }
  return null;
}

function angleDimensionFrameFromLocalRays({
  dimension,
  planeFrame,
  pivotLocal,
  aOther,
  bOther,
}: {
  dimension: SketchDimensionScene;
  planeFrame: SketchFeatureParameters["plane_frame"];
  pivotLocal: [number, number];
  aOther: [number, number];
  bOther: [number, number];
}): AngleDimensionFrame | null {
  const aDx = aOther[0] - pivotLocal[0];
  const aDy = aOther[1] - pivotLocal[1];
  const bDx = bOther[0] - pivotLocal[0];
  const bDy = bOther[1] - pivotLocal[1];
  const aLength = Math.hypot(aDx, aDy);
  const bLength = Math.hypot(bDx, bDy);
  if (aLength <= 1e-8 || bLength <= 1e-8) {
    return null;
  }

  const pivot = new THREE.Vector3(
    ...toWorldPoint(dimension.planeId, pivotLocal, planeFrame),
  );
  const aUnitPoint = new THREE.Vector3(
    ...toWorldPoint(
      dimension.planeId,
      [pivotLocal[0] + aDx / aLength, pivotLocal[1] + aDy / aLength],
      planeFrame,
    ),
  );
  const bUnitPoint = new THREE.Vector3(
    ...toWorldPoint(
      dimension.planeId,
      [pivotLocal[0] + bDx / bLength, pivotLocal[1] + bDy / bLength],
      planeFrame,
    ),
  );
  const startUnit = aUnitPoint.sub(pivot).normalize();
  const endUnit = bUnitPoint.sub(pivot).normalize();
  const bisector = startUnit.clone().add(endUnit);
  if (bisector.lengthSq() <= 1e-8) {
    return null;
  }

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

function angleDimensionFrameFromArcCenter(
  dimension: SketchDimensionScene,
): AngleDimensionFrame | null {
  if (!dimension.arcCenter) {
    return null;
  }

  const pivot = new THREE.Vector3(...dimension.arcCenter);
  const startUnit = new THREE.Vector3(...dimension.dimensionStart)
    .sub(pivot)
    .normalize();
  const endUnit = new THREE.Vector3(...dimension.dimensionEnd)
    .sub(pivot)
    .normalize();
  const bisector = startUnit.clone().add(endUnit);
  if (bisector.lengthSq() <= 1e-8) {
    return null;
  }

  const anchorRadius = new THREE.Vector3(...dimension.anchorEnd).distanceTo(
    pivot,
  );
  const dimensionRadius = new THREE.Vector3(
    ...dimension.dimensionStart,
  ).distanceTo(pivot);
  return {
    pivot,
    startUnit,
    endUnit,
    bisector: bisector.normalize(),
    anchorRadius,
    dimensionRadius,
  };
}

export function beginDimensionLabelDragPointerDown({
  event,
  renderer,
  camera,
  controls,
  hit,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  dimensions,
  suppressNextDimensionEditorOpenRef,
  dimensionLabelDragRef,
  setIsDimensionEditorOpen,
  selectSketchDimension,
  setAngleDimensionDragRadius,
  getDimensionPlacementAxis,
}: {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  controls: { enabled: boolean };
  hit: SketchDimensionHit | null;
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: ActiveSketchGridPlaneFrame | null;
  dimensions: readonly SketchDimensionScene[];
  suppressNextDimensionEditorOpenRef: MutableRef<boolean>;
  dimensionLabelDragRef: MutableRef<DimensionLabelDragState | null>;
  setIsDimensionEditorOpen: (open: boolean) => void;
  selectSketchDimension: (dimensionId: string) => Promise<void>;
  setAngleDimensionDragRadius: (
    dimension: SketchDimensionScene,
    dimensionId: string,
    worldPoint: readonly [number, number, number],
  ) => void;
  getDimensionPlacementAxis: (
    dimension: SketchDimensionScene,
  ) => THREE.Vector3 | null;
}) {
  if (!hit || !activeSketchPlaneId) {
    return false;
  }

  suppressNextDimensionEditorOpenRef.current = true;
  setIsDimensionEditorOpen(false);
  void selectSketchDimension(hit.id);

  const dimension = dimensions.find((entry) => entry.dimensionId === hit.id);
  const sketchPoint = resolveSketchPlanePoint(
    event,
    renderer,
    camera,
    activeSketchPlaneId,
    activeSketchPlaneFrame,
  );
  if (!dimension || !sketchPoint) {
    return false;
  }

  if (dimension.kind === "angle" || dimension.kind === "line_angle") {
    setAngleDimensionDragRadius(dimension, hit.id, sketchPoint.world);
  }

  const dragAxis =
    dimension.kind === "circle_radius" || dimension.kind === "arc_radius"
      ? new THREE.Vector3(0, 0, 0)
      : getDimensionPlacementAxis(dimension);
  if (dimension.kind !== "circle_radius" && dimension.kind !== "arc_radius" && !dragAxis) {
    return true;
  }

  dimensionLabelDragRef.current = {
    dimensionId: hit.id,
    hitPart: hit.part === "label" ? "label" : "geometry",
    startClientX: event.clientX,
    startClientY: event.clientY,
    startWorld: sketchPoint.world,
    startLabelPosition: dimension.labelPosition,
    dragAxis: dragAxis ? [dragAxis.x, dragAxis.y, dragAxis.z] : [0, 0, 0],
    hasMoved: false,
  };
  controls.enabled = false;
  (renderer.domElement as HTMLCanvasElement).style.cursor = "grabbing";
  return true;
}

export function buildDimensionPlacementStart({
  event,
  renderer,
  camera,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  dimension,
  relationPosition,
  relation,
  getDimensionPlacementAxis,
}: {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  activeSketchPlaneId: string;
  activeSketchPlaneFrame: ActiveSketchGridPlaneFrame | null;
  dimension: SketchDimensionScene;
  relationPosition: [number, number, number] | null;
  relation?: DimensionRelationPreview | null;
  getDimensionPlacementAxis: (
    dimension: SketchDimensionScene,
  ) => THREE.Vector3 | null;
}): DimensionPlacementStart | null {
  const sketchPoint = resolveSketchPlanePoint(
    event,
    renderer,
    camera,
    activeSketchPlaneId,
    activeSketchPlaneFrame,
  );
  if (!sketchPoint) {
    return null;
  }

  const isAngleKind =
    dimension.kind === "angle" || dimension.kind === "line_angle";
  const originalPosition = dimension.labelPosition;
  const isCircleOrArc = dimension.kind === "circle_radius" || dimension.kind === "arc_radius";
  const circlePosition =
    isCircleOrArc
      ? circleDimensionLabelNearPoint({
          dimension,
          worldPoint: sketchPoint.world,
          planeFrame: activeSketchPlaneFrame,
        })
      : null;
  const dragAxis =
    isCircleOrArc
      ? new THREE.Vector3(0, 0, 0)
      : isAngleKind
      ? null
      : getDimensionPlacementAxis(dimension);
  if (!isAngleKind && !isCircleOrArc && !dragAxis) {
    return null;
  }

  const nextPosition =
    relationPosition ??
    circlePosition ??
    (isAngleKind
      ? originalPosition
      : constrainedDimensionPlacementPosition({
          originalPosition,
          worldPoint: sketchPoint.world,
          dragAxis,
        }));

  return {
    originalPosition,
    nextPosition,
    isAngleKind,
    angleWorldPoint: isAngleKind ? sketchPoint.world : null,
    dragState: {
      dimensionId: dimension.dimensionId,
      startClientX: event.clientX,
      startClientY: event.clientY,
      startWorld: sketchPoint.world,
      startLabelPosition: nextPosition,
      dragAxis: dragAxis ? [dragAxis.x, dragAxis.y, dragAxis.z] : [0, 0, 0],
      hasMoved: true,
      isPlacement: true,
      anglePlacementRelation:
        isAngleKind && relation?.kind === "line_angle" ? relation : undefined,
    },
  };
}

function constrainedDimensionPlacementPosition({
  originalPosition,
  worldPoint,
  dragAxis,
}: {
  originalPosition: [number, number, number];
  worldPoint: [number, number, number];
  dragAxis: THREE.Vector3 | null;
}): [number, number, number] {
  if (!dragAxis) {
    return originalPosition;
  }

  const originalPositionVector = new THREE.Vector3(...originalPosition);
  const pointerDelta = new THREE.Vector3(...worldPoint).sub(
    originalPositionVector,
  );
  const nextPositionVector = originalPositionVector
    .clone()
    .add(dragAxis.clone().multiplyScalar(pointerDelta.dot(dragAxis)));
  return [nextPositionVector.x, nextPositionVector.y, nextPositionVector.z];
}

export function circleDimensionLabelNearPoint({
  dimension,
  worldPoint,
  planeFrame,
}: {
  dimension: SketchDimensionScene;
  worldPoint: [number, number, number];
  planeFrame: ActiveSketchGridPlaneFrame | null;
}): [number, number, number] | null {
  if (dimension.kind !== "circle_radius" && dimension.kind !== "arc_radius") {
    return null;
  }
  const projection = circleRadiusDimensionProjection({
    dimension,
    worldPoint,
    planeFrame,
  });
  if (!projection) {
    return null;
  }
  const position = projection.center.add(
    projection.direction.multiplyScalar(projection.radius + 4),
  );
  return [position.x, position.y, position.z];
}

export function circleRadiusDimensionProjection({
  dimension,
  worldPoint,
  planeFrame,
}: {
  dimension: SketchDimensionScene;
  worldPoint: [number, number, number];
  planeFrame: ActiveSketchGridPlaneFrame | null;
}): CircleRadiusDimensionProjection | null {
  // circle_radius: dimensionStart = left edge, dimensionEnd = right edge → center = midpoint
  // arc_radius:   dimensionStart = center,       dimensionEnd = point on arc → center = dimStart
  const isArcRadius = dimension.kind === "arc_radius";
  const center = isArcRadius
    ? new THREE.Vector3(...dimension.dimensionStart)
    : new THREE.Vector3(...dimension.dimensionStart)
        .add(new THREE.Vector3(...dimension.dimensionEnd))
        .multiplyScalar(0.5);
  const radius = isArcRadius
    ? new THREE.Vector3(...dimension.dimensionStart).distanceTo(
        new THREE.Vector3(...dimension.dimensionEnd))
    : new THREE.Vector3(...dimension.dimensionStart).distanceTo(
        new THREE.Vector3(...dimension.dimensionEnd)) * 0.5;
  const direction = new THREE.Vector3(...worldPoint).sub(center);
  const planeNormal = getSketchGridFrame(dimension.planeId, planeFrame).normal;
  direction.addScaledVector(planeNormal, -direction.dot(planeNormal));
  if (direction.lengthSq() <= 1e-8 || radius <= 1e-8) {
    return null;
  }
  return { center, radius, direction: direction.normalize() };
}

function constrainedDimensionLabelPosition(
  drag: DimensionLabelDragState,
  worldPoint: [number, number, number],
): [number, number, number] {
  const dragAxis = new THREE.Vector3(...drag.dragAxis);
  const worldDelta = new THREE.Vector3(
    worldPoint[0] - drag.startWorld[0],
    worldPoint[1] - drag.startWorld[1],
    worldPoint[2] - drag.startWorld[2],
  );
  const constrainedDelta = dragAxis.multiplyScalar(worldDelta.dot(dragAxis));
  const nextPositionVector = new THREE.Vector3(
    ...drag.startLabelPosition,
  ).add(constrainedDelta);
  return [nextPositionVector.x, nextPositionVector.y, nextPositionVector.z];
}

export type DimensionLabelDragMoveUpdate =
  | {
      kind: "angle_radius";
      dimensionId: string;
      radius: number | null;
    }
  | {
      kind: "label_position";
      dimensionId: string;
      position: [number, number, number];
    };

export function dimensionLabelDragMoveUpdate({
  event,
  drag,
  dimension,
  worldPoint,
  planeFrame,
  angleFrameForDimension,
}: {
  event: PointerEvent;
  drag: DimensionLabelDragState;
  dimension: SketchDimensionScene | undefined;
  worldPoint: [number, number, number];
  planeFrame: ActiveSketchGridPlaneFrame | null;
  angleFrameForDimension: (
    dimension: SketchDimensionScene,
  ) => AngleDimensionFrame | null;
}): DimensionLabelDragMoveUpdate {
  if (dimensionLabelDragHasMoved(event, drag)) {
    drag.hasMoved = true;
  }

  if (dimension?.kind === "angle" || dimension?.kind === "line_angle") {
    const frame = angleFrameForDimension(dimension);
    return {
      kind: "angle_radius",
      dimensionId: drag.dimensionId,
      radius: frame ? angleDimensionDragRadius(worldPoint, frame) : null,
    };
  }

  if (dimension?.kind === "circle_radius" || dimension?.kind === "arc_radius") {
    const nextPosition = circleDimensionLabelNearPoint({
      dimension,
      worldPoint,
      planeFrame,
    });
    if (nextPosition) {
      return {
        kind: "label_position",
        dimensionId: drag.dimensionId,
        position: nextPosition,
      };
    }
  }

  return {
    kind: "label_position",
    dimensionId: drag.dimensionId,
    position: constrainedDimensionLabelPosition(drag, worldPoint),
  };
}

export function handleDimensionLabelDragPointerMove({
  event,
  renderer,
  camera,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  dimensionLabelDragRef,
  dimensions,
  angleDragRadiiRef,
  setAngleDragRadii,
  updateDimensionRelationPreview,
  updateAngleDimensionPlacementPreview,
  angleFrameForDimension,
  setDimensionLabelPosition,
}: {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: ActiveSketchGridPlaneFrame | null;
  dimensionLabelDragRef: MutableRef<DimensionLabelDragState | null>;
  dimensions: readonly SketchDimensionScene[];
  angleDragRadiiRef: MutableRef<Record<string, number>>;
  setAngleDragRadii: (
    update:
      | Record<string, number>
      | ((current: Record<string, number>) => Record<string, number>),
  ) => void;
  updateDimensionRelationPreview: (localPoint: [number, number]) => unknown;
  updateAngleDimensionPlacementPreview?: (
    drag: DimensionLabelDragState,
    localPoint: [number, number],
    worldPoint: [number, number, number],
  ) => boolean;
  angleFrameForDimension: (
    dimension: SketchDimensionScene,
  ) => AngleDimensionFrame | null;
  setDimensionLabelPosition: (
    dimensionId: string,
    position: [number, number, number],
  ) => void;
}) {
  const dimensionDrag = dimensionLabelDragRef.current;
  if (!dimensionDrag || !activeSketchPlaneId) {
    return false;
  }

  const sketchPoint = resolveSketchPlanePoint(
    event,
    renderer,
    camera,
    activeSketchPlaneId,
    activeSketchPlaneFrame,
  );
  if (!sketchPoint) {
    return true;
  }

  if (
    dimensionDrag.isPlacement &&
    updateAngleDimensionPlacementPreview?.(
      dimensionDrag,
      sketchPoint.local,
      sketchPoint.world,
    )
  ) {
    dimensionDrag.hasMoved = true;
    return true;
  }

  if (
    dimensionDrag.isPlacement &&
    updateDimensionRelationPreview(sketchPoint.local)
  ) {
    dimensionDrag.hasMoved = true;
    return true;
  }

  const draggedDimension = dimensions.find(
    (dimension) => dimension.dimensionId === dimensionDrag.dimensionId,
  );
  const update = dimensionLabelDragMoveUpdate({
    event,
    drag: dimensionDrag,
    dimension: draggedDimension,
    worldPoint: sketchPoint.world,
    planeFrame: activeSketchPlaneFrame,
    angleFrameForDimension,
  });

  if (update.kind === "angle_radius") {
    const radius = update.radius;
    if (radius !== null) {
      angleDragRadiiRef.current = {
        ...angleDragRadiiRef.current,
        [update.dimensionId]: radius,
      };
      setAngleDragRadii((prev) => ({
        ...prev,
        [update.dimensionId]: radius,
      }));
    }
    return true;
  }

  setDimensionLabelPosition(update.dimensionId, update.position);
  return true;
}
