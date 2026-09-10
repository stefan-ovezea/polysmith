import { useState } from "react";
import { useTranslation } from "react-i18next";

import { Dropdown, ScrollArea } from "@/lib";
import type { ToolEntry } from "@/types";
import {
  CamNumberField,
  CamStatusLine,
  type CamToolpathStats,
  useCamEscapeCancel,
  useDebouncedCamUpdate,
} from "./camPanelShared";

// Editable open-slot parameters.  Each selected edge is a slot's open
// side; the tool cuts a tool-width groove from the edge into the
// material, depth_mm below the adjacent top face.  stepdown_mm absent
// means a single pass at that face level.
export interface SlotFormState {
  depth_mm: number;
  feedrate_mm_per_min: number;
  plunge_feedrate_mm_per_min: number;
  spindle_rpm: number;
  stepdown_mm?: number;
}

export const DEFAULT_SLOT_PARAMS: SlotFormState = {
  depth_mm: 5,
  feedrate_mm_per_min: 1200,
  plunge_feedrate_mm_per_min: 600,
  spindle_rpm: 8000,
};

interface CamSlotPanelProps {
  operationName: string;
  initialParams: SlotFormState;
  initialToolId: string;
  tools: ToolEntry[];
  edgeCount: number;
  status: string;
  statusMessage: string;
  toolpathStats: CamToolpathStats | null;
  disabled: boolean;
  onUpdate: (partial: Partial<SlotFormState>, toolId: string) => void;
  onRepickEdges: () => void;
  onRemoveEdge: (index: number) => void;
  onPreview: () => void;
  onGenerate: () => void;
  onExport: () => void;
  onDelete: () => void;
  onClose: () => void;
}

