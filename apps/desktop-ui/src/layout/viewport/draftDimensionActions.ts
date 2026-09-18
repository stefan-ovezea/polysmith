import type {
  Dispatch,
  KeyboardEvent as ReactKeyboardEvent,
  MutableRefObject,
  SetStateAction,
} from "react";

import type { ParameterEntry } from "@/types";
import type { DisplayUnits } from "@/utils/units";
import { makeUiLogEntry } from "@/lib/logger";
import { useCadCoreStore } from "@/state/cadCoreStore";
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
    // The draft must never silently ignore a typed value — when the
    // three_point arc radius is below half the chord (geometrically
    // impossible with fixed ends) the apex was clamped to the
    // semicircle; tell the user why the badge value and the geometry
    // differ.
    if (
      session.tool === "arc" &&
      field === "radius" &&
      session.toolMode !== "center_start_end" &&
      session.secondPoint
    ) {
      const numericValue = Number(mmValue);
      const chord = Math.hypot(
        session.secondPoint[0] - session.start[0],
        session.secondPoint[1] - session.start[1],
      );
      if (
        Number.isFinite(numericValue) &&
        numericValue > 0 &&
        numericValue < chord / 2
      ) {
        useCadCoreStore
          .getState()
          .addLogEntry(
            makeUiLogEntry(
              "warn",
              "draft_dim",
              `Arc radius ${mmValue} is smaller than half the chord ` +
                `(${(chord / 2).toFixed(2)}) — clamped to a semicircle.`,
            ),
          );
      }
    }
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
    // Select the displayed value so typing REPLACES it — the badge is
    // a numeric entry, not a text field, and appending to "23.45" is
    // never what the user wants. Fires on every focus landing
    // (pointer-down auto-focus, Tab cycling, stage transitions).
    window.requestAnimationFrame(() => {
      draftDimensionInputRefs.current[field]?.select();
    });
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
    // Focus-once: pointer moves call this every frame while the field
    // is unlocked. Re-focusing + re-selecting an already-focused input
    // is pure churn — DOM focus flapping and the OS cursor flickering
    // between the hidden canvas cursor and the I-beam, which reads as
    // unexplained jerkiness while dragging. The blur handler nulls
    // draftFieldFocusedRef, so chained lines re-focus on their next
    // segment after commitLineDraft blurs the inputs.
    if (draftFieldFocusedRef.current === field) {
      return;
    }
    // Double-rAF: the badge may not be mounted yet (stage-transition
    // focus — the three_point arc badge only exists from the second
    // click on). The second frame runs after React committed the
    // render, so the input is guaranteed to exist.
    window.requestAnimationFrame(() => {
      window.requestAnimationFrame(() => {
        const input = draftDimensionInputRefs.current[field];
        if (!input) {
          return;
        }
        input.focus();
        input.select();
      });
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
    // First keystroke on the live readout replaces it. The badge shows
    // the live distance while the draft follows the pointer, and every
    // pointer move re-renders the value — which clears any selection
    // the focus handler made, so a bare keystroke would APPEND to the
    // readout ("5" over "23.45" → "23.455"). Select the readout first
    // and the browser's default insertion replaces it. Once the user
    // has typed (raw input exists) keystrokes append normally.
    if (
      event.key.length === 1 &&
      event.key !== " " &&
      !event.ctrlKey &&
      !event.metaKey &&
      !event.altKey &&
      draftRawInputRef.current[field] === undefined
    ) {
      draftDimensionInputRefs.current[field]?.select();
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
