import type { SketchFeatureParameters } from "@/types";

type LineBodyHost = { lineId: string; t: number };

interface MutableRef<T> {
  current: T;
}

export interface LineCommitSnapPoint {
  snapMidpointHostLineId?: string | null;
  snapMidpointT?: number | null;
  snapPerpendicularHostLineId?: string | null;
  snapEndpointHostLineId?: string | null;
  snapLineBodyHostLineId?: string | null;
  snapLineBodyT?: number | null;
  snapAxisLock?: "horizontal" | "vertical" | null;
  snapTangentCircleId?: string | null;
  snapParallelHostLineId?: string | null;
}

export interface PendingLineCommitRelations {
  midpointAnchor: {
    fromLineCount: number;
    startHostLineId: string | null;
    endHostLineId: string | null;
  } | null;
  perpendicularConstraint: {
    fromLineCount: number;
    hostLineId: string;
  } | null;
  pointLineAnchor: {
    fromLineCount: number;
    startHost: LineBodyHost | null;
    endHost: LineBodyHost | null;
  } | null;
  axisConstraint: {
    fromLineCount: number;
    kind: "horizontal" | "vertical";
  } | null;
  tangentConstraint: {
    fromLineCount: number;
    circleId: string;
  } | null;
  parallelConstraint: {
    fromLineCount: number;
    hostLineId: string;
  } | null;
  endHostLineId: string | null;
  endLineBodyHost: LineBodyHost | null;
}

export interface PendingLineCommitRelationRefs {
  midpointAnchor: MutableRef<PendingLineCommitRelations["midpointAnchor"]>;
  perpendicularConstraint: MutableRef<
    PendingLineCommitRelations["perpendicularConstraint"]
  >;
  pointLineAnchor: MutableRef<PendingLineCommitRelations["pointLineAnchor"]>;
  axisConstraint: MutableRef<PendingLineCommitRelations["axisConstraint"]>;
  tangentConstraint: MutableRef<PendingLineCommitRelations["tangentConstraint"]>;
  parallelConstraint: MutableRef<PendingLineCommitRelations["parallelConstraint"]>;
}

export interface PendingLineCommitRelationActions {
  setSketchMidpointAnchor: (
    pointId: string,
    hostLineId: string,
  ) => void | Promise<void>;
  setSketchPerpendicularConstraint: (
    lineId: string,
    hostLineId: string,
  ) => void | Promise<void>;
  setSketchPointLineAnchor: (
    pointId: string,
    hostLineId: string,
    t: number,
  ) => void | Promise<void>;
  setSketchLineConstraint: (
    lineId: string,
    kind: "horizontal" | "vertical",
  ) => void | Promise<void>;
  setSketchTangentConstraint: (
    lineId: string,
    circleId: string,
  ) => void | Promise<void>;
  setSketchParallelConstraint: (
    lineId: string,
    hostLineId: string,
  ) => void | Promise<void>;
}

type PendingLineCommitRelationSnapshot = {
  midpointAnchor: PendingLineCommitRelations["midpointAnchor"];
  perpendicularConstraint: PendingLineCommitRelations["perpendicularConstraint"];
  pointLineAnchor: PendingLineCommitRelations["pointLineAnchor"];
  axisConstraint: PendingLineCommitRelations["axisConstraint"];
  tangentConstraint: PendingLineCommitRelations["tangentConstraint"];
  parallelConstraint: PendingLineCommitRelations["parallelConstraint"];
};

type PendingLineCommitRelationEntry = { fromLineCount: number } | null;
type CommittedSketchLine = SketchFeatureParameters["lines"][number];

export interface DraftStartRelations {
  midpointHostLineId: string | null;
  endpointHostLineId: string | null;
  lineBodyHost: LineBodyHost | null;
}

export function draftStartRelations(
  sketchPoint: LineCommitSnapPoint,
): DraftStartRelations {
  const midpointHostLineId = sketchPoint.snapMidpointHostLineId;
  const midpointT = sketchPoint.snapMidpointT ?? null;
  const isWholeLineMidpoint =
    midpointHostLineId !== null &&
    midpointHostLineId !== undefined &&
    midpointT !== null &&
    Math.abs(midpointT - 0.5) < 1e-9;

  let lineBodyHost: LineBodyHost | null = null;
  if (!isWholeLineMidpoint && midpointHostLineId && midpointT !== null) {
    lineBodyHost = {
      lineId: midpointHostLineId,
      t: midpointT,
    };
  } else if (
    sketchPoint.snapLineBodyHostLineId &&
    typeof sketchPoint.snapLineBodyT === "number"
  ) {
    lineBodyHost = {
      lineId: sketchPoint.snapLineBodyHostLineId,
      t: sketchPoint.snapLineBodyT,
    };
  }

  return {
    midpointHostLineId: isWholeLineMidpoint
      ? (midpointHostLineId ?? null)
      : null,
    endpointHostLineId: sketchPoint.snapEndpointHostLineId ?? null,
    lineBodyHost,
  };
}

