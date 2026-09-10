import { useState } from "react";
import { useTranslation } from "react-i18next";

import { Dropdown, ScrollArea } from "@/lib";
import { engraveParametersSchema } from "@/lib/schemas/ipc/camSchema";
import type { EngraveParameters, ToolEntry } from "@/types";
import {
  CamNumberField,
  CamStatusLine,
  type CamToolpathStats,
  useCamEscapeCancel,
  useDebouncedCamUpdate,
} from "./camPanelShared";

// Editable mill-engrave parameters: the shared milling fields
// (feedrate, plunge, spindle) plus the engrave block (depth).  Single
// pass by design — no stepdown field (the trace is a shallow mark).
export interface EngraveFormState {
  depth_mm: number;
  feedrate_mm_per_min: number;
  plunge_feedrate_mm_per_min: number;
  spindle_rpm: number;
}

// Engrave-block defaults come from the zod schema — single source of
// truth, no parallel constants block to drift from cam_types.h.  The
// shared milling fields follow the contour trigger defaults.
export const DEFAULT_ENGRAVE_PARAMS: EngraveParameters =
  engraveParametersSchema.parse({});

export const DEFAULT_ENGRAVE_FORM: EngraveFormState = {
  ...DEFAULT_ENGRAVE_PARAMS,
  feedrate_mm_per_min: 1200,
  plunge_feedrate_mm_per_min: 600,
  spindle_rpm: 8000,
};

interface CamEngravePanelProps {
  operationName: string;
  initialParams: EngraveFormState;
  initialToolId: string;
  tools: ToolEntry[];
  // Sketch-profile geometry (the contour profile branch).
  geometryCount: number;
  selectedProfileCount: number;
  repickArmed: boolean;
  sketches: Array<{ feature_id: string; name: string }>;
  scopeSketchId: string | null;
  onSetScope: (featureId: string) => void;
  onStartRepick: () => void;
  onCancelRepick: () => void;
  onApplyRepick: () => void;
  onClearSelection: () => void;
  status: string;
  statusMessage: string;
  toolpathStats: CamToolpathStats | null;
  disabled: boolean;
  onUpdate: (partial: Partial<EngraveFormState>, toolId: string) => void;
  onPreview: () => void;
  onGenerate: () => void;
  onExport: () => void;
  onDelete: () => void;
  onClose: () => void;
}

