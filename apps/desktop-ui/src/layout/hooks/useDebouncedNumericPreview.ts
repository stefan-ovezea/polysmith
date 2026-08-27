import { useEffect, useRef, useState } from "react";

const DEFAULT_PREVIEW_DEBOUNCE_MS = 200;

interface UseDebouncedNumericPreviewOptions {
  initialValue: number;
  onPreviewValue: (value: number) => Promise<void> | void;
  debounceMs?: number;
  isValid?: (value: number) => boolean;
  immediate?: boolean;
}

function isPositiveFiniteNumber(value: number) {
  return Number.isFinite(value) && value > 0;
}

export function useDebouncedNumericPreview({
  initialValue,
  onPreviewValue,
  debounceMs = DEFAULT_PREVIEW_DEBOUNCE_MS,
  isValid = isPositiveFiniteNumber,
  immediate = false,
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
    // Never overwrite the input while the user is editing it — the
    // debounced preview fires mid-typing and updates `initialValue`
    // (e.g. the offset/fillet session distance), which would
    // otherwise snap the field back to the stale previewed value
    // and make the panel feel stuck.
    if (document.activeElement === inputRef.current) {
      return;
    }
    setValue(String(initialValue));
    lastPreviewedRef.current = initialValue;
  }, [initialValue]);

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

    if (immediate) {
      if (previewTimerRef.current !== null) {
        window.clearTimeout(previewTimerRef.current);
        previewTimerRef.current = null;
      }
      lastPreviewedRef.current = parsed;
      void onPreviewValueRef.current(parsed);
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
      return null;
    }

    if (parsed === lastPreviewedRef.current) {
      return parsed;
    }

    lastPreviewedRef.current = parsed;
    await onPreviewValueRef.current(parsed);
    return parsed;
  }

  return {
    value,
    inputRef,
    handleValueChange,
    flushPendingValue,
  };
}
