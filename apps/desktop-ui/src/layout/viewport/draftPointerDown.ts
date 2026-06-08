import * as THREE from "three";

import type { SketchPlaneFrame, SketchPreviewPoint, SketchTool } from "@/types";
import { resolveSketchPlanePoint } from "@/utils";
import {
  isDraftDimensionTool,
  type DraftDimensionField,
  type DraftDimensionSession,
  type DraftDimensionTool,
} from "./draftDimensions";

export interface MutableRef<T> {
  current: T;
}

export interface PointerDownPosition {
  x: number;
  y: number;
}

export interface UpdateDraftChainBreakParams {
  event: PointerEvent;
  activeSketchTool: SketchTool;
  draftStartRef: MutableRef<[number, number] | null>;
  lastPointerDownTimeRef: MutableRef<number>;
  lastPointerDownPosRef: MutableRef<PointerDownPosition | null>;
  chainBreakRequestedRef: MutableRef<boolean>;
}

export interface BeginDraftPointerDownParams {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  activeSketchTool: SketchTool;
  draftStartRef: MutableRef<[number, number] | null>;
  draftStartedOnPointerDownRef: MutableRef<boolean>;
  draftDimensionSessionRef: MutableRef<DraftDimensionSession | null>;
  resolveSnappedSketchPoint: (
    rawPoint: {
      local: [number, number];
      world: [number, number, number];
    },
    draftStartLocal?: [number, number] | null,
  ) => SketchPreviewPoint;
  createDraftDimensionSession: (
    tool: DraftDimensionTool,
    start: [number, number],
    current: [number, number],
  ) => DraftDimensionSession;
  setDraftDimensionSession: (session: DraftDimensionSession) => void;
  focusDraftField: (field: DraftDimensionField) => void;
}

export function beginDraftPointerDown({
  event,
  renderer,
  camera,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  activeSketchTool,
  draftStartRef,
  draftStartedOnPointerDownRef,
  draftDimensionSessionRef,
  resolveSnappedSketchPoint,
  createDraftDimensionSession,
  setDraftDimensionSession,
  focusDraftField,
}: BeginDraftPointerDownParams) {
  if (
    !activeSketchPlaneId ||
    !isDraftDimensionTool(activeSketchTool) ||
    draftStartRef.current
  ) {
    return false;
  }

  const rawPoint = resolveSketchPlanePoint(
    event,
    renderer,
    camera,
    activeSketchPlaneId,
    activeSketchPlaneFrame,
  );
  if (!rawPoint) {
    return true;
  }

  const sketchPoint = resolveSnappedSketchPoint(rawPoint, draftStartRef.current);
  draftStartRef.current = sketchPoint.local;
  draftStartedOnPointerDownRef.current = true;
  const session = createDraftDimensionSession(
    activeSketchTool,
    sketchPoint.local,
    sketchPoint.local,
  );
  draftDimensionSessionRef.current = session;
  setDraftDimensionSession(session);
  focusDraftField(session.activeField);
  return true;
}

export function updateDraftChainBreakRequest({
  event,
  activeSketchTool,
  draftStartRef,
  lastPointerDownTimeRef,
  lastPointerDownPosRef,
  chainBreakRequestedRef,
}: UpdateDraftChainBreakParams) {
  const now = performance.now();
  const prevTime = lastPointerDownTimeRef.current;
  const prevPos = lastPointerDownPosRef.current;
  lastPointerDownTimeRef.current = now;
  lastPointerDownPosRef.current = { x: event.clientX, y: event.clientY };
  chainBreakRequestedRef.current =
    prevTime > 0 &&
    now - prevTime < 300 &&
    prevPos !== null &&
    Math.abs(event.clientX - prevPos.x) < 6 &&
    Math.abs(event.clientY - prevPos.y) < 6 &&
    draftStartRef.current !== null &&
    isDraftDimensionTool(activeSketchTool);
}

export function finishDraftStartedPointerUp({
  deltaX,
  deltaY,
  draftStartedOnPointerDownRef,
  draftDimensionSessionRef,
  draftDimensionInputRefs,
}: {
  deltaX: number;
  deltaY: number;
  draftStartedOnPointerDownRef: MutableRef<boolean>;
  draftDimensionSessionRef: MutableRef<DraftDimensionSession | null>;
  draftDimensionInputRefs: MutableRef<
    Partial<Record<DraftDimensionField, HTMLInputElement | null>>
  >;
}) {
  if (!draftStartedOnPointerDownRef.current) {
    return false;
  }

  draftStartedOnPointerDownRef.current = false;
  if (deltaX > 4 || deltaY > 4) {
    const field = draftDimensionSessionRef.current?.activeField;
    if (field) {
      window.requestAnimationFrame(() => {
        draftDimensionInputRefs.current[field]?.focus();
        draftDimensionInputRefs.current[field]?.select();
      });
    }
  }
  return true;
}
