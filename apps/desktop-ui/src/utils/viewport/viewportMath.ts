import type { SketchPlaneFrame } from "@/types";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";

export const SKETCH_PLANE_OFFSET = 0.2;
export const SKETCH_SNAP_DISTANCE = 2.5;

const DIMENSION_EDITOR_MARGIN = 20;

export function frameCamera(
  camera: THREE.OrthographicCamera,
  controls: OrbitControls,
  center: [number, number, number],
  maxDimension: number,
) {
  const distance = Math.max(maxDimension * 1.8, 160);
  const viewHeight = Math.max(maxDimension * 2.4, 120);
  camera.position.set(
    center[0] + distance,
    center[1] + distance * 0.8,
    center[2] + distance,
  );
  camera.zoom = Math.max((camera.top - camera.bottom) / viewHeight, 0.01);
  camera.updateProjectionMatrix();
  controls.target.set(...center);
  controls.update();
}

export function frameCameraToSketchPlane(
  camera: THREE.OrthographicCamera,
  controls: OrbitControls,
  activePlaneId: string,
  planeFrame: SketchPlaneFrame | null,
  maxDimension: number,
) {
  const distance = Math.max(maxDimension * 1.6, 120);
  const viewHeight = Math.max(maxDimension * 1.35, 80);
  camera.zoom = Math.max((camera.top - camera.bottom) / viewHeight, 0.01);
  camera.updateProjectionMatrix();

  if (planeFrame) {
    const origin = new THREE.Vector3(
      planeFrame.origin.x,
      planeFrame.origin.y,
      planeFrame.origin.z,
    );
    const normal = new THREE.Vector3(
      planeFrame.normal.x,
      planeFrame.normal.y,
      planeFrame.normal.z,
    ).normalize();

    // CAD-style up: prefer world Y; if the face normal is vertical, fall
    // back to world -Z so the sketch reads top-down without rolling.
    const worldUp = new THREE.Vector3(0, 1, 0);
    const up =
      Math.abs(normal.dot(worldUp)) > 0.95
        ? new THREE.Vector3(0, 0, -1)
        : worldUp.clone();

    camera.position.copy(origin.clone().add(normal.multiplyScalar(distance)));
    camera.up.copy(up);
    controls.target.copy(origin);
    controls.update();
    return;
  }

  if (activePlaneId === "ref-plane-xy") {
    camera.position.set(0, distance, 0);
    camera.up.set(0, 0, -1);
    controls.target.set(0, 0, 0);
    controls.update();
    return;
  }

  if (activePlaneId === "ref-plane-yz") {
    camera.position.set(distance, 0, 0);
    camera.up.set(0, 1, 0);
    controls.target.set(0, 0, 0);
    controls.update();
    return;
  }

  camera.position.set(0, 0, distance);
  camera.up.set(0, 1, 0);
  controls.target.set(0, 0, 0);
  controls.update();
}

export function resolveSketchPlanePoint(
  event: PointerEvent,
  renderer: THREE.WebGLRenderer,
  camera: THREE.Camera,
  activePlaneId: string,
  planeFrame: SketchPlaneFrame | null,
) {
  const rect = renderer.domElement.getBoundingClientRect();
  const pointer = new THREE.Vector2(
    ((event.clientX - rect.left) / rect.width) * 2 - 1,
    -((event.clientY - rect.top) / rect.height) * 2 + 1,
  );
  const raycaster = new THREE.Raycaster();
  raycaster.setFromCamera(pointer, camera);

  if (planeFrame) {
    const origin = new THREE.Vector3(
      planeFrame.origin.x,
      planeFrame.origin.y,
      planeFrame.origin.z,
    );
    const normal = new THREE.Vector3(
      planeFrame.normal.x,
      planeFrame.normal.y,
      planeFrame.normal.z,
    );
    const xAxis = new THREE.Vector3(
      planeFrame.x_axis.x,
      planeFrame.x_axis.y,
      planeFrame.x_axis.z,
    );
    const yAxis = new THREE.Vector3(
      planeFrame.y_axis.x,
      planeFrame.y_axis.y,
      planeFrame.y_axis.z,
    );
    const plane = new THREE.Plane().setFromNormalAndCoplanarPoint(
      normal,
      origin,
    );
    const hitPoint = new THREE.Vector3();
    const hit = raycaster.ray.intersectPlane(plane, hitPoint);
    if (!hit) {
      return null;
    }
    const relative = hitPoint.clone().sub(origin);
    return {
      local: [relative.dot(xAxis), relative.dot(yAxis)] as [number, number],
      world: [hitPoint.x, hitPoint.y, hitPoint.z] as [number, number, number],
    };
  }

  const plane =
    activePlaneId === "ref-plane-xy"
      ? new THREE.Plane(new THREE.Vector3(0, 1, 0), -SKETCH_PLANE_OFFSET)
      : activePlaneId === "ref-plane-yz"
        ? new THREE.Plane(new THREE.Vector3(1, 0, 0), -SKETCH_PLANE_OFFSET)
        : new THREE.Plane(new THREE.Vector3(0, 0, 1), -SKETCH_PLANE_OFFSET);

  const hitPoint = new THREE.Vector3();
  const hit = raycaster.ray.intersectPlane(plane, hitPoint);
  if (!hit) {
    return null;
  }

  if (activePlaneId === "ref-plane-xy") {
    return {
      local: [hitPoint.x, hitPoint.z] as [number, number],
      world: [hitPoint.x, SKETCH_PLANE_OFFSET, hitPoint.z] as [
        number,
        number,
        number,
      ],
    };
  }

  if (activePlaneId === "ref-plane-yz") {
    return {
      local: [hitPoint.y, hitPoint.z] as [number, number],
      world: [SKETCH_PLANE_OFFSET, hitPoint.y, hitPoint.z] as [
        number,
        number,
        number,
      ],
    };
  }

  return {
    local: [hitPoint.x, hitPoint.y] as [number, number],
    world: [hitPoint.x, hitPoint.y, SKETCH_PLANE_OFFSET] as [
      number,
      number,
      number,
    ],
  };
}

