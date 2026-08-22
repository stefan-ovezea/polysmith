import { useTranslation } from "react-i18next";

import { PreviewPanelActions } from "./PreviewPanelActions";
import { useDebouncedNumericPreview } from "./hooks/useDebouncedNumericPreview";

interface SketchChamferPanelProps {
  // Initial trim distances for the panel session. Used as the
  // default for every chamfer created through this session and as
  // the starting values of the two inputs.
  initialDistanceA: number;
  initialDistanceB: number;
  disabled: boolean;
  // Number of chamfers created so far in this panel session. Drives
  // the subtitle and gates the Confirm button (same contract as the
  // fillet panel).
  count: number;
  // Live-preview hook. Called on every debounced numeric change;
  // App fans the new distances out across all created chamfers via
  // `update_sketch_chamfer`.
  onPreviewValues: (distanceA: number, distanceB: number) => Promise<void>;
  onConfirm: () => void | Promise<void>;
  // Cancel = discard the session. App calls `delete_sketch_chamfer`
  // for every chamfer it tracks, restoring each corner.
  onCancel: () => Promise<void>;
}

function isPositiveFiniteNumber(value: number) {
  return Number.isFinite(value) && value > 0;
}

// Contextual modeling floating panel for the 2D sketch Chamfer tool.
// Mirrors `SketchFilletPanel` with two trim distances instead of one
// radius.
export function SketchChamferPanel({
  initialDistanceA,
  initialDistanceB,
  disabled,
  count,
  onPreviewValues,
  onConfirm,
  onCancel,
}: SketchChamferPanelProps) {
  const { t } = useTranslation();
  const distanceA = useDebouncedNumericPreview({
    initialValue: initialDistanceA,
    onPreviewValue: async (value) => {
      await onPreviewValues(value, Number(distanceB.value));
    },
  });
  const distanceB = useDebouncedNumericPreview({
    initialValue: initialDistanceB,
    onPreviewValue: async (value) => {
      await onPreviewValues(Number(distanceA.value), value);
    },
  });

  const parsedA = Number(distanceA.value);
  const parsedB = Number(distanceB.value);
  const confirmDisabled =
    disabled ||
    count === 0 ||
    !isPositiveFiniteNumber(parsedA) ||
    !isPositiveFiniteNumber(parsedB);

  async function handleConfirm() {
    if (confirmDisabled) {
      return;
    }
    await distanceA.flushPendingValue();
    await distanceB.flushPendingValue();
    await onConfirm();
  }

  return (
    <section className="pointer-events-auto cad-floating-panel px-5 py-5">
      <p className="cad-kicker">{t("panels.sketchChamfer.title")}</p>
      <p className="mt-3 text-xs text-on-surface-muted">
        {count === 0
          ? t("panels.sketchChamfer.clickCorner")
          : t("panels.sketchChamfer.addAnother", {
              count,
              plural: count === 1 ? "" : "s",
            })}
      </p>
      <form
        className="mt-4 space-y-4"
        onSubmit={(event) => {
          event.preventDefault();
          void handleConfirm();
        }}
      >
        <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
          {t("panels.sketchChamfer.distanceA")}
          <input
            ref={distanceA.inputRef}
            className="cad-input mt-2"
            type="number"
            min="0.01"
            step="0.01"
            value={distanceA.value}
            disabled={disabled}
            onChange={(event) => {
              distanceA.handleValueChange(event.target.value);
            }}
            onKeyDown={(event) => {
              if (event.key === "Escape") {
                event.preventDefault();
                void onCancel();
              }
            }}
          />
        </label>
        <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
          {t("panels.sketchChamfer.distanceB")}
          <input
            ref={distanceB.inputRef}
            className="cad-input mt-2"
            type="number"
            min="0.01"
            step="0.01"
            value={distanceB.value}
            disabled={disabled}
            onChange={(event) => {
              distanceB.handleValueChange(event.target.value);
            }}
            onKeyDown={(event) => {
              if (event.key === "Escape") {
                event.preventDefault();
                void onCancel();
              }
            }}
          />
        </label>
        <PreviewPanelActions
          confirmDisabled={confirmDisabled}
          cancelDisabled={disabled}
          onCancel={() => {
            void onCancel();
          }}
        />
        <p className="text-[11px] uppercase tracking-[0.16em] text-on-surface-dim">
          {t("panels.shortcutHint.confirm")}
        </p>
      </form>
    </section>
  );
}
