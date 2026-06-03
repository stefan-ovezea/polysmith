import { useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";

import { Dropdown, ScrollArea } from "@/lib";

export interface CamSetupFormState {
  stock: { width: number; height: number; depth: number; offset_x: number; offset_y: number; offset_z: number };
  wcs_origin: { x: number; y: number; z: number };
  safety_plane_z: number;
  wcs_angle: number;
  orientation_mode: string;
  origin_mode: string;
}

interface CamSetupPanelProps {
  initialSetup: CamSetupFormState;
  bodies: Array<{ id: string; label: string }>;
  showStock: boolean;
  onShowStockChange: (show: boolean) => void;
  disabled: boolean;
  onUpdate: (state: CamSetupFormState) => void;
  onConfirm: () => void;
  onCancel: () => void;
}

function normalizeNumberInputValue(value: string) {
  if (value === "") return value;
  const sign = value.startsWith("-") ? "-" : "";
  const unsigned = sign ? value.slice(1) : value;
  if (unsigned.startsWith("0.") || unsigned === "0") return value;
  const normalized = unsigned.replace(/^0+(?=\d)/, "");
  return `${sign}${normalized || "0"}`;
}

function readNumberInputValue(input: HTMLInputElement) {
  const normalized = normalizeNumberInputValue(input.value);
  if (normalized !== input.value) input.value = normalized;
  return Number(normalized);
}

function StockField({
  label,
  value,
  disabled,
  step = 0.5,
  min = 0,
  onChange,
}: {
  label: string;
  value: number;
  disabled: boolean;
  step?: number;
  min?: number;
  onChange: (v: number) => void;
}) {
  return (
    <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
      {label}
      <input
        className="cad-input mt-2"
        type="number"
        min={min}
        step={step}
        value={value}
        disabled={disabled}
        onChange={(event) => onChange(readNumberInputValue(event.currentTarget))}
      />
    </label>
  );
}

export function CamSetupPanel({
  initialSetup,
  bodies,
  showStock,
  onShowStockChange,
  disabled,
  onUpdate,
  onConfirm,
  onCancel,
}: CamSetupPanelProps) {
  const { t } = useTranslation();
  const [state, setState] = useState<CamSetupFormState>(initialSetup);
  const lastSentRef = useRef<string>("");
  const confirmRef = useRef(onConfirm);
  confirmRef.current = onConfirm;

  // Debounce: send updates to the core 200 ms after the last change.
  useEffect(() => {
    const serialized = JSON.stringify(state);
    if (serialized === lastSentRef.current) return;
    const timer = setTimeout(() => {
      lastSentRef.current = serialized;
      onUpdate(state);
    }, 200);
    return () => clearTimeout(timer);
  }, [state, onUpdate]);

  // Keyboard: Escape to cancel
  useEffect(() => {
    function handleKeyDown(event: KeyboardEvent) {
      if (event.key === "Escape") {
        if ((event.target as HTMLElement | null)?.closest(".cad-dropdown")) return;
        event.preventDefault();
        onCancel();
      }
    }
    window.addEventListener("keydown", handleKeyDown);
    return () => window.removeEventListener("keydown", handleKeyDown);
  }, [onCancel]);

  function updateStock(patch: Partial<CamSetupFormState["stock"]>) {
    setState((prev) => ({ ...prev, stock: { ...prev.stock, ...patch } }));
  }

  function updateWcs(patch: Partial<CamSetupFormState["wcs_origin"]>) {
    setState((prev) => ({ ...prev, wcs_origin: { ...prev.wcs_origin, ...patch } }));
  }

  return (
    <section className="pointer-events-auto cad-floating-panel flex max-h-full min-h-0 w-[340px] max-w-full flex-col overflow-hidden px-5 py-5">
      <p className="cad-kicker">{t("cam.setup.title", "Setup")}</p>
      <p className="mt-2 text-xs text-on-surface-muted">
        {t("cam.setup.description", "Configure machine, stock, and work coordinate system.")}
      </p>

      <form
        className="mt-4 flex min-h-0 flex-1 flex-col overflow-hidden"
        onSubmit={(event) => {
          event.preventDefault();
          // Flush any pending debounce before confirming
          lastSentRef.current = JSON.stringify(state);
          onUpdate(state);
          confirmRef.current();
        }}
      >
        <ScrollArea className="min-h-0 flex-1" viewportClassName="space-y-4 pr-4">
          {/* ── Machine ──────────────────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.setup.machine", "Machine")}
            </legend>
            <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.setup.axisCount", "Axis count")}
              <select
                className="cad-input mt-2 w-full"
                value={3}
                disabled
              >
                <option value={3}>{t("cam.setup.axis3", "3-Axis")}</option>
              </select>
              <span className="mt-1 block text-[10px] normal-case text-on-surface-dim">
                {t("cam.setup.axis3Note", "4-axis and 5-axis are planned for a future release.")}
              </span>
            </label>

            {/* Model body selector */}
            <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.setup.modelBody", "Model body")}
              <Dropdown
                className="mt-2 w-full"
                value={bodies[0]?.id ?? ""}
                label={t("cam.setup.modelBody", "Model body")}
                options={bodies.map((b) => ({
                  value: b.id,
                  label: b.label,
                }))}
                disabled={disabled || bodies.length <= 1}
              />
            </label>

            {/* Show stock checkbox */}
            <label className="flex items-center gap-2 text-xs cursor-pointer">
              <input
                type="checkbox"
                className="cad-checkbox"
                checked={showStock}
                disabled={disabled}
                onChange={(e) => onShowStockChange(e.target.checked)}
              />
              <span className="text-on-surface-muted">
                {t("cam.setup.showStock", "Show stock")}
              </span>
            </label>
          </fieldset>

          {/* ── Stock ────────────────────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.setup.stock", "Stock")}
            </legend>
            <div className="grid grid-cols-2 gap-2">
              <StockField
                label={t("cam.setup.stockWidth", "Width (mm)")}
                value={state.stock.width}
                disabled={disabled}
                onChange={(v) => updateStock({ width: v })}
              />
              <StockField
                label={t("cam.setup.stockHeight", "Height (mm)")}
                value={state.stock.height}
                disabled={disabled}
                onChange={(v) => updateStock({ height: v })}
              />
            </div>
            <StockField
              label={t("cam.setup.stockDepth", "Depth (mm)")}
              value={state.stock.depth}
              disabled={disabled}
              onChange={(v) => updateStock({ depth: v })}
            />
            <p className="text-[10px] leading-relaxed text-on-surface-dim">
              {t("cam.setup.offsetsNote", "Offsets add extra material beyond the part bounds on each axis.")}
            </p>
            <div className="grid grid-cols-3 gap-2">
              <StockField
                label={t("cam.setup.offsetX", "Offset X")}
                value={state.stock.offset_x}
                disabled={disabled}
                onChange={(v) => updateStock({ offset_x: v })}
              />
              <StockField
                label={t("cam.setup.offsetY", "Offset Y")}
                value={state.stock.offset_y}
                disabled={disabled}
                onChange={(v) => updateStock({ offset_y: v })}
              />
              <StockField
                label={t("cam.setup.offsetZ", "Offset Z")}
                value={state.stock.offset_z}
                disabled={disabled}
                onChange={(v) => updateStock({ offset_z: v })}
              />
            </div>
          </fieldset>

          {/* ── WCS Origin & Orientation ──────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.setup.wcs", "WCS Origin")}
            </legend>

            {/* Orientation mode dropdown */}
            <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.setup.orientation", "Orientation")}
              <Dropdown
                className="mt-2 w-full"
                value={state.orientation_mode}
                label={t("cam.setup.orientation", "Orientation")}
                options={[
                  { value: "model", label: t("cam.setup.orientModel", "Model orientation") },
                  { value: "z_x", label: t("cam.setup.orientZX", "Select Z axis/plane & X axis"), disabled: true },
                  { value: "z_y", label: t("cam.setup.orientZY", "Select Z axis/plane & Y axis"), disabled: true },
                  { value: "x_y", label: t("cam.setup.orientXY", "Select X & Y axes"), disabled: true },
                  { value: "cs", label: t("cam.setup.orientCS", "Select coordinate system"), disabled: true },
                ]}
                disabled={disabled}
                onChange={(value) =>
                  setState((prev) => ({ ...prev, orientation_mode: value }))
                }
              />
            </label>

            {/* Origin mode dropdown */}
            <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.setup.origin", "Origin")}
              <Dropdown
                className="mt-2 w-full"
                value={state.origin_mode}
                label={t("cam.setup.origin", "Origin")}
                options={[
                  { value: "model", label: t("cam.setup.originModel", "Model origin") },
                  { value: "point", label: t("cam.setup.originPoint", "Selected point"), disabled: true },
                  { value: "stock_box", label: t("cam.setup.originStockBox", "Stock box point"), disabled: true },
                  { value: "model_box", label: t("cam.setup.originModelBox", "Model box point"), disabled: true },
                ]}
                disabled={disabled}
                onChange={(value) =>
                  setState((prev) => ({ ...prev, origin_mode: value }))
                }
              />
            </label>

            {/* WCS origin position */}
            <div className="grid grid-cols-3 gap-2">
              <StockField
                label="X"
                value={state.wcs_origin.x}
                disabled={disabled}
                step={0.5}
                min={undefined}
                onChange={(v) => updateWcs({ x: v })}
              />
              <StockField
                label="Y"
                value={state.wcs_origin.y}
                disabled={disabled}
                step={0.5}
                min={undefined}
                onChange={(v) => updateWcs({ y: v })}
              />
              <StockField
                label="Z"
                value={state.wcs_origin.z}
                disabled={disabled}
                step={0.5}
                min={undefined}
                onChange={(v) => updateWcs({ z: v })}
              />
            </div>

            {/* XY rotation — only shown for Model orientation */}
            {state.orientation_mode === "model" ? (
              <>
                <StockField
                  label={t("cam.setup.wcsAngle", "XY rotation (°)")}
                  value={state.wcs_angle}
                  disabled={disabled}
                  step={1}
                  min={undefined}
                  onChange={(v) =>
                    setState((prev) => ({ ...prev, wcs_angle: v }))
                  }
                />
                <p className="text-[10px] leading-relaxed text-on-surface-dim">
                  {t("cam.setup.wcsAngleNote", "Rotates X and Y axes around Z to align with part fixturing.")}
                </p>
              </>
            ) : (
              <p className="text-[10px] leading-relaxed text-on-surface-dim">
                {t("cam.setup.orientComingSoon", "This orientation mode is planned for a future release.")}
              </p>
            )}
          </fieldset>

          {/* ── Safety Plane ─────────────────────────────────────── */}
          <fieldset className="space-y-3">
            <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
              {t("cam.setup.safety", "Safety Plane")}
            </legend>
            <StockField
              label={t("cam.setup.safetyPlaneZ", "Z height (mm)")}
              value={state.safety_plane_z}
              disabled={disabled}
              onChange={(v) =>
                setState((prev) => ({ ...prev, safety_plane_z: v }))
              }
            />
            <p className="text-[10px] leading-relaxed text-on-surface-dim">
              {t("cam.setup.safetyNote", "Z height for rapid moves between operations.")}
            </p>
          </fieldset>
        </ScrollArea>

        {/* ── Actions ────────────────────────────────────────────── */}
        <div className="mt-3 grid grid-cols-2 gap-2 pt-2">
          <button
            type="button"
            className="cad-action-ghost"
            disabled={disabled}
            onClick={onCancel}
          >
            {t("cam.setup.cancel", "Cancel")}
          </button>
          <button type="submit" className="cad-action-primary" disabled={disabled}>
            {t("cam.setup.confirm", "OK")}
          </button>
        </div>
      </form>
    </section>
  );
}
