import * as THREE from "three";

import type { SketchPlaneFrame, SketchTool } from "@/types";
import {
  buildSketchDimensionObject,
  distanceBetweenPoints,
  toWorldPoint,
} from "@/utils";
import { buildArcDraftPreview, type ArcToolMode } from "./arcDraftPreview";
import { buildCircleDraftPreview, type CircleToolMode } from "./circleDraftPreview";
import {
  buildDraftLinePreview,
  buildInferenceGuideLines,
} from "./draftLinePreview";
import { buildEllipseDraftPreview } from "./ellipseDraftPreview";
import { buildSlotDraftPreview } from "./slotDraftPreview";
import { formatDraftDimension } from "./draftDimensions";
import {
  buildRectangleDraftPreview,
  type RectangleToolMode,
} from "./rectangleDraftPreview";
import type { MutableRef } from "./draftPointerMove";

export interface DraftPointerPreviewControls {
  arcToolMode: ArcToolMode;
  circleToolMode: CircleToolMode;
  rectangleToolMode: RectangleToolMode;
  arcSecondPoint: [number, number] | null;
  circleSecondPoint: [number, number] | null;
  rectSecondPoint: [number, number] | null;
  ellipseSecondPoint: [number, number] | null;
  isConstruction: boolean;
  previewLineRef: MutableRef<THREE.Line | null>;
  previewCircleRef: MutableRef<THREE.LineLoop | null>;
  previewArcRef: MutableRef<THREE.Line | null>;
  // Slot previews are stadium groups (2 lines + 2 arcs) — one ref per
  // group, cleared recursively.
  previewSlotRef: MutableRef<THREE.Group | null>;
  previewDimensionRef: MutableRef<{
    line: THREE.Object3D;
    label: THREE.Sprite;
  } | null>;
  previewInferenceRef: MutableRef<THREE.Line[]>;
  clearPreviewLine: () => void;
  clearPreviewCircle: () => void;
  clearPreviewArc: () => void;
  clearPreviewSlot: () => void;
  clearPreviewDimension: () => void;
  clearPreviewInference: () => void;
}

interface DraftPointerPreviewParams extends DraftPointerPreviewControls {
  activeSketchTool: SketchTool;
  activeSketchPlaneId: string;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  draftStart: [number, number];
  draftPreviewLocal: [number, number];
  sketchGroup: THREE.Group;
  /** Inference guide lines to render as dotted alignment hints. */
  inferenceLines?: Array<{
    from: [number, number];
    draft: [number, number];
  }>;
}

export function renderDraftPointerPreview(params: DraftPointerPreviewParams) {
  params.clearPreviewLine();
  params.clearPreviewCircle();
  params.clearPreviewArc();
  params.clearPreviewSlot();
  params.clearPreviewDimension();
  params.clearPreviewInference();

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
  if (params.activeSketchTool === "ellipse") {
    renderEllipsePointerPreview(params);
    return;
  }
  if (params.activeSketchTool === "slot") {
    renderSlotPointerPreview(params);
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

function renderEllipsePointerPreview({
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  draftStart,
  draftPreviewLocal,
  sketchGroup,
  ellipseSecondPoint,
  isConstruction,
  previewCircleRef,
}: DraftPointerPreviewParams) {
  const preview = buildEllipseDraftPreview({
    start: draftStart,
    current: draftPreviewLocal,
    axisPoint: ellipseSecondPoint,
    planeId: activeSketchPlaneId,
    planeFrame: activeSketchPlaneFrame,
    isConstruction,
  });
  if (preview) {
    // The ellipse preview is a single LineLoop — reuse the circle
    // preview ref (same shape class, same clear path).
    previewCircleRef.current = preview;
    sketchGroup.add(preview);
  }
}

function renderSlotPointerPreview({
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  draftStart,
  draftPreviewLocal,
  sketchGroup,
  isConstruction,
  previewSlotRef,
}: DraftPointerPreviewParams) {
  const preview = buildSlotDraftPreview({
    start: draftStart,
    current: draftPreviewLocal,
    planeId: activeSketchPlaneId,
    planeFrame: activeSketchPlaneFrame,
    isConstruction,
  });
  if (preview) {
    previewSlotRef.current = preview;
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
  previewInferenceRef,
  inferenceLines,
}: DraftPointerPreviewParams) {
  const preview = buildDraftLinePreview({
    points: [draftStart, draftPreviewLocal],
    planeId: activeSketchPlaneId,
    planeFrame: activeSketchPlaneFrame,
    isConstruction,
  });
  previewLineRef.current = preview;
  sketchGroup.add(preview);

  // Render inference / tracking guide lines as dotted alignment hints.
  if (inferenceLines && inferenceLines.length > 0) {
    const guides = buildInferenceGuideLines({
      guides: inferenceLines,
      planeId: activeSketchPlaneId,
      planeFrame: activeSketchPlaneFrame,
    });
    guides.forEach((g) => sketchGroup.add(g));
    previewInferenceRef.current = guides;
  }
}
