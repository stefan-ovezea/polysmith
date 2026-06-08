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

export function createDimensionRelationPlacementActions(
  context: RelationPlacementActionsContext,
) {
  function commitDimensionRelationPreview() {
    const relation = context.dimensionRelationPreviewRef.current;
    if (!relation) {
      return false;
    }
    context.pendingRelationPlacementLabelRef.current =
      relationLabelPositionFromPointer() ??
      context.dimensionRelationPreviewLabelRef.current;
    context.pendingRelationPlacementMatchRef.current = relation;
    context.clearPreviewDimension();
    context.dimensionRelationPreviewRef.current = null;
    const unaryDimensionId = unaryDimensionIdForEntity(relation.firstEntityId);
    if (unaryDimensionId) {
      void context.deleteSketchDimensionRef.current(unaryDimensionId);
    }
    context.pendingDimensionIdRef.current = null;
    context.pendingDimSourceEntityIdRef.current = null;
    context.pendingDimensionPlacementRef.current = false;
    context.dimensionLabelDragRef.current = null;
    context.dimensionPlacementOriginalPositionRef.current = null;
    if (context.controlsRef.current) {
      context.controlsRef.current.enabled = true;
    }
    context.setCanvasCursor("");
    context.dimensionToolFirstLineRef.current = null;
    context.setDimensionToolFirstLine(null);
    context.dimensionToolFirstPointRef.current = null;
    context.createDimensionAngleOrDistance(
      relation.firstEntityId,
      relation.targetEntityId,
    );
    schedulePendingRelationPlacementRetry();
    return true;
  }

  function relationLabelPositionFromPointer() {
    const pointerEvent = context.lastPointerEventRef.current;
    const renderer = context.rendererRef.current;
    const camera = context.cameraRef.current;
    const sketchPlaneId = context.activeSketchPlaneIdRef.current;
    return pointerEvent && renderer && camera && sketchPlaneId
      ? resolveSketchPlanePoint(
          pointerEvent,
          renderer,
          camera,
          sketchPlaneId,
          context.activeSketchPlaneFrameRef.current,
        )?.world ?? null
      : null;
  }

  function startPendingRelationPlacementIfReady() {
    if (
      !context.pendingDimensionPlacementRef.current ||
      context.activeSketchToolRef.current !== "dimension"
    ) {
      return true;
    }
    const relation = context.pendingRelationPlacementMatchRef.current;
    if (!relation) {
      return true;
    }
    const placementDimension = findPendingRelationDimension(
      relation,
      context.displayedSketchDimensionsRef.current,
      context.sketchLinesRef.current,
    );
    if (!placementDimension) {
      return false;
    }
    context.pendingRelationPlacementMatchRef.current = null;
    context.pendingDimensionIdRef.current = null;
    context.pendingDimensionPlacementRef.current = false;
    context.pendingDimSourceEntityIdRef.current = null;
    context.beginDimensionPlacement(placementDimension);
    return true;
  }

  function stopPendingRelationPlacementRetry() {
    if (context.pendingRelationPlacementRetryRef.current !== null) {
      window.cancelAnimationFrame(context.pendingRelationPlacementRetryRef.current);
      context.pendingRelationPlacementRetryRef.current = null;
    }
  }

  function schedulePendingRelationPlacementRetry() {
    stopPendingRelationPlacementRetry();
    let attempts = 0;
    const tick = () => {
      if (startPendingRelationPlacementIfReady()) {
        context.pendingRelationPlacementRetryRef.current = null;
        return;
      }
      attempts += 1;
      if (attempts >= 90) {
        context.pendingRelationPlacementRetryRef.current = null;
        return;
      }
      context.pendingRelationPlacementRetryRef.current =
        window.requestAnimationFrame(tick);
    };
    context.pendingRelationPlacementRetryRef.current =
      window.requestAnimationFrame(tick);
  }

  return {
    commitDimensionRelationPreview,
    startPendingRelationPlacementIfReady,
    stopPendingRelationPlacementRetry,
  };
}
