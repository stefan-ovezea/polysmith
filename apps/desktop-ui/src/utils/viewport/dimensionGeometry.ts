import * as THREE from "three";

import type { SketchDimensionScene } from "@/types";

export interface SketchDimensionGeometry {
  points: THREE.Vector3[];
  arrowPositions: number[];
  arrowIndices: number[];
  refLineData: { start: THREE.Vector3; end: THREE.Vector3 } | null;
}

export interface FilledArrowGeometry {
  arrowPositions: number[];
  arrowIndices: number[];
}

export function appendFilledArrow(
  geometry: FilledArrowGeometry,
  tip: THREE.Vector3,
  inward: THREE.Vector3,
  perp: THREE.Vector3,
  arrowLength = 1.5,
  arrowWidth = 0.27,
) {
  const base = tip.clone().add(inward.clone().multiplyScalar(arrowLength));
  const side = perp.clone().multiplyScalar(arrowWidth);
  const index = geometry.arrowPositions.length / 3;
  geometry.arrowPositions.push(tip.x, tip.y, tip.z);
  geometry.arrowPositions.push(
    base.x + side.x,
    base.y + side.y,
    base.z + side.z,
  );
  geometry.arrowPositions.push(
    base.x - side.x,
    base.y - side.y,
    base.z - side.z,
  );
  geometry.arrowIndices.push(index, index + 1, index + 2);
}

export function buildFilledArrowMesh({
  arrowPositions,
  arrowIndices,
  color,
  opacity,
  renderOrder,
  userData,
}: FilledArrowGeometry & {
  color: THREE.ColorRepresentation;
  opacity: number;
  renderOrder: number;
  userData?: Record<string, unknown>;
}) {
  if (arrowIndices.length === 0) {
    return null;
  }

  const arrowGeometry = new THREE.BufferGeometry();
  arrowGeometry.setAttribute(
    "position",
    new THREE.Float32BufferAttribute(arrowPositions, 3),
  );
  arrowGeometry.setIndex(arrowIndices);

  const arrowMaterial = new THREE.MeshBasicMaterial({
    color,
    transparent: true,
    opacity,
    depthTest: false,
    side: THREE.DoubleSide,
  });
  const arrowMesh = new THREE.Mesh(arrowGeometry, arrowMaterial);
  arrowMesh.renderOrder = renderOrder;
  if (userData) {
    Object.assign(arrowMesh.userData, userData);
  }
  return arrowMesh;
}

function appendAngleArcGeometry({
  addSegment,
  arrows,
  pivot,
  startPoint,
  endPoint,
  startVector,
  endVector,
  normal,
  sweep,
  arrowLength,
  arrowWidth,
}: {
  addSegment: (start: THREE.Vector3, end: THREE.Vector3) => void;
  arrows: FilledArrowGeometry;
  pivot: THREE.Vector3;
  startPoint: THREE.Vector3;
  endPoint: THREE.Vector3;
  startVector: THREE.Vector3;
  endVector: THREE.Vector3;
  normal: THREE.Vector3;
  sweep: number;
  arrowLength: number;
  arrowWidth: number;
}) {
  const radius = startVector.length();
  const normalizedNormal = normal.clone().normalize();
  if (radius <= 1e-6 || normalizedNormal.lengthSq() <= 1e-8) {
    return;
  }

  const uAxis = startVector.clone().normalize();
  const vAxis = normalizedNormal.clone().cross(uAxis).normalize();
  const arcSegments = 32;
  let previousPoint = startPoint.clone();
  for (let index = 1; index <= arcSegments; index += 1) {
    const angle = (sweep * index) / arcSegments;
    const point = pivot
      .clone()
      .add(uAxis.clone().multiplyScalar(Math.cos(angle) * radius))
      .add(vAxis.clone().multiplyScalar(Math.sin(angle) * radius));
    addSegment(previousPoint, point);
    previousPoint = point;
  }

  const startTangent = normalizedNormal.clone().cross(startVector).normalize();
  const endTangent = normalizedNormal.clone().cross(endVector).normalize();
  const sweepSign = sweep >= 0 ? 1 : -1;
  appendFilledArrow(
    arrows,
    startPoint,
    startTangent.clone().multiplyScalar(sweepSign),
    startVector.clone().normalize(),
    arrowLength,
    arrowWidth,
  );
  appendFilledArrow(
    arrows,
    endPoint,
    endTangent.clone().multiplyScalar(-sweepSign),
    endVector.clone().normalize(),
    arrowLength,
    arrowWidth,
  );
}

