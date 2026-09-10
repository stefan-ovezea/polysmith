import { useState } from "react";
import { useTranslation } from "react-i18next";

import { Dropdown, ScrollArea } from "@/lib";
import type {
  CamOperationParameters,
  DrillingCycleType,
  ToolEntry,
} from "@/types";
import {
  CamCheckboxField,
  CamNumberField,
  CamStatusLine,
  type CamToolpathStats,
  useCamEscapeCancel,
  useDebouncedCamUpdate,
} from "./camPanelShared";

// Editable drilling parameters.  Defaults mirror the C++ struct
// defaults in cam_types.h (CamOperationParameters).
export interface DrillingFormState {
  cycle_type: DrillingCycleType;
  hole_depth_mm: number;
  peck_depth_mm: number;
  through_hole: boolean;
  feedrate_mm_per_min: number;
  plunge_feedrate_mm_per_min: number;
  spindle_rpm: number;
}

export const DEFAULT_DRILLING_PARAMS: DrillingFormState = {
  cycle_type: "g81_standard",
  hole_depth_mm: 5,
  peck_depth_mm: 2,
  through_hole: false,
  feedrate_mm_per_min: 200,
  plunge_feedrate_mm_per_min: 200,
  spindle_rpm: 8000,
};

// One hole location shown in the points list.  Holes are BODY
// geometry — a captured hole-wall face, a captured rim edge — or a
// free-picked world point.  "legacy" marks a saved operation that
// still references sketch circles (no longer a drilling input).
export interface DrillPointRow {
  label: string;
  kind: "wall" | "rim" | "point" | "legacy";
}

interface CamDrillingPanelProps {
  operationName: string;
  initialParams: DrillingFormState;
  initialToolId: string;
  tools: ToolEntry[];
  status: string;
  statusMessage: string;
  toolpathStats: CamToolpathStats | null;
  disabled: boolean;
  // Hole locations — the label is user-facing copy ("Hole wall 1",
  // "Hole rim 2", "X 10 Y 5 Z 0"); never an internal id.
  points: DrillPointRow[];
  // Armed "Add point" pick: the next viewport click adds a hole.
  pickArmed: boolean;
  onPickPoint: () => void;
  onCancelPick: () => void;
  onRemovePoint: (index: number) => void;
  onUpdate: (partial: Partial<CamOperationParameters>, toolId: string) => void;
  onPreview: () => void;
  onGenerate: () => void;
  onExport: () => void;
  onDelete: () => void;
  onClose: () => void;
}

