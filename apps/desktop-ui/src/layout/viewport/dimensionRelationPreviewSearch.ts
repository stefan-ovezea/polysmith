import type {
  SelectionFilter,
  SketchCircleEntry,
  SketchFeatureParameters,
  SketchLineEntry,
  SketchPlaneFrame,
} from "@/types";
import { SKETCH_SNAP_DISTANCE } from "@/utils";
import {
  areLinesParallel,
  createCircleCenterDistancePreview,
  createCircleLineDistancePreview,
  createLineAnglePreview,
  createParallelLineDistancePreview,
  distanceToCircleEdge,
  distanceToLineSegment,
} from "./dimensionRelationPreviewGeometry";
import type { DimensionRelationPreviewResult } from "./dimensionRelationPreview";

interface PreviewSearchContext {
  firstEntityId: string;
  sketchParameters: SketchFeatureParameters;
  filter: SelectionFilter;
  cursor: [number, number];
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  allowConstruction: boolean;
}

function betterPreview(
  current: DimensionRelationPreviewResult | null,
  candidate: DimensionRelationPreviewResult,
  score: number,
  currentScore: number | null,
) {
  if (!current || currentScore === null || score < currentScore) {
    return { best: candidate, score };
  }
  return { best: current, score: currentScore };
}

export function buildLineDimensionRelationPreview({
  firstEntityId,
  sketchParameters,
  filter,
  cursor,
  planeId,
  planeFrame,
  allowConstruction,
}: PreviewSearchContext): DimensionRelationPreviewResult | null {
  const firstLine = sketchParameters.lines.find(
    (line) => line.line_id === firstEntityId,
  );
  if (!firstLine || (firstLine.is_construction && !allowConstruction)) {
    return null;
  }

  let best: DimensionRelationPreviewResult | null = null;
  let bestScore: number | null = null;

  for (const line of sketchParameters.lines) {
    if (line.line_id === firstLine.line_id) {
      continue;
    }
    if (line.is_construction && !allowConstruction) {
      continue;
    }

    const hit = distanceToLineSegment(cursor, line);
    if (hit.distance > SKETCH_SNAP_DISTANCE) {
      continue;
    }

    const candidate = buildLineLineDimensionRelationPreview({
      firstEntityId,
      firstLine,
      line,
      filter,
      cursor,
      planeId,
      planeFrame,
    });
    if (candidate) {
      const next = betterPreview(best, candidate, hit.distance, bestScore);
      best = next.best;
      bestScore = next.score;
    }
  }

  if (filter.snap_nearest) {
    for (const circle of sketchParameters.circles) {
      if (circle.is_construction && !allowConstruction) {
        continue;
      }
      const distance = distanceToCircleEdge(cursor, circle);
      if (distance > SKETCH_SNAP_DISTANCE) {
        continue;
      }
      const candidate = buildLineCircleDimensionRelationPreview({
        firstEntityId,
        firstLine,
        circle,
        planeId,
        planeFrame,
      });
      if (candidate) {
        const next = betterPreview(best, candidate, distance, bestScore);
        best = next.best;
        bestScore = next.score;
      }
    }
  }

  return best;
}

function buildLineLineDimensionRelationPreview({
  firstEntityId,
  firstLine,
  line,
  filter,
  cursor,
  planeId,
  planeFrame,
}: {
  firstEntityId: string;
  firstLine: SketchLineEntry;
  line: SketchLineEntry;
  filter: SelectionFilter;
  cursor: [number, number];
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
}): DimensionRelationPreviewResult | null {
  if (filter.snap_intersection) {
    const anglePreview = createLineAnglePreview({
      first: firstLine,
      second: line,
      cursor,
      planeId,
      planeFrame,
    });
    if (anglePreview) {
      return {
        relation: {
          kind: "line_angle",
          firstEntityId,
          targetEntityId: line.line_id,
        },
        dimension: anglePreview.dimension,
        anglePreview: anglePreview.anglePreview,
      };
    }
  }

  if (filter.snap_parallel && areLinesParallel(firstLine, line)) {
    const dimension = createParallelLineDistancePreview({
      first: firstLine,
      second: line,
      cursor,
      planeId,
      planeFrame,
    });
    if (dimension) {
      return {
        relation: {
          kind: "parallel_line_distance",
          firstEntityId,
          targetEntityId: line.line_id,
        },
        dimension,
        anglePreview: null,
      };
    }
  }

  return null;
}

