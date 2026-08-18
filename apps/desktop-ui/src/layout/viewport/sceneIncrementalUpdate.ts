import * as THREE from "three";

import type { ViewportScene } from "@/types";
import type { ActiveSketchGridPlaneFrame } from "./grid";

function sketchCirclePlaneAxes(
  frame: ActiveSketchGridPlaneFrame | null,
): {
  xAxis: [number, number, number];
  yAxis: [number, number, number];
} {
  if (!frame) {
    return {
      xAxis: [1, 0, 0],
      yAxis: [0, 0, 1],
    };
  }

  return {
    xAxis: [frame.x_axis.x, frame.x_axis.y, frame.x_axis.z],
    yAxis: [frame.y_axis.x, frame.y_axis.y, frame.y_axis.z],
  };
}

export function updateSketchLineObject(
  lineObject: THREE.Line | THREE.LineLoop | undefined,
  start: readonly [number, number, number],
  end: readonly [number, number, number],
) {
  if (
    !(lineObject instanceof THREE.Line) ||
    !lineObject.geometry.attributes.position
  ) {
    return;
  }

  const position = lineObject.geometry.attributes.position.array as Float32Array;
  position[0] = start[0];
  position[1] = start[1];
  position[2] = start[2];
  position[3] = end[0];
  position[4] = end[1];
  position[5] = end[2];
  lineObject.geometry.attributes.position.needsUpdate = true;
}

// Recovers the circle's sampling basis (unit xAxis / yAxis) from its own
// buffer: sample 0 lies at center + radius·xAxis and sample N/4 at
// center + radius·yAxis, so the axes are exact regardless of the sketch
// plane frame (origin-plane sketches carry plane_frame = null, and a
// wrong fallback frame would re-sample the circle edge-on).
function circleAxesFromBuffer(
  circleObject: THREE.LineLoop,
): {
  xAxis: [number, number, number];
  yAxis: [number, number, number];
} {
  const position =
    circleObject.geometry.attributes.position.array as Float32Array;
  const segments = position.length / 3 - 1;
  const cx = (position[0] + position[(segments / 2) * 3]) / 2;
  const cy = (position[1] + position[(segments / 2) * 3 + 1]) / 2;
  const cz = (position[2] + position[(segments / 2) * 3 + 2]) / 2;
  const xAxis: [number, number, number] = [
    position[0] - cx,
    position[1] - cy,
    position[2] - cz,
  ];
  const yAxis: [number, number, number] = [
    position[(segments / 4) * 3] - cx,
    position[(segments / 4) * 3 + 1] - cy,
    position[(segments / 4) * 3 + 2] - cz,
  ];
  const normalize = (v: [number, number, number]) => {
    const length = Math.hypot(v[0], v[1], v[2]);
    if (length < 1.0e-9) {
      return [1, 0, 0] as [number, number, number];
    }
    return [v[0] / length, v[1] / length, v[2] / length] as [
      number,
      number,
      number,
    ];
  };
  return { xAxis: normalize(xAxis), yAxis: normalize(yAxis) };
}

export function updateSketchCircleObject({
  circleObject,
  center,
  radius,
  xAxis,
  yAxis,
}: {
  circleObject: THREE.Line | THREE.LineLoop | undefined;
  center: readonly [number, number, number];
  radius: number;
  /** When omitted, the basis is recovered from the circle's own buffer
   *  (correct for any sketch plane frame). */
  xAxis?: readonly [number, number, number];
  yAxis?: readonly [number, number, number];
}) {
  if (
    !(circleObject instanceof THREE.LineLoop) ||
    !circleObject.geometry.attributes.position
  ) {
    return;
  }

  const position =
    circleObject.geometry.attributes.position.array as Float32Array;
  const segments = position.length / 3 - 1;
  const basis =
    xAxis && yAxis
      ? { xAxis, yAxis }
      : circleAxesFromBuffer(circleObject);
  for (let index = 0; index <= segments; index++) {
    const angle = (index / segments) * Math.PI * 2;
    const localX = Math.cos(angle) * radius;
    const localY = Math.sin(angle) * radius;
    position[index * 3] =
      center[0] + basis.xAxis[0] * localX + basis.yAxis[0] * localY;
    position[index * 3 + 1] =
      center[1] + basis.xAxis[1] * localX + basis.yAxis[1] * localY;
    position[index * 3 + 2] =
      center[2] + basis.xAxis[2] * localX + basis.yAxis[2] * localY;
  }
  circleObject.geometry.attributes.position.needsUpdate = true;
}

