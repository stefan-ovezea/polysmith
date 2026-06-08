import type { Dispatch, MutableRefObject, SetStateAction } from "react";
import * as THREE from "three";

import { resolveSketchPlanePoint } from "@/utils";
import type {
  SketchDimensionScene,
  SketchFeatureParameters,
  SketchTool,
} from "@/types";
import type { ActiveSketchGridPlaneFrame } from "./grid";
import {
  pendingRelationDimension as findPendingRelationDimension,
} from "./dimensionRelationPreview";
import { unaryDimensionIdForEntity } from "./dimensionToolPicking";
import type {
  DimensionLabelDragState,
  DimensionRelationPreview,
} from "./draftDimensions";

interface RelationPlacementActionsContext {
  rendererRef: MutableRefObject<THREE.WebGLRenderer | null>;
  cameraRef: MutableRefObject<THREE.OrthographicCamera | null>;
  controlsRef: MutableRefObject<{ enabled: boolean } | null>;
  activeSketchPlaneIdRef: MutableRefObject<string | null>;
  activeSketchPlaneFrameRef: MutableRefObject<ActiveSketchGridPlaneFrame | null>;
  activeSketchToolRef: MutableRefObject<SketchTool | null>;
  lastPointerEventRef: MutableRefObject<PointerEvent | null>;
  dimensionRelationPreviewRef: MutableRefObject<
    DimensionRelationPreview | null
  >;
  dimensionRelationPreviewLabelRef: MutableRefObject<
    [number, number, number] | null
  >;
  pendingRelationPlacementLabelRef: MutableRefObject<
    [number, number, number] | null
  >;
  pendingRelationPlacementMatchRef: MutableRefObject<
    DimensionRelationPreview | null
  >;
  pendingRelationPlacementRetryRef: MutableRefObject<number | null>;
  pendingDimensionIdRef: MutableRefObject<string | null>;
  pendingDimSourceEntityIdRef: MutableRefObject<string | null>;
  pendingDimensionPlacementRef: MutableRefObject<boolean>;
  dimensionLabelDragRef: MutableRefObject<DimensionLabelDragState | null>;
  dimensionPlacementOriginalPositionRef: MutableRefObject<
    [number, number, number] | null
  >;
  dimensionToolFirstLineRef: MutableRefObject<string | null>;
  dimensionToolFirstPointRef: MutableRefObject<{
    id: string;
    x: number;
    y: number;
  } | null>;
  displayedSketchDimensionsRef: MutableRefObject<SketchDimensionScene[]>;
  sketchLinesRef: MutableRefObject<SketchFeatureParameters | null>;
  deleteSketchDimensionRef: MutableRefObject<
    (dimensionId: string) => Promise<void>
  >;
  setDimensionToolFirstLine: Dispatch<SetStateAction<string | null>>;
  clearPreviewDimension: () => void;
  setCanvasCursor: (cursor: string) => void;
  createDimensionAngleOrDistance: (
    firstEntityId: string,
    secondEntityId: string,
  ) => void;
  beginDimensionPlacement: (dimension: SketchDimensionScene) => void;
}

export function createDimensionRelationPlacementActions({
  rendererRef,
  cameraRef,
  controlsRef,
  activeSketchPlaneIdRef,
  activeSketchPlaneFrameRef,
  activeSketchToolRef,
  lastPointerEventRef,
  dimensionRelationPreviewRef,
  dimensionRelationPreviewLabelRef,
  pendingRelationPlacementLabelRef,
  pendingRelationPlacementMatchRef,
  pendingRelationPlacementRetryRef,
  pendingDimensionIdRef,
  pendingDimSourceEntityIdRef,
  pendingDimensionPlacementRef,
  dimensionLabelDragRef,
  dimensionPlacementOriginalPositionRef,
  dimensionToolFirstLineRef,
  dimensionToolFirstPointRef,
  displayedSketchDimensionsRef,
  sketchLinesRef,
  deleteSketchDimensionRef,
  setDimensionToolFirstLine,
  clearPreviewDimension,
  setCanvasCursor,
  createDimensionAngleOrDistance,
  beginDimensionPlacement,
}: RelationPlacementActionsContext) {
  function commitDimensionRelationPreview() {
    const relation = dimensionRelationPreviewRef.current;
    if (!relation) {
      return false;
    }
    pendingRelationPlacementLabelRef.current =
      relationLabelPositionFromPointer() ??
      dimensionRelationPreviewLabelRef.current;
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
    if (controlsRef.current) {
      controlsRef.current.enabled = true;
    }
    setCanvasCursor("");
    dimensionToolFirstLineRef.current = null;
    setDimensionToolFirstLine(null);
    dimensionToolFirstPointRef.current = null;
    createDimensionAngleOrDistance(
      relation.firstEntityId,
      relation.targetEntityId,
    );
    schedulePendingRelationPlacementRetry();
    return true;
  }

  function relationLabelPositionFromPointer() {
    const pointerEvent = lastPointerEventRef.current;
    const renderer = rendererRef.current;
    const camera = cameraRef.current;
    const sketchPlaneId = activeSketchPlaneIdRef.current;
    return pointerEvent && renderer && camera && sketchPlaneId
      ? resolveSketchPlanePoint(
          pointerEvent,
          renderer,
          camera,
          sketchPlaneId,
          activeSketchPlaneFrameRef.current,
        )?.world ?? null
      : null;
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
    const placementDimension = findPendingRelationDimension(
      relation,
      displayedSketchDimensionsRef.current,
      sketchLinesRef.current,
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

  function stopPendingRelationPlacementRetry() {
    if (pendingRelationPlacementRetryRef.current !== null) {
      window.cancelAnimationFrame(pendingRelationPlacementRetryRef.current);
      pendingRelationPlacementRetryRef.current = null;
    }
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
      pendingRelationPlacementRetryRef.current =
        window.requestAnimationFrame(tick);
    };
    pendingRelationPlacementRetryRef.current =
      window.requestAnimationFrame(tick);
  }

  return {
    commitDimensionRelationPreview,
    startPendingRelationPlacementIfReady,
    stopPendingRelationPlacementRetry,
  };
}
