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

interface MutableRef<T> {
  current: T;
}

interface DraftPointerMoveParams {
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
  ) => SketchPreviewPoint;
  updateDraftSessionFromPoint: (point: [number, number]) => void;
  setSketchSnapLabel: (label: string | null) => void;
  setConstraintPreview: (preview: ConstraintPreviewState | null) => void;
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
  updateDraftSessionFromPoint,
  setSketchSnapLabel,
  setConstraintPreview,
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

  const sketchPoint = resolveSnappedSketchPoint(rawPoint, draftStart);
  setSketchSnapLabel(sketchPoint.snapLabel);

  if (isDraftDimensionTool(activeSketchTool) && draftDimensionSessionRef.current) {
    updateDraftSessionFromPoint(sketchPoint.local);
  }

  const canvasRect = renderer.domElement.getBoundingClientRect();
  setConstraintPreview(
    constraintPreviewFromSnap(
      sketchPoint,
      event.clientX - canvasRect.left,
      event.clientY - canvasRect.top,
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
