import * as THREE from "three";

import type {
  SketchConstraintScene,
  SketchFeatureParameters,
  SketchPlaneFrame,
  SketchPreviewPoint,
} from "@/types";
import { resolveSketchPlanePoint, toWorldPoint } from "@/utils";
import type { SketchConstraintData } from "@/lib/planegcsBridge";
import { applySolvedPointsToSketchScene } from "./sketchPreviewSceneUpdate";
import {
  resolveSketchMoveFrame,
  sketchMoveConstraintDeltas,
  sketchMoveScreenAngle,
  solvePendingSketchMove,
  type PendingSketchMove,
  type SketchMoveDrag,
  type SketchMoveFrameResult,
} from "./sketchMoveTool";
import type { ResolveSnapOptions } from "./snapResolution";

interface MutableRef<T> {
  current: T;
}

type ResolveSnappedSketchPoint = (
  rawPoint: {
    local: [number, number];
    world: [number, number, number];
  },
  draftStartLocal?: [number, number] | null,
  options?: ResolveSnapOptions,
) => SketchPreviewPoint;

interface PendingMoveDragFrame {
  x: number;
  y: number;
  clientX: number;
  clientY: number;
}

interface HandleSketchMovePointerMoveParams {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  sketchMoveDragRef: MutableRef<SketchMoveDrag | null>;
  /** Move/Copy dialog state — drag deltas accumulate here. */
  pendingSketchMoveRef: MutableRef<PendingSketchMove | null>;
  activeSketchPlaneIdRef: MutableRef<string | null>;
  activeSketchPlaneFrameRef: MutableRef<SketchPlaneFrame | null>;
  sketchLinesRef: MutableRef<SketchFeatureParameters | null>;
  /** planegcs constraint data from the viewport state. */
  sketchConstraintsRef: MutableRef<SketchConstraintData[]>;
  /** Scene constraint data, for badge-follow deltas during the preview. */
  sceneConstraintsRef: MutableRef<readonly SketchConstraintScene[]>;
  pendingMoveDragRef: MutableRef<PendingMoveDragFrame | null>;
  pendingMoveDragFrameRef: MutableRef<number | null>;
  /** Last resolved frame result — used by the commit on pointer-up. */
  moveFrameResultRef: MutableRef<SketchMoveFrameResult | null>;
  /** Set while the preview is mutating committed scene objects. */
  moveDragPreviewActiveRef: MutableRef<boolean>;
  sketchEntityObjectByIdRef: MutableRef<
    Map<string, THREE.Line | THREE.LineLoop>
  >;
  sketchPointObjectByIdRef: MutableRef<Map<string, THREE.Mesh>>;
  sketchConstraintObjectsRef: MutableRef<THREE.Object3D[]>;
  sketchProfileObjectsRef: MutableRef<THREE.Group[]>;
  resolveSnappedSketchPoint: ResolveSnappedSketchPoint;
  setSketchSnapLabel: (label: string | null) => void;
  /** Live-updates the Move/Copy dialog's numeric fields during the drag. */
  reportMoveValues?: (values: {
    dx: number;
    dy: number;
    angleDeg: number;
  }) => void;
  requestRender: () => void;
}

export function handleSketchMovePointerMove(
  params: HandleSketchMovePointerMoveParams,
) {
  const drag = params.sketchMoveDragRef.current;
  const activeSketchPlaneId = params.activeSketchPlaneIdRef.current;
  if (!drag || !activeSketchPlaneId) {
    return false;
  }

  const rawPoint = resolveSketchPlanePoint(
    params.event,
    params.renderer,
    params.camera,
    activeSketchPlaneId,
    params.activeSketchPlaneFrameRef.current,
  );
  if (!rawPoint) {
    return true;
  }

  if (
    Math.abs(params.event.clientX - drag.startClientX) > 4 ||
    Math.abs(params.event.clientY - drag.startClientY) > 4
  ) {
    drag.hasMoved = true;
  }

  params.pendingMoveDragRef.current = {
    x: rawPoint.local[0],
    y: rawPoint.local[1],
    clientX: params.event.clientX,
    clientY: params.event.clientY,
  };

  requestSketchMoveFrame(params, drag);
  return true;
}

function requestSketchMoveFrame(
  params: HandleSketchMovePointerMoveParams,
  drag: SketchMoveDrag,
) {
  if (params.pendingMoveDragFrameRef.current !== null) {
    return;
  }

  params.pendingMoveDragFrameRef.current = window.requestAnimationFrame(() => {
    params.pendingMoveDragFrameRef.current = null;
    const next = params.pendingMoveDragRef.current;
    params.pendingMoveDragRef.current = null;
    if (!next) {
      return;
    }

    const planeId = params.activeSketchPlaneIdRef.current ?? "ref-plane-xy";
    const planeFrame = params.activeSketchPlaneFrameRef.current;
    const centerWorld = toWorldPoint(planeId, drag.centerLocal, planeFrame);
    const screenAngleRad = sketchMoveScreenAngle(
      next.clientX,
      next.clientY,
      centerWorld,
      params.camera,
      params.renderer,
    );

    const result = resolveSketchMoveFrame({
      drag,
      sketch: params.sketchLinesRef.current,
      planeId,
      planeFrame,
      resolveSnappedSketchPoint: params.resolveSnappedSketchPoint,
      constraints: params.sketchConstraintsRef.current,
      sceneConstraints: params.sceneConstraintsRef.current,
      rawLocal: [next.x, next.y],
      screenAngleRad,
    });
    params.moveFrameResultRef.current = result;
    params.setSketchSnapLabel(result.snapLabel);

    // Accumulate the incremental drag delta into the Move/Copy dialog's
    // pending transform and preview the TOTAL transform from the base
    // positions (the ring stays at the original centroid).
    const pending = params.pendingSketchMoveRef.current;
    if (!pending) {
      params.requestRender();
      return;
    }
    pending.dx += result.dx;
    pending.dy += result.dy;
    pending.angleDeg += result.angleDeg;
    params.reportMoveValues?.({
      dx: pending.dx,
      dy: pending.dy,
      angleDeg: pending.angleDeg,
    });

    const solvedPoints = solvePendingSketchMove({
      pending,
      sketch: params.sketchLinesRef.current,
      constraints: params.sketchConstraintsRef.current,
    });

    // Live preview: write the solved positions into the real committed
    // scene objects so the actual geometry moves with the pointer.
    params.moveDragPreviewActiveRef.current = true;
    applySolvedPointsToSketchScene({
      solvedPoints,
      sketch: params.sketchLinesRef.current,
      planeId,
      planeFrame,
      sketchEntityObjectById: params.sketchEntityObjectByIdRef.current,
      sketchPointObjectById: params.sketchPointObjectByIdRef.current,
      sketchConstraintObjects: params.sketchConstraintObjectsRef.current,
      sketchProfileObjects: params.sketchProfileObjectsRef.current,
      constraintDeltas: sketchMoveConstraintDeltas({
        sceneConstraints: params.sceneConstraintsRef.current,
        sketch: params.sketchLinesRef.current,
        entityIds: pending.entityIds,
        vertexIds: pending.vertexIds,
        baseVertexPositions: pending.baseVertexPositions,
        dx: pending.dx,
        dy: pending.dy,
        center: pending.centerLocal,
        angleRad: (pending.angleDeg * Math.PI) / 180,
        planeId,
        planeFrame,
      }),
    });
    params.requestRender();
  });
}
