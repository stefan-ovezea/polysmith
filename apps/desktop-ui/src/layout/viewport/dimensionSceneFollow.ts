import type * as THREE from "three";

import type { SketchDimensionScene } from "@/types";
import { buildSketchDimensionObject } from "@/utils/viewport/sketchObjects";
import { disposeGroup } from "@/utils/viewport.utils";

import {
  type Point3,
  type PointPositionSnapshot,
  type SketchPointMove,
  collectDragMoves,
  followDimensionsForMoves,
} from "./dimensionFollow";

function pointsMatch(a: Point3, b: Point3, tolerance: number) {
  return (
    Math.abs(a[0] - b[0]) <= tolerance &&
    Math.abs(a[1] - b[1]) <= tolerance &&
    Math.abs(a[2] - b[2]) <= tolerance
  );
}

/**
 * Rebuild ONE dimension's Three.js objects in place — dispose the old geometry and
 * label texture, build from the given scene data, and swap both into the sketch
 * group and the lookup tables. Used by the label drag and by the drag-time follow,
 * which cannot afford a whole scene rebuild.
 */
export function replaceSketchDimensionObject({
  dimension,
  sketchGroup,
  dimensionObjectById,
  sketchDimensionObjects,
  displayUnits,
}: {
  dimension: SketchDimensionScene;
  sketchGroup: THREE.Group;
  dimensionObjectById: Map<
    string,
    { line: THREE.Group; label: THREE.Sprite }
  >;
  sketchDimensionObjects: THREE.Object3D[];
  displayUnits?: "mm" | "in";
}) {
  const objects = dimensionObjectById.get(dimension.dimensionId);
  if (!objects) return;

  const replacement = buildSketchDimensionObject(dimension, displayUnits);

  sketchGroup.remove(objects.line);
  disposeGroup(objects.line);

  sketchGroup.remove(objects.label);
  objects.label.geometry?.dispose();
  const material = objects.label.material as THREE.SpriteMaterial;
  material?.map?.dispose();
  material?.dispose();

  sketchGroup.add(replacement.line);
  sketchGroup.add(replacement.label);

  dimensionObjectById.set(dimension.dimensionId, {
    line: replacement.line as THREE.Group,
    label: replacement.label,
  });

  const oldLineIndex = sketchDimensionObjects.indexOf(objects.line);
  const oldLabelIndex = sketchDimensionObjects.indexOf(objects.label);
  if (oldLineIndex !== -1) sketchDimensionObjects[oldLineIndex] = replacement.line;
  if (oldLabelIndex !== -1) sketchDimensionObjects[oldLabelIndex] = replacement.label;
}

/**
 * Make the dimensions follow the points a drag is moving.
 *
 * Rebuilding a dimension re-renders its label sprite (a canvas texture upload), so
 * `lastApplied` remembers where each dimension was last rebuilt and a dimension is
 * skipped until its label has moved by `labelMoveThreshold` — unless its VALUE
 * changed, which must always be shown. Without that second test a drag that changes
 * a measurement without moving the label much (a radius, a line being stretched
 * along itself) would keep printing the old number.
 */
export function followDraggedDimensions({
  dimensions,
  base,
  current,
  sketchGroup,
  dimensionObjectById,
  sketchDimensionObjects,
  lastApplied,
  displayUnits,
  labelMoveThreshold = 0.25,
}: {
  dimensions: readonly SketchDimensionScene[];
  base: PointPositionSnapshot;
  current: readonly { id: string; position: Point3 }[];
  sketchGroup: THREE.Group | null;
  dimensionObjectById: Map<
    string,
    { line: THREE.Group; label: THREE.Sprite }
  >;
  sketchDimensionObjects: THREE.Object3D[];
  lastApplied: Map<string, { position: Point3; value: number }>;
  displayUnits?: "mm" | "in";
  labelMoveThreshold?: number;
}): SketchDimensionScene[] {
  if (!sketchGroup) return [];

  const moves = collectDragMoves(base, current);
  if (moves.length === 0) return [];

  const applied: SketchDimensionScene[] = [];
  for (const dimension of followDimensionsForMoves(dimensions, moves)) {
    const previous = lastApplied.get(dimension.dimensionId);
    if (previous !== undefined) {
      const moved = Math.hypot(
        previous.position[0] - dimension.labelPosition[0],
        previous.position[1] - dimension.labelPosition[1],
        previous.position[2] - dimension.labelPosition[2],
      );
      if (moved < labelMoveThreshold && previous.value === dimension.rawValue) {
        continue;
      }
    }
    lastApplied.set(dimension.dimensionId, {
      position: dimension.labelPosition,
      value: dimension.rawValue,
    });
    replaceSketchDimensionObject({
      dimension,
      sketchGroup,
      dimensionObjectById,
      sketchDimensionObjects,
      displayUnits,
    });
    applied.push(dimension);
  }
  return applied;
}
