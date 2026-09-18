// Floating tool editor — a two-column window with a live schematic on
// the left and the parameter fields on the right.  Edits stay in a
// local draft; Save commits the whole record (the caller dispatches
// cam_tool_add / cam_tool_update), Cancel discards.  Validation
// mirrors validate_tool_entry in the core
// (session_cam_commands.inc) so the Save button can never dispatch a
// record the core will reject.

import { useMemo, useState } from "react";
import { useTranslation } from "react-i18next";

import { Dropdown, ScrollArea } from "@/lib";
import type { ToolEntry, ToolType } from "@/types";
import { computedFeedMmPerMin, computedSpindleRpm } from "@/lib/toolOptions";
import {
  CamCheckboxField,
  CamNumberField,
  useCamEscapeCancel,
} from "./camPanelShared";
import { ToolSchematicView } from "./ToolSchematicView";

const TOOL_TYPES: ToolType[] = [
  "endmill_flat",
  "endmill_ball",
  "endmill_bull",
  "facemill",
  "drill",
  "spot_drill",
  "chamfer",
  "v_bit",
  "threadmill",
  "turning_insert",
  "laser",
  "plasma",
];

const CONE_TYPES: ToolType[] = ["chamfer", "v_bit", "spot_drill", "drill"];
const RADIUS_TYPES: ToolType[] = ["endmill_flat", "endmill_bull"];
const NO_GEOMETRY_TYPES: ToolType[] = ["laser", "plasma"];

export interface ToolValidationIssue {
  severity: "error" | "warning";
  message: string;
}

export function validateToolDraft(
  tool: ToolEntry,
  library: ToolEntry[],
): ToolValidationIssue[] {
  const issues: ToolValidationIssue[] = [];
  const error = (message: string) => issues.push({ severity: "error", message });
  const warn = (message: string) =>
    issues.push({ severity: "warning", message });

  if (!tool.name.trim()) {
    error("nameRequired");
  }
  if (tool.diameter_mm <= 0) {
    error("diameterPositive");
  }
  if (tool.shank_diameter_mm <= 0) {
    error("shankPositive");
  }
  if (tool.flute_length_mm <= 0) {
    error("flutePositive");
  }
  if (tool.overall_length_mm < tool.flute_length_mm) {
    error("overallAtLeastFlute");
  }
  if (tool.corner_radius_mm < 0 || tool.corner_radius_mm > tool.diameter_mm / 2) {
    error("cornerRadiusRange");
  }
  if (tool.flutes < 1 || tool.flutes > 12) {
    error("flutesRange");
  }
  if (tool.orientation < 0 || tool.orientation > 9) {
    error("orientationRange");
  }
  if (tool.tool_number < 0) {
    error("toolNumberRange");
  }
  if (tool.tool_number > 0) {
    const clash = library.find(
      (existing) =>
        existing.tool_id !== tool.tool_id &&
        existing.tool_number === tool.tool_number,
    );
    if (clash) {
      error("toolNumberClash");
    }
  }
  // Consistent-shape warnings — the core accepts these but they
  // almost always indicate a typo.
  if (tool.shoulder_length_mm > 0 && tool.shoulder_length_mm > tool.overall_length_mm) {
    warn("shoulderAboveOverall");
  }
  if (CONE_TYPES.includes(tool.type) && tool.tip_diameter_mm >= tool.diameter_mm) {
    warn("tipDiameterLarge");
  }
  return issues;
}

interface CamToolEditorPanelProps {
  /** Seed record — the existing tool (edit) or a blank template with
   *  tool_number 0 so the core auto-assigns (create). */
  tool: ToolEntry;
  mode: "create" | "edit";
  /** Document library for the duplicate tool-number check. */
  library: ToolEntry[];
  onSave: (tool: ToolEntry) => void;
  onClose: () => void;
}

