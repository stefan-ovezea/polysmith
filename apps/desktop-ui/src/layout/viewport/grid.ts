import * as THREE from "three";
import type { DocumentState } from "@/types";
import { disposeGroup, SKETCH_PLANE_OFFSET } from "@/utils";

export type DynamicGridRef = {
  key: string;
  group: THREE.Group;
};

export type GridPlaneFrame = {
  origin: THREE.Vector3;
  xAxis: THREE.Vector3;
  yAxis: THREE.Vector3;
  normal: THREE.Vector3;
};

type GridPlaneBounds = {
  minU: number;
  maxU: number;
  minV: number;
  maxV: number;
};

export type ActiveSketchGridPlaneFrame = NonNullable<
  NonNullable<
    DocumentState["feature_history"][number]["sketch_parameters"]
  >["plane_frame"]
>;

const GRID_STEPS_MM = [
  0.1, 0.5, 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000,
];
const GRID_MIN_HALF_LINE_COUNT = 80;
const GRID_MAJOR_EVERY = 10;
const GRID_CAMERA_SCALE = 40;
const SKETCH_GRID_BACK_OFFSET = 0.015;
const CARDINAL_VIEW_DOT_THRESHOLD = 0.985;

export const GRID_SKETCH_PADDING_MULTIPLIER = 2;
export const GRID_WORLD_PADDING_MULTIPLIER = 6;

function snapGridCenter(value: number, spacing: number): number {
  return Math.round(value / spacing) * spacing;
}

function isGridMajorLine(value: number, spacing: number): boolean {
  const majorSpacing = spacing * GRID_MAJOR_EVERY;
  return (
    Math.abs(value / majorSpacing - Math.round(value / majorSpacing)) < 1e-5
  );
}

function pushGridLine(
  positions: number[],
  colors: number[],
  start: THREE.Vector3,
  end: THREE.Vector3,
  color: THREE.Color,
): void {
  positions.push(start.x, start.y, start.z, end.x, end.y, end.z);
  colors.push(color.r, color.g, color.b, color.r, color.g, color.b);
}

export function buildDynamicGrid(
  frame: GridPlaneFrame,
  spacing: number,
  bounds: GridPlaneBounds,
  minorColor: THREE.Color,
  majorColor: THREE.Color,
  _axisColor: THREE.Color,
  opacity: number,
): THREE.LineSegments {
  const positions: number[] = [];
  const colors: number[] = [];
  const start = new THREE.Vector3();
  const end = new THREE.Vector3();

  for (let u = bounds.minU; u <= bounds.maxU + spacing * 0.5; u += spacing) {
    const uColor = isGridMajorLine(u, spacing) ? majorColor : minorColor;

    start
      .copy(frame.origin)
      .addScaledVector(frame.xAxis, u)
      .addScaledVector(frame.yAxis, bounds.minV);
    end
      .copy(frame.origin)
      .addScaledVector(frame.xAxis, u)
      .addScaledVector(frame.yAxis, bounds.maxV);
    pushGridLine(positions, colors, start, end, uColor);
  }

  for (let v = bounds.minV; v <= bounds.maxV + spacing * 0.5; v += spacing) {
    const vColor = isGridMajorLine(v, spacing) ? majorColor : minorColor;

    start
      .copy(frame.origin)
      .addScaledVector(frame.xAxis, bounds.minU)
      .addScaledVector(frame.yAxis, v);
    end
      .copy(frame.origin)
      .addScaledVector(frame.xAxis, bounds.maxU)
      .addScaledVector(frame.yAxis, v);
    pushGridLine(positions, colors, start, end, vColor);
  }

  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute(
    "position",
    new THREE.Float32BufferAttribute(positions, 3),
  );
  geometry.setAttribute("color", new THREE.Float32BufferAttribute(colors, 3));
  const material = new THREE.LineBasicMaterial({
    vertexColors: true,
    transparent: true,
    opacity,
    depthWrite: false,
    // The sketch plane is often exactly coplanar with a body face
    // (sketch-on-face); draw the grid over the coincident surface like
    // the sketch overlay entities. (polygonOffset does not affect line
    // fragments in WebGL, so the offset approach never helped.)
    depthTest: false,
  });
  return new THREE.LineSegments(geometry, material);
}

const AXIS_LINE_HALF_LENGTH = 100000;

