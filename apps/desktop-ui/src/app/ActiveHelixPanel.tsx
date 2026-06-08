import { useTranslation } from "react-i18next";
import { Dropdown } from "../lib";
import type { HelixFeatureParameters } from "../types";
import { ActivePanelActions } from "./ActivePanelActions";
import type { HelixAction } from "./appState";

type HelixHandedness = HelixFeatureParameters["handedness"];

interface ActiveHelixPanelProps {
  action: HelixAction;
  axisLabel: string;
  disabled: boolean;
  parameters: HelixFeatureParameters | null;
  onCancel: () => void;
  onConfirm: () => void;
  onUpdateParameters: (patch: Partial<HelixFeatureParameters>) => void;
}

export function ActiveHelixPanel({
  action,
  axisLabel,
  disabled,
  parameters,
  onCancel,
  onConfirm,
  onUpdateParameters,
}: ActiveHelixPanelProps) {
  const { t } = useTranslation();

  return (
    <section className="pointer-events-auto cad-floating-panel w-[20rem] px-5 py-5">
      <p className="cad-kicker">{t("panels.helix.title")}</p>
      {action.phase === "pending" || !parameters ? (
        <>
          <p className="mt-3 text-xs text-on-surface-muted">
            {t("panels.helix.pickAxis")}
          </p>
          <button
            type="button"
            className="cad-action-ghost mt-4 w-full"
            disabled={disabled}
            onClick={onCancel}
          >
            {t("common.cancel")}
          </button>
        </>
      ) : (
        <>
          <div className="mt-4 grid grid-cols-[auto_1fr] gap-x-4 gap-y-2 text-xs uppercase tracking-[0.18em] text-on-surface-dim">
            <span>{t("panels.helix.axis")}</span>
            <span className="truncate text-right text-on-surface">
              {axisLabel}
            </span>
          </div>
          <div className="mt-5 space-y-4">
            <label className="block">
              <span className="cad-field-label">
                {t("panels.helix.radius")}
              </span>
              <input
                type="number"
                min={0}
                step={0.1}
                className="cad-input mt-2 w-full"
                value={parameters.radius}
                disabled={disabled}
                onChange={(event) => {
                  const value = event.currentTarget.valueAsNumber;
                  if (!Number.isFinite(value)) {
                    return;
                  }
                  onUpdateParameters({ radius: value });
                }}
              />
            </label>
            <label className="block">
              <span className="cad-field-label">
                {t("panels.helix.pitch")}
              </span>
              <input
                type="number"
                min={0}
                step={0.1}
                className="cad-input mt-2 w-full"
                value={parameters.pitch}
                disabled={disabled}
                onChange={(event) => {
                  const value = event.currentTarget.valueAsNumber;
                  if (!Number.isFinite(value)) {
                    return;
                  }
                  onUpdateParameters({ pitch: value });
                }}
              />
            </label>
            <label className="block">
              <span className="cad-field-label">
                {t("panels.helix.height")}
              </span>
              <input
                type="number"
                min={0}
                step={0.1}
                className="cad-input mt-2 w-full"
                value={parameters.height}
                disabled={disabled}
                onChange={(event) => {
                  const value = event.currentTarget.valueAsNumber;
                  if (!Number.isFinite(value)) {
                    return;
                  }
                  onUpdateParameters({ height: value });
                }}
              />
            </label>
            <label className="block">
              <span className="cad-field-label">
                {t("panels.helix.startAngle")}
              </span>
              <input
                type="number"
                step={1}
                className="cad-input mt-2 w-full"
                value={parameters.start_angle_degrees}
                disabled={disabled}
                onChange={(event) => {
                  const value = event.currentTarget.valueAsNumber;
                  if (!Number.isFinite(value)) {
                    return;
                  }
                  onUpdateParameters({ start_angle_degrees: value });
                }}
              />
            </label>
            <div>
              <span className="cad-field-label">
                {t("panels.helix.handedness")}
              </span>
              <Dropdown<HelixHandedness>
                label={t("panels.helix.handedness")}
                className="mt-2"
                value={parameters.handedness}
                disabled={disabled}
                options={[
                  {
                    value: "right",
                    label: t("panels.helix.rightHand"),
                  },
                  {
                    value: "left",
                    label: t("panels.helix.leftHand"),
                  },
                ]}
                onChange={(value) => {
                  onUpdateParameters({ handedness: value });
                }}
              />
            </div>
          </div>
          <ActivePanelActions
            disabled={disabled}
            onCancel={onCancel}
            onConfirm={onConfirm}
          />
        </>
      )}
    </section>
  );
}
