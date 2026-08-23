import { useTranslation } from "react-i18next";

import { NumericPreviewPanel } from "./NumericPreviewPanel";

interface SketchOffsetPanelProps {
  // Initial signed offset distance for the session. Applies to every
  // entity clicked while the session is open.
  initialValue: number;
  disabled: boolean;
  // Number of offsets created so far. Gates Confirm and drives the
  // subtitle (same contract as the fillet panel).
  count: number;
  onPreviewValue: (value: number) => Promise<void>;
  onConfirm: () => void | Promise<void>;
  // Cancel deletes every offset created in the session.
  onCancel: () => Promise<void>;
}

// Contextual modeling floating panel for the 2D sketch Offset tool.
// Each entity click creates a copy at the panel's current distance;
// the numeric input changes the distance for subsequent clicks.
export function SketchOffsetPanel({
  initialValue,
  disabled,
  count,
  onPreviewValue,
  onConfirm,
  onCancel,
}: SketchOffsetPanelProps) {
  const { t } = useTranslation();

  return (
    <NumericPreviewPanel
      title={t("panels.sketchOffset.title")}
      helperText={
        count === 0
          ? t("panels.sketchOffset.clickEntity")
          : t("panels.sketchOffset.addAnother", {
              count,
              plural: count === 1 ? "" : "s",
            })
      }
      valueLabel={t("forms.distanceMm")}
      initialValue={initialValue}
      disabled={disabled}
      canConfirm={count > 0}
      inputStep="0.1"
      isPreviewValueValid={(value) =>
        Number.isFinite(value) && Math.abs(value) > 0.001
      }
      onPreviewValue={onPreviewValue}
      onConfirm={onConfirm}
      onCancel={onCancel}
    />
  );
}