export function buildAxisLines(
  frame: GridPlaneFrame,
  xColor: THREE.Color,
  yColor: THREE.Color,
  opacity: number,
): THREE.LineSegments {
  const positions: number[] = [];
  const colors: number[] = [];
  const origin = frame.origin.clone();
  const L = AXIS_LINE_HALF_LENGTH;

  // X axis (red)
  const xNeg = origin.clone().addScaledVector(frame.xAxis, -L);
  const xPos = origin.clone().addScaledVector(frame.xAxis, L);
  positions.push(xNeg.x, xNeg.y, xNeg.z, xPos.x, xPos.y, xPos.z);
  colors.push(xColor.r, xColor.g, xColor.b, xColor.r, xColor.g, xColor.b);

  // Y axis (green)
  const yNeg = origin.clone().addScaledVector(frame.yAxis, -L);
  const yPos = origin.clone().addScaledVector(frame.yAxis, L);
  positions.push(yNeg.x, yNeg.y, yNeg.z, yPos.x, yPos.y, yPos.z);
  colors.push(yColor.r, yColor.g, yColor.b, yColor.r, yColor.g, yColor.b);

  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute(
    "position",
    new THREE.Float32BufferAttribute(positions, 3),
  );
  geometry.setAttribute("color", new THREE.Float32BufferAttribute(colors, 3));
  const material = new THREE.LineBasicMaterial({
    vertexColors: true,
    transparent: true,
    opacity,
    depthWrite: false,
    // The sketch plane is often exactly coplanar with a body face
    // (sketch-on-face); draw the grid over the coincident surface like
    // the sketch overlay entities. (polygonOffset does not affect line
    // fragments in WebGL, so the offset approach never helped.)
    depthTest: false,
  });
  return new THREE.LineSegments(geometry, material);
}

const AXIS_LABEL_TARGET_PX = 28;

export function buildAxisTickLabels(
  frame: GridPlaneFrame,
  spacing: number,
  bounds: GridPlaneBounds,
  xColor: THREE.Color,
  yColor: THREE.Color,
  worldUnitsPerPixel: number,
): THREE.Group {
  const group = new THREE.Group();
  const majorStep = spacing * GRID_MAJOR_EVERY;
  // Size labels so they appear at a consistent screen height (~14 px)
  // regardless of zoom level.  The 2× factor accounts for the canvas
  // texture aspect ratio (128×64 canvas → the sprite is twice as wide
  // as it is tall).
  const labelScale = worldUnitsPerPixel * AXIS_LABEL_TARGET_PX;

  // X-axis labels — snap to majorStep multiples
  const firstU = Math.ceil(bounds.minU / majorStep) * majorStep;
  for (let u = firstU; u <= bounds.maxU + spacing * 0.5; u += majorStep) {
    if (Math.abs(u) < spacing * 0.25) continue; // skip origin
    group.add(makeAxisLabelSprite(
      frame, u, 0, formatAxisLabel(u), xColor, labelScale,
    ));
  }

  // Y-axis labels — snap to majorStep multiples
  const firstV = Math.ceil(bounds.minV / majorStep) * majorStep;
  for (let v = firstV; v <= bounds.maxV + spacing * 0.5; v += majorStep) {
    if (Math.abs(v) < spacing * 0.25) continue; // skip origin
    group.add(makeAxisLabelSprite(
      frame, 0, v, formatAxisLabel(v), yColor, labelScale,
    ));
  }

  return group;
}

function formatAxisLabel(value: number): string {
  if (Math.abs(value) >= 1000) {
    return `${(value / 1000).toFixed(value % 1000 === 0 ? 0 : 1)}k`;
  }
  if (Number.isInteger(value)) return String(value);
  return value.toFixed(1);
}

function makeAxisLabelSprite(
  frame: GridPlaneFrame,
  u: number,
  v: number,
  text: string,
  color: THREE.Color,
  scale: number,
): THREE.Sprite {
  const canvas = document.createElement("canvas");
  canvas.width = 128;
  canvas.height = 64;
  const ctx = canvas.getContext("2d")!;
  ctx.font = "36px Inter, system-ui, sans-serif";
  ctx.fillStyle = `rgb(${Math.round(color.r * 255)},${Math.round(color.g * 255)},${Math.round(color.b * 255)})`;
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.fillText(text, 64, 32);

  const texture = new THREE.CanvasTexture(canvas);
  texture.minFilter = THREE.LinearFilter;
  const material = new THREE.SpriteMaterial({
    map: texture,
    transparent: true,
    depthWrite: false,
    depthTest: false,
  });
  const sprite = new THREE.Sprite(material);
  const pos = frame.origin.clone()
    .addScaledVector(frame.xAxis, u)
    .addScaledVector(frame.yAxis, v);
  // Offset labels slightly from the axis line
  pos.addScaledVector(frame.yAxis, scale * 0.54);
  sprite.position.copy(pos);
  sprite.scale.set(scale * 2, scale, 1);
  return sprite;
}

