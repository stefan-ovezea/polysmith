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
import { buildSplineDraftPreview } from "./splineDraftPreview";
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
  previewCircleRef: MutableRef<THREE.LineLoop | THREE.Line | null>;
  previewArcRef: MutableRef<THREE.Line | null>;
  // Slot previews are stadium groups (2 lines + 2 arcs) — one ref per
  // group, cleared recursively.
  previewSlotRef: MutableRef<THREE.Group | null>;
  // Spline draft state: the placed poles + the preview group
  // (curve + control polygon + rubber segment).
  splineDraftPolesRef: MutableRef<[number, number][]>;
  previewSplineRef: MutableRef<THREE.Group | null>;
  clearPreviewSpline: () => void;
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
  if (params.activeSketchTool === "spline") {
    renderSplinePointerPreview(params);
    return;
  }

  renderLinePointerPreview(params);
}

function renderSplinePointerPreview({
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  draftPreviewLocal,
  sketchGroup,
  splineDraftPolesRef,
  previewSplineRef,
  clearPreviewSpline,
  isConstruction,
}: DraftPointerPreviewParams) {
  clearPreviewSpline();
  const preview = buildSplineDraftPreview({
    poles: splineDraftPolesRef.current,
    planeId: activeSketchPlaneId,
    planeFrame: activeSketchPlaneFrame,
    isConstruction,
    cursor: draftPreviewLocal,
  });
  if (preview) {
    previewSplineRef.current = preview;
    sketchGroup.add(preview);
  }
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
  previewDimensionRef,
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

  // Three-point arc, second click pending: read the chord between the
  // two end vertices as the user places the second one.
  if (arcToolMode === "three_point" && !arcSecondPoint) {
    renderChordDraftDimension({
      activeSketchPlaneId,
      activeSketchPlaneFrame,
      sketchGroup,
      previewDimensionRef,
      start: draftStart,
      end: draftPreviewLocal,
    });
  }
}

function renderChordDraftDimension({
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  sketchGroup,
  previewDimensionRef,
  start,
  end,
}: Pick<
  DraftPointerPreviewParams,
  "activeSketchPlaneId" | "activeSketchPlaneFrame" | "sketchGroup" | "previewDimensionRef"
> & {
  start: [number, number];
  end: [number, number];
}) {
  const length = distanceBetweenPoints(start, end);
  if (length <= 0.001) {
    return;
  }

  // point_distance renders the generic linear dimension — extension
  // lines, arrows, and a label offset perpendicular to the chord.
  const nx = -(end[1] - start[1]) / length;
  const ny = (end[0] - start[0]) / length;
  const offset = 8;
  const toWorld = (local: [number, number]) =>
    toWorldPoint(activeSketchPlaneId, local, activeSketchPlaneFrame);
  const midX = (start[0] + end[0]) / 2;
  const midY = (start[1] + end[1]) / 2;
  const draftDimension = buildSketchDimensionObject({
    dimensionId: "preview-arc-chord",
    planeId: activeSketchPlaneId,
    kind: "point_distance",
    entityId: "preview-arc",
    label: `${formatDraftDimension(length)} mm`,
    rawValue: length,
    unitSuffix: "mm",
    isSelected: false,
    anchorStart: toWorld(start),
    anchorEnd: toWorld(end),
    dimensionStart: toWorld(start),
    dimensionEnd: toWorld(end),
    labelPosition: toWorld([midX + nx * offset, midY + ny * offset]),
  });
  previewDimensionRef.current = draftDimension;
  sketchGroup.add(draftDimension.line);
  sketchGroup.add(draftDimension.label);
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
  if (
    circleToolMode === "tangent_two_lines" ||
    circleToolMode === "tangent_three_lines"
  ) {
    // Tangent modes pick lines — no rubber-band preview.
    return;
  }
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

  // Mirrors the field convention the core emits for circle_radius: the
  // near rim contact, a quarter-turn rim point as the renderer's in-plane
  // direction reference, both tips of the through-centre line, and the
  // centre/radius in the arc fields.
  const ux = dx / length;
  const uy = dy / length;
  const nearContactLocal: [number, number] = [
    center[0] + ux * radius,
    center[1] + uy * radius,
  ];
  const farContactLocal: [number, number] = [
    center[0] - ux * radius,
    center[1] - uy * radius,
  ];
  const quadrantLocal: [number, number] = [
    center[0] - uy * radius,
    center[1] + ux * radius,
  ];
  const labelLocal: [number, number] = [
    center[0] + ux * (radius + 4),
    center[1] + uy * (radius + 4),
  ];
  const toWorld = (local: [number, number]) =>
    toWorldPoint(activeSketchPlaneId, local, activeSketchPlaneFrame);
  const centerWorld = toWorld(center);
  const draftDimension = buildSketchDimensionObject({
    dimensionId: "preview-circle-diameter",
    planeId: activeSketchPlaneId,
    kind: "circle_radius",
    entityId: "preview-circle",
    label: `D ${formatDraftDimension(radius * 2)} mm`,
    rawValue: radius * 2,
    unitSuffix: "mm",
    isSelected: false,
    anchorStart: toWorld(nearContactLocal),
    anchorEnd: toWorld(quadrantLocal),
    dimensionStart: toWorld(farContactLocal),
    dimensionEnd: toWorld(nearContactLocal),
    labelPosition: toWorld(labelLocal),
    arcCenter: centerWorld,
    arcRadius: radius,
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
