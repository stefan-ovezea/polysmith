import { useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";

import { Dropdown, ScrollArea } from "@/lib";
import type {
  CamSetup,
  LaserMachineSettings,
  MachineType,
  StockDefinition,
  StockType,
} from "@/types";
import {
  CamNumberField,
  useCamEscapeCancel,
  useDebouncedCamUpdate,
} from "./camPanelShared";

// Defaults mirror the C++ struct defaults in cam_types.h.  Used both to
// seed the setup panel before a setup exists and to build the
// cam_setup_create payload.
export function createDefaultCamSetup(): CamSetup {
  return {
    setup_id: "",
    name: "Setup",
    machine_type: "3_axis_mill",
    machine_axes: { x: 500, y: 400, z: 300 },
    stock: {
      type: "bounding_box",
      origin: [0, 0, 0],
      size: [120, 120, 20],
      margin: 3,
    },
    wcs_origin: {
      feature_id: "",
      face_reference: {
        persistent_id: "",
        attestation: { bounds: { min_x: 0, min_y: 0, min_z: 0, max_x: 0, max_y: 0, max_z: 0 }, area: 0, normal: [0, 0, 1], sample_points: [] },
      },
      position: [0, 0, 0],
    },
    safety_height: 50,
    retract_height: 5,
    units: "mm",
  };
}

const MACHINE_TYPES: MachineType[] = [
  "3_axis_mill",
  "4_axis_mill",
  "5_axis_mill",
  "lathe_2_axis",
  "lathe_live_tooling",
  "laser",
  "plasma",
  "printer",
];

const STOCK_TYPES: StockType[] = [
  "bounding_box",
  "cylinder",
  "from_solid",
  "from_mesh",
];

interface CamSetupFormState {
  stockType: StockType;
  origin: [number, number, number];
  wcsOrigin: [number, number, number];
  size: [number, number, number];
  diameter: number;
  length: number;
  margin: number;
  machineType: MachineType;
  safetyHeight: number;
  retractHeight: number;
  units: "mm" | "inch";
}

interface CamSetupPanelProps {
  initialSetup: CamSetup;
  bodies: Array<{ id: string; label: string; center: { x: number; y: number; z: number }; size: { x: number; y: number; z: number } }>;
  showStock: boolean;
  onShowStockChange: (show: boolean) => void;
  wcsOrientation: string;
  onWcsOrientationChange: (mode: string) => void;
  postProcessorType: string;
  onPostProcessorChange: (postType: string) => void;
  posts: Array<{ name: string; path: string }>;
  onImportPost: () => void;
  onEditPost: (path: string) => void;
  onPickOrigin: () => void;
  pickedOrigin: [number, number, number] | null;
  originPickArmed: boolean;
  machineSettings: LaserMachineSettings | null;
  onMachineSettingsChange: (settings: LaserMachineSettings) => void;
  wcsPickArmed: boolean;
  onPickWcsFace: () => void;
  disabled: boolean;
  onUpdate: (setup: CamSetup) => void;
  onConfirm: () => void;
  onCancel: () => void;
}

type TabId = "machine" | "stock" | "wcs";

function formStateFromSetup(setup: CamSetup): CamSetupFormState {
  return {
    stockType: setup.stock.type ?? "bounding_box",
    origin: setup.stock.origin ?? [0, 0, 0],
    wcsOrigin: setup.wcs_origin.position ?? [0, 0, 0],
    size: setup.stock.size ?? [120, 120, 20],
    diameter: setup.stock.diameter ?? 40,
    length: setup.stock.length ?? 20,
    margin: setup.stock.margin ?? 3,
    machineType: setup.machine_type ?? "3_axis_mill",
    safetyHeight: setup.safety_height ?? 50,
    retractHeight: setup.retract_height ?? 5,
    units: setup.units === "inch" ? "inch" : "mm",
  };
}

function setupFromFormState(
  initial: CamSetup,
  state: CamSetupFormState,
): CamSetup {
  const stock: StockDefinition = {
    ...initial.stock,
    type: state.stockType,
    origin: state.origin,
    margin: state.margin,
    size: state.stockType === "cylinder" ? undefined : state.size,
    diameter: state.stockType === "cylinder" ? state.diameter : undefined,
    length: state.stockType === "cylinder" ? state.length : undefined,
  };
  return {
    ...initial,
    name: initial.name || "Setup",
    machine_type: state.machineType,
    stock,
    wcs_origin: { ...initial.wcs_origin, position: state.wcsOrigin },
    safety_height: state.safetyHeight,
    retract_height: state.retractHeight,
    units: state.units,
  };
}

export function CamSetupPanel({
  initialSetup,
  bodies,
  showStock,
  onShowStockChange,
  wcsOrientation,
  onWcsOrientationChange,
  postProcessorType,
  onPostProcessorChange,
  posts,
  onImportPost,
  onEditPost,
  onPickOrigin,
  pickedOrigin,
  originPickArmed,
  machineSettings,
  onMachineSettingsChange,
  wcsPickArmed,
  onPickWcsFace,
  disabled,
  onUpdate,
  onConfirm,
  onCancel,
}: CamSetupPanelProps) {
  const { t } = useTranslation();
  const [state, setState] = useState<CamSetupFormState>(() =>
    formStateFromSetup(initialSetup),
  );
  const [tab, setTab] = useState<TabId>("machine");
  const confirmRef = useRef(onConfirm);
  confirmRef.current = onConfirm;
  const serialized = JSON.stringify(state);
  const markUpdateSent = useDebouncedCamUpdate(serialized, () => {
    onUpdate(setupFromFormState(initialSetup, state));
  });

  useCamEscapeCancel(onCancel);

  // Auto-size stock from model bounds when the setup has no explicit
  // stock size yet (fresh setup / bounding_box with default size).
  const autoSizedRef = useRef(false);
  useEffect(() => {
    if (autoSizedRef.current) return;
    if (bodies.length === 0) return;
    const hasExplicitSize =
      initialSetup.stock.size !== undefined &&
      (initialSetup.stock.size[0] !== 120 ||
        initialSetup.stock.size[1] !== 120 ||
        initialSetup.stock.size[2] !== 20);
    if (hasExplicitSize) return;
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
      const size: [number, number, number] = [
        Math.round((maxX - minX) * 10) / 10,
        Math.round((maxY - minY) * 10) / 10,
        Math.round((maxZ - minZ) * 10) / 10,
      ];
      setState((prev) => ({
        ...prev,
        size,
        origin: [
          Math.round((minX + maxX) / 2 * 10) / 10,
          Math.round((minY + maxY) / 2 * 10) / 10,
          Math.round((minZ + maxZ) / 2 * 10) / 10,
        ],
      }));
    }
    autoSizedRef.current = true;
  }, [bodies, initialSetup.stock.size]);

  // A graphical origin pick (viewport click) updates the document
  // directly through the parent — mirror it into the form fields.
  const lastPickedOriginRef = useRef<string | null>(null);
  useEffect(() => {
    if (!pickedOrigin) {
      return;
    }
    const key = pickedOrigin.join(",");
    if (key === lastPickedOriginRef.current) {
      return;
    }
    lastPickedOriginRef.current = key;
    setState((prev) => ({
      ...prev,
      origin: [pickedOrigin[0], pickedOrigin[1], pickedOrigin[2]],
    }));
  }, [pickedOrigin]);

  function update(patch: Partial<CamSetupFormState>) {
    setState((prev) => ({ ...prev, ...patch }));
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

  const machineTypeOptions = MACHINE_TYPES.map((type) => ({
    value: type,
    label: t(`cam.setup.machineType.${type}`),
  }));
  const stockTypeOptions = STOCK_TYPES.map((type) => ({
    value: type,
    label: t(`cam.setup.stockType.${type}`),
  }));

  // Laser/plasma machines cut from a sheet: the WCS is the sheet origin
  // and there is no model-body reference to pick.
  const isSheetMachine =
    state.machineType === "laser" || state.machineType === "plasma";

  return (
    <section className="pointer-events-auto cad-floating-panel flex max-h-full min-h-0 w-[340px] max-w-full flex-col overflow-hidden px-5 py-5">
      <p className="cad-kicker">{t("cam.setup.title", "Setup")}</p>

      {/* ── Tabs ─────────────────────────────────────────────────── */}
      <div className="mt-3 flex gap-1">
        {tabBtn("machine", t("cam.setup.machine", "Machine"))}
        {tabBtn("stock", t("cam.setup.stock", "Stock"))}
        {!isSheetMachine && tabBtn("wcs", t("cam.setup.wcs", "WCS"))}
      </div>

      <form
        className="mt-4 flex min-h-0 flex-1 flex-col overflow-hidden"
        onSubmit={(event) => {
          event.preventDefault();
          markUpdateSent();
          onUpdate(setupFromFormState(initialSetup, state));
          confirmRef.current();
        }}
      >
        <ScrollArea className="min-h-0 flex-1" viewportClassName="space-y-4 pr-4">
          {/* ════════════════════════════════════════════════════════
              TAB: Machine
              ════════════════════════════════════════════════════════ */}
          {(tab === "machine" || (tab === "wcs" && isSheetMachine)) && (
            <>
              <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                {t("cam.setup.machineTypeLabel", "Machine type")}
                <Dropdown
                  className="mt-2 w-full"
                  value={state.machineType}
                  label={t("cam.setup.machineTypeLabel", "Machine type")}
                  options={machineTypeOptions}
                  disabled={disabled}
                  onChange={(value) => update({ machineType: value as MachineType })}
                />
              </label>

              {isSheetMachine ? (
                <fieldset className="space-y-3">
                  <legend className="text-xs font-semibold uppercase tracking-[0.18em] text-on-surface-muted">
                    {t("cam.machine.title", "Machine Settings")}
                  </legend>
                  <div className="grid grid-cols-2 gap-2">
                    <CamNumberField
                      label={t("cam.machine.workAreaX", "Work area X (mm)")}
                      value={machineSettings?.work_area_x_mm ?? 400}
                      disabled={disabled}
                      step={10}
                      onChange={(v) =>
                        onMachineSettingsChange({
                          work_area_x_mm: v,
                          work_area_y_mm: machineSettings?.work_area_y_mm ?? 400,
                          pointer_offset_x_mm:
                            machineSettings?.pointer_offset_x_mm ?? 0,
                          pointer_offset_y_mm:
                            machineSettings?.pointer_offset_y_mm ?? 0,
                        })
                      }
                    />
                    <CamNumberField
                      label={t("cam.machine.workAreaY", "Work area Y (mm)")}
                      value={machineSettings?.work_area_y_mm ?? 400}
                      disabled={disabled}
                      step={10}
                      onChange={(v) =>
                        onMachineSettingsChange({
                          work_area_x_mm: machineSettings?.work_area_x_mm ?? 400,
                          work_area_y_mm: v,
                          pointer_offset_x_mm:
                            machineSettings?.pointer_offset_x_mm ?? 0,
                          pointer_offset_y_mm:
                            machineSettings?.pointer_offset_y_mm ?? 0,
                        })
                      }
                    />
                    <CamNumberField
                      label={t("cam.machine.pointerOffsetX", "Red pointer offset X (mm)")}
                      value={machineSettings?.pointer_offset_x_mm ?? 0}
                      disabled={disabled}
                      step={0.1}
                      min={undefined}
                      onChange={(v) =>
                        onMachineSettingsChange({
                          work_area_x_mm: machineSettings?.work_area_x_mm ?? 400,
                          work_area_y_mm: machineSettings?.work_area_y_mm ?? 400,
                          pointer_offset_x_mm: v,
                          pointer_offset_y_mm:
                            machineSettings?.pointer_offset_y_mm ?? 0,
                        })
                      }
                    />
                    <CamNumberField
                      label={t("cam.machine.pointerOffsetY", "Red pointer offset Y (mm)")}
                      value={machineSettings?.pointer_offset_y_mm ?? 0}
                      disabled={disabled}
                      step={0.1}
                      min={undefined}
                      onChange={(v) =>
                        onMachineSettingsChange({
                          work_area_x_mm: machineSettings?.work_area_x_mm ?? 400,
                          work_area_y_mm: machineSettings?.work_area_y_mm ?? 400,
                          pointer_offset_x_mm:
                            machineSettings?.pointer_offset_x_mm ?? 0,
                          pointer_offset_y_mm: v,
                        })
                      }
                    />
                  </div>
                  <p className="text-[10px] leading-relaxed text-on-surface-dim">
                    {t("cam.machine.pointerNote")}
                  </p>
                </fieldset>
              ) : null}

              <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                {t("cam.setup.unitsLabel", "Units")}
                <Dropdown
                  className="mt-2 w-full"
                  value={state.units}
                  label={t("cam.setup.unitsLabel", "Units")}
                  options={[
                    { value: "mm", label: t("cam.setup.unitsMm", "Millimeters (mm)") },
                    { value: "inch", label: t("cam.setup.unitsInch", "Inches") },
                  ]}
                  disabled={disabled}
                  onChange={(value) => update({ units: value as "mm" | "inch" })}
                />
              </label>

              <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                {t("cam.setup.postProcessor", "Post processor")}
                <Dropdown
                  className="mt-2 w-full"
                  value={postProcessorType}
                  label={t("cam.setup.postProcessor", "Post processor")}
                  options={
                    posts.length > 0
                      ? posts.map((post) => ({
                          value: post.name,
                          label: post.name,
                        }))
                      : [
                          { value: postProcessorType, label: postProcessorType },
                        ]
                  }
                  disabled={disabled}
                  onChange={(value) => onPostProcessorChange(value)}
                />
                <span className="mt-2 flex gap-2">
                  <button
                    type="button"
                    className="cad-action-ghost flex-1 px-2 py-1 text-[10px] uppercase tracking-wider"
                    disabled={disabled}
                    onClick={onImportPost}
                  >
                    {t("cam.setup.importPost", "Import…")}
                  </button>
                  <button
                    type="button"
                    className="cad-action-ghost flex-1 px-2 py-1 text-[10px] uppercase tracking-wider"
                    disabled={
                      disabled ||
                      !posts.some(
                        (post) =>
                          post.name === postProcessorType && post.path !== "",
                      )
                    }
                    onClick={() => {
                      const post = posts.find(
                        (entry) => entry.name === postProcessorType,
                      );
                      if (post?.path) {
                        onEditPost(post.path);
                      }
                    }}
                  >
                    {t("cam.setup.editPost", "Edit")}
                  </button>
                </span>
              </label>

              {!isSheetMachine && (
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
              )}

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
              <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                {t("cam.setup.stockTypeLabel", "Stock type")}
                <Dropdown
                  className="mt-2 w-full"
                  value={state.stockType}
                  label={t("cam.setup.stockTypeLabel", "Stock type")}
                  options={stockTypeOptions}
                  disabled={disabled}
                  onChange={(value) => update({ stockType: value as StockType })}
                />
              </label>

              {state.stockType === "bounding_box" ? (
                <>
                  <div className="grid grid-cols-3 gap-2">
                    <CamNumberField
                      label={t("cam.setup.sizeX", "Size X (mm)")}
                      value={state.size[0]}
                      disabled={disabled}
                      onChange={(v) => update({ size: [v, state.size[1], state.size[2]] })}
                    />
                    <CamNumberField
                      label={t("cam.setup.sizeY", "Size Y (mm)")}
                      value={state.size[1]}
                      disabled={disabled}
                      onChange={(v) => update({ size: [state.size[0], v, state.size[2]] })}
                    />
                    <CamNumberField
                      label={t("cam.setup.sizeZ", "Size Z (mm)")}
                      value={state.size[2]}
                      disabled={disabled}
                      onChange={(v) => update({ size: [state.size[0], state.size[1], v] })}
                    />
                  </div>
                  <p className="-mt-2 text-[10px] leading-relaxed text-on-surface-dim">
                    {t("cam.setup.sizeNote", "The stock box is centered on the part origin.")}
                  </p>
                </>
              ) : null}

              {state.stockType === "cylinder" ? (
                <div className="grid grid-cols-2 gap-2">
                  <CamNumberField
                    label={t("cam.setup.diameter", "Diameter (mm)")}
                    value={state.diameter}
                    disabled={disabled}
                    onChange={(v) => update({ diameter: v })}
                  />
                  <CamNumberField
                    label={t("cam.setup.length", "Length (mm)")}
                    value={state.length}
                    disabled={disabled}
                    onChange={(v) => update({ length: v })}
                  />
                </div>
              ) : null}

              <div className="flex items-end gap-2">
                <span className="flex-1 text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                  {t("cam.setup.originLabel", "Origin")}
                </span>
                <button
                  type="button"
                  className={
                    originPickArmed
                      ? "cad-action-primary px-2 py-1 text-[10px] uppercase tracking-wider"
                      : "cad-action-ghost px-2 py-1 text-[10px] uppercase tracking-wider"
                  }
                  disabled={disabled}
                  onClick={onPickOrigin}
                >
                  {originPickArmed
                    ? t("cam.setup.pickOriginArmed", "Click the part…")
                    : t("cam.setup.pickOrigin", "Pick origin…")}
                </button>
              </div>
              <div className="grid grid-cols-3 gap-2">
                <CamNumberField
                  label={t("cam.setup.originX", "Origin X")}
                  value={state.origin[0]}
                  disabled={disabled}
                  min={undefined}
                  step="any"
                  onChange={(v) => update({ origin: [v, state.origin[1], state.origin[2]] })}
                />
                <CamNumberField
                  label={t("cam.setup.originY", "Origin Y")}
                  value={state.origin[1]}
                  disabled={disabled}
                  min={undefined}
                  step="any"
                  onChange={(v) => update({ origin: [state.origin[0], v, state.origin[2]] })}
                />
                <CamNumberField
                  label={t("cam.setup.originZ", "Origin Z")}
                  value={state.origin[2]}
                  disabled={disabled}
                  min={undefined}
                  step="any"
                  onChange={(v) => update({ origin: [state.origin[0], state.origin[1], v] })}
                />
              </div>

              <CamNumberField
                label={t("cam.setup.margin", "Margin (mm)")}
                value={state.margin}
                disabled={disabled}
                step={0.5}
                onChange={(v) => update({ margin: v })}
              />
              <p className="-mt-2 text-[10px] leading-relaxed text-on-surface-dim">
                {t("cam.setup.marginNote", "Extra material beyond the part bounds on each axis.")}
              </p>
            </>
          )}

          {/* ════════════════════════════════════════════════════════
              TAB: WCS
              ════════════════════════════════════════════════════════ */}
          {tab === "wcs" && (
            <>
              {/* Face-anchored WCS: pick a face, the core captures the
                  TNP-safe witness and resolves the origin from it. */}
              <div className="flex items-end gap-2">
                <span className="flex-1 text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                  {t("cam.setup.wcsFaceAnchor", "Face anchor")}
                </span>
                <button
                  type="button"
                  className={
                    wcsPickArmed
                      ? "cad-action-primary px-2 py-1 text-[10px] uppercase tracking-wider"
                      : "cad-action-ghost px-2 py-1 text-[10px] uppercase tracking-wider"
                  }
                  disabled={disabled}
                  onClick={onPickWcsFace}
                >
                  {wcsPickArmed
                    ? t("cam.setup.pickWcsFaceArmed", "Click a face…")
                    : t("cam.setup.pickWcsFace", "Pick WCS face…")}
                </button>
              </div>

              {/* Origin position */}
              <div className="grid grid-cols-3 gap-2">
                <CamNumberField label="X" value={state.wcsOrigin[0]} disabled={disabled} min={undefined} step="any" onChange={(v) => update({ wcsOrigin: [v, state.wcsOrigin[1], state.wcsOrigin[2]] })} />
                <CamNumberField label="Y" value={state.wcsOrigin[1]} disabled={disabled} min={undefined} step="any" onChange={(v) => update({ wcsOrigin: [state.wcsOrigin[0], v, state.wcsOrigin[2]] })} />
                <CamNumberField label="Z" value={state.wcsOrigin[2]} disabled={disabled} min={undefined} step="any" onChange={(v) => update({ wcsOrigin: [state.wcsOrigin[0], state.wcsOrigin[1], v] })} />
              </div>

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

              {/* Safety / retract plane */}
              <CamNumberField
                label={t("cam.setup.safetyHeight", "Safety height (mm)")}
                value={state.safetyHeight}
                disabled={disabled}
                onChange={(v) => update({ safetyHeight: v })}
              />
              <p className="-mt-2 text-[10px] leading-relaxed text-on-surface-dim">
                {t("cam.setup.safetyNote", "Z height for rapid moves between operations.")}
              </p>
              <CamNumberField
                label={t("cam.setup.retractHeight", "Retract height (mm)")}
                value={state.retractHeight}
                disabled={disabled}
                onChange={(v) => update({ retractHeight: v })}
              />
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
