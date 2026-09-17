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

  const requestId = crypto.randomUUID();
  cornerPreviewLastSentRef.current = {
    x: rawPoint.local[0],
    y: rawPoint.local[1],
    entityId: hit.id,
    requestId,
  };
  void sendCoreCommand(
    makeCornerTrimPreviewCommand(
      firstEntityId,
      hit.id,
      rawPoint.local[0],
      rawPoint.local[1],
      requestId,
    ),
  );
}
