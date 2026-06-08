import type { SketchFeatureParameters } from "@/types";
import { distanceBetweenPoints, toWorldPoint } from "@/utils";
import type { DraftDimensionField, DraftDimensionTool } from "./draftDimensions";

type MutableRef<T> = { current: T };
type DimensionLabelPositions = Record<string, [number, number, number]>;
type SetDimensionLabelPositions = (
  updater: (current: DimensionLabelPositions) => DimensionLabelPositions,
) => void;
type SketchDimensionUpdater = (
  dimensionId: string,
  value: number | string,
) => Promise<void>;
type SketchDimensionDeleter = (dimensionId: string) => Promise<void>;

export interface PendingCircleDimensionPlacement {
  fromCircleCount: number;
  center: [number, number];
  end: [number, number];
}

export interface PendingDimensionDeletion {
  shouldDeleteLine: boolean;
  shouldDeleteCircle: boolean;
  shouldDeletePolygon: boolean;
  shouldDeleteRectangle: boolean;
  shouldDeleteLineAngle: boolean;
}

export interface PendingDraftDimensionExpressions {
  tool: DraftDimensionTool;
  fromLineCount: number;
  fromCircleCount: number;
  fromPolygonCount: number;
  expressions: Partial<Record<DraftDimensionField, string>>;
}

export function placePendingCircleDimensionLabel({
  pendingCircleDimensionPlacementRef,
  sketch,
  setDimensionLabelPositions,
}: {
  pendingCircleDimensionPlacementRef: MutableRef<PendingCircleDimensionPlacement | null>;
  sketch: SketchFeatureParameters | null | undefined;
  setDimensionLabelPositions: SetDimensionLabelPositions;
}) {
  const placement = pendingCircleDimensionLabelPlacement(
    pendingCircleDimensionPlacementRef.current,
    sketch,
  );
  if (placement === "wait") {
    return;
  }
  if (placement === "clear") {
    pendingCircleDimensionPlacementRef.current = null;
    return;
  }

  setDimensionLabelPositions((current) => ({
    ...current,
    [`dim-circle-${placement.circleId}`]: toWorldPoint(
      placement.planeId,
      placement.labelLocal,
      placement.planeFrame,
    ),
  }));
  pendingCircleDimensionPlacementRef.current = null;
}

function pendingCircleDimensionLabelPlacement(
  pending: PendingCircleDimensionPlacement | null,
  sketch: SketchFeatureParameters | null | undefined,
) {
  if (!pending || !sketch) {
    return "wait" as const;
  }
  if (sketch.circles.length <= pending.fromCircleCount) {
    return "wait" as const;
  }

  const circle =
    sketch.circles[pending.fromCircleCount] ??
    sketch.circles[sketch.circles.length - 1];
  const labelLocal = pendingCircleLabelLocalPoint(pending);
  if (!circle || !labelLocal) {
    return "clear" as const;
  }
  return {
    circleId: circle.circle_id,
    labelLocal,
    planeId: sketch.plane_id,
    planeFrame: sketch.plane_frame,
  };
}

function pendingCircleLabelLocalPoint(
  pending: PendingCircleDimensionPlacement,
): [number, number] | null {
  const radius = distanceBetweenPoints(pending.center, pending.end);
  const dx = pending.end[0] - pending.center[0];
  const dy = pending.end[1] - pending.center[1];
  const length = Math.hypot(dx, dy);
  if (radius <= 1e-6 || length <= 1e-6) {
    return null;
  }
  return [
    pending.center[0] + (dx / length) * (radius + 4),
    pending.center[1] + (dy / length) * (radius + 4),
  ];
}

export function deletePendingAutoDimensions({
  pendingDimensionDeletionRef,
  sketch,
  deleteSketchDimension,
}: {
  pendingDimensionDeletionRef: MutableRef<PendingDimensionDeletion | null>;
  sketch: SketchFeatureParameters | null | undefined;
  deleteSketchDimension: SketchDimensionDeleter;
}) {
  const pending = pendingDimensionDeletionRef.current;
  if (!pending) {
    return;
  }
  if (!sketch) {
    pendingDimensionDeletionRef.current = null;
    return;
  }

  deletePendingLineDimensions(pending, sketch, deleteSketchDimension);
  deletePendingCircleDimension(pending, sketch, deleteSketchDimension);
  deletePendingPolygonDimension(pending, sketch, deleteSketchDimension);
  deletePendingRectangleDimensions(pending, sketch, deleteSketchDimension);
  pendingDimensionDeletionRef.current = null;
}

function deletePendingLineDimensions(
  pending: PendingDimensionDeletion,
  sketch: SketchFeatureParameters,
  deleteSketchDimension: SketchDimensionDeleter,
) {
  const line = sketch.lines[sketch.lines.length - 1];
  if (!line || line.is_construction) {
    return;
  }
  if (pending.shouldDeleteLine) {
    deleteDimensionForEntity(sketch, line.line_id, "line_length", deleteSketchDimension);
  }
  if (pending.shouldDeleteLineAngle) {
    deleteDimensionForEntity(sketch, line.line_id, "line_angle", deleteSketchDimension);
  }
}

function deletePendingCircleDimension(
  pending: PendingDimensionDeletion,
  sketch: SketchFeatureParameters,
  deleteSketchDimension: SketchDimensionDeleter,
) {
  const circle = sketch.circles[sketch.circles.length - 1];
  if (pending.shouldDeleteCircle && circle && !circle.is_construction) {
    deleteDimensionForEntity(
      sketch,
      circle.circle_id,
      "circle_radius",
      deleteSketchDimension,
    );
  }
}

