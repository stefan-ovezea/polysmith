import type {
  Dispatch,
  KeyboardEvent as ReactKeyboardEvent,
  MutableRefObject,
  SetStateAction,
} from "react";

import type { SketchDimensionScene } from "@/types";
import type { DisplayUnits } from "@/utils/units";
import { parseDimensionInput } from "@/utils/units";
import { isAngleDimensionKind } from "./dimensionLabelDrag";

import {
  parameterTokenAtCursor,
  type DimensionLabelDragState,
  type ParameterSuggestion,
} from "./draftDimensions";

type DimensionUpdateValue = number | string;

interface DimensionEditorValueParams {
  dimension: SketchDimensionScene;
  rawValue: string;
  displayUnits: DisplayUnits;
}

interface SubmitDimensionEditorValueParams extends DimensionEditorValueParams {
  updateSketchDimension: (
    dimensionId: string,
    value: DimensionUpdateValue,
  ) => Promise<void>;
  toCoreValue: (dimension: SketchDimensionScene, displayValue: number) => number;
}

interface UpdateDimensionEditorDraftParams extends SubmitDimensionEditorValueParams {
  setDraftValue: Dispatch<SetStateAction<string>>;
  expressionTimeoutRef: MutableRefObject<ReturnType<typeof setTimeout> | null>;
}

export interface DimensionEditOriginalValue {
  dimensionId: string;
  value: number;
  expression: string;
}

interface CancelDimensionEditorEditParams {
  expressionTimeoutRef: MutableRefObject<ReturnType<typeof setTimeout> | null>;
  dimension: SketchDimensionScene | null;
  originalValueRef: MutableRefObject<DimensionEditOriginalValue | null>;
  selectedDimensionValue: number | null;
  selectedDimensionExpression: string;
  cancelDimensionPlacement: () => void;
  updateSketchDimension: (
    dimensionId: string,
    value: DimensionUpdateValue,
  ) => Promise<void>;
  setDimensionLabelPositions: Dispatch<
    SetStateAction<Record<string, [number, number, number]>>
  >;
  setDraftValue: Dispatch<SetStateAction<string>>;
  setIsEditorOpen: Dispatch<SetStateAction<boolean>>;
  formatDisplayValue: (
    dimension: SketchDimensionScene,
    coreValue: number,
  ) => string;
}

interface CancelDimensionEditorPlacementParams {
  dimensionLabelDragRef: MutableRefObject<DimensionLabelDragState | null>;
  dimensionPlacementOriginalPositionRef: MutableRefObject<
    [number, number, number] | null
  >;
  controlsRef: MutableRefObject<{ enabled: boolean } | null>;
  originalValueRef: MutableRefObject<DimensionEditOriginalValue | null>;
  setIsEditorOpen: Dispatch<SetStateAction<boolean>>;
  setCanvasCursor: (cursor: string) => void;
  deleteSketchDimension: (dimensionId: string) => Promise<void>;
}

interface InsertDimensionParameterSuggestionParams {
  name: string;
  input: HTMLInputElement | null;
  draftValue: string;
  applyValue: (value: string) => void;
}

interface DimensionEditorKeyDownParams {
  event: ReactKeyboardEvent<HTMLInputElement>;
  suggestions: ParameterSuggestion[];
  suggestionIndex: number;
  setSuggestionIndex: Dispatch<SetStateAction<number>>;
  insertParameterSuggestion: (name: string) => void;
  cancelPlacementDimension: () => boolean;
  cancelEdit: () => void;
}

export async function submitDimensionEditorValue({
  dimension,
  rawValue,
  displayUnits,
  updateSketchDimension,
  toCoreValue,
}: SubmitDimensionEditorValueParams) {
  const parsed = parseDimensionEditorValue({
    dimension,
    rawValue,
    displayUnits,
  });
  await updateSketchDimension(
    dimension.dimensionId,
    parsed !== null && parsed > 0 ? toCoreValue(dimension, parsed) : rawValue,
  );
}

export function updateDimensionEditorDraft({
  dimension,
  rawValue,
  displayUnits,
  updateSketchDimension,
  toCoreValue,
  setDraftValue,
  expressionTimeoutRef,
}: UpdateDimensionEditorDraftParams) {
  setDraftValue(rawValue);
  const trimmed = rawValue.trim();
  if (!trimmed) {
    return;
  }

  // Numeric values are NOT submitted on every keystroke — that would
  // commit partial values like "3" while the user is still typing "35",
  // distorting the geometry.  Numeric values are only committed on
  // Enter (via submitDimensionEditorValue).
  //
  // Expression values (containing letters) still get a 300 ms debounced
  // preview so the user can see the resolved value while editing a
  // parameter reference like "width / 2".
  if (/[a-zA-Z_]/.test(trimmed)) {
    scheduleDimensionExpressionPreview({
      dimensionId: dimension.dimensionId,
      expression: trimmed,
      updateSketchDimension,
      expressionTimeoutRef,
    });
  }
}

