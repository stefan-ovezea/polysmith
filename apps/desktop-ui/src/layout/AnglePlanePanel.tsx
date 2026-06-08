import { useTranslation } from "react-i18next";

import { NumericPreviewPanel } from "./NumericPreviewPanel";

interface AnglePlanePanelProps {
  phase: "pick_plane" | "pick_axis" | "active";
  initialAngle: number;
  sourceSummary: string;
  axisSummary: string;
  disabled: boolean;
  onPreviewAngle: (angleDegrees: number) => Promise<void> | void;
  onConfirm: () => void | Promise<void>;
  onCancel: () => Promise<void> | void;
}

export function AnglePlanePanel({
  phase,
  initialAngle,
  sourceSummary,
  axisSummary,
  disabled,
  onPreviewAngle,
  onConfirm,
  onCancel,
}: AnglePlanePanelProps) {
  const { t } = useTranslation();

  async function handleConfirm() {
    await onConfirm();
  }

  const helperText =
    phase === "pick_plane"
      ? t("panels.anglePlane.pickSource")
      : phase === "pick_axis"
        ? t("panels.anglePlane.pickAxis", { source: sourceSummary })
        : t("panels.anglePlane.fromSource", {
            source: sourceSummary,
            axis: axisSummary,
          });

  return (
    <NumericPreviewPanel
      title={t("panels.anglePlane.title")}
      helperText={helperText}
      valueLabel={t("forms.angleDegrees")}
      initialValue={initialAngle}
      disabled={disabled}
      canConfirm={phase === "active"}
      inputStep="1"
      isPreviewValueValid={Number.isFinite}
      isConfirmValueValid={Number.isFinite}
      onPreviewValue={onPreviewAngle}
      onConfirm={handleConfirm}
      onCancel={onCancel}
    />
  );
}