function buildLineCircleDimensionRelationPreview({
  firstEntityId,
  firstLine,
  circle,
  planeId,
  planeFrame,
}: {
  firstEntityId: string;
  firstLine: SketchLineEntry;
  circle: SketchCircleEntry;
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
}): DimensionRelationPreviewResult | null {
  const dimension = createCircleLineDistancePreview({
    line: firstLine,
    circle,
    planeId,
    planeFrame,
  });
  if (!dimension) {
    return null;
  }
  return {
    relation: {
      kind: "circle_line_distance",
      firstEntityId,
      targetEntityId: circle.circle_id,
    },
    dimension,
    anglePreview: null,
  };
}

export function buildCircleDimensionRelationPreview({
  firstEntityId,
  sketchParameters,
  filter,
  cursor,
  planeId,
  planeFrame,
  allowConstruction,
}: PreviewSearchContext): DimensionRelationPreviewResult | null {
  const firstCircle = sketchParameters.circles.find(
    (circle) => circle.circle_id === firstEntityId,
  );
  if (
    !firstCircle ||
    (firstCircle.is_construction && !allowConstruction) ||
    !filter.snap_nearest
  ) {
    return null;
  }

  let best: DimensionRelationPreviewResult | null = null;
  let bestScore: number | null = null;

  for (const line of sketchParameters.lines) {
    if (line.is_construction && !allowConstruction) {
      continue;
    }
    const hit = distanceToLineSegment(cursor, line);
    if (hit.distance > SKETCH_SNAP_DISTANCE) {
      continue;
    }
    const candidate = buildCircleLineDimensionRelationPreview({
      firstEntityId,
      firstCircle,
      line,
      planeId,
      planeFrame,
    });
    if (candidate) {
      const next = betterPreview(best, candidate, hit.distance, bestScore);
      best = next.best;
      bestScore = next.score;
    }
  }

  for (const circle of sketchParameters.circles) {
    if (circle.circle_id === firstCircle.circle_id) {
      continue;
    }
    if (circle.is_construction && !allowConstruction) {
      continue;
    }
    const distance = distanceToCircleEdge(cursor, circle);
    if (distance > SKETCH_SNAP_DISTANCE) {
      continue;
    }
    const candidate = buildCircleCircleDimensionRelationPreview({
      firstEntityId,
      firstCircle,
      circle,
      planeId,
      planeFrame,
    });
    if (candidate) {
      const next = betterPreview(best, candidate, distance, bestScore);
      best = next.best;
      bestScore = next.score;
    }
  }

  return best;
}

function buildCircleLineDimensionRelationPreview({
  firstEntityId,
  firstCircle,
  line,
  planeId,
  planeFrame,
}: {
  firstEntityId: string;
  firstCircle: SketchCircleEntry;
  line: SketchLineEntry;
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
}): DimensionRelationPreviewResult | null {
  const dimension = createCircleLineDistancePreview({
    line,
    circle: firstCircle,
    planeId,
    planeFrame,
  });
  if (!dimension) {
    return null;
  }
  return {
    relation: {
      kind: "circle_line_distance",
      firstEntityId,
      targetEntityId: line.line_id,
    },
    dimension,
    anglePreview: null,
  };
}

function buildCircleCircleDimensionRelationPreview({
  firstEntityId,
  firstCircle,
  circle,
  planeId,
  planeFrame,
}: {
  firstEntityId: string;
  firstCircle: SketchCircleEntry;
  circle: SketchCircleEntry;
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
}): DimensionRelationPreviewResult | null {
  const dimension = createCircleCenterDistancePreview({
    first: firstCircle,
    second: circle,
    planeId,
    planeFrame,
  });
  if (!dimension) {
    return null;
  }
  return {
    relation: {
      kind: "circle_center_distance",
      firstEntityId,
      targetEntityId: circle.circle_id,
    },
    dimension,
    anglePreview: null,
  };
}
