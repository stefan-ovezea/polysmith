import { clearTrimHighlights } from "./previewObjectCleanup";
import * as THREE from "three";

import { sendCoreCommand } from "@/lib/cadCoreClient";
import { makeTrimPreviewCommand } from "@/lib/ipcProtocol";
import type { SketchPlaneFrame, ViewportScene } from "@/types";
import { resolveSketchPlanePoint } from "@/utils";
import type { ViewportPickHit } from "./contextMenuState";
import type { PointerMoveHoverActions } from "./pointerMoveHover";
import { applyTrimToolHover } from "./pointerMoveHover";

interface MutableRef<T> {
  current: T;
}

type TrimEntityKind = "line" | "circle" | "arc";

interface TrimPreviewLastSent {
  x: number;
  y: number;
  entityId: string;
  /** id of the last trim_preview command actually written to the core. */
  requestId: string | null;
}

interface TrimPointerMoveParams {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  activeSketchPlaneId: string;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  activeSketchPlaneFrameRef: MutableRef<SketchPlaneFrame | null>;
  sceneDataRef: MutableRef<ViewportScene | null>;
  trimPreviewLastSentRef: MutableRef<TrimPreviewLastSent | null>;
  hoverActions: PointerMoveHoverActions;
  intersectSceneTargets: (event: PointerEvent) => ViewportPickHit | null;
  clearTrimSegmentHighlight: () => void;
  clearTrimArcHighlight: () => void;
}

export function handleTrimPointerMove({
  event,
  renderer,
  camera,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  activeSketchPlaneFrameRef,
  sceneDataRef,
  trimPreviewLastSentRef,
  hoverActions,
  intersectSceneTargets,
  clearTrimSegmentHighlight,
  clearTrimArcHighlight,
}: TrimPointerMoveParams) {
  void activeSketchPlaneFrameRef;
  void sceneDataRef;
  const trimHit = intersectSceneTargets(event);
  applyTrimToolHover(trimHit, hoverActions);

  const entityKind = trimEntityKind(trimHit);
  if (!trimHit || trimHit.kind !== "sketch_entity" || !entityKind) {
    clearTrimHighlights(clearTrimSegmentHighlight, clearTrimArcHighlight);
    trimPreviewLastSentRef.current = null;
    return;
  }

  const rawPoint = resolveSketchPlanePoint(
    event,
    renderer,
    camera,
    activeSketchPlaneId,
    activeSketchPlaneFrame,
  );
  if (!rawPoint) {
    clearTrimHighlights(clearTrimSegmentHighlight, clearTrimArcHighlight);
    trimPreviewLastSentRef.current = null;
    return;
  }

  // Single-authority preview: the red highlight is rendered ONLY from
  // the core's trim_preview_result (the event handler in ViewportPanel
  // redraws it on every response), and the trim deletes exactly the
  // hovered index that result reports. A second, local TS preview
  // computation used to race it — the highlight showed one segment
  // while the trim deleted the core's pick for a slightly different
  // point, which is the "excessive trimming" users saw on dense
  // circle arrangements. Sending here is the whole job.
  sendTrimPreviewIfMoved(trimPreviewLastSentRef, trimHit.id, rawPoint.local);
}

function trimEntityKind(hit: ViewportPickHit | null): TrimEntityKind | null {
  if (
    hit?.kind === "sketch_entity" &&
    (hit.entityKind === "line" ||
      hit.entityKind === "circle" ||
      hit.entityKind === "arc")
  ) {
    return hit.entityKind;
  }
  return null;
}

// At most one trim_preview command per animation frame, always the
// newest request. Without this, a fast pointer sweep queues a preview
// per pointermove event and responses can arrive in any order; the
// viewport renders whatever lands last, which may not be the request
// the user's cursor is currently on.
let scheduledPreview: number | null = null;
let scheduledPreviewEntity: string | null = null;
let scheduledPreviewX = 0;
let scheduledPreviewY = 0;
let scheduledPreviewLastSentRef: MutableRef<TrimPreviewLastSent | null> | null =
  null;

function flushScheduledPreview() {
  scheduledPreview = null;
  const ref = scheduledPreviewLastSentRef;
  const entityId = scheduledPreviewEntity;
  if (ref === null || entityId === null) return;
  const requestId = crypto.randomUUID();
  ref.current = {
    x: scheduledPreviewX,
    y: scheduledPreviewY,
    entityId,
    requestId,
  };
  void sendCoreCommand(
    makeTrimPreviewCommand(entityId, scheduledPreviewX, scheduledPreviewY, requestId),
  );
}

function sendTrimPreviewIfMoved(
  trimPreviewLastSentRef: MutableRef<TrimPreviewLastSent | null>,
  entityId: string,
  cursorLocal: [number, number],
) {
  const [mx, my] = cursorLocal;
  const prev = trimPreviewLastSentRef.current;
  // The last-sent gate also stores WHICH entity the preview targeted:
  // hovering a different entity must send immediately.
  const entityChanged =
    !prev || prev.entityId !== entityId;
  if (
    entityChanged ||
    Math.abs(mx - prev.x) > 0.5 ||
    Math.abs(my - prev.y) > 0.5
  ) {
    scheduledPreviewEntity = entityId;
    scheduledPreviewX = mx;
    scheduledPreviewY = my;
    scheduledPreviewLastSentRef = trimPreviewLastSentRef;
    if (scheduledPreview === null) {
      scheduledPreview = requestAnimationFrame(flushScheduledPreview);
    }
  }
}
