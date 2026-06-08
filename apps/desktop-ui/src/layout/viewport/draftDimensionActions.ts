import type {
  Dispatch,
  KeyboardEvent as ReactKeyboardEvent,
  MutableRefObject,
  SetStateAction,
} from "react";

import type { ParameterEntry } from "@/types";
import type { DisplayUnits } from "@/utils/units";
import {
  applyDraftDimensionFieldValue,
  parameterTokenAtCursor,
  type DraftDimensionField,
  type DraftDimensionSession,
} from "./draftDimensions";
import {
  draftDimensionFieldInputValue,
  handleDraftDimensionFieldKeyDown,
  resolveDraftDimensionFieldValue,
  type DraftSuggestionState,
} from "./draftDimensionInput";
import { getDimensionParameterSuggestions } from "./dimensionParameterSuggestions";

type DraftInputRefs = MutableRefObject<
  Partial<Record<DraftDimensionField, HTMLInputElement | null>>
>;

interface DraftDimensionActionParams {
  displayUnits: DisplayUnits;
  parameters: ParameterEntry[] | undefined;
  draftDimensionSessionRef: MutableRefObject<DraftDimensionSession | null>;
  draftRawInputRef: MutableRefObject<
    Partial<Record<DraftDimensionField, string>>
  >;
  draftParameterExpressionRef: MutableRefObject<
    Partial<Record<DraftDimensionField, string>>
  >;
  draftFieldFocusedRef: MutableRefObject<DraftDimensionField | null>;
  draftDimScreenPositionsRef: MutableRefObject<
    Partial<Record<DraftDimensionField, { x: number; y: number }>>
  >;
  draftDimensionInputRefs: DraftInputRefs;
  draftSuggestionState: DraftSuggestionState;
  setDraftSuggestionState: Dispatch<SetStateAction<DraftSuggestionState>>;
  setDraftDimensionSession: Dispatch<
    SetStateAction<DraftDimensionSession | null>
  >;
  commitDraftDimensionSession: (
    session?: DraftDimensionSession | null,
  ) => Promise<void> | void;
  selectTool: () => Promise<void> | void;
  cancelActiveSketchDraft: () => void;
}

export function createDraftDimensionActions({
  displayUnits,
  parameters,
  draftDimensionSessionRef,
  draftRawInputRef,
  draftParameterExpressionRef,
  draftFieldFocusedRef,
  draftDimScreenPositionsRef,
  draftDimensionInputRefs,
  draftSuggestionState,
  setDraftSuggestionState,
  setDraftDimensionSession,
  commitDraftDimensionSession,
  selectTool,
  cancelActiveSketchDraft,
}: DraftDimensionActionParams) {
  function handleDraftDimensionChange(
    field: DraftDimensionField,
    value: string,
  ) {
    const session = draftDimensionSessionRef.current;
    if (!session) {
      return;
    }
    // Preserve partial input like "2." while the field is focused.
    draftRawInputRef.current[field] = value;
    const { mmValue, parameterExpression } = resolveDraftDimensionFieldValue({
      value,
      displayUnits,
      parameters,
    });
    if (parameterExpression === null) {
      delete draftParameterExpressionRef.current[field];
    } else {
      draftParameterExpressionRef.current[field] = parameterExpression;
    }
    const next = applyDraftDimensionFieldValue(session, field, mmValue);
    draftDimensionSessionRef.current = next;
    draftDimScreenPositionsRef.current = {};
    setDraftDimensionSession(next);
    setDraftSuggestionState({ field, index: 0 });
  }

  function handleDraftDimensionFocus(field: DraftDimensionField) {
    const session = draftDimensionSessionRef.current;
    if (!session) {
      return;
    }
    draftFieldFocusedRef.current = field;
    const next = {
      ...session,
      activeField: field,
    };
    draftDimensionSessionRef.current = next;
    setDraftDimensionSession(next);
    setDraftSuggestionState({ field, index: 0 });
  }

  function handleDraftDimensionBlur(field: DraftDimensionField) {
    draftFieldFocusedRef.current = null;
    if (!draftParameterExpressionRef.current[field]) {
      delete draftRawInputRef.current[field];
    }
  }

  function getDraftFieldInputValue(
    session: DraftDimensionSession,
    field: DraftDimensionField,
  ) {
    return draftDimensionFieldInputValue({
      session,
      field,
      focusedField: draftFieldFocusedRef.current,
      rawInputs: draftRawInputRef.current,
      parameterExpressions: draftParameterExpressionRef.current,
      displayUnits,
    });
  }

  function getDraftParameterSuggestions(
    field: DraftDimensionField,
    value: string,
  ) {
    const input = draftDimensionInputRefs.current[field];
    const cursor = input?.selectionStart ?? value.length;
    return getDimensionParameterSuggestions({
      parameters,
      value,
      cursor,
      isAngleDimension: field === "angle",
    });
  }

  function insertDraftParameterSuggestion(
    field: DraftDimensionField,
    name: string,
  ) {
    const input = draftDimensionInputRefs.current[field];
    const currentValue = input?.value ?? draftRawInputRef.current[field] ?? "";
    const cursor = input?.selectionStart ?? currentValue.length;
    const token = parameterTokenAtCursor(currentValue, cursor);
    const start = token?.start ?? cursor;
    const end = token?.end ?? cursor;
    const nextValue =
      currentValue.slice(0, start) + name + currentValue.slice(end);
    handleDraftDimensionChange(field, nextValue);
    window.requestAnimationFrame(() => {
      const nextCursor = start + name.length;
      input?.focus();
      input?.setSelectionRange(nextCursor, nextCursor);
    });
  }

  function focusDraftField(field: DraftDimensionField) {
    window.requestAnimationFrame(() => {
      draftDimensionInputRefs.current[field]?.focus();
      draftDimensionInputRefs.current[field]?.select();
    });
  }

  function handleDraftDimensionKeyDown(
    event: ReactKeyboardEvent<HTMLInputElement>,
    field: DraftDimensionField,
  ) {
    const session = draftDimensionSessionRef.current;
    if (!session) {
      return;
    }
    const suggestions = getDraftParameterSuggestions(
      field,
      event.currentTarget.value,
    );
    handleDraftDimensionFieldKeyDown({
      event,
      field,
      session,
      suggestions,
      draftSuggestionState,
      setDraftSuggestionState,
      insertParameterSuggestion: insertDraftParameterSuggestion,
      commitDraftDimensionSession,
      selectTool,
      cancelActiveSketchDraft,
      setDraftDimensionSession: (next) => {
        draftDimensionSessionRef.current = next;
        setDraftDimensionSession(next);
      },
      focusDraftField,
    });
  }

  return {
    getDraftFieldInputValue,
    getDraftParameterSuggestions,
    handleDraftDimensionBlur,
    handleDraftDimensionChange,
    handleDraftDimensionFocus,
    handleDraftDimensionKeyDown,
    insertDraftParameterSuggestion,
    focusDraftField,
  };
}
