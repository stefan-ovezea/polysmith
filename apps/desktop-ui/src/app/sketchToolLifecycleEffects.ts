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
  setSketchTextAction: Dispatch<SetStateAction<SketchTextAction | null>>;
}

export function useSketchToolLifecycleEffects({
  activeSketchPlaneId,
  activeSketchTool,
  sketchFilletAction,
  sketchFilletIdsRef,
  sketchChamferAction,
  sketchChamferIdsRef,
  sketchTextAction,
  setTimelineEditVisibleFeatureIds,
  setArmedSketchConstraint,
  setMirrorFocusedSlot,
  setSketchFilletAction,
  setSketchChamferAction,
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
