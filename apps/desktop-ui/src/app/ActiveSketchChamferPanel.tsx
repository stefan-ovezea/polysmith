import type { Dispatch, MutableRefObject, SetStateAction } from "react";

import type { ChamferToolMode } from "../types";
import { SketchChamferPanel } from "../layout";
import type { SketchTool } from "../types";
import type { SketchChamferAction } from "./sketchToolLifecycleEffects";

type RunAction = (action: () => Promise<void>) => Promise<void>;

interface ActiveSketchChamferPanelProps {
  action: SketchChamferAction;
  disabled: boolean;
  sketchChamferIdsRef: MutableRefObject<string[]>;
  setSketchChamferAction: Dispatch<
    SetStateAction<SketchChamferAction | null>
  >;
  runAction: RunAction;
  setSketchTool: (tool: SketchTool) => Promise<void>;
  updateSketchChamfer: (
    chamferId: string,
    distanceA: number,
    distanceB: number,
    mode?: ChamferToolMode,
    angleDegrees?: number,
  ) => Promise<void>;
  deleteSketchChamfer: (chamferId: string) => Promise<void>;
}

export function ActiveSketchChamferPanel({
  action,
  disabled,
  sketchChamferIdsRef,
  setSketchChamferAction,
  runAction,
  setSketchTool,
  updateSketchChamfer,
  deleteSketchChamfer,
}: ActiveSketchChamferPanelProps) {
  return (
    <SketchChamferPanel
      mode={action.mode}
      initialDistanceA={action.distanceA}
      initialDistanceB={action.distanceB}
      initialAngle={action.angleDegrees}
      disabled={disabled}
      count={action.phase === "active" ? action.chamferIds.length : 0}
      onPreviewValues={async (distanceA, distanceB, angleDegrees) => {
        setSketchChamferAction((prev) =>
          prev
            ? { ...prev, distanceA, distanceB, angleDegrees }
            : prev,
        );
        if (action.phase !== "active") {
          return;
        }
        await runAction(async () => {
          for (const chamferId of action.chamferIds) {
            await updateSketchChamfer(
              chamferId,
              distanceA,
              distanceB,
              action.mode,
              angleDegrees,
            );
          }
        });
      }}
      onConfirm={async () => {
        sketchChamferIdsRef.current = [];
        setSketchChamferAction(null);
        await runAction(async () => {
          await setSketchTool("select");
        });
      }}
      onCancel={async () => {
        if (action.phase === "active") {
          await runAction(async () => {
            for (const chamferId of action.chamferIds) {
              await deleteSketchChamfer(chamferId);
            }
          });
        }
        sketchChamferIdsRef.current = [];
        setSketchChamferAction(null);
        await runAction(async () => {
          await setSketchTool("select");
        });
      }}
    />
  );
}
