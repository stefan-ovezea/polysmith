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
  addSketchPolygonRadiusDimensionRef: MutableRefObject<
    (entityId: string) => Promise<void>
  >;
  addSketchAngleDimensionRef: MutableRefObject<
    (firstEntityId: string, secondEntityId: string) => Promise<void>
  >;
  addSketchDistanceDimensionRef: MutableRefObject<
    (firstEntityId: string, secondEntityId: string) => Promise<void>
  >;
  addSketchPointDistanceDimensionRef: MutableRefObject<
    (pointAId: string, pointBId: string) => Promise<void>
  >;
  updateSketchDimensionRef: MutableRefObject<
    (dimensionId: string, value: UpdateDimensionValue) => Promise<void>
  >;
  setDimensionToolFirstLine: Dispatch<SetStateAction<string | null>>;
  handleDimensionClick: (dimensionId: string) => void;
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
  addSketchPolygonRadiusDimensionRef,
  addSketchAngleDimensionRef,
  addSketchDistanceDimensionRef,
  addSketchPointDistanceDimensionRef,
  updateSketchDimensionRef,
  setDimensionToolFirstLine,
  handleDimensionClick,
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
    handleDimensionClick(`dim-circle-${entityId}`);
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

  function selectDimensionLine(entityId: string) {
    handleDimensionClick(`dim-line-${entityId}`);
    stageFollowUpPick(entityId);
  }

  function createDimensionPolygon(entityId: string) {
    stageUnaryDimension(entityId, `dim-polygon-${entityId}`);
    void addSketchPolygonRadiusDimensionRef.current(entityId).catch(() => {
      clearUnaryDimensionStage();
    });
  }

  function selectDimensionPolygon(entityId: string) {
    handleDimensionClick(`dim-polygon-${entityId}`);
    stageFollowUpPick(entityId);
  }

  function createDimensionAngleOrDistance(
    firstEntityId: string,
    secondEntityId: string,
  ) {
    pendingDimensionPlacementRef.current = true;
    pendingDimSourceEntityIdRef.current = null;
    if (
      firstEntityId.startsWith("line-") &&
      secondEntityId.startsWith("line-")
    ) {
      const isReflex = pendingAngleIsReflexRef.current;
      const reflexAngle = pendingReflexAngleRef.current;
      pendingAngleIsReflexRef.current = false;
      pendingReflexAngleRef.current = 0;
      void addSketchAngleDimensionRef
        .current(firstEntityId, secondEntityId)
        .then(() => {
          if (isReflex) {
            const ids = [firstEntityId, secondEntityId].sort();
            const dimId = `dim-angle-${ids[0]}-${ids[1]}`;
            void updateSketchDimensionRef.current(dimId, reflexAngle);
          }
        })
        .catch(clearRelationPlacementStage);
      return;
    }

    void addSketchDistanceDimensionRef
      .current(firstEntityId, secondEntityId)
      .catch(clearRelationPlacementStage);
  }

  function createDimensionPointDistance(pointAId: string, pointBId: string) {
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
    createDimensionPointDistance,
    createDimensionPolygon,
    selectDimensionCircle,
    selectDimensionLine,
    selectDimensionPolygon,
  };
}
