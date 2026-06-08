import * as THREE from "three";

import type { SketchPlaneFrame } from "@/types";
import {
  projectWorldPointToViewport,
  toWorldPoint,
} from "@/utils";
import {
  appendFilledArrow,
  buildFilledArrowMesh,
} from "@/utils/viewport/dimensionGeometry";
import type { DraftDimensionSession } from "./draftDimensions";
import { getOrthographicViewHeight } from "./grid";

export type DraftDimensionScreenPositions = Partial<
  Record<"length" | "angle", { x: number; y: number }>
>;

export type DraftDimensionPreviewResult =
  | {
      kind: "none";
    }
  | {
      kind: "positions";
      screenPositions: DraftDimensionScreenPositions;
    }
  | {
      kind: "group";
      group: THREE.Group;
      screenPositions: DraftDimensionScreenPositions;
    };

export function buildDraftDimensionPreview({
  session,
  camera,
  renderer,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  previousLineAngle,
}: {
  session: DraftDimensionSession;
  camera: THREE.OrthographicCamera;
  renderer: THREE.WebGLRenderer;
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  previousLineAngle: number | null;
}): DraftDimensionPreviewResult {
  const [sx, sy] = session.start;
  const [ex, ey] = session.current;
  const dx = ex - sx;
  const dy = ey - sy;
  const len = Math.hypot(dx, dy);
  if (len < 0.001) {
    return { kind: "none" };
  }

  if (session.tool !== "line") {
    return { kind: "positions", screenPositions: {} };
  }

  const planeId = activeSketchPlaneId ?? "ref-plane-xy";
  const planeFrame = activeSketchPlaneFrame;
  const startWorld = toWorldPoint(planeId, [sx, sy], planeFrame);
  const endWorld = toWorldPoint(planeId, [ex, ey], planeFrame);
  const group = new THREE.Group();
  group.renderOrder = 6;

  const lineSegments: THREE.Vector3[] = [];
  const addSegment = (
    start: [number, number, number],
    end: [number, number, number],
  ) => {
    lineSegments.push(new THREE.Vector3(start[0], start[1], start[2]));
    lineSegments.push(new THREE.Vector3(end[0], end[1], end[2]));
  };

  const arrowPositions: number[] = [];
  const arrowIndices: number[] = [];
  const arrows = { arrowPositions, arrowIndices };
  const planeNormal = planeNormalFromFrame(planeFrame);
  const startVector = new THREE.Vector3(...startWorld);
  const endVector = new THREE.Vector3(...endWorld);
  const lineDirection = endVector.clone().sub(startVector).normalize();
  const perpendicularDirection = new THREE.Vector3()
    .crossVectors(lineDirection, planeNormal)
    .normalize();

  const toCamera = new THREE.Vector3()
    .copy(camera.position)
    .sub(startVector)
    .normalize();
  if (perpendicularDirection.dot(toCamera) < 0) {
    perpendicularDirection.negate();
  }

  const viewHeight = getOrthographicViewHeight(camera);
  const viewportHeight = renderer.domElement.height || 600;
  const zoomDimensionOffset = Math.max(4, (30 * viewHeight) / viewportHeight);
  const arrowLength = 1.5;
  const arrowWidth = 0.27;
  const dimensionLabelPosition: [number, number, number] = [0, 0, 0];

  const lineAngle = Math.atan2(dy, dx);
  const lineLength = Math.hypot(dx, dy);
  const referenceAngle = previousLineAngle ?? 0;
  let displayAngle = lineAngle - referenceAngle;
  while (displayAngle > Math.PI) displayAngle -= 2 * Math.PI;
  while (displayAngle < -Math.PI) displayAngle += 2 * Math.PI;
  const zoomCap = Math.max(20, (480 * viewHeight) / viewportHeight);
  const arcRadius = Math.max(8, Math.min(lineLength, zoomCap));

  addLengthDimension({
    startWorld,
    endWorld,
    perpendicularDirection,
    zoomDimensionOffset,
    arrows,
    arrowLength,
    arrowWidth,
    dimensionLabelPosition,
    addSegment,
  });

  addReferenceLine({
    group,
    planeId,
    planeFrame,
    start: [sx, sy],
    referenceAngle,
    arcRadius,
  });

  const arcMidWorldLabel = addAngleArc({
    planeId,
    planeFrame,
    start: [sx, sy],
    dx,
    dy,
    lineLength,
    lineAngle,
    referenceAngle,
    displayAngle,
    arcRadius,
    zoomDimensionOffset,
    startVector,
    arrows,
    arrowLength,
    arrowWidth,
    addSegment,
  });

  addLineSegmentGeometry(group, lineSegments);
  addArrowMesh(group, arrowPositions, arrowIndices);

  return {
    kind: "group",
    group,
    screenPositions: lineDraftScreenPositions({
      dimensionLabelPosition,
      arcMidWorldLabel,
      camera,
      renderer,
    }),
  };
}

