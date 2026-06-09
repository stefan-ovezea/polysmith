import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";

import { sendCoreCommand } from "@/lib/cadCoreClient";
import { makeGetViewportStateCommand } from "@/lib/ipcProtocol";
import { makeDeleteFeatureCommand } from "@/lib/ipc/bodyFeatureCommands";
import type { GridfinityFeatureParameters } from "./types";
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
  label: string;
  max: number;
  min: number;
  value: number;
  onCommit: (value: number) => void;
}

function clampInteger(value: number, min: number, max: number) {
  if (!Number.isFinite(value)) {
    return min;
  }
  return Math.min(Math.max(Math.round(value), min), max);
}

function numericValue(value: string, fallback: number) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : fallback;
}

function NumberField({
  disabled = false,
  label,
  max,
  min,
  value,
  onCommit,
}: NumberFieldProps) {
  const [draft, setDraft] = useState(String(value));

  useEffect(() => {
    setDraft(String(value));
  }, [value]);

  function commit() {
    const next = clampInteger(numericValue(draft, value), min, max);
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
    { key: "gridX", label: t("plugins.gridfinity.gridX"), min: 1, max: 12 },
    { key: "gridY", label: t("plugins.gridfinity.gridY"), min: 1, max: 12 },
    { key: "gridZ", label: t("plugins.gridfinity.gridZ"), min: 1, max: 24 },
  ];

  return (
    <section className="pointer-events-auto cad-floating-panel cad-scrollbar box-border flex max-h-full min-h-0 w-full flex-col overflow-y-auto px-5 py-5">
      <div>
        <p className="cad-kicker">{t("plugins.gridfinity.title")}</p>
        <h2 className="mt-1 font-display text-lg text-on-surface">
          {parameters.modelKind === "baseplate"
            ? t("plugins.gridfinity.baseplate")
            : t("plugins.gridfinity.bin")}
        </h2>
      </div>

      <fieldset
        disabled={disabled || isBusy}
        className="m-0 mt-4 space-y-3 border-0 p-0"
      >
        <div className="grid grid-cols-2 gap-2">
          {(["bin", "baseplate"] as const).map((kind) => (
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
              {kind === "bin"
                ? t("plugins.gridfinity.bin")
                : t("plugins.gridfinity.baseplate")}
            </button>
          ))}
        </div>

        <div className="grid grid-cols-3 gap-2">
          {dimensionFields.map(({ key, label, min, max }) => (
            <NumberField
              key={key}
              label={label}
              min={min}
              max={max}
              value={parameters[key as "gridX" | "gridY" | "gridZ"]}
              disabled={key === "gridZ" && parameters.modelKind === "baseplate"}
              onCommit={(value) => {
                void update({ ...parameters, [key]: value });
              }}
            />
          ))}
        </div>

        {parameters.modelKind === "bin" ? (
          <>
            <div className="grid grid-cols-2 gap-2">
              <NumberField
                label={t("plugins.gridfinity.compartmentsX")}
                min={1}
                max={parameters.gridX}
                value={parameters.compartmentsX}
                onCommit={(value) =>
                  void update({ ...parameters, compartmentsX: value })
                }
              />
              <NumberField
                label={t("plugins.gridfinity.compartmentsY")}
                min={1}
                max={parameters.gridY}
                value={parameters.compartmentsY}
                onCommit={(value) =>
                  void update({ ...parameters, compartmentsY: value })
                }
              />
            </div>
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
          </>
        ) : null}

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
