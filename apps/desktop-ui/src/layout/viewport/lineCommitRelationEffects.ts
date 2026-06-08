import { useEffect, type MutableRefObject } from "react";

import type { SketchFeatureParameters } from "@/types";
import {
  applyPendingLineCommitRelations,
  type PendingLineCommitRelationRefs,
} from "./lineCommitRelations";

interface UsePendingLineCommitRelationsArgs {
  sketchParameters: SketchFeatureParameters | null | undefined;
  sketchLinesRef: MutableRefObject<SketchFeatureParameters | null>;
  sketchLineCountRef: MutableRefObject<number>;
  pendingRefs: PendingLineCommitRelationRefs;
  actionRefs: {
    setSketchMidpointAnchorRef: MutableRefObject<
      (pointId: string, hostLineId: string) => Promise<void>
    >;
    setSketchPerpendicularConstraintRef: MutableRefObject<
      (lineId: string, otherLineId: string | null) => Promise<void>
    >;
    setSketchPointLineAnchorRef: MutableRefObject<
      (pointId: string, hostLineId: string, t: number) => Promise<void>
    >;
    setSketchLineConstraintRef: MutableRefObject<
      (
        lineId: string,
        constraint: "none" | "horizontal" | "vertical",
      ) => Promise<void>
    >;
    setSketchTangentConstraintRef: MutableRefObject<
      (lineId: string, circleId: string) => Promise<void>
    >;
    setSketchParallelConstraintRef: MutableRefObject<
      (lineId: string, otherLineId: string | null) => Promise<void>
    >;
  };
}

export function usePendingLineCommitRelations({
  sketchParameters,
  sketchLinesRef,
  sketchLineCountRef,
  pendingRefs,
  actionRefs,
}: UsePendingLineCommitRelationsArgs) {
  const {
    midpointAnchor,
    perpendicularConstraint,
    pointLineAnchor,
    axisConstraint,
    tangentConstraint,
    parallelConstraint,
  } = pendingRefs;
  const {
    setSketchMidpointAnchorRef,
    setSketchPerpendicularConstraintRef,
    setSketchPointLineAnchorRef,
    setSketchLineConstraintRef,
    setSketchTangentConstraintRef,
    setSketchParallelConstraintRef,
  } = actionRefs;

  useEffect(() => {
    const params = sketchParameters ?? null;
    sketchLinesRef.current = params;
    const newCount = params?.lines.length ?? 0;
    const previousCount = sketchLineCountRef.current;
    sketchLineCountRef.current = newCount;

    applyPendingLineCommitRelations({
      sketchParameters: params,
      previousLineCount: previousCount,
      currentLineCount: newCount,
      refs: {
        midpointAnchor,
        perpendicularConstraint,
        pointLineAnchor,
        axisConstraint,
        tangentConstraint,
        parallelConstraint,
      },
      actions: {
        setSketchMidpointAnchor: setSketchMidpointAnchorRef.current,
        setSketchPerpendicularConstraint:
          setSketchPerpendicularConstraintRef.current,
        setSketchPointLineAnchor: setSketchPointLineAnchorRef.current,
        setSketchLineConstraint: setSketchLineConstraintRef.current,
        setSketchTangentConstraint: setSketchTangentConstraintRef.current,
        setSketchParallelConstraint: setSketchParallelConstraintRef.current,
      },
    });
  }, [
    axisConstraint,
    midpointAnchor,
    parallelConstraint,
    perpendicularConstraint,
    pointLineAnchor,
    setSketchLineConstraintRef,
    setSketchMidpointAnchorRef,
    setSketchParallelConstraintRef,
    setSketchPerpendicularConstraintRef,
    setSketchPointLineAnchorRef,
    setSketchTangentConstraintRef,
    sketchLineCountRef,
    sketchLinesRef,
    sketchParameters,
    tangentConstraint,
  ]);
}
