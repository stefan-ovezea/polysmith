import { useRef, useState } from "react";
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

// Editable face-milling parameters.  Defaults mirror the C++ struct
// defaults in cam_types.h (CamOperationParameters).
export interface FaceMillingFormState {
  feedrate_mm_per_min: number;
  plunge_feedrate_mm_per_min: number;
  stepover_percent: number;
  zigzag_angle_deg: number;
  spindle_rpm: number;
}

export const DEFAULT_FACE_MILLING_PARAMS: FaceMillingFormState = {
  feedrate_mm_per_min: 1200,
  plunge_feedrate_mm_per_min: 600,
  stepover_percent: 50,
  zigzag_angle_deg: 0,
  spindle_rpm: 8000,
};

interface CamFaceMillingPanelProps {
  operationName: string;
  initialParams: FaceMillingFormState;
  initialToolId: string;
  tools: ToolEntry[];
  status: string;
  statusMessage: string;
  toolpathStats: CamToolpathStats | null;
  disabled: boolean;
  onUpdate: (partial: Partial<CamOperationParameters>, toolId: string) => void;
  onPreview: () => void;
  onGenerate: () => void;
  onExport: () => void;
  onDelete: () => void;
  onClose: () => void;
}

export function CamFaceMillingPanel({
  operationName,
  initialParams,
  initialToolId,
  tools,
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
}: CamFaceMillingPanelProps) {
  const { t } = useTranslation();
  const [params, setParams] = useState<FaceMillingFormState>(() => ({
    ...DEFAULT_FACE_MILLING_PARAMS,
    ...initialParams,
  }));
  const [toolId, setToolId] = useState(initialToolId);
  const serialized = JSON.stringify({ params, toolId });
  const markUpdateSent = useDebouncedCamUpdate(serialized, () => {
    onUpdate(params, toolId);
  });

  useCamEscapeCancel(onClose);

  function update(patch: Partial<FaceMillingFormState>) {
    setParams((prev) => ({ ...prev, ...patch }));
  }

  const toolOptions = tools.map((tool) => ({
    value: tool.tool_id,
    label: `${tool.name} (Ø${tool.diameter_mm} mm)`,
  }));

  return (
    <section className="pointer-events-auto cad-floating-panel flex max-h-full min-h-0 w-[320px] max-w-full flex-col overflow-hidden px-5 py-5">
      <div className="flex items-center justify-between">
        <p className="cad-kicker">{t("cam.faceMilling.title", "Face Milling")}</p>
        <button
          type="button"
          className="cad-action-ghost h-7 px-2 text-[10px] uppercase tracking-wider text-danger hover:opacity-80"
          disabled={disabled}
          onClick={onDelete}
        >
          {t("cam.faceMilling.delete", "Delete")}
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
              {t("cam.faceMilling.tool", "Tool")}
            </legend>
            <Dropdown
              className="w-full"
              value={toolId}
              label={t("cam.faceMilling.tool", "Tool")}
              options={toolOptions}
              disabled={disabled}
              onChange={(value) => setToolId(value)}
            />
          </fieldset>

          {/* ── Cutting Parameters ───────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.faceMilling.cuttingParams", "Cutting Parameters")}
            </legend>
            <div className="grid grid-cols-2 gap-2">
              <CamNumberField
                label={t("cam.faceMilling.feedrate", "Feedrate (mm/min)")}
                value={params.feedrate_mm_per_min}
                disabled={disabled}
                step={50}
                onChange={(v) => update({ feedrate_mm_per_min: v })}
              />
              <CamNumberField
                label={t("cam.faceMilling.plungeFeedrate", "Plunge (mm/min)")}
                value={params.plunge_feedrate_mm_per_min}
                disabled={disabled}
                step={50}
                onChange={(v) => update({ plunge_feedrate_mm_per_min: v })}
              />
              <CamNumberField
                label={t("cam.faceMilling.stepoverPercent", "Stepover (%)")}
                value={params.stepover_percent}
                disabled={disabled}
                step={5}
                min={undefined}
                onChange={(v) => update({ stepover_percent: v })}
              />
              <CamNumberField
                label={t("cam.faceMilling.zigzagAngle", "Zigzag angle (°)")}
                value={params.zigzag_angle_deg}
                disabled={disabled}
                step={1}
                min={undefined}
                onChange={(v) => update({ zigzag_angle_deg: v })}
              />
            </div>
            <CamNumberField
              label={t("cam.faceMilling.spindleRpm", "Spindle (RPM)")}
              value={params.spindle_rpm}
              disabled={disabled}
              step={100}
              onChange={(v) => update({ spindle_rpm: v })}
            />
          </fieldset>

          <p className="text-[10px] leading-relaxed text-on-surface-dim">
            {t("cam.faceMilling.help", "Changes update the toolpath preview in real time.")}
          </p>

          {/* ── Status ───────────────────────────────────────────── */}
          <CamStatusLine
            status={status}
            statusMessage={statusMessage}
            toolpathStats={toolpathStats}
            prefix="cam.faceMilling"
          />
        </ScrollArea>

        <div className="mt-3 grid grid-cols-2 gap-2 pt-2">
          <button
            type="button"
            className="cad-action-ghost"
            disabled={disabled}
            onClick={onClose}
          >
            {t("cam.faceMilling.close", "Close")}
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
            {t("cam.faceMilling.preview", "Preview")}
          </button>
          <button
            type="submit"
            className="cad-action-primary col-span-2"
            disabled={disabled}
          >
            {t("cam.faceMilling.generate", "Generate")}
          </button>
          <button
            type="button"
            className="cad-action-primary col-span-2"
            disabled={disabled}
            onClick={onExport}
          >
            {t("cam.faceMilling.exportGcode", "Export G-code")}
          </button>
        </div>
      </form>
    </section>
  );
}