function planeNormalFromFrame(planeFrame: SketchPlaneFrame | null) {
  if (!planeFrame) {
    return new THREE.Vector3(0, 1, 0);
  }

  return new THREE.Vector3(
    planeFrame.normal.x,
    planeFrame.normal.y,
    planeFrame.normal.z,
  );
}

function addLengthDimension({
  startWorld,
  endWorld,
  perpendicularDirection,
  zoomDimensionOffset,
  arrows,
  arrowLength,
  arrowWidth,
  dimensionLabelPosition,
  addSegment,
}: {
  startWorld: [number, number, number];
  endWorld: [number, number, number];
  perpendicularDirection: THREE.Vector3;
  zoomDimensionOffset: number;
  arrows: { arrowPositions: number[]; arrowIndices: number[] };
  arrowLength: number;
  arrowWidth: number;
  dimensionLabelPosition: [number, number, number];
  addSegment: (
    start: [number, number, number],
    end: [number, number, number],
  ) => void;
}) {
  const dimStart = new THREE.Vector3(...startWorld).add(
    perpendicularDirection.clone().multiplyScalar(-2 * zoomDimensionOffset),
  );
  const dimEnd = new THREE.Vector3(...endWorld).add(
    perpendicularDirection.clone().multiplyScalar(-2 * zoomDimensionOffset),
  );
  const dimDirection = dimEnd.clone().sub(dimStart);

  addSegment(startWorld, [dimStart.x, dimStart.y, dimStart.z]);
  addSegment(endWorld, [dimEnd.x, dimEnd.y, dimEnd.z]);

  if (dimDirection.lengthSq() <= 0.001) {
    return;
  }

  const dimDirectionNormalized = dimDirection.clone().normalize();
  addSegment(
    [dimStart.x, dimStart.y, dimStart.z],
    [dimEnd.x, dimEnd.y, dimEnd.z],
  );
  appendFilledArrow(
    arrows,
    dimStart,
    dimDirectionNormalized,
    perpendicularDirection,
    arrowLength,
    arrowWidth,
  );
  appendFilledArrow(
    arrows,
    dimEnd,
    dimDirectionNormalized.clone().negate(),
    perpendicularDirection,
    arrowLength,
    arrowWidth,
  );

  const mid = dimStart.clone().add(dimEnd).multiplyScalar(0.5);
  dimensionLabelPosition[0] = mid.x;
  dimensionLabelPosition[1] = mid.y;
  dimensionLabelPosition[2] = mid.z;
}

