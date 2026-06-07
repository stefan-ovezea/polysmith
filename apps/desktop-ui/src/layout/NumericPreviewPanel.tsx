import type { ReactNode } from "react";
import { useTranslation } from "react-i18next";

import { useDebouncedNumericPreview } from "./hooks/useDebouncedNumericPreview";

interface NumericPreviewPanelProps {
  title: ReactNode;
  helperText: ReactNode;
  valueLabel: ReactNode;
  initialValue: number;
  disabled: boolean;
  canConfirm?: boolean;
  inputMin?: string;
  inputMax?: string;
  inputStep?: string;
  shortcutHint?: ReactNode;
  isPreviewValueValid?: (value: number) => boolean;
  isConfirmValueValid?: (value: number) => boolean;
  onPreviewValue: (value: number) => Promise<void> | void;
  onConfirm: () => Promise<void> | void;
  onCancel: () => Promise<void> | void;
}

function isPositiveFiniteNumber(value: number) {
  return Number.isFinite(value) && value > 0;
}

export function NumericPreviewPanel({
  title,
  helperText,
  valueLabel,
  initialValue,
  disabled,
  canConfirm = true,
  inputMin,
  inputMax,
  inputStep = "any",
  shortcutHint,
  isPreviewValueValid,
  isConfirmValueValid = isPositiveFiniteNumber,
  onPreviewValue,
  onConfirm,
  onCancel,
}: NumericPreviewPanelProps) {
  const { t } = useTranslation();
  const { value, inputRef, handleValueChange, flushPendingValue } =
    useDebouncedNumericPreview({
      initialValue,
      onPreviewValue,
      isValid: isPreviewValueValid,
    });
  const parsedValue = Number(value);
  const confirmDisabled =
    disabled || !canConfirm || !isConfirmValueValid(parsedValue);

  async function handleConfirm() {
    if (confirmDisabled) {
      return;
    }
    await flushPendingValue();
    await onConfirm();
  }

  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5">
      <p className="cad-kicker">{title}</p>
      <p className="mt-3 text-xs text-on-surface-muted">{helperText}</p>
      <form
        className="mt-4 space-y-4"
        onSubmit={(event) => {
          event.preventDefault();
          void handleConfirm();
        }}
      >
        <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
          {valueLabel}
          <input
            ref={inputRef}
            className="cad-input mt-2"
            type="number"
            min={inputMin}
            max={inputMax}
            step={inputStep}
            value={value}
            disabled={disabled}
            onChange={(event) => {
              handleValueChange(event.target.value);
            }}
            onKeyDown={(event) => {
              if (event.key === "Escape") {
                event.preventDefault();
                void onCancel();
              }
            }}
          />
        </label>
        <div className="flex gap-3">
          <button
            type="submit"
            className="cad-action-primary flex-1"
            disabled={confirmDisabled}
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
          {shortcutHint ?? t("panels.shortcutHint.confirm")}
        </p>
      </form>
    </section>
  );
}
