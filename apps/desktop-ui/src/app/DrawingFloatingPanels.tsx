import {
  useEffect,
  useMemo,
  useRef,
  useState,
  type ReactNode,
} from "react";
import { useTranslation } from "react-i18next";
import { open } from "@tauri-apps/plugin-dialog";

import { Dropdown } from "@/lib/components/Dropdown";
import type {
  DrawingAnnotationKind,
  DrawingAnnotationPreviewPayload,
  DrawingDimensionPreviewPayload,
  DrawingSheet,
  DrawingTemplate,
  DrawingView,
  SectionDefinition,
  TitleBlock,
} from "@/types";
import {
  bodyCenterForChoice,
  clampSheetPosition,
  HATCH_ANGLES,
  ISO_SCALES,
  SECTION_NORMALS,
} from "@/lib/drawingViewMath";

// ── Section view creation panel (R1, the old InsertViewPanel's
// section mode) ──────────────────────────────────────────────────────
//
// VIEWS → Section arms the section tool; this panel carries the
// cutting-plane settings (axis, cut-away, label, ISO 128-3 hatch)
// while the ghost follows the cursor over the sheet.  Enter or a
// click commits drawing_view_create; Escape cancels — the contextual
// modeling workflow pattern.  The committed SectionPanel (below)
// takes over for label / cut-away / hatch edits.

export interface SectionViewPanelProps {
  disabled: boolean;
  /** Grid fallback position (no cursor over the sheet yet). */
  nextSheetPosition: [number, number];
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
   *  (null = off the sheet) — the ghost follows it. */
  cursorPosition: [number, number] | null;
  /** A click on the sheet commits the view at this point — the token
   *  bumps per click so repeated clicks at the same spot still fire. */
  commitPoint: { token: number; point: [number, number] } | null;
  /** Live ghost: called (debounced) with the uncommitted view
   *  definition on every change, null when the panel closes. */
  onPreviewChange: (view: DrawingView | null) => void;
  onCommit: (view: DrawingView) => void;
  onCancel: () => void;
}

