import { useState } from "react";
import { useTranslation } from "react-i18next";

import { Dropdown, ScrollArea } from "@/lib";
import type { CamOperationParameters, ToolEntry } from "@/types";
import {
  CamNumberField,
  CamStatusLine,
  type CamToolpathStats,
  useCamEscapeCancel,
  useDebouncedCamUpdate,
} from "./camPanelShared";

// Editable Adaptive Clearing parameters.  The v1 toolpath is the
// contour-parallel spiral: the stepover % is the radial loop spacing.
// stepdown_mm absent means a single pass at the selected face.
export interface AdaptiveFormState {
  feedrate_mm_per_min: number;
  plunge_feedrate_mm_per_min: number;
  stepover_percent: number;
  spindle_rpm: number;
  stepdown_mm?: number;
}

export const DEFAULT_ADAPTIVE_PARAMS: AdaptiveFormState = {
  feedrate_mm_per_min: 1200,
  plunge_feedrate_mm_per_min: 600,
  stepover_percent: 50,
  spindle_rpm: 8000,
};

interface CamAdaptivePanelProps {
  operationName: string;
  initialParams: AdaptiveFormState;
  initialToolId: string;
  tools: ToolEntry[];
  islandCount: number;
  // Which face pick is armed right now — the panel shows the matching
  // hint and Escape cancels the pick instead of closing.
  pickTarget: "outer" | "island" | null;
  status: string;
  statusMessage: string;
  toolpathStats: CamToolpathStats | null;
  disabled: boolean;
  onUpdate: (partial: Partial<CamOperationParameters>, toolId: string) => void;
  onRepickFace: () => void;
  onAddIsland: () => void;
  onRemoveIsland: (index: number) => void;
  onCancelPick: () => void;
  onPreview: () => void;
  onGenerate: () => void;
  onExport: () => void;
  onDelete: () => void;
  onClose: () => void;
}