export function lineCommitRelations({
  sketchPoint,
  fromLineCount,
  startMidpointHostLineId,
  startLineBodyHost,
}: {
  sketchPoint: LineCommitSnapPoint;
  fromLineCount: number;
  startMidpointHostLineId: string | null;
  startLineBodyHost: LineBodyHost | null;
}): PendingLineCommitRelations {
  const endMidpointT = sketchPoint.snapMidpointT ?? null;
  const endIsWholeLineMidpoint =
    sketchPoint.snapMidpointHostLineId &&
    endMidpointT !== null &&
    Math.abs(endMidpointT - 0.5) < 1e-9;
  const endHostLineId = endIsWholeLineMidpoint
    ? (sketchPoint.snapMidpointHostLineId ?? null)
    : null;

  const perpHostLineId = sketchPoint.snapPerpendicularHostLineId;
  const endIsSubSegmentMidpoint =
    sketchPoint.snapMidpointHostLineId &&
    endMidpointT !== null &&
    !endIsWholeLineMidpoint;
  const endLineBodyHost = endIsSubSegmentMidpoint
    ? {
        lineId: sketchPoint.snapMidpointHostLineId,
        t: endMidpointT,
      }
    : sketchPoint.snapLineBodyHostLineId &&
        typeof sketchPoint.snapLineBodyT === "number"
      ? {
          lineId: sketchPoint.snapLineBodyHostLineId,
          t: sketchPoint.snapLineBodyT,
        }
      : null;

  return {
    midpointAnchor:
      startMidpointHostLineId || endHostLineId
        ? {
            fromLineCount,
            startHostLineId: startMidpointHostLineId,
            endHostLineId,
          }
        : null,
    perpendicularConstraint: perpHostLineId
      ? {
          fromLineCount,
          hostLineId: perpHostLineId,
        }
      : null,
    pointLineAnchor:
      startLineBodyHost || endLineBodyHost
        ? {
            fromLineCount,
            startHost: startLineBodyHost,
            endHost: endLineBodyHost,
          }
        : null,
    axisConstraint:
      sketchPoint.snapAxisLock && !perpHostLineId
        ? {
            fromLineCount,
            kind: sketchPoint.snapAxisLock,
          }
        : null,
    tangentConstraint:
      sketchPoint.snapTangentCircleId && !perpHostLineId
        ? {
            fromLineCount,
            circleId: sketchPoint.snapTangentCircleId,
          }
        : null,
    parallelConstraint:
      sketchPoint.snapParallelHostLineId &&
      !perpHostLineId &&
      !sketchPoint.snapAxisLock &&
      !sketchPoint.snapTangentCircleId
        ? {
            fromLineCount,
            hostLineId: sketchPoint.snapParallelHostLineId,
          }
        : null,
    endHostLineId,
    endLineBodyHost,
  };
}

export function applyPendingLineCommitRelations({
  sketchParameters,
  previousLineCount,
  currentLineCount,
  refs,
  actions,
}: {
  sketchParameters: SketchFeatureParameters | null;
  previousLineCount: number;
  currentLineCount: number;
  refs: PendingLineCommitRelationRefs;
  actions: PendingLineCommitRelationActions;
}) {
  const pending = pendingLineCommitRelationSnapshot(refs);

  if (!sketchParameters) {
    return;
  }
  if (!hasPendingLineCommitRelations(pending)) {
    return;
  }

  if (currentLineCount !== pendingLineCommitBaseline(pending) + 1) {
    if (currentLineCount !== previousLineCount) {
      clearPendingLineCommitRelationRefs(refs);
    }
    return;
  }

  clearPendingLineCommitRelationRefs(refs);
  const newLine = sketchParameters.lines[sketchParameters.lines.length - 1];
  if (!newLine) {
    return;
  }

  dispatchPendingLineCommitRelations(newLine, pending, actions);
}

function pendingLineCommitRelationSnapshot(
  refs: PendingLineCommitRelationRefs,
): PendingLineCommitRelationSnapshot {
  return {
    midpointAnchor: refs.midpointAnchor.current,
    perpendicularConstraint: refs.perpendicularConstraint.current,
    pointLineAnchor: refs.pointLineAnchor.current,
    axisConstraint: refs.axisConstraint.current,
    tangentConstraint: refs.tangentConstraint.current,
    parallelConstraint: refs.parallelConstraint.current,
  };
}

