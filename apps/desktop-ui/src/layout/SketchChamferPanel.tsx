import { useTranslation } from "react-i18next";

import type { ChamferToolMode } from "@/types";

import { PreviewPanelActions } from "./PreviewPanelActions";
import { useDebouncedNumericPreview } from "./hooks/useDebouncedNumericPreview";

interface SketchChamferPanelProps {
  // Which values drive the chamfer geometry. The panel shows only the
  // fields that mode needs:
  //   equal           one distance (both lines)
  //   two_distance    distance along each line
  //   distance_angle  distance along line A + the chamfer's angle to it
  mode: ChamferToolMode;
  // Initial values for the panel session. Used as the defaults for
  // every chamfer created through this session and as the starting
  // values of the inputs.
  initialDistanceA: number;
  initialDistanceB: number;
  initialAngle: number;
  disabled: boolean;
  // Number of chamfers created so far in this panel session. Drives
  // the subtitle and gates the Confirm button (same contract as the
  // fillet panel).
  count: number;
  // Live-preview hook. Called on every debounced numeric change;
  // App fans the new values out across all created chamfers via
  // `update_sketch_chamfer`.
  onPreviewValues: (
    distanceA: number,
    distanceB: number,
    angleDegrees: number,
  ) => Promise<void>;
  onConfirm: () => void | Promise<void>;
  // Cancel = discard the session. App calls `delete_sketch_chamfer`
  // for every chamfer it tracks, restoring each corner.
  onCancel: () => Promise<void>;
}

function isPositiveFiniteNumber(value: number) {
  return Number.isFinite(value) && value > 0;
}

// Contextual modeling floating panel for the 2D sketch Chamfer tool.
// Mirrors `SketchFilletPanel`, with the input set decided by the tool
// mode (equal / two distances / distance + angle).
export function SketchChamferPanel({
  mode,
  initialDistanceA,
  initialDistanceB,
  initialAngle,
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
      await onPreviewValues(
        value,
        mode === "equal" ? value : Number(distanceB.value),
        Number(angle.value),
      );
    },
  });
  const distanceB = useDebouncedNumericPreview({
    initialValue: initialDistanceB,
    onPreviewValue: async (value) => {
      await onPreviewValues(Number(distanceA.value), value, Number(angle.value));
    },
  });
  const angle = useDebouncedNumericPreview({
    initialValue: initialAngle,
    // The core refuses angles at or beyond the corner's own angle; keep
    // the debounced preview from firing (and toasting) while the field is
    // still outside the plainly valid range.
    isValid: (value) => Number.isFinite(value) && value > 0 && value < 180,
    onPreviewValue: async (value) => {
      await onPreviewValues(
        Number(distanceA.value),
        mode === "equal"
          ? Number(distanceA.value)
          : Number(distanceB.value),
        value,
      );
    },
  });

  const parsedA = Number(distanceA.value);
  const parsedB = Number(distanceB.value);
  const parsedAngle = Number(angle.value);
  const valuesValid =
    isPositiveFiniteNumber(parsedA) &&
    (mode !== "two_distance" || isPositiveFiniteNumber(parsedB)) &&
    (mode !== "distance_angle" ||
      (Number.isFinite(parsedAngle) &&
        parsedAngle > 0 &&
        parsedAngle < 180));
  const confirmDisabled = disabled || count === 0 || !valuesValid;

  async function handleConfirm() {
    if (confirmDisabled) {
      return;
    }
    await distanceA.flushPendingValue();
    await distanceB.flushPendingValue();
    await angle.flushPendingValue();
    await onConfirm();
  }

  const distanceField = (
    preview: typeof distanceA,
    label: string,
  ) => (
    <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
      {label}
      <input
        ref={preview.inputRef}
        className="cad-input mt-2"
        type="number"
        min="0.01"
        step="0.01"
        value={preview.value}
        disabled={disabled}
        onChange={(event) => {
          preview.handleValueChange(event.target.value);
        }}
        onKeyDown={(event) => {
          if (event.key === "Escape") {
            event.preventDefault();
            void onCancel();
          }
        }}
      />
    </label>
  );

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
        {distanceField(
          distanceA,
          mode === "equal"
            ? t("panels.sketchChamfer.distance")
            : t("panels.sketchChamfer.distanceA"),
        )}
        {mode === "two_distance" ? distanceField(distanceB, t("panels.sketchChamfer.distanceB")) : null}
        {mode === "distance_angle"
          ? distanceField(angle, t("panels.sketchChamfer.angle"))
          : null}
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
