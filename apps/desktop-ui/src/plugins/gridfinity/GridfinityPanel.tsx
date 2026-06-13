import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";

import { sendCoreCommand } from "@/lib/cadCoreClient";
import { makeGetViewportStateCommand } from "@/lib/ipcProtocol";
import { makeDeleteFeatureCommand } from "@/lib/ipc/bodyFeatureCommands";
import type { GridfinityFeatureParameters, GridfinityModelKind } from "./types";
import {
  makeConfirmGridfinityFeatureCommand,
  makeUpdateGridfinityFeatureCommand,
  normalizeGridfinityFeatureParameters,
} from "./commands";

interface GridfinityPanelProps {
  disabled: boolean;
  featureId: string;
  initialParameters: GridfinityFeatureParameters;
  onClose: () => void;
}

interface NumberFieldProps {
  disabled?: boolean;
  integer?: boolean;
  label: string;
  max: number;
  min: number;
  step?: number;
  value: number;
  onCommit: (value: number) => void;
}

function clampInteger(value: number, min: number, max: number) {
  if (!Number.isFinite(value)) {
    return min;
  }
  return Math.min(Math.max(Math.round(value), min), max);
}

function clampNumber(value: number, min: number, max: number) {
  if (!Number.isFinite(value)) {
    return min;
  }
  return Math.min(Math.max(value, min), max);
}

function numericValue(value: string, fallback: number) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : fallback;
}

function NumberField({
  disabled = false,
  integer = true,
  label,
  max,
  min,
  step = 1,
  value,
  onCommit,
}: NumberFieldProps) {
  const [draft, setDraft] = useState(String(value));

  useEffect(() => {
    setDraft(String(value));
  }, [value]);

  function commit() {
    const next = integer
      ? clampInteger(numericValue(draft, value), min, max)
      : clampNumber(numericValue(draft, value), min, max);
    setDraft(String(next));
    if (next !== value) {
      onCommit(next);
    }
  }

  return (
    <label className="block">
      <span className="cad-kicker">{label}</span>
      <input
        className="cad-input mt-1"
        type="number"
        min={min}
        max={max}
        step={step}
        value={draft}
        disabled={disabled}
        onBlur={commit}
        onChange={(event) => setDraft(event.target.value)}
        onFocus={(event) => event.currentTarget.select()}
        onKeyDown={(event) => {
          if (event.key === "Enter") {
            event.preventDefault();
            commit();
            event.currentTarget.blur();
          } else if (event.key === "Escape") {
            event.preventDefault();
            setDraft(String(value));
            event.currentTarget.blur();
          }
        }}
      />
    </label>
  );
}

