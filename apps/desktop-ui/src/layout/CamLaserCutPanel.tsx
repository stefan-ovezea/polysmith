import { useState } from "react";
import { useTranslation } from "react-i18next";

import { Dropdown, ScrollArea } from "@/lib";
import { laserCutParametersSchema } from "@/lib/schemas/ipc/camSchema";
import type { LaserCutParameters } from "@/types";
import {
  CamCheckboxField,
  CamNumberField,
  CamStatusLine,
  type CamToolpathStats,
  useCamEscapeCancel,
  useDebouncedCamUpdate,
} from "./camPanelShared";

// Defaults come from the zod schema — single source of truth, no
// parallel constants block to drift from cam_types.h.
export const DEFAULT_LASER_PARAMS: LaserCutParameters =
  laserCutParametersSchema.parse({});

type LaserMode = LaserCutParameters["mode"];

interface CamLaserCutPanelProps {
  operationName: string;
  initialParams: LaserCutParameters;
  // Legacy operations carry no speed_mm_per_s — display their
  // feedrate as mm/s until the user types a speed.
  initialFeedrate: number;
  status: string;
  statusMessage: string;
  toolpathStats: CamToolpathStats | null;
  disabled: boolean;
  onUpdate: (partial: Partial<LaserCutParameters>) => void;
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

  // Legacy ops carry no speed_mm_per_s — display their feedrate as
  // mm/s, rounded to one decimal so the field reads cleanly (the
  // exact fallback value stays in the core until the user types one).
  const speedValue =
    params.speed_mm_per_s !== undefined
      ? params.speed_mm_per_s
      : Math.round((initialFeedrate / 60.0) * 10) / 10;

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

