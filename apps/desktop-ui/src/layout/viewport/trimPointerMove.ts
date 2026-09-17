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

type TrimEntityKind = "line" | "circle" | "arc" | "ellipse" | "spline";

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
  // Drag-paint stroke (R5): while the pointer is down in trim mode,
  // every hovered entity is recorded with the cursor position it was
  // crossed at. Null when no stroke is in progress.
  trimStrokeRef?: MutableRef<Map<string, { x: number; y: number }> | null>;
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
  trimStrokeRef,
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

  // Stroke accumulation: the latest crossing position per entity wins
  // (the core re-derives each segment from these click points).
  trimStrokeRef?.current?.set(trimHit.id, {
    x: rawPoint.local[0],
    y: rawPoint.local[1],
  });

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
      hit.entityKind === "arc" ||
      hit.entityKind === "ellipse" ||
      hit.entityKind === "spline")
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
//
// AND at most ONE preview in flight (M-freeze fix): on a dense sketch
// (e.g. a 910-line STEP projection) each trim_preview costs ~100-300ms
// in the core, which processes commands sequentially. One preview per
// frame regardless of completion meant a 3-second hover queued ~180
// previews — every later command (including the trim click itself) sat
// behind minutes of backlog and the app froze completely. The newest
// request is kept pending and sent when the in-flight response lands,
// so the core queue can never grow past one preview.
let scheduledPreview: number | null = null;
let scheduledPreviewEntity: string | null = null;
let scheduledPreviewX = 0;
let scheduledPreviewY = 0;
let scheduledPreviewLastSentRef: MutableRef<TrimPreviewLastSent | null> | null =
  null;
let previewInFlightId: string | null = null;
let previewInFlightSince = 0;
let previewPending = false;

// Safety valve: the core answers a preview in well under a second on
// any sketch. A slot still occupied after this long means the reply
// was lost (core restart, dropped response) — release it so previews
// don't wedge.
const PREVIEW_STUCK_AFTER_MS = 10_000;

function previewSlotFree(): boolean {
  if (previewInFlightId === null) {
    return true;
  }
  if (performance.now() - previewInFlightSince > PREVIEW_STUCK_AFTER_MS) {
    previewInFlightId = null;
    return true;
  }
  return false;
}

function sendTrimPreviewNow(entityId: string, x: number, y: number) {
  const ref = scheduledPreviewLastSentRef;
  if (ref === null) {
    return;
  }
  const requestId = crypto.randomUUID();
  previewInFlightId = requestId;
  previewInFlightSince = performance.now();
  ref.current = { x, y, entityId, requestId };
  void sendCoreCommand(makeTrimPreviewCommand(entityId, x, y, requestId));
}

function flushScheduledPreview() {
  scheduledPreview = null;
  const entityId = scheduledPreviewEntity;
  if (entityId === null) return;
  if (!previewSlotFree()) {
    // A preview is still being computed — keep this latest request
    // pending; it goes out when the response arrives.
    previewPending = true;
    return;
  }
  previewPending = false;
  sendTrimPreviewNow(entityId, scheduledPreviewX, scheduledPreviewY);
}

// Called by the viewport when a trim_preview_result arrives. Releases
// the in-flight slot and sends the newest pending request, if any.
export function notifyTrimPreviewResponse(requestId: string) {
  if (previewInFlightId === null || previewInFlightId !== requestId) {
    return;
  }
  previewInFlightId = null;
  if (previewPending) {
    previewPending = false;
    if (scheduledPreviewEntity !== null) {
      sendTrimPreviewNow(
        scheduledPreviewEntity,
        scheduledPreviewX,
        scheduledPreviewY,
      );
    }
  }
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
