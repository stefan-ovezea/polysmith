import { useState } from "react";
import { useTranslation } from "react-i18next";

import { Dropdown, ScrollArea } from "@/lib";
import { contourParametersSchema } from "@/lib/schemas/ipc/camSchema";
import type { ContourParameters, ToolEntry } from "@/types";
import {
  CamNumberField,
  CamStatusLine,
  type CamToolpathStats,
  useCamEscapeCancel,
  useDebouncedCamUpdate,
} from "./camPanelShared";

// Editable 2D-contour parameters: the shared milling fields (feedrate,
// plunge, spindle) plus the contour block (side / depth / allowance).
export interface ContourFormState {
  side: ContourParameters["side"];
  depth_mm: number;
  stock_allowance_mm: number;
  feedrate_mm_per_min: number;
  plunge_feedrate_mm_per_min: number;
  spindle_rpm: number;
}

// Contour-block defaults come from the zod schema — single source of
// truth, no parallel constants block to drift from cam_types.h.  The
// shared milling fields follow the pocket trigger defaults.
export const DEFAULT_CONTOUR_PARAMS: ContourParameters =
  contourParametersSchema.parse({});

export const DEFAULT_CONTOUR_FORM: ContourFormState = {
  ...DEFAULT_CONTOUR_PARAMS,
  feedrate_mm_per_min: 1200,
  plunge_feedrate_mm_per_min: 600,
  spindle_rpm: 8000,
};

// How this operation's geometry was picked: a body face (face witness)
// or sketch profiles (profile attestations).  null = no geometry yet.
export type ContourInputKind = "face" | "profile" | null;

interface CamContourPanelProps {
  operationName: string;
  initialParams: ContourFormState;
  initialToolId: string;
  tools: ToolEntry[];
  inputKind: ContourInputKind;
  // Profile-mode geometry (laser panel pattern).
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
  // Face-mode geometry (pocket panel pattern).
  facePickArmed: boolean;
  onRepickFace: () => void;
  onCancelFacePick: () => void;
  status: string;
  statusMessage: string;
  toolpathStats: CamToolpathStats | null;
  disabled: boolean;
  onUpdate: (partial: Partial<ContourFormState>, toolId: string) => void;
  onPreview: () => void;
  onGenerate: () => void;
  onExport: () => void;
  onDelete: () => void;
  onClose: () => void;
}

