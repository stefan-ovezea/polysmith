import { useEffect, useMemo, useRef, useState } from "react";
import { useTranslation } from "react-i18next";

import type { DrawingView, SectionDefinition } from "@/types";

// ── Insert View panel (contextual workflow, P3/P4) ────────────────
//
// Select a body → Insert View → this panel: pick a projected view
// direction (first-angle layout is P5's flatten concern), an ISO 5455
// scale, hidden edges, OR a section view (P4): a cutting plane through
// the referenced bodies' center, normal along a picked axis, with the
// ISO 128-3 hatching defaults.  Enter commits the drawing_view_create,
// Escape cancels — the contextual modeling workflow pattern.  The live
// core-computed projection preview lands with the dimension preview
// plumbing (P6).

const ISO_SCALES = ["0.1", "0.2", "0.5", "1", "2", "5"];

const STANDARD_VIEWS = [
  "front",
  "right",
  "left",
  "top",
  "bottom",
  "back",
] as const;

// Cutting-plane directions: the plane normal points along the picked
// axis, through the referenced bodies' center.
const SECTION_NORMALS: Array<{ key: string; vector: [number, number, number] }> = [
  { key: "+X", vector: [1, 0, 0] },
  { key: "−X", vector: [-1, 0, 0] },
  { key: "+Y", vector: [0, 1, 0] },
  { key: "−Y", vector: [0, -1, 0] },
  { key: "+Z", vector: [0, 0, 1] },
  { key: "−Z", vector: [0, 0, -1] },
];

const HATCH_ANGLES = ["30", "45", "60"];

export interface InsertViewPanelProps {
  disabled: boolean;
  /** Suggested sheet position for the new view (the UI lays views out
   *  side by side; P5 replaces this with first-angle placement). */
  nextSheetPosition: [number, number];
  /** Body ids the view may reference (the selected body, or all). */
  bodyIds: string[];
  /** Union center of the referenced bodies — the default cutting
   *  plane passes through it (a moveable plane point is P5+). */
  sectionPlaneCenter: { x: number; y: number; z: number };
  onCommit: (view: DrawingView) => void;
  onCancel: () => void;
}

