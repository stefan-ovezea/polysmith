import { useEffect, useMemo, useRef, useState } from "react";
import { useTranslation } from "react-i18next";

import type { DrawingView } from "@/types";

// ── Insert View panel (contextual workflow, P3) ──────────────────
//
// Select a body → Insert View → this panel: pick a standard view
// direction (first-angle layout is P5's flatten concern), an ISO 5455
// scale, and whether hidden edges are shown.  Enter commits the
// drawing_view_create, Escape cancels — the contextual modeling
// workflow pattern.  The live core-computed projection preview lands
// with the dimension preview plumbing (P6).

const ISO_SCALES = ["0.1", "0.2", "0.5", "1", "2", "5"];

const STANDARD_VIEWS = [
  "front",
  "right",
  "left",
  "top",
  "bottom",
  "back",
] as const;

export interface InsertViewPanelProps {
  disabled: boolean;
  /** Suggested sheet position for the new view (the UI lays views out
   *  side by side; P5 replaces this with first-angle placement). */
  nextSheetPosition: [number, number];
  /** Body ids the view may reference (the selected body, or all). */
  bodyIds: string[];
  onCommit: (view: DrawingView) => void;
  onCancel: () => void;
}

export function InsertViewPanel({
  disabled,
  nextSheetPosition,
  bodyIds,
  onCommit,
  onCancel,
}: InsertViewPanelProps) {
  const { t } = useTranslation();
  const [standardView, setStandardView] = useState<string>("front");
  const [scale, setScale] = useState<string>("1");
  const [showHidden, setShowHidden] = useState(false);
  const commitRef = useRef(onCommit);
  commitRef.current = onCommit;

  const view = useMemo<DrawingView>(
    () => ({
      view_id: "",
      kind: "projection",
      standard_view: standardView,
      source_body_ids: bodyIds,
      scale: Number(scale) || 1,
      sheet_position: nextSheetPosition,
      show_hidden: showHidden,
      warning: "",
    }),
    [standardView, scale, showHidden, bodyIds, nextSheetPosition],
  );

  // Enter commits, Escape cancels (the contextual workflow pattern).
  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Enter" && !disabled && bodyIds.length > 0) {
        commitRef.current(view);
      } else if (event.key === "Escape") {
        onCancel();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => {
      window.removeEventListener("keydown", onKey);
    };
  }, [disabled, bodyIds.length, view, onCancel]);

  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5">
      <div className="space-y-4">
        <div>
          <p className="cad-kicker">{t("drawing.insertPanel.title")}</p>
          {bodyIds.length === 0 ? (
            <p className="mt-3 text-sm text-[color:var(--cad-muted)]">
              {t("drawing.insertPanel.noBody")}
            </p>
          ) : (
            <p className="mt-3 text-sm tracking-[0.18em] text-[color:var(--cad-muted)] uppercase">
              {t("drawing.insertPanel.bodySelected", {
                count: bodyIds.length,
              })}
            </p>
          )}
        </div>

        <div className="flex flex-wrap gap-2">
          {STANDARD_VIEWS.map((name) => (
            <button
              key={name}
              type="button"
              className={
                standardView === name
                  ? "cad-ribbon-action cad-ribbon-action-primary"
                  : "cad-ribbon-action"
              }
              disabled={disabled}
              onClick={() => {
                setStandardView(name);
              }}
            >
              {t(`drawing.insertPanel.${name}`)}
            </button>
          ))}
        </div>

        <div className="flex items-center gap-3">
          <label className="text-xs text-[var(--cad-muted)]">
            {t("drawing.insertPanel.scale")}
          </label>
          <select
            className="cad-input"
            value={scale}
            onChange={(event) => {
              setScale(event.target.value);
            }}
          >
            {ISO_SCALES.map((value) => (
              <option key={value} value={value}>
                {value === "1" ? "1:1" : `${value}:1`}
              </option>
            ))}
          </select>
          <label className="flex items-center gap-2 text-xs text-[var(--cad-muted)]">
            <input
              type="checkbox"
              checked={showHidden}
              onChange={(event) => {
                setShowHidden(event.target.checked);
              }}
            />
            {t("drawing.insertPanel.showHidden")}
          </label>
        </div>

        <div className="flex gap-3 pt-1">
          <button
            type="button"
            className="cad-ribbon-action cad-ribbon-action-primary flex-1"
            disabled={disabled || bodyIds.length === 0}
            onClick={() => {
              commitRef.current(view);
            }}
          >
            {t("common.confirm")}
          </button>
          <button
            type="button"
            className="cad-ribbon-action flex-1"
            onClick={onCancel}
          >
            {t("common.cancel")}
          </button>
        </div>
      </div>
    </section>
  );
}
