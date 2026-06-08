import { useTranslation } from "react-i18next";

import { PreviewPanelActions } from "./PreviewPanelActions";
import { useDebouncedNumericPreview } from "./hooks/useDebouncedNumericPreview";

interface RevolvePreviewPanelProps {
  phase: "pending" | "active";
  initialAngle: number;
  profileLabel: string | null;
  axisLabel: string | null;
  disabled: boolean;
  canConfirm: boolean;
  onPreviewAngle: (angleDegrees: number) => Promise<void>;
  onConfirm: (angleDegrees: number) => void | Promise<void>;
  onCancel: () => Promise<void>;
}

export function RevolvePreviewPanel({
  phase,
  initialAngle,
  profileLabel,
  axisLabel,
  disabled,
  canConfirm,
  onPreviewAngle,
  onConfirm,
  onCancel,
}: RevolvePreviewPanelProps) {
  const { t } = useTranslation();
  const {
    value: angle,
    inputRef,
    handleValueChange: handleAngleChange,
    flushPendingValue: flushPendingAngle,
  } = useDebouncedNumericPreview({
    initialValue: initialAngle,
    onPreviewValue: onPreviewAngle,
    isValid: isValidAngle,
    immediate: phase === "pending",
  });

  function parseAngle(value: string) {
    const parsed = Number(value);
    return isValidAngle(parsed) ? parsed : null;
  }

  async function handleConfirm() {
    const parsed =
      phase === "active" ? await flushPendingAngle() : parseAngle(angle);
    if (parsed === null) {
      return;
    }
    await onConfirm(parsed);
  }

  return (
    <section className="pointer-events-auto cad-floating-panel box-border w-80 max-w-[calc(100vw-2rem)] overflow-hidden px-5 py-5">
      <p className="cad-kicker">{t("panels.revolve.title")}</p>
      <div className="mt-3 space-y-2 rounded-md bg-surface-container-low px-3 py-2 text-xs uppercase tracking-[0.16em] text-on-surface-muted">
        <div className="flex items-center justify-between gap-3">
          <span>{t("panels.revolve.profile")}</span>
          <span className="min-w-0 truncate text-on-surface">
            {profileLabel ?? t("panels.revolve.pickProfile")}
          </span>
        </div>
        <div className="flex items-center justify-between gap-3">
          <span>{t("panels.revolve.axis")}</span>
          <span className="min-w-0 truncate text-on-surface">
            {axisLabel ?? t("panels.revolve.pickAxis")}
          </span>
        </div>
      </div>
      <form
        className="mt-4 space-y-4"
        noValidate
        onSubmit={(event) => {
          event.preventDefault();
          void handleConfirm();
        }}
      >
        <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
          {t("panels.revolve.angle")}
          <input
            ref={inputRef}
            className="cad-input mt-2"
            type="number"
            min="0.01"
            max="360"
            step="any"
            value={angle}
            disabled={disabled}
            onChange={(event) => handleAngleChange(event.target.value)}
            onKeyDown={(event) => {
              if (event.key === "Escape") {
                event.preventDefault();
                void onCancel();
              }
            }}
          />
        </label>
        <PreviewPanelActions
          confirmDisabled={disabled || !canConfirm || parseAngle(angle) === null}
          cancelDisabled={disabled}
          onCancel={() => {
            void onCancel();
          }}
        />
        <p className="text-[11px] uppercase tracking-[0.16em] text-on-surface-dim">
          {t("panels.revolve.pickHint")}
        </p>
      </form>
    </section>
  );
}

function isValidAngle(value: number) {
  return Number.isFinite(value) && value > 0 && value <= 360;
}
