import type { Dispatch, SetStateAction } from "react";
import { MirrorToolPanel } from "../layout";
import type { PendingMirrorEntry } from "../types";

type RunAction = (action: () => Promise<void>) => Promise<void>;
type MirrorFocusedSlot = "objects" | "axis" | null;

interface ActiveMirrorPanelProps {
  disabled: boolean;
  focusedSlot: MirrorFocusedSlot;
  pendingMirror: PendingMirrorEntry | null;
  persistent: boolean;
  runAction: RunAction;
  setFocusedSlot: Dispatch<SetStateAction<MirrorFocusedSlot>>;
  setPersistent: Dispatch<SetStateAction<boolean>>;
  cancelMirrorPreview: () => Promise<void>;
  commitMirrorPreview: (persistent: boolean) => Promise<void>;
  updateMirrorPreviewAxis: (axisLineId: string | null) => Promise<void>;
  updateMirrorPreviewObjects: (objectIds: string[]) => Promise<void>;
}

export function ActiveMirrorPanel({
  disabled,
  focusedSlot,
  pendingMirror,
  persistent,
  runAction,
  setFocusedSlot,
  setPersistent,
  cancelMirrorPreview,
  commitMirrorPreview,
  updateMirrorPreviewAxis,
  updateMirrorPreviewObjects,
}: ActiveMirrorPanelProps) {
  if (!pendingMirror) {
    return null;
  }

  return (
    <MirrorToolPanel
      axisLineId={pendingMirror.axis_line_id}
      objectIds={pendingMirror.object_ids}
      generatedLineCount={pendingMirror.generated_lines.length}
      generatedCircleCount={pendingMirror.generated_circles.length}
      focusedSlot={focusedSlot}
      persistent={persistent}
      onTogglePersistent={() => setPersistent((value) => !value)}
      disabled={disabled}
      onFocusObjects={() => setFocusedSlot("objects")}
      onFocusAxis={() => setFocusedSlot("axis")}
      onClearObjects={async () => {
        await runAction(async () => {
          await updateMirrorPreviewObjects([]);
        });
      }}
      onClearAxis={async () => {
        await runAction(async () => {
          await updateMirrorPreviewAxis(null);
        });
      }}
      onConfirm={async () => {
        await runAction(async () => {
          await commitMirrorPreview(persistent);
        });
        setFocusedSlot(null);
      }}
      onCancel={async () => {
        await runAction(async () => {
          await cancelMirrorPreview();
        });
        setFocusedSlot(null);
      }}
    />
  );
}
