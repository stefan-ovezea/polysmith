import type { Dispatch, SetStateAction } from "react";
import { SketchOffsetPanel } from "../layout";
import type { SketchTool } from "../types";
import type { SketchOffsetAction } from "./sketchToolLifecycleEffects";

type RunAction = (action: () => Promise<void>) => Promise<void>;

interface ActiveSketchOffsetPanelProps {
  action: SketchOffsetAction;
  disabled: boolean;
  setSketchOffsetAction: Dispatch<SetStateAction<SketchOffsetAction | null>>;
  runAction: RunAction;
  setSketchTool: (tool: SketchTool) => Promise<void>;
  deleteSketchSelection: (
    entityIds: string[],
    pointIds: string[],
    profileIds: string[],
  ) => Promise<void>;
  // Live distance fan-out. App deletes the session's copies and
  // re-creates each from its source at the new value (offsets are
  // non-parametric), or just updates the session when nothing has
  // been created yet.
  onDistanceChange: (distance: number) => Promise<void>;
}

export function ActiveSketchOffsetPanel({
  action,
  disabled,
  setSketchOffsetAction,
  runAction,
  setSketchTool,
  deleteSketchSelection,
  onDistanceChange,
}: ActiveSketchOffsetPanelProps) {
  return (
    <SketchOffsetPanel
      initialValue={action.distance}
      disabled={disabled}
      count={action.phase === "active" ? action.offsets.length : 0}
      onPreviewValue={onDistanceChange}
      onConfirm={async () => {
        setSketchOffsetAction(null);
        await runAction(async () => {
          await setSketchTool("select");
        });
      }}
      onCancel={async () => {
        if (action.phase === "active" && action.offsets.length > 0) {
          await runAction(async () => {
            await deleteSketchSelection(
              action.offsets.map((pair) => pair.offsetEntityId),
              [],
              [],
            );
          });
        }
        setSketchOffsetAction(null);
        await runAction(async () => {
          await setSketchTool("select");
        });
      }}
    />
  );
}
