import { useEffect, useMemo, useRef, useState } from "react";
import { useTranslation } from "react-i18next";

import type {
  DrawingDimensionPreviewPayload,
  DrawingSheet,
  DrawingView,
  SectionDefinition,
  TitleBlock,
} from "@/types";

// ── Insert View panel (contextual workflow, P3/P4/P5) ─────────────
//
// Select a body → Insert View → this panel: pick a projected view
// direction — placed around the front view per the sheet's projection
// angle (ISO 128-3: first angle top-below/right-left, third angle
// mirrored), an ISO 5455 scale, hidden edges, OR a section view (P4):
// a cutting plane through the referenced bodies' center, normal along
// a picked axis, with the ISO 128-3 hatching defaults.  Enter commits
// the drawing_view_create, Escape cancels — the contextual modeling
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

// View-direction slots around the front view's bounds, as (column,
// row) multipliers of the base view's size + gap: first angle puts
// the top view BELOW and the right view LEFT of the front view
// (ISO 128-3); third angle mirrors both.
const FIRST_ANGLE_SLOT: Record<string, [number, number]> = {
  top: [0, -1],
  bottom: [0, 1],
  right: [-1, 0],
  left: [1, 0],
};
const THIRD_ANGLE_SLOT: Record<string, [number, number]> = {
  top: [0, 1],
  bottom: [0, -1],
  right: [1, 0],
  left: [-1, 0],
};

export interface InsertViewPanelProps {
  disabled: boolean;
  /** Bumped after each committed projection view (the panel stays
   *  open) — clears the manual position override for the next view. */
  resetToken: number;
  /** Grid fallback position (front itself, sections, no base view). */
  nextSheetPosition: [number, number];
  /** Bounds (sheet-mm) + scale of the front view already on the sheet
   *  — the anchor for first/third-angle placement of further views
   *  and the size reference for the on-sheet clamp. */
  baseViewBounds:
    | { min: [number, number]; max: [number, number]; scale: number }
    | null;
  /** The active sheet's projection angle (placement mirrors on it). */
  projectionAngle: "first_angle" | "third_angle";
  /** Body ids the view may reference (the no-bodies gate). */
  bodyIds: string[];
  /** The compiled bodies with user labels — the Fusion-style body
   *  choice ("All bodies" = the assembly view). */
  availableBodies: Array<{
    id: string;
    label: string;
    center: { x: number; y: number; z: number };
  }>;
  /** The active sheet's trimmed size (the on-sheet clamp). */
  sheetSize: { width_mm: number; height_mm: number };
  /** Mouse-first placement: the cursor's current sheet-mm position
   *  (null = off the sheet) — the ghost follows it until the user
   *  types a manual override. */
  cursorPosition: [number, number] | null;
  /** A click on the sheet commits the view at this point — the token
   *  bumps per click so repeated clicks at the same spot still fire. */
  commitPoint: { token: number; point: [number, number] } | null;
  /** Live ghost: called (debounced) with the uncommitted view
   *  definition on every change, null when the panel closes — the
   *  host sends drawing_view_preview and renders the reply. */
  onPreviewChange: (view: DrawingView | null) => void;
  onCommit: (view: DrawingView) => void;
  onCancel: () => void;
}

/** Clamps a view position so its (estimated) content stays inside the
 *  sheet's 10 mm drawing margin — the first/third-angle slots can
 *  otherwise push views off the paper when the base view sits near an
 *  edge. */
export function clampSheetPosition(
  position: [number, number],
  viewWidthMm: number,
  viewHeightMm: number,
  sheetWidthMm: number,
  sheetHeightMm: number,
): [number, number] {
  const margin = 10;
  const x = Math.min(
    Math.max(position[0], margin),
    Math.max(margin, sheetWidthMm - margin - viewWidthMm),
  );
  const y = Math.min(
    Math.max(position[1], margin),
    Math.max(margin, sheetHeightMm - margin - viewHeightMm),
  );
  return [x, y];
}

