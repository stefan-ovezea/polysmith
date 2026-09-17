// Corner-trim hover pointer move.
//
// While the corner tool holds a first pick, hovering a line/arc sends
// corner_trim_preview (throttled: at most one per 0.5 mm of movement
// or on entity change). The viewport renders the response — the ghost
// of the two resulting segments and the corner — exclusively from the
// core's result, the same single-authority contract as the trim
// preview.

import * as THREE from "three";

import { sendCoreCommand } from "@/lib/cadCoreClient";
import { makeCornerTrimPreviewCommand } from "@/lib/ipcProtocol";
import type { SketchPlaneFrame } from "@/types";
import { resolveSketchPlanePoint } from "@/utils";
import type { ViewportPickHit } from "./contextMenuState";
import { applyTrimToolHover, type PointerMoveHoverActions } from "./pointerMoveHover";

interface MutableRef<T> {
  current: T;
}

interface CornerPreviewLastSent {
  x: number;
  y: number;
  entityId: string;
  requestId: string | null;
}

export interface CornerTrimPointerMoveParams {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  activeSketchPlaneId: string;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  cornerFirstEntityIdRef: MutableRef<string | null>;
  cornerPreviewLastSentRef: MutableRef<CornerPreviewLastSent | null>;
  hoverActions: PointerMoveHoverActions;
  intersectSceneTargets: (event: PointerEvent) => ViewportPickHit | null;
  clearCornerPreview: () => void;
}

export function handleCornerTrimPointerMove({
  event,
  renderer,
  camera,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  cornerFirstEntityIdRef,
  cornerPreviewLastSentRef,
  hoverActions,
  intersectSceneTargets,
  clearCornerPreview,
}: CornerTrimPointerMoveParams) {
  const firstEntityId = cornerFirstEntityIdRef.current;
  const hit = intersectSceneTargets(event);
  applyTrimToolHover(hit, hoverActions);

  // No first pick yet: nothing to preview (the first pick itself is
  // the ordinary click).
  if (!firstEntityId) {
    return;
  }

  const isLineOrArc =
    hit?.kind === "sketch_entity" &&
    (hit.entityKind === "line" || hit.entityKind === "arc");
  if (!isLineOrArc || hit.id === firstEntityId) {
    cornerPreviewLastSentRef.current = null;
    clearCornerPreview();
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
    return;
  }

  const prev = cornerPreviewLastSentRef.current;
  const entityChanged = !prev || prev.entityId !== hit.id;
  if (
    !entityChanged &&
    Math.abs(rawPoint.local[0] - prev.x) <= 0.5 &&
    Math.abs(rawPoint.local[1] - prev.y) <= 0.5
  ) {
    return;
  }

  // Same one-in-flight cap as the trim preview (dense sketches would
  // otherwise flood the core queue and freeze every later command).
  pendingCornerFirstId = firstEntityId;
  pendingCornerEntityId = hit.id;
  pendingCornerX = rawPoint.local[0];
  pendingCornerY = rawPoint.local[1];
  pendingCornerLastSentRef = cornerPreviewLastSentRef;
  if (!cornerPreviewSlotFree()) {
    cornerPreviewPending = true;
    return;
  }
  cornerPreviewPending = false;
  sendCornerTrimPreviewNow();
}

let cornerPreviewInFlightId: string | null = null;
let cornerPreviewInFlightSince = 0;
let cornerPreviewPending = false;

// Same safety valve as the trim preview: a reply lost (core restart,
// dropped response) must not wedge the slot forever.
const CORNER_PREVIEW_STUCK_AFTER_MS = 10_000;

function cornerPreviewSlotFree(): boolean {
  if (cornerPreviewInFlightId === null) {
    return true;
  }
  if (
    performance.now() - cornerPreviewInFlightSince >
    CORNER_PREVIEW_STUCK_AFTER_MS
  ) {
    cornerPreviewInFlightId = null;
    return true;
  }
  return false;
}
let pendingCornerFirstId: string | null = null;
let pendingCornerEntityId: string | null = null;
let pendingCornerX = 0;
let pendingCornerY = 0;
let pendingCornerLastSentRef: MutableRef<CornerPreviewLastSent | null> | null =
  null;

function sendCornerTrimPreviewNow() {
  const ref = pendingCornerLastSentRef;
  const firstEntityId = pendingCornerFirstId;
  const entityId = pendingCornerEntityId;
  if (ref === null || firstEntityId === null || entityId === null) {
    return;
  }
  const requestId = crypto.randomUUID();
  cornerPreviewInFlightId = requestId;
  cornerPreviewInFlightSince = performance.now();
  ref.current = {
    x: pendingCornerX,
    y: pendingCornerY,
    entityId,
    requestId,
  };
  void sendCoreCommand(
    makeCornerTrimPreviewCommand(
      firstEntityId,
      entityId,
      pendingCornerX,
      pendingCornerY,
      requestId,
    ),
  );
}

// Called by the viewport when a corner_trim_preview_result arrives.
// Releases the in-flight slot and sends the newest pending request.
export function notifyCornerTrimPreviewResponse(requestId: string) {
  if (
    cornerPreviewInFlightId === null ||
    cornerPreviewInFlightId !== requestId
  ) {
    return;
  }
  cornerPreviewInFlightId = null;
  if (cornerPreviewPending) {
    cornerPreviewPending = false;
    sendCornerTrimPreviewNow();
  }
}