export function CamAdaptivePanel({
  operationName,
  initialParams,
  initialToolId,
  tools,
  islandCount,
  pickTarget,
  status,
  statusMessage,
  toolpathStats,
  disabled,
  onUpdate,
  onRepickFace,
  onAddIsland,
  onRemoveIsland,
  onCancelPick,
  onPreview,
  onGenerate,
  onExport,
  onDelete,
  onClose,
}: CamAdaptivePanelProps) {
  const { t } = useTranslation();
  const [params, setParams] = useState<AdaptiveFormState>(() => ({
    ...DEFAULT_ADAPTIVE_PARAMS,
    ...initialParams,
  }));
  const [toolId, setToolId] = useState(initialToolId);
  const serialized = JSON.stringify({ params, toolId });
  const markUpdateSent = useDebouncedCamUpdate(serialized, () => {
    onUpdate(params, toolId);
  });

  // Escape cancels an armed pick first; only then closes the panel.
  useCamEscapeCancel(() => {
    if (pickTarget) {
      onCancelPick();
    } else {
      onClose();
    }
  });

  function update(patch: Partial<AdaptiveFormState>) {
    setParams((prev) => ({ ...prev, ...patch }));
  }

  const toolOptions = tools.map((tool) => ({
    value: tool.tool_id,
    label: `${tool.name} (Ø${tool.diameter_mm} mm)`,
  }));

  return (
    <section className="pointer-events-auto cad-floating-panel flex max-h-full min-h-0 w-[320px] max-w-full flex-col overflow-hidden px-5 py-5">
      <div className="flex items-center justify-between">
        <p className="cad-kicker">
          {t("cam.adaptive.title", "Adaptive Clearing")}
        </p>
        <button
          type="button"
          className="cad-action-ghost h-7 px-2 text-[10px] uppercase tracking-wider text-danger hover:opacity-80"
          disabled={disabled}
          onClick={onDelete}
        >
          {t("cam.adaptive.delete", "Delete")}
        </button>
      </div>
      <p className="mt-1 text-xs text-on-surface-muted">{operationName}</p>

      <form
        className="mt-4 flex min-h-0 flex-1 flex-col overflow-hidden"
        onSubmit={(event) => {
          event.preventDefault();
          markUpdateSent();
          onUpdate(params, toolId);
          onGenerate();
        }}
      >
        <ScrollArea className="min-h-0 flex-1" viewportClassName="space-y-4 pr-4">
          {/* ── Tool ─────────────────────────────────────────────── */}
          <fieldset className="space-y-2">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.adaptive.tool", "Tool")}
            </legend>
            <Dropdown
              className="w-full"
              value={toolId}
              label={t("cam.adaptive.tool", "Tool")}
              options={toolOptions}
              disabled={disabled}
              onChange={(value) => setToolId(value)}
            />
          </fieldset>

          {/* ── Geometry ─────────────────────────────────────────── */}
          <fieldset className="space-y-2">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.adaptive.geometry", "Geometry")}
            </legend>
            <div className="flex items-center justify-between gap-2 rounded border border-[var(--cad-panel-border)] px-2 py-1.5">
              <span className="min-w-0 truncate text-xs">
                {t("cam.adaptive.clearingFace", "Clearing face")}
              </span>
              <button
                type="button"
                className="cad-action-ghost h-6 shrink-0 px-2 text-[10px] uppercase tracking-wider"
                disabled={disabled}
                onClick={onRepickFace}
              >
                {t("cam.adaptive.repickFace", "Re-pick")}
              </button>
            </div>
            {/* Islands are indexed rows only — witness ids stay out of
                the UI (the core owns geometry identity). */}
            {islandCount > 0 ? (
              <div className="space-y-1">
                {Array.from({ length: islandCount }, (_, index) => (
                  <div
                    key={index}
                    className="flex items-center justify-between gap-2 rounded border border-[var(--cad-panel-border)] px-2 py-1.5"
                  >
                    <span className="min-w-0 truncate text-xs">
                      {t("cam.adaptive.island", { index: index + 1 })}
                    </span>
                    <button
                      type="button"
                      className="h-6 w-6 shrink-0 rounded text-on-surface-muted hover:text-danger"
                      aria-label={t("cam.adaptive.islandRemove", {
                        index: index + 1,
                      })}
                      disabled={disabled}
                      onClick={() => onRemoveIsland(index)}
                    >
                      ×
                    </button>
                  </div>
                ))}
              </div>
            ) : null}
            {/* Single-pass toolpaths never add the island-top levels, so
                islands only change the toolpath with a stepdown set. */}
            {islandCount > 0 && !params.stepdown_mm ? (
              <p className="text-[10px] leading-relaxed text-on-surface-dim">
                {t(
                  "cam.adaptive.islandSinglePassHint",
                  "Islands only change the toolpath with multi-pass milling — set a Stepdown to mill the stock above them flush.",
                )}
              </p>
            ) : null}
            <button
              type="button"
              className="cad-action-ghost h-7 w-full text-[10px] uppercase tracking-wider"
              disabled={disabled}
              onClick={onAddIsland}
            >
              {t("cam.adaptive.addIsland", "Add island")}
            </button>
            {pickTarget ? (
              <div className="space-y-2 rounded border border-[var(--cad-panel-border)] px-2 py-2">
                <p className="text-[10px] leading-relaxed text-on-surface-dim">
                  {pickTarget === "island"
                    ? t(
                        "cam.adaptive.addIslandHint",
                        "Click a boss top face in the viewport to add it as an island — the toolpath will avoid it.",
                      )
                    : t(
                        "cam.adaptive.repickFaceHint",
                        "Click a face in the viewport to set the area to clear.",
                      )}
                </p>
                <button
                  type="button"
                  className="cad-action-ghost h-6 w-full text-[10px] uppercase tracking-wider"
                  disabled={disabled}
                  onClick={onCancelPick}
                >
                  {t("cam.adaptive.cancelPick", "Cancel pick")}
                </button>
              </div>
            ) : null}
          </fieldset>

          {/* ── Cutting Parameters ───────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.adaptive.cuttingParams", "Cutting Parameters")}
            </legend>
            <div className="grid grid-cols-2 gap-2">
              <CamNumberField
                label={t("cam.adaptive.feedrate", "Feedrate (mm/min)")}
                value={params.feedrate_mm_per_min}
                disabled={disabled}
                step={50}
                onChange={(v) => update({ feedrate_mm_per_min: v })}
              />
              <CamNumberField
                label={t("cam.adaptive.plungeFeedrate", "Plunge (mm/min)")}
                value={params.plunge_feedrate_mm_per_min}
                disabled={disabled}
                step={50}
                onChange={(v) => update({ plunge_feedrate_mm_per_min: v })}
              />
              <CamNumberField
                label={t("cam.adaptive.stepoverPercent", "Stepover (%)")}
                value={params.stepover_percent}
                disabled={disabled}
                step={5}
                min={undefined}
                onChange={(v) => update({ stepover_percent: v })}
              />
              <CamNumberField
                label={t("cam.adaptive.stepdown", "Stepdown (mm)")}
                value={params.stepdown_mm}
                disabled={disabled}
                step={0.5}
                clearable
                onChange={(v) => update({ stepdown_mm: v })}
              />
            </div>
            <p className="text-[10px] leading-relaxed text-on-surface-dim">
              {t(
                "cam.adaptive.stepdownHelp",
                "Axial depth per pass, cut from the stock top down to the selected face. Leave empty for a single pass at the face.",
              )}
            </p>
            <CamNumberField
              label={t("cam.adaptive.spindleRpm", "Spindle (RPM)")}
              value={params.spindle_rpm}
              disabled={disabled}
              step={100}
              onChange={(v) => update({ spindle_rpm: v })}
            />
          </fieldset>

          <p className="text-[10px] leading-relaxed text-on-surface-dim">
            {t(
              "cam.adaptive.help",
              "Changes update the toolpath preview in real time.",
            )}
          </p>

          {/* ── Status ───────────────────────────────────────────── */}
          <CamStatusLine
            status={status}
            statusMessage={statusMessage}
            toolpathStats={toolpathStats}
            prefix="cam.adaptive"
          />
        </ScrollArea>

        <div className="mt-3 grid grid-cols-2 gap-2 pt-2">
          <button
            type="button"
            className="cad-action-ghost"
            disabled={disabled}
            onClick={onClose}
          >
            {t("cam.adaptive.close", "Close")}
          </button>
          <button
            type="button"
            className="cad-action-ghost"
            disabled={disabled}
            onClick={() => {
              markUpdateSent();
              onUpdate(params, toolId);
              onPreview();
            }}
          >
            {t("cam.adaptive.preview", "Preview")}
          </button>
          <button
            type="submit"
            className="cad-action-primary col-span-2"
            disabled={disabled}
          >
            {t("cam.adaptive.generate", "Generate")}
          </button>
          <button
            type="button"
            className="cad-action-primary col-span-2"
            disabled={disabled}
            onClick={onExport}
          >
            {t("cam.adaptive.exportGcode", "Export G-code")}
          </button>
        </div>
      </form>
    </section>
  );
}
