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
  options?: { toolMode?: string },
): DraftDimensionSession {
  const fields = draftSessionFields(tool);
  return {
    tool,
    start,
    current,
    secondPoint: null,
    toolMode: options?.toolMode,
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
    options?: {
      // Force the rectangle's four auto side dimensions to be deleted even
      // when the user typed draft values. The rounded-rectangle mode needs
      // this: its sides are trimmed by the corner fillets, so a side
      // dimension measures the trimmed line, not the overall width/length
      // the draft badge showed. Typed values still size the shape (the
      // session's current point was reshaped before this commit).
      deleteRectangleDimensions?: boolean;
    },
  ) {
    const session = preCapturedSession ?? draftDimensionSessionRef.current;
    pendingDimensionDeletionRef.current = {
      tool,
      // Pre-add counts gate the drain: the post-commit effect also
      // runs on intermediate replies (e.g. the begin_group reply of
      // the click commit path), when the previous entity is still
      // "the last one" — deleting then would strip the WRONG
      // entity's dimension.
      fromLineCount: sketchLineCountRef.current,
      fromCircleCount: sketchParameters?.circles.length ?? 0,
      fromPolygonCount: sketchParameters?.polygons?.length ?? 0,
      fromArcCount: sketchParameters?.arcs?.length ?? 0,
      shouldDeleteLine: tool === "line" && !session?.touchedFields.length,
      shouldDeleteCircle:
        tool === "circle" && !session?.touchedFields.diameter,
      shouldDeletePolygon:
        tool === "polygon" && !session?.touchedFields.radius,
      shouldDeleteRectangle:
        tool === "rectangle" &&
        (options?.deleteRectangleDimensions === true ||
          (!session?.touchedFields.width &&
            !session?.touchedFields.length)),
      shouldDeleteLineAngle: tool === "line" && !session?.touchedFields.angle,
      // Same convention as circles: the auto radius dim is deleted
      // unless the user typed a value during the draft, in which case
      // it stays showing the arc's real radius.
      shouldDeleteArc:
        tool === "arc" &&
        !session?.touchedFields.length &&
        !session?.touchedFields.radius,
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
      fromArcCount: sketchParameters?.arcs?.length ?? 0,
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