function deletePendingPolygonDimension(
  pending: PendingDimensionDeletion,
  sketch: SketchFeatureParameters,
  deleteSketchDimension: SketchDimensionDeleter,
) {
  const polygons = sketch.polygons ?? [];
  const polygon = polygons[polygons.length - 1];
  if (pending.shouldDeletePolygon && polygon && !polygon.is_construction) {
    deleteDimensionForEntity(
      sketch,
      polygon.polygon_id,
      "polygon_radius",
      deleteSketchDimension,
    );
  }
}

function deletePendingRectangleDimensions(
  pending: PendingDimensionDeletion,
  sketch: SketchFeatureParameters,
  deleteSketchDimension: SketchDimensionDeleter,
) {
  if (!pending.shouldDeleteRectangle || sketch.lines.length < 4) {
    return;
  }
  for (let i = sketch.lines.length - 4; i < sketch.lines.length; i++) {
    const line = sketch.lines[i];
    if (line && !line.is_construction) {
      deleteDimensionForEntity(sketch, line.line_id, "line_length", deleteSketchDimension);
    }
  }
}

function deleteDimensionForEntity(
  sketch: SketchFeatureParameters,
  entityId: string,
  kind: string,
  deleteSketchDimension: SketchDimensionDeleter,
) {
  const dimensionId = findDimensionId(sketch, entityId, kind);
  if (dimensionId) {
    void deleteSketchDimension(dimensionId);
  }
}

function findDimensionId(
  sketch: SketchFeatureParameters,
  entityId: string,
  kind: string,
) {
  return sketch.dimensions.find(
    (dimension) => dimension.entity_id === entityId && dimension.kind === kind,
  )?.dimension_id;
}

export function applyPendingDraftDimensionExpressions({
  pendingDraftDimensionExpressionsRef,
  sketch,
  updateSketchDimension,
}: {
  pendingDraftDimensionExpressionsRef: MutableRef<PendingDraftDimensionExpressions | null>;
  sketch: SketchFeatureParameters | null | undefined;
  updateSketchDimension: SketchDimensionUpdater;
}) {
  const pending = pendingDraftDimensionExpressionsRef.current;
  if (!pending || !sketch) {
    return;
  }

  const didApply = applyPendingExpressionForTool({
    pending,
    sketch,
    updateSketchDimension,
  });
  if (didApply) {
    pendingDraftDimensionExpressionsRef.current = null;
  }
}

function applyPendingExpressionForTool({
  pending,
  sketch,
  updateSketchDimension,
}: {
  pending: PendingDraftDimensionExpressions;
  sketch: SketchFeatureParameters;
  updateSketchDimension: SketchDimensionUpdater;
}) {
  if (pending.tool === "line") {
    return applyLineExpressions(pending, sketch, updateSketchDimension);
  }
  if (pending.tool === "rectangle") {
    return applyRectangleExpressions(pending, sketch, updateSketchDimension);
  }
  if (pending.tool === "circle") {
    return applyCircleExpression(pending, sketch, updateSketchDimension);
  }
  return applyPolygonExpression(pending, sketch, updateSketchDimension);
}

function applyLineExpressions(
  pending: PendingDraftDimensionExpressions,
  sketch: SketchFeatureParameters,
  updateSketchDimension: SketchDimensionUpdater,
) {
  const line = sketch.lines[pending.fromLineCount] ?? null;
  if (!line) {
    return false;
  }
  updateDimensionExpression(
    `dim-line-${line.line_id}`,
    pending.expressions.length,
    updateSketchDimension,
  );
  updateDimensionExpression(
    `dim-line-angle-${line.line_id}`,
    pending.expressions.angle,
    updateSketchDimension,
  );
  return true;
}

function applyRectangleExpressions(
  pending: PendingDraftDimensionExpressions,
  sketch: SketchFeatureParameters,
  updateSketchDimension: SketchDimensionUpdater,
) {
  if (sketch.lines.length < pending.fromLineCount + 4) {
    return false;
  }
  const topLine = sketch.lines[pending.fromLineCount];
  const rightLine = sketch.lines[pending.fromLineCount + 1];
  if (topLine) {
    updateDimensionExpression(
      `dim-line-${topLine.line_id}`,
      pending.expressions.width,
      updateSketchDimension,
    );
  }
  if (rightLine) {
    updateDimensionExpression(
      `dim-line-${rightLine.line_id}`,
      pending.expressions.length,
      updateSketchDimension,
    );
  }
  return true;
}

function applyCircleExpression(
  pending: PendingDraftDimensionExpressions,
  sketch: SketchFeatureParameters,
  updateSketchDimension: SketchDimensionUpdater,
) {
  const circle =
    sketch.circles[pending.fromCircleCount] ??
    sketch.circles[sketch.circles.length - 1];
  if (!circle) {
    return false;
  }
  updateDimensionExpression(
    `dim-circle-${circle.circle_id}`,
    pending.expressions.diameter,
    updateSketchDimension,
  );
  return true;
}

function applyPolygonExpression(
  pending: PendingDraftDimensionExpressions,
  sketch: SketchFeatureParameters,
  updateSketchDimension: SketchDimensionUpdater,
) {
  const polygons = sketch.polygons ?? [];
  const polygon =
    polygons[pending.fromPolygonCount] ?? polygons[polygons.length - 1];
  if (!polygon) {
    return false;
  }
  updateDimensionExpression(
    `dim-polygon-${polygon.polygon_id}`,
    pending.expressions.radius,
    updateSketchDimension,
  );
  return true;
}

function updateDimensionExpression(
  dimensionId: string,
  expression: string | undefined,
  updateSketchDimension: SketchDimensionUpdater,
) {
  if (expression) {
    void updateSketchDimension(dimensionId, expression).catch(() => {});
  }
}