          {/* ── Cutting parameters ───────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.laserCut.cuttingParams", "Cutting Parameters")}
            </legend>
            <div className="grid grid-cols-2 gap-2">
              <CamNumberField
                label={t("cam.laserCut.power", "Power (%)")}
                value={params.power_percent}
                disabled={disabled}
                step={1}
                min={undefined}
                onChange={(v) => update({ power_percent: v })}
              />
              <CamNumberField
                label={t("cam.laserCut.speed", "Speed (mm/s)")}
                value={speedValue}
                disabled={disabled}
                step="any"
                min={undefined}
                onChange={(v) => update({ speed_mm_per_s: Math.max(0.1, v) })}
              />
              <CamNumberField
                label={t("cam.laserCut.passes", "Passes")}
                value={params.passes}
                disabled={disabled}
                step={1}
                onChange={(v) => update({ passes: Math.max(1, Math.round(v)) })}
              />
              <CamNumberField
                label={t("cam.laserCut.kerf", "Kerf width (mm)")}
                value={params.kerf_width_mm}
                disabled={disabled}
                step={0.05}
                onChange={(v) => update({ kerf_width_mm: v })}
              />
            </div>
            <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.laserCut.kerfSide", "Kerf side")}
              <Dropdown
                className="mt-2 w-full"
                value={params.kerf_side}
                label={t("cam.laserCut.kerfSide", "Kerf side")}
                options={[
                  { value: "auto", label: t("cam.laserCut.kerfSideAuto", "Auto (scrap side)") },
                  { value: "outside", label: t("cam.laserCut.kerfSideOutside", "Outside") },
                  { value: "inside", label: t("cam.laserCut.kerfSideInside", "Inside") },
                  { value: "none", label: t("cam.laserCut.kerfSideNone", "None") },
                ]}
                disabled={disabled}
                onChange={(value) => update({ kerf_side: value as LaserCutParameters["kerf_side"] })}
              />
            </label>
            <div className="grid grid-cols-2 gap-2">
              <CamCheckboxField
                label={t("cam.laserCut.dynamicPower", "Dynamic power (M4)")}
                checked={params.dynamic_power}
                disabled={disabled}
                onChange={(v) => update({ dynamic_power: v })}
              />
              <CamCheckboxField
                label={t("cam.laserCut.airAssist", "Air assist (M8)")}
                checked={params.air_assist}
                disabled={disabled}
                onChange={(v) => update({ air_assist: v })}
              />
            </div>
          </fieldset>

          {/* ── Leads ────────────────────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.laserCut.leads", "Leads")}
            </legend>
            <div className="grid grid-cols-2 gap-2">
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
            </div>
            <div className="grid grid-cols-2 gap-2">
              <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                {t("cam.laserCut.leadInStyle", "Lead-in style")}
                <Dropdown
                  className="mt-2 w-full"
                  value={params.lead_in_style}
                  label={t("cam.laserCut.leadInStyle", "Lead-in style")}
                  options={[
                    { value: "line", label: t("cam.laserCut.leadStyleLine", "Line") },
                    { value: "arc", label: t("cam.laserCut.leadStyleArc", "Arc") },
                  ]}
                  disabled={disabled}
                  onChange={(value) => update({ lead_in_style: value as LaserCutParameters["lead_in_style"] })}
                />
              </label>
              <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                {t("cam.laserCut.leadOutStyle", "Lead-out style")}
                <Dropdown
                  className="mt-2 w-full"
                  value={params.lead_out_style}
                  label={t("cam.laserCut.leadOutStyle", "Lead-out style")}
                  options={[
                    { value: "line", label: t("cam.laserCut.leadStyleLine", "Line") },
                    { value: "arc", label: t("cam.laserCut.leadStyleArc", "Arc") },
                  ]}
                  disabled={disabled}
                  onChange={(value) => update({ lead_out_style: value as LaserCutParameters["lead_out_style"] })}
                />
              </label>
              <CamNumberField
                label={t("cam.laserCut.leadInAngle", "Lead-in angle (°)")}
                value={params.lead_in_angle_deg}
                disabled={disabled}
                step={5}
                min={undefined}
                onChange={(v) => update({ lead_in_angle_deg: v })}
              />
              <CamNumberField
                label={t("cam.laserCut.leadOutAngle", "Lead-out angle (°)")}
                value={params.lead_out_angle_deg}
                disabled={disabled}
                step={5}
                min={undefined}
                onChange={(v) => update({ lead_out_angle_deg: v })}
              />
              <CamNumberField
                label={t("cam.laserCut.overcut", "Overcut (mm)")}
                value={params.overcut_mm}
                disabled={disabled}
                step={0.5}
                onChange={(v) => update({ overcut_mm: v })}
              />
            </div>
          </fieldset>

          {/* ── Pierce ───────────────────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.laserCut.pierce", "Pierce")}
            </legend>
            <div className="grid grid-cols-2 gap-2">
              <CamNumberField
                label={t("cam.laserCut.pierceDwell", "Pierce dwell (s)")}
                value={params.pierce_dwell_seconds}
                disabled={disabled}
                step={0.1}
                onChange={(v) => update({ pierce_dwell_seconds: v })}
              />
              <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                {t("cam.laserCut.piercePosition", "Pierce position")}
                <Dropdown
                  className="mt-2 w-full"
                  value={params.pierce_position}
                  label={t("cam.laserCut.piercePosition", "Pierce position")}
                  options={[
                    { value: "auto", label: t("cam.laserCut.pierceAuto", "Auto") },
                    { value: "lead_start", label: t("cam.laserCut.pierceLeadStart", "Lead start") },
                    { value: "nearest_centroid", label: t("cam.laserCut.pierceCentroid", "Near centroid") },
                  ]}
                  disabled={disabled}
                  onChange={(value) => update({ pierce_position: value as LaserCutParameters["pierce_position"] })}
                />
              </label>
            </div>
          </fieldset>

          {/* ── Tabs ─────────────────────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.laserCut.tabs", "Tabs")}
            </legend>
            <CamCheckboxField
              label={t("cam.laserCut.tabsEnabled", "Hold the part with tabs")}
              checked={params.tabs_enabled}
              disabled={disabled}
              onChange={(v) => update({ tabs_enabled: v })}
            />
            <div className="grid grid-cols-2 gap-2">
              <CamNumberField
                label={t("cam.laserCut.tabWidth", "Tab width (mm)")}
                value={params.tab_width_mm}
                disabled={disabled || !params.tabs_enabled}
                step={0.1}
                onChange={(v) => update({ tab_width_mm: v })}
              />
              <CamNumberField
                label={t("cam.laserCut.tabSpacing", "Tab spacing (mm)")}
                value={params.tab_spacing_mm}
                disabled={disabled || !params.tabs_enabled}
                step={1}
                onChange={(v) => update({ tab_spacing_mm: v })}
              />
              <CamNumberField
                label={t("cam.laserCut.tabPower", "Tab power (%)")}
                value={params.tab_power_percent}
                disabled={disabled || !params.tabs_enabled}
                step={1}
                min={undefined}
                onChange={(v) => update({ tab_power_percent: v })}
              />
              <CamCheckboxField
                label={t("cam.laserCut.tabsOnHoles", "Tabs on holes")}
                checked={params.tabs_on_holes}
                disabled={disabled || !params.tabs_enabled}
                onChange={(v) => update({ tabs_on_holes: v })}
              />
            </div>
          </fieldset>

          {/* ── Engrave fill ─────────────────────────────────────── */}
          {params.mode === "engrave" ? (
            <fieldset className="space-y-3">
              <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
                {t("cam.laserCut.fill", "Engrave Fill")}
              </legend>
              <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                {t("cam.laserCut.engraveStyle", "Engrave style")}
                <Dropdown
                  className="mt-2 w-full"
                  value={params.engrave_style}
                  label={t("cam.laserCut.engraveStyle", "Engrave style")}
                  options={[
                    { value: "line", label: t("cam.laserCut.engraveLine", "Contour trace") },
                    { value: "fill", label: t("cam.laserCut.engraveFill", "Fill / hatch") },
                  ]}
                  disabled={disabled}
                  onChange={(value) => update({ engrave_style: value as LaserCutParameters["engrave_style"] })}
                />
              </label>
              {params.engrave_style === "fill" ? (
                <div className="grid grid-cols-2 gap-2">
                  <CamNumberField
                    label={t("cam.laserCut.lineSpacing", "Line spacing (mm)")}
                    value={params.line_spacing_mm}
                    disabled={disabled}
                    step={0.05}
                    onChange={(v) => update({ line_spacing_mm: v })}
                  />
                  <CamNumberField
                    label={t("cam.laserCut.fillAngle", "Fill angle (°)")}
                    value={params.fill_angle_deg}
                    disabled={disabled}
                    step={5}
                    min={undefined}
                    onChange={(v) => update({ fill_angle_deg: v })}
                  />
                  <CamCheckboxField
                    label={t("cam.laserCut.fillBidirectional", "Bidirectional")}
                    checked={params.fill_bidirectional}
                    disabled={disabled}
                    onChange={(v) => update({ fill_bidirectional: v })}
                  />
                </div>
              ) : null}
            </fieldset>
          ) : null}

          {/* ── Ordering ─────────────────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.laserCut.ordering", "Cut Ordering")}
            </legend>
            <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.laserCut.cutOrder", "Cut order")}
              <Dropdown
                className="mt-2 w-full"
                value={params.cut_order}
                label={t("cam.laserCut.cutOrder", "Cut order")}
                options={[
                  { value: "inner_first", label: t("cam.laserCut.cutOrderInnerFirst", "Inner shapes first") },
                  { value: "nearest_neighbor", label: t("cam.laserCut.cutOrderNearest", "Nearest neighbor") },
                  { value: "by_area", label: t("cam.laserCut.cutOrderByArea", "By area") },
                ]}
                disabled={disabled}
                onChange={(value) => update({ cut_order: value as LaserCutParameters["cut_order"] })}
              />
            </label>
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
