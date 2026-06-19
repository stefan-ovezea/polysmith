import type { Dispatch, MutableRefObject, SetStateAction } from "react";

import type { DimensionRelationPreview } from "./draftDimensions";

type UpdateDimensionValue = number | string;

interface DimensionToolActionParams {
  pendingDimensionIdRef: MutableRefObject<string | null>;
  pendingDimSourceEntityIdRef: MutableRefObject<string | null>;
  pendingDimensionPlacementRef: MutableRefObject<boolean>;
  dimensionToolFirstLineRef: MutableRefObject<string | null>;
  pendingAngleIsReflexRef: MutableRefObject<boolean>;
  pendingReflexAngleRef: MutableRefObject<number>;
  pendingRelationPlacementLabelRef: MutableRefObject<
    [number, number, number] | null
  >;
  pendingRelationPlacementMatchRef: MutableRefObject<
    DimensionRelationPreview | null
  >;
  addSketchCircleRadiusDimensionRef: MutableRefObject<
    (entityId: string, displayAs?: string) => Promise<void>
  >;
  addSketchLineLengthDimensionRef: MutableRefObject<
    (entityId: string) => Promise<void>
  >;
  addSketchLineAngleDimensionRef: MutableRefObject<
    (entityId: string) => Promise<void>
  >;
  addSketchPolygonRadiusDimensionRef: MutableRefObject<
    (entityId: string) => Promise<void>
  >;
  addSketchAngleDimensionRef: MutableRefObject<
    (firstEntityId: string, secondEntityId: string, value?: number) => Promise<void>
  >;
  addSketchDistanceDimensionRef: MutableRefObject<
    (firstEntityId: string, secondEntityId: string) => Promise<void>
  >;
  addSketchPointDistanceDimensionRef: MutableRefObject<
    (pointAId: string, pointBId: string, axis?: "x" | "y") => Promise<void>
  >;
  updateSketchDimensionRef: MutableRefObject<
    (dimensionId: string, value: UpdateDimensionValue) => Promise<void>
  >;
  setDimensionToolFirstLine: Dispatch<SetStateAction<string | null>>;
}

