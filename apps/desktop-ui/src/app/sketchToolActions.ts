import type { Dispatch, SetStateAction } from "react";

import type { ArmedSketchConstraint, SketchTool } from "../types";
import { handleSketchConstraintLinePickFromContext } from "./sketchConstraintLinePick";
import {
  handleSketchConstraintPointPickFromContext,
  type SketchConstraintPointKind,
} from "./sketchConstraintPointPick";
import type { SketchLineConstraintActions } from "./sketchLineConstraintActions";

type RunAction = (action: () => Promise<void>) => Promise<void>;

interface SketchToolActionContext extends SketchLineConstraintActions {
  armedSketchConstraint: ArmedSketchConstraint;
  setArmedSketchConstraint: Dispatch<SetStateAction<ArmedSketchConstraint>>;
  runAction: RunAction;
  finishSketch: () => Promise<void>;
  restoreTimelineCursorAfterEdit: () => Promise<void>;
  setSketchTool: (tool: SketchTool) => Promise<void>;
  selectSketchEntity: (entityId: string, additive?: boolean) => Promise<void>;
  selectSketchPoint: (
    pointId: string,
    additive?: boolean,
  ) => Promise<void>;
  setSketchCoincidentConstraint: (
    pointId: string,
    otherPointId: string,
  ) => Promise<void>;
  addMessage: (message: string) => void;
}

export type { SketchConstraintPointKind };

export function createSketchToolActions({
  armedSketchConstraint,
  setArmedSketchConstraint,
  runAction,
  finishSketch,
  restoreTimelineCursorAfterEdit,
  setSketchTool,
  selectSketchEntity,
  selectSketchPoint,
  setSketchLineConstraint,
  clearSketchLineConstraints,
  setSketchEqualLengthConstraint,
  setSketchParallelConstraint,
  setSketchPerpendicularConstraint,
  setSketchCoincidentConstraint,
  addMessage,
}: SketchToolActionContext) {
  function clearArmedSketchConstraint() {
    setArmedSketchConstraint(null);
  }

  async function finishActiveSketch() {
    await runAction(async () => {
      clearArmedSketchConstraint();
      await finishSketch();
      await restoreTimelineCursorAfterEdit();
    });
  }

  async function setActiveSketchTool(tool: SketchTool) {
    await runAction(async () => {
      clearArmedSketchConstraint();
      await setSketchTool(tool);
    });
  }

  async function handleSketchConstraintLinePick(
    lineId: string,
    additive = false,
  ) {
    await handleSketchConstraintLinePickFromContext({
      lineId,
      additive,
      armedSketchConstraint,
      selectSketchEntity,
      setSketchLineConstraint,
      clearSketchLineConstraints,
      setSketchEqualLengthConstraint,
      setSketchParallelConstraint,
      setSketchPerpendicularConstraint,
      setArmedSketchConstraint,
    });
  }

  async function handleSketchConstraintPointPick(
    pointId: string,
    kind: SketchConstraintPointKind,
    additive = false,
  ) {
    await handleSketchConstraintPointPickFromContext({
      pointId,
      kind,
      additive,
      armedSketchConstraint,
      selectSketchPoint,
      setSketchCoincidentConstraint,
      setArmedSketchConstraint,
      addMessage,
    });
  }

  return {
    clearArmedSketchConstraint,
    finishActiveSketch,
    handleSketchConstraintLinePick,
    handleSketchConstraintPointPick,
    setActiveSketchTool,
  };
}
