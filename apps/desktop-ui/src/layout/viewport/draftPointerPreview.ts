import * as THREE from "three";

import type { SketchPlaneFrame, SketchTool } from "@/types";
import {
  buildSketchDimensionObject,
  distanceBetweenPoints,
  toWorldPoint,
} from "@/utils";
import { buildArcDraftPreview, type ArcToolMode } from "./arcDraftPreview";
import { buildCircleDraftPreview, type CircleToolMode } from "./circleDraftPreview";
import { buildDraftLinePreview } from "./draftLinePreview";
import { formatDraftDimension } from "./draftDimensions";
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
  previewDimensionRef: MutableRef<{
    line: THREE.Object3D;
    label: THREE.Sprite;
  } | null>;
  clearPreviewLine: () => void;
  clearPreviewCircle: () => void;
  clearPreviewArc: () => void;
  clearPreviewDimension: () => void;
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
  previewDimensionRef,
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
    renderCircleDraftDimension({
      activeSketchPlaneId,
      activeSketchPlaneFrame,
      sketchGroup,
      previewDimensionRef,
      center: draftStart,
      edge: draftPreviewLocal,
    });
  }
}

function renderCircleDraftDimension({
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  sketchGroup,
  previewDimensionRef,
  center,
  edge,
}: Pick<
  DraftPointerPreviewParams,
  "activeSketchPlaneId" | "activeSketchPlaneFrame" | "sketchGroup" | "previewDimensionRef"
> & {
  center: [number, number];
  edge: [number, number];
}) {
  const radius = distanceBetweenPoints(center, edge);
  const dx = edge[0] - center[0];
  const dy = edge[1] - center[1];
  const length = Math.hypot(dx, dy);
  if (radius <= 0.001 || length <= 1e-6) {
    return;
  }

  const ux = dx / length;
  const uy = dy / length;
  const dimensionStartLocal: [number, number] = [
    center[0] - ux * radius,
    center[1] - uy * radius,
  ];
  const dimensionEndLocal: [number, number] = [
    center[0] + ux * radius,
    center[1] + uy * radius,
  ];
  const labelLocal: [number, number] = [
    center[0] + ux * (radius + 4),
    center[1] + uy * (radius + 4),
  ];
  const draftDimension = buildSketchDimensionObject({
    dimensionId: "preview-circle-diameter",
    planeId: activeSketchPlaneId,
    kind: "circle_radius",
    entityId: "preview-circle",
    label: `D ${formatDraftDimension(radius * 2)} mm`,
    rawValue: radius * 2,
    unitSuffix: "mm",
    isSelected: false,
    anchorStart: toWorldPoint(
      activeSketchPlaneId,
      dimensionStartLocal,
      activeSketchPlaneFrame,
    ),
    anchorEnd: toWorldPoint(
      activeSketchPlaneId,
      dimensionEndLocal,
      activeSketchPlaneFrame,
    ),
    dimensionStart: toWorldPoint(
      activeSketchPlaneId,
      dimensionStartLocal,
      activeSketchPlaneFrame,
    ),
    dimensionEnd: toWorldPoint(
      activeSketchPlaneId,
      dimensionEndLocal,
      activeSketchPlaneFrame,
    ),
    labelPosition: toWorldPoint(
      activeSketchPlaneId,
      labelLocal,
      activeSketchPlaneFrame,
    ),
  });
  previewDimensionRef.current = draftDimension;
  sketchGroup.add(draftDimension.line);
  sketchGroup.add(draftDimension.label);
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
