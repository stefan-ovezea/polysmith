import type { Dispatch, SetStateAction } from "react";
import * as THREE from "three";
import type { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";

import type { SketchDimensionScene } from "@/types";
import { worldPointToSketchLocal } from "./grid";
import type { ActiveSketchGridPlaneFrame } from "./grid";
import {
  buildDimensionPlacementStart,
  type AngleDimensionFrame,
} from "./dimensionLabelDrag";
import {
  clampAngleRadius,
  type DimensionLabelDragState,
  type DimensionRelationPreview,
} from "./draftDimensions";
import { getSketchGridFrame } from "./grid";

interface MutableRef<T> {
  current: T;
}

interface DimensionPlacementActionsContext {
  rendererRef: MutableRef<THREE.WebGLRenderer | null>;
  cameraRef: MutableRef<THREE.OrthographicCamera | null>;
  controlsRef: MutableRef<OrbitControls | null>;
  activeSketchPlaneIdRef: MutableRef<string | null>;
  activeSketchPlaneFrameRef: MutableRef<ActiveSketchGridPlaneFrame | null>;
  lastPointerEventRef: MutableRef<PointerEvent | null>;
  dimensionLabelDragRef: MutableRef<DimensionLabelDragState | null>;
  dimensionPlacementOriginalPositionRef: MutableRef<
    [number, number, number] | null
  >;
  pendingRelationPlacementLabelRef: MutableRef<
    [number, number, number] | null
  >;
  dimensionLabelPositionsRef: MutableRef<
    Record<string, [number, number, number]>
  >;
  setDimensionLabelPositions: Dispatch<
    SetStateAction<Record<string, [number, number, number]>>
  >;
  angleDragRadiiRef: MutableRef<Record<string, number>>;
  setAngleDragRadii: Dispatch<SetStateAction<Record<string, number>>>;
  anglePlacementPreviewsRef: MutableRef<Record<string, SketchDimensionScene>>;
  setAnglePlacementPreviews: Dispatch<
    SetStateAction<Record<string, SketchDimensionScene>>
  >;
  anglePlacementPreviewValuesRef: MutableRef<Record<string, number>>;
  displayedSketchDimensionsRef: MutableRef<SketchDimensionScene[]>;
  updateSketchDimensionRef: MutableRef<
    (dimensionId: string, value: number | string) => Promise<void>
  >;
  updateSketchDimensionLabelPositionRef: MutableRef<
    (dimensionId: string, labelX: number, labelY: number) => Promise<void>
  >;
  angleDimensionFrame: (
    dimension: SketchDimensionScene,
  ) => AngleDimensionFrame | null;
  clearPreviewDimension: () => void;
  setCanvasCursor: (cursor: string) => void;
}

export function createDimensionPlacementActions({
  rendererRef,
  cameraRef,
  controlsRef,
  activeSketchPlaneIdRef,
  activeSketchPlaneFrameRef,
  lastPointerEventRef,
  dimensionLabelDragRef,
  dimensionPlacementOriginalPositionRef,
  pendingRelationPlacementLabelRef,
  dimensionLabelPositionsRef,
  setDimensionLabelPositions,
  angleDragRadiiRef,
  setAngleDragRadii,
  anglePlacementPreviewsRef,
  setAnglePlacementPreviews,
  anglePlacementPreviewValuesRef,
  displayedSketchDimensionsRef,
  updateSketchDimensionRef,
  updateSketchDimensionLabelPositionRef,
  angleDimensionFrame,
  clearPreviewDimension,
  setCanvasCursor,
}: DimensionPlacementActionsContext) {
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

  function clearAnglePlacementPreview(dimensionId: string) {
    if (anglePlacementPreviewsRef.current[dimensionId] !== undefined) {
      const nextPreviews = { ...anglePlacementPreviewsRef.current };
      delete nextPreviews[dimensionId];
      anglePlacementPreviewsRef.current = nextPreviews;
      setAnglePlacementPreviews(nextPreviews);
    }
    if (anglePlacementPreviewValuesRef.current[dimensionId] !== undefined) {
      const nextValues = { ...anglePlacementPreviewValuesRef.current };
      delete nextValues[dimensionId];
      anglePlacementPreviewValuesRef.current = nextValues;
    }
  }

  function setAngleDimensionDragRadius(
    dimension: SketchDimensionScene,
    dimensionId: string,
    worldPoint: readonly [number, number, number],
  ) {
    const frame = angleDimensionFrame(dimension);
    if (!frame) {
      return;
    }
    const radius = clampAngleRadius(
      new THREE.Vector3(worldPoint[0], worldPoint[1], worldPoint[2]).distanceTo(
        frame.pivot,
      ),
    );
    angleDragRadiiRef.current = {
      ...angleDragRadiiRef.current,
      [dimensionId]: radius,
    };
    setAngleDragRadii((prev) => ({
      ...prev,
      [dimensionId]: radius,
    }));
  }

  function persistAngleDimensionLabelRadius(
    dimensionId: string,
    dragRadius: number,
  ) {
    const dragged = displayedSketchDimensionsRef.current.find(
      (dimension) => dimension.dimensionId === dimensionId,
    );
    const frame = dragged ? angleDimensionFrame(dragged) : null;
    if (!frame) {
      return;
    }
    const labelWorld = frame.pivot
      .clone()
      .add(frame.bisector.clone().multiplyScalar(dragRadius));
    persistDimensionLabelPosition(dimensionId, [
      labelWorld.x,
      labelWorld.y,
      labelWorld.z,
    ]);
  }

  function persistDimensionDragLabelPosition(
    dimensionDrag: DimensionLabelDragState,
  ) {
    const anglePlacementPreview =
      anglePlacementPreviewsRef.current[dimensionDrag.dimensionId];
    const anglePlacementValue =
      anglePlacementPreviewValuesRef.current[dimensionDrag.dimensionId];
    if (
      anglePlacementPreview &&
      anglePlacementValue !== undefined &&
      dimensionDrag.anglePlacementRelation?.kind === "line_angle"
    ) {
      const labelPosition = anglePlacementPreview.labelPosition;
      void updateSketchDimensionRef
        .current(dimensionDrag.dimensionId, anglePlacementValue)
        .then(() => {
          persistDimensionLabelPosition(dimensionDrag.dimensionId, labelPosition);
        })
        .catch(() => {});
      clearAnglePlacementPreview(dimensionDrag.dimensionId);
      return;
    }

    const dragRadius = angleDragRadiiRef.current[dimensionDrag.dimensionId];
    if (dragRadius !== undefined) {
      persistAngleDimensionLabelRadius(dimensionDrag.dimensionId, dragRadius);
      return;
    }
    persistDimensionLabelPosition(
      dimensionDrag.dimensionId,
      dimensionLabelPositionsRef.current[dimensionDrag.dimensionId],
    );
  }

  function finishDimensionPlacement() {
    const dimensionDrag = dimensionLabelDragRef.current;
    if (!dimensionDrag?.isPlacement) {
      return false;
    }
    persistDimensionDragLabelPosition(dimensionDrag);
    clearPreviewDimension();
    dimensionLabelDragRef.current = null;
    dimensionPlacementOriginalPositionRef.current = null;
    if (controlsRef.current) {
      controlsRef.current.enabled = true;
    }
    setCanvasCursor("");
    return true;
  }

  function cancelDimensionPlacement() {
    const dimensionDrag = dimensionLabelDragRef.current;
    if (!dimensionDrag?.isPlacement) {
      return false;
    }
    clearPreviewDimension();
    clearAnglePlacementPreview(dimensionDrag.dimensionId);
    if (angleDragRadiiRef.current[dimensionDrag.dimensionId] !== undefined) {
      const next = { ...angleDragRadiiRef.current };
      delete next[dimensionDrag.dimensionId];
      angleDragRadiiRef.current = next;
      setAngleDragRadii(next);
    }
    const originalPosition = dimensionPlacementOriginalPositionRef.current;
    if (originalPosition) {
      setDimensionLabelPositions((current) => ({
        ...current,
        [dimensionDrag.dimensionId]: originalPosition,
      }));
    }
    dimensionLabelDragRef.current = null;
    dimensionPlacementOriginalPositionRef.current = null;
    if (controlsRef.current) {
      controlsRef.current.enabled = true;
    }
    setCanvasCursor("");
    return true;
  }

  function beginDimensionPlacement(
    dimension: SketchDimensionScene,
    relation?: DimensionRelationPreview | null,
  ) {
    const renderer = rendererRef.current;
    const camera = cameraRef.current;
    const sketchPlaneId = activeSketchPlaneIdRef.current;
    const controls = controlsRef.current;
    const pointerEvent = lastPointerEventRef.current;
    if (!renderer || !camera || !sketchPlaneId || !controls || !pointerEvent) {
      return;
    }
    const placement = buildDimensionPlacementStart({
      event: pointerEvent,
      renderer,
      camera,
      activeSketchPlaneId: sketchPlaneId,
      activeSketchPlaneFrame: activeSketchPlaneFrameRef.current,
      dimension,
      relationPosition: pendingRelationPlacementLabelRef.current,
      relation,
      getDimensionPlacementAxis,
    });
    if (!placement) {
      return;
    }

    if (placement.isAngleKind && placement.angleWorldPoint) {
      setAngleDimensionDragRadius(
        dimension,
        dimension.dimensionId,
        placement.angleWorldPoint,
      );
    }
    pendingRelationPlacementLabelRef.current = null;
    dimensionPlacementOriginalPositionRef.current = placement.originalPosition;
    if (!placement.isAngleKind) {
      setDimensionLabelPosition(dimension.dimensionId, placement.nextPosition);
    }
    dimensionLabelDragRef.current = placement.dragState;
    controls.enabled = false;
    setCanvasCursor("grabbing");
  }

  return {
    beginDimensionPlacement,
    cancelDimensionPlacement,
    finishDimensionPlacement,
    getDimensionPlacementAxis,
    persistDimensionDragLabelPosition,
    setAngleDimensionDragRadius,
    setDimensionLabelPosition,
  };
}