export function InsertViewPanel({
  disabled,
  nextSheetPosition,
  bodyIds,
  sectionPlaneCenter,
  onCommit,
  onCancel,
}: InsertViewPanelProps) {
  const { t } = useTranslation();
  const [kind, setKind] = useState<"projection" | "section">("projection");
  const [standardView, setStandardView] = useState<string>("front");
  const [scale, setScale] = useState<string>("1");
  const [showHidden, setShowHidden] = useState(false);
  const [sectionNormal, setSectionNormal] = useState<string>("+X");
  const [cutAway, setCutAway] = useState(true);
  const [sectionLabel, setSectionLabel] = useState("A");
  const [hatchAngle, setHatchAngle] = useState("45");
  const [hatchSpacing, setHatchSpacing] = useState("3");
  const commitRef = useRef(onCommit);
  commitRef.current = onCommit;

  const view = useMemo<DrawingView>(() => {
    const base = {
      view_id: "",
      source_body_ids: bodyIds,
      scale: Number(scale) || 1,
      sheet_position: nextSheetPosition,
      // ISO 128-3 §7: hidden edges are not drawn on sectioned parts.
      show_hidden: kind === "projection" && showHidden,
      warning: "",
    };
    if (kind === "section") {
      const normal = SECTION_NORMALS.find((entry) => entry.key === sectionNormal);
      return {
        ...base,
        kind: "section",
        standard_view: "",
        section: {
          cutting_plane_point: [
            sectionPlaneCenter.x,
            sectionPlaneCenter.y,
            sectionPlaneCenter.z,
          ],
          cutting_plane_normal: normal?.vector ?? [1, 0, 0],
          cut_away: cutAway,
          label: sectionLabel.trim() || "A",
          hatch_angle_deg: Number(hatchAngle) || 45,
          hatch_spacing_mm: Number(hatchSpacing) || 3,
        },
      };
    }
    return { ...base, kind: "projection", standard_view: standardView };
  }, [
    kind,
    standardView,
    scale,
    showHidden,
    sectionNormal,
    cutAway,
    sectionLabel,
    hatchAngle,
    hatchSpacing,
    bodyIds,
    nextSheetPosition,
    sectionPlaneCenter,
  ]);

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

        <div className="flex items-center gap-3">
          <label className="text-xs text-[var(--cad-muted)]">
            {t("drawing.insertPanel.viewKind")}
          </label>
          <div className="flex gap-2">
            <button
              type="button"
              className={
                kind === "projection"
                  ? "cad-ribbon-action cad-ribbon-action-primary"
                  : "cad-ribbon-action"
              }
              disabled={disabled}
              onClick={() => {
                setKind("projection");
              }}
            >
              {t("drawing.insertPanel.projectionKind")}
            </button>
            <button
              type="button"
              className={
                kind === "section"
                  ? "cad-ribbon-action cad-ribbon-action-primary"
                  : "cad-ribbon-action"
              }
              disabled={disabled}
              onClick={() => {
                setKind("section");
              }}
            >
              {t("drawing.insertPanel.sectionKind")}
            </button>
          </div>
        </div>

        {kind === "projection" ? (
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
        ) : (
          <>
            <div>
              <p className="mb-2 text-xs text-[var(--cad-muted)]">
                {t("drawing.insertPanel.cuttingPlane")}
              </p>
              <div className="flex flex-wrap gap-2">
                {SECTION_NORMALS.map((entry) => (
                  <button
                    key={entry.key}
                    type="button"
                    className={
                      sectionNormal === entry.key
                        ? "cad-ribbon-action cad-ribbon-action-primary"
                        : "cad-ribbon-action"
                    }
                    disabled={disabled}
                    onClick={() => {
                      setSectionNormal(entry.key);
                    }}
                  >
                    {entry.key}
                  </button>
                ))}
              </div>
            </div>
            <div className="flex flex-wrap items-center gap-4">
              <label className="flex items-center gap-2 text-xs text-[var(--cad-muted)]">
                <input
                  type="checkbox"
                  checked={cutAway}
                  onChange={(event) => {
                    setCutAway(event.target.checked);
                  }}
                />
                {t("drawing.insertPanel.cutAway")}
              </label>
              <label className="flex items-center gap-2 text-xs text-[var(--cad-muted)]">
                {t("drawing.insertPanel.sectionLabel")}
                <input
                  className="cad-input w-14"
                  value={sectionLabel}
                  maxLength={2}
                  onChange={(event) => {
                    setSectionLabel(event.target.value);
                  }}
                />
              </label>
              <label className="flex items-center gap-2 text-xs text-[var(--cad-muted)]">
                {t("drawing.insertPanel.hatchAngle")}
                <select
                  className="cad-input"
                  value={hatchAngle}
                  onChange={(event) => {
                    setHatchAngle(event.target.value);
                  }}
                >
                  {HATCH_ANGLES.map((value) => (
                    <option key={value} value={value}>
                      {value}°
                    </option>
                  ))}
                </select>
              </label>
              <label className="flex items-center gap-2 text-xs text-[var(--cad-muted)]">
                {t("drawing.insertPanel.hatchSpacing")}
                <input
                  className="cad-input w-16"
                  type="number"
                  min="0.7"
                  max="3"
                  step="0.1"
                  value={hatchSpacing}
                  onChange={(event) => {
                    setHatchSpacing(event.target.value);
                  }}
                />
                <span>mm</span>
              </label>
            </div>
          </>
        )}

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
          {kind === "projection" ? (
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
          ) : null}
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

// ── Section panel (P4) ────────────────────────────────────────────
//
// After a section view is inserted the panel stays open bound to it:
// label, cut-away toggle, direction reverse, and the ISO 128-3 hatch
// angle/spacing.  Enter (or the confirm button) applies through
// drawing_section_update; Escape closes — the contextual workflow
// pattern again.

export interface SectionPanelProps {
  disabled: boolean;
  /** The view's current section definition (the document round-trip
   *  re-syncs the fields after every commit). */
  section: SectionDefinition;
  onCommit: (section: SectionDefinition) => void;
  onClose: () => void;
}

export function SectionPanel({
  disabled,
  section,
  onCommit,
  onClose,
}: SectionPanelProps) {
  const { t } = useTranslation();
  const [label, setLabel] = useState(section.label);
  const [cutAway, setCutAway] = useState(section.cut_away);
  const [hatchAngle, setHatchAngle] = useState(String(section.hatch_angle_deg));
  const [hatchSpacing, setHatchSpacing] = useState(
    String(section.hatch_spacing_mm),
  );
  const commitRef = useRef(onCommit);
  commitRef.current = onCommit;

  // Re-sync when the document round-trip lands with committed values.
  useEffect(() => {
    setLabel(section.label);
    setCutAway(section.cut_away);
    setHatchAngle(String(section.hatch_angle_deg));
    setHatchSpacing(String(section.hatch_spacing_mm));
  }, [section]);

  const built = useMemo<SectionDefinition>(
    () => ({
      ...section,
      label: label.trim() || "A",
      cut_away: cutAway,
      hatch_angle_deg: Number(hatchAngle) || 45,
      hatch_spacing_mm: Number(hatchSpacing) || 3,
    }),
    [section, label, cutAway, hatchAngle, hatchSpacing],
  );

  // Enter commits, Escape cancels.
  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Enter" && !disabled) {
        commitRef.current(built);
      } else if (event.key === "Escape") {
        onClose();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => {
      window.removeEventListener("keydown", onKey);
    };
  }, [disabled, built, onClose]);

  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5">
      <div className="space-y-4">
        <div>
          <p className="cad-kicker">
            {t("drawing.sectionPanel.title", { label: section.label })}
          </p>
        </div>

        <div className="flex flex-wrap items-center gap-4">
          <label className="flex items-center gap-2 text-xs text-[var(--cad-muted)]">
            {t("drawing.insertPanel.sectionLabel")}
            <input
              className="cad-input w-14"
              value={label}
              maxLength={2}
              onChange={(event) => {
                setLabel(event.target.value);
              }}
            />
          </label>
          <label className="flex items-center gap-2 text-xs text-[var(--cad-muted)]">
            <input
              type="checkbox"
              checked={cutAway}
              onChange={(event) => {
                setCutAway(event.target.checked);
              }}
            />
            {t("drawing.sectionPanel.cutAway")}
          </label>
          <button
            type="button"
            className="cad-ribbon-action"
            disabled={disabled}
            onClick={() => {
              // Reverse = flip the cutting plane normal (the material
              // side flips with it — an immediate directional commit).
              commitRef.current({
                ...built,
                cutting_plane_normal: [
                  -section.cutting_plane_normal[0],
                  -section.cutting_plane_normal[1],
                  -section.cutting_plane_normal[2],
                ],
              });
            }}
          >
            {t("drawing.sectionPanel.reverse")}
          </button>
        </div>

        <div className="flex flex-wrap items-center gap-4">
          <label className="flex items-center gap-2 text-xs text-[var(--cad-muted)]">
            {t("drawing.sectionPanel.hatchAngle")}
            <select
              className="cad-input"
              value={hatchAngle}
              onChange={(event) => {
                setHatchAngle(event.target.value);
              }}
            >
              {HATCH_ANGLES.map((value) => (
                <option key={value} value={value}>
                  {value}°
                </option>
              ))}
            </select>
          </label>
          <label className="flex items-center gap-2 text-xs text-[var(--cad-muted)]">
            {t("drawing.sectionPanel.hatchSpacing")}
            <input
              className="cad-input w-16"
              type="number"
              min="0.7"
              max="3"
              step="0.1"
              value={hatchSpacing}
              onChange={(event) => {
                setHatchSpacing(event.target.value);
              }}
            />
            <span>mm</span>
          </label>
        </div>

        <p className="text-xs text-[var(--cad-muted)]">
          {t("drawing.sectionPanel.hint")}
        </p>

        <div className="flex gap-3 pt-1">
          <button
            type="button"
            className="cad-ribbon-action cad-ribbon-action-primary flex-1"
            disabled={disabled}
            onClick={() => {
              commitRef.current(built);
            }}
          >
            {t("common.confirm")}
          </button>
          <button
            type="button"
            className="cad-ribbon-action flex-1"
            onClick={onClose}
          >
            {t("common.cancel")}
          </button>
        </div>
      </div>
    </section>
  );
}