export function CamDrillingPanel({
  operationName,
  initialParams,
  initialToolId,
  tools,
  status,
  statusMessage,
  toolpathStats,
  disabled,
  points,
  pickArmed,
  onPickPoint,
  onCancelPick,
  onRemovePoint,
  onUpdate,
  onPreview,
  onGenerate,
  onExport,
  onDelete,
  onClose,
}: CamDrillingPanelProps) {
  const { t } = useTranslation();
  const [params, setParams] = useState<DrillingFormState>(() => ({
    ...DEFAULT_DRILLING_PARAMS,
    ...initialParams,
  }));
  const [toolId, setToolId] = useState(initialToolId);
  const serialized = JSON.stringify({ params, toolId });
  const markUpdateSent = useDebouncedCamUpdate(serialized, () => {
    onUpdate(toParametersPatch(params), toolId);
  });

  useCamEscapeCancel(onClose);

  function update(patch: Partial<DrillingFormState>) {
    setParams((prev) => ({ ...prev, ...patch }));
  }

  const toolOptions = tools.map((tool) => ({
    value: tool.tool_id,
    label: `${tool.name} (Ø${tool.diameter_mm} mm)`,
  }));

  const cycleOptions: Array<{ value: DrillingCycleType; label: string }> = [
    { value: "g81_standard", label: t("cam.drilling.cycleG81", "G81 — simple drill") },
    { value: "g83_peck", label: t("cam.drilling.cycleG83", "G83 — peck drilling") },
  ];

  // Through holes ignore the depth — the generator drills to the stock
  // bottom, so the depth field (and G83 pecks) no longer apply.
  const depthDisabled = disabled || params.through_hole;

  return (
    <section className="pointer-events-auto cad-floating-panel flex max-h-full min-h-0 w-[320px] max-w-full flex-col overflow-hidden px-5 py-5">
      <div className="flex items-center justify-between">
        <p className="cad-kicker">{t("cam.drilling.title", "Drilling")}</p>
        <button
          type="button"
          className="cad-action-ghost h-7 px-2 text-[10px] uppercase tracking-wider text-danger hover:opacity-80"
          disabled={disabled}
          onClick={onDelete}
        >
          {t("cam.drilling.delete", "Delete")}
        </button>
      </div>
      <p className="mt-1 text-xs text-on-surface-muted">{operationName}</p>

      <form
        className="mt-4 flex min-h-0 flex-1 flex-col overflow-hidden"
        onSubmit={(event) => {
          event.preventDefault();
          markUpdateSent();
          onUpdate(toParametersPatch(params), toolId);
          onGenerate();
        }}
      >
        <ScrollArea className="min-h-0 flex-1" viewportClassName="space-y-4 pr-4">
          {/* ── Tool ─────────────────────────────────────────────── */}
          <fieldset className="space-y-2">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.drilling.tool", "Tool")}
            </legend>
            <Dropdown
              className="w-full"
              value={toolId}
              label={t("cam.drilling.tool", "Tool")}
              options={toolOptions}
              disabled={disabled}
              onChange={(value) => setToolId(value)}
            />
          </fieldset>

          {/* ── Cycle ───────────────────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.drilling.cycle", "Drill Cycle")}
            </legend>
            <Dropdown
              className="w-full"
              value={params.cycle_type}
              label={t("cam.drilling.cycle", "Drill Cycle")}
              options={cycleOptions}
              disabled={disabled}
              onChange={(value) => update({ cycle_type: value })}
            />
            <div className="grid grid-cols-2 gap-2">
              <CamNumberField
                label={t("cam.drilling.depth", "Depth (mm)")}
                value={params.hole_depth_mm}
                disabled={depthDisabled}
                step={0.5}
                onChange={(v) => update({ hole_depth_mm: v ?? 5 })}
              />
              <CamNumberField
                label={t("cam.drilling.peck", "Peck (mm)")}
                value={params.peck_depth_mm}
                disabled={disabled || params.cycle_type !== "g83_peck"}
                step={0.5}
                onChange={(v) => update({ peck_depth_mm: v ?? 2 })}
              />
            </div>
            <CamCheckboxField
              label={t("cam.drilling.through", "Through hole (to stock bottom)")}
              checked={params.through_hole}
              disabled={disabled}
              onChange={(checked) => update({ through_hole: checked })}
            />
            {params.through_hole ? (
              <p className="text-[10px] leading-relaxed text-on-surface-dim">
                {t(
                  "cam.drilling.throughNote",
                  "Through holes drill to the stock bottom — the depth and peck settings are ignored.",
                )}
              </p>
            ) : null}
          </fieldset>

          {/* ── Hole locations ──────────────────────────────────── */}
          <fieldset className="space-y-2">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.drilling.points", "Hole Locations")}
            </legend>
            {points.length > 0 ? (
              <ul className="space-y-1">
                {points.map((point, index) => (
                  <li
                    key={`${point.kind}-${index}`}
                    className="flex items-center justify-between rounded-md cad-subtle-block px-2 py-1"
                  >
                    <span className="truncate text-xs text-on-surface">
                      {point.label}
                    </span>
                    <button
                      type="button"
                      className="cad-action-ghost h-6 px-1.5 text-[10px] uppercase tracking-wider text-danger hover:opacity-80"
                      disabled={disabled}
                      onClick={() => onRemovePoint(index)}
                    >
                      {t("cam.drilling.removePoint", "Remove")}
                    </button>
                  </li>
                ))}
              </ul>
            ) : (
              <p className="text-[10px] leading-relaxed text-on-surface-dim">
                {t("cam.drilling.noPoints", "No hole locations yet.")}
              </p>
            )}
            <button
              type="button"
              className={
                pickArmed ? "cad-action-primary w-full" : "cad-action-ghost w-full"
              }
              disabled={disabled}
              onClick={pickArmed ? onCancelPick : onPickPoint}
            >
              {pickArmed
                ? t("cam.drilling.cancelPick", "Cancel pick")
                : t("cam.drilling.addHole", "Add hole")}
            </button>
            {pickArmed ? (
              <p className="text-[10px] leading-relaxed text-on-surface-dim">
                {t(
                  "cam.drilling.pickHint",
                  "Click a hole in the viewport — the dots mark hole rims and walls; a click on a rim or wall stores the hole, a free click elsewhere stores that point. Sketch circles are not drilling targets.",
                )}
              </p>
            ) : null}
          </fieldset>

          {/* ── Cutting Parameters ───────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.drilling.cuttingParams", "Cutting Parameters")}
            </legend>
            <div className="grid grid-cols-2 gap-2">
              <CamNumberField
                label={t("cam.drilling.feedrate", "Feedrate (mm/min)")}
                value={params.feedrate_mm_per_min}
                disabled={disabled}
                step={25}
                onChange={(v) => update({ feedrate_mm_per_min: v ?? 200 })}
              />
              <CamNumberField
                label={t("cam.drilling.plungeFeedrate", "Plunge (mm/min)")}
                value={params.plunge_feedrate_mm_per_min}
                disabled={disabled}
                step={25}
                onChange={(v) => update({ plunge_feedrate_mm_per_min: v ?? 200 })}
              />
            </div>
            <CamNumberField
              label={t("cam.drilling.spindleRpm", "Spindle (RPM)")}
              value={params.spindle_rpm}
              disabled={disabled}
              step={100}
              onChange={(v) => update({ spindle_rpm: v ?? 8000 })}
            />
          </fieldset>

          <p className="text-[10px] leading-relaxed text-on-surface-dim">
            {t("cam.drilling.help", "Changes update the toolpath preview in real time.")}
          </p>

          {/* ── Status ───────────────────────────────────────────── */}
          <CamStatusLine
            status={status}
            statusMessage={statusMessage}
            toolpathStats={toolpathStats}
            prefix="cam.drilling"
          />
        </ScrollArea>

        <div className="mt-3 grid grid-cols-2 gap-2 pt-2">
          <button
            type="button"
            className="cad-action-ghost"
            disabled={disabled}
            onClick={onClose}
          >
            {t("cam.drilling.close", "Close")}
          </button>
          <button
            type="button"
            className="cad-action-ghost"
            disabled={disabled}
            onClick={() => {
              markUpdateSent();
              onUpdate(toParametersPatch(params), toolId);
              onPreview();
            }}
          >
            {t("cam.drilling.preview", "Preview")}
          </button>
          <button
            type="submit"
            className="cad-action-primary col-span-2"
            disabled={disabled}
          >
            {t("cam.drilling.generate", "Generate")}
          </button>
          <button
            type="button"
            className="cad-action-primary col-span-2"
            disabled={disabled}
            onClick={onExport}
          >
            {t("cam.drilling.exportGcode", "Export G-code")}
          </button>
        </div>
      </form>
    </section>
  );
}

// The form state mapped onto the core's CamOperationParameters shape —
// the parent merges it over the operation's existing parameters.
function toParametersPatch(
  params: DrillingFormState,
): Partial<CamOperationParameters> {
  return {
    cycle_type: params.cycle_type,
    hole_depth_mm: params.hole_depth_mm,
    peck_depth_mm: params.peck_depth_mm,
    through_hole: params.through_hole,
    feedrate_mm_per_min: params.feedrate_mm_per_min,
    plunge_feedrate_mm_per_min: params.plunge_feedrate_mm_per_min,
    spindle_rpm: params.spindle_rpm,
  };
}
