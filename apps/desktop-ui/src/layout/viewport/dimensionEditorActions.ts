import type { Dispatch, MutableRefObject, SetStateAction } from "react";

import type { SketchDimensionScene } from "@/types";
import type { DisplayUnits } from "@/utils/units";
import {
  cancelDimensionEditorEdit,
  cancelDimensionEditorPlacement,
  type DimensionEditOriginalValue,
  insertDimensionEditorParameterSuggestion,
  submitDimensionEditorValue,
  updateDimensionEditorDraft,
} from "./dimensionEditorInput";
import type { DimensionLabelDragState } from "./draftDimensions";

type DimensionUpdateValue = number | string;

interface DimensionEditorActionParams {
  selectedSketchDimension: SketchDimensionScene | null;
  selectedSketchDimensionValue: number | null;
  selectedSketchDimensionExpression: string;
  dimensionDraftValue: string;
  displayUnits: DisplayUnits;
  dimensionInputRef: MutableRefObject<HTMLInputElement | null>;
  dimensionExpressionTimeoutRef: MutableRefObject<
    ReturnType<typeof setTimeout> | null
  >;
  dimensionEditOriginalValueRef: MutableRefObject<
    DimensionEditOriginalValue | null
  >;
  dimensionLabelDragRef: MutableRefObject<DimensionLabelDragState | null>;
  dimensionPlacementOriginalPositionRef: MutableRefObject<
    [number, number, number] | null
  >;
  controlsRef: MutableRefObject<{ enabled: boolean } | null>;
  updateSketchDimension: (
    dimensionId: string,
    value: DimensionUpdateValue,
  ) => Promise<void>;
  deleteSketchDimension: (dimensionId: string) => Promise<void>;
  dimensionCoreValue: (
    dimension: SketchDimensionScene,
    displayValue: number,
  ) => number;
  formattedDimensionDisplayValue: (
    dimension: SketchDimensionScene,
    coreValue: number,
  ) => string;
  finishDimensionPlacement: () => void;
  cancelDimensionPlacement: () => void;
  setDimensionDraftValue: Dispatch<SetStateAction<string>>;
  setDimensionLabelPositions: Dispatch<
    SetStateAction<Record<string, [number, number, number]>>
  >;
  setIsDimensionEditorOpen: Dispatch<SetStateAction<boolean>>;
  setCanvasCursor: (cursor: string) => void;
}

export function createDimensionEditorActions({
  selectedSketchDimension,
  selectedSketchDimensionValue,
  selectedSketchDimensionExpression,
  dimensionDraftValue,
  displayUnits,
  dimensionInputRef,
  dimensionExpressionTimeoutRef,
  dimensionEditOriginalValueRef,
  dimensionLabelDragRef,
  dimensionPlacementOriginalPositionRef,
  controlsRef,
  updateSketchDimension,
  deleteSketchDimension,
  dimensionCoreValue,
  formattedDimensionDisplayValue,
  finishDimensionPlacement,
  cancelDimensionPlacement,
  setDimensionDraftValue,
  setDimensionLabelPositions,
  setIsDimensionEditorOpen,
  setCanvasCursor,
}: DimensionEditorActionParams) {
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

    await submitDimensionEditorValue({
      dimension: selectedSketchDimension,
      rawValue,
      displayUnits,
      updateSketchDimension,
      toCoreValue: dimensionCoreValue,
    });
    // Clear the ENTIRE UI label position cache — editing a driving
    // dimension triggers a solver pass that may move any geometry,
    // making ALL cached positions stale.
    setDimensionLabelPositions({});
    finishDimensionPlacement();
    dimensionEditOriginalValueRef.current = null;
    setIsDimensionEditorOpen(false);
  }

  function handleDimensionDraftChange(value: string) {
    if (!selectedSketchDimension) {
      setDimensionDraftValue(value);
      return;
    }
    updateDimensionEditorDraft({
      dimension: selectedSketchDimension,
      rawValue: value,
      displayUnits,
      updateSketchDimension,
      toCoreValue: dimensionCoreValue,
      setDraftValue: setDimensionDraftValue,
      expressionTimeoutRef: dimensionExpressionTimeoutRef,
    });
  }

  function insertDimensionParameterSuggestion(name: string) {
    insertDimensionEditorParameterSuggestion({
      name,
      input: dimensionInputRef.current,
      draftValue: dimensionDraftValue,
      applyValue: handleDimensionDraftChange,
    });
  }

  function cancelDimensionEdit() {
    cancelDimensionEditorEdit({
      expressionTimeoutRef: dimensionExpressionTimeoutRef,
      dimension: selectedSketchDimension,
      originalValueRef: dimensionEditOriginalValueRef,
      selectedDimensionValue: selectedSketchDimensionValue,
      selectedDimensionExpression: selectedSketchDimensionExpression,
      cancelDimensionPlacement,
      updateSketchDimension,
      setDimensionLabelPositions,
      setDraftValue: setDimensionDraftValue,
      setIsEditorOpen: setIsDimensionEditorOpen,
      formatDisplayValue: formattedDimensionDisplayValue,
    });
  }

  function cancelDimensionPlacementFromEditor() {
    return cancelDimensionEditorPlacement({
      dimensionLabelDragRef,
      dimensionPlacementOriginalPositionRef,
      controlsRef,
      originalValueRef: dimensionEditOriginalValueRef,
      setIsEditorOpen: setIsDimensionEditorOpen,
      setCanvasCursor,
      deleteSketchDimension,
    });
  }

  return {
    cancelDimensionEdit,
    cancelDimensionPlacementFromEditor,
    handleDimensionDraftChange,
    handleSubmitDimensionEdit,
    insertDimensionParameterSuggestion,
  };
}