export function buildSketchDimensionGeometry(
  dimension: SketchDimensionScene,
): SketchDimensionGeometry {
  const anchorStart = new THREE.Vector3(...dimension.anchorStart);
  const anchorEnd = new THREE.Vector3(...dimension.anchorEnd);
  const dimensionStart = new THREE.Vector3(...dimension.dimensionStart);
  const dimensionEnd = new THREE.Vector3(...dimension.dimensionEnd);
  const extensionOverrun = 0.75;
  const arrowLength = 1.5;
  const arrowWidth = 0.27;
  const dimensionDirection = dimensionEnd.clone().sub(dimensionStart);
  const dimensionLength = dimensionDirection.length();
  if (dimensionLength > 1e-6) {
    dimensionDirection.divideScalar(dimensionLength);
  } else {
    dimensionDirection.set(1, 0, 0);
  }
  const extensionDirection = dimensionStart.clone().sub(anchorStart);
  if (extensionDirection.length() > 1e-6) {
    extensionDirection.normalize();
  } else {
    extensionDirection.set(0, 1, 0);
  }

  const points: THREE.Vector3[] = [];
  const addSegment = (start: THREE.Vector3, end: THREE.Vector3) => {
    points.push(start.clone(), end.clone());
  };

  const arrowPositions: number[] = [];
  const arrowIndices: number[] = [];
  const arrows = { arrowPositions, arrowIndices };

  const refLineData: { start: THREE.Vector3; end: THREE.Vector3 } | null =
    (dimension.kind === "angle" || dimension.kind === "line_angle") &&
    dimension.refLineStart &&
    dimension.refLineEnd
      ? {
          start: new THREE.Vector3(...dimension.refLineStart),
          end: new THREE.Vector3(...dimension.refLineEnd),
        }
      : null;

  if (dimension.kind === "angle" || dimension.kind === "line_angle") {
    if (dimension.arcRadius && dimension.arcRadius > 0 && dimension.arcCenter) {
      const pivot = new THREE.Vector3(...dimension.arcCenter);
      const startAngle = dimension.arcStartAngle ?? 0;
      const endAngle = dimension.arcEndAngle ?? 0;

      const startRay = dimensionStart.clone().sub(pivot);
      const endRay = dimensionEnd.clone().sub(pivot);
      const radius = startRay.length();
      const normal = startRay.clone().cross(endRay).normalize();
      if (radius > 1e-6 && normal.lengthSq() > 1e-8) {
        let sweep = endAngle - startAngle;
        while (sweep > Math.PI) sweep -= 2 * Math.PI;
        while (sweep <= -Math.PI) sweep += 2 * Math.PI;
        sweep = Math.abs(sweep);
        appendAngleArcGeometry({
          addSegment,
          arrows,
          pivot,
          startPoint: dimensionStart,
          endPoint: dimensionEnd,
          startVector: startRay,
          endVector: endRay,
          normal,
          sweep,
          arrowLength,
          arrowWidth,
        });
      }
    } else {
      const startRay = dimensionStart.clone().sub(anchorStart);
      const endRay = dimensionEnd.clone().sub(anchorEnd);
      if (startRay.lengthSq() > 1e-8 && endRay.lengthSq() > 1e-8) {
        const startDirection = startRay.clone().normalize();
        const endDirection = endRay.clone().normalize();
        const betweenAnchors = anchorStart.clone().sub(anchorEnd);
        const directionDot = startDirection.dot(endDirection);
        const denominator = 1 - directionDot * directionDot;
        if (Math.abs(denominator) > 1e-8) {
          const startOffset =
            (directionDot * endDirection.dot(betweenAnchors) -
              startDirection.dot(betweenAnchors)) /
            denominator;
          const endOffset =
            (endDirection.dot(betweenAnchors) -
              directionDot * startDirection.dot(betweenAnchors)) /
            denominator;
          const pivot = anchorStart
            .clone()
            .add(startDirection.clone().multiplyScalar(startOffset))
            .add(
              anchorEnd
                .clone()
                .add(endDirection.clone().multiplyScalar(endOffset)),
            )
            .multiplyScalar(0.5);
          const startVector = dimensionStart.clone().sub(pivot);
          const endVector = dimensionEnd.clone().sub(pivot);
          const radius = startVector.length();
          const normal2 = startVector.clone().cross(endVector).normalize();
          if (radius > 1e-6 && normal2.lengthSq() > 1e-8) {
            const uAxis = startVector.clone().normalize();
            const vAxis = normal2.clone().cross(uAxis).normalize();
            let sweep = Math.atan2(endVector.dot(vAxis), endVector.dot(uAxis));
            if (sweep > Math.PI) {
              sweep -= Math.PI * 2;
            } else if (sweep < -Math.PI) {
              sweep += Math.PI * 2;
            }
            appendAngleArcGeometry({
              addSegment,
              arrows,
              pivot,
              startPoint: dimensionStart,
              endPoint: dimensionEnd,
              startVector,
              endVector,
              normal: normal2,
              sweep,
              arrowLength,
              arrowWidth,
            });
          }
        }
      }
    }
  } else {
    addSegment(
      anchorStart,
      dimensionStart
        .clone()
        .add(extensionDirection.clone().multiplyScalar(extensionOverrun)),
    );
    addSegment(dimensionStart, dimensionEnd);
    addSegment(
      anchorEnd,
      dimensionEnd
        .clone()
        .add(extensionDirection.clone().multiplyScalar(extensionOverrun)),
    );
    appendFilledArrow(
      arrows,
      dimensionStart,
      dimensionDirection,
      extensionDirection,
      arrowLength,
      arrowWidth,
    );
    appendFilledArrow(
      arrows,
      dimensionEnd,
      dimensionDirection.clone().multiplyScalar(-1),
      extensionDirection,
      arrowLength,
      arrowWidth,
    );
  }

  return { points, arrowPositions, arrowIndices, refLineData };
}
