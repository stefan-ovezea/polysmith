import * as THREE from "three";

import type { SketchPlaneFrame } from "@/types";
import { resolveSketchPlanePoint } from "@/utils";
import { endpointDragDelta, type EndpointDrag } from "./endpointDrag";

interface MutableRef<T> {
  current: T;
}

interface EndpointDragPointerUpParams {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  controls: { enabled: boolean };
  endpointDragRef: MutableRef<EndpointDrag | null>;
  dragSnapResultRef: MutableRef<{ snapX: number; snapY: number } | null>;
  activeSketchPlaneIdRef: MutableRef<string | null>;
  activeSketchPlaneFrameRef: MutableRef<SketchPlaneFrame | null>;
  pendingEndpointCommitRef: MutableRef<boolean>;
  dragCursorRef: MutableRef<{ x: number; y: number } | null>;
  updateSketchPoint: (
    pointId: string,
    x: number,
    y: number,
  ) => Promise<void> | void;
  clearDragPreviewLines: () => void;
  setConstraintPreview: (preview: null) => void;
  setSketchSnapLabel: (label: string | null) => void;
  setHoveredSketchEntity: (entityId: string | null) => void;
  setHoveredSketchPoint: (pointId: string | null) => void;
  setPointerDown: (point: { x: number; y: number } | null) => void;
}

export function finishEndpointDragPointerUp({
  event,
  renderer,
  camera,
  controls,
  endpointDragRef,
  dragSnapResultRef,
  activeSketchPlaneIdRef,
  activeSketchPlaneFrameRef,
  pendingEndpointCommitRef,
  dragCursorRef,
  updateSketchPoint,
  clearDragPreviewLines,
  setConstraintPreview,
  setSketchSnapLabel,
  setHoveredSketchEntity,
  setHoveredSketchPoint,
  setPointerDown,
}: EndpointDragPointerUpParams) {
  const drag = endpointDragRef.current;
  if (!drag) {
    return "inactive" as const;
  }

  const { hasMoved } = endpointDragDelta(event, drag);
  if (hasMoved) {
    commitEndpointDrag({
      event,
      renderer,
      camera,
      drag,
      dragSnapResultRef,
      activeSketchPlaneIdRef,
      activeSketchPlaneFrameRef,
      updateSketchPoint,
    });
    pendingEndpointCommitRef.current = true;
  } else {
    endpointDragRef.current = null;
    clearDragPreviewLines();
    setConstraintPreview(null);
    dragCursorRef.current = null;
  }

  controls.enabled = true;
  (renderer.domElement as HTMLCanvasElement).style.cursor = "";
  setSketchSnapLabel(null);
  setHoveredSketchEntity(null);
  setHoveredSketchPoint(null);

  if (hasMoved) {
    setPointerDown(null);
    return "consumed" as const;
  }

  setPointerDown({ x: event.clientX, y: event.clientY });
  return "continue" as const;
}

function commitEndpointDrag({
  event,
  renderer,
  camera,
  drag,
  dragSnapResultRef,
  activeSketchPlaneIdRef,
  activeSketchPlaneFrameRef,
  updateSketchPoint,
}: Pick<
  EndpointDragPointerUpParams,
  | "event"
  | "renderer"
  | "camera"
  | "dragSnapResultRef"
  | "activeSketchPlaneIdRef"
  | "activeSketchPlaneFrameRef"
  | "updateSketchPoint"
> & {
  drag: EndpointDrag;
}) {
  const snapResult = dragSnapResultRef.current;
  if (snapResult) {
    void updateSketchPoint(drag.vertexId, snapResult.snapX, snapResult.snapY);
    return;
  }

  const activeSketchPlaneId = activeSketchPlaneIdRef.current;
  if (!activeSketchPlaneId) {
    return;
  }
  const rawPoint = resolveSketchPlanePoint(
    event,
    renderer,
    camera,
    activeSketchPlaneId,
    activeSketchPlaneFrameRef.current,
  );
  if (rawPoint) {
    void updateSketchPoint(drag.vertexId, rawPoint.local[0], rawPoint.local[1]);
  }
}