export function toWorldPoint(
  planeId: string,
  local: [number, number],
  planeFrame: SketchPlaneFrame | null = null,
): [number, number, number] {
  if (planeFrame) {
    return [
      planeFrame.origin.x +
        planeFrame.x_axis.x * local[0] +
        planeFrame.y_axis.x * local[1],
      planeFrame.origin.y +
        planeFrame.x_axis.y * local[0] +
        planeFrame.y_axis.y * local[1],
      planeFrame.origin.z +
        planeFrame.x_axis.z * local[0] +
        planeFrame.y_axis.z * local[1],
    ];
  }
  if (planeId === "ref-plane-xy") {
    return [local[0], SKETCH_PLANE_OFFSET, local[1]];
  }

  if (planeId === "ref-plane-yz") {
    return [SKETCH_PLANE_OFFSET, local[0], local[1]];
  }

  return [local[0], local[1], SKETCH_PLANE_OFFSET];
}

export function distanceBetweenPoints(
  a: [number, number],
  b: [number, number],
) {
  const dx = a[0] - b[0];
  const dy = a[1] - b[1];
  return Math.sqrt(dx * dx + dy * dy);
}

export function signedPolygonArea2d(points: Array<[number, number]>) {
  if (points.length < 3) {
    return 0;
  }

  let area = 0;
  for (let index = 0; index < points.length; index += 1) {
    const current = points[index];
    const next = points[(index + 1) % points.length];
    area += current[0] * next[1] - next[0] * current[1];
  }
  return area * 0.5;
}

export function polygonArea2d(points: Array<[number, number]>) {
  return Math.abs(signedPolygonArea2d(points));
}

export function circleFromThreePoints2d(
  first: [number, number],
  second: [number, number],
  third: [number, number],
) {
  const [p1x, p1y] = first;
  const [p2x, p2y] = second;
  const [p3x, p3y] = third;
  const denominator =
    2 * (p1x * (p2y - p3y) + p2x * (p3y - p1y) + p3x * (p1y - p2y));
  if (Math.abs(denominator) < 1e-9) {
    return null;
  }

  const p1Magnitude = p1x * p1x + p1y * p1y;
  const p2Magnitude = p2x * p2x + p2y * p2y;
  const p3Magnitude = p3x * p3x + p3y * p3y;
  const center: [number, number] = [
    (p1Magnitude * (p2y - p3y) +
      p2Magnitude * (p3y - p1y) +
      p3Magnitude * (p1y - p2y)) /
      denominator,
    (p1Magnitude * (p3x - p2x) +
      p2Magnitude * (p1x - p3x) +
      p3Magnitude * (p2x - p1x)) /
      denominator,
  ];

  return {
    center,
    radius: distanceBetweenPoints(first, center),
  };
}