export function CamEngravePanel({
  operationName,
  initialParams,
  initialToolId,
  tools,
  geometryCount,
  selectedProfileCount,
  repickArmed,
  sketches,
  scopeSketchId,
  onSetScope,
  onStartRepick,
  onCancelRepick,
  onApplyRepick,
  onClearSelection,
  status,
  statusMessage,
  toolpathStats,
  disabled,
  onUpdate,
  onPreview,
  onGenerate,
  onExport,
  onDelete,
  onClose,
}: CamEngravePanelProps) {
  const { t } = useTranslation();
  const [params, setParams] = useState<EngraveFormState>(() => ({
    ...DEFAULT_ENGRAVE_FORM,
    ...initialParams,
  }));
  const [toolId, setToolId] = useState(initialToolId);
  const serialized = JSON.stringify({ params, toolId });
  const markUpdateSent = useDebouncedCamUpdate(serialized, () => {
    onUpdate(params, toolId);
  });

  // Escape cancels an armed re-pick first; only then closes the panel.
  useCamEscapeCancel(() => {
    if (repickArmed) {
      onCancelRepick();
    } else {
      onClose();
    }
  });

  function update(patch: Partial<EngraveFormState>) {
    setParams((prev) => ({ ...prev, ...patch }));
  }

  const toolOptions = tools.map((tool) => ({
    value: tool.tool_id,
    label: `${tool.name} (Ø${tool.diameter_mm} mm)`,
  }));

  return (
    <section className="pointer-events-auto cad-floating-panel flex max-h-full min-h-0 w-[320px] max-w-full flex-col overflow-hidden px-5 py-5">
      <div className="flex items-center justify-between">
        <p className="cad-kicker">{t("cam.engrave.title", "Engrave")}</p>
        <button
          type="button"
          className="cad-action-ghost h-7 px-2 text-[10px] uppercase tracking-wider text-danger hover:opacity-80"
          disabled={disabled}
          onClick={onDelete}
        >
          {t("cam.engrave.delete", "Delete")}
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
              {t("cam.engrave.tool", "Tool")}
            </legend>
            <Dropdown
              className="w-full"
              value={toolId}
              label={t("cam.engrave.tool", "Tool")}
              options={toolOptions}
              disabled={disabled}
              onChange={(value) => setToolId(value)}
            />
          </fieldset>

          {/* ── Geometry ─────────────────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.engrave.geometry", "Geometry")}
            </legend>
            <p className="text-[10px] leading-relaxed text-on-surface-dim">
              {t("cam.engrave.geometrySummary", {
                count: geometryCount,
              })}
              {repickArmed
                ? t("cam.engrave.pickHint", {
                    count: selectedProfileCount,
                  })
                : null}
            </p>
            <Dropdown
              className="w-full"
              value={scopeSketchId ?? ""}
              label={t("cam.engrave.scopeSketch", "Reference sketch")}
              options={[
                {
                  value: "",
                  label: scopeSketchId
                    ? t("cam.engrave.scopeMixed", "Selected profiles")
                    : t("cam.engrave.scopeNone", "No sketch selected"),
                },
                ...sketches.map((sketch) => ({
                  value: sketch.feature_id,
                  label: sketch.name,
                })),
              ]}
              disabled={disabled || repickArmed}
              onChange={(value) => {
                if (value) {
                  onSetScope(value);
                }
              }}
            />
            {repickArmed ? (
              <div className="grid grid-cols-2 gap-2">
                <button
                  type="button"
                  className="cad-action-primary px-2 py-1 text-[10px] uppercase tracking-wider"
                  disabled={disabled}
                  onClick={onApplyRepick}
                >
                  {t("cam.engrave.applyRepick", "Apply selection")}
                </button>
                <button
                  type="button"
                  className="cad-action-ghost px-2 py-1 text-[10px] uppercase tracking-wider"
                  disabled={disabled}
                  onClick={onCancelRepick}
                >
                  {t("cam.engrave.cancelRepick", "Cancel")}
                </button>
                {selectedProfileCount > 0 ? (
                  <button
                    type="button"
                    className="cad-action-ghost col-span-2 px-2 py-1 text-[10px] uppercase tracking-wider"
                    disabled={disabled}
                    onClick={onClearSelection}
                  >
                    {t("cam.engrave.clearSelection", "Clear selection")}
                  </button>
                ) : null}
              </div>
            ) : (
              <button
                type="button"
                className="cad-action-ghost px-2 py-1 text-[10px] uppercase tracking-wider"
                disabled={disabled}
                onClick={onStartRepick}
              >
                {t("cam.engrave.reselectGeometry", "Re-select geometry")}
              </button>
            )}
          </fieldset>

          {/* ── Cutting Parameters ───────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.engrave.cuttingParams", "Cutting Parameters")}
            </legend>
            <div className="grid grid-cols-2 gap-2">
              <CamNumberField
                label={t("cam.engrave.depth", "Depth (mm)")}
                value={params.depth_mm}
                disabled={disabled}
                step={0.1}
                onChange={(v) =>
                  // Clamp above zero — the zod schema requires a
                  // positive depth and a parse failure would take down
                  // the whole CAM document state.
                  update({
                    depth_mm: v === undefined ? params.depth_mm : Math.max(0.01, v),
                  })
                }
              />
              <CamNumberField
                label={t("cam.engrave.feedrate", "Feedrate (mm/min)")}
                value={params.feedrate_mm_per_min}
                disabled={disabled}
                step={50}
                onChange={(v) =>
                  update({ feedrate_mm_per_min: v ?? params.feedrate_mm_per_min })
                }
              />
              <CamNumberField
                label={t("cam.engrave.plungeFeedrate", "Plunge (mm/min)")}
                value={params.plunge_feedrate_mm_per_min}
                disabled={disabled}
                step={50}
                onChange={(v) =>
                  update({
                    plunge_feedrate_mm_per_min:
                      v ?? params.plunge_feedrate_mm_per_min,
                  })
                }
              />
              <CamNumberField
                label={t("cam.engrave.spindleRpm", "Spindle (RPM)")}
                value={params.spindle_rpm}
                disabled={disabled}
                step={100}
                onChange={(v) => update({ spindle_rpm: v ?? params.spindle_rpm })}
              />
            </div>
            <p className="text-[10px] leading-relaxed text-on-surface-dim">
              {t(
                "cam.engrave.depthHelp",
                "Cut depth below the sketch plane — a shallow single pass.",
              )}
            </p>
          </fieldset>

          <p className="text-[10px] leading-relaxed text-on-surface-dim">
            {t(
              "cam.engrave.help",
              "Changes update the toolpath preview in real time.",
            )}
          </p>

          {/* ── Status ───────────────────────────────────────────── */}
          <CamStatusLine
            status={status}
            statusMessage={statusMessage}
            toolpathStats={toolpathStats}
            prefix="cam.engrave"
          />
        </ScrollArea>

        <div className="mt-3 grid grid-cols-2 gap-2 pt-2">
          <button
            type="button"
            className="cad-action-ghost"
            disabled={disabled}
            onClick={onClose}
          >
            {t("cam.engrave.close", "Close")}
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
            {t("cam.engrave.preview", "Preview")}
          </button>
          <button
            type="submit"
            className="cad-action-primary col-span-2"
            disabled={disabled}
          >
            {t("cam.engrave.generate", "Generate")}
          </button>
          <button
            type="button"
            className="cad-action-primary col-span-2"
            disabled={disabled}
            onClick={onExport}
          >
            {t("cam.engrave.exportGcode", "Export G-code")}
          </button>
        </div>
      </form>
    </section>
  );
}
