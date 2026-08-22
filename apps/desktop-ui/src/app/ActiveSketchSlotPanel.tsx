import type { Dispatch, SetStateAction } from "react";
import { SketchSlotPanel } from "../layout";
import type { SketchSlotEntry, SketchTool } from "../types";
import type { SketchSlotAction } from "./sketchToolLifecycleEffects";

type RunAction = (action: () => Promise<void>) => Promise<void>;

interface ActiveSketchSlotPanelProps {
  action: SketchSlotAction;
  disabled: boolean;
  setSketchSlotAction: Dispatch<SetStateAction<SketchSlotAction | null>>;
  runAction: RunAction;
  setSketchTool: (tool: SketchTool) => Promise<void>;
  updateSketchSlot: (
    slotId: string,
    centerX: number,
    centerY: number,
    length: number,
    radius: number,
    rotation: number,
  ) => Promise<void>;
}

export function ActiveSketchSlotPanel({
  action,
  disabled,
  setSketchSlotAction,
  runAction,
  setSketchTool,
  updateSketchSlot,
}: ActiveSketchSlotPanelProps) {
  const params: SketchSlotEntry = action.params;
  return (
    <SketchSlotPanel
      slot={params}
      disabled={disabled}
      onUpdate={async (length, radius, rotation) => {
        await runAction(async () => {
          await updateSketchSlot(
            params.slot_id,
            params.center_x,
            params.center_y,
            length,
            radius,
            rotation,
          );
        });
      }}
      onConfirm={async () => {
        setSketchSlotAction(null);
        await runAction(async () => {
          await setSketchTool("select");
        });
      }}
      onCancel={async () => {
        // Close only — the slot survives (mirrors the text panel's
        // cancel semantics are intentionally NOT copied here; a
        // picked slot is existing geometry, not a pending creation).
        setSketchSlotAction(null);
      }}
    />
  );
}
