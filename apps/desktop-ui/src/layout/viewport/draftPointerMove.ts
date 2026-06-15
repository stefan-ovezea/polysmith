import * as THREE from "three";

import type { SketchPlaneFrame, SketchPreviewPoint, SketchTool } from "@/types";
import { resolveSketchPlanePoint } from "@/utils";
import {
  constraintPreviewFromSnap,
  type ConstraintPreviewState,
} from "./constraintPreview";
import {
  isDraftDimensionTool,
  type DraftDimensionSession,
} from "./draftDimensions";
import type { ResolveSnapOptions } from "./snapResolution";

export interface MutableRef<T> {
  current: T;
}

export interface DraftPointerMoveParams {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  activeSketchPlaneId: string;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  activeSketchTool: SketchTool;
  draftStartRef: MutableRef<[number, number] | null>;
  draftDimensionSessionRef: MutableRef<DraftDimensionSession | null>;
  resolveSnappedSketchPoint: (
    rawPoint: {
      local: [number, number];
      world: [number, number, number];
    },
    draftStartLocal?: [number, number] | null,
    options?: ResolveSnapOptions,
  ) => SketchPreviewPoint;
  objectSnapLatchRef: MutableRef<string | null>;
  updateDraftSessionFromPoint: (point: [number, number]) => void;
  setSketchSnapLabel: (label: string | null) => void;
  setConstraintPreview: (preview: ConstraintPreviewState | null) => void;
  setDraftCursorPoint: (point: { x: number; y: number } | null) => void;
}

export interface DraftPointerMoveState {
  draftStart: [number, number] | null;
  sketchPoint: SketchPreviewPoint;
  draftPreviewLocal: [number, number];
}

export function resolveDraftPointerMove({
  event,
  renderer,
  camera,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  activeSketchTool,
  draftStartRef,
  draftDimensionSessionRef,
  resolveSnappedSketchPoint,
  objectSnapLatchRef,
  updateDraftSessionFromPoint,
  setSketchSnapLabel,
  setConstraintPreview,
  setDraftCursorPoint,
}: DraftPointerMoveParams): DraftPointerMoveState | null {
  const draftStart = draftStartRef.current;
  const rawPoint = resolveSketchPlanePoint(
    event,
    renderer,
    camera,
    activeSketchPlaneId,
    activeSketchPlaneFrame,
  );
  if (!rawPoint) {
    return null;
  }

  const sketchPoint = resolveSnappedSketchPoint(rawPoint, draftStart, {
    objectSnapLatchKey: objectSnapLatchRef.current,
  });
  objectSnapLatchRef.current =
    sketchPoint.snapFeedbackSource === "object"
      ? (sketchPoint.snapTargetKey ?? null)
      : null;
  setSketchSnapLabel(sketchPoint.snapLabel);

  if (isDraftDimensionTool(activeSketchTool) && draftDimensionSessionRef.current) {
    updateDraftSessionFromPoint(sketchPoint.local);
  }

  const canvasRect = renderer.domElement.getBoundingClientRect();
  const rawCanvasPoint = {
    x: event.clientX - canvasRect.left,
    y: event.clientY - canvasRect.top,
  };
  const feedbackPoint = snapFeedbackCanvasPoint({
    sketchPoint,
    rawCanvasPoint,
    camera,
    renderer,
  });
  setDraftCursorPoint(feedbackPoint);
  setConstraintPreview(
    constraintPreviewFromSnap(
      sketchPoint,
      feedbackPoint.x,
      feedbackPoint.y,
    ),
  );

  return {
    draftStart,
    sketchPoint,
    draftPreviewLocal:
      isDraftDimensionTool(activeSketchTool) && draftDimensionSessionRef.current
        ? draftDimensionSessionRef.current.current
        : sketchPoint.local,
  };
}

function snapFeedbackCanvasPoint({
  sketchPoint,
  rawCanvasPoint,
  camera,
  renderer,
}: {
  sketchPoint: SketchPreviewPoint;
  rawCanvasPoint: { x: number; y: number };
  camera: THREE.Camera;
  renderer: THREE.WebGLRenderer;
}) {
  if (sketchPoint.snapFeedbackSource !== "object") {
    return rawCanvasPoint;
  }
  return (
    projectWorldPointToCanvas(sketchPoint.world, camera, renderer) ??
    rawCanvasPoint
  );
}

function projectWorldPointToCanvas(
  point: [number, number, number],
  camera: THREE.Camera,
  renderer: THREE.WebGLRenderer,
) {
  const projected = new THREE.Vector3(...point).project(camera);
  if (projected.z < -1 || projected.z > 1) {
    return null;
  }
  const widthHalf = renderer.domElement.clientWidth / 2;
  const heightHalf = renderer.domElement.clientHeight / 2;
  return {
    x: projected.x * widthHalf + widthHalf,
    y: -projected.y * heightHalf + heightHalf,
  };
}
