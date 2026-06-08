import type { Dispatch, MutableRefObject, SetStateAction } from "react";
import { SketchFilletPanel } from "../layout";
import type { SketchTool } from "../types";
import type { SketchFilletAction } from "./sketchToolLifecycleEffects";

type RunAction = (action: () => Promise<void>) => Promise<void>;

interface ActiveSketchFilletPanelProps {
  action: SketchFilletAction;
  disabled: boolean;
  sketchFilletIdsRef: MutableRefObject<string[]>;
  setSketchFilletAction: Dispatch<SetStateAction<SketchFilletAction | null>>;
  runAction: RunAction;
  setSketchTool: (tool: SketchTool) => Promise<void>;
  updateSketchFilletRadius: (
    filletId: string,
    radius: number,
  ) => Promise<void>;
  deleteSketchFillet: (filletId: string) => Promise<void>;
}

export function ActiveSketchFilletPanel({
  action,
  disabled,
  sketchFilletIdsRef,
  setSketchFilletAction,
  runAction,
  setSketchTool,
  updateSketchFilletRadius,
  deleteSketchFillet,
}: ActiveSketchFilletPanelProps) {
  return (
    <SketchFilletPanel
      initialValue={action.radius}
      disabled={disabled}
      count={action.phase === "active" ? action.filletIds.length : 0}
      onPreviewValue={async (value) => {
        setSketchFilletAction((prev) =>
          prev ? { ...prev, radius: value } : prev,
        );
        if (action.phase !== "active") {
          return;
        }
        await runAction(async () => {
          for (const filletId of action.filletIds) {
            await updateSketchFilletRadius(filletId, value);
          }
        });
      }}
      onConfirm={async () => {
        sketchFilletIdsRef.current = [];
        setSketchFilletAction(null);
        await runAction(async () => {
          await setSketchTool("select");
        });
      }}
      onCancel={async () => {
        if (action.phase === "active") {
          await runAction(async () => {
            for (const filletId of action.filletIds) {
              await deleteSketchFillet(filletId);
            }
          });
        }
        sketchFilletIdsRef.current = [];
        setSketchFilletAction(null);
        await runAction(async () => {
          await setSketchTool("select");
        });
      }}
    />
  );
}