export function CamSlotPanel({
  operationName,
  initialParams,
  initialToolId,
  tools,
  edgeCount,
  status,
  statusMessage,
  toolpathStats,
  disabled,
  onUpdate,
  onRepickEdges,
  onRemoveEdge,
  onPreview,
  onGenerate,
  onExport,
  onDelete,
  onClose,
}: CamSlotPanelProps) {
  const { t } = useTranslation();
  const [params, setParams] = useState<SlotFormState>(() => ({
    ...DEFAULT_SLOT_PARAMS,
    ...initialParams,
  }));
  const [toolId, setToolId] = useState(initialToolId);
  const serialized = JSON.stringify({ params, toolId });
  const markUpdateSent = useDebouncedCamUpdate(serialized, () => {
    onUpdate(params, toolId);
  });

  // Re-pick has no armed state — Escape just closes the panel.
  useCamEscapeCancel(onClose);

  function update(patch: Partial<SlotFormState>) {
    setParams((prev) => ({ ...prev, ...patch }));
  }

  const toolOptions = tools.map((tool) => ({
    value: tool.tool_id,
    label: `${tool.name} (Ø${tool.diameter_mm} mm)`,
  }));

  return (
    <section className="pointer-events-auto cad-floating-panel flex max-h-full min-h-0 w-[320px] max-w-full flex-col overflow-hidden px-5 py-5">
      <div className="flex items-center justify-between">
        <p className="cad-kicker">{t("cam.slot.title", "Slot")}</p>
        <button
          type="button"
          className="cad-action-ghost h-7 px-2 text-[10px] uppercase tracking-wider text-danger hover:opacity-80"
          disabled={disabled}
          onClick={onDelete}
        >
          {t("cam.slot.delete", "Delete")}
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
              {t("cam.slot.tool", "Tool")}
            </legend>
            <Dropdown
              className="w-full"
              value={toolId}
              label={t("cam.slot.tool", "Tool")}
              options={toolOptions}
              disabled={disabled}
              onChange={(value) => setToolId(value)}
            />
          </fieldset>

          {/* ── Geometry ─────────────────────────────────────────── */}
          <fieldset className="space-y-2">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.slot.geometry", "Geometry")}
            </legend>
            {/* Slots are indexed rows only — witness ids stay out of the
                UI (the core owns geometry identity). */}
            {edgeCount > 0 ? (
              <div className="space-y-1">
                {Array.from({ length: edgeCount }, (_, index) => (
                  <div
                    key={index}
                    className="flex items-center justify-between gap-2 rounded border border-[var(--cad-panel-border)] px-2 py-1.5"
                  >
                    <span className="min-w-0 truncate text-xs">
                      {t("cam.slot.edge", { index: index + 1 })}
                    </span>
                    <button
                      type="button"
                      className="h-6 w-6 shrink-0 rounded text-on-surface-muted hover:text-danger"
                      aria-label={t("cam.slot.edgeRemove", {
                        index: index + 1,
                      })}
                      disabled={disabled}
                      onClick={() => onRemoveEdge(index)}
                    >
                      ×
                    </button>
                  </div>
                ))}
              </div>
            ) : (
              <p className="text-[10px] leading-relaxed text-on-surface-dim">
                {t(
                  "cam.slot.repickHint",
                  "Select one or more straight edges in the viewport, then Re-pick.",
                )}
              </p>
            )}
            <button
              type="button"
              className="cad-action-ghost h-7 w-full text-[10px] uppercase tracking-wider"
              disabled={disabled}
              onClick={onRepickEdges}
            >
              {t("cam.slot.repickEdges", "Re-pick edges")}
            </button>
          </fieldset>

          {/* ── Cutting Parameters ───────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.slot.cuttingParams", "Cutting Parameters")}
            </legend>
            <div className="grid grid-cols-2 gap-2">
              <CamNumberField
                label={t("cam.slot.depth", "Depth (mm)")}
                value={params.depth_mm}
                disabled={disabled}
                step={0.5}
                onChange={(v) => update({ depth_mm: v })}
              />
              <CamNumberField
                label={t("cam.slot.feedrate", "Feedrate (mm/min)")}
                value={params.feedrate_mm_per_min}
                disabled={disabled}
                step={50}
                onChange={(v) => update({ feedrate_mm_per_min: v })}
              />
              <CamNumberField
                label={t("cam.slot.plungeFeedrate", "Plunge (mm/min)")}
                value={params.plunge_feedrate_mm_per_min}
                disabled={disabled}
                step={50}
                onChange={(v) => update({ plunge_feedrate_mm_per_min: v })}
              />
              <CamNumberField
                label={t("cam.slot.stepdown", "Stepdown (mm)")}
                value={params.stepdown_mm}
                disabled={disabled}
                step={0.5}
                clearable
                onChange={(v) => update({ stepdown_mm: v })}
              />
            </div>
            <p className="text-[10px] leading-relaxed text-on-surface-dim">
              {t(
                "cam.slot.stepdownHelp",
                "Axial depth per pass, cut from the stock top down to the slot floor. Leave empty for a single pass.",
              )}
            </p>
            <CamNumberField
              label={t("cam.slot.spindleRpm", "Spindle (RPM)")}
              value={params.spindle_rpm}
              disabled={disabled}
              step={100}
              onChange={(v) => update({ spindle_rpm: v })}
            />
          </fieldset>

          <p className="text-[10px] leading-relaxed text-on-surface-dim">
            {t(
              "cam.slot.help",
              "Changes update the toolpath preview in real time.",
            )}
          </p>

          {/* ── Status ───────────────────────────────────────────── */}
          <CamStatusLine
            status={status}
            statusMessage={statusMessage}
            toolpathStats={toolpathStats}
            prefix="cam.slot"
          />
        </ScrollArea>

        <div className="mt-3 grid grid-cols-2 gap-2 pt-2">
          <button
            type="button"
            className="cad-action-ghost"
            disabled={disabled}
            onClick={onClose}
          >
            {t("cam.slot.close", "Close")}
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
            {t("cam.slot.preview", "Preview")}
          </button>
          <button
            type="submit"
            className="cad-action-primary col-span-2"
            disabled={disabled}
          >
            {t("cam.slot.generate", "Generate")}
          </button>
          <button
            type="button"
            className="cad-action-primary col-span-2"
            disabled={disabled}
            onClick={onExport}
          >
            {t("cam.slot.exportGcode", "Export G-code")}
          </button>
        </div>
      </form>
    </section>
  );
}