export function rectangleFromThreePoints2d(
  first: [number, number],
  second: [number, number],
  offsetPoint: [number, number],
) {
  const [p1x, p1y] = first;
  const [p2x, p2y] = second;
  const dx = p2x - p1x;
  const dy = p2y - p1y;
  const edgeLength = Math.hypot(dx, dy);
  if (edgeLength < 1e-9) {
    return null;
  }

  const normalX = -dy / edgeLength;
  const normalY = dx / edgeLength;
  const offset =
    normalX * (offsetPoint[0] - p1x) + normalY * (offsetPoint[1] - p1y);
  const fourth: [number, number] = [
    p1x + normalX * offset,
    p1y + normalY * offset,
  ];
  const third: [number, number] = [
    p2x + normalX * offset,
    p2y + normalY * offset,
  ];
  const corners: Array<[number, number]> = [first, second, third, fourth];
  const xValues = corners.map((corner) => corner[0]);
  const yValues = corners.map((corner) => corner[1]);

  return {
    corners,
    closedCorners: [...corners, first] as Array<[number, number]>,
    bounds: {
      minX: Math.min(...xValues),
      minY: Math.min(...yValues),
      maxX: Math.max(...xValues),
      maxY: Math.max(...yValues),
    },
  };
}

export function axisAlignedRectangleCorners2d(
  mode: "corner_corner" | "center_point",
  start: [number, number],
  current: [number, number],
): Array<[number, number]> {
  const [sx, sy] = start;
  const [ex, ey] = current;
  if (mode === "center_point") {
    return [
      [2 * sx - ex, 2 * sy - ey],
      [ex, 2 * sy - ey],
      [ex, ey],
      [2 * sx - ex, ey],
      [2 * sx - ex, 2 * sy - ey],
    ];
  }

  return [
    [sx, sy],
    [ex, sy],
    [ex, ey],
    [sx, ey],
    [sx, sy],
  ];
}

export function projectWorldPointToViewport(
  point: [number, number, number],
  camera: THREE.Camera,
  renderer: THREE.WebGLRenderer,
) {
  const projected = new THREE.Vector3(...point).project(camera);
  if (projected.z < -1 || projected.z > 1) {
    return null;
  }

  const widthHalf = renderer.domElement.clientWidth / 2;
  const heightHalf = renderer.domElement.clientHeight / 2;

  const rawX = projected.x * widthHalf + widthHalf;
  const rawY = -projected.y * heightHalf + heightHalf;

  return {
    x: Math.min(
      Math.max(rawX, DIMENSION_EDITOR_MARGIN),
      renderer.domElement.clientWidth - DIMENSION_EDITOR_MARGIN,
    ),
    y: Math.min(
      Math.max(rawY, DIMENSION_EDITOR_MARGIN),
      renderer.domElement.clientHeight - DIMENSION_EDITOR_MARGIN,
    ),
  };
}

// -- Trim tool: 2D intersection helpers -----------------------------------

const TRIM_COINCIDENT_TS = 0.01;

export function lineLineIntersectionTrim(
  ax: number, ay: number, bx: number, by: number,
  cx: number, cy: number, dx: number, dy: number,
): number | null {
  const abx = bx - ax, aby = by - ay;
  const cdx = dx - cx, cdy = dy - cy;
  const denom = abx * cdy - aby * cdx;
  if (Math.abs(denom) < TRIM_COINCIDENT_TS) return null;
  const acx = cx - ax, acy = cy - ay;
  const t = (acx * cdy - acy * cdx) / denom;
  const u = (acx * aby - acy * abx) / denom;
  if (t < -1e-12 || t > 1 + 1e-12) return null;
  if (u < -1e-12 || u > 1 + 1e-12) return null;
  return Math.max(0, Math.min(1, t));
}

export function lineCircleIntersectionTrim(
  ax: number, ay: number, bx: number, by: number,
  cx: number, cy: number, r: number,
): number[] {
  const abx = bx - ax, aby = by - ay;
  const dx = ax - cx, dy = ay - cy;
  const a = abx * abx + aby * aby;
  if (a < TRIM_COINCIDENT_TS * TRIM_COINCIDENT_TS) return [];
  const b = 2 * (dx * abx + dy * aby);
  const c = dx * dx + dy * dy - r * r;
  const disc = b * b - 4 * a * c;
  if (disc < -TRIM_COINCIDENT_TS) return [];
  const sqrtDisc = disc <= 0 ? 0 : Math.sqrt(disc);
  const inv2a = 1 / (2 * a);
  const result: number[] = [];
  for (const t of [(-b - sqrtDisc) * inv2a, (-b + sqrtDisc) * inv2a]) {
    if (t >= -1e-12 && t <= 1 + 1e-12) {
      result.push(Math.max(0, Math.min(1, t)));
    }
  }
  return result;
}
