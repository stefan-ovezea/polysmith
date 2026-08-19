import { useEffect } from "react";
import type { Dispatch, MutableRefObject, SetStateAction } from "react";

import type {
  ArmedSketchConstraint,
  SketchTextEntry,
  SketchTool,
} from "../types";

export type SketchFilletAction =
  | { phase: "pending"; radius: number }
  | { phase: "active"; radius: number; filletIds: string[] };

// Sketch Text tool session. Pending = panel is open but no text
// exists yet ("click to place"); active = panel is bound to a text
// entry (freshly created or picked from the sketch in Select mode).
export type SketchTextAction =
  | { phase: "pending" }
  | { phase: "active"; textId: string; params: SketchTextEntry };

interface SketchToolLifecycleEffectsContext {
  activeSketchPlaneId: string | null;
  activeSketchTool: SketchTool | null;
  sketchFilletAction: SketchFilletAction | null;
  sketchFilletIdsRef: MutableRefObject<string[]>;
  sketchTextAction: SketchTextAction | null;
  setTimelineEditVisibleFeatureIds: Dispatch<SetStateAction<Set<string>>>;
  setArmedSketchConstraint: Dispatch<SetStateAction<ArmedSketchConstraint>>;
  setMirrorFocusedSlot: Dispatch<
    SetStateAction<"objects" | "axis" | null>
  >;
  setSketchFilletAction: Dispatch<
    SetStateAction<SketchFilletAction | null>
  >;
  setSketchTextAction: Dispatch<SetStateAction<SketchTextAction | null>>;
}

export function useSketchToolLifecycleEffects({
  activeSketchPlaneId,
  activeSketchTool,
  sketchFilletAction,
  sketchFilletIdsRef,
  sketchTextAction,
  setTimelineEditVisibleFeatureIds,
  setArmedSketchConstraint,
  setMirrorFocusedSlot,
  setSketchFilletAction,
  setSketchTextAction,
}: SketchToolLifecycleEffectsContext) {
  useEffect(() => {
    if (!activeSketchPlaneId) {
      setTimelineEditVisibleFeatureIds((current) =>
        current.size === 0 ? current : new Set<string>(),
      );
      setArmedSketchConstraint(null);
      setMirrorFocusedSlot(null);
      setSketchFilletAction(null);
      setSketchTextAction(null);
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

  useEffect(() => {
    if (
      activeSketchTool === "text" &&
      activeSketchPlaneId &&
      !sketchTextAction
    ) {
      setSketchTextAction({ phase: "pending" });
      return;
    }
    if (activeSketchTool !== "text" && sketchTextAction) {
      setSketchTextAction(null);
    }
  }, [activeSketchTool, activeSketchPlaneId, sketchTextAction]);
}
