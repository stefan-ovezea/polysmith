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

interface TrimPointerMoveParams {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  activeSketchPlaneId: string;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  activeSketchPlaneFrameRef: MutableRef<SketchPlaneFrame | null>;
  sceneDataRef: MutableRef<ViewportScene | null>;
  trimPreviewLastSentRef: MutableRef<{ x: number; y: number } | null>;
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

function sendTrimPreviewIfMoved(
  trimPreviewLastSentRef: MutableRef<{ x: number; y: number } | null>,
  entityId: string,
  cursorLocal: [number, number],
) {
  const [mx, my] = cursorLocal;
  const prev = trimPreviewLastSentRef.current;
  // The last-sent gate also stores WHICH entity the preview targeted:
  // hovering a different entity must send immediately.
  const entityChanged =
    !prev || (prev as { entityId?: string }).entityId !== entityId;
  if (
    entityChanged ||
    Math.abs(mx - prev.x) > 0.5 ||
    Math.abs(my - prev.y) > 0.5
  ) {
    trimPreviewLastSentRef.current = {
      x: mx,
      y: my,
      entityId,
    } as { x: number; y: number };
    void sendCoreCommand(makeTrimPreviewCommand(entityId, mx, my));
  }
}
