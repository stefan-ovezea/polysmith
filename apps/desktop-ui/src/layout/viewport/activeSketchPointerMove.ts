import * as THREE from "three";

import type {
  SketchPlaneFrame,
  SketchPreviewPoint,
  SketchTool,
  ViewportScene,
} from "@/types";
import type { ArcToolMode } from "./arcDraftPreview";
import type { CircleToolMode } from "./circleDraftPreview";
import type { ConstraintPreviewState } from "./constraintPreview";
import type { DraftDimensionSession } from "./draftDimensions";
import {
  resolveDraftPointerMove,
  type DraftPointerMoveParams,
  type MutableRef,
} from "./draftPointerMove";
import {
  renderDraftPointerPreview,
  type DraftPointerPreviewControls,
} from "./draftPointerPreview";
import {
  applyProjectToolHover,
  applySelectToolHover,
  clearSketchEntityHover,
  type PointerMoveHoverActions,
} from "./pointerMoveHover";
import type { RectangleToolMode } from "./rectangleDraftPreview";
import { handleTrimPointerMove } from "./trimPointerMove";
import type { TrimLineHighlightSegment } from "./trimHoverPreview";
import type { ViewportPickHit } from "./contextMenuState";

interface ActiveSketchPointerMoveParams
  extends DraftPointerMoveParams,
    DraftPointerPreviewControls {
  activeSketchPlaneFrameRef: MutableRef<SketchPlaneFrame | null>;
  sceneDataRef: MutableRef<ViewportScene | null>;
  trimPreviewLastSentRef: MutableRef<{ x: number; y: number } | null>;
  hoverActions: PointerMoveHoverActions;
  intersectSceneTargets: (event: PointerEvent) => ViewportPickHit | null;
  sketchGroupRef: MutableRef<THREE.Group | null>;
  clearTrimSegmentHighlight: () => void;
  clearTrimArcHighlight: () => void;
  updateTrimSegmentHighlight: (
    lineId: string,
    segments: TrimLineHighlightSegment[],
    hoveredSegmentIndex: number,
  ) => void;
  updateTrimArcHighlight: (worldPoints: Array<[number, number, number]>) => void;
}

export function handleActiveSketchPointerMove(params: ActiveSketchPointerMoveParams) {
  if (params.activeSketchTool === "select") {
    applySelectToolHover(params.intersectSceneTargets(params.event), params.hoverActions);
    return;
  }

  if (params.activeSketchTool === "project") {
    applyProjectToolHover(params.intersectSceneTargets(params.event), params.hoverActions);
    return;
  }

  if (params.activeSketchTool === "trim") {
    handleTrimPointerMove(params);
    return;
  }

  handleDraftToolPointerMove(params);
}

function handleDraftToolPointerMove(params: ActiveSketchPointerMoveParams) {
  clearSketchEntityHover(params.hoverActions);
  const draftMove = resolveDraftPointerMove(params);
  if (!draftMove) {
    return;
  }

  const { draftStart, draftPreviewLocal, sketchPoint } = draftMove;
  if (!draftStart) {
    // Tool is armed but no line started yet — snap feedback (crosshair
    // position, snap label, constraint preview) already updated by
    // resolveDraftPointerMove. Just clear hover and skip the rubber-band.
    params.hoverActions.setHoveredPrimitive(null);
    params.hoverActions.setHoveredReference(null);
    return;
  }

  const sketchGroup = params.sketchGroupRef.current;
  if (!sketchGroup) {
    return;
  }

  renderDraftPointerPreview({
    activeSketchTool: params.activeSketchTool,
    activeSketchPlaneId: params.activeSketchPlaneId,
    activeSketchPlaneFrame: params.activeSketchPlaneFrame,
    draftStart,
    draftPreviewLocal,
    sketchGroup,
    inferenceLines: sketchPoint.inferenceLines?.map((gl) => ({
      from: gl.from,
      draft: gl.draft,
    })),
    arcToolMode: params.arcToolMode,
    circleToolMode: params.circleToolMode,
    rectangleToolMode: params.rectangleToolMode,
    arcSecondPoint: params.arcSecondPoint,
    circleSecondPoint: params.circleSecondPoint,
    rectSecondPoint: params.rectSecondPoint,
    ellipseSecondPoint: params.ellipseSecondPoint,
    isConstruction: params.isConstruction,
    previewLineRef: params.previewLineRef,
    previewCircleRef: params.previewCircleRef,
    previewArcRef: params.previewArcRef,
    previewSlotRef: params.previewSlotRef,
    splineDraftPolesRef: params.splineDraftPolesRef,
    previewSplineRef: params.previewSplineRef,
    previewDimensionRef: params.previewDimensionRef,
    previewInferenceRef: params.previewInferenceRef,
    clearPreviewLine: params.clearPreviewLine,
    clearPreviewCircle: params.clearPreviewCircle,
    clearPreviewArc: params.clearPreviewArc,
    clearPreviewSlot: params.clearPreviewSlot,
    clearPreviewSpline: params.clearPreviewSpline,
    clearPreviewDimension: params.clearPreviewDimension,
    clearPreviewInference: params.clearPreviewInference,
  });
}