export function disposeDynamicGrid(grid: DynamicGridRef | null): void {
  if (!grid) {
    return;
  }
  disposeGroup(grid.group);
}

export function getSketchGridFrame(
  planeId: string,
  planeFrame: ActiveSketchGridPlaneFrame | null,
): GridPlaneFrame {
  if (planeFrame) {
    const normal = new THREE.Vector3(
      planeFrame.normal.x,
      planeFrame.normal.y,
      planeFrame.normal.z,
    ).normalize();
    return {
      origin: new THREE.Vector3(
        planeFrame.origin.x,
        planeFrame.origin.y,
        planeFrame.origin.z,
      ).addScaledVector(normal, -SKETCH_GRID_BACK_OFFSET),
      xAxis: new THREE.Vector3(
        planeFrame.x_axis.x,
        planeFrame.x_axis.y,
        planeFrame.x_axis.z,
      ).normalize(),
      yAxis: new THREE.Vector3(
        planeFrame.y_axis.x,
        planeFrame.y_axis.y,
        planeFrame.y_axis.z,
      ).normalize(),
      normal,
    };
  }

  if (planeId === "ref-plane-yz") {
    return {
      origin: new THREE.Vector3(
        SKETCH_PLANE_OFFSET - SKETCH_GRID_BACK_OFFSET,
        0,
        0,
      ),
      xAxis: new THREE.Vector3(0, 1, 0),
      yAxis: new THREE.Vector3(0, 0, 1),
      normal: new THREE.Vector3(1, 0, 0),
    };
  }

  if (planeId === "ref-plane-xz") {
    return {
      origin: new THREE.Vector3(
        0,
        SKETCH_PLANE_OFFSET - SKETCH_GRID_BACK_OFFSET,
        0,
      ),
      xAxis: new THREE.Vector3(1, 0, 0),
      yAxis: new THREE.Vector3(0, 0, 1),
      normal: new THREE.Vector3(0, 1, 0),
    };
  }

  return {
    origin: new THREE.Vector3(
      0,
      0,
      SKETCH_PLANE_OFFSET - SKETCH_GRID_BACK_OFFSET,
    ),
    xAxis: new THREE.Vector3(1, 0, 0),
    yAxis: new THREE.Vector3(0, 1, 0),
    normal: new THREE.Vector3(0, 0, 1),
  };
}

export function projectPointToGridFrame(
  point: THREE.Vector3,
  frame: GridPlaneFrame,
) {
  const relative = point.clone().sub(frame.origin);
  return {
    u: relative.dot(frame.xAxis),
    v: relative.dot(frame.yAxis),
  };
}

export function worldPointToSketchLocal(
  world: [number, number, number],
  planeId: string | null,
  planeFrame: ActiveSketchGridPlaneFrame | null,
): [number, number] | null {
  if (!planeId) {
    return null;
  }
  if (planeFrame) {
    const point = new THREE.Vector3(...world);
    const origin = new THREE.Vector3(
      planeFrame.origin.x,
      planeFrame.origin.y,
      planeFrame.origin.z,
    );
    const xAxis = new THREE.Vector3(
      planeFrame.x_axis.x,
      planeFrame.x_axis.y,
      planeFrame.x_axis.z,
    ).normalize();
    const yAxis = new THREE.Vector3(
      planeFrame.y_axis.x,
      planeFrame.y_axis.y,
      planeFrame.y_axis.z,
    ).normalize();
    const delta = point.sub(origin);
    return [delta.dot(xAxis), delta.dot(yAxis)];
  }
  if (planeId === "ref-plane-xy") {
    return [world[0], world[1]];
  }
  if (planeId === "ref-plane-yz") {
    return [world[1], world[2]];
  }
  return [world[0], world[2]];
}

function fallbackGridBounds(
  center: { u: number; v: number },
  spacing: number,
): GridPlaneBounds {
  const extent = spacing * GRID_MIN_HALF_LINE_COUNT;
  return {
    minU: snapGridCenter(center.u - extent, spacing),
    maxU: snapGridCenter(center.u + extent, spacing),
    minV: snapGridCenter(center.v - extent, spacing),
    maxV: snapGridCenter(center.v + extent, spacing),
  };
}