export function CamContourPanel({
  operationName,
  initialParams,
  initialToolId,
  tools,
  inputKind,
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
  facePickArmed,
  onRepickFace,
  onCancelFacePick,
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
}: CamContourPanelProps) {
  const { t } = useTranslation();
  const [params, setParams] = useState<ContourFormState>(() => ({
    ...DEFAULT_CONTOUR_FORM,
    ...initialParams,
  }));
  const [toolId, setToolId] = useState(initialToolId);
  const serialized = JSON.stringify({ params, toolId });
  const markUpdateSent = useDebouncedCamUpdate(serialized, () => {
    onUpdate(params, toolId);
  });

  // Escape cancels an armed pick first; only then closes the panel.
  useCamEscapeCancel(() => {
    if (facePickArmed) {
      onCancelFacePick();
    } else if (repickArmed) {
      onCancelRepick();
    } else {
      onClose();
    }
  });

  function update(patch: Partial<ContourFormState>) {
    setParams((prev) => ({ ...prev, ...patch }));
  }

  const toolOptions = tools.map((tool) => ({
    value: tool.tool_id,
    label: `${tool.name} (Ø${tool.diameter_mm} mm)`,
  }));

  return (
    <section className="pointer-events-auto cad-floating-panel flex max-h-full min-h-0 w-[320px] max-w-full flex-col overflow-hidden px-5 py-5">
      <div className="flex items-center justify-between">
        <p className="cad-kicker">{t("cam.contour.title", "2D Contour")}</p>
        <button
          type="button"
          className="cad-action-ghost h-7 px-2 text-[10px] uppercase tracking-wider text-danger hover:opacity-80"
          disabled={disabled}
          onClick={onDelete}
        >
          {t("cam.contour.delete", "Delete")}
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
              {t("cam.contour.tool", "Tool")}
            </legend>
            <Dropdown
              className="w-full"
              value={toolId}
              label={t("cam.contour.tool", "Tool")}
              options={toolOptions}
              disabled={disabled}
              onChange={(value) => setToolId(value)}
            />
          </fieldset>

          {/* ── Geometry ─────────────────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.contour.geometry", "Geometry")}
            </legend>
            {inputKind === "profile" || inputKind === null ? (
              <>
                <p className="text-[10px] leading-relaxed text-on-surface-dim">
                  {t("cam.contour.geometrySummary", {
                    count: geometryCount,
                  })}
                  {repickArmed
                    ? t("cam.contour.pickHint", {
                        count: selectedProfileCount,
                      })
                    : null}
                </p>
                <Dropdown
                  className="w-full"
                  value={scopeSketchId ?? ""}
                  label={t("cam.contour.scopeSketch", "Reference sketch")}
                  options={[
                    {
                      value: "",
                      label: scopeSketchId
                        ? t("cam.contour.scopeMixed", "Selected profiles")
                        : t("cam.contour.scopeNone", "No sketch selected"),
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
                      {t("cam.contour.applyRepick", "Apply selection")}
                    </button>
                    <button
                      type="button"
                      className="cad-action-ghost px-2 py-1 text-[10px] uppercase tracking-wider"
                      disabled={disabled}
                      onClick={onCancelRepick}
                    >
                      {t("cam.contour.cancelRepick", "Cancel")}
                    </button>
                    {selectedProfileCount > 0 ? (
                      <button
                        type="button"
                        className="cad-action-ghost col-span-2 px-2 py-1 text-[10px] uppercase tracking-wider"
                        disabled={disabled}
                        onClick={onClearSelection}
                      >
                        {t("cam.contour.clearSelection", "Clear selection")}
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
                    {t("cam.contour.reselectGeometry", "Re-select geometry")}
                  </button>
                )}
              </>
            ) : (
              <>
                <div className="flex items-center justify-between gap-2 rounded border border-[var(--cad-panel-border)] px-2 py-1.5">
                  <span className="min-w-0 truncate text-xs">
                    {t("cam.contour.inputFace", "Contour face")}
                  </span>
                  <button
                    type="button"
                    className="cad-action-ghost h-6 shrink-0 px-2 text-[10px] uppercase tracking-wider"
                    disabled={disabled}
                    onClick={onRepickFace}
                  >
                    {t("cam.contour.repickFace", "Re-pick")}
                  </button>
                </div>
                {facePickArmed ? (
                  <div className="space-y-2 rounded border border-[var(--cad-panel-border)] px-2 py-2">
                    <p className="text-[10px] leading-relaxed text-on-surface-dim">
                      {t(
                        "cam.contour.repickFaceHint",
                        "Click a face in the viewport to set the contour face.",
                      )}
                    </p>
                    <button
                      type="button"
                      className="cad-action-ghost h-6 w-full text-[10px] uppercase tracking-wider"
                      disabled={disabled}
                      onClick={onCancelFacePick}
                    >
                      {t("cam.contour.cancelPick", "Cancel pick")}
                    </button>
                  </div>
                ) : null}
              </>
            )}
          </fieldset>

          {/* ── Cutting Parameters ───────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.contour.cuttingParams", "Cutting Parameters")}
            </legend>
            <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.contour.side", "Side")}
              <Dropdown
                className="mt-2 w-full"
                value={params.side}
                label={t("cam.contour.side", "Side")}
                options={[
                  { value: "outside", label: t("cam.contour.sideOutside", "Outside") },
                  { value: "inside", label: t("cam.contour.sideInside", "Inside") },
                  { value: "on_line", label: t("cam.contour.sideOnLine", "On line") },
                ]}
                disabled={disabled}
                onChange={(value) =>
                  update({ side: value as ContourParameters["side"] })
                }
              />
            </label>
            <div className="grid grid-cols-2 gap-2">
              <CamNumberField
                label={t("cam.contour.depth", "Depth (mm)")}
                value={params.depth_mm}
                disabled={disabled}
                step={0.5}
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
                label={t("cam.contour.allowance", "Stock allowance (mm)")}
                value={params.stock_allowance_mm}
                disabled={disabled}
                step={0.05}
                onChange={(v) =>
                  update({ stock_allowance_mm: v ?? params.stock_allowance_mm })
                }
              />
              <CamNumberField
                label={t("cam.contour.feedrate", "Feedrate (mm/min)")}
                value={params.feedrate_mm_per_min}
                disabled={disabled}
                step={50}
                onChange={(v) =>
                  update({ feedrate_mm_per_min: v ?? params.feedrate_mm_per_min })
                }
              />
              <CamNumberField
                label={t("cam.contour.plungeFeedrate", "Plunge (mm/min)")}
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
            </div>
            <CamNumberField
              label={t("cam.contour.spindleRpm", "Spindle (RPM)")}
              value={params.spindle_rpm}
              disabled={disabled}
              step={100}
              onChange={(v) => update({ spindle_rpm: v ?? params.spindle_rpm })}
            />
          </fieldset>

          <p className="text-[10px] leading-relaxed text-on-surface-dim">
            {t(
              "cam.contour.help",
              "Changes update the toolpath preview in real time.",
            )}
          </p>

          {/* ── Status ───────────────────────────────────────────── */}
          <CamStatusLine
            status={status}
            statusMessage={statusMessage}
            toolpathStats={toolpathStats}
            prefix="cam.contour"
          />
        </ScrollArea>

        <div className="mt-3 grid grid-cols-2 gap-2 pt-2">
          <button
            type="button"
            className="cad-action-ghost"
            disabled={disabled}
            onClick={onClose}
          >
            {t("cam.contour.close", "Close")}
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
            {t("cam.contour.preview", "Preview")}
          </button>
          <button
            type="submit"
            className="cad-action-primary col-span-2"
            disabled={disabled}
          >
            {t("cam.contour.generate", "Generate")}
          </button>
          <button
            type="button"
            className="cad-action-primary col-span-2"
            disabled={disabled}
            onClick={onExport}
          >
            {t("cam.contour.exportGcode", "Export G-code")}
          </button>
        </div>
      </form>
    </section>
  );
}
