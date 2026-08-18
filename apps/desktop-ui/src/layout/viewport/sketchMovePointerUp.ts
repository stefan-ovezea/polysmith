import * as THREE from "three";

import type { PendingSketchMove, SketchMoveDrag } from "./sketchMoveTool";
import { disposeSketchMoveRingObject } from "./sketchMoveTool";

interface MutableRef<T> {
  current: T;
}

interface SketchMovePointerUpParams {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  controls: { enabled: boolean };
  sketchMoveDragRef: MutableRef<SketchMoveDrag | null>;
  sketchMoveRingGroupRef: MutableRef<THREE.Group | null>;
  pendingSketchMoveRef: MutableRef<PendingSketchMove | null>;
  setSketchSnapLabel: (label: string | null) => void;
  setHoveredSketchEntity: (entityId: string | null) => void;
  setHoveredSketchPoint: (pointId: string | null) => void;
  setPointerDown: (point: { x: number; y: number } | null) => void;
  /** Restores committed geometry after a preview that mutated real scene
   *  objects (used when no dialog is open — a safety fallback). */
  restorePreviewScene: () => void;
  /** Syncs the dialog's numeric fields after a drag ends. */
  reportMoveValues: (values: {
    dx: number;
    dy: number;
    angleDeg: number;
  }) => void;
}

export function finishSketchMovePointerUp({
  event,
  renderer,
  controls,
  sketchMoveDragRef,
  sketchMoveRingGroupRef,
  pendingSketchMoveRef,
  setSketchSnapLabel,
  setHoveredSketchEntity,
  setHoveredSketchPoint,
  setPointerDown,
  restorePreviewScene,
  reportMoveValues,
}: SketchMovePointerUpParams) {
  const drag = sketchMoveDragRef.current;
  if (!drag) {
    return "inactive" as const;
  }

  const moved =
    Math.abs(event.clientX - drag.startClientX) > 4 ||
    Math.abs(event.clientY - drag.startClientY) > 4;

  const pending = pendingSketchMoveRef.current;
  if (pending) {
    // Move/Copy dialog mode: the drag accumulated into the pending
    // transform and the preview stays on screen — nothing commits here
    // (OK commits, Cancel reverts).  Clicks do nothing while the dialog
    // is open (the target set is fixed).
    reportMoveValues({
      dx: pending.dx,
      dy: pending.dy,
      angleDeg: pending.angleDeg,
    });
  } else if (moved || drag.hasMoved) {
    // No dialog open (safety fallback): restore committed geometry.
    restorePreviewScene();
  }

  sketchMoveDragRef.current = null;
  disposeSketchMoveRingObject(sketchMoveRingGroupRef.current);
  sketchMoveRingGroupRef.current = null;
  controls.enabled = true;
  (renderer.domElement as HTMLCanvasElement).style.cursor = "";
  setSketchSnapLabel(null);
  setHoveredSketchEntity(null);
  setHoveredSketchPoint(null);
  setPointerDown(null);
  return "consumed" as const;
}