function addReferenceLine({
  group,
  planeId,
  planeFrame,
  start,
  referenceAngle,
  arcRadius,
}: {
  group: THREE.Group;
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  start: [number, number];
  referenceAngle: number;
  arcRadius: number;
}) {
  const [sx, sy] = start;
  const referenceStartWorld = toWorldPoint(planeId, [sx, sy], planeFrame);
  const referenceEndWorld = toWorldPoint(
    planeId,
    [
      sx + arcRadius * Math.cos(referenceAngle),
      sy + arcRadius * Math.sin(referenceAngle),
    ],
    planeFrame,
  );
  const referenceGeometry = new THREE.BufferGeometry().setFromPoints([
    new THREE.Vector3(...referenceStartWorld),
    new THREE.Vector3(...referenceEndWorld),
  ]);
  const referenceMaterial = new THREE.LineDashedMaterial({
    color: new THREE.Color(0x8feaf7),
    transparent: true,
    opacity: 0.4,
    dashSize: 2,
    gapSize: 2,
    depthTest: false,
  });
  const referenceLine = new THREE.Line(referenceGeometry, referenceMaterial);
  referenceLine.computeLineDistances();
  referenceLine.renderOrder = 7;
  group.add(referenceLine);
}

function addAngleArc({
  planeId,
  planeFrame,
  start,
  dx,
  dy,
  lineLength,
  lineAngle,
  referenceAngle,
  displayAngle,
  arcRadius,
  zoomDimensionOffset,
  startVector,
  arrows,
  arrowLength,
  arrowWidth,
  addSegment,
}: {
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  start: [number, number];
  dx: number;
  dy: number;
  lineLength: number;
  lineAngle: number;
  referenceAngle: number;
  displayAngle: number;
  arcRadius: number;
  zoomDimensionOffset: number;
  startVector: THREE.Vector3;
  arrows: { arrowPositions: number[]; arrowIndices: number[] };
  arrowLength: number;
  arrowWidth: number;
  addSegment: (
    start: [number, number, number],
    end: [number, number, number],
  ) => void;
}) {
  const [sx, sy] = start;
  const arcSegments = 24;
  let previousArcPoint: THREE.Vector3 | null = null;
  let arcStartWorldPoint: [number, number, number] = [0, 0, 0];
  let arcEndWorldPoint: [number, number, number] = [0, 0, 0];
  for (let index = 0; index <= arcSegments; index += 1) {
    const angle = referenceAngle + displayAngle * (index / arcSegments);
    const localX = sx + arcRadius * Math.cos(angle);
    const localY = sy + arcRadius * Math.sin(angle);
    const worldPoint = toWorldPoint(planeId, [localX, localY], planeFrame);
    const point = new THREE.Vector3(...worldPoint);
    if (index === 0) {
      arcStartWorldPoint = worldPoint;
    }
    if (index === arcSegments) {
      arcEndWorldPoint = worldPoint;
    }
    if (previousArcPoint) {
      addSegment(
        [previousArcPoint.x, previousArcPoint.y, previousArcPoint.z],
        [point.x, point.y, point.z],
      );
    }
    previousArcPoint = point;
  }

  addArcArrow({
    planeId,
    planeFrame,
    start,
    startVector,
    arcRadius,
    tipWorld: arcStartWorldPoint,
    tipAngle: referenceAngle,
    arrows,
    arrowLength,
    arrowWidth,
  });
  addArcArrow({
    planeId,
    planeFrame,
    start,
    startVector,
    arcRadius,
    tipWorld: arcEndWorldPoint,
    tipAngle: lineAngle,
    arrows,
    arrowLength,
    arrowWidth,
  });

  const labelAngle = referenceAngle + displayAngle / 2;
  const angleDegrees = (Math.abs(displayAngle) * 180) / Math.PI;
  if (angleDegrees < 20 && lineLength > 0.001) {
    const lineUnitX = dx / lineLength;
    const lineUnitY = dy / lineLength;
    const perpendicularFlip = dy >= 0 ? -1 : 1;
    const perpendicularUnitX = -lineUnitY * perpendicularFlip;
    const perpendicularUnitY = lineUnitX * perpendicularFlip;
    return toWorldPoint(
      planeId,
      [
        sx + arcRadius * lineUnitX + perpendicularUnitX * 2.0 * zoomDimensionOffset,
        sy + arcRadius * lineUnitY + perpendicularUnitY * 2.0 * zoomDimensionOffset,
      ],
      planeFrame,
    );
  }

  return toWorldPoint(
    planeId,
    [
      sx + arcRadius * Math.cos(labelAngle),
      sy + arcRadius * Math.sin(labelAngle),
    ],
    planeFrame,
  );
}