export function SectionViewPanel({
  disabled,
  nextSheetPosition,
  bodyIds,
  availableBodies,
  sheetSize,
  cursorPosition,
  commitPoint,
  onPreviewChange,
  onCommit,
  onCancel,
}: SectionViewPanelProps) {
  const { t } = useTranslation();
  const [scale, setScale] = useState<string>("1");
  const [sectionNormal, setSectionNormal] = useState<string>("+X");
  const [cutAway, setCutAway] = useState(true);
  const [sectionLabel, setSectionLabel] = useState("A");
  const [hatchAngle, setHatchAngle] = useState("45");
  const [hatchSpacing, setHatchSpacing] = useState("3");
  // The Fusion-style body choice: "__all__" = the assembly view.
  const [bodyChoice, setBodyChoice] = useState<string>(() =>
    bodyIds.length === 1 ? bodyIds[0] : "__all__",
  );
  const commitRef = useRef(onCommit);
  commitRef.current = onCommit;

  // The cursor position when over the sheet, else the grid fallback
  // (sections never slot around a base view).
  const sheetPosition = useMemo<[number, number]>(() => {
    const position = cursorPosition ?? nextSheetPosition;
    return clampSheetPosition(
      position,
      0,
      0,
      sheetSize.width_mm,
      sheetSize.height_mm,
    );
  }, [cursorPosition, nextSheetPosition, sheetSize]);

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
  const chosenBodyCenter = bodyCenterForChoice(
    effectiveBodyChoice,
    availableBodies,
  );

  const view = useMemo<DrawingView>(() => {
    const normal = SECTION_NORMALS.find((entry) => entry.key === sectionNormal);
    return {
      view_id: "",
      kind: "section",
      standard_view: "",
      source_body_ids: chosenBodyIds,
      scale: Number(scale) || 1,
      sheet_position: sheetPosition,
      // ISO 128-3 §7: hidden edges are not drawn on sectioned parts.
      show_hidden: false,
      section: {
        cutting_plane_point: [
          chosenBodyCenter[0],
          chosenBodyCenter[1],
          chosenBodyCenter[2],
        ],
        cutting_plane_normal: normal?.vector ?? [1, 0, 0],
        cut_away: cutAway,
        label: sectionLabel.trim() || "A",
        hatch_angle_deg: Number(hatchAngle) || 45,
        hatch_spacing_mm: Number(hatchSpacing) || 3,
      },
      warning: "",
    };
  }, [
    scale,
    sectionNormal,
    cutAway,
    sectionLabel,
    hatchAngle,
    hatchSpacing,
    chosenBodyIds,
    sheetPosition,
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
          <p className="cad-kicker">{t("drawing.sectionCreatePanel.title")}</p>
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

// ── Create Drawing dialog (Fusion-style) ───────────────────────────
//
// The "New Drawing" ribbon button opens THIS: Drawing Type
// (Automatic = auto-places a front base view, Manual = empty
// drawing), Contents (which bodies the auto view references) and a
// collapsible Destination section (standard/units/sheet settings +
// template create/load).  Enter/OK commits, Escape cancels — the
// contextual workflow pattern.
//
// Templates are setup-only JSON {name, sheets[]} files: the core owns
// the file I/O (drawing_template_save/load); the panel drives the
// pickers.  A template loaded here keeps its per-sheet definitions
// verbatim (including title blocks — the dialog itself cannot edit
// title blocks, so templates SAVED from here carry empty ones).

export type CreateDrawingType = "automatic" | "manual";
export type CreateDrawingContents = "all" | "selected" | "pick";

export interface CreateDrawingSettings {
  name: string;
  drawing_type: CreateDrawingType;
  contents: CreateDrawingContents;
  /** The dialog-built sheets (sheetCount copies of the controls). */
  sheets: Array<{
    name: string;
    paper_size: "A0" | "A1" | "A2" | "A3" | "A4";
    orientation: "portrait" | "landscape";
    projection_angle: "first_angle" | "third_angle";
  }>;
  /** A loaded template's sheets verbatim (per-sheet variance + title
   *  blocks ride through) — null when the dialog builds fresh. */
  template_sheets: DrawingSheet[] | null;
}

export interface NewDrawingPanelProps {
  disabled: boolean;
  defaultName: string;
  /** The Selected-contents gate (button disabled while null). */
  selectedBodyId: string | null;
  /** App-data drawing templates dir (the load picker's default). */
  templatesDir: string;
  onCommit: (settings: CreateDrawingSettings) => void;
  onClose: () => void;
  onSaveTemplate: (template: DrawingTemplate) => Promise<void>;
  onLoadTemplate: (filePath: string) => Promise<DrawingTemplate>;
}

type PaperSize = "A0" | "A1" | "A2" | "A3" | "A4";
type SheetOrientation = "portrait" | "landscape";

/** ISO 5457 trimmed PORTRAIT sizes (short × long, sheet-mm) — the
 *  landscape orientation swaps them.  Mirrors the core's
 *  paper_size_mm (drawing_sheet.cpp). */
const PAPER_SIZE_MM: Record<PaperSize, [number, number]> = {
  A0: [841, 1189],
  A1: [594, 841],
  A2: [420, 594],
  A3: [297, 420],
  A4: [210, 297],
};

const EMPTY_TITLE_BLOCK: TitleBlock = {
  legal_owner: "",
  identification: "",
  date: "",
  title: "",
  approver: "",
  creator: "",
  document_type: "",
  revision_rows: [],
};

/** The panel's radio-style toggle button (DrawingRibbon's pattern). */
function PanelToggle({
  active,
  onClick,
  disabled,
  tooltip,
  children,
}: {
  active: boolean;
  onClick?: () => void;
  disabled?: boolean;
  tooltip?: string;
  children: ReactNode;
}) {
  const state = active
    ? "cad-ribbon-action cad-ribbon-action-primary"
    : "cad-ribbon-action";
  return (
    <button
      type="button"
      className={`${state} flex h-8 items-center gap-1 px-2 leading-none`}
      disabled={disabled}
      data-tooltip={tooltip}
      onClick={onClick}
    >
      {children}
    </button>
  );
}

/** A label-left / control-right form row. */
function PanelRow({
  label,
  children,
}: {
  label: string;
  children: ReactNode;
}) {
  return (
    <div className="flex items-center gap-2">
      <span className="w-28 shrink-0 text-xs text-[var(--cad-muted)]">
        {label}
      </span>
      {children}
    </div>
  );
}

export function NewDrawingPanel({
  disabled,
  defaultName,
  selectedBodyId,
  templatesDir,
  onCommit,
  onClose,
  onSaveTemplate,
  onLoadTemplate,
}: NewDrawingPanelProps) {
  const { t } = useTranslation();
  const [name, setName] = useState(defaultName);
  const [drawingType, setDrawingType] = useState<CreateDrawingType>(
    "automatic",
  );
  const [contents, setContents] = useState<CreateDrawingContents>("all");
  const [destinationCollapsed, setDestinationCollapsed] = useState(false);
  const [drawingChoice, setDrawingChoice] = useState<"new" | "template">(
    "new",
  );
  const [template, setTemplate] = useState<DrawingTemplate | null>(null);
  const [templateError, setTemplateError] = useState<string | null>(null);
  const [paperSize, setPaperSize] = useState<PaperSize>("A4");
  const [orientation, setOrientation] = useState<SheetOrientation>(
    "landscape",
  );
  const [projectionAngle, setProjectionAngle] = useState<
    "first_angle" | "third_angle"
  >("first_angle");
  const [sheetCount, setSheetCount] = useState("1");

  // Sheet size → trimmed Width/Height (the Fusion dialog's read-only
  // derived row; the core's flatten applies the same swap).
  const [widthMm, heightMm] = useMemo(() => {
    const [short, long] = PAPER_SIZE_MM[paperSize];
    return orientation === "landscape" ? [long, short] : [short, long];
  }, [paperSize, orientation]);

  const settings = useMemo<CreateDrawingSettings>(() => {
    const count = Math.min(
      Math.max(parseInt(sheetCount, 10) || 1, 1),
      99,
    );
    return {
      name: name.trim() || defaultName,
      drawing_type: drawingType,
      contents,
      sheets: Array.from({ length: count }, (_, index) => ({
        name: t("drawing.newPanel.sheetName", { number: index + 1 }),
        paper_size: paperSize,
        orientation,
        projection_angle: projectionAngle,
      })),
      template_sheets:
        drawingChoice === "template" && template ? template.sheets : null,
    };
  }, [
    name,
    defaultName,
    drawingType,
    contents,
    sheetCount,
    paperSize,
    orientation,
    projectionAngle,
    drawingChoice,
    template,
    t,
  ]);

  const browseTemplate = async () => {
    const path = await open({
      title: t("drawing.newPanel.drawingBrowseTemplate"),
      multiple: false,
      defaultPath: templatesDir,
      filters: [{ name: "PolySmith drawing template", extensions: ["json"] }],
    });
    if (!path || typeof path !== "string") {
      setDrawingChoice("new");
      return;
    }
    try {
      const loaded = await onLoadTemplate(path);
      setTemplate(loaded);
      setDrawingChoice("template");
      setTemplateError(null);
    } catch {
      setTemplateError(t("drawing.newPanel.templateLoadFailed"));
      setDrawingChoice("new");
    }
  };

  const saveTemplate = () => {
    // One canonical sheet — sheetCount copies would be redundant
    // (the template holds SETTINGS, not the sheet run).
    void onSaveTemplate({
      name: name.trim() || defaultName,
      sheets: [
        {
          sheet_id: "",
          name: t("drawing.defaultSheet"),
          paper_size: paperSize,
          orientation,
          projection_angle: projectionAngle,
          view_ids: [],
          title_block: EMPTY_TITLE_BLOCK,
        },
      ],
    });
  };

  // Enter commits, Escape cancels — never while a dropdown menu is
  // open (Enter/Escape belong to the menu then).
  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if ((event.target as HTMLElement | null)?.closest(".cad-dropdown")) {
        return;
      }
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
    <section className="pointer-events-auto cad-floating-panel w-[360px] px-5 py-5">
      <div className="space-y-4">
        {/* Header: title + the Destination collapse chevron. */}
        <div className="flex items-center justify-between">
          <p className="cad-kicker">{t("drawing.newPanel.title")}</p>
          <button
            type="button"
            className="px-1 py-1 text-[9px] text-[var(--cad-muted)] hover:text-on-surface"
            aria-label={
              destinationCollapsed
                ? t("drawing.newPanel.destinationExpand")
                : t("drawing.newPanel.destinationCollapse")
            }
            onClick={() => {
              setDestinationCollapsed((current) => !current);
            }}
          >
            {destinationCollapsed ? "▸" : "▾"}
          </button>
        </div>

        <label className="flex items-center gap-2">
          <span className="w-28 shrink-0 text-xs text-[var(--cad-muted)]">
            {t("drawing.newPanel.name")}
          </span>
          <input
            className="cad-input flex-1"
            value={name}
            autoFocus
            onChange={(event) => {
              setName(event.target.value);
            }}
          />
        </label>

        {/* Drawing Type: Automatic (auto base view) / Manual (empty). */}
        <PanelRow label={t("drawing.newPanel.drawingType")}>
          <PanelToggle
            active={drawingType === "automatic"}
            onClick={() => {
              setDrawingType("automatic");
            }}
          >
            {t("drawing.newPanel.automatic")}
          </PanelToggle>
          <PanelToggle
            active={drawingType === "manual"}
            onClick={() => {
              setDrawingType("manual");
            }}
          >
            {t("drawing.newPanel.manual")}
          </PanelToggle>
        </PanelRow>

        {/* Contents: which bodies the auto base view references. */}
        {drawingType === "automatic" && (
          <PanelRow label={t("drawing.newPanel.contents")}>
            <PanelToggle
              active={contents === "all"}
              onClick={() => {
                setContents("all");
              }}
            >
              {t("drawing.newPanel.contentsAll")}
            </PanelToggle>
            <PanelToggle
              active={contents === "selected"}
              disabled={!selectedBodyId}
              tooltip={
                selectedBodyId
                  ? undefined
                  : t("drawing.newPanel.contentsSelectedHint")
              }
              onClick={() => {
                setContents("selected");
              }}
            >
              {t("drawing.newPanel.contentsSelected")}
            </PanelToggle>
            <PanelToggle
              active={contents === "pick"}
              onClick={() => {
                setContents("pick");
              }}
            >
              {t("drawing.newPanel.contentsPick")}
            </PanelToggle>
          </PanelRow>
        )}

        {/* Destination: the collapsible sheet/template settings. */}
        {!destinationCollapsed && (
          <div className="space-y-3 border-t border-white/10 pt-3">
            <PanelRow label={t("drawing.newPanel.drawing")}>
              <Dropdown
                value={drawingChoice}
                options={[
                  {
                    value: "new",
                    label: t("drawing.newPanel.drawingCreateNew"),
                  },
                  {
                    value: "template",
                    label: t("drawing.newPanel.drawingBrowseTemplate"),
                  },
                ]}
                label={t("drawing.newPanel.drawing")}
                onChange={(value) => {
                  if (value === "template") {
                    void browseTemplate();
                  } else {
                    setDrawingChoice("new");
                    setTemplate(null);
                    setTemplateError(null);
                  }
                }}
                buttonClassName="flex-1"
              />
              <button
                type="button"
                className="cad-ribbon-action h-8 px-2 leading-none"
                onClick={saveTemplate}
              >
                {t("drawing.newPanel.createTemplate")}
              </button>
            </PanelRow>

            {templateError != null && (
              <p className="text-xs text-danger">{templateError}</p>
            )}
            {drawingChoice === "template" && template != null && (
              <p className="text-xs text-[var(--cad-muted)]">
                {t("drawing.newPanel.templateLoadedSummary", {
                  name: template.name,
                  count: template.sheets.length,
                })}
              </p>
            )}

            <PanelRow label={t("drawing.newPanel.baseDocument")}>
              <PanelToggle active={true}>
                {t("drawing.newPanel.baseNew")}
              </PanelToggle>
              <PanelToggle
                active={false}
                disabled
                tooltip={t("drawing.newPanel.baseReferenceTitle")}
              >
                {t("drawing.newPanel.baseReference")}
              </PanelToggle>
            </PanelRow>

            <PanelRow label={t("drawing.newPanel.standard")}>
              <Dropdown
                value="iso"
                options={[{ value: "iso", label: "ISO" }]}
                label={t("drawing.newPanel.standard")}
                buttonClassName="flex-1"
              />
            </PanelRow>

            <PanelRow label={t("drawing.newPanel.units")}>
              <Dropdown
                value="mm"
                options={[{ value: "mm", label: "mm" }]}
                label={t("drawing.newPanel.units")}
                buttonClassName="flex-1"
              />
            </PanelRow>

            <PanelRow label={t("drawing.newPanel.sheetSize")}>
              <Dropdown
                value={paperSize}
                options={PAPER_SIZES.map((value) => ({ value, label: value }))}
                label={t("drawing.newPanel.sheetSize")}
                onChange={(value) => {
                  setPaperSize(value);
                }}
                buttonClassName="flex-1"
              />
            </PanelRow>

            <PanelRow label={t("drawing.newPanel.width")}>
              <span className="text-sm text-[var(--cad-muted)]">
                {widthMm}
                <span className="cad-metric">mm</span>
              </span>
              <span className="text-sm text-[var(--cad-muted)]">
                {t("drawing.newPanel.height")}
              </span>
              <span className="text-sm text-[var(--cad-muted)]">
                {heightMm}
                <span className="cad-metric">mm</span>
              </span>
            </PanelRow>

            <PanelRow label={t("drawing.newPanel.orientation")}>
              <PanelToggle
                active={orientation === "portrait"}
                onClick={() => {
                  setOrientation("portrait");
                }}
              >
                {t("drawing.newPanel.portrait")}
              </PanelToggle>
              <PanelToggle
                active={orientation === "landscape"}
                onClick={() => {
                  setOrientation("landscape");
                }}
              >
                {t("drawing.newPanel.landscape")}
              </PanelToggle>
            </PanelRow>

            <PanelRow label={t("drawing.newPanel.sheetCount")}>
              <input
                className="cad-input w-16"
                inputMode="numeric"
                value={sheetCount}
                onChange={(event) => {
                  if (/^\d{0,2}$/.test(event.target.value)) {
                    setSheetCount(event.target.value);
                  }
                }}
              />
            </PanelRow>

            <PanelRow label={t("drawing.newPanel.projectionAngle")}>
              <PanelToggle
                active={projectionAngle === "first_angle"}
                onClick={() => {
                  setProjectionAngle("first_angle");
                }}
              >
                {t("drawing.newPanel.firstAngle")}
              </PanelToggle>
              <PanelToggle
                active={projectionAngle === "third_angle"}
                onClick={() => {
                  setProjectionAngle("third_angle");
                }}
              >
                {t("drawing.newPanel.thirdAngle")}
              </PanelToggle>
            </PanelRow>
          </div>
        )}

        {/* Footer: info summary left, OK/Cancel right. */}
        <div className="flex items-center gap-2 pt-1">
          <button
            type="button"
            className="flex h-5 w-5 items-center justify-center rounded-full border border-white/20 text-[10px] text-[var(--cad-muted)]"
            data-tooltip={t("drawing.newPanel.infoSummary", {
              paperSize,
              orientation: t(
                orientation === "portrait"
                  ? "drawing.newPanel.portrait"
                  : "drawing.newPanel.landscape",
              ),
            })}
          >
            i
          </button>
          <div className="flex-1" />
          <button
            type="button"
            className="cad-ribbon-action cad-ribbon-action-primary"
            disabled={disabled}
            onClick={() => {
              onCommit(settings);
            }}
          >
            {t("common.ok")}
          </button>
          <button
            type="button"
            className="cad-ribbon-action"
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

// ── Annotation panel (GEOMETRY / SYMBOLS / ANNOTATE tabs) ───────────
//
// The pick→preview→Enter flow mirrors DimensionPanel: picks
// accumulate (max 2 — the second circle of a centerline), the core
// preview replies with the attachment graphics, Enter commits
// drawing_annotation_create.  P1 ships leader_text; the
// GEOMETRY/SYMBOLS kinds join in P2/P3.

export interface AnnotationPanelProps {
  disabled: boolean;
  /** The armed annotation kind (P1: "leader_text"). */
  kind: DrawingAnnotationKind;
  /** Picks accumulated in sheet-mm (the pick is just a point — the
   *  core resolves the nearest edge). */
  picks: Array<[number, number]>;
  /** The latest core preview (graphics + error). */
  preview: DrawingAnnotationPreviewPayload | null;
  /** The annotation's rendered text (leader_text content). */
  text: string;
  onTextChange: (text: string) => void;
  onClearPicks: () => void;
  onCommit: () => void;
  onClose: () => void;
}

export function AnnotationPanel({
  disabled,
  kind,
  picks,
  preview,
  text,
  onTextChange,
  onClearPicks,
  onCommit,
  onClose,
}: AnnotationPanelProps) {
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
          <p className="cad-kicker">
            {t(`drawing.annotationPanel.kind${
              kind
                .split("_")
                .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
                .join("")
            }`)}
          </p>
          <p className="mt-3 text-sm text-[color:var(--cad-muted)]">
            {picks.length === 0
              ? t("drawing.annotationPanel.pickHint")
              : picks.length === 1 && kind === "centerline"
                ? t("drawing.annotationPanel.pickTwoHint")
                : t("drawing.annotationPanel.pickCount", { count: picks.length })}
          </p>
        </div>

        <div>
          <p className="mb-2 text-xs text-[var(--cad-muted)]">
            {t("drawing.annotationPanel.textLabel")}
          </p>
          <input
            className="cad-input w-full"
            type="text"
            value={text}
            disabled={disabled}
            placeholder={t("drawing.annotationPanel.textPlaceholder")}
            onChange={(event) => {
              onTextChange(event.currentTarget.value);
            }}
          />
        </div>

        {preview ? (
          preview.error ? (
            <p className="text-xs text-[color:var(--cad-danger)]">
              {preview.error}
            </p>
          ) : null
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
            {t("drawing.annotationPanel.clearPicks")}
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

// ── Note panel (ANNOTATE → Text) ───────────────────────────────────
//
// Armed: the panel holds the draft text + height; a CLICK on the
// sheet places the note (Enter has nothing to commit while armed).
// Editing (a note was clicked): Enter commits drawing_note_update,
// Escape closes.

export interface NotePanelProps {
  disabled: boolean;
  /** true = armed (click places), false = editing an existing note. */
  armed: boolean;
  text: string;
  heightMm: number;
  onTextChange: (text: string) => void;
  onHeightChange: (heightMm: number) => void;
  onCommit: () => void;
  onClose: () => void;
}

export function NotePanel({
  disabled,
  armed,
  text,
  heightMm,
  onTextChange,
  onHeightChange,
  onCommit,
  onClose,
}: NotePanelProps) {
  const { t } = useTranslation();
  const commitRef = useRef(onCommit);
  commitRef.current = onCommit;

  // Editing: Enter commits, Escape closes.  Armed: Escape only (the
  // click on the sheet is the commit gesture).
  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Enter" && !disabled && !armed) {
        commitRef.current();
      } else if (event.key === "Escape") {
        onClose();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => {
      window.removeEventListener("keydown", onKey);
    };
  }, [disabled, armed, onClose]);

  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5">
      <div className="space-y-4">
        <div>
          <p className="cad-kicker">
            {t(
              armed
                ? "drawing.notePanel.title"
                : "drawing.notePanel.editTitle",
            )}
          </p>
          {armed ? (
            <p className="mt-3 text-sm text-[color:var(--cad-muted)]">
              {t("drawing.notePanel.placeHint")}
            </p>
          ) : null}
        </div>

        <div>
          <p className="mb-2 text-xs text-[var(--cad-muted)]">
            {t("drawing.notePanel.textLabel")}
          </p>
          <input
            className="cad-input w-full"
            type="text"
            value={text}
            disabled={disabled}
            placeholder={t("drawing.notePanel.textPlaceholder")}
            onChange={(event) => {
              onTextChange(event.currentTarget.value);
            }}
          />
        </div>

        <div>
          <p className="mb-2 text-xs text-[var(--cad-muted)]">
            {t("drawing.notePanel.heightLabel")}
          </p>
          <input
            className="cad-input"
            type="number"
            min={1}
            max={20}
            step={0.5}
            value={heightMm}
            disabled={disabled}
            onChange={(event) => {
              const value = Number(event.currentTarget.value);
              if (Number.isFinite(value) && value > 0) {
                onHeightChange(value);
              }
            }}
          />
        </div>

        <div className="flex gap-3 pt-1">
          {!armed ? (
            <button
              type="button"
              className="cad-ribbon-action cad-ribbon-action-primary flex-1"
              disabled={disabled}
              onClick={() => {
                commitRef.current();
              }}
            >
              {t("common.confirm")}
            </button>
          ) : null}
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
