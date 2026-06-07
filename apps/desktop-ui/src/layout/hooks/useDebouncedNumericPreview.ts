import { useEffect, useRef, useState } from "react";

const DEFAULT_PREVIEW_DEBOUNCE_MS = 200;

interface UseDebouncedNumericPreviewOptions {
  initialValue: number;
  onPreviewValue: (value: number) => Promise<void> | void;
  debounceMs?: number;
  isValid?: (value: number) => boolean;
}

function isPositiveFiniteNumber(value: number) {
  return Number.isFinite(value) && value > 0;
}

export function useDebouncedNumericPreview({
  initialValue,
  onPreviewValue,
  debounceMs = DEFAULT_PREVIEW_DEBOUNCE_MS,
  isValid = isPositiveFiniteNumber,
}: UseDebouncedNumericPreviewOptions) {
  const [value, setValue] = useState(String(initialValue));
  const inputRef = useRef<HTMLInputElement | null>(null);
  const previewTimerRef = useRef<number | null>(null);
  const lastPreviewedRef = useRef<number>(initialValue);
  const onPreviewValueRef = useRef(onPreviewValue);

  useEffect(() => {
    onPreviewValueRef.current = onPreviewValue;
  }, [onPreviewValue]);

  useEffect(() => {
    inputRef.current?.focus();
    inputRef.current?.select();
    return () => {
      if (previewTimerRef.current !== null) {
        window.clearTimeout(previewTimerRef.current);
        previewTimerRef.current = null;
      }
    };
  }, []);

  function handleValueChange(nextValue: string) {
    setValue(nextValue);
    const parsed = Number(nextValue);
    if (!isValid(parsed)) {
      return;
    }

    if (previewTimerRef.current !== null) {
      window.clearTimeout(previewTimerRef.current);
    }

    previewTimerRef.current = window.setTimeout(() => {
      previewTimerRef.current = null;
      if (parsed === lastPreviewedRef.current) {
        return;
      }
      lastPreviewedRef.current = parsed;
      void onPreviewValueRef.current(parsed);
    }, debounceMs);
  }

  async function flushPendingValue() {
    if (previewTimerRef.current !== null) {
      window.clearTimeout(previewTimerRef.current);
      previewTimerRef.current = null;
    }

    const parsed = Number(value);
    if (!isValid(parsed)) {
      return;
    }

    if (parsed === lastPreviewedRef.current) {
      return;
    }

    lastPreviewedRef.current = parsed;
    await onPreviewValueRef.current(parsed);
  }

  return {
    value,
    inputRef,
    handleValueChange,
    flushPendingValue,
  };
}