function addArcArrow({
  planeId,
  planeFrame,
  start,
  startVector,
  arcRadius,
  tipWorld,
  tipAngle,
  arrows,
  arrowLength,
  arrowWidth,
}: {
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  start: [number, number];
  startVector: THREE.Vector3;
  arcRadius: number;
  tipWorld: [number, number, number];
  tipAngle: number;
  arrows: { arrowPositions: number[]; arrowIndices: number[] };
  arrowLength: number;
  arrowWidth: number;
}) {
  const [sx, sy] = start;
  const tip = new THREE.Vector3(...tipWorld);
  const tangentLocalX =
    sx + arcRadius * Math.cos(tipAngle) - arcRadius * Math.sin(tipAngle);
  const tangentLocalY =
    sy + arcRadius * Math.sin(tipAngle) + arcRadius * Math.cos(tipAngle);
  const tangentWorld = toWorldPoint(
    planeId,
    [tangentLocalX, tangentLocalY],
    planeFrame,
  );
  const tangentDirection = new THREE.Vector3(...tangentWorld)
    .sub(tip)
    .normalize();
  const radialDirection = tip.clone().sub(startVector).normalize();
  appendFilledArrow(
    arrows,
    tip,
    tangentDirection,
    radialDirection,
    arrowLength,
    arrowWidth,
  );
}

function addLineSegmentGeometry(
  group: THREE.Group,
  lineSegments: readonly THREE.Vector3[],
) {
  if (lineSegments.length === 0) {
    return;
  }

  const positions = new Float32Array(lineSegments.length * 3);
  for (let index = 0; index < lineSegments.length; index += 1) {
    const segment = lineSegments[index];
    positions[index * 3] = segment.x;
    positions[index * 3 + 1] = segment.y;
    positions[index * 3 + 2] = segment.z;
  }
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute("position", new THREE.BufferAttribute(positions, 3));
  const material = new THREE.LineBasicMaterial({
    color: new THREE.Color(0x8feaf7),
    transparent: true,
    opacity: 0.78,
    depthTest: false,
  });
  const line = new THREE.LineSegments(geometry, material);
  line.renderOrder = 6;
  group.add(line);
}

function addArrowMesh(
  group: THREE.Group,
  arrowPositions: number[],
  arrowIndices: number[],
) {
  const arrowMesh = buildFilledArrowMesh({
    arrowPositions,
    arrowIndices,
    color: new THREE.Color(0x8feaf7),
    opacity: 0.78,
    renderOrder: 6,
  });
  if (arrowMesh) {
    group.add(arrowMesh);
  }
}

function lineDraftScreenPositions({
  dimensionLabelPosition,
  arcMidWorldLabel,
  camera,
  renderer,
}: {
  dimensionLabelPosition: [number, number, number];
  arcMidWorldLabel: [number, number, number];
  camera: THREE.OrthographicCamera;
  renderer: THREE.WebGLRenderer;
}) {
  const screenPositions: DraftDimensionScreenPositions = {};
  const lengthLabel = projectWorldPointToViewport(
    dimensionLabelPosition,
    camera,
    renderer,
  );
  if (lengthLabel) {
    screenPositions.length = lengthLabel;
  }
  const angleLabel = projectWorldPointToViewport(
    arcMidWorldLabel,
    camera,
    renderer,
  );
  if (angleLabel) {
    screenPositions.angle = angleLabel;
  }
  return screenPositions;
}
