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

// Sketch Chamfer tool session (mirrors the fillet session with two
// distances instead of a radius). Clicks use the session's current
// distances so the panel edits feed the next chamfer.
export type SketchChamferAction =
  | { phase: "pending"; distanceA: number; distanceB: number }
  | {
      phase: "active";
      distanceA: number;
      distanceB: number;
      chamferIds: string[];
    };

// Sketch Slot panel session. Selection-driven (like the text pick
// flow): picking a slot's generated outline in Select mode opens the
// panel bound to that slot. No pending phase — a slot only exists
// after a draft commit.
export type SketchSlotAction = {
  slotId: string;
  params: import("../types").SketchSlotEntry;
};

// Sketch Offset tool session. Pending = tool armed, no offsets yet
// (each entity click creates a copy at the session distance); active
// = one or more offset pairs recorded. Changing the distance
// re-creates every pair at the new value (offsets are non-parametric
// copies, so a change = delete + re-offset each source). Cancel
// deletes every copy; Confirm just closes.
export interface SketchOffsetPair {
  sourceEntityId: string;
  offsetEntityId: string;
}
export type SketchOffsetAction =
  | { phase: "pending"; distance: number }
  | { phase: "active"; distance: number; offsets: SketchOffsetPair[] };

// Sketch Text tool session. Pending = panel is open but no text
// exists yet ("click to place"); active = panel is bound to a text
// entry (freshly created or picked from the sketch in Select mode).
export type SketchTextAction =
  | { phase: "pending" }
  | {
      phase: "active";
      textId: string;
      params: SketchTextEntry;
      // True while the path picker is armed: the next viewport click
      // on a sketch line/arc binds it as the text path instead of
      // placing a new text.
      pathPicking?: boolean;
    };

interface SketchToolLifecycleEffectsContext {
  activeSketchPlaneId: string | null;
  activeSketchTool: SketchTool | null;
  sketchFilletAction: SketchFilletAction | null;
  sketchFilletIdsRef: MutableRefObject<string[]>;
  sketchChamferAction: SketchChamferAction | null;
  sketchChamferIdsRef: MutableRefObject<string[]>;
  sketchOffsetAction: SketchOffsetAction | null;
  sketchTextAction: SketchTextAction | null;
  setTimelineEditVisibleFeatureIds: Dispatch<SetStateAction<Set<string>>>;
  setArmedSketchConstraint: Dispatch<SetStateAction<ArmedSketchConstraint>>;
  setMirrorFocusedSlot: Dispatch<
    SetStateAction<"objects" | "axis" | null>
  >;
  setSketchFilletAction: Dispatch<
    SetStateAction<SketchFilletAction | null>
  >;
  setSketchChamferAction: Dispatch<
    SetStateAction<SketchChamferAction | null>
  >;
  setSketchOffsetAction: Dispatch<SetStateAction<SketchOffsetAction | null>>;
  setSketchTextAction: Dispatch<SetStateAction<SketchTextAction | null>>;
}

export function useSketchToolLifecycleEffects({
  activeSketchPlaneId,
  activeSketchTool,
  sketchFilletAction,
  sketchFilletIdsRef,
  sketchChamferAction,
  sketchChamferIdsRef,
  sketchOffsetAction,
  sketchTextAction,
  setTimelineEditVisibleFeatureIds,
  setArmedSketchConstraint,
  setMirrorFocusedSlot,
  setSketchFilletAction,
  setSketchChamferAction,
  setSketchOffsetAction,
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
      setSketchChamferAction(null);
      setSketchOffsetAction(null);
      setSketchTextAction(null);
      sketchFilletIdsRef.current = [];
      sketchChamferIdsRef.current = [];
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
      activeSketchTool === "chamfer" &&
      activeSketchPlaneId &&
      !sketchChamferAction
    ) {
      setSketchChamferAction({ phase: "pending", distanceA: 5, distanceB: 5 });
      sketchChamferIdsRef.current = [];
      return;
    }
    if (activeSketchTool !== "chamfer" && sketchChamferAction) {
      setSketchChamferAction(null);
      sketchChamferIdsRef.current = [];
    }
  }, [activeSketchTool, activeSketchPlaneId, sketchChamferAction]);

  useEffect(() => {
    if (
      activeSketchTool === "offset" &&
      activeSketchPlaneId &&
      !sketchOffsetAction
    ) {
      setSketchOffsetAction({ phase: "pending", distance: 2 });
      return;
    }
    if (activeSketchTool !== "offset" && sketchOffsetAction) {
      setSketchOffsetAction(null);
    }
  }, [activeSketchTool, activeSketchPlaneId, sketchOffsetAction]);

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
