import { useTranslation } from "react-i18next";
import { Checkbox, Dropdown } from "@/lib";
import type { PluginSettingsPanelProps } from "../sdk";
import type { GridfinityPluginConfig } from "./types";

function clampInteger(value: number, min: number, max: number) {
  if (!Number.isFinite(value)) {
    return min;
  }
  return Math.min(Math.max(Math.round(value), min), max);
}

function numberValue(value: string, fallback: number) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : fallback;
}

export function GridfinitySettingsPanel({
  config,
  disabled,
  onChange,
}: PluginSettingsPanelProps<GridfinityPluginConfig>) {
  const { t } = useTranslation();

  return (
    <fieldset
      disabled={disabled}
      className={
        disabled
          ? "m-0 space-y-4 border-0 p-0 opacity-45"
          : "m-0 space-y-4 border-0 p-0"
      }
    >
      <label className="block">
        <span className="cad-kicker">{t("plugins.gridfinity.defaultModel")}</span>
        <Dropdown
          className="mt-2"
          label={t("plugins.gridfinity.defaultModel")}
          value={config.defaultModelKind}
          options={[
            { value: "bin", label: t("plugins.gridfinity.bin") },
            { value: "baseplate", label: t("plugins.gridfinity.baseplate") },
          ]}
          onChange={(defaultModelKind) =>
            onChange({ ...config, defaultModelKind })
          }
        />
      </label>

      <div className="grid grid-cols-3 gap-3">
        {[
          ["gridX", t("plugins.gridfinity.gridX"), 1, 12],
          ["gridY", t("plugins.gridfinity.gridY"), 1, 12],
          ["gridZ", t("plugins.gridfinity.gridZ"), 1, 24],
        ].map(([key, label, min, max]) => (
          <label key={key} className="block">
            <span className="cad-kicker">{label}</span>
            <input
              className="mt-2 w-full rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
              type="number"
              min={min}
              max={max}
              value={config[key as "gridX" | "gridY" | "gridZ"]}
              onChange={(event) =>
                onChange({
                  ...config,
                  [key]: clampInteger(
                    numberValue(event.target.value, Number(min)),
                    Number(min),
                    Number(max),
                  ),
                })
              }
            />
          </label>
        ))}
      </div>

      <div className="grid grid-cols-2 gap-3">
        <label className="block">
          <span className="cad-kicker">
            {t("plugins.gridfinity.compartmentsX")}
          </span>
          <input
            className="mt-2 w-full rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
            type="number"
            min={1}
            max={12}
            value={config.compartmentsX}
            onChange={(event) =>
              onChange({
                ...config,
                compartmentsX: clampInteger(
                  numberValue(event.target.value, 1),
                  1,
                  12,
                ),
              })
            }
          />
        </label>
        <label className="block">
          <span className="cad-kicker">
            {t("plugins.gridfinity.compartmentsY")}
          </span>
          <input
            className="mt-2 w-full rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
            type="number"
            min={1}
            max={12}
            value={config.compartmentsY}
            onChange={(event) =>
              onChange({
                ...config,
                compartmentsY: clampInteger(
                  numberValue(event.target.value, 1),
                  1,
                  12,
                ),
              })
            }
          />
        </label>
      </div>

      <div className="grid grid-cols-2 gap-3">
        <label className="block">
          <span className="cad-kicker">
            {t("plugins.gridfinity.wallThickness")}
          </span>
          <input
            className="mt-2 w-full rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
            type="number"
            min={0.8}
            step={0.1}
            value={config.wallThickness}
            onChange={(event) =>
              onChange({
                ...config,
                wallThickness: Math.max(
                  numberValue(event.target.value, 1.6),
                  0.8,
                ),
              })
            }
          />
        </label>
        <label className="block">
          <span className="cad-kicker">
            {t("plugins.gridfinity.floorThickness")}
          </span>
          <input
            className="mt-2 w-full rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
            type="number"
            min={1.2}
            step={0.1}
            value={config.floorThickness}
            onChange={(event) =>
              onChange({
                ...config,
                floorThickness: Math.max(
                  numberValue(event.target.value, 2.4),
                  1.2,
                ),
              })
            }
          />
        </label>
      </div>

      {[
        ["stackingLip", t("plugins.gridfinity.stackingLip")],
        ["labelTab", t("plugins.gridfinity.labelTab")],
        ["magnetHoles", t("plugins.gridfinity.magnetHoles")],
        ["screwHoles", t("plugins.gridfinity.screwHoles")],
      ].map(([key, label]) => (
        <label
          key={key}
          className="flex items-center justify-between gap-4 rounded-md border border-white/10 bg-white/[0.025] px-4 py-3"
        >
          <span className="text-sm text-on-surface">{label}</span>
          <Checkbox
            checked={Boolean(config[key as keyof GridfinityPluginConfig])}
            ariaLabel={label}
            onCheckedChange={(checked) => onChange({ ...config, [key]: checked })}
          />
        </label>
      ))}
    </fieldset>
  );
}
