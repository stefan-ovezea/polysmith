import * as THREE from "three";

import type { SketchPlaneFrame, SketchTool } from "@/types";
import { buildArcDraftPreview, type ArcToolMode } from "./arcDraftPreview";
import { buildCircleDraftPreview, type CircleToolMode } from "./circleDraftPreview";
import { buildDraftLinePreview } from "./draftLinePreview";
import {
  buildRectangleDraftPreview,
  type RectangleToolMode,
} from "./rectangleDraftPreview";

interface MutableRef<T> {
  current: T;
}

interface DraftPointerPreviewParams {
  activeSketchTool: SketchTool;
  activeSketchPlaneId: string;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  draftStart: [number, number];
  draftPreviewLocal: [number, number];
  sketchGroup: THREE.Group;
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
  clearPreviewLine: () => void;
  clearPreviewCircle: () => void;
  clearPreviewArc: () => void;
  clearPreviewDimension: () => void;
  renderCircleDraftDimension: (
    sketchGroup: THREE.Group,
    center: [number, number],
    edge: [number, number],
  ) => void;
}

export function renderDraftPointerPreview(params: DraftPointerPreviewParams) {
  params.clearPreviewLine();
  params.clearPreviewCircle();
  params.clearPreviewArc();
  params.clearPreviewDimension();

  if (params.activeSketchTool === "arc") {
    renderArcPointerPreview(params);
    return;
  }
  if (params.activeSketchTool === "circle") {
    renderCirclePointerPreview(params);
    return;
  }
  if (params.activeSketchTool === "rectangle") {
    renderRectanglePointerPreview(params);
    return;
  }

  renderLinePointerPreview(params);
}

function renderArcPointerPreview({
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  draftStart,
  draftPreviewLocal,
  sketchGroup,
  arcToolMode,
  arcSecondPoint,
  isConstruction,
  previewArcRef,
}: DraftPointerPreviewParams) {
  const preview = buildArcDraftPreview({
    mode: arcToolMode,
    start: draftStart,
    current: draftPreviewLocal,
    secondPoint: arcSecondPoint,
    planeId: activeSketchPlaneId,
    planeFrame: activeSketchPlaneFrame,
    isConstruction,
  });
  if (preview) {
    previewArcRef.current = preview;
    sketchGroup.add(preview);
  }
}

function renderCirclePointerPreview({
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  draftStart,
  draftPreviewLocal,
  sketchGroup,
  circleToolMode,
  circleSecondPoint,
  isConstruction,
  previewLineRef,
  previewCircleRef,
  renderCircleDraftDimension,
}: DraftPointerPreviewParams) {
  const preview = buildCircleDraftPreview({
    mode: circleToolMode,
    start: draftStart,
    current: draftPreviewLocal,
    secondPoint: circleSecondPoint,
    planeId: activeSketchPlaneId,
    planeFrame: activeSketchPlaneFrame,
    isConstruction,
  });
  if (!preview) {
    return;
  }
  if (preview.kind === "hint") {
    previewLineRef.current = preview.object;
  } else {
    previewCircleRef.current = preview.object;
  }
  sketchGroup.add(preview.object);
  if (preview.renderDraftDimension) {
    renderCircleDraftDimension(sketchGroup, draftStart, draftPreviewLocal);
  }
}

function renderRectanglePointerPreview({
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  draftStart,
  draftPreviewLocal,
  sketchGroup,
  rectangleToolMode,
  rectSecondPoint,
  isConstruction,
  previewLineRef,
}: DraftPointerPreviewParams) {
  const preview = buildRectangleDraftPreview({
    mode: rectangleToolMode,
    start: draftStart,
    current: draftPreviewLocal,
    secondPoint: rectSecondPoint,
    planeId: activeSketchPlaneId,
    planeFrame: activeSketchPlaneFrame,
    isConstruction,
  });
  if (preview) {
    previewLineRef.current = preview;
    sketchGroup.add(preview);
  }
}

function renderLinePointerPreview({
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  draftStart,
  draftPreviewLocal,
  sketchGroup,
  isConstruction,
  previewLineRef,
}: DraftPointerPreviewParams) {
  const preview = buildDraftLinePreview({
    points: [draftStart, draftPreviewLocal],
    planeId: activeSketchPlaneId,
    planeFrame: activeSketchPlaneFrame,
    isConstruction,
  });
  previewLineRef.current = preview;
  sketchGroup.add(preview);
}
