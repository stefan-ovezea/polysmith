import { useRef, useState } from "react";
import { useTranslation } from "react-i18next";

import { Dropdown, ScrollArea } from "@/lib";
import {
  CamNumberField,
  useCamEscapeCancel,
  useDebouncedCamUpdate,
} from "./camPanelShared";

interface FaceMillingFormState {
  depth: number;
  stepover: number;
  angle_deg: number;
}

export interface ToolOption {
  tool_id: string;
  name: string;
  diameter: number;
}

interface FaceMillingPanelProps {
  operationName: string;
  initialParams: FaceMillingFormState;
  initialToolId: string;
  tools: ToolOption[];
  disabled: boolean;
  onUpdate: (params: FaceMillingFormState, toolId: string) => void;
  onDelete: () => void;
  onConfirm: () => void;
  onCancel: () => void;
}

export function FaceMillingPanel({
  operationName,
  initialParams,
  initialToolId,
  tools,
  disabled,
  onUpdate,
  onDelete,
  onConfirm,
  onCancel,
}: FaceMillingPanelProps) {
  const { t } = useTranslation();
  const [params, setParams] = useState<FaceMillingFormState>(initialParams);
  const [toolId, setToolId] = useState(initialToolId);
  const confirmRef = useRef(onConfirm);
  confirmRef.current = onConfirm;
  const serialized = JSON.stringify({ params, toolId });
  const markUpdateSent = useDebouncedCamUpdate(serialized, () => {
    onUpdate(params, toolId);
  });

  useCamEscapeCancel(onCancel);

  function update(patch: Partial<FaceMillingFormState>) {
    setParams((prev) => ({ ...prev, ...patch }));
  }

  const toolOptions = tools.map((tool) => ({
    value: tool.tool_id,
    label: `${tool.name} (Ø${tool.diameter} mm)`,
  }));

  return (
    <section className="pointer-events-auto cad-floating-panel flex max-h-full min-h-0 w-[320px] max-w-full flex-col overflow-hidden px-5 py-5">
      <div className="flex items-center justify-between">
        <p className="cad-kicker">{t("cam.faceMilling.title", "Face Milling")}</p>
        <button
          type="button"
          className="cad-action-ghost h-7 px-2 text-[10px] uppercase tracking-wider text-red-400 hover:text-red-300"
          disabled={disabled}
          onClick={onDelete}
        >
          {t("cam.faceMilling.delete", "Delete")}
        </button>
      </div>
      <p className="mt-1 text-xs text-on-surface-muted">
        {operationName}
      </p>

      <form
        className="mt-4 flex min-h-0 flex-1 flex-col overflow-hidden"
        onSubmit={(event) => {
          event.preventDefault();
          markUpdateSent();
          onUpdate(params, toolId);
          confirmRef.current();
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
                label={t("cam.faceMilling.depth", "Depth (mm)")}
                value={params.depth}
                disabled={disabled}
                step={0.1}
                onChange={(v) => update({ depth: v })}
              />
              <CamNumberField
                label={t("cam.faceMilling.stepover", "Stepover (mm)")}
                value={params.stepover}
                disabled={disabled}
                step={0.5}
                onChange={(v) => update({ stepover: v })}
              />
            </div>
            <CamNumberField
              label={t("cam.faceMilling.angle", "Angle (°)")}
              value={params.angle_deg}
              disabled={disabled}
              step={1}
              min={undefined}
              onChange={(v) => update({ angle_deg: v })}
            />
          </fieldset>

          <p className="text-[10px] leading-relaxed text-on-surface-dim">
            {t("cam.faceMilling.help", "Changes update the toolpath preview in real time.")}
          </p>
        </ScrollArea>

        <div className="mt-3 grid grid-cols-2 gap-2 pt-2">
          <button
            type="button"
            className="cad-action-ghost"
            disabled={disabled}
            onClick={onCancel}
          >
            {t("cam.faceMilling.close", "Close")}
          </button>
          <button type="submit" className="cad-action-primary" disabled={disabled}>
            {t("cam.faceMilling.ok", "OK")}
          </button>
        </div>
      </form>
    </section>
  );
}
