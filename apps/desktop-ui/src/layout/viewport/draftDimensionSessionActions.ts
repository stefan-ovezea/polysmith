import type { Dispatch, MutableRefObject, SetStateAction } from "react";

import type { SketchFeatureParameters } from "@/types";
import {
  draftSessionFields,
  draftSessionValues,
  type DraftDimensionField,
  type DraftDimensionSession,
  type DraftDimensionTool,
} from "./draftDimensions";
import type {
  PendingDimensionDeletion,
  PendingDraftDimensionExpressions,
} from "./draftDimensionPostCommit";
import type { DraftSuggestionState } from "./draftDimensionInput";

type DraftInputRefs = MutableRefObject<
  Partial<Record<DraftDimensionField, HTMLInputElement | null>>
>;

interface DraftDimensionSessionActionParams {
  sketchParameters: SketchFeatureParameters | null;
  draftDimensionInputRefs: DraftInputRefs;
  draftDimensionSessionRef: MutableRefObject<DraftDimensionSession | null>;
  draftFieldFocusedRef: MutableRefObject<DraftDimensionField | null>;
  draftRawInputRef: MutableRefObject<
    Partial<Record<DraftDimensionField, string>>
  >;
  draftParameterExpressionRef: MutableRefObject<
    Partial<Record<DraftDimensionField, string>>
  >;
  previousLineAngleRef: MutableRefObject<number | null>;
  pendingDimensionDeletionRef: MutableRefObject<
    PendingDimensionDeletion | null
  >;
  pendingDraftDimensionExpressionsRef: MutableRefObject<
    PendingDraftDimensionExpressions | null
  >;
  sketchLineCountRef: MutableRefObject<number>;
  setDraftDimensionSession: Dispatch<
    SetStateAction<DraftDimensionSession | null>
  >;
  setDraftSuggestionState: Dispatch<SetStateAction<DraftSuggestionState>>;
  setIsDimensionEditorOpen: Dispatch<SetStateAction<boolean>>;
  suppressNextDimensionEditorOpenRef: MutableRefObject<boolean>;
  dimensionInputRef: MutableRefObject<HTMLInputElement | null>;
  clearDraftDimGroup: () => void;
}

export function createDraftDimensionSession(
  tool: DraftDimensionTool,
  start: [number, number],
  current: [number, number],
): DraftDimensionSession {
  const fields = draftSessionFields(tool);
  return {
    tool,
    start,
    current,
    values: draftSessionValues(tool, start, current),
    activeField: fields[0],
    lockedFields: {},
    touchedFields: {},
  };
}

export function createDraftDimensionSessionActions({
  sketchParameters,
  draftDimensionInputRefs,
  draftDimensionSessionRef,
  draftFieldFocusedRef,
  draftRawInputRef,
  draftParameterExpressionRef,
  previousLineAngleRef,
  pendingDimensionDeletionRef,
  pendingDraftDimensionExpressionsRef,
  sketchLineCountRef,
  setDraftDimensionSession,
  setDraftSuggestionState,
  setIsDimensionEditorOpen,
  suppressNextDimensionEditorOpenRef,
  dimensionInputRef,
  clearDraftDimGroup,
}: DraftDimensionSessionActionParams) {
  function clearDraftDimensionSession() {
    Object.values(draftDimensionInputRefs.current).forEach((input) => {
      input?.blur();
    });
    clearDraftDimGroup();
    setDraftDimensionSession(null);
    draftDimensionSessionRef.current = null;
    draftFieldFocusedRef.current = null;
    draftRawInputRef.current = {};
    draftParameterExpressionRef.current = {};
    previousLineAngleRef.current = null;
    setDraftSuggestionState(null);
  }

  function scheduleDimensionDeletion(
    tool: DraftDimensionTool,
    preCapturedSession?: DraftDimensionSession | null,
  ) {
    const session = preCapturedSession ?? draftDimensionSessionRef.current;
    pendingDimensionDeletionRef.current = {
      shouldDeleteLine: tool === "line" && !session?.touchedFields.length,
      shouldDeleteCircle:
        tool === "circle" && !session?.touchedFields.diameter,
      shouldDeletePolygon:
        tool === "polygon" && !session?.touchedFields.radius,
      shouldDeleteRectangle:
        tool === "rectangle" &&
        !session?.touchedFields.width &&
        !session?.touchedFields.length,
      shouldDeleteLineAngle: tool === "line" && !session?.touchedFields.angle,
    };
  }

  function scheduleDraftDimensionExpressionUpdate(tool: DraftDimensionTool) {
    const entries = Object.entries(draftParameterExpressionRef.current).filter(
      ([, expression]) => expression.trim().length > 0,
    );
    if (entries.length === 0) {
      pendingDraftDimensionExpressionsRef.current = null;
      return;
    }
    pendingDraftDimensionExpressionsRef.current = {
      tool,
      fromLineCount: sketchLineCountRef.current,
      fromCircleCount: sketchParameters?.circles.length ?? 0,
      fromPolygonCount: sketchParameters?.polygons?.length ?? 0,
      expressions: Object.fromEntries(entries) as Partial<
        Record<DraftDimensionField, string>
      >,
    };
    draftParameterExpressionRef.current = {};
  }

  function suppressDimensionEditorAfterSketchCommit() {
    suppressNextDimensionEditorOpenRef.current = true;
    dimensionInputRef.current?.blur();
    setIsDimensionEditorOpen(false);
  }

  return {
    clearDraftDimensionSession,
    scheduleDimensionDeletion,
    scheduleDraftDimensionExpressionUpdate,
    suppressDimensionEditorAfterSketchCommit,
  };
}
