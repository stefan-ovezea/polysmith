import { useTranslation } from "react-i18next";

import { NumericPreviewPanel } from "./NumericPreviewPanel";

interface SketchFilletPanelProps {
  // Initial radius for the panel session. Used as the default for
  // every fillet created through this session and as the starting
  // value of the radius input.
  initialValue: number;
  disabled: boolean;
  // Number of fillets created so far in this panel session. Drives
  // the subtitle (so the user sees the picker react as they click
  // corners) and gates the Confirm button: with no fillets
  // created, Confirm is a no-op and disabled, matching the
  // `EdgeOpPreviewPanel` (3D fillet) contract.
  count: number;
  // Live-preview hook. Called on every debounced numeric change;
  // App is responsible for fanning the new radius out across all
  // created fillets via `update_sketch_fillet_radius`.
  onPreviewValue: (value: number) => Promise<void>;
  onConfirm: () => void | Promise<void>;
  // Cancel = discard the session. App calls
  // `delete_sketch_fillet` for every fillet it tracks, restoring
  // each corner.
  onCancel: () => Promise<void>;
}

// contextual modeling floating panel for the 2D sketch Fillet tool.
// Mirrors `EdgeOpPreviewPanel` (3D fillet/chamfer) one-to-one:
// pending phase (count === 0) prompts the user to click a corner;
// each click adds a fillet at the panel's current radius; the
// numeric input drives a debounced fan-out update across every
// fillet in the session.
export function SketchFilletPanel({
  initialValue,
  disabled,
  count,
  onPreviewValue,
  onConfirm,
  onCancel,
}: SketchFilletPanelProps) {
  const { t } = useTranslation();

  async function handleConfirm() {
    await onConfirm();
  }

  return (
    <NumericPreviewPanel
      title={t("panels.sketchFillet.title")}
      helperText={
        count === 0
          ? t("panels.sketchFillet.clickCorner")
          : t("panels.sketchFillet.addAnother", {
              count,
              plural: count === 1 ? "" : "s",
            })
      }
      valueLabel={t("forms.radiusMm")}
      initialValue={initialValue}
      disabled={disabled}
      canConfirm={count > 0}
      inputMin="0.01"
      inputStep="0.01"
      onPreviewValue={onPreviewValue}
      onConfirm={handleConfirm}
      onCancel={onCancel}
    />
  );
}