export function InsertViewPanel({
  disabled,
  resetToken,
  nextSheetPosition,
  baseViewBounds,
  projectionAngle,
  bodyIds,
  availableBodies,
  sheetSize,
  cursorPosition,
  commitPoint,
  onPreviewChange,
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
  // The Fusion-style body choice: "__all__" = the assembly view.
  const [bodyChoice, setBodyChoice] = useState<string>(() =>
    bodyIds.length === 1 ? bodyIds[0] : "__all__",
  );
  // Manual position override (mm) — null follows the auto slotting.
  const [positionX, setPositionX] = useState<string | null>(null);
  const [positionY, setPositionY] = useState<string | null>(null);
  const commitRef = useRef(onCommit);
  commitRef.current = onCommit;

  // Re-slot automatically when the view choice changes or a view was
  // just committed (the panel stays open); a typed override belongs
  // to the previous slot and must not leak.
  useEffect(() => {
    setPositionX(null);
    setPositionY(null);
  }, [kind, standardView, resetToken]);

  // Projection views slot around the front view per the sheet's
  // projection angle; front/back, sections and the no-base-view case
  // fall back to the side-by-side grid.  The result is clamped into
  // the sheet's drawing area — slots near the sheet edge must not
  // push the view off the paper.
  const sheetPosition = useMemo<[number, number]>(() => {
    const slots =
      projectionAngle === "third_angle" ? THIRD_ANGLE_SLOT : FIRST_ANGLE_SLOT;
    const slot = slots[standardView];
    let position = nextSheetPosition;
    if (kind === "projection" && baseViewBounds && slot) {
      const gap = 20;
      const sx = baseViewBounds.max[0] - baseViewBounds.min[0] + gap;
      const sy = baseViewBounds.max[1] - baseViewBounds.min[1] + gap;
      position = [
        baseViewBounds.min[0] + slot[0] * sx,
        baseViewBounds.min[1] + slot[1] * sy,
      ];
    }
    // The new view's estimated content size: the base view's bounds
    // (the same body) at the chosen scale.
    const scaleRatio = (Number(scale) || 1) / (baseViewBounds?.scale ?? 1);
    const viewW = baseViewBounds
      ? (baseViewBounds.max[0] - baseViewBounds.min[0]) * scaleRatio
      : 0;
    const viewH = baseViewBounds
      ? (baseViewBounds.max[1] - baseViewBounds.min[1]) * scaleRatio
      : 0;
    return clampSheetPosition(position, viewW, viewH, sheetSize.width_mm,
                              sheetSize.height_mm);
  }, [kind, standardView, projectionAngle, baseViewBounds,
      nextSheetPosition, scale, sheetSize]);

  // Position priority: a typed override wins; otherwise the CURSOR
  // (mouse-first placement — the ghost follows the pointer over the
  // sheet); otherwise the auto-slot.
  const finalPosition = useMemo<[number, number]>(() => {
    if (positionX === null && positionY === null && cursorPosition) {
      return cursorPosition;
    }
    if (positionX === null && positionY === null) {
      return sheetPosition;
    }
    return [
      positionX !== null && positionX.trim() !== ""
        ? Number(positionX) || 0
        : cursorPosition?.[0] ?? sheetPosition[0],
      positionY !== null && positionY.trim() !== ""
        ? Number(positionY) || 0
        : cursorPosition?.[1] ?? sheetPosition[1],
    ];
  }, [sheetPosition, positionX, positionY, cursorPosition]);

  // A body chosen earlier may disappear while the panel is open
  // (deleted upstream) — fall back to the assembly view instead of
  // committing a ghost id.
  const effectiveBodyChoice =
    bodyChoice === "__all__" ||
    availableBodies.some((body) => body.id === bodyChoice)
      ? bodyChoice
      : "__all__";

  const chosenBodyIds = useMemo(() => {
    if (effectiveBodyChoice === "__all__") {
      return availableBodies.map((body) => body.id);
    }
    return [effectiveBodyChoice];
  }, [effectiveBodyChoice, availableBodies]);

  // The default cutting plane passes through the CHOSEN bodies'
  // union center (a moveable plane point is P5+).
  const chosenBodyCenter = useMemo(() => {
    const chosen = availableBodies.filter((body) =>
      chosenBodyIds.includes(body.id),
    );
    if (chosen.length === 0) {
      return { x: 0, y: 0, z: 0 };
    }
    return {
      x: chosen.reduce((sum, body) => sum + body.center.x, 0) / chosen.length,
      y: chosen.reduce((sum, body) => sum + body.center.y, 0) / chosen.length,
      z: chosen.reduce((sum, body) => sum + body.center.z, 0) / chosen.length,
    };
  }, [availableBodies, chosenBodyIds]);

  const view = useMemo<DrawingView>(() => {
    const base = {
      view_id: "",
      source_body_ids: chosenBodyIds,
      scale: Number(scale) || 1,
      sheet_position: finalPosition,
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
            chosenBodyCenter.x,
            chosenBodyCenter.y,
            chosenBodyCenter.z,
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
    chosenBodyIds,
    finalPosition,
    chosenBodyCenter,
  ]);

  // Enter commits, Escape cancels (the contextual workflow pattern).
  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Enter" && !disabled && chosenBodyIds.length > 0) {
        commitRef.current(view);
      } else if (event.key === "Escape") {
        onCancel();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => {
      window.removeEventListener("keydown", onKey);
    };
  }, [disabled, chosenBodyIds.length, view, onCancel]);

  // ── Live ghost ─────────────────────────────────────────────────
  // Every definition change sends the uncommitted view to the host
  // (debounced — cursor-follow and typing must not spam the core);
  // the host projects it and the sheet draws the translucent result
  // with its placement frame.
  const previewRef = useRef(onPreviewChange);
  previewRef.current = onPreviewChange;
  useEffect(() => {
    if (disabled || chosenBodyIds.length === 0) {
      previewRef.current(null);
      return;
    }
    const timer = window.setTimeout(() => {
      previewRef.current(view);
    }, 80);
    return () => {
      window.clearTimeout(timer);
    };
  }, [view, disabled, chosenBodyIds.length]);
  // The ghost must never outlive the panel.
  useEffect(() => {
    return () => {
      previewRef.current(null);
    };
  }, []);
  // A click on the sheet commits the view AT the clicked point (the
  // token guards repeated clicks at the same coordinates).
  const lastCommitTokenRef = useRef(0);
  useEffect(() => {
    if (!commitPoint || commitPoint.token === lastCommitTokenRef.current) {
      return;
    }
    lastCommitTokenRef.current = commitPoint.token;
    commitRef.current({ ...view, sheet_position: commitPoint.point });
  }, [commitPoint, view]);

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
            <label className="mt-3 flex items-center gap-2 text-xs text-[var(--cad-muted)]">
              {t("drawing.insertPanel.body")}
              <select
                className="cad-input flex-1"
                value={effectiveBodyChoice}
                disabled={disabled}
                onChange={(event) => {
                  setBodyChoice(event.target.value);
                }}
              >
                {availableBodies.length > 1 ? (
                  <option value="__all__">
                    {t("drawing.insertPanel.bodyAll", {
                      count: availableBodies.length,
                    })}
                  </option>
                ) : null}
                {availableBodies.map((body) => (
                  <option key={body.id} value={body.id}>
                    {body.label}
                  </option>
                ))}
              </select>
            </label>
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

        <div className="flex items-center gap-3">
          <label className="text-xs text-[var(--cad-muted)]">
            {t("drawing.insertPanel.position")}
          </label>
          {/* X/Y sheet-mm, auto-filled from the slotting; typing one
              value overrides it (cleared on view change). */}
          <label className="flex items-center gap-2 text-xs text-[var(--cad-muted)]">
            {t("drawing.insertPanel.positionX")}
            <input
              className="cad-input w-16"
              type="number"
              step="1"
              value={
                positionX !== null
                  ? positionX
                  : String(Math.round(finalPosition[0] * 10) / 10)
              }
              onChange={(event) => {
                setPositionX(event.target.value);
              }}
            />
          </label>
          <label className="flex items-center gap-2 text-xs text-[var(--cad-muted)]">
            {t("drawing.insertPanel.positionY")}
            <input
              className="cad-input w-16"
              type="number"
              step="1"
              value={
                positionY !== null
                  ? positionY
                  : String(Math.round(finalPosition[1] * 10) / 10)
              }
              onChange={(event) => {
                setPositionY(event.target.value);
              }}
            />
          </label>
          <span className="text-xs text-[var(--cad-muted)]">mm</span>
        </div>

        <p className="text-xs text-[var(--cad-muted)]">
          {t("drawing.insertPanel.clickToPlace")}
        </p>

        <div className="flex gap-3 pt-1">
          <button
            type="button"
            className="cad-ribbon-action cad-ribbon-action-primary flex-1"
            disabled={disabled || chosenBodyIds.length === 0}
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

// ── Sheet panel (P5) ──────────────────────────────────────────────
//
// Paper size, orientation and the ISO 5456 projection-angle override
// edit the active sheet live: the selects commit through
// drawing_sheet_update immediately, the name on Enter.  Escape
// closes — the contextual workflow pattern.

const PAPER_SIZES = ["A0", "A1", "A2", "A3", "A4"] as const;

export interface SheetPanelProps {
  disabled: boolean;
  /** The active sheet (the document round-trip re-syncs the fields). */
  sheet: DrawingSheet;
  onCommit: (settings: {
    paper_size: DrawingSheet["paper_size"];
    orientation: DrawingSheet["orientation"];
    projection_angle: DrawingSheet["projection_angle"];
    name: string;
  }) => void;
  /** Opens the ISO 7200 title block editor for this sheet (P7). */
  onOpenTitleBlock?: () => void;
  onClose: () => void;
}

export function SheetPanel({
  disabled,
  sheet,
  onCommit,
  onOpenTitleBlock,
  onClose,
}: SheetPanelProps) {
  const { t } = useTranslation();
  const [name, setName] = useState(sheet.name);
  const commitRef = useRef(onCommit);
  commitRef.current = onCommit;

  // Re-sync when the document round-trip lands with committed values.
  useEffect(() => {
    setName(sheet.name);
  }, [sheet.name]);

  const settings = useMemo(
    () => ({
      paper_size: sheet.paper_size,
      orientation: sheet.orientation,
      projection_angle: sheet.projection_angle,
      name: name.trim() || sheet.name,
    }),
    [
      sheet.paper_size,
      sheet.orientation,
      sheet.projection_angle,
      name,
      sheet.name,
    ],
  );

  // Enter commits the name, Escape cancels.
  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Enter" && !disabled) {
        commitRef.current(settings);
      } else if (event.key === "Escape") {
        onClose();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => {
      window.removeEventListener("keydown", onKey);
    };
  }, [disabled, settings, onClose]);

  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5">
      <div className="space-y-4">
        <div>
          <p className="cad-kicker">{t("drawing.sheetPanel.title")}</p>
        </div>

        <label className="flex items-center gap-2 text-xs text-[var(--cad-muted)]">
          {t("drawing.sheetPanel.name")}
          <input
            className="cad-input w-40"
            value={name}
            onChange={(event) => {
              setName(event.target.value);
            }}
          />
        </label>

        <div className="flex flex-wrap items-center gap-4">
          <label className="flex items-center gap-2 text-xs text-[var(--cad-muted)]">
            {t("drawing.sheetPanel.paperSize")}
            <select
              className="cad-input"
              value={sheet.paper_size}
              onChange={(event) => {
                commitRef.current({
                  ...settings,
                  paper_size: event.target.value as DrawingSheet["paper_size"],
                });
              }}
            >
              {PAPER_SIZES.map((value) => (
                <option key={value} value={value}>
                  {value}
                </option>
              ))}
            </select>
          </label>
          <label className="flex items-center gap-2 text-xs text-[var(--cad-muted)]">
            {t("drawing.sheetPanel.orientation")}
            <select
              className="cad-input"
              value={sheet.orientation}
              onChange={(event) => {
                commitRef.current({
                  ...settings,
                  orientation: event.target
                    .value as DrawingSheet["orientation"],
                });
              }}
            >
              <option value="portrait">
                {t("drawing.sheetPanel.portrait")}
              </option>
              <option value="landscape">
                {t("drawing.sheetPanel.landscape")}
              </option>
            </select>
          </label>
          <label className="flex items-center gap-2 text-xs text-[var(--cad-muted)]">
            {t("drawing.sheetPanel.projectionAngle")}
            <select
              className="cad-input"
              value={sheet.projection_angle}
              onChange={(event) => {
                commitRef.current({
                  ...settings,
                  projection_angle: event.target
                    .value as DrawingSheet["projection_angle"],
                });
              }}
            >
              <option value="first_angle">
                {t("drawing.sheetPanel.firstAngle")}
              </option>
              <option value="third_angle">
                {t("drawing.sheetPanel.thirdAngle")}
              </option>
            </select>
          </label>
        </div>

        <div className="flex gap-3 pt-1">
          <button
            type="button"
            className="cad-ribbon-action cad-ribbon-action-primary flex-1"
            disabled={disabled}
            onClick={() => {
              // The selects already committed live; Confirm applies the
              // name and closes the panel (the contextual Enter/Escape
              // pattern — a confirm that does nothing visible is broken).
              commitRef.current(settings);
              onClose();
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

        {onOpenTitleBlock ? (
          <button
            type="button"
            className="cad-ribbon-action w-full"
            onClick={onOpenTitleBlock}
          >
            {t("drawing.sheetPanel.titleBlock")}
          </button>
        ) : null}
      </div>
    </section>
  );
}