export function GridfinityPanel({
  disabled,
  featureId,
  initialParameters,
  onClose,
}: GridfinityPanelProps) {
  const { t } = useTranslation();
  const [parameters, setParameters] =
    useState<GridfinityFeatureParameters>(initialParameters);
  const [isBusy, setIsBusy] = useState(false);

  function modelLabel(modelKind: GridfinityModelKind) {
    if (modelKind === "baseplate") {
      return t("plugins.gridfinity.baseplate");
    }
    if (modelKind === "solid_bin") {
      return t("plugins.gridfinity.solidBin");
    }
    if (modelKind === "holey_bin") {
      return t("plugins.gridfinity.holeyBin");
    }
    if (modelKind === "light_bin") {
      return t("plugins.gridfinity.lightBin");
    }
    return t("plugins.gridfinity.bin");
  }

  useEffect(() => {
    setParameters(initialParameters);
  }, [initialParameters]);

  async function update(next: GridfinityFeatureParameters) {
    const normalized = normalizeGridfinityFeatureParameters(next);
    setParameters(normalized);
    setIsBusy(true);
    try {
      await sendCoreCommand(
        makeUpdateGridfinityFeatureCommand(featureId, normalized),
      );
      await sendCoreCommand(makeGetViewportStateCommand());
    } finally {
      setIsBusy(false);
    }
  }

  const dimensionFields: Array<{
    key: "gridX" | "gridY" | "gridZ";
    label: string;
    min: number;
    max: number;
  }> = [
    { key: "gridX", label: t("plugins.gridfinity.gridX"), min: 1, max: 6 },
    { key: "gridY", label: t("plugins.gridfinity.gridY"), min: 1, max: 6 },
    { key: "gridZ", label: t("plugins.gridfinity.gridZ"), min: 2, max: 12 },
  ];

  return (
    <section className="pointer-events-auto cad-floating-panel cad-scrollbar box-border flex max-h-full min-h-0 w-full flex-col overflow-y-auto px-5 py-5">
      <div>
        <p className="cad-kicker">{t("plugins.gridfinity.title")}</p>
        <h2 className="mt-1 font-display text-lg text-on-surface">
          {modelLabel(parameters.modelKind)}
        </h2>
      </div>

      <fieldset
        disabled={disabled || isBusy}
        className="m-0 mt-4 space-y-3 border-0 p-0"
      >
        <div className="grid grid-cols-2 gap-2">
          {(["bin", "light_bin", "solid_bin", "holey_bin", "baseplate"] as const).map((kind) => (
            <button
              key={kind}
              type="button"
              className={
                parameters.modelKind === kind
                  ? "cad-ribbon-action cad-ribbon-action-primary justify-center"
                  : "cad-ribbon-action justify-center"
              }
              onClick={() => void update({ ...parameters, modelKind: kind })}
            >
              {modelLabel(kind)}
            </button>
          ))}
        </div>

        <div className="grid grid-cols-3 gap-2">
          {dimensionFields.map(({ key, label, min, max }) => (
            <NumberField
              key={key}
              label={label}
              min={
                key === "gridZ" &&
                (parameters.modelKind === "solid_bin" ||
                  parameters.modelKind === "light_bin")
                  ? 1
                  : min
              }
              max={max}
              value={parameters[key as "gridX" | "gridY" | "gridZ"]}
              disabled={
                parameters.modelKind === "holey_bin" ||
                (key === "gridZ" && parameters.modelKind === "baseplate")
              }
              onCommit={(value) => {
                void update({ ...parameters, [key]: value });
              }}
            />
          ))}
        </div>

        {parameters.modelKind !== "baseplate" ? (
          <label className="flex items-center justify-between gap-3 text-sm text-on-surface">
            <span>{t("plugins.gridfinity.stackingLip")}</span>
            <input
              type="checkbox"
              checked={parameters.stackingLip}
              onChange={(event) =>
                void update({ ...parameters, stackingLip: event.target.checked })
              }
            />
          </label>
        ) : null}

        {parameters.modelKind === "holey_bin" ? (
          <>
            <div className="grid grid-cols-2 gap-2">
              <NumberField
                label={t("plugins.gridfinity.holeyHolesX")}
                min={1}
                max={24}
                value={parameters.holeyHolesX}
                onCommit={(value) =>
                  void update({ ...parameters, holeyHolesX: value })
                }
              />
              <NumberField
                label={t("plugins.gridfinity.holeyHolesY")}
                min={1}
                max={24}
                value={parameters.holeyHolesY}
                onCommit={(value) =>
                  void update({ ...parameters, holeyHolesY: value })
                }
              />
            </div>
            <label className="block">
              <span className="cad-kicker">
                {t("plugins.gridfinity.holeyHoleShape")}
              </span>
              <select
                className="cad-input mt-1"
                value={parameters.holeyHoleShape}
                onChange={(event) =>
                  void update({
                    ...parameters,
                    holeyHoleShape: event.target.value as typeof parameters.holeyHoleShape,
                  })
                }
              >
                <option value="circle">
                  {t("plugins.gridfinity.holeyCircle")}
                </option>
                <option value="square">
                  {t("plugins.gridfinity.holeySquare")}
                </option>
                <option value="hexagon">
                  {t("plugins.gridfinity.holeyHexagon")}
                </option>
              </select>
            </label>
            <div className="grid grid-cols-1 gap-2">
              <NumberField
                label={t("plugins.gridfinity.holeyHoleSize")}
                min={0.5}
                max={parameters.holeyKeepoutDiameter}
                step={0.1}
                integer={false}
                value={parameters.holeyHoleSize}
                onCommit={(value) =>
                  void update({ ...parameters, holeyHoleSize: value })
                }
              />
              <NumberField
                label={t("plugins.gridfinity.holeyHoleDepth")}
                min={0.5}
                max={70}
                step={0.1}
                integer={false}
                value={parameters.holeyHoleDepth}
                onCommit={(value) =>
                  void update({ ...parameters, holeyHoleDepth: value })
                }
              />
              <NumberField
                label={t("plugins.gridfinity.holeyKeepout")}
                min={1}
                max={60}
                step={0.1}
                integer={false}
                value={parameters.holeyKeepoutDiameter}
                onCommit={(value) =>
                  void update({ ...parameters, holeyKeepoutDiameter: value })
                }
              />
            </div>
          </>
        ) : null}

        {parameters.modelKind === "bin" || parameters.modelKind === "light_bin" ? (
          <>
            <div className="grid grid-cols-2 gap-2">
              <NumberField
                label={t("plugins.gridfinity.compartmentsX")}
                min={1}
                max={parameters.gridX * 4}
                value={parameters.compartmentsX}
                onCommit={(value) =>
                  void update({ ...parameters, compartmentsX: value })
                }
              />
              <NumberField
                label={t("plugins.gridfinity.compartmentsY")}
                min={1}
                max={parameters.gridY * 4}
                value={parameters.compartmentsY}
                onCommit={(value) =>
                  void update({ ...parameters, compartmentsY: value })
                }
              />
            </div>
            {parameters.modelKind === "light_bin" ? (
              <div className="grid grid-cols-2 gap-2">
                <NumberField
                  label={t("plugins.gridfinity.lightWallThickness")}
                  min={0.1}
                  max={8}
                  step={0.1}
                  integer={false}
                  value={parameters.lightWallThickness}
                  onCommit={(value) =>
                    void update({ ...parameters, lightWallThickness: value })
                  }
                />
                <NumberField
                  label={t("plugins.gridfinity.labelRidgeWidth")}
                  min={1}
                  max={30}
                  step={0.1}
                  integer={false}
                  value={parameters.labelRidgeWidth}
                  onCommit={(value) =>
                    void update({ ...parameters, labelRidgeWidth: value })
                  }
                />
              </div>
            ) : null}
            {parameters.modelKind === "bin" ? (
              <div className="grid grid-cols-2 gap-2">
                <NumberField
                  label={t("plugins.gridfinity.dividerThickness")}
                  min={0.1}
                  max={8}
                  step={0.1}
                  integer={false}
                  value={parameters.dividerThickness}
                  onCommit={(value) =>
                    void update({ ...parameters, dividerThickness: value })
                  }
                />
                <NumberField
                  label={t("plugins.gridfinity.labelRidgeWidth")}
                  min={1}
                  max={30}
                  step={0.1}
                  integer={false}
                  value={parameters.labelRidgeWidth}
                  onCommit={(value) =>
                    void update({ ...parameters, labelRidgeWidth: value })
                  }
                />
              </div>
            ) : null}
            <label className="flex items-center justify-between gap-3 text-sm text-on-surface">
              <span>{t("plugins.gridfinity.labelTab")}</span>
              <input
                type="checkbox"
                checked={parameters.labelTab}
                onChange={(event) =>
                  void update({ ...parameters, labelTab: event.target.checked })
                }
              />
            </label>
            <label className="flex items-center justify-between gap-3 text-sm text-on-surface">
              <span>{t("plugins.gridfinity.multiLabel")}</span>
              <input
                type="checkbox"
                checked={parameters.multiLabel}
                onChange={(event) =>
                  void update({ ...parameters, multiLabel: event.target.checked })
                }
              />
            </label>
            {parameters.modelKind === "bin" ? (
              <label className="flex items-center justify-between gap-3 text-sm text-on-surface">
                <span>{t("plugins.gridfinity.grabCurve")}</span>
                <input
                  type="checkbox"
                  checked={parameters.grabCurve}
                  onChange={(event) =>
                    void update({ ...parameters, grabCurve: event.target.checked })
                  }
                />
              </label>
            ) : null}
          </>
        ) : null}

        <NumberField
          label={t("plugins.gridfinity.magnetHoleDiameter")}
          min={1}
          max={20}
          step={0.1}
          integer={false}
          value={parameters.magnetHoleDiameter}
          onCommit={(value) =>
            void update({ ...parameters, magnetHoleDiameter: value })
          }
        />
        <label className="flex items-center justify-between gap-3 text-sm text-on-surface">
          <span>{t("plugins.gridfinity.magnetHoles")}</span>
          <input
            type="checkbox"
            checked={parameters.magnetHoles}
            onChange={(event) =>
              void update({ ...parameters, magnetHoles: event.target.checked })
            }
          />
        </label>
        <label className="flex items-center justify-between gap-3 text-sm text-on-surface">
          <span>{t("plugins.gridfinity.magnetRemovalHoles")}</span>
          <input
            type="checkbox"
            checked={parameters.magnetRemovalHoles}
            onChange={(event) =>
              void update({
                ...parameters,
                magnetRemovalHoles: event.target.checked,
              })
            }
          />
        </label>
        <label className="flex items-center justify-between gap-3 text-sm text-on-surface">
          <span>{t("plugins.gridfinity.screwHoles")}</span>
          <input
            type="checkbox"
            checked={parameters.screwHoles}
            onChange={(event) =>
              void update({ ...parameters, screwHoles: event.target.checked })
            }
          />
        </label>
      </fieldset>

      <div className="mt-4 flex justify-end gap-2 border-t border-white/10 pt-3">
        <button
          type="button"
          className="cad-ribbon-action"
          disabled={disabled || isBusy}
          onClick={async () => {
            setIsBusy(true);
            try {
              await sendCoreCommand(makeDeleteFeatureCommand(featureId));
              await sendCoreCommand(makeGetViewportStateCommand());
              onClose();
            } finally {
              setIsBusy(false);
            }
          }}
        >
          {t("common.cancel")}
        </button>
        <button
          type="button"
          className="cad-ribbon-action cad-ribbon-action-primary"
          disabled={disabled || isBusy}
          onClick={async () => {
            setIsBusy(true);
            try {
              await sendCoreCommand(makeConfirmGridfinityFeatureCommand(featureId));
              await sendCoreCommand(makeGetViewportStateCommand());
              onClose();
            } finally {
              setIsBusy(false);
            }
          }}
        >
          {t("common.confirm")}
        </button>
      </div>
    </section>
  );
}
