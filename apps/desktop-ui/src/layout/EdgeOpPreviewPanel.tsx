import { useTranslation } from "react-i18next";

import { NumericPreviewPanel } from "./NumericPreviewPanel";

interface EdgeOpPreviewPanelProps {
  // "Fillet" or "Chamfer" — the only label that differs between the
  // two, so we keep the panel itself shared.
  title: string;
  // The numeric input label, e.g. "Radius (mm)" or "Distance (mm)".
  valueLabel: string;
  initialValue: number;
  disabled: boolean;
  // Live count of edges currently in the feature, so the user sees
  // the picker react as they shift-click in the viewport. The panel
  // doesn't drive the count itself — it just reflects the document.
  edgeCount: number;
  onPreviewValue: (value: number) => Promise<void>;
  onConfirm: () => void | Promise<void>;
  onCancel: () => Promise<void>;
}

// Floating contextual modeling "Edit Feature" panel for the in-progress fillet
// or chamfer. The native core has already created the feature with the
// initial value, so the viewport is showing a real preview. Typing here
// drives update_fillet_radius / update_chamfer_distance for live
// updates; Enter/Confirm closes; Escape/Cancel undoes.
export function EdgeOpPreviewPanel({
  title,
  valueLabel,
  initialValue,
  disabled,
  edgeCount,
  onPreviewValue,
  onConfirm,
  onCancel,
}: EdgeOpPreviewPanelProps) {
  const { t } = useTranslation();

  async function handleConfirm() {
    await onConfirm();
  }

  return (
    <NumericPreviewPanel
      title={title}
      helperText={t("panels.edgeOp.edgePicker", {
        count: edgeCount,
        plural: edgeCount === 1 ? "" : "s",
      })}
      valueLabel={valueLabel}
      initialValue={initialValue}
      disabled={disabled}
      inputMin="0.01"
      inputStep="0.01"
      onPreviewValue={onPreviewValue}
      onConfirm={handleConfirm}
      onCancel={onCancel}
    />
  );
}