// Re-samples an arc's existing THREE.Line buffer from new
// center/start/end positions, mirroring buildSketchArcObject's sampling
// (project endpoints into the plane axes anchored at the center, recover
// start/end angles, walk the ccw-normalized sweep).  The buffer keeps its
// original segment count.
//
// When no plane axes are supplied, the sampling basis is derived from the
// arc's own geometry (e1 = normalize(start - center), e2 = normal × e1):
// the same circle arc point set, independent of the sketch plane frame.
export function updateSketchArcObject({
  arcObject,
  center,
  start,
  end,
  radius,
  ccw,
  xAxis,
  yAxis,
}: {
  arcObject: THREE.Line | THREE.LineLoop | undefined;
  center: readonly [number, number, number];
  start: readonly [number, number, number];
  end: readonly [number, number, number];
  radius: number;
  ccw: boolean;
  xAxis?: readonly [number, number, number];
  yAxis?: readonly [number, number, number];
}) {
  if (
    !(arcObject instanceof THREE.Line) ||
    !arcObject.geometry.attributes.position
  ) {
    return;
  }

  const position = arcObject.geometry.attributes.position.array as Float32Array;
  const segments = position.length / 3 - 1;

  // Geometry-derived orthonormal basis (used when axes are omitted):
  // e1 along start-center, e2 = normal × e1, so the sampled arc passes
  // through `start` at angle 0 and `end` at the swept angle.
  const deriveBasis = () => {
    const sx = start[0] - center[0];
    const sy = start[1] - center[1];
    const sz = start[2] - center[2];
    const ex = end[0] - center[0];
    const ey = end[1] - center[1];
    const ez = end[2] - center[2];
    const e1 = [sx, sy, sz] as [number, number, number];
    const normal: [number, number, number] = [
      sy * ez - sz * ey,
      sz * ex - sx * ez,
      sx * ey - sy * ex,
    ];
    const normalize = (v: [number, number, number]) => {
      const length = Math.hypot(v[0], v[1], v[2]);
      if (length < 1.0e-9) {
        return [1, 0, 0] as [number, number, number];
      }
      return [v[0] / length, v[1] / length, v[2] / length] as [
        number,
        number,
        number,
      ];
    };
    const e1n = normalize(e1);
    const nn = normalize(normal);
    const e2: [number, number, number] = [
      nn[1] * e1n[2] - nn[2] * e1n[1],
      nn[2] * e1n[0] - nn[0] * e1n[2],
      nn[0] * e1n[1] - nn[1] * e1n[0],
    ];
    return { e1: e1n, e2 };
  };

  let startAngle = 0;
  let endAngle = 0;
  let basisX: readonly [number, number, number];
  let basisY: readonly [number, number, number];
  if (xAxis && yAxis) {
    const projectLocal = (
      p: readonly [number, number, number],
    ): [number, number] => {
      const dx = p[0] - center[0];
      const dy = p[1] - center[1];
      const dz = p[2] - center[2];
      return [
        dx * xAxis[0] + dy * xAxis[1] + dz * xAxis[2],
        dx * yAxis[0] + dy * yAxis[1] + dz * yAxis[2],
      ];
    };
    const [sx, sy] = projectLocal(start);
    const [ex, ey] = projectLocal(end);
    startAngle = Math.atan2(sy, sx);
    endAngle = Math.atan2(ey, ex);
    basisX = xAxis;
    basisY = yAxis;
  } else {
    const basis = deriveBasis();
    startAngle = 0;
    endAngle = Math.atan2(
      (end[0] - center[0]) * basis.e2[0] +
        (end[1] - center[1]) * basis.e2[1] +
        (end[2] - center[2]) * basis.e2[2],
      (end[0] - center[0]) * basis.e1[0] +
        (end[1] - center[1]) * basis.e1[1] +
        (end[2] - center[2]) * basis.e1[2],
    );
    basisX = basis.e1;
    basisY = basis.e2;
  }

  let sweep = endAngle - startAngle;
  if (ccw) {
    while (sweep <= 0) sweep += Math.PI * 2;
  } else {
    while (sweep >= 0) sweep -= Math.PI * 2;
  }

  for (let index = 0; index <= segments; index++) {
    const t = index / segments;
    const angle = startAngle + sweep * t;
    const localX = radius * Math.cos(angle);
    const localY = radius * Math.sin(angle);
    position[index * 3] = center[0] + basisX[0] * localX + basisY[0] * localY;
    position[index * 3 + 1] =
      center[1] + basisX[1] * localX + basisY[1] * localY;
    position[index * 3 + 2] =
      center[2] + basisX[2] * localX + basisY[2] * localY;
  }
  arcObject.geometry.attributes.position.needsUpdate = true;
}

export function updateEndpointDragSceneObjects({
  sceneData,
  planeFrame,
  sketchEntityObjectById,
  sketchPointObjectById,
  sketchConstraintObjects,
}: {
  sceneData: ViewportScene;
  planeFrame: ActiveSketchGridPlaneFrame | null;
  sketchEntityObjectById: ReadonlyMap<string, THREE.Line | THREE.LineLoop>;
  sketchPointObjectById: ReadonlyMap<string, THREE.Mesh>;
  sketchConstraintObjects: readonly THREE.Object3D[];
}) {
  for (const lineData of sceneData.sketchLines) {
    updateSketchLineObject(
      sketchEntityObjectById.get(lineData.lineId),
      lineData.start,
      lineData.end,
    );
  }

  const { xAxis, yAxis } = sketchCirclePlaneAxes(planeFrame);
  for (const circleData of sceneData.sketchCircles) {
    updateSketchCircleObject({
      circleObject: sketchEntityObjectById.get(circleData.circleId),
      center: circleData.center,
      radius: circleData.radius,
      xAxis,
      yAxis,
    });
  }

  for (const arcData of sceneData.sketchArcs) {
    updateSketchArcObject({
      arcObject: sketchEntityObjectById.get(arcData.arcId),
      center: arcData.center,
      start: arcData.start,
      end: arcData.end,
      radius: arcData.radius,
      ccw: arcData.ccw,
      xAxis,
      yAxis,
    });
  }

  for (const pointData of sceneData.sketchPoints) {
    const pointObject = sketchPointObjectById.get(pointData.id);
    if (pointObject) {
      pointObject.position.set(
        pointData.position[0],
        pointData.position[1],
        pointData.position[2],
      );
    }
  }

  for (const constraintData of sceneData.sketchConstraints) {
    const constraintObject = sketchConstraintObjects.find(
      (object) =>
        object.userData.sketchConstraintId === constraintData.constraintId,
    );
    if (constraintObject) {
      constraintObject.position.set(
        constraintData.position[0],
        constraintData.position[1],
        constraintData.position[2],
      );
    }
  }
}
