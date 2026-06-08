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
import { resolveDraftPointerMove } from "./draftPointerMove";
import { renderDraftPointerPreview } from "./draftPointerPreview";
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

interface MutableRef<T> {
  current: T;
}

interface ActiveSketchPointerMoveParams {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  activeSketchPlaneId: string;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  activeSketchTool: SketchTool;
  activeSketchPlaneFrameRef: MutableRef<SketchPlaneFrame | null>;
  sceneDataRef: MutableRef<ViewportScene | null>;
  trimPreviewLastSentRef: MutableRef<{ x: number; y: number } | null>;
  hoverActions: PointerMoveHoverActions;
  intersectSceneTargets: (event: PointerEvent) => ViewportPickHit | null;
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
  sketchGroupRef: MutableRef<THREE.Group | null>;
  arcToolMode: ArcToolMode;
  circleToolMode: CircleToolMode;
  rectangleToolMode: RectangleToolMode;
  arcSecondPoint: [number, number] | null;
  circleSecondPoint: [number, number] | null;
  rectSecondPoint: [number, number] | null;
  isConstruction: boolean;
  previewLineRef: MutableRef<THREE.Line | null>;
  previewCircleRef: MutableRef<THREE.LineLoop | null>;
  previewArcRef: MutableRef<THREE.Line | null>;
  previewDimensionRef: MutableRef<{
    line: THREE.Object3D;
    label: THREE.Sprite;
  } | null>;
  clearPreviewLine: () => void;
  clearPreviewCircle: () => void;
  clearPreviewArc: () => void;
  clearPreviewDimension: () => void;
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
  const draftMove = resolveDraftPointerMove({
    event: params.event,
    renderer: params.renderer,
    camera: params.camera,
    activeSketchPlaneId: params.activeSketchPlaneId,
    activeSketchPlaneFrame: params.activeSketchPlaneFrame,
    activeSketchTool: params.activeSketchTool,
    draftStartRef: params.draftStartRef,
    draftDimensionSessionRef: params.draftDimensionSessionRef,
    resolveSnappedSketchPoint: params.resolveSnappedSketchPoint,
    updateDraftSessionFromPoint: params.updateDraftSessionFromPoint,
    setSketchSnapLabel: params.setSketchSnapLabel,
    setConstraintPreview: params.setConstraintPreview,
  });
  if (!draftMove) {
    return;
  }

  const { draftStart, draftPreviewLocal } = draftMove;
  if (!draftStart) {
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
    arcToolMode: params.arcToolMode,
    circleToolMode: params.circleToolMode,
    rectangleToolMode: params.rectangleToolMode,
    arcSecondPoint: params.arcSecondPoint,
    circleSecondPoint: params.circleSecondPoint,
    rectSecondPoint: params.rectSecondPoint,
    isConstruction: params.isConstruction,
    previewLineRef: params.previewLineRef,
    previewCircleRef: params.previewCircleRef,
    previewArcRef: params.previewArcRef,
    previewDimensionRef: params.previewDimensionRef,
    clearPreviewLine: params.clearPreviewLine,
    clearPreviewCircle: params.clearPreviewCircle,
    clearPreviewArc: params.clearPreviewArc,
    clearPreviewDimension: params.clearPreviewDimension,
  });
}
