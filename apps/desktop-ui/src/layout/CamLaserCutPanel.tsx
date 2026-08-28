import { useRef, useState } from "react";
import { useTranslation } from "react-i18next";

import { Dropdown, ScrollArea } from "@/lib";
import type { LaserCutParameters } from "@/types";
import {
  CamNumberField,
  CamStatusLine,
  type CamToolpathStats,
  useCamEscapeCancel,
  useDebouncedCamUpdate,
} from "./camPanelShared";

// Defaults mirror LaserCutParameters in cam_types.h.
export const DEFAULT_LASER_PARAMS: LaserCutParameters = {
  kerf_width_mm: 0.15,
  lead_in_mm: 2,
  lead_out_mm: 2,
  pierce_dwell_seconds: 0,
  power_percent: 85,
  passes: 1,
  mode: "cut",
  material_thickness_mm: 3,
  cut_plane_offset_mm: 0,
  dynamic_power: true,
};

type LaserMode = LaserCutParameters["mode"];

interface CamLaserCutPanelProps {
  operationName: string;
  initialParams: LaserCutParameters;
  initialFeedrate: number;
  status: string;
  statusMessage: string;
  toolpathStats: CamToolpathStats | null;
  disabled: boolean;
  onUpdate: (partial: Partial<LaserCutParameters>) => void;
  onFeedrateChange: (feedrateMmPerMin: number) => void;
  onPreview: () => void;
  onGenerate: () => void;
  onExport: () => void;
  onDelete: () => void;
  onClose: () => void;
}

export function CamLaserCutPanel({
  operationName,
  initialParams,
  initialFeedrate,
  status,
  statusMessage,
  toolpathStats,
  disabled,
  onUpdate,
  onFeedrateChange,
  onPreview,
  onGenerate,
  onExport,
  onDelete,
  onClose,
}: CamLaserCutPanelProps) {
  const { t } = useTranslation();
  const [params, setParams] = useState<LaserCutParameters>(() => ({
    ...DEFAULT_LASER_PARAMS,
    ...initialParams,
  }));
  const serialized = JSON.stringify(params);
  const markUpdateSent = useDebouncedCamUpdate(serialized, () => {
    onUpdate(params);
  });

  useCamEscapeCancel(onClose);

  function update(patch: Partial<LaserCutParameters>) {
    setParams((prev) => ({ ...prev, ...patch }));
  }

  return (
    <section className="pointer-events-auto cad-floating-panel flex max-h-full min-h-0 w-[320px] max-w-full flex-col overflow-hidden px-5 py-5">
      <div className="flex items-center justify-between">
        <p className="cad-kicker">{t("cam.laserCut.title", "Laser Cut")}</p>
        <button
          type="button"
          className="cad-action-ghost h-7 px-2 text-[10px] uppercase tracking-wider text-danger hover:opacity-80"
          disabled={disabled}
          onClick={onDelete}
        >
          {t("cam.laserCut.delete", "Delete")}
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
          {/* ── Mode ─────────────────────────────────────────────── */}
          <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
            {t("cam.laserCut.mode", "Mode")}
            <Dropdown
              className="mt-2 w-full"
              value={params.mode}
              label={t("cam.laserCut.mode", "Mode")}
              options={[
                { value: "cut", label: t("cam.laserCut.modeCut", "Cut") },
                { value: "score", label: t("cam.laserCut.modeScore", "Score") },
                { value: "engrave", label: t("cam.laserCut.modeEngrave", "Engrave") },
              ]}
              disabled={disabled}
              onChange={(value) => update({ mode: value as LaserMode })}
            />
          </label>

          {/* ── Cut parameters ───────────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.laserCut.cuttingParams", "Cutting Parameters")}
            </legend>
            <div className="grid grid-cols-2 gap-2">
              <CamNumberField
                label={t("cam.laserCut.kerf", "Kerf width (mm)")}
                value={params.kerf_width_mm}
                disabled={disabled}
                step={0.05}
                onChange={(v) => update({ kerf_width_mm: v })}
              />
              <CamNumberField
                label={t("cam.laserCut.power", "Power (%)")}
                value={params.power_percent}
                disabled={disabled}
                step={1}
                min={undefined}
                onChange={(v) => update({ power_percent: v })}
              />
              <CamNumberField
                label={t("cam.laserCut.leadIn", "Lead in (mm)")}
                value={params.lead_in_mm}
                disabled={disabled}
                step={0.5}
                onChange={(v) => update({ lead_in_mm: v })}
              />
              <CamNumberField
                label={t("cam.laserCut.leadOut", "Lead out (mm)")}
                value={params.lead_out_mm}
                disabled={disabled}
                step={0.5}
                onChange={(v) => update({ lead_out_mm: v })}
              />
              <CamNumberField
                label={t("cam.laserCut.pierceDwell", "Pierce dwell (s)")}
                value={params.pierce_dwell_seconds}
                disabled={disabled}
                step={0.1}
                onChange={(v) => update({ pierce_dwell_seconds: v })}
              />
              <CamNumberField
                label={t("cam.laserCut.passes", "Passes")}
                value={params.passes}
                disabled={disabled}
                step={1}
                onChange={(v) => update({ passes: Math.max(1, Math.round(v)) })}
              />
              <CamNumberField
                label={t("cam.laserCut.speed", "Speed (mm/min)")}
                value={initialFeedrate}
                disabled={disabled}
                step={50}
                min={undefined}
                onChange={(v) => onFeedrateChange(Math.max(1, v))}
              />
            </div>
          </fieldset>

          {/* ── Material ─────────────────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.laserCut.material", "Material")}
            </legend>
            <div className="grid grid-cols-2 gap-2">
              <CamNumberField
                label={t("cam.laserCut.materialThickness", "Thickness (mm)")}
                value={params.material_thickness_mm}
                disabled={disabled}
                step={0.5}
                onChange={(v) => update({ material_thickness_mm: v })}
              />
              <CamNumberField
                label={t("cam.laserCut.cutPlaneOffset", "Cut plane offset (mm)")}
                value={params.cut_plane_offset_mm}
                disabled={disabled}
                step={0.5}
                min={undefined}
                onChange={(v) => update({ cut_plane_offset_mm: v })}
              />
            </div>
          </fieldset>

          {/* ── Status ───────────────────────────────────────────── */}
          <CamStatusLine
            status={status}
            statusMessage={statusMessage}
            toolpathStats={toolpathStats}
            prefix="cam.laserCut"
          />
        </ScrollArea>

        <div className="mt-3 grid grid-cols-2 gap-2 pt-2">
          <button
            type="button"
            className="cad-action-ghost"
            disabled={disabled}
            onClick={onClose}
          >
            {t("cam.laserCut.close", "Close")}
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
            {t("cam.laserCut.preview", "Preview")}
          </button>
          <button
            type="submit"
            className="cad-action-primary col-span-2"
            disabled={disabled}
          >
            {t("cam.laserCut.generate", "Generate")}
          </button>
          <button
            type="button"
            className="cad-action-primary col-span-2"
            disabled={disabled}
            onClick={onExport}
          >
            {t("cam.laserCut.exportGcode", "Export G-code")}
          </button>
        </div>
      </form>
    </section>
  );
}
