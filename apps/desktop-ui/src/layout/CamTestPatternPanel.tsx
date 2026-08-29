import { useState } from "react";
import { useTranslation } from "react-i18next";

import { Dropdown, ScrollArea } from "@/lib";
import { laserTestPatternParametersSchema } from "@/lib/schemas/ipc/camSchema";
import type { LaserTestPatternParameters } from "@/types";
import {
  CamCheckboxField,
  CamNumberField,
  CamStatusLine,
  type CamToolpathStats,
  useCamEscapeCancel,
  useDebouncedCamUpdate,
} from "./camPanelShared";

// Defaults come from the zod schema — single source of truth.
export const DEFAULT_TEST_PATTERN_PARAMS: LaserTestPatternParameters =
  laserTestPatternParametersSchema.parse({});

interface CamTestPatternPanelProps {
  operationName: string;
  initialParams: LaserTestPatternParameters;
  status: string;
  statusMessage: string;
  toolpathStats: CamToolpathStats | null;
  disabled: boolean;
  onUpdate: (partial: Partial<LaserTestPatternParameters>) => void;
  onPreview: () => void;
  onGenerate: () => void;
  onExport: () => void;
  onDelete: () => void;
  onClose: () => void;
}

export function CamTestPatternPanel({
  operationName,
  initialParams,
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
}: CamTestPatternPanelProps) {
  const { t } = useTranslation();
  const [params, setParams] = useState<LaserTestPatternParameters>(() => ({
    ...DEFAULT_TEST_PATTERN_PARAMS,
    ...initialParams,
  }));
  const serialized = JSON.stringify(params);
  const markUpdateSent = useDebouncedCamUpdate(serialized, () => {
    onUpdate(params);
  });

  useCamEscapeCancel(onClose);

  function update(patch: Partial<LaserTestPatternParameters>) {
    setParams((prev) => ({ ...prev, ...patch }));
  }

  return (
    <section className="pointer-events-auto cad-floating-panel flex max-h-full min-h-0 w-[320px] max-w-full flex-col overflow-hidden px-5 py-5">
      <div className="flex items-center justify-between">
        <p className="cad-kicker">{t("cam.testPattern.title", "Test Pattern")}</p>
        <button
          type="button"
          className="cad-action-ghost h-7 px-2 text-[10px] uppercase tracking-wider text-danger hover:opacity-80"
          disabled={disabled}
          onClick={onDelete}
        >
          {t("cam.testPattern.delete", "Delete")}
        </button>
      </div>
      <p className="mt-1 text-xs text-on-surface-muted">{operationName}</p>

      <form
        className="mt-4 flex min-h-0 flex-1 flex-col overflow-hidden"
        onSubmit={(event) => {
          event.preventDefault();
          markUpdateSent();
          onUpdate(params);
          onGenerate();
        }}
      >
        <ScrollArea className="min-h-0 flex-1" viewportClassName="space-y-4 pr-4">
          {/* ── Pattern ──────────────────────────────────────────── */}
          <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
            {t("cam.testPattern.pattern", "Pattern")}
            <Dropdown
              className="mt-2 w-full"
              value={params.pattern}
              label={t("cam.testPattern.pattern", "Pattern")}
              options={[
                { value: "engrave_grid", label: t("cam.testPattern.engraveGrid", "Engrave grid (filled)") },
                { value: "cut_grid", label: t("cam.testPattern.cutGrid", "Cut grid (through-cut)") },
                { value: "kerf_gauge", label: t("cam.testPattern.kerfGauge", "Kerf gauge (calibration square)") },
              ]}
              disabled={disabled}
              onChange={(value) =>
                update({ pattern: value as LaserTestPatternParameters["pattern"] })
              }
            />
          </label>

          {/* ── Power sweep (columns) ───────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.testPattern.powerSweep", "Power (columns)")}
            </legend>
            <div className="grid grid-cols-3 gap-2">
              <CamNumberField
                label={t("cam.testPattern.powerMin", "Min (%)")}
                value={params.power_min_percent}
                disabled={disabled}
                step={5}
                min={undefined}
                onChange={(v) => update({ power_min_percent: v })}
              />
              <CamNumberField
                label={t("cam.testPattern.powerMax", "Max (%)")}
                value={params.power_max_percent}
                disabled={disabled}
                step={5}
                min={undefined}
                onChange={(v) => update({ power_max_percent: v })}
              />
              <CamNumberField
                label={t("cam.testPattern.powerSteps", "Steps")}
                value={params.power_steps}
                disabled={disabled}
                step={1}
                onChange={(v) => update({ power_steps: Math.max(2, Math.round(v)) })}
              />
            </div>
          </fieldset>

          {/* ── Speed sweep (rows) ──────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.testPattern.speedSweep", "Speed (rows)")}
            </legend>
            <div className="grid grid-cols-3 gap-2">
              <CamNumberField
                label={t("cam.testPattern.speedMin", "Min (mm/s)")}
                value={params.speed_min_mm_per_s}
                disabled={disabled}
                step={5}
                min={undefined}
                onChange={(v) => update({ speed_min_mm_per_s: v })}
              />
              <CamNumberField
                label={t("cam.testPattern.speedMax", "Max (mm/s)")}
                value={params.speed_max_mm_per_s}
                disabled={disabled}
                step={5}
                min={undefined}
                onChange={(v) => update({ speed_max_mm_per_s: v })}
              />
              <CamNumberField
                label={t("cam.testPattern.speedSteps", "Steps")}
                value={params.speed_steps}
                disabled={disabled}
                step={1}
                onChange={(v) => update({ speed_steps: Math.max(2, Math.round(v)) })}
              />
            </div>
          </fieldset>

          {/* ── Grid geometry ───────────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.testPattern.grid", "Grid")}
            </legend>
            <div className="grid grid-cols-2 gap-2">
              <CamNumberField
                label={t("cam.testPattern.cellSize", "Cell size (mm)")}
                value={params.cell_size_mm}
                disabled={disabled}
                step={1}
                onChange={(v) => update({ cell_size_mm: v })}
              />
              <CamNumberField
                label={t("cam.testPattern.cellSpacing", "Cell spacing (mm)")}
                value={params.cell_spacing_mm}
                disabled={disabled}
                step={1}
                onChange={(v) => update({ cell_spacing_mm: v })}
              />
              <CamNumberField
                label={t("cam.testPattern.startX", "Start X (mm)")}
                value={params.start_x_mm}
                disabled={disabled}
                step={1}
                min={undefined}
                onChange={(v) => update({ start_x_mm: v })}
              />
              <CamNumberField
                label={t("cam.testPattern.startY", "Start Y (mm)")}
                value={params.start_y_mm}
                disabled={disabled}
                step={1}
                min={undefined}
                onChange={(v) => update({ start_y_mm: v })}
              />
              {params.pattern === "engrave_grid" ? (
                <CamNumberField
                  label={t("cam.testPattern.lineSpacing", "Fill line spacing (mm)")}
                  value={params.line_spacing_mm}
                  disabled={disabled}
                  step={0.05}
                  onChange={(v) => update({ line_spacing_mm: v })}
                />
              ) : null}
              {params.pattern !== "kerf_gauge" ? (
                <CamCheckboxField
                  label={t("cam.testPattern.cellLabels", "Label every cell (P… S…)")}
                  checked={params.cell_labels}
                  disabled={disabled}
                  onChange={(v) => update({ cell_labels: v })}
                />
              ) : null}
            </div>
          </fieldset>

          {/* ── Kerf gauge cut settings ──────────────────────────── */}
          {params.pattern === "kerf_gauge" ? (
            <fieldset className="space-y-3">
              <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
                {t("cam.testPattern.gaugeSettings", "Gauge Cut Settings")}
              </legend>
              <div className="grid grid-cols-3 gap-2">
                <CamNumberField
                  label={t("cam.testPattern.kerfWidth", "Kerf (mm)")}
                  value={params.kerf_width_mm}
                  disabled={disabled}
                  step={0.05}
                  onChange={(v) => update({ kerf_width_mm: v })}
                />
                <CamNumberField
                  label={t("cam.testPattern.gaugePower", "Power (%)")}
                  value={params.power_percent}
                  disabled={disabled}
                  step={5}
                  min={undefined}
                  onChange={(v) => update({ power_percent: v })}
                />
                <CamNumberField
                  label={t("cam.testPattern.gaugeSpeed", "Speed (mm/s)")}
                  value={params.speed_mm_per_s}
                  disabled={disabled}
                  step={1}
                  min={undefined}
                  onChange={(v) => update({ speed_mm_per_s: v })}
                />
              </div>
              <p className="text-[10px] leading-relaxed text-on-surface-dim">
                {t("cam.testPattern.kerfGaugeNote")}
              </p>
            </fieldset>
          ) : null}

          {/* ── Status ──────────────────────────────────────────── */}
          <CamStatusLine
            status={status}
            statusMessage={statusMessage}
            toolpathStats={toolpathStats}
            prefix="cam.testPattern"
          />
        </ScrollArea>

        <div className="mt-3 grid grid-cols-2 gap-2 pt-2">
          <button
            type="button"
            className="cad-action-ghost"
            disabled={disabled}
            onClick={onClose}
          >
            {t("cam.testPattern.close", "Close")}
          </button>
          <button
            type="button"
            className="cad-action-ghost"
            disabled={disabled}
            onClick={() => {
              markUpdateSent();
              onUpdate(params);
              onPreview();
            }}
          >
            {t("cam.testPattern.preview", "Preview")}
          </button>
          <button
            type="submit"
            className="cad-action-primary col-span-2"
            disabled={disabled}
          >
            {t("cam.testPattern.generate", "Generate")}
          </button>
          <button
            type="button"
            className="cad-action-primary col-span-2"
            disabled={disabled}
            onClick={onExport}
          >
            {t("cam.testPattern.exportGcode", "Export G-code")}
          </button>
        </div>
      </form>
    </section>
  );
}
