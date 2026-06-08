import { useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";

import type { MoveFeatureParameters } from "@/types";
import { normalizeNumberInputValue } from "./numberInput";

interface MovePreviewPanelProps {
  phase: "pending" | "active";
  bodyLabel: string | null;
  parameters: MoveFeatureParameters;
  disabled: boolean;
  onPreviewParameters: (parameters: MoveFeatureParameters) => Promise<void>;
  onConfirm: () => Promise<void> | void;
  onCancel: () => Promise<void> | void;
}

type MoveNumberField =
  | "translation_x"
  | "translation_y"
  | "translation_z"
  | "rotation_x_degrees"
  | "rotation_y_degrees"
  | "rotation_z_degrees";

function valuesFromParameters(parameters: MoveFeatureParameters) {
  return {
    translation_x: String(parameters.translation_x),
    translation_y: String(parameters.translation_y),
    translation_z: String(parameters.translation_z),
    rotation_x_degrees: String(parameters.rotation_x_degrees),
    rotation_y_degrees: String(parameters.rotation_y_degrees),
    rotation_z_degrees: String(parameters.rotation_z_degrees),
  };
}

export function MovePreviewPanel({
  phase,
  bodyLabel,
  parameters,
  disabled,
  onPreviewParameters,
  onConfirm,
  onCancel,
}: MovePreviewPanelProps) {
  const { t } = useTranslation();
  const [values, setValues] = useState(() => valuesFromParameters(parameters));
  const parametersRef = useRef(parameters);
  const onPreviewParametersRef = useRef(onPreviewParameters);

  useEffect(() => {
    parametersRef.current = parameters;
    setValues(valuesFromParameters(parameters));
  }, [parameters]);

  useEffect(() => {
    onPreviewParametersRef.current = onPreviewParameters;
  }, [onPreviewParameters]);

  function updateField(field: MoveNumberField, rawValue: string) {
    const normalized = normalizeNumberInputValue(rawValue);
    setValues((current) => ({ ...current, [field]: normalized }));
    const parsed = Number(normalized);
    if (!Number.isFinite(parsed)) {
      return;
    }
    const next = {
      ...parametersRef.current,
      [field]: parsed,
    };
    parametersRef.current = next;
    void onPreviewParametersRef.current(next);
  }

  function renderField(
    field: MoveNumberField,
    label: string,
    suffix: string,
  ) {
    return (
      <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
        <span>{label}</span>
        <input
          className="cad-input mt-2"
          type="number"
          step="any"
          value={values[field]}
          disabled={disabled || phase === "pending"}
          aria-label={`${label} ${suffix}`}
          onChange={(event) => updateField(field, event.target.value)}
          onKeyDown={(event) => {
            if (event.key === "Escape") {
              event.preventDefault();
              void onCancel();
            }
          }}
        />
      </label>
    );
  }

  return (
    <section className="pointer-events-auto cad-floating-panel w-80 px-5 py-5">
      <p className="cad-kicker">{t("panels.move.title")}</p>
      <p className="mt-3 text-xs uppercase tracking-[0.18em] text-on-surface-muted">
        {phase === "pending"
          ? t("panels.move.pickBody")
          : bodyLabel
            ? t("panels.move.selectedBody", { body: bodyLabel })
            : t("panels.move.bodySelected")}
      </p>
      <form
        className="mt-4 space-y-4"
        onSubmit={(event) => {
          event.preventDefault();
          void onConfirm();
        }}
      >
        <div>
          <p className="cad-field-label">{t("panels.move.translation")}</p>
          <div className="mt-2 grid grid-cols-3 gap-2">
            {renderField("translation_x", t("panels.move.x"), t("forms.distanceMm"))}
            {renderField("translation_y", t("panels.move.y"), t("forms.distanceMm"))}
            {renderField("translation_z", t("panels.move.z"), t("forms.distanceMm"))}
          </div>
        </div>
        <div>
          <p className="cad-field-label">{t("panels.move.rotation")}</p>
          <div className="mt-2 grid grid-cols-3 gap-2">
            {renderField("rotation_x_degrees", t("panels.move.x"), t("forms.angleDegrees"))}
            {renderField("rotation_y_degrees", t("panels.move.y"), t("forms.angleDegrees"))}
            {renderField("rotation_z_degrees", t("panels.move.z"), t("forms.angleDegrees"))}
          </div>
        </div>
        <div className="flex gap-3 pt-1">
          <button
            type="submit"
            className="cad-action-primary flex-1"
            disabled={disabled || phase === "pending"}
          >
            {t("common.confirm")}
          </button>
          <button
            type="button"
            className="cad-action-ghost flex-1"
            disabled={disabled}
            onClick={() => {
              void onCancel();
            }}
          >
            {t("common.cancel")}
          </button>
        </div>
        <p className="text-[11px] uppercase tracking-[0.16em] text-on-surface-dim">
          {t("panels.shortcutHint.confirm")}
        </p>
      </form>
    </section>
  );
}