export function createDimensionToolActions({
  pendingDimensionIdRef,
  pendingDimSourceEntityIdRef,
  pendingDimensionPlacementRef,
  dimensionToolFirstLineRef,
  pendingAngleIsReflexRef,
  pendingReflexAngleRef,
  pendingRelationPlacementLabelRef,
  pendingRelationPlacementMatchRef,
  addSketchCircleRadiusDimensionRef,
  addSketchLineLengthDimensionRef,
  addSketchLineAngleDimensionRef,
  addSketchPolygonRadiusDimensionRef,
  addSketchAngleDimensionRef,
  addSketchDistanceDimensionRef,
  addSketchPointDistanceDimensionRef,
  updateSketchDimensionRef,
  setDimensionToolFirstLine,
}: DimensionToolActionParams) {
  function stageUnaryDimension(entityId: string, dimensionId: string) {
    pendingDimensionIdRef.current = dimensionId;
    pendingDimSourceEntityIdRef.current = entityId;
    pendingDimensionPlacementRef.current = true;
  }

  function clearUnaryDimensionStage() {
    pendingDimensionIdRef.current = null;
    pendingDimSourceEntityIdRef.current = null;
    pendingDimensionPlacementRef.current = false;
  }

  function stageFollowUpPick(entityId: string) {
    dimensionToolFirstLineRef.current = entityId;
    setDimensionToolFirstLine(entityId);
  }

  function clearFollowUpPick() {
    dimensionToolFirstLineRef.current = null;
    setDimensionToolFirstLine(null);
  }

  function createDimensionCircle(entityId: string, displayAs: string) {
    stageUnaryDimension(entityId, `dim-circle-${entityId}`);
    stageFollowUpPick(entityId);
    void addSketchCircleRadiusDimensionRef
      .current(entityId, displayAs)
      .catch(() => {
        clearUnaryDimensionStage();
        clearFollowUpPick();
      });
  }

  function selectDimensionCircle(entityId: string) {
    stageFollowUpPick(entityId);
  }

  function createDimensionLine(entityId: string) {
    stageUnaryDimension(entityId, `dim-line-${entityId}`);
    stageFollowUpPick(entityId);
    void addSketchLineLengthDimensionRef.current(entityId).catch(() => {
      clearUnaryDimensionStage();
      clearFollowUpPick();
    });
  }

  function createDimensionLineAngle(entityId: string) {
    stageUnaryDimension(entityId, `dim-line-angle-${entityId}`);
    stageFollowUpPick(entityId);
    void addSketchLineAngleDimensionRef.current(entityId).catch(() => {
      clearUnaryDimensionStage();
      clearFollowUpPick();
    });
  }

  function selectDimensionLine(entityId: string) {
    stageFollowUpPick(entityId);
  }

  function createDimensionPolygon(entityId: string) {
    stageUnaryDimension(entityId, `dim-polygon-${entityId}`);
    void addSketchPolygonRadiusDimensionRef.current(entityId).catch(() => {
      clearUnaryDimensionStage();
    });
  }

  function selectDimensionPolygon(entityId: string) {
    stageFollowUpPick(entityId);
  }

  function createDimensionAngleOrDistance(
    firstEntityId: string,
    secondEntityId: string,
    forceMode?: "angle" | "distance",
  ) {
    pendingDimensionPlacementRef.current = true;
    pendingDimSourceEntityIdRef.current = null;
    if (
      firstEntityId.startsWith("line-") &&
      secondEntityId.startsWith("line-")
    ) {
      // In auto mode, the preview relation determines whether it's a
      // parallel distance or an angle.  When a mode is forced, skip the
      // auto-detection.
      if (forceMode !== "angle") {
        const relation = pendingRelationPlacementMatchRef.current;
        if (
          relation?.kind === "parallel_line_distance" &&
          ((relation.firstEntityId === firstEntityId &&
            relation.targetEntityId === secondEntityId) ||
            (relation.firstEntityId === secondEntityId &&
              relation.targetEntityId === firstEntityId))
        ) {
          pendingAngleIsReflexRef.current = false;
          pendingReflexAngleRef.current = 0;
          void addSketchDistanceDimensionRef
            .current(firstEntityId, secondEntityId)
            .catch(clearRelationPlacementStage);
          return;
        }
      }
      if (forceMode !== "distance") {
        const shouldApply = pendingAngleIsReflexRef.current;
        const previewAngle = pendingReflexAngleRef.current;
        pendingAngleIsReflexRef.current = false;
        pendingReflexAngleRef.current = 0;
        // Pass the quadrant angle directly so the dimension is created
        // with the correct value — no post‑commit update that would
        // rotate the line.
        const angleValue = shouldApply ? previewAngle : undefined;
        void addSketchAngleDimensionRef
          .current(firstEntityId, secondEntityId, angleValue)
          .then(clearRelationPlacementStage)
          .catch(clearRelationPlacementStage);
        return;
      }
    }

    void addSketchDistanceDimensionRef
      .current(firstEntityId, secondEntityId)
      .catch(clearRelationPlacementStage);
  }

  function createDimensionPointDistance(pointAId: string, pointBId: string, axis?: "x" | "y") {
    pendingDimensionIdRef.current =
      `dim-point-distance-${pointAId}-${pointBId}`;
    pendingDimensionPlacementRef.current = true;
    pendingDimSourceEntityIdRef.current = null;
    void addSketchPointDistanceDimensionRef
      .current(pointAId, pointBId, axis)
      .catch(() => {
        pendingDimensionIdRef.current = null;
        pendingDimensionPlacementRef.current = false;
      });
  }

  function clearRelationPlacementStage() {
    pendingDimensionPlacementRef.current = false;
    pendingRelationPlacementLabelRef.current = null;
    pendingRelationPlacementMatchRef.current = null;
    pendingAngleIsReflexRef.current = false;
    pendingReflexAngleRef.current = 0;
  }

  return {
    createDimensionAngleOrDistance,
    createDimensionCircle,
    createDimensionLine,
    createDimensionLineAngle,
    createDimensionPointDistance,
    createDimensionPolygon,
    selectDimensionCircle,
    selectDimensionLine,
    selectDimensionPolygon,
  };
}
