import { useEffect, useRef } from "react";

import { readNumberInputValue } from "./numberInput";

export function CamNumberField({
  label,
  value,
  disabled,
  step = 0.5,
  min = 0,
  onChange,
}: {
  label: string;
  value: number;
  disabled: boolean;
  step?: number;
  min?: number;
  onChange: (value: number) => void;
}) {
  return (
    <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
      {label}
      <input
        className="cad-input mt-2"
        type="number"
        min={min}
        step={step}
        value={value}
        disabled={disabled}
        onChange={(event) => onChange(readNumberInputValue(event.currentTarget))}
      />
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