// ── New Drawing setup dialog ───────────────────────────────────────
//
// The "New Drawing" toolbar button opens THIS instead of creating a
// silent default: name, ISO 5457 paper size, orientation and the
// ISO 5456 projection angle are chosen up front (the document-level
// settings, editable later in the Sheet panel).  Enter/Confirm
// commits, Escape cancels — the contextual workflow pattern.

export interface NewDrawingPanelProps {
  disabled: boolean;
  defaultName: string;
  onCommit: (settings: {
    name: string;
    paper_size: "A0" | "A1" | "A2" | "A3" | "A4";
    orientation: "portrait" | "landscape";
    projection_angle: "first_angle" | "third_angle";
  }) => void;
  onClose: () => void;
}

export function NewDrawingPanel({
  disabled,
  defaultName,
  onCommit,
  onClose,
}: NewDrawingPanelProps) {
  const { t } = useTranslation();
  const [name, setName] = useState(defaultName);
  const [paperSize, setPaperSize] = useState<"A0" | "A1" | "A2" | "A3" | "A4">(
    "A4",
  );
  const [orientation, setOrientation] = useState<"portrait" | "landscape">(
    "landscape",
  );
  const [projectionAngle, setProjectionAngle] = useState<
    "first_angle" | "third_angle"
  >("first_angle");

  const settings = useMemo(
    () => ({
      name: name.trim() || defaultName,
      paper_size: paperSize,
      orientation,
      projection_angle: projectionAngle,
    }),
    [name, defaultName, paperSize, orientation, projectionAngle],
  );

  // Enter commits, Escape cancels.
  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Enter" && !disabled) {
        onCommit(settings);
      } else if (event.key === "Escape") {
        onClose();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => {
      window.removeEventListener("keydown", onKey);
    };
  }, [disabled, settings, onCommit, onClose]);

  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5">
      <div className="space-y-4">
        <div>
          <p className="cad-kicker">{t("drawing.newPanel.title")}</p>
        </div>

        <label className="flex items-center gap-2 text-xs text-[var(--cad-muted)]">
          {t("drawing.newPanel.name")}
          <input
            className="cad-input w-40"
            value={name}
            autoFocus
            onChange={(event) => {
              setName(event.target.value);
            }}
          />
        </label>

        <div className="flex flex-wrap items-center gap-4">
          <label className="flex items-center gap-2 text-xs text-[var(--cad-muted)]">
            {t("drawing.newPanel.paperSize")}
            <select
              className="cad-input"
              value={paperSize}
              onChange={(event) => {
                setPaperSize(
                  event.target.value as
                    | "A0"
                    | "A1"
                    | "A2"
                    | "A3"
                    | "A4",
                );
              }}
            >
              {PAPER_SIZES.map((value) => (
                <option key={value} value={value}>
                  {value}
                </option>
              ))}
            </select>
          </label>
          <label className="flex items-center gap-2 text-xs text-[var(--cad-muted)]">
            {t("drawing.newPanel.orientation")}
            <select
              className="cad-input"
              value={orientation}
              onChange={(event) => {
                setOrientation(
                  event.target.value as "portrait" | "landscape",
                );
              }}
            >
              <option value="portrait">
                {t("drawing.newPanel.portrait")}
              </option>
              <option value="landscape">
                {t("drawing.newPanel.landscape")}
              </option>
            </select>
          </label>
          <label className="flex items-center gap-2 text-xs text-[var(--cad-muted)]">
            {t("drawing.newPanel.projectionAngle")}
            <select
              className="cad-input"
              value={projectionAngle}
              onChange={(event) => {
                setProjectionAngle(
                  event.target.value as "first_angle" | "third_angle",
                );
              }}
            >
              <option value="first_angle">
                {t("drawing.newPanel.firstAngle")}
              </option>
              <option value="third_angle">
                {t("drawing.newPanel.thirdAngle")}
              </option>
            </select>
          </label>
        </div>

        <div className="flex gap-3 pt-1">
          <button
            type="button"
            className="cad-ribbon-action cad-ribbon-action-primary flex-1"
            disabled={disabled}
            onClick={() => {
              onCommit(settings);
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

// ── Title block editor (P7) ────────────────────────────────────────
//
// Edits the sheet's ISO 7200 title block: the eight mandatory fields
// plus revision rows (zone/rev/description/date/approved).  Enter
// commits drawing_title_block_update with the whole record; Escape
// cancels.  The document round-trip re-syncs the draft.

export interface TitleBlockPanelProps {
  disabled: boolean;
  /** The active sheet's title block (the round-trip re-syncs it). */
  titleBlock: TitleBlock;
  onCommit: (titleBlock: TitleBlock) => void;
  onClose: () => void;
}

const TITLE_BLOCK_FIELD_KEYS = [
  "legal_owner",
  "identification",
  "date",
  "title",
  "approver",
  "creator",
  "document_type",
] as const;

const REVISION_COLUMN_KEYS = [
  "revisionZone",
  "revisionRev",
  "revisionDescription",
  "revisionDate",
  "revisionApproved",
] as const;

export function TitleBlockPanel({
  disabled,
  titleBlock,
  onCommit,
  onClose,
}: TitleBlockPanelProps) {
  const { t } = useTranslation();
  const [draft, setDraft] = useState<TitleBlock>(titleBlock);
  const commitRef = useRef(onCommit);
  commitRef.current = onCommit;

  // Re-sync when the document round-trip lands with committed values.
  useEffect(() => {
    setDraft(titleBlock);
  }, [titleBlock]);

  // Enter commits the whole record, Escape cancels.
  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Enter" && !disabled) {
        commitRef.current(draft);
      } else if (event.key === "Escape") {
        onClose();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => {
      window.removeEventListener("keydown", onKey);
    };
  }, [disabled, draft, onClose]);

  const setField = (field: (typeof TITLE_BLOCK_FIELD_KEYS)[number], value: string) => {
    setDraft((prev) => ({ ...prev, [field]: value }));
  };
  const setRevisionCell = (
    rowIndex: number,
    column: 0 | 1 | 2 | 3 | 4,
    value: string,
  ) => {
    setDraft((prev) => {
      const rows = prev.revision_rows.map((row, index) =>
        index === rowIndex
          ? (row.map((cell, c) => (c === column ? value : cell)) as [
              string,
              string,
              string,
              string,
              string,
            ])
          : row,
      );
      return { ...prev, revision_rows: rows };
    });
  };

  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5">
      <div className="space-y-4">
        <div>
          <p className="cad-kicker">{t("drawing.titleBlockPanel.title")}</p>
        </div>

        {TITLE_BLOCK_FIELD_KEYS.map((field) => (
          <label
            key={field}
            className="flex items-center gap-2 text-xs text-[var(--cad-muted)]"
          >
            <span className="w-32 shrink-0">
              {t(`drawing.titleBlockPanel.${field}`)}
            </span>
            <input
              className="cad-input flex-1"
              value={draft[field]}
              onChange={(event) => {
                setField(field, event.target.value);
              }}
            />
          </label>
        ))}

        <div>
          <div className="flex items-center justify-between">
            <p className="cad-kicker">{t("drawing.titleBlockPanel.revisions")}</p>
            <button
              type="button"
              className="cad-ribbon-action"
              disabled={disabled}
              onClick={() => {
                setDraft((prev) => ({
                  ...prev,
                  revision_rows: [...prev.revision_rows, ["", "", "", "", ""]],
                }));
              }}
            >
              {t("drawing.titleBlockPanel.addRevision")}
            </button>
          </div>
          <div className="mt-2 space-y-1">
            {draft.revision_rows.length === 0 ? (
              <p className="text-xs text-[var(--cad-muted)]">
                {t("drawing.titleBlockPanel.noRevisions")}
              </p>
            ) : (
              draft.revision_rows.map((row, rowIndex) => (
                <div key={rowIndex} className="flex items-center gap-1">
                  {REVISION_COLUMN_KEYS.map((columnKey, column) => (
                    <input
                      key={columnKey}
                      className="cad-input flex-1"
                      placeholder={t(
                        `drawing.titleBlockPanel.${columnKey}`,
                      )}
                      value={row[column]}
                      onChange={(event) => {
                        setRevisionCell(
                          rowIndex,
                          column as 0 | 1 | 2 | 3 | 4,
                          event.target.value,
                        );
                      }}
                    />
                  ))}
                  <button
                    type="button"
                    className="cad-ribbon-action"
                    disabled={disabled}
                    onClick={() => {
                      setDraft((prev) => ({
                        ...prev,
                        revision_rows: prev.revision_rows.filter(
                          (_, index) => index !== rowIndex,
                        ),
                      }));
                    }}
                  >
                    ✕
                  </button>
                </div>
              ))
            )}
          </div>
        </div>

        <div className="flex gap-3 pt-1">
          <button
            type="button"
            className="cad-ribbon-action cad-ribbon-action-primary flex-1"
            disabled={disabled}
            onClick={() => {
              commitRef.current(draft);
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

// ── Dimension panel (P6) ──────────────────────────────────────────
//
// The Dimension tool arms a sheet pick: every click appends a pick
// (max 2 — the second turns a linear into a distance/angle), the
// core preview replies with the measured value + graphics (drawn
// live on the sheet), Enter commits drawing_dimension_create with
// the picks.  Escape closes — the contextual workflow pattern.

const DIMENSION_KINDS = ["linear", "radius", "diameter", "angular"] as const;

const DIMENSION_KIND_LABEL_KEYS: Record<(typeof DIMENSION_KINDS)[number], string> = {
  linear: "drawing.dimensionPanel.kindLinear",
  radius: "drawing.dimensionPanel.kindRadius",
  diameter: "drawing.dimensionPanel.kindDiameter",
  angular: "drawing.dimensionPanel.kindAngular",
};

export interface DimensionPanelProps {
  disabled: boolean;
  /** Picks accumulated in sheet-mm (the pick itself is just a point —
   *  the core resolves the nearest edge). */
  picks: Array<[number, number]>;
  dimType: "linear" | "radius" | "diameter" | "angular";
  /** The latest core preview (value + graphics + error). */
  preview: DrawingDimensionPreviewPayload | null;
  onDimTypeChange: (kind: "linear" | "radius" | "diameter" | "angular") => void;
  onClearPicks: () => void;
  onCommit: () => void;
  onClose: () => void;
}

export function DimensionPanel({
  disabled,
  picks,
  dimType,
  preview,
  onDimTypeChange,
  onClearPicks,
  onCommit,
  onClose,
}: DimensionPanelProps) {
  const { t } = useTranslation();
  const commitRef = useRef(onCommit);
  commitRef.current = onCommit;

  const canCommit = picks.length > 0 && !preview?.error;

  // Enter commits, Escape cancels.
  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Enter" && !disabled && canCommit) {
        commitRef.current();
      } else if (event.key === "Escape") {
        onClose();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => {
      window.removeEventListener("keydown", onKey);
    };
  }, [disabled, canCommit, onClose]);

  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5">
      <div className="space-y-4">
        <div>
          <p className="cad-kicker">{t("drawing.dimensionPanel.title")}</p>
          <p className="mt-3 text-sm text-[color:var(--cad-muted)]">
            {picks.length === 0
              ? t("drawing.dimensionPanel.pickHint")
              : t("drawing.dimensionPanel.pickCount", { count: picks.length })}
          </p>
        </div>

        <div>
          <p className="mb-2 text-xs text-[var(--cad-muted)]">
            {t("drawing.dimensionPanel.kind")}
          </p>
          <div className="flex flex-wrap gap-2">
            {DIMENSION_KINDS.map((kind) => (
              <button
                key={kind}
                type="button"
                className={
                  dimType === kind
                    ? "cad-ribbon-action cad-ribbon-action-primary"
                    : "cad-ribbon-action"
                }
                disabled={disabled}
                onClick={() => {
                  onDimTypeChange(kind);
                }}
              >
                {t(DIMENSION_KIND_LABEL_KEYS[kind])}
              </button>
            ))}
          </div>
        </div>

        {preview ? (
          preview.error ? (
            <p className="text-xs text-[color:var(--cad-danger)]">
              {preview.error}
            </p>
          ) : (
            <p className="text-2xl font-semibold tracking-wide">
              {preview.text_value}
            </p>
          )
        ) : null}

        <div className="flex gap-3 pt-1">
          <button
            type="button"
            className="cad-ribbon-action cad-ribbon-action-primary flex-1"
            disabled={disabled || !canCommit}
            onClick={() => {
              commitRef.current();
            }}
          >
            {t("common.confirm")}
          </button>
          <button
            type="button"
            className="cad-ribbon-action flex-1"
            disabled={picks.length === 0}
            onClick={onClearPicks}
          >
            {t("drawing.dimensionPanel.clearPicks")}
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
