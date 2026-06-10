import type {
  Dispatch,
  KeyboardEvent as ReactKeyboardEvent,
  SetStateAction,
} from "react";

import type { DisplayUnits } from "@/utils/units";
import { mmToDisplay, parseDimensionInput } from "@/utils/units";
import type { ParameterEntry } from "@/types";

import {
  draftSessionFields,
  type DraftDimensionField,
  type DraftDimensionSession,
  type ParameterSuggestion,
} from "./draftDimensions";

export type DraftSuggestionState = {
  field: DraftDimensionField;
  index: number;
} | null;

interface DraftFieldInputValueParams {
  session: DraftDimensionSession;
  field: DraftDimensionField;
  focusedField: DraftDimensionField | null;
  rawInputs: Partial<Record<DraftDimensionField, string>>;
  parameterExpressions: Partial<Record<DraftDimensionField, string>>;
  displayUnits: DisplayUnits;
}

interface DraftDimensionKeyDownParams {
  event: ReactKeyboardEvent<HTMLInputElement>;
  field: DraftDimensionField;
  session: DraftDimensionSession;
  suggestions: ParameterSuggestion[];
  draftSuggestionState: DraftSuggestionState;
  setDraftSuggestionState: Dispatch<SetStateAction<DraftSuggestionState>>;
  insertParameterSuggestion: (
    field: DraftDimensionField,
    name: string,
  ) => void;
  commitDraftDimensionSession: (
    session: DraftDimensionSession,
  ) => Promise<void> | void;
  selectTool: () => Promise<void> | void;
  cancelActiveSketchDraft: () => void;
  setDraftDimensionSession: (session: DraftDimensionSession) => void;
  focusDraftField: (field: DraftDimensionField) => void;
}

interface ResolveDraftDimensionFieldValueParams {
  value: string;
  displayUnits: DisplayUnits;
  parameters: ParameterEntry[] | undefined;
}

export interface ResolvedDraftDimensionFieldValue {
  mmValue: string;
  parameterExpression: string | null;
}

export function resolveDraftDimensionFieldValue({
  value,
  displayUnits,
  parameters,
}: ResolveDraftDimensionFieldValueParams): ResolvedDraftDimensionFieldValue {
  const parsed = parseDimensionInput(value, displayUnits);
  if (parsed !== null) {
    return {
      mmValue: String(parsed),
      parameterExpression: null,
    };
  }

  if (!/[a-zA-Z_]/.test(value)) {
    return {
      mmValue: value,
      parameterExpression: null,
    };
  }

  const expression = value.trim();
  const parameter = parameters?.find(
    (candidate) => candidate.name === expression,
  );
  if (
    parameter &&
    !parameter.has_error &&
    Number.isFinite(parameter.resolved_value) &&
    parameter.resolved_value > 0
  ) {
    return {
      mmValue: String(parameter.resolved_value),
      parameterExpression: expression,
    };
  }

  return {
    mmValue: value,
    parameterExpression: expression,
  };
}

export function handleDraftDimensionFieldKeyDown({
  event,
  field,
  session,
  suggestions,
  draftSuggestionState,
  setDraftSuggestionState,
  insertParameterSuggestion,
  commitDraftDimensionSession,
  selectTool,
  cancelActiveSketchDraft,
  setDraftDimensionSession,
  focusDraftField,
}: DraftDimensionKeyDownParams) {
  if (
    navigateDraftSuggestions({
      event,
      field,
      suggestions,
      setDraftSuggestionState,
    })
  ) {
    return;
  }
  if (
    acceptDraftSuggestion({
      event,
      field,
      suggestions,
      draftSuggestionState,
      insertParameterSuggestion,
    })
  ) {
    return;
  }
  if (
    commitOrCancelDraft({
      event,
      session,
      commitDraftDimensionSession,
      selectTool,
      cancelActiveSketchDraft,
    })
  ) {
    return;
  }
  tabToNextDraftField({
    event,
    field,
    session,
    setDraftDimensionSession,
    focusDraftField,
  });
}

