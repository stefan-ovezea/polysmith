import { useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";

import { open } from "@tauri-apps/plugin-dialog";

import { Dropdown } from "@/lib";
import { PreviewPanelActions } from "./PreviewPanelActions";

// Debounced full-parameter patch dispatched to `update_sketch_text`.
// The core merges the patch over the stored record, but the panel
// always sends the COMPLETE set so no field can silently regress when
// the user edits another one. Key names mirror the core's snake_case
// command payload.
export interface SketchTextPanelValue {
  text: string;
  // Absolute path to a user-loaded .ttf; "" = engine default font.
  font_path: string;
  height_mm: number;
  angle_deg: number;
  h_align: "left" | "center" | "right";
  v_align: "top" | "middle" | "bottom";
  char_spacing: number;
  // Text on path: the sketch line/arc the glyphs flow along; null =
  // flat text. When set, angle and v_align are ignored (the curve
  // drives rotation; path_offset drives the above/below shift).
  path_entity_id: string | null;
  path_offset: number;
}

// Core defaults for `add_sketch_text` (see sketch_text_command_handlers).
export const DEFAULT_SKETCH_TEXT_VALUE: SketchTextPanelValue = {
  text: "Text",
  font_path: "",
  height_mm: 10,
  angle_deg: 0,
  h_align: "center",
  v_align: "middle",
  char_spacing: 0,
  path_entity_id: null,
  path_offset: 0,
};

const TEXT_DEBOUNCE_MS = 250;
// Sentinel dropdown option that triggers the font file picker.
const LOAD_FONT_OPTION = "__load_font__";

interface SketchTextPanelProps {
  disabled: boolean;
  // True while the tool is in its "click to place" phase. The panel
  // shows only the placement instructions; the parameter controls
  // appear once a text exists and the panel is bound to it.
  pending: boolean;
  // Parameters the panel is bound to (the freshly-created or picked
  // text). Null in the pending phase. The wrapper remounts the panel
  // (React `key`) whenever the bound text changes, so this only needs
  // to be read at mount.
  initialValue: SketchTextPanelValue | null;
  // Debounced full-parameter update. The wrapper ignores calls while
  // the action is still pending (no text id to update yet).
  onPreviewValue: (value: SketchTextPanelValue) => void | Promise<void>;
  onConfirm: () => void | Promise<void>;
  // Cancel = discard the session. The wrapper deletes the bound text
  // (if any) and returns to Select.
  onCancel: () => void | Promise<void>;
  // True while the path picker is armed: the next viewport click on a
  // sketch line/arc binds it as the text path.
  pathPicking: boolean;
  onArmPathPick: () => void;
  onClearPath: () => void;
}

function parseFinite(text: string, fallback: number) {
  const parsed = Number(text);
  return Number.isFinite(parsed) ? parsed : fallback;
}

function fontFileName(fontPath: string) {
  const segments = fontPath.split(/[\\/]/);
  return segments[segments.length - 1] || fontPath;
}

// contextual modeling floating panel for the 2D sketch Text tool
// (Fusion-style). Pending phase prompts the user to click a placement
// point; every subsequent click creates another text at the click and
// rebinds the panel to it. Each parameter edit debounces a
// full-parameter `update_sketch_text` so the glyphs re-render live in
// the core. Enter in a numeric input confirms the session; Escape
// cancels it (deleting the bound text).
export function SketchTextPanel({
  disabled,
  pending,
  initialValue,
  onPreviewValue,
  onConfirm,
  onCancel,
  pathPicking,
  onArmPathPick,
  onClearPath,
}: SketchTextPanelProps) {
  const { t } = useTranslation();
  const [textValue, setTextValue] = useState(
    initialValue?.text ?? DEFAULT_SKETCH_TEXT_VALUE.text,
  );
  const [heightText, setHeightText] = useState(
    String(initialValue?.height_mm ?? DEFAULT_SKETCH_TEXT_VALUE.height_mm),
  );
  const [angleText, setAngleText] = useState(
    String(initialValue?.angle_deg ?? DEFAULT_SKETCH_TEXT_VALUE.angle_deg),
  );
  const [spacingText, setSpacingText] = useState(
    String(initialValue?.char_spacing ?? DEFAULT_SKETCH_TEXT_VALUE.char_spacing),
  );
  const [pathOffsetText, setPathOffsetText] = useState(
    String(initialValue?.path_offset ?? DEFAULT_SKETCH_TEXT_VALUE.path_offset),
  );
  const [hAlign, setHAlign] = useState<SketchTextPanelValue["h_align"]>(
    initialValue?.h_align ?? DEFAULT_SKETCH_TEXT_VALUE.h_align,
  );
  const [vAlign, setVAlign] = useState<SketchTextPanelValue["v_align"]>(
    initialValue?.v_align ?? DEFAULT_SKETCH_TEXT_VALUE.v_align,
  );
  const [fontPath, setFontPath] = useState(
    initialValue?.font_path ?? DEFAULT_SKETCH_TEXT_VALUE.font_path,
  );
  const [fontError, setFontError] = useState<string | null>(null);
  const debounceTimerRef = useRef<number | null>(null);
  const onPreviewValueRef = useRef(onPreviewValue);
  // When a path is bound, the curve drives rotation and the offset
  // drives vertical placement — angle and v_align are no-ops.
  const pathBound = Boolean(initialValue?.path_entity_id);

  useEffect(() => {
    onPreviewValueRef.current = onPreviewValue;
  }, [onPreviewValue]);

  useEffect(() => {
    return () => {
      if (debounceTimerRef.current !== null) {
        window.clearTimeout(debounceTimerRef.current);
        debounceTimerRef.current = null;
      }
    };
  }, []);

  function buildValue(): SketchTextPanelValue {
    return {
      text: textValue,
      font_path: fontPath,
      height_mm: parseFinite(heightText, DEFAULT_SKETCH_TEXT_VALUE.height_mm),
      angle_deg: parseFinite(angleText, DEFAULT_SKETCH_TEXT_VALUE.angle_deg),
      char_spacing: parseFinite(
        spacingText,
        DEFAULT_SKETCH_TEXT_VALUE.char_spacing,
      ),
      h_align: hAlign,
      v_align: vAlign,
      path_entity_id: initialValue?.path_entity_id ?? null,
      path_offset: parseFinite(
        pathOffsetText,
        DEFAULT_SKETCH_TEXT_VALUE.path_offset,
      ),
    };
  }

  function scheduleUpdate() {
    const next = buildValue();
    if (debounceTimerRef.current !== null) {
      window.clearTimeout(debounceTimerRef.current);
    }
    debounceTimerRef.current = window.setTimeout(() => {
      debounceTimerRef.current = null;
      void onPreviewValueRef.current(next);
    }, TEXT_DEBOUNCE_MS);
  }

  async function flushPendingValue() {
    if (debounceTimerRef.current !== null) {
      window.clearTimeout(debounceTimerRef.current);
      debounceTimerRef.current = null;
    }
    await onPreviewValueRef.current(buildValue());
  }

  async function handleConfirm() {
    if (disabled) {
      return;
    }
    await flushPendingValue();
    await onConfirm();
  }

  async function loadFontFile() {
    try {
      const result = await open({
        title: t("panels.text.loadFontTitle"),
        multiple: false,
        directory: false,
        filters: [
          {
            name: t("panels.text.fontFileType"),
            extensions: ["ttf"],
          },
        ],
      });
      if (result === null || Array.isArray(result)) {
        return; // canceled
      }
      setFontError(null);
      setFontPath(result);
      scheduleUpdate();
    } catch {
      setFontError(t("errors.text.fontLoadFailed"));
    }
  }

  function handleFontChange(value: string) {
    if (value === LOAD_FONT_OPTION) {
      void loadFontFile();
      return;
    }
    setFontError(null);
    setFontPath(value);
    scheduleUpdate();
  }

  const alignButtons: Array<{
    value: SketchTextPanelValue["h_align"];
    label: string;
  }> = [
    { value: "left", label: t("panels.text.alignHLeft") },
    { value: "center", label: t("panels.text.alignHCenter") },
    { value: "right", label: t("panels.text.alignHRight") },
  ];

  const vAlignButtons: Array<{
    value: SketchTextPanelValue["v_align"];
    label: string;
  }> = [
    { value: "top", label: t("panels.text.alignVTop") },
    { value: "middle", label: t("panels.text.alignVMiddle") },
    { value: "bottom", label: t("panels.text.alignVBottom") },
  ];

  const fontOptions: Array<{ value: string; label: string }> = [
    { value: "", label: t("panels.text.fontDefault") },
    ...(fontPath ? [{ value: fontPath, label: fontFileName(fontPath) }] : []),
    { value: LOAD_FONT_OPTION, label: t("panels.text.loadFont") },
  ];

  return (
    <section
      className="pointer-events-auto cad-floating-panel w-72 px-5 py-5"
      onKeyDown={(event) => {
        if (event.key === "Escape") {
          event.preventDefault();
          void onCancel();
        }
      }}
    >
      <p className="cad-kicker">{t("panels.text.title")}</p>
      <p className="mt-3 text-xs text-on-surface-muted">
        {pending
          ? t("panels.text.instructions")
          : t("panels.shortcutHint.confirm")}
      </p>
      {!pending ? (
        <form
          className="mt-4 space-y-4"
          onSubmit={(event) => {
            event.preventDefault();
            void handleConfirm();
          }}
        >
          <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
            {t("panels.text.string")}
            <textarea
              className="cad-input mt-2 min-h-16 w-full resize-y"
              rows={2}
              value={textValue}
              disabled={disabled}
              onChange={(event) => {
                setTextValue(event.target.value);
                scheduleUpdate();
              }}
            />
          </label>
          {/* Text inputs with inputMode="decimal" instead of
              type="number": native number inputs render a theme-
              clashing spinner and fire browser validation bubbles for
              values that don't match their min/step sequence (10 is
              not on the 0.01 + 0.1·k ladder). The core validates the
              parsed value anyway. */}
          <div className="grid grid-cols-3 gap-2">
            <label className="block truncate text-[11px] uppercase tracking-[0.16em] text-on-surface-muted">
              {t("panels.text.height")}
              <input
                className="cad-input mt-1 w-full"
                type="text"
                inputMode="decimal"
                value={heightText}
                disabled={disabled}
                onChange={(event) => {
                  setHeightText(event.target.value);
                  scheduleUpdate();
                }}
                onKeyDown={(event) => {
                  if (event.key === "Enter") {
                    event.preventDefault();
                    void handleConfirm();
                  }
                }}
              />
            </label>
            <label className="block truncate text-[11px] uppercase tracking-[0.16em] text-on-surface-muted">
              {t("panels.text.angle")}
              <input
                className="cad-input mt-1 w-full"
                type="text"
                inputMode="decimal"
                value={angleText}
                disabled={disabled || pathBound}
                title={pathBound ? t("panels.text.angleOnPathIgnored") : undefined}
                onChange={(event) => {
                  setAngleText(event.target.value);
                  scheduleUpdate();
                }}
                onKeyDown={(event) => {
                  if (event.key === "Enter") {
                    event.preventDefault();
                    void handleConfirm();
                  }
                }}
              />
            </label>
            <label className="block truncate text-[11px] uppercase tracking-[0.16em] text-on-surface-muted">
              {t("panels.text.spacing")}
              <input
                className="cad-input mt-1 w-full"
                type="text"
                inputMode="decimal"
                value={spacingText}
                disabled={disabled}
                onChange={(event) => {
                  setSpacingText(event.target.value);
                  scheduleUpdate();
                }}
                onKeyDown={(event) => {
                  if (event.key === "Enter") {
                    event.preventDefault();
                    void handleConfirm();
                  }
                }}
              />
            </label>
          </div>
          <div>
            <p className="cad-kicker">{t("panels.text.alignH")}</p>
            <div className="mt-2 flex gap-2">
              {alignButtons.map((option) => (
                <button
                  key={option.value}
                  type="button"
                  className={
                    hAlign === option.value
                      ? "cad-action-primary flex-1"
                      : "cad-action-ghost flex-1"
                  }
                  disabled={disabled}
                  onClick={() => {
                    setHAlign(option.value);
                    scheduleUpdate();
                  }}
                  onKeyDown={(event) => {
                    if (event.key === "Enter") {
                      event.preventDefault();
                      void handleConfirm();
                    }
                  }}
                >
                  {option.label}
                </button>
              ))}
            </div>
          </div>
          <div>
            <p className="cad-kicker">{t("panels.text.alignV")}</p>
            <div className="mt-2 flex gap-2">
              {vAlignButtons.map((option) => (
                <button
                  key={option.value}
                  type="button"
                  className={
                    vAlign === option.value
                      ? "cad-action-primary flex-1"
                      : "cad-action-ghost flex-1"
                  }
                  disabled={disabled || pathBound}
                  title={pathBound ? t("panels.text.vAlignOnPathIgnored") : undefined}
                  onClick={() => {
                    setVAlign(option.value);
                    scheduleUpdate();
                  }}
                  onKeyDown={(event) => {
                    if (event.key === "Enter") {
                      event.preventDefault();
                      void handleConfirm();
                    }
                  }}
                >
                  {option.label}
                </button>
              ))}
            </div>
          </div>
          <div>
            <p className="cad-kicker">{t("panels.text.path")}</p>
            <div className="mt-2 flex items-center gap-2">
              <button
                type="button"
                className={pathPicking ? "cad-action-primary" : "cad-action-ghost"}
                disabled={disabled}
                onClick={() => {
                  onArmPathPick();
                }}
              >
                {t(pathPicking ? "panels.text.pathPicking" : "panels.text.pathPick")}
              </button>
              {pathBound ? (
                <button
                  type="button"
                  className="cad-action-ghost"
                  disabled={disabled}
                  onClick={() => {
                    onClearPath();
                  }}
                >
                  {t("panels.text.pathClear")}
                </button>
              ) : null}
              <span className="truncate text-xs text-on-surface-muted">
                {pathBound
                  ? t("panels.text.pathBound", {
                      entity: initialValue?.path_entity_id,
                    })
                  : t("panels.text.pathNone")}
              </span>
            </div>
            <label className="mt-3 block text-[11px] uppercase tracking-[0.16em] text-on-surface-muted">
              {t("panels.text.pathOffset")}
              <input
                className="cad-input mt-1 w-full"
                type="text"
                inputMode="decimal"
                value={pathOffsetText}
                disabled={disabled}
                onChange={(event) => {
                  setPathOffsetText(event.target.value);
                  scheduleUpdate();
                }}
                onKeyDown={(event) => {
                  if (event.key === "Enter") {
                    event.preventDefault();
                    void handleConfirm();
                  }
                }}
              />
            </label>
          </div>
          <div>
            <p className="cad-kicker">{t("panels.text.font")}</p>
            <Dropdown
              className="mt-2 w-full"
              buttonClassName="h-8 w-full"
              value={fontPath}
              options={fontOptions}
              label={t("panels.text.font")}
              disabled={disabled}
              onChange={handleFontChange}
            />
            {fontError ? (
              <p className="mt-2 text-xs text-red-300">{fontError}</p>
            ) : null}
          </div>
          <PreviewPanelActions
            confirmDisabled={disabled}
            cancelDisabled={disabled}
            onCancel={() => {
              void onCancel();
            }}
          />
        </form>
      ) : (
        <div className="mt-5">
          <PreviewPanelActions
            confirmDisabled={disabled}
            cancelDisabled={disabled}
            onCancel={() => {
              void onCancel();
            }}
          />
        </div>
      )}
    </section>
  );
}
