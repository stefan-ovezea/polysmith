/**
 * Linear dimension placement lifecycle.
 *
 * When the dimension tool is in "linear" mode and the user clicks a line,
 * we defer dimension creation.  The user drags to choose the alignment
 * — horizontal (X), vertical (Y), or aligned (line length) — with a
 * live preview that updates on every pointer move.  Click to commit,
 * Escape to cancel.
 */
import * as THREE from "three";

import type {
  SketchDimensionScene,
  SketchFeatureParameters,
  SketchPlaneFrame,
} from "@/types";
import { buildSketchDimensionObject } from "@/utils";
import type { DisplayUnits } from "@/utils/units";
import {
  buildLinearDimensionPreview,
} from "./dimensionRelationPreviewGeometry";
import {
  computePointDistanceAxis,
} from "./dimensionToolPicking";

// Re‑export for ViewportPanel.
export { computePointDistanceAxis };

export interface LinearPlacementState {
  lineId: string;
  startPointId: string;
  endPointId: string;
  startX: number;
  startY: number;
  endX: number;
  endY: number;
  currentAxis: "x" | "y" | undefined;
  /** Last sketch-local cursor position (for label placement after commit). */
  lastCursorX: number;
  lastCursorY: number;
}

function resolveLinearPlacementLine(
  sketch: SketchFeatureParameters | null,
  lineId: string,
): LinearPlacementState | null {
  if (!sketch) return null;
  const line = sketch.lines.find((l) => l.line_id === lineId);
  if (!line) return null;
  return {
    lineId,
    startPointId: line.start_point_id,
    endPointId: line.end_point_id,
    startX: line.start_x,
    startY: line.start_y,
    endX: line.end_x,
    endY: line.end_y,
    currentAxis: undefined,
    lastCursorX: 0,
    lastCursorY: 0,
  };
}

/** Begin a linear placement session.  Returns the state or null. */
export function beginLinearPlacement(
  sketch: SketchFeatureParameters | null,
  lineId: string,
): LinearPlacementState | null {
  return resolveLinearPlacementLine(sketch, lineId);
}

/**
 * Update the live preview for the current cursor position.
 * Returns the preview scene object (for rendering) or null.
 */
export function updateLinearPlacementPreview(
  state: LinearPlacementState,
  cursor: [number, number],
  planeId: string,
  planeFrame: SketchPlaneFrame | null,
  displayUnits: DisplayUnits,
  sketchGroup: THREE.Group,
  previewGroupRef: { current: { line: THREE.Object3D; label: THREE.Sprite } | null },
): SketchDimensionScene | null {
  const axis = computePointDistanceAxis(
    cursor,
    { x: state.startX, y: state.startY },
    { x: state.endX, y: state.endY },
  );
  state.currentAxis = axis;
  state.lastCursorX = cursor[0];
  state.lastCursorY = cursor[1];

  const scene = buildLinearDimensionPreview({
    startX: state.startX,
    startY: state.startY,
    endX: state.endX,
    endY: state.endY,
    cursor,
    axis,
    planeId,
    planeFrame,
  });

  // Clear previous preview.
  if (previewGroupRef.current) {
    sketchGroup.remove(previewGroupRef.current.line);
    sketchGroup.remove(previewGroupRef.current.label);
    previewGroupRef.current = null;
  }

  // Build and add new preview.
  const preview = buildSketchDimensionObject(scene, displayUnits, {
    variant: "muted-preview",
    pickable: false,
  });
  sketchGroup.add(preview.line);
  sketchGroup.add(preview.label);
  previewGroupRef.current = preview;

  return scene;
}

/** Clean up the linear placement preview without committing. */
export function cancelLinearPlacement(
  sketchGroup: THREE.Group,
  previewGroupRef: { current: { line: THREE.Object3D; label: THREE.Sprite } | null },
) {
  if (previewGroupRef.current) {
    sketchGroup.remove(previewGroupRef.current.line);
    sketchGroup.remove(previewGroupRef.current.label);
    previewGroupRef.current = null;
  }
}

/**
 * Determine what IPC to send and the sketch-local label position.
 */
export function resolveLinearPlacementCommit(
  state: LinearPlacementState,
): { kind: "line_length"; lineId: string; labelX: number; labelY: number }
   | { kind: "point_distance"; pointAId: string; pointBId: string; axis: "x" | "y"; labelX: number; labelY: number } {
  const midX = (state.startX + state.endX) / 2;
  const midY = (state.startY + state.endY) / 2;

  if (state.currentAxis === "x") {
    return {
      kind: "point_distance",
      pointAId: state.startPointId,
      pointBId: state.endPointId,
      axis: "x",
      labelX: midX,
      labelY: state.lastCursorY,
    };
  }
  if (state.currentAxis === "y") {
    return {
      kind: "point_distance",
      pointAId: state.startPointId,
      pointBId: state.endPointId,
      axis: "y",
      labelX: state.lastCursorX,
      labelY: midY,
    };
  }
  // Aligned: label offset perpendicular to the line toward the cursor.
  const dx = state.endX - state.startX;
  const dy = state.endY - state.startY;
  const length = Math.sqrt(dx * dx + dy * dy);
  let labelX = midX;
  let labelY = midY;
  if (length > 1e-6) {
    const nx = -dy / length;
    const ny = dx / length;
    const offset = (state.lastCursorX - midX) * nx + (state.lastCursorY - midY) * ny;
    labelX = midX + nx * offset;
    labelY = midY + ny * offset;
  }
  return { kind: "line_length", lineId: state.lineId, labelX, labelY };
}
