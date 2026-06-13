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

function lineLength(line: SketchLineEntry) {
  return Math.hypot(line.end_x - line.start_x, line.end_y - line.start_y);
}

function lineDirection(line: SketchLineEntry): [number, number] | null {
  const length = lineLength(line);
  if (length <= 1e-9) {
    return null;
  }
  return [
    (line.end_x - line.start_x) / length,
    (line.end_y - line.start_y) / length,
  ];
}

function parallelLineRelationScore({
  first,
  second,
  cursor,
}: {
  first: SketchLineEntry;
  second: SketchLineEntry;
  cursor: [number, number];
}) {
  if (!areLinesParallel(first, second)) {
    return null;
  }
  const dir = lineDirection(first);
  if (!dir) {
    return null;
  }
  const firstLength = lineLength(first);
  const secondLength = lineLength(second);
  if (firstLength <= 1e-9 || secondLength <= 1e-9) {
    return null;
  }

  let normal: [number, number] = [-dir[1], dir[0]];
  const firstMid: [number, number] = [
    (first.start_x + first.end_x) / 2,
    (first.start_y + first.end_y) / 2,
  ];
  const secondMid: [number, number] = [
    (second.start_x + second.end_x) / 2,
    (second.start_y + second.end_y) / 2,
  ];
  let signedDistance =
    (secondMid[0] - firstMid[0]) * normal[0] +
    (secondMid[1] - firstMid[1]) * normal[1];
  if (Math.abs(signedDistance) <= 1e-6) {
    return null;
  }
  if (signedDistance < 0) {
    normal = [-normal[0], -normal[1]];
    signedDistance = -signedDistance;
  }

  const cursorAlong =
    (cursor[0] - first.start_x) * dir[0] +
    (cursor[1] - first.start_y) * dir[1];
  const secondStartAlong =
    (second.start_x - first.start_x) * dir[0] +
    (second.start_y - first.start_y) * dir[1];
  const secondEndAlong =
    (second.end_x - first.start_x) * dir[0] +
    (second.end_y - first.start_y) * dir[1];
  const minAlong =
    Math.min(0, firstLength, secondStartAlong, secondEndAlong) -
    SKETCH_SNAP_DISTANCE * 2;
  const maxAlong =
    Math.max(0, firstLength, secondStartAlong, secondEndAlong) +
    SKETCH_SNAP_DISTANCE * 2;
  if (cursorAlong < minAlong || cursorAlong > maxAlong) {
    return null;
  }

  const cursorNormal =
    (cursor[0] - first.start_x) * normal[0] +
    (cursor[1] - first.start_y) * normal[1];
  const withinCorridor =
    cursorNormal >= -SKETCH_SNAP_DISTANCE &&
    cursorNormal <= signedDistance + SKETCH_SNAP_DISTANCE;
  if (!withinCorridor) {
    return null;
  }

  return Math.abs(cursorNormal - signedDistance / 2);
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
    const parallelScore = parallelLineRelationScore({
      first: firstLine,
      second: line,
      cursor,
    });
    if (hit.distance > SKETCH_SNAP_DISTANCE && parallelScore === null) {
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
      const next = betterPreview(
        best,
        candidate,
        parallelScore ?? hit.distance,
        bestScore,
      );
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
