import { useEffect } from "react";
import type { Dispatch, MutableRefObject, SetStateAction } from "react";

import type { ArmedSketchConstraint, SketchTool } from "../types";

export type SketchFilletAction =
  | { phase: "pending"; radius: number }
  | { phase: "active"; radius: number; filletIds: string[] };

interface SketchToolLifecycleEffectsContext {
  activeSketchPlaneId: string | null;
  activeSketchTool: SketchTool | null;
  sketchFilletAction: SketchFilletAction | null;
  sketchFilletIdsRef: MutableRefObject<string[]>;
  setTimelineEditVisibleFeatureIds: Dispatch<SetStateAction<Set<string>>>;
  setArmedSketchConstraint: Dispatch<SetStateAction<ArmedSketchConstraint>>;
  setMirrorFocusedSlot: Dispatch<
    SetStateAction<"objects" | "axis" | null>
  >;
  setSketchFilletAction: Dispatch<
    SetStateAction<SketchFilletAction | null>
  >;
}

export function useSketchToolLifecycleEffects({
  activeSketchPlaneId,
  activeSketchTool,
  sketchFilletAction,
  sketchFilletIdsRef,
  setTimelineEditVisibleFeatureIds,
  setArmedSketchConstraint,
  setMirrorFocusedSlot,
  setSketchFilletAction,
}: SketchToolLifecycleEffectsContext) {
  useEffect(() => {
    if (!activeSketchPlaneId) {
      setTimelineEditVisibleFeatureIds((current) =>
        current.size === 0 ? current : new Set<string>(),
      );
      setArmedSketchConstraint(null);
      setMirrorFocusedSlot(null);
      setSketchFilletAction(null);
      sketchFilletIdsRef.current = [];
    }
  }, [activeSketchPlaneId]);

  useEffect(() => {
    if (
      activeSketchTool === "fillet" &&
      activeSketchPlaneId &&
      !sketchFilletAction
    ) {
      setSketchFilletAction({ phase: "pending", radius: 5 });
      sketchFilletIdsRef.current = [];
      return;
    }
    if (activeSketchTool !== "fillet" && sketchFilletAction) {
      setSketchFilletAction(null);
      sketchFilletIdsRef.current = [];
    }
  }, [activeSketchTool, activeSketchPlaneId, sketchFilletAction]);
}
