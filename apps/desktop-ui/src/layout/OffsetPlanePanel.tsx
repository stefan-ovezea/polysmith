import { useTranslation } from "react-i18next";

import { NumericPreviewPanel } from "./NumericPreviewPanel";

interface OffsetPlanePanelProps {
  // True while the user hasn't picked a source plane yet. The panel
  // shows the offset input but no live preview is happening; the
  // typed value is captured in a ref so the next plane click creates
  // the feature with the latest value. Mirrors the
  // `EdgeOpPreviewPanel` pending-phase pattern.
  isPending: boolean;
  initialOffset: number;
  // Human-friendly description of what the user just clicked
  // (e.g. "XY plane", "Top face"). Empty during the pending phase.
  // Never an internal id — see AGENTS.md UI Copy Rules.
  sourceSummary: string;
  disabled: boolean;
  // Fires (debounced) on every typed value. During the pending phase
  // the parent stashes it in a ref; during the active phase it
  // dispatches `update_offset_plane` for live preview.
  onPreviewOffset: (offset: number) => Promise<void> | void;
  onConfirm: () => void | Promise<void>;
  onCancel: () => Promise<void> | void;
}

// Floating contextual modeling "Offset Plane" panel. Two phases:
//
//   * Pending: panel is open, no feature yet. The user picks a plane
//     in the viewport; the parent's click handler reads the typed
//     offset and dispatches `create_offset_plane`. Enter / Confirm is
//     a no-op until a source has been picked.
//   * Active: feature exists in the document; typing here drives
//     `update_offset_plane` (debounced) for live preview. Enter
//     confirms; Escape calls `undo` to drop the feature.
export function OffsetPlanePanel({
  isPending,
  initialOffset,
  sourceSummary,
  disabled,
  onPreviewOffset,
  onConfirm,
  onCancel,
}: OffsetPlanePanelProps) {
  const { t } = useTranslation();

  async function handleConfirm() {
    await onConfirm();
  }

  return (
    <NumericPreviewPanel
      title={t("panels.offsetPlane.title")}
      helperText={
        isPending
          ? t("panels.offsetPlane.pickSource")
          : sourceSummary
            ? t("panels.offsetPlane.fromSource", { source: sourceSummary })
            : t("panels.offsetPlane.adjustOffset")
      }
      valueLabel={t("forms.offsetMm")}
      initialValue={initialOffset}
      disabled={disabled}
      canConfirm={!isPending}
      inputStep="0.1"
      isPreviewValueValid={Number.isFinite}
      isConfirmValueValid={Number.isFinite}
      onPreviewValue={onPreviewOffset}
      onConfirm={handleConfirm}
      onCancel={onCancel}
    />
  );
}