export function cancelDimensionEditorEdit({
  expressionTimeoutRef,
  dimension,
  originalValueRef,
  selectedDimensionValue,
  selectedDimensionExpression,
  cancelDimensionPlacement,
  updateSketchDimension,
  setDimensionLabelPositions,
  setDraftValue,
  setIsEditorOpen,
  formatDisplayValue,
}: CancelDimensionEditorEditParams) {
  clearDimensionExpressionPreview(expressionTimeoutRef);
  const originalValue = originalValueRef.current;
  cancelDimensionPlacement();
  if (dimension && originalValue?.dimensionId === dimension.dimensionId) {
    void updateSketchDimension(
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
    setDraftValue(
      originalValue.expression.trim().length > 0
        ? originalValue.expression
        : formatDisplayValue(dimension, originalValue.value),
    );
  } else if (dimension && selectedDimensionValue !== null) {
    setDraftValue(
      selectedDimensionExpression.trim().length > 0
        ? selectedDimensionExpression
        : formatDisplayValue(dimension, selectedDimensionValue),
    );
  } else {
    setDraftValue("");
  }
  originalValueRef.current = null;
  setIsEditorOpen(false);
}

export function cancelDimensionEditorPlacement({
  dimensionLabelDragRef,
  dimensionPlacementOriginalPositionRef,
  controlsRef,
  originalValueRef,
  setIsEditorOpen,
  setCanvasCursor,
  deleteSketchDimension,
}: CancelDimensionEditorPlacementParams) {
  const drag = dimensionLabelDragRef.current;
  if (!drag?.isPlacement || !drag.dimensionId) {
    return false;
  }

  const dimensionId = drag.dimensionId;
  dimensionLabelDragRef.current = null;
  dimensionPlacementOriginalPositionRef.current = null;
  if (controlsRef.current) {
    controlsRef.current.enabled = true;
  }
  setCanvasCursor("");
  setIsEditorOpen(false);
  originalValueRef.current = null;
  void deleteSketchDimension(dimensionId);
  return true;
}

export function insertDimensionEditorParameterSuggestion({
  name,
  input,
  draftValue,
  applyValue,
}: InsertDimensionParameterSuggestionParams) {
  const cursor = inputCursor(input, draftValue);
  const { start, end } = replacementBounds(draftValue, cursor);
  const nextValue = draftValue.slice(0, start) + name + draftValue.slice(end);
  applyValue(nextValue);
  refocusInputAt(input, start + name.length);
}

export function handleDimensionEditorKeyDown({
  event,
  suggestions,
  suggestionIndex,
  setSuggestionIndex,
  insertParameterSuggestion,
  cancelPlacementDimension,
  cancelEdit,
}: DimensionEditorKeyDownParams) {
  if (
    suggestions.length > 0 &&
    (event.key === "ArrowDown" || event.key === "ArrowUp")
  ) {
    event.preventDefault();
    setSuggestionIndex((current) => {
      const delta = event.key === "ArrowDown" ? 1 : -1;
      return (current + delta + suggestions.length) % suggestions.length;
    });
    return;
  }

  if (
    suggestions.length > 0 &&
    (event.key === "Tab" || event.key === "Enter")
  ) {
    event.preventDefault();
    const suggestion = suggestions[suggestionIndex] ?? suggestions[0];
    insertParameterSuggestion(suggestion.name);
    return;
  }

  if (event.key === "Escape") {
    event.preventDefault();
    if (!cancelPlacementDimension()) {
      cancelEdit();
    }
  }
}

function inputCursor(input: HTMLInputElement | null, fallbackValue: string) {
  return input?.selectionStart ?? fallbackValue.length;
}

function replacementBounds(value: string, cursor: number) {
  const token = parameterTokenAtCursor(value, cursor);
  return {
    start: token?.start ?? cursor,
    end: token?.end ?? cursor,
  };
}

function refocusInputAt(input: HTMLInputElement | null, cursor: number) {
  window.requestAnimationFrame(() => {
    input?.focus();
    input?.setSelectionRange(cursor, cursor);
  });
}

export function clearDimensionExpressionPreview(
  expressionTimeoutRef: MutableRefObject<ReturnType<typeof setTimeout> | null>,
) {
  if (expressionTimeoutRef.current !== null) {
    clearTimeout(expressionTimeoutRef.current);
    expressionTimeoutRef.current = null;
  }
}

function parseDimensionEditorValue({
  dimension,
  rawValue,
  displayUnits,
}: DimensionEditorValueParams): number | null {
  if (isAngleDimension(dimension)) {
    const parsed = parseFloat(rawValue.replace(",", "."));
    return isNaN(parsed) ? null : parsed;
  }
  return parseDimensionInput(rawValue, displayUnits);
}

function isAngleDimension(dimension: SketchDimensionScene) {
  // Angle kinds are typed in degrees, so they bypass the mm/inch parser.
  return isAngleDimensionKind(dimension.kind);
}

function scheduleDimensionExpressionPreview({
  dimensionId,
  expression,
  updateSketchDimension,
  expressionTimeoutRef,
}: {
  dimensionId: string;
  expression: string;
  updateSketchDimension: (
    dimensionId: string,
    value: DimensionUpdateValue,
  ) => Promise<void>;
  expressionTimeoutRef: MutableRefObject<ReturnType<typeof setTimeout> | null>;
}) {
  clearDimensionExpressionPreview(expressionTimeoutRef);
  expressionTimeoutRef.current = setTimeout(() => {
    expressionTimeoutRef.current = null;
    void updateSketchDimension(dimensionId, expression).catch(() => {});
  }, 300);
}