export function draftDimensionFieldInputValue({
  session,
  field,
  focusedField,
  rawInputs,
  parameterExpressions,
  displayUnits,
}: DraftFieldInputValueParams) {
  if (focusedField === field && rawInputs[field] !== undefined) {
    return rawInputs[field] ?? "";
  }
  const expression = parameterExpressions[field];
  if (expression && expression.trim().length > 0) {
    return expression;
  }
  return draftDimensionDisplayValue(session.values[field], displayUnits);
}

function draftDimensionDisplayValue(
  rawValue: string,
  displayUnits: DisplayUnits,
): string {
  if (displayUnits === "mm") {
    return rawValue;
  }
  const num = Number(rawValue);
  if (!Number.isFinite(num) || num <= 0) {
    return rawValue;
  }
  const display = mmToDisplay(num, displayUnits);
  return String(parseFloat(display.toFixed(3)));
}

function navigateDraftSuggestions({
  event,
  field,
  suggestions,
  setDraftSuggestionState,
}: Pick<
  DraftDimensionKeyDownParams,
  "event" | "field" | "suggestions" | "setDraftSuggestionState"
>) {
  if (
    suggestions.length === 0 ||
    (event.key !== "ArrowDown" && event.key !== "ArrowUp")
  ) {
    return false;
  }

  event.preventDefault();
  setDraftSuggestionState((current) => {
    const currentIndex = current?.field === field ? current.index : 0;
    const delta = event.key === "ArrowDown" ? 1 : -1;
    return {
      field,
      index: (currentIndex + delta + suggestions.length) % suggestions.length,
    };
  });
  return true;
}

function acceptDraftSuggestion({
  event,
  field,
  suggestions,
  draftSuggestionState,
  insertParameterSuggestion,
}: Pick<
  DraftDimensionKeyDownParams,
  | "event"
  | "field"
  | "suggestions"
  | "draftSuggestionState"
  | "insertParameterSuggestion"
>) {
  if (suggestions.length === 0 || !isSuggestionAcceptKey(event.key)) {
    return false;
  }

  event.preventDefault();
  const suggestion = activeDraftSuggestion({
    field,
    suggestions,
    draftSuggestionState,
  });
  insertParameterSuggestion(field, suggestion.name);
  return true;
}

function isSuggestionAcceptKey(key: string) {
  return key === "Tab" || key === "Enter";
}

function activeDraftSuggestion({
  field,
  suggestions,
  draftSuggestionState,
}: Pick<
  DraftDimensionKeyDownParams,
  "field" | "suggestions" | "draftSuggestionState"
>) {
  const suggestionIndex =
    draftSuggestionState?.field === field ? draftSuggestionState.index : 0;
  return suggestions[suggestionIndex] ?? suggestions[0];
}

function commitOrCancelDraft({
  event,
  session,
  commitDraftDimensionSession,
  selectTool,
  cancelActiveSketchDraft,
}: Pick<
  DraftDimensionKeyDownParams,
  | "event"
  | "session"
  | "commitDraftDimensionSession"
  | "selectTool"
  | "cancelActiveSketchDraft"
>) {
  if (event.key === "Enter") {
    event.preventDefault();
    const result = commitDraftDimensionSession(session);
    if (result && typeof result.then === "function") {
      void result.then(() => { void selectTool(); });
    } else {
      void selectTool();
    }
    return true;
  }
  if (event.key === "Escape") {
    event.preventDefault();
    event.stopPropagation();
    cancelActiveSketchDraft();
    return true;
  }
  return false;
}

function tabToNextDraftField({
  event,
  field,
  session,
  setDraftDimensionSession,
  focusDraftField,
}: Pick<
  DraftDimensionKeyDownParams,
  | "event"
  | "field"
  | "session"
  | "setDraftDimensionSession"
  | "focusDraftField"
>) {
  if (event.key !== "Tab") {
    return;
  }

  event.preventDefault();
  const fields = draftSessionFields(session.tool);
  const index = fields.indexOf(field);
  const nextField =
    fields[(index + (event.shiftKey ? -1 : 1) + fields.length) % fields.length];
  setDraftDimensionSession({ ...session, activeField: nextField });
  focusDraftField(nextField);
}
