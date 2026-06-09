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
            { value: "light_bin", label: t("plugins.gridfinity.lightBin") },
            { value: "solid_bin", label: t("plugins.gridfinity.solidBin") },
            { value: "holey_bin", label: t("plugins.gridfinity.holeyBin") },
            { value: "baseplate", label: t("plugins.gridfinity.baseplate") },
          ]}
          onChange={(defaultModelKind) =>
            onChange({ ...config, defaultModelKind })
          }
        />
      </label>

      <div className="grid grid-cols-3 gap-3">
        {[
          ["gridX", t("plugins.gridfinity.gridX"), 1, 6],
          ["gridY", t("plugins.gridfinity.gridY"), 1, 6],
          [
            "gridZ",
            t("plugins.gridfinity.gridZ"),
            config.defaultModelKind === "solid_bin" ||
            config.defaultModelKind === "light_bin"
              ? 1
              : 2,
            12,
          ],
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
            {t("plugins.gridfinity.holeyHolesX")}
          </span>
          <input
            className="mt-2 w-full rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
            type="number"
            min={1}
            max={24}
            value={config.holeyHolesX}
            onChange={(event) =>
              onChange({
                ...config,
                holeyHolesX: clampInteger(
                  numberValue(event.target.value, 3),
                  1,
                  24,
                ),
              })
            }
          />
        </label>
        <label className="block">
          <span className="cad-kicker">
            {t("plugins.gridfinity.holeyHolesY")}
          </span>
          <input
            className="mt-2 w-full rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
            type="number"
            min={1}
            max={24}
            value={config.holeyHolesY}
            onChange={(event) =>
              onChange({
                ...config,
                holeyHolesY: clampInteger(
                  numberValue(event.target.value, 3),
                  1,
                  24,
                ),
              })
            }
          />
        </label>
      </div>

      <label className="block">
        <span className="cad-kicker">
          {t("plugins.gridfinity.holeyHoleShape")}
        </span>
        <select
          className="mt-2 w-full rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
          value={config.holeyHoleShape}
          onChange={(event) =>
            onChange({
              ...config,
              holeyHoleShape: event.target.value as typeof config.holeyHoleShape,
            })
          }
        >
          <option value="circle">{t("plugins.gridfinity.holeyCircle")}</option>
          <option value="square">{t("plugins.gridfinity.holeySquare")}</option>
          <option value="hexagon">{t("plugins.gridfinity.holeyHexagon")}</option>
        </select>
      </label>

      <div className="grid grid-cols-2 gap-3">
        <label className="block">
          <span className="cad-kicker">
            {t("plugins.gridfinity.holeyHoleSize")}
          </span>
          <input
            className="mt-2 w-full rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
            type="number"
            min={0.5}
            step={0.1}
            value={config.holeyHoleSize}
            onChange={(event) =>
              onChange({
                ...config,
                holeyHoleSize: Math.min(
                  Math.max(numberValue(event.target.value, 4), 0.5),
                  config.holeyKeepoutDiameter,
                ),
              })
            }
          />
        </label>
        <label className="block">
          <span className="cad-kicker">
            {t("plugins.gridfinity.holeyHoleDepth")}
          </span>
          <input
            className="mt-2 w-full rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
            type="number"
            min={0.5}
            step={0.1}
            value={config.holeyHoleDepth}
            onChange={(event) =>
              onChange({
                ...config,
                holeyHoleDepth: Math.max(
                  numberValue(event.target.value, 5),
                  0.5,
                ),
              })
            }
          />
        </label>
      </div>

      <label className="block">
        <span className="cad-kicker">
          {t("plugins.gridfinity.holeyKeepout")}
        </span>
        <input
          className="mt-2 w-full rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
          type="number"
          min={1}
          step={0.1}
          value={config.holeyKeepoutDiameter}
          onChange={(event) =>
            onChange({
              ...config,
              holeyKeepoutDiameter: Math.max(
                numberValue(event.target.value, 12),
                1,
              ),
            })
          }
        />
      </label>

      <div className="grid grid-cols-2 gap-3">
        <label className="block">
          <span className="cad-kicker">
            {t("plugins.gridfinity.compartmentsX")}
          </span>
          <input
            className="mt-2 w-full rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
            type="number"
            min={1}
            max={config.gridX * 4}
            value={config.compartmentsX}
            onChange={(event) =>
              onChange({
                ...config,
                compartmentsX: clampInteger(
                  numberValue(event.target.value, 1),
                  1,
                  config.gridX * 4,
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
            max={config.gridY * 4}
            value={config.compartmentsY}
            onChange={(event) =>
              onChange({
                ...config,
                compartmentsY: clampInteger(
                  numberValue(event.target.value, 1),
                  1,
                  config.gridY * 4,
                ),
              })
            }
          />
        </label>
      </div>

      <label className="block">
        <span className="cad-kicker">
          {t("plugins.gridfinity.lightWallThickness")}
        </span>
        <input
          className="mt-2 w-full rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
          type="number"
          min={0.1}
          step={0.1}
          value={config.lightWallThickness}
          onChange={(event) =>
            onChange({
              ...config,
              lightWallThickness: Math.max(
                numberValue(event.target.value, 1.5),
                0.1,
              ),
            })
          }
        />
      </label>

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

      <div className="grid grid-cols-2 gap-3">
        <label className="block">
          <span className="cad-kicker">
            {t("plugins.gridfinity.dividerThickness")}
          </span>
          <input
            className="mt-2 w-full rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
            type="number"
            min={0.1}
            step={0.1}
            value={config.dividerThickness}
            onChange={(event) =>
              onChange({
                ...config,
                dividerThickness: Math.max(
                  numberValue(event.target.value, 1.5),
                  0.1,
                ),
              })
            }
          />
        </label>
        <label className="block">
          <span className="cad-kicker">
            {t("plugins.gridfinity.labelRidgeWidth")}
          </span>
          <input
            className="mt-2 w-full rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
            type="number"
            min={1}
            step={0.1}
            value={config.labelRidgeWidth}
            onChange={(event) =>
              onChange({
                ...config,
                labelRidgeWidth: Math.max(
                  numberValue(event.target.value, 13),
                  1,
                ),
              })
            }
          />
        </label>
      </div>

      <label className="block">
        <span className="cad-kicker">
          {t("plugins.gridfinity.magnetHoleDiameter")}
        </span>
        <input
          className="mt-2 w-full rounded-md border border-white/10 bg-black/20 px-3 py-2 text-sm text-on-surface outline-none transition-colors focus:border-primary-edge"
          type="number"
          min={1}
          step={0.1}
          value={config.magnetHoleDiameter}
          onChange={(event) =>
            onChange({
              ...config,
              magnetHoleDiameter: Math.max(
                numberValue(event.target.value, 6.5),
                1,
              ),
            })
          }
        />
      </label>

      {[
        ["stackingLip", t("plugins.gridfinity.stackingLip")],
        ["labelTab", t("plugins.gridfinity.labelTab")],
        ["multiLabel", t("plugins.gridfinity.multiLabel")],
        ["grabCurve", t("plugins.gridfinity.grabCurve")],
        ["magnetHoles", t("plugins.gridfinity.magnetHoles")],
        ["magnetRemovalHoles", t("plugins.gridfinity.magnetRemovalHoles")],
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
