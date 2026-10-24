import {
  useEffect,
  useRef,
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
  // Which dimension the current draft value belongs to. See the seeding effect.
  const draftSeededForRef = useRef<string | null>(null);

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
    // Do NOT bail out merely because the input still has focus: clicking a
    // different dimension while the field is focused kept the PREVIOUS value in
    // the box (reported in-app - edit dimension A, click B, the box shows A's
    // value). Only the SAME dimension may keep its in-progress text.
    const draftSeededFor = draftSeededForRef.current;
    if (
      window.document.activeElement === dimensionInputRef.current &&
      draftSeededFor === selectedSketchDimension.dimensionId
    ) {
      return;
    }
    draftSeededForRef.current = selectedSketchDimension.dimensionId;

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

    if (
      window.document.activeElement === input &&
      !dimensionInputSelectionLockedRef.current
    ) {
      return;
    }

    input.focus();
    if (dimensionInputSelectionLockedRef.current) {
      input.select();
      dimensionInputSelectionLockedRef.current = false;
    }
  }, [
    isDimensionEditorOpen,
    selectedSketchDimension?.dimensionId,
    dimensionInputRef,
    dimensionInputSelectionLockedRef,
  ]);
}
