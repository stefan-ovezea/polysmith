import type {
  KeyboardEvent as ReactKeyboardEvent,
  MutableRefObject,
  RefObject,
} from "react";

import {
  draftSessionFields,
  type DraftDimensionField,
  type DraftDimensionSession,
  type ParameterSuggestion,
} from "./draftDimensions";
import type { DraftSuggestionState } from "./draftDimensionInput";

type ScreenPosition = { x: number; y: number };
type DraftInputRefs = MutableRefObject<
  Partial<Record<DraftDimensionField, HTMLInputElement | null>>
>;

interface DraftDimensionFieldEditorsProps {
  session: DraftDimensionSession | null;
  suggestionState: DraftSuggestionState;
  inputRefs: DraftInputRefs;
  getScreenPosition: (field: DraftDimensionField) => ScreenPosition | null;
  getInputValue: (
    session: DraftDimensionSession,
    field: DraftDimensionField,
  ) => string;
  getSuggestions: (
    field: DraftDimensionField,
    inputValue: string,
  ) => ParameterSuggestion[];
  onSubmit: () => void;
  onChange: (field: DraftDimensionField, value: string) => void;
  onFocus: (field: DraftDimensionField) => void;
  onBlur: (field: DraftDimensionField) => void;
  onKeyDown: (
    event: ReactKeyboardEvent<HTMLInputElement>,
    field: DraftDimensionField,
  ) => void;
  onInsertSuggestion: (field: DraftDimensionField, name: string) => void;
}

interface DimensionEditorOverlayProps {
  visible: boolean;
  editorRef: RefObject<HTMLFormElement | null>;
  inputRef: RefObject<HTMLInputElement | null>;
  draftValue: string;
  suggestions: ParameterSuggestion[];
  suggestionIndex: number;
  onSubmit: () => void;
  onDraftChange: (value: string) => void;
  onFocus: (event: React.FocusEvent<HTMLInputElement>) => void;
  onKeyDown: (event: ReactKeyboardEvent<HTMLInputElement>) => void;
  onInsertSuggestion: (name: string) => void;
}

interface SuggestionListProps {
  suggestions: ParameterSuggestion[];
  activeIndex: number;
  className?: string;
  onSelect: (name: string) => void;
}

export function DraftDimensionFieldEditors({
  session,
  suggestionState,
  inputRefs,
  getScreenPosition,
  getInputValue,
  getSuggestions,
  onSubmit,
  onChange,
  onFocus,
  onBlur,
  onKeyDown,
  onInsertSuggestion,
}: DraftDimensionFieldEditorsProps) {
  if (!session) {
    return null;
  }

  return (
    <>
      {draftSessionFields(session.tool).map((field) => {
        const position = getScreenPosition(field);
        if (!position) {
          return null;
        }
        const inputValue = getInputValue(session, field);
        const suggestions = getSuggestions(field, inputValue);
        const suggestionIndex =
          suggestionState?.field === field ? suggestionState.index : 0;

        return (
          <form
            key={field}
            className="pointer-events-auto absolute z-30 flex w-[120px] items-center rounded-md border px-2 py-1 backdrop-blur-md"
            style={{
              left: position.x,
              top: position.y,
              transform: "translate(-50%, -50%)",
              opacity: 0.65,
              background: "var(--cad-dimension-editor-bg)",
              borderColor: "var(--cad-dimension-editor-border)",
              boxShadow: "0 4px 12px var(--cad-dimension-editor-shadow)",
            }}
            onSubmit={(event) => {
              event.preventDefault();
              onSubmit();
            }}
          >
            <input
              ref={(input) => {
                inputRefs.current[field] = input;
              }}
              className="h-6 w-full bg-transparent text-center text-sm font-semibold text-on-surface tabular-nums outline-none"
              value={inputValue}
              inputMode="text"
              onChange={(event) => {
                onChange(field, event.target.value);
              }}
              onFocus={() => {
                onFocus(field);
              }}
              onBlur={() => {
                onBlur(field);
              }}
              onKeyDown={(event) => {
                onKeyDown(event, field);
              }}
            />
            {suggestions.length > 0 ? (
              <SuggestionList
                suggestions={suggestions}
                activeIndex={suggestionIndex}
                onSelect={(name) => onInsertSuggestion(field, name)}
              />
            ) : null}
          </form>
        );
      })}
    </>
  );
}

export function DimensionEditorOverlay({
  visible,
  editorRef,
  inputRef,
  draftValue,
  suggestions,
  suggestionIndex,
  onSubmit,
  onDraftChange,
  onFocus,
  onKeyDown,
  onInsertSuggestion,
}: DimensionEditorOverlayProps) {
  if (!visible) {
    return null;
  }

  return (
    <form
      ref={editorRef}
      className="pointer-events-none absolute z-20 flex w-[172px] items-center gap-1 rounded-md border px-2 py-1 backdrop-blur-md"
      style={{
        left: 0,
        top: 0,
        opacity: 0,
        background: "var(--cad-dimension-editor-bg)",
        borderColor: "var(--cad-dimension-editor-border)",
        boxShadow: "0 4px 12px var(--cad-dimension-editor-shadow)",
      }}
      onSubmit={(event) => {
        event.preventDefault();
        onSubmit();
      }}
    >
      <input
        ref={inputRef}
        className="h-6 min-w-0 flex-1 bg-transparent text-center text-sm font-medium text-on-surface tabular-nums outline-none pointer-events-none"
        type="text"
        inputMode="text"
        value={draftValue}
        onChange={(event) => {
          onDraftChange(event.target.value);
        }}
        onFocus={onFocus}
        onKeyDown={onKeyDown}
      />

      {suggestions.length > 0 ? (
        <SuggestionList
          suggestions={suggestions}
          activeIndex={suggestionIndex}
          className="pointer-events-auto"
          onSelect={onInsertSuggestion}
        />
      ) : null}
    </form>
  );
}

function SuggestionList({
  suggestions,
  activeIndex,
  className = "",
  onSelect,
}: SuggestionListProps) {
  return (
    <div
      className={`${className} absolute left-0 top-[calc(100%+0.35rem)] w-[220px] overflow-hidden rounded-lg border border-surface-high bg-surface-container py-1 text-left shadow-xl`}
      onMouseDown={(event) => event.preventDefault()}
    >
      {suggestions.map((suggestion, index) => (
        <button
          key={suggestion.name}
          type="button"
          className={`flex w-full items-center justify-between gap-3 px-3 py-1.5 text-xs ${
            index === activeIndex
              ? "bg-surface-bright text-on-surface"
              : "text-on-surface-muted hover:bg-surface-high hover:text-on-surface"
          }`}
          onClick={() => onSelect(suggestion.name)}
        >
          <span className="min-w-0 truncate font-mono">{suggestion.name}</span>
          <span className="shrink-0 text-[10px] uppercase tracking-[0.12em] text-on-surface-dim">
            {suggestion.kind}
          </span>
        </button>
      ))}
    </div>
  );
}