function hasPendingLineCommitRelations(
  pending: PendingLineCommitRelationSnapshot,
) {
  return pendingLineCommitRelationEntries(pending).some(Boolean);
}

function pendingLineCommitBaseline(
  pending: PendingLineCommitRelationSnapshot,
) {
  return (
    pendingLineCommitRelationEntries(pending).find(Boolean)?.fromLineCount ??
    -1
  );
}

function pendingLineCommitRelationEntries(
  pending: PendingLineCommitRelationSnapshot,
): PendingLineCommitRelationEntry[] {
  return [
    pending.midpointAnchor,
    pending.perpendicularConstraint,
    pending.pointLineAnchor,
    pending.axisConstraint,
    pending.tangentConstraint,
    pending.parallelConstraint,
  ];
}

function dispatchPendingLineCommitRelations(
  newLine: CommittedSketchLine,
  pending: PendingLineCommitRelationSnapshot,
  actions: PendingLineCommitRelationActions,
) {
  dispatchMidpointAnchorRelation(newLine, pending, actions);
  dispatchPerpendicularRelation(newLine, pending, actions);
  dispatchPointLineAnchorRelation(newLine, pending, actions);
  dispatchAxisConstraintRelation(newLine, pending, actions);
  dispatchTangentConstraintRelation(newLine, pending, actions);
  dispatchParallelConstraintRelation(newLine, pending, actions);
}

function dispatchMidpointAnchorRelation(
  newLine: CommittedSketchLine,
  pending: PendingLineCommitRelationSnapshot,
  actions: PendingLineCommitRelationActions,
) {
  if (pending.midpointAnchor?.endHostLineId) {
    void actions.setSketchMidpointAnchor(
      newLine.end_point_id,
      pending.midpointAnchor.endHostLineId,
    );
  }
}

function dispatchPerpendicularRelation(
  newLine: CommittedSketchLine,
  pending: PendingLineCommitRelationSnapshot,
  actions: PendingLineCommitRelationActions,
) {
  if (pending.perpendicularConstraint) {
    void actions.setSketchPerpendicularConstraint(
      newLine.line_id,
      pending.perpendicularConstraint.hostLineId,
    );
  }
}

function dispatchPointLineAnchorRelation(
  newLine: CommittedSketchLine,
  pending: PendingLineCommitRelationSnapshot,
  actions: PendingLineCommitRelationActions,
) {
  if (
    pending.pointLineAnchor?.endHost &&
    !pending.midpointAnchor?.endHostLineId
  ) {
    void actions.setSketchPointLineAnchor(
      newLine.end_point_id,
      pending.pointLineAnchor.endHost.lineId,
      pending.pointLineAnchor.endHost.t,
    );
  }
}

function dispatchAxisConstraintRelation(
  newLine: CommittedSketchLine,
  pending: PendingLineCommitRelationSnapshot,
  actions: PendingLineCommitRelationActions,
) {
  if (pending.axisConstraint && !pending.perpendicularConstraint) {
    void actions.setSketchLineConstraint(
      newLine.line_id,
      pending.axisConstraint.kind,
    );
  }
}

function dispatchTangentConstraintRelation(
  newLine: CommittedSketchLine,
  pending: PendingLineCommitRelationSnapshot,
  actions: PendingLineCommitRelationActions,
) {
  if (
    pending.tangentConstraint &&
    !pending.perpendicularConstraint &&
    !pending.axisConstraint
  ) {
    void actions.setSketchTangentConstraint(
      newLine.line_id,
      pending.tangentConstraint.circleId,
    );
  }
}

function dispatchParallelConstraintRelation(
  newLine: CommittedSketchLine,
  pending: PendingLineCommitRelationSnapshot,
  actions: PendingLineCommitRelationActions,
) {
  if (
    pending.parallelConstraint &&
    !pending.perpendicularConstraint &&
    !pending.axisConstraint &&
    !pending.tangentConstraint
  ) {
    void actions.setSketchParallelConstraint(
      newLine.line_id,
      pending.parallelConstraint.hostLineId,
    );
  }
}

export function clearPendingLineCommitRelationRefs(
  refs: PendingLineCommitRelationRefs,
) {
  refs.midpointAnchor.current = null;
  refs.perpendicularConstraint.current = null;
  refs.pointLineAnchor.current = null;
  refs.axisConstraint.current = null;
  refs.tangentConstraint.current = null;
  refs.parallelConstraint.current = null;
}
