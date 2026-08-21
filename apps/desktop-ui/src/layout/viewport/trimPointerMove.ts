import { clearTrimHighlights } from "./previewObjectCleanup";
import * as THREE from "three";

import { sendCoreCommand } from "@/lib/cadCoreClient";
import { makeTrimPreviewCommand } from "@/lib/ipcProtocol";
import type { SketchPlaneFrame, ViewportScene } from "@/types";
import { resolveSketchPlanePoint } from "@/utils";
import type { ViewportPickHit } from "./contextMenuState";
import type { PointerMoveHoverActions } from "./pointerMoveHover";
import { applyTrimToolHover } from "./pointerMoveHover";
import {
  computeTrimHoverPreview,
  type TrimHoverPreview,
  type TrimLineHighlightSegment,
} from "./trimHoverPreview";

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
  updateTrimSegmentHighlight: (
    lineId: string,
    segments: TrimLineHighlightSegment[],
    hoveredSegmentIndex: number,
  ) => void;
  updateTrimArcHighlight: (worldPoints: Array<[number, number, number]>) => void;
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
  updateTrimSegmentHighlight,
  updateTrimArcHighlight,
}: TrimPointerMoveParams) {
  const trimHit = intersectSceneTargets(event);
  applyTrimToolHover(trimHit, hoverActions);

  const sceneData = sceneDataRef.current;
  const entityKind = trimEntityKind(trimHit);
  if (!sceneData || !trimHit || trimHit.kind !== "sketch_entity" || !entityKind) {
    clearTrimHighlights(clearTrimSegmentHighlight, clearTrimArcHighlight);
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
    return;
  }

  sendTrimPreviewIfMoved(trimPreviewLastSentRef, trimHit.id, rawPoint.local);

  renderTrimPreview(
    computeTrimHoverPreview({
      sceneData,
      target: { id: trimHit.id, entityKind },
      cursorLocal: rawPoint.local,
      planeId: activeSketchPlaneId,
      planeFrame: activeSketchPlaneFrameRef.current,
    }),
    {
      clearTrimSegmentHighlight,
      clearTrimArcHighlight,
      updateTrimSegmentHighlight,
      updateTrimArcHighlight,
    },
  );
}

function renderTrimPreview(
  preview: TrimHoverPreview | null,
  {
    clearTrimSegmentHighlight,
    clearTrimArcHighlight,
    updateTrimSegmentHighlight,
    updateTrimArcHighlight,
  }: Pick<
    TrimPointerMoveParams,
    | "clearTrimSegmentHighlight"
    | "clearTrimArcHighlight"
    | "updateTrimSegmentHighlight"
    | "updateTrimArcHighlight"
  >,
) {
  if (!preview) {
    clearTrimHighlights(clearTrimSegmentHighlight, clearTrimArcHighlight);
    return;
  }

  if (preview.kind === "line") {
    updateTrimSegmentHighlight(
      preview.lineId,
      preview.segments,
      preview.hoveredSegmentIndex,
    );
    return;
  }

  clearTrimSegmentHighlight();
  updateTrimArcHighlight(preview.points);
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
  if (!prev || Math.abs(mx - prev.x) > 0.5 || Math.abs(my - prev.y) > 0.5) {
    trimPreviewLastSentRef.current = { x: mx, y: my };
    void sendCoreCommand(makeTrimPreviewCommand(entityId, mx, my));
  }
}