export function getGridViewBounds(
  camera: THREE.OrthographicCamera,
  frame: GridPlaneFrame,
  spacing: number,
  fallbackCenter: { u: number; v: number },
  paddingMultiplier: number,
): GridPlaneBounds {
  const plane = new THREE.Plane().setFromNormalAndCoplanarPoint(
    frame.normal.clone().normalize(),
    frame.origin,
  );
  const rayDirection = new THREE.Vector3();
  camera.getWorldDirection(rayDirection);
  const corners = [
    [-1, -1],
    [1, -1],
    [-1, 1],
    [1, 1],
  ] as const;
  const projectedCorners: Array<{ u: number; v: number }> = [];

  for (const [x, y] of corners) {
    const rayOrigin = new THREE.Vector3(x, y, -1).unproject(camera);
    const denominator = plane.normal.dot(rayDirection);
    if (Math.abs(denominator) > 1e-6) {
      const t = -(rayOrigin.dot(plane.normal) + plane.constant) / denominator;
      const hit = rayOrigin.clone().addScaledVector(rayDirection, t);
      projectedCorners.push(projectPointToGridFrame(hit, frame));
    }
  }

  if (projectedCorners.length < 2) {
    return fallbackGridBounds(fallbackCenter, spacing);
  }

  const minU = Math.min(...projectedCorners.map((point) => point.u));
  const maxU = Math.max(...projectedCorners.map((point) => point.u));
  const minV = Math.min(...projectedCorners.map((point) => point.v));
  const maxV = Math.max(...projectedCorners.map((point) => point.v));
  const spanU = Math.max(maxU - minU, spacing);
  const spanV = Math.max(maxV - minV, spacing);
  const minPadding = spacing * GRID_MIN_HALF_LINE_COUNT;
  const paddingU = Math.max(spanU * paddingMultiplier, minPadding);
  const paddingV = Math.max(spanV * paddingMultiplier, minPadding);

  return {
    minU: Math.floor((minU - paddingU) / spacing) * spacing,
    maxU: Math.ceil((maxU + paddingU) / spacing) * spacing,
    minV: Math.floor((minV - paddingV) / spacing) * spacing,
    maxV: Math.ceil((maxV + paddingV) / spacing) * spacing,
  };
}

export function getOrthographicViewHeight(
  camera: THREE.OrthographicCamera,
): number {
  return (camera.top - camera.bottom) / Math.max(camera.zoom, 0.0001);
}

export function selectOrthographicGridSpacing(
  camera: THREE.OrthographicCamera,
): number {
  const desiredSpacing = Math.max(
    getOrthographicViewHeight(camera) / GRID_CAMERA_SCALE,
    GRID_STEPS_MM[0],
  );
  return (
    GRID_STEPS_MM.find((spacing) => spacing >= desiredSpacing) ??
    GRID_STEPS_MM[GRID_STEPS_MM.length - 1]
  );
}

function nearestCardinalAxis(viewOffset: THREE.Vector3): THREE.Vector3 | null {
  const candidates = [
    new THREE.Vector3(1, 0, 0),
    new THREE.Vector3(-1, 0, 0),
    new THREE.Vector3(0, 1, 0),
    new THREE.Vector3(0, -1, 0),
    new THREE.Vector3(0, 0, 1),
    new THREE.Vector3(0, 0, -1),
  ];
  let best: THREE.Vector3 | null = null;
  let bestDot = -1;
  for (const candidate of candidates) {
    const dot = viewOffset.dot(candidate);
    if (dot > bestDot) {
      best = candidate;
      bestDot = dot;
    }
  }
  return best && bestDot >= CARDINAL_VIEW_DOT_THRESHOLD ? best : null;
}

export function getCardinalGridFrame(
  viewOffset: THREE.Vector3,
): GridPlaneFrame | null {
  const axis = nearestCardinalAxis(viewOffset);
  if (!axis) {
    return null;
  }

  if (Math.abs(axis.x) > 0.5) {
    return {
      origin: new THREE.Vector3(0, 0, 0),
      xAxis: new THREE.Vector3(0, 1, 0),
      yAxis: new THREE.Vector3(0, 0, 1),
      normal: axis,
    };
  }

  if (Math.abs(axis.z) > 0.5) {
    return {
      origin: new THREE.Vector3(0, 0, 0),
      xAxis: new THREE.Vector3(1, 0, 0),
      yAxis: new THREE.Vector3(0, 1, 0),
      normal: axis,
    };
  }

  return {
    origin: new THREE.Vector3(0, 0, 0),
    xAxis: new THREE.Vector3(1, 0, 0),
    yAxis: new THREE.Vector3(0, 0, 1),
    normal: axis,
  };
}
