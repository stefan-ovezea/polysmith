import { useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";

import { readNumberInputValue } from "./numberInput";

export function CamNumberField({
  label,
  value,
  disabled,
  step = 0.5,
  min = 0,
  clearable = false,
  onChange,
}: {
  label: string;
  value: number | undefined;
  disabled: boolean;
  step?: number | "any";
  min?: number;
  /** Empty input commits undefined instead of 0 (optional parameters
   *  like stepdown: cleared = "no multi-pass"). */
  clearable?: boolean;
  onChange: (value: number | undefined) => void;
}) {
  // String draft so a cleared input stays empty while the user types —
  // a controlled number input would repaint "" as 0 immediately.
  const [draft, setDraft] = useState<string | null>(null);
  const display = draft ?? (value === undefined ? "" : String(value));

  return (
    <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
      {label}
      <input
        className="cad-input mt-2"
        type="number"
        min={min}
        step={step}
        value={display}
        disabled={disabled}
        onChange={(event) => {
          const raw = event.currentTarget.value;
          setDraft(raw);
          if (clearable && raw === "") {
            onChange(undefined);
            return;
          }
          onChange(readNumberInputValue(event.currentTarget));
        }}
      />
    </label>
  );
}

export function CamCheckboxField({
  label,
  checked,
  disabled,
  onChange,
}: {
  label: string;
  checked: boolean;
  disabled: boolean;
  onChange: (checked: boolean) => void;
}) {
  return (
    <label className="flex items-center gap-2 text-xs uppercase tracking-[0.18em] text-on-surface-muted">
      <input
        className="h-3.5 w-3.5"
        type="checkbox"
        checked={checked}
        disabled={disabled}
        onChange={(event) => onChange(event.currentTarget.checked)}
      />
      {label}
    </label>
  );
}

export function useCamEscapeCancel(onCancel: () => void) {
  useEffect(() => {
    function handleKeyDown(event: KeyboardEvent) {
      if (event.key !== "Escape") {
        return;
      }
      if ((event.target as HTMLElement | null)?.closest(".cad-dropdown")) {
        return;
      }
      event.preventDefault();
      onCancel();
    }

    window.addEventListener("keydown", handleKeyDown);
    return () => window.removeEventListener("keydown", handleKeyDown);
  }, [onCancel]);
}

export function useDebouncedCamUpdate(
  serialized: string,
  onDispatch: () => void,
  delayMs = 200,
) {
  const lastSentRef = useRef("");

  useEffect(() => {
    if (serialized === lastSentRef.current) {
      return;
    }
    const timer = window.setTimeout(() => {
      lastSentRef.current = serialized;
      onDispatch();
    }, delayMs);
    return () => window.clearTimeout(timer);
  }, [delayMs, onDispatch, serialized]);

  return () => {
    lastSentRef.current = serialized;
  };
}

// Operation status readout shared by the operation panels.  Maps the
// core's CamOperationStatus to themed text:
//   pending          → muted placeholder
//   generated        → success text + toolpath stats (if cached)
//   needs_regenerate → warning text
//   error            → the core's status_message in danger red
export interface CamToolpathStats {
  totalLengthMm?: number;
  estimatedTimeSeconds?: number;
}

export function CamStatusLine({
  status,
  statusMessage,
  toolpathStats,
  prefix,
}: {
  status: string;
  statusMessage: string;
  toolpathStats: CamToolpathStats | null;
  prefix:
    | "cam.laserCut"
    | "cam.faceMilling"
    | "cam.testPattern"
    | "cam.pocket"
    | "cam.contour";
}) {
  const { t } = useTranslation();

  if (status === "generated") {
    const stats =
      toolpathStats?.totalLengthMm !== undefined ||
      toolpathStats?.estimatedTimeSeconds !== undefined
        ? t(`${prefix}.statusStats`, {
            length:
              toolpathStats?.totalLengthMm !== undefined
                ? toolpathStats.totalLengthMm.toFixed(1)
                : "-",
            time:
              toolpathStats?.estimatedTimeSeconds !== undefined
                ? toolpathStats.estimatedTimeSeconds.toFixed(1)
                : "-",
          })
        : null;
    return (
      <p className="text-[10px] leading-relaxed text-success">
        {t(`${prefix}.statusGenerated`)}
        {stats ? <span> — {stats}</span> : null}
      </p>
    );
  }

  if (status === "needs_regenerate") {
    return (
      <p className="text-[10px] leading-relaxed text-on-surface-muted">
        {t(`${prefix}.statusNeedsRegenerate`)}
      </p>
    );
  }

  if (status === "error") {
    return (
      <p className="text-[10px] leading-relaxed text-danger">
        {statusMessage || t(`${prefix}.statusError`)}
      </p>
    );
  }

  // pending (or any unknown status): muted placeholder.
  return (
    <p className="text-[10px] leading-relaxed text-on-surface-dim">
      {t(`${prefix}.statusPending`)}
    </p>
  );
}
