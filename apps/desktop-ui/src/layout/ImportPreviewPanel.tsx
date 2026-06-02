import { useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";

import type { ImportPlacementPayload } from "@/types";

interface ImportPreviewPanelProps {
  kind: "image" | "svg";
  planeSelected: boolean;
  fileSelected: boolean;
  fileName: string | null;
  parameters: ImportPlacementPayload | null;
  disabled: boolean;
  onPickFile: () => Promise<void> | void;
  onPreviewParameters: (parameters: ImportPlacementPayload) => Promise<void>;
  onConfirm: () => Promise<void> | void;
  onCancel: () => Promise<void> | void;
  onError: (message: string) => void;
}

type NumberField =
  | "offset_u_mm"
  | "offset_v_mm"
  | "rotation_degrees"
  | "width_mm"
  | "height_mm";

function stringValues(parameters: ImportPlacementPayload | null) {
  return {
    offset_u_mm: String(parameters?.offset_u_mm ?? 0),
    offset_v_mm: String(parameters?.offset_v_mm ?? 0),
    rotation_degrees: String(parameters?.rotation_degrees ?? 0),
    width_mm: String(parameters?.width_mm ?? 100),
    height_mm: String(parameters?.height_mm ?? 100),
  };
}

export function ImportPreviewPanel({
  kind,
  planeSelected,
  fileSelected,
  fileName,
  parameters,
  disabled,
  onPickFile,
  onPreviewParameters,
  onConfirm,
  onCancel,
  onError,
}: ImportPreviewPanelProps) {
  const { t } = useTranslation();
  const [values, setValues] = useState(() => stringValues(parameters));
  const [isSubmitting, setIsSubmitting] = useState(false);
  const parametersRef = useRef(parameters);

  useEffect(() => {
    parametersRef.current = parameters;
    setValues(stringValues(parameters));
  }, [parameters]);

  function updateField(field: NumberField, raw: string) {
    setValues((current) => ({ ...current, [field]: raw }));
    const parsed = Number(raw);
    const current = parametersRef.current;
    if (!current || !Number.isFinite(parsed)) {
      return;
    }
    let next = { ...current, [field]: parsed };
    if (current.lock_aspect && field === "width_mm" && current.width_mm > 0) {
      next = {
        ...next,
        height_mm: (parsed / current.width_mm) * current.height_mm,
      };
    } else if (
      current.lock_aspect &&
      field === "height_mm" &&
      current.height_mm > 0
    ) {
      next = {
        ...next,
        width_mm: (parsed / current.height_mm) * current.width_mm,
      };
    }
    parametersRef.current = next;
    void onPreviewParameters(next);
  }

  async function handleConfirm() {
    if (disabled || isSubmitting) {
      return;
    }
    if (!fileSelected || !parametersRef.current) {
      onError(t("panels.import.missingFileOrPlane"));
      return;
    }
    setIsSubmitting(true);
    try {
      await onConfirm();
    } catch (error) {
      onError(String(error));
    } finally {
      setIsSubmitting(false);
    }
  }

  function renderField(field: NumberField, label: string) {
    return (
      <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
        <span>{label}</span>
        <input
          className="cad-input mt-2"
          type="number"
          step="any"
          value={values[field]}
          disabled={disabled || isSubmitting || !parameters}
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
      <p className="cad-kicker">
        {kind === "image"
          ? t("panels.import.imageTitle")
          : t("panels.import.svgTitle")}
      </p>
      <p className="mt-3 text-xs uppercase tracking-[0.18em] text-on-surface-muted">
        {planeSelected ? t("panels.import.planeReady") : t("panels.import.choosePlane")}
      </p>
      <button
        type="button"
        className="cad-action-ghost mt-4 w-full"
        disabled={disabled || isSubmitting || !planeSelected}
        onClick={() => void onPickFile()}
      >
        {fileName ?? t("panels.import.chooseFile")}
      </button>
      <form
        noValidate
        className="mt-4 space-y-4"
        onSubmit={(event) => {
          event.preventDefault();
          void handleConfirm();
        }}
      >
        <div>
          <p className="cad-field-label">{t("panels.import.offset")}</p>
          <div className="mt-2 grid grid-cols-2 gap-2">
            {renderField("offset_u_mm", "X")}
            {renderField("offset_v_mm", "Y")}
          </div>
        </div>
        <div>{renderField("rotation_degrees", t("panels.import.rotation"))}</div>
        <div>
          <p className="cad-field-label">{t("panels.import.size")}</p>
          <div className="mt-2 grid grid-cols-2 gap-2">
            {renderField("width_mm", t("panels.import.width"))}
            {renderField("height_mm", t("panels.import.height"))}
          </div>
        </div>
        <label className="flex items-center gap-2 text-xs uppercase tracking-[0.16em] text-on-surface-muted">
          <input
            type="checkbox"
            checked={parameters?.lock_aspect ?? true}
            disabled={disabled || isSubmitting || !parameters}
            onChange={(event) => {
              const current = parametersRef.current;
              if (!current) return;
              const next = { ...current, lock_aspect: event.target.checked };
              parametersRef.current = next;
              void onPreviewParameters(next);
            }}
          />
          {t("panels.import.lockAspect")}
        </label>
        <div className="flex gap-3 pt-1">
          <button
            type="submit"
            className="cad-action-primary flex-1"
            disabled={disabled || isSubmitting || !fileSelected || !parameters}
            onClick={(event) => {
              event.preventDefault();
              void handleConfirm();
            }}
          >
            {isSubmitting ? t("common.working") : t("common.confirm")}
          </button>
          <button
            type="button"
            className="cad-action-ghost flex-1"
            disabled={disabled || isSubmitting}
            onClick={() => void onCancel()}
          >
            {t("common.cancel")}
          </button>
        </div>
      </form>
    </section>
  );
}
