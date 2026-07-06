import { useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";

import { Dropdown, ScrollArea } from "@/lib";
// Legacy CAM types (being rebuilt on cam_types.h schema).
interface CamSetupStock {
  width: number; height: number; depth: number;
  offset_x: number; offset_y: number; offset_z: number;
}
interface CamSetupOrigin { x: number; y: number; z: number; }
import {
  CamNumberField,
  useCamEscapeCancel,
  useDebouncedCamUpdate,
} from "./camPanelShared";

interface CamSetupFormState {
  stock: CamSetupStock;
  wcs_origin: CamSetupOrigin;
  safety_plane_z: number;
  wcs_angle: number;
  orientation_mode: string;
  origin_mode: string;
}

interface CamSetupPanelProps {
  initialSetup: CamSetupFormState;
  bodies: Array<{ id: string; label: string; center: { x: number; y: number; z: number }; size: { x: number; y: number; z: number } }>;
  showStock: boolean;
  onShowStockChange: (show: boolean) => void;
  wcsOrientation: string;
  onWcsOrientationChange: (mode: string) => void;
  disabled: boolean;
  onUpdate: (state: CamSetupFormState) => void;
  onConfirm: () => void;
  onCancel: () => void;
}

type TabId = "machine" | "stock" | "wcs";

export function CamSetupPanel({
  initialSetup,
  bodies,
  showStock,
  onShowStockChange,
  wcsOrientation,
  onWcsOrientationChange,
  disabled,
  onUpdate,
  onConfirm,
  onCancel,
}: CamSetupPanelProps) {
  const { t } = useTranslation();
  const [state, setState] = useState<CamSetupFormState>(initialSetup);
  const [tab, setTab] = useState<TabId>("machine");
  const confirmRef = useRef(onConfirm);
  confirmRef.current = onConfirm;
  const serialized = JSON.stringify(state);
  const markUpdateSent = useDebouncedCamUpdate(serialized, () => {
    onUpdate(state);
  });

  useCamEscapeCancel(onCancel);

  // Auto-size stock from model bounds when no setup exists yet.
  const autoSizedRef = useRef(false);
  useEffect(() => {
    if (autoSizedRef.current) return;
    if (bodies.length === 0) return;
    if (initialSetup.stock.width === 120 && initialSetup.stock.height === 120 && initialSetup.stock.depth === 20) {
      let minX = Infinity, minY = Infinity, minZ = Infinity;
      let maxX = -Infinity, maxY = -Infinity, maxZ = -Infinity;
      for (const body of bodies) {
        const hw = body.size.x / 2, hh = body.size.y / 2, hd = body.size.z / 2;
        minX = Math.min(minX, body.center.x - hw);
        maxX = Math.max(maxX, body.center.x + hw);
        minY = Math.min(minY, body.center.y - hh);
        maxY = Math.max(maxY, body.center.y + hh);
        minZ = Math.min(minZ, body.center.z - hd);
        maxZ = Math.max(maxZ, body.center.z + hd);
      }
      if (Number.isFinite(minX)) {
        setState((prev) => ({
          ...prev,
          stock: {
            ...prev.stock,
            width: Math.round((maxX - minX) * 10) / 10,
            height: Math.round((maxY - minY) * 10) / 10,
            depth: Math.round((maxZ - minZ) * 10) / 10,
          },
        }));
      }
    }
    autoSizedRef.current = true;
  }, [bodies, initialSetup.stock.width, initialSetup.stock.height, initialSetup.stock.depth]);

  function updateStock(patch: Partial<CamSetupFormState["stock"]>) {
    setState((prev) => ({ ...prev, stock: { ...prev.stock, ...patch } }));
  }

  function updateWcs(patch: Partial<CamSetupFormState["wcs_origin"]>) {
    setState((prev) => ({ ...prev, wcs_origin: { ...prev.wcs_origin, ...patch } }));
  }

  const tabBtn = (id: TabId, label: string) => (
    <button
      type="button"
      className={
        tab === id
          ? "flex-1 rounded px-2 py-1 text-xs font-semibold cad-panel-item-active"
          : "flex-1 rounded px-2 py-1 text-xs cad-panel-item"
      }
      onClick={() => setTab(id)}
    >
      {label}
    </button>
  );

  return (
    <section className="pointer-events-auto cad-floating-panel flex max-h-full min-h-0 w-[340px] max-w-full flex-col overflow-hidden px-5 py-5">
      <p className="cad-kicker">{t("cam.setup.title", "Setup")}</p>

      {/* ── Tabs ─────────────────────────────────────────────────── */}
      <div className="mt-3 flex gap-1">
        {tabBtn("machine", t("cam.setup.machine", "Machine"))}
        {tabBtn("stock", t("cam.setup.stock", "Stock"))}
        {tabBtn("wcs", t("cam.setup.wcs", "WCS"))}
      </div>

      <form
        className="mt-4 flex min-h-0 flex-1 flex-col overflow-hidden"
        onSubmit={(event) => {
          event.preventDefault();
          markUpdateSent();
          onUpdate(state);
          confirmRef.current();
        }}
      >
        <ScrollArea className="min-h-0 flex-1" viewportClassName="space-y-4 pr-4">
          {/* ════════════════════════════════════════════════════════
              TAB: Machine
              ════════════════════════════════════════════════════════ */}
          {tab === "machine" && (
            <>
              <CamNumberField
                label={t("cam.setup.axisCount", "Axis count")}
                value={3}
                disabled={true}
                onChange={() => {}}
              />
              <p className="-mt-2 text-[10px] leading-relaxed text-on-surface-dim">
                {t("cam.setup.axis3Note", "4-axis and 5-axis are planned for a future release.")}
              </p>

              <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                {t("cam.setup.modelBody", "Model body")}
                <Dropdown
                  className="mt-2 w-full"
                  value={bodies[0]?.id ?? ""}
                  label={t("cam.setup.modelBody", "Model body")}
                  options={bodies.map((b) => ({ value: b.id, label: b.label }))}
                  disabled={disabled || bodies.length <= 1}
                />
              </label>

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
            </>
          )}

          {/* ════════════════════════════════════════════════════════
              TAB: Stock
              ════════════════════════════════════════════════════════ */}
          {tab === "stock" && (
            <>
              <div className="grid grid-cols-2 gap-2">
                <CamNumberField
                  label={t("cam.setup.stockWidth", "Width (mm)")}
                  value={state.stock.width}
                  disabled={disabled}
                  onChange={(v) => updateStock({ width: v })}
                />
                <CamNumberField
                  label={t("cam.setup.stockHeight", "Height (mm)")}
                  value={state.stock.height}
                  disabled={disabled}
                  onChange={(v) => updateStock({ height: v })}
                />
              </div>
              <CamNumberField
                label={t("cam.setup.stockDepth", "Depth (mm)")}
                value={state.stock.depth}
                disabled={disabled}
                onChange={(v) => updateStock({ depth: v })}
              />
              <p className="-mt-2 text-[10px] leading-relaxed text-on-surface-dim">
                {t("cam.setup.offsetsNote", "Offsets add extra material beyond the part bounds on each axis.")}
              </p>
              <div className="grid grid-cols-3 gap-2">
                <CamNumberField
                  label={t("cam.setup.offsetX", "Offset X")}
                  value={state.stock.offset_x}
                  disabled={disabled}
                  onChange={(v) => updateStock({ offset_x: v })}
                />
                <CamNumberField
                  label={t("cam.setup.offsetY", "Offset Y")}
                  value={state.stock.offset_y}
                  disabled={disabled}
                  onChange={(v) => updateStock({ offset_y: v })}
                />
                <CamNumberField
                  label={t("cam.setup.offsetZ", "Offset Z")}
                  value={state.stock.offset_z}
                  disabled={disabled}
                  onChange={(v) => updateStock({ offset_z: v })}
                />
              </div>
            </>
          )}

          {/* ════════════════════════════════════════════════════════
              TAB: WCS
              ════════════════════════════════════════════════════════ */}
          {tab === "wcs" && (
            <>
              {/* Origin position */}
              <div className="grid grid-cols-3 gap-2">
                <CamNumberField label="X" value={state.wcs_origin.x} disabled={disabled} min={undefined} onChange={(v) => updateWcs({ x: v })} />
                <CamNumberField label="Y" value={state.wcs_origin.y} disabled={disabled} min={undefined} onChange={(v) => updateWcs({ y: v })} />
                <CamNumberField label="Z" value={state.wcs_origin.z} disabled={disabled} min={undefined} onChange={(v) => updateWcs({ z: v })} />
              </div>

              {/* Origin mode */}
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
                  onChange={(value) => setState((prev) => ({ ...prev, origin_mode: value }))}
                />
              </label>

              {/* Orientation mode */}
              <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                {t("cam.setup.orientation", "Orientation")}
                <Dropdown
                  className="mt-2 w-full"
                  value={wcsOrientation}
                  label={t("cam.setup.orientation", "Orientation")}
                  options={[
                    { value: "model", label: t("cam.setup.orientModel", "Model orientation") },
                    { value: "z_up", label: t("cam.setup.orientZUp", "Z axis up") },
                    { value: "y_up", label: t("cam.setup.orientYUp", "Y axis up") },
                    { value: "z_x", label: t("cam.setup.orientZX", "Select Z axis/plane & X axis"), disabled: true },
                    { value: "z_y", label: t("cam.setup.orientZY", "Select Z axis/plane & Y axis"), disabled: true },
                    { value: "x_y", label: t("cam.setup.orientXY", "Select X & Y axes"), disabled: true },
                    { value: "cs", label: t("cam.setup.orientCS", "Select coordinate system"), disabled: true },
                  ]}
                  disabled={disabled}
                  onChange={(value) => onWcsOrientationChange(value)}
                />
              </label>

              <p className="-mt-2 text-[10px] leading-relaxed text-on-surface-dim">
                {t("cam.setup.orientModelNote", "WCS axes follow the model coordinate system.")}
              </p>

              {/* Safety plane */}
              <CamNumberField
                label={t("cam.setup.safetyPlaneZ", "Safety Z (mm)")}
                value={state.safety_plane_z}
                disabled={disabled}
                onChange={(v) => setState((prev) => ({ ...prev, safety_plane_z: v }))}
              />
              <p className="-mt-2 text-[10px] leading-relaxed text-on-surface-dim">
                {t("cam.setup.safetyNote", "Z height for rapid moves between operations.")}
              </p>
            </>
          )}
        </ScrollArea>

        {/* ── Actions ────────────────────────────────────────────── */}
        <div className="mt-3 grid grid-cols-2 gap-2 pt-2">
          <button type="button" className="cad-action-ghost" disabled={disabled} onClick={onCancel}>
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
