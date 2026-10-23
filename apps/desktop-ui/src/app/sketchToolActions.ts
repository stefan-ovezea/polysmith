import type { Dispatch, SetStateAction } from "react";

import type { ArmedSketchConstraint, SketchTool } from "../types";
import { handleSketchConstraintLinePickFromContext } from "./sketchConstraintLinePick";
import {
  handleSketchConstraintVertexPickFromContext,
  type SketchConstraintVertexKind,
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
  /** True for a line entity id (false for circles/arcs) — the tangent and
   *  tangent-pair flows need the distinction. */
  isLineEntity: (entityId: string) => boolean;
  selectSketchPoint: (
    pointId: string,
    additive?: boolean,
  ) => Promise<void>;
  setSketchPointFixed: (pointId: string, isFixed: boolean) => Promise<void>;
  setSketchCoincidentConstraint: (
    pointId: string,
    otherPointId: string,
  ) => Promise<void>;
  addMessage: (message: string) => void;
}

export type { SketchConstraintVertexKind };

export function createSketchToolActions({
  armedSketchConstraint,
  setArmedSketchConstraint,
  runAction,
  finishSketch,
  restoreTimelineCursorAfterEdit,
  setSketchTool,
  selectSketchEntity,
  isLineEntity,
  selectSketchPoint,
  setSketchPointFixed,
  setSketchLineConstraint,
  clearSketchLineConstraints,
  setSketchEqualLengthConstraint,
  setSketchParallelConstraint,
  setSketchPerpendicularConstraint,
  setSketchCollinearConstraint,
  setSketchMidpointConstraint,
  setSketchTangentConstraint,
  setSketchTangentPairConstraint,
  setSketchSymmetricConstraint,
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
      isLineEntity,
      armedSketchConstraint,
      selectSketchEntity,
      setSketchLineConstraint,
      clearSketchLineConstraints,
      setSketchEqualLengthConstraint,
      setSketchParallelConstraint,
      setSketchPerpendicularConstraint,
      setSketchCollinearConstraint,
      setSketchMidpointConstraint,
      setSketchTangentConstraint,
      setSketchTangentPairConstraint,
      setSketchSymmetricConstraint,
      setArmedSketchConstraint,
    });
  }

  async function handleSketchConstraintVertexPick(
    pointId: string,
    kind: SketchConstraintVertexKind,
    additive = false,
  ) {
    await handleSketchConstraintVertexPickFromContext({
      vertexId: pointId,
      kind,
      additive,
      armedSketchConstraint,
      selectSketchPoint,
      setSketchPointFixed,
      setSketchCoincidentConstraint,
      setArmedSketchConstraint,
      addMessage,
    });
  }

  return {
    clearArmedSketchConstraint,
    finishActiveSketch,
    handleSketchConstraintLinePick,
    handleSketchConstraintVertexPick,
    setActiveSketchTool,
  };
}
