import {
  useEffect,
  type Dispatch,
  type MutableRefObject,
  type SetStateAction,
} from "react";

import type { SketchDimensionScene } from "@/types";

interface DimensionEditOriginalValue {
  dimensionId: string;
  value: number;
  expression: string;
}

interface DimensionEditorEffectsContext {
  selectedSketchDimension: SketchDimensionScene | null;
  selectedSketchDimensionId: string | null | undefined;
  selectedSketchDimensionValue: number | null;
  selectedSketchDimensionExpression: string;
  isDimensionEditorOpen: boolean;
  dimensionInputRef: MutableRefObject<HTMLInputElement | null>;
  dimensionInputSelectionLockedRef: MutableRefObject<boolean>;
  dimensionEditOriginalValueRef: MutableRefObject<
    DimensionEditOriginalValue | null
  >;
  suppressNextDimensionEditorOpenRef: MutableRefObject<boolean>;
  setDimensionDraftValue: Dispatch<SetStateAction<string>>;
  setIsDimensionEditorOpen: Dispatch<SetStateAction<boolean>>;
  isProjectedCircleDimension: (dimensionId: string) => boolean;
  formattedDimensionDisplayValue: (
    dimension: SketchDimensionScene,
    coreValue: number,
  ) => string;
}

export function useDimensionEditorEffects({
  selectedSketchDimension,
  selectedSketchDimensionId,
  selectedSketchDimensionValue,
  selectedSketchDimensionExpression,
  isDimensionEditorOpen,
  dimensionInputRef,
  dimensionInputSelectionLockedRef,
  dimensionEditOriginalValueRef,
  suppressNextDimensionEditorOpenRef,
  setDimensionDraftValue,
  setIsDimensionEditorOpen,
  isProjectedCircleDimension,
  formattedDimensionDisplayValue,
}: DimensionEditorEffectsContext) {
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
    selectedSketchDimensionId,
    selectedSketchDimension,
    selectedSketchDimension?.dimensionId,
    selectedSketchDimension?.kind,
    dimensionInputRef,
    dimensionEditOriginalValueRef,
    setDimensionDraftValue,
  ]);

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
  }, [
    selectedSketchDimension?.dimensionId,
    selectedSketchDimension,
    dimensionInputRef,
    dimensionInputSelectionLockedRef,
    suppressNextDimensionEditorOpenRef,
    setIsDimensionEditorOpen,
  ]);

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
  }, [
    isDimensionEditorOpen,
    selectedSketchDimension?.dimensionId,
    selectedSketchDimension,
    dimensionInputRef,
  ]);
}