export function CamToolEditorPanel({
  tool: seed,
  mode,
  library,
  onSave,
  onClose,
}: CamToolEditorPanelProps) {
  const { t } = useTranslation();
  const [draft, setDraft] = useState<ToolEntry>(seed);
  const [effectiveDepth, setEffectiveDepth] = useState(0.5);

  useCamEscapeCancel(onClose);

  const issues = useMemo(() => validateToolDraft(draft, library), [draft, library]);
  const hasErrors = issues.some((issue) => issue.severity === "error");
  const showGeometry = !NO_GEOMETRY_TYPES.includes(draft.type);
  const showCone = CONE_TYPES.includes(draft.type);
  const showRadius = RADIUS_TYPES.includes(draft.type);
  const showLathe = draft.type === "turning_insert";

  const rpm = computedSpindleRpm(draft);
  const feed = computedFeedMmPerMin(draft);
  const effectiveDiameter =
    draft.tip_diameter_mm +
    2 * effectiveDepth * Math.tan((draft.point_angle_deg / 2) * (Math.PI / 180));

  function patch(partial: Partial<ToolEntry>) {
    setDraft((prev) => ({ ...prev, ...partial }));
  }

  const typeOptions = TOOL_TYPES.map((type) => ({
    value: type,
    label: t(`cam.toolEditor.types.${type}`, type),
  }));

  return (
    <section className="pointer-events-auto cad-floating-panel flex max-h-full min-h-0 w-[560px] max-w-full flex-col overflow-hidden px-5 py-5">
      <div className="flex items-center justify-between">
        <p className="cad-kicker">
          {mode === "create"
            ? t("cam.toolEditor.createTitle", "New Tool")
            : t("cam.toolEditor.title", "Tool")}
          {draft.tool_number > 0 ? ` — T${draft.tool_number}` : ""}
        </p>
        <button
          type="button"
          className="cad-action-ghost h-7 px-2 text-[10px] uppercase tracking-wider hover:opacity-80"
          onClick={onClose}
        >
          {t("cam.toolEditor.close", "Close")}
        </button>
      </div>

      <form
        className="mt-4 flex min-h-0 flex-1 gap-4 overflow-hidden"
        onSubmit={(event) => {
          event.preventDefault();
          if (!hasErrors) {
            onSave(draft);
          }
        }}
      >
        {/* ── Schematic ──────────────────────────────────────────── */}
        <div className="cad-panel-soft flex w-[224px] shrink-0 flex-col rounded-xl p-2">
          <ToolSchematicView tool={draft} className="h-full w-full" />
          <p className="mt-1 truncate text-center text-[10px] text-on-surface-muted">
            {draft.name || t("cam.toolEditor.untitled", "Untitled tool")}
          </p>
        </div>

        {/* ── Fields ─────────────────────────────────────────────── */}
        <div className="flex min-h-0 min-w-0 flex-1 flex-col">
          <ScrollArea className="min-h-0 flex-1" viewportClassName="space-y-4 pr-2">
            {/* Identity */}
            <fieldset className="space-y-2">
              <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
                {t("cam.toolEditor.identity", "Identity")}
              </legend>
              <div className="grid grid-cols-2 gap-2">
                <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                  {t("cam.toolEditor.type", "Type")}
                  <div className="mt-2">
                    <Dropdown
                      className="w-full"
                      value={draft.type}
                      label={t("cam.toolEditor.type", "Type")}
                      options={typeOptions}
                      onChange={(value) => patch({ type: value as ToolType })}
                    />
                  </div>
                </label>
                <CamNumberField
                  label={t("cam.toolEditor.toolNumber", "Tool # (0 = auto)")}
                  value={draft.tool_number}
                  disabled={false}
                  step={1}
                  min={0}
                  onChange={(v) => patch({ tool_number: v ?? 0 })}
                />
                <CamNumberField
                  label={t("cam.toolEditor.pocketNumber", "Pocket # (0 = tool #)")}
                  value={draft.pocket_number}
                  disabled={false}
                  step={1}
                  min={0}
                  onChange={(v) => patch({ pocket_number: v ?? 0 })}
                />
              </div>
              <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                {t("cam.toolEditor.name", "Name")}
                <input
                  className="cad-input mt-2 w-full"
                  type="text"
                  value={draft.name}
                  onChange={(event) => patch({ name: event.currentTarget.value })}
                />
              </label>
              <div className="grid grid-cols-2 gap-2">
                <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                  {t("cam.toolEditor.vendor", "Vendor")}
                  <input
                    className="cad-input mt-2 w-full"
                    type="text"
                    value={draft.vendor}
                    onChange={(event) => patch({ vendor: event.currentTarget.value })}
                  />
                </label>
                <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                  {t("cam.toolEditor.productId", "Product ID")}
                  <input
                    className="cad-input mt-2 w-full"
                    type="text"
                    value={draft.product_id}
                    onChange={(event) => patch({ product_id: event.currentTarget.value })}
                  />
                </label>
              </div>
              <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                {t("cam.toolEditor.description", "Description")}
                <input
                  className="cad-input mt-2 w-full"
                  type="text"
                  value={draft.description}
                  onChange={(event) => patch({ description: event.currentTarget.value })}
                />
              </label>
            </fieldset>

            {showGeometry && (
              <fieldset className="space-y-3">
                <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
                  {t("cam.toolEditor.geometry", "Geometry")}
                </legend>
                <div className="grid grid-cols-2 gap-2">
                  <CamNumberField
                    label={t("cam.toolEditor.diameter", "Diameter (mm)")}
                    value={draft.diameter_mm}
                    disabled={false}
                    step={0.1}
                    onChange={(v) => patch({ diameter_mm: v ?? 0 })}
                  />
                  <CamNumberField
                    label={t("cam.toolEditor.shankDiameter", "Shank Ø (mm)")}
                    value={draft.shank_diameter_mm}
                    disabled={false}
                    step={0.1}
                    onChange={(v) => patch({ shank_diameter_mm: v ?? 0 })}
                  />
                  <CamNumberField
                    label={t("cam.toolEditor.fluteLength", "Flute length (mm)")}
                    value={draft.flute_length_mm}
                    disabled={false}
                    step={0.5}
                    onChange={(v) => patch({ flute_length_mm: v ?? 0 })}
                  />
                  <CamNumberField
                    label={t("cam.toolEditor.overallLength", "Overall length (mm)")}
                    value={draft.overall_length_mm}
                    disabled={false}
                    step={0.5}
                    onChange={(v) => patch({ overall_length_mm: v ?? 0 })}
                  />
                  <CamNumberField
                    label={t("cam.toolEditor.shoulderLength", "Shoulder (mm, 0 = flute)")}
                    value={draft.shoulder_length_mm}
                    disabled={false}
                    step={0.5}
                    onChange={(v) => patch({ shoulder_length_mm: v ?? 0 })}
                  />
                  <CamNumberField
                    label={t("cam.toolEditor.flutes", "Flutes")}
                    value={draft.flutes}
                    disabled={false}
                    step={1}
                    min={1}
                    max={12}
                    onChange={(v) => patch({ flutes: v ?? 2 })}
                  />
                  <CamNumberField
                    label={t("cam.toolEditor.helixAngle", "Helix angle (°)")}
                    value={draft.helix_angle_deg}
                    disabled={false}
                    step={1}
                    min={undefined}
                    onChange={(v) => patch({ helix_angle_deg: v ?? 0 })}
                  />
                  {showRadius && (
                    <CamNumberField
                      label={t("cam.toolEditor.cornerRadius", "Corner radius (mm)")}
                      value={draft.corner_radius_mm}
                      disabled={false}
                      step={0.1}
                      onChange={(v) => patch({ corner_radius_mm: v ?? 0 })}
                    />
                  )}
                  {showCone && (
                    <>
                      <CamNumberField
                        label={t("cam.toolEditor.pointAngle", "Point angle (°)")}
                        value={draft.point_angle_deg}
                        disabled={false}
                        step={1}
                        min={1}
                        max={180}
                        onChange={(v) => patch({ point_angle_deg: v ?? 0 })}
                      />
                      <CamNumberField
                        label={t("cam.toolEditor.tipDiameter", "Tip Ø (mm)")}
                        value={draft.tip_diameter_mm}
                        disabled={false}
                        step={0.05}
                        onChange={(v) => patch({ tip_diameter_mm: v ?? 0 })}
                      />
                    </>
                  )}
                </div>
                <CamNumberField
                  label={t("cam.toolEditor.taperAngle", "Taper angle (°)")}
                  value={draft.taper_angle_deg}
                  disabled={false}
                  step={0.5}
                  min={undefined}
                  onChange={(v) => patch({ taper_angle_deg: v ?? 0 })}
                />
              </fieldset>
            )}

            {/* Cutting data — the primary inputs; RPM/feed are derived
                and shown as readouts. */}
            {!NO_GEOMETRY_TYPES.includes(draft.type) && (
              <fieldset className="space-y-3">
                <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
                  {t("cam.toolEditor.cuttingData", "Cutting Data")}
                </legend>
                <div className="grid grid-cols-2 gap-2">
                  <CamNumberField
                    label={t("cam.toolEditor.surfaceSpeed", "Surface speed (m/min)")}
                    value={draft.surface_speed_m_per_min}
                    disabled={false}
                    step={5}
                    onChange={(v) => patch({ surface_speed_m_per_min: v ?? 0 })}
                  />
                  <CamNumberField
                    label={t("cam.toolEditor.feedPerTooth", "Feed / tooth (mm)")}
                    value={draft.feed_per_tooth_mm}
                    disabled={false}
                    step={0.005}
                    onChange={(v) => patch({ feed_per_tooth_mm: v ?? 0 })}
                  />
                </div>
                <p className="text-[10px] leading-relaxed text-on-surface-dim">
                  {t("cam.toolEditor.derivedRpm", "Derived spindle:")} {rpm > 0 ? `${rpm} RPM` : "—"}
                  {" · "}
                  {t("cam.toolEditor.derivedFeed", "feed:")} {feed > 0 ? `${feed} mm/min` : "—"}
                </p>
                {showCone && (
                  <div className="flex items-end gap-2">
                    <div className="flex-1">
                      <CamNumberField
                        label={t("cam.toolEditor.effectiveDepth", "Cut depth (mm)")}
                        value={effectiveDepth}
                        disabled={false}
                        step={0.1}
                        onChange={(v) => setEffectiveDepth(v ?? 0)}
                      />
                    </div>
                    <p className="flex-1 pb-1 text-[10px] leading-relaxed text-on-surface-dim">
                      {t("cam.toolEditor.effectiveDiameter", "Effective Ø at depth:")}{" "}
                      {Math.min(effectiveDiameter, draft.diameter_mm).toFixed(3)} mm
                    </p>
                  </div>
                )}
              </fieldset>
            )}

            {/* Lathe insert data */}
            {showLathe && (
              <fieldset className="space-y-3">
                <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
                  {t("cam.toolEditor.lathe", "Lathe Insert")}
                </legend>
                <div className="grid grid-cols-2 gap-2">
                  <CamNumberField
                    label={t("cam.toolEditor.frontAngle", "Front angle (°)")}
                    value={draft.front_angle_deg}
                    disabled={false}
                    step={1}
                    min={undefined}
                    onChange={(v) => patch({ front_angle_deg: v ?? 0 })}
                  />
                  <CamNumberField
                    label={t("cam.toolEditor.backAngle", "Back angle (°)")}
                    value={draft.back_angle_deg}
                    disabled={false}
                    step={1}
                    min={undefined}
                    onChange={(v) => patch({ back_angle_deg: v ?? 0 })}
                  />
                  <CamNumberField
                    label={t("cam.toolEditor.orientation", "Orientation (0–9)")}
                    value={draft.orientation}
                    disabled={false}
                    step={1}
                    min={0}
                    max={9}
                    onChange={(v) => patch({ orientation: v ?? 0 })}
                  />
                </div>
              </fieldset>
            )}

            <CamCheckboxField
              label={t("cam.toolEditor.coolantThrough", "Coolant through")}
              checked={draft.coolant_through}
              disabled={false}
              onChange={(checked) => patch({ coolant_through: checked })}
            />

            {/* Validation — errors block Save (mirrors the core);
                warnings are advisory. */}
            {issues.length > 0 && (
              <ul className="space-y-1">
                {issues.map((issue) => (
                  <li
                    key={`${issue.severity}-${issue.message}`}
                    className={
                      issue.severity === "error"
                        ? "text-[10px] leading-relaxed text-danger"
                        : "text-[10px] leading-relaxed text-warning"
                    }
                  >
                    {t(
                      `cam.toolEditor.validation.${issue.message}`,
                      issue.message,
                    )}
                  </li>
                ))}
              </ul>
            )}
          </ScrollArea>

          <div className="mt-3 grid grid-cols-2 gap-2 pt-2">
            <button type="button" className="cad-action-ghost" onClick={onClose}>
              {t("cam.toolEditor.cancel", "Cancel")}
            </button>
            <button type="submit" className="cad-action-primary" disabled={hasErrors}>
              {t("cam.toolEditor.save", "Save")}
            </button>
          </div>
        </div>
      </form>
    </section>
  );
}
