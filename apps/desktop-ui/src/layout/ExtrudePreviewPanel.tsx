import { useEffect, useRef, useState } from "react";

const PREVIEW_DEBOUNCE_MS = 200;

interface ExtrudePreviewPanelProps {
  initialDepth: number;
  disabled: boolean;
  onPreviewDepth: (depth: number) => Promise<void>;
  onConfirm: () => void;
  onCancel: () => Promise<void>;
}

// Floating Fusion-style "Edit Feature" panel for the in-progress extrude.
// The native core already created the extrude with the initial depth, so the
// viewport is showing a real preview. Typing here drives update_extrude_depth
// for live updates; Enter/Confirm closes; Escape/Cancel undoes.
export function ExtrudePreviewPanel({
  initialDepth,
  disabled,
  onPreviewDepth,
  onConfirm,
  onCancel,
}: ExtrudePreviewPanelProps) {
  const [depth, setDepth] = useState(String(initialDepth));
  const inputRef = useRef<HTMLInputElement | null>(null);
  const previewTimerRef = useRef<number | null>(null);
  const lastPreviewedRef = useRef<number>(initialDepth);
  const onPreviewDepthRef = useRef(onPreviewDepth);

  useEffect(() => {
    onPreviewDepthRef.current = onPreviewDepth;
  }, [onPreviewDepth]);

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

  function handleDepthChange(nextValue: string) {
    setDepth(nextValue);
    const parsed = Number(nextValue);
    if (!Number.isFinite(parsed) || parsed <= 0) {
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
      void onPreviewDepthRef.current(parsed);
    }, PREVIEW_DEBOUNCE_MS);
  }

  // Force-commit the current input value to the core. Used on Confirm so
  // pressing Enter while the debounce timer is still pending does not close
  // the panel before the typed depth has reached the core.
  async function flushPendingDepth() {
    if (previewTimerRef.current !== null) {
      window.clearTimeout(previewTimerRef.current);
      previewTimerRef.current = null;
    }

    const parsed = Number(depth);
    if (!Number.isFinite(parsed) || parsed <= 0) {
      return;
    }

    if (parsed === lastPreviewedRef.current) {
      return;
    }

    lastPreviewedRef.current = parsed;
    await onPreviewDepthRef.current(parsed);
  }

  async function handleConfirm() {
    await flushPendingDepth();
    onConfirm();
  }

  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5">
      <p className="cad-kicker">Action</p>
      <h2 className="cad-title mt-2">Extrude</h2>
      <form
        className="mt-4 space-y-4"
        onSubmit={(event) => {
          event.preventDefault();
          void handleConfirm();
        }}
      >
        <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
          Depth (mm)
          <input
            ref={inputRef}
            className="cad-input mt-2"
            type="number"
            min="0.01"
            step="0.01"
            value={depth}
            disabled={disabled}
            onChange={(event) => {
              handleDepthChange(event.target.value);
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
            disabled={disabled || Number(depth) <= 0}
          >
            Confirm
          </button>
          <button
            type="button"
            className="cad-action-ghost flex-1"
            disabled={disabled}
            onClick={() => {
              void onCancel();
            }}
          >
            Cancel
          </button>
        </div>
        <p className="text-[11px] uppercase tracking-[0.16em] text-on-surface-dim">
          Enter to confirm · Esc to cancel
        </p>
      </form>
    </section>
  );
}
