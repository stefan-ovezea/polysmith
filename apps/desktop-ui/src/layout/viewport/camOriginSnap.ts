import * as THREE from "three";

import type { DocumentState, ViewportScene, ViewportState } from "@/types";
import { themeColor } from "@/utils";
import {
  modelCenterFromBodies,
  resolveActiveCamSetup,
} from "./camSceneObjects";

// ── CAM origin snap candidates ───────────────────────────────────
// Pure geometry helpers for the graphical origin pick.  Candidates
// are world-space points the pointer can snap to within a screen
// distance: sketch points, body vertices, body edge midpoints, body
// face centers, and the stock box's top-face corners + edge
// midpoints (computed client-side from the same stock definition the
// stock box is drawn from — see addStockBoundingBox in
// camSceneObjects.ts).

export type CamOriginSnapKind =
  | "sketch_point"
  | "sketch_center"
  | "vertex"
  | "edge"
  | "face"
  | "stock_corner"
  | "stock_midpoint";

export interface CamOriginSnapCandidate {
  kind: CamOriginSnapKind;
  position: THREE.Vector3;
}

export function buildCamOriginSnapCandidates({
  document,
  activeCamSetupId,
  viewport,
  showStock,
  sketchPointObjects,
  sketchPrimitives,
  vertexObjects,
  edgeLineObjects,
  faceMeshes,
}: {
  document: DocumentState | null;
  activeCamSetupId?: string | null;
  viewport: ViewportState | null;
  showStock: boolean;
  sketchPointObjects: THREE.Object3D[];
  sketchPrimitives?: ViewportScene | null;
  vertexObjects: THREE.Mesh[];
  edgeLineObjects: THREE.Line[];
  faceMeshes: THREE.Mesh[];
}): CamOriginSnapCandidate[] {
  const candidates: CamOriginSnapCandidate[] = [];

  // Sketch points (world positions of the viewport point sprites).
  for (const pointObject of sketchPointObjects) {
    const world = new THREE.Vector3();
    pointObject.getWorldPosition(world);
    candidates.push({ kind: "sketch_point", position: world });
  }

  candidates.push(...buildCamOriginSketchSnapCandidates(sketchPrimitives));

  // Body vertices — exact 3D points, the primary geometry snap.
  for (const vertexObject of vertexObjects) {
    const world = new THREE.Vector3();
    vertexObject.getWorldPosition(world);
    candidates.push({ kind: "vertex", position: world });
  }

  // Body edge midpoints — the middle of the edge polyline.  For a
  // straight edge (two points) this is the exact midpoint.
  for (const edgeObject of edgeLineObjects) {
    const midpoint = edgeMidpointWorld(edgeObject);
    if (midpoint) {
      candidates.push({ kind: "edge", position: midpoint });
    }
  }

  // Body face centers — world-space bounding-box center of the face
  // mesh (an approximation of the analytic face center, fine for
  // origin placement).
  for (const faceMesh of faceMeshes) {
    const box = new THREE.Box3().setFromObject(faceMesh);
    if (box.isEmpty()) {
      continue;
    }
    candidates.push({ kind: "face", position: box.getCenter(new THREE.Vector3()) });
  }

  if (showStock) {
    const setup = resolveActiveCamSetup(document, activeCamSetupId);
    if (setup?.stock) {
      candidates.push(
        ...stockBoxTopFaceCandidates(setup.stock, viewport),
      );
    }
  }

  return candidates;
}

// Sketch-geometry snap targets derived from the always-emitted sketch
// primitives.  The core only emits vertex sprites for the ACTIVE
// sketch, so a finished sketch contributes no point sprites in the
// CAM workspace — but its lines, circles and arcs are always
// emitted, so the same snap targets (line endpoints, circle/arc
// centers, …) are derived here.  Transient tool previews (Mirror,
// Pattern) are skipped.  Shared by the snap resolver and the
// while-armed pick markers so both always agree on the targets.
export function buildCamOriginSketchSnapCandidates(
  sketchPrimitives: ViewportScene | null | undefined,
): CamOriginSnapCandidate[] {
  const candidates: CamOriginSnapCandidate[] = [];
  if (!sketchPrimitives) {
    return candidates;
  }
  for (const line of sketchPrimitives.sketchLines) {
    if (line.isPreview) {
      continue;
    }
    candidates.push({
      kind: "sketch_point",
      position: new THREE.Vector3(...line.start),
    });
    candidates.push({
      kind: "sketch_point",
      position: new THREE.Vector3(...line.end),
    });
  }
  for (const circle of sketchPrimitives.sketchCircles) {
    if (circle.isPreview) {
      continue;
    }
    candidates.push({
      kind: "sketch_center",
      position: new THREE.Vector3(...circle.center),
    });
  }
  for (const arc of sketchPrimitives.sketchArcs) {
    if (arc.isPreview) {
      continue;
    }
    candidates.push({
      kind: "sketch_point",
      position: new THREE.Vector3(...arc.start),
    });
    candidates.push({
      kind: "sketch_point",
      position: new THREE.Vector3(...arc.end),
    });
    candidates.push({
      kind: "sketch_center",
      position: new THREE.Vector3(...arc.center),
    });
  }
  for (const ellipse of sketchPrimitives.sketchEllipses) {
    if (ellipse.isPreview) {
      continue;
    }
    candidates.push({
      kind: "sketch_center",
      position: new THREE.Vector3(...ellipse.center),
    });
  }
  for (const spline of sketchPrimitives.sketchSplines) {
    if (spline.isPreview || spline.curvePoints.length === 0) {
      continue;
    }
    candidates.push({
      kind: "sketch_point",
      position: new THREE.Vector3(...spline.curvePoints[0]),
    });
    if (spline.curvePoints.length > 1) {
      candidates.push({
        kind: "sketch_point",
        position: new THREE.Vector3(
          ...spline.curvePoints[spline.curvePoints.length - 1],
        ),
      });
    }
  }
  for (const polygon of sketchPrimitives.sketchPolygons) {
    if (polygon.isPreview) {
      continue;
    }
    // Corners are a flat xyz-triple list.
    for (let index = 0; index + 2 < polygon.corners.length; index += 3) {
      candidates.push({
        kind: "sketch_point",
        position: new THREE.Vector3(
          polygon.corners[index],
          polygon.corners[index + 1],
          polygon.corners[index + 2],
        ),
      });
    }
  }
  return candidates;
}

// While the origin pick is armed, draw a visible dot at every
// sketch-derived snap target (endpoints, corners, circle/arc/ellipse
// centers).  Finished-sketch geometry has no point sprites in the
// CAM workspace — without markers a circle center is invisible and
// the user has nothing to aim at.  Same visual language as the
// sketch-point sprites: draw-on-top spheres, axis-z blue for
// centers, warm yellow for points.  Display-only; picking goes
// through the screen-space snap resolver.
export function addCamOriginPickMarkerObjects({
  sceneData,
  referenceGroup,
  originPickArmed,
}: {
  sceneData: ViewportScene | null;
  referenceGroup: THREE.Group;
  originPickArmed: boolean;
}) {
  if (!originPickArmed || !sceneData) {
    return;
  }
  const centerColor = themeColor("--color-axis-z", "#6db4ff");
  const pointColor = themeColor("--color-tertiary-plane-edge", "#ffe784");
  const candidates = buildCamOriginSketchSnapCandidates(sceneData);
  for (const candidate of candidates) {
    const isCenter = candidate.kind === "sketch_center";
    const geometry = new THREE.SphereGeometry(isCenter ? 0.9 : 0.7, 12, 12);
    const material = new THREE.MeshBasicMaterial({
      color: isCenter ? centerColor : pointColor,
      transparent: true,
      opacity: 0.9,
      depthTest: false,
      depthWrite: false,
    });
    const marker = new THREE.Mesh(geometry, material);
    marker.position.copy(candidate.position);
    marker.renderOrder = 8;
    referenceGroup.add(marker);
  }
}

export function resolveCamOriginSnap({
  candidates,
  camera,
  pointer,
  rect,
  maxPx = 12,
}: {
  candidates: CamOriginSnapCandidate[];
  camera: THREE.Camera;
  pointer: THREE.Vector2;
  rect: { width: number; height: number };
  maxPx?: number;
}): CamOriginSnapCandidate | null {
  let best: CamOriginSnapCandidate | null = null;
  let bestPx = maxPx;
  for (const candidate of candidates) {
    const screen = candidate.position.clone().project(camera);
    if (screen.z < -1 || screen.z > 1) {
      continue;
    }
    const px = ((screen.x - pointer.x) * rect.width) / 2;
    const py = ((screen.y - pointer.y) * rect.height) / 2;
    const distance = Math.hypot(px, py);
    if (distance < bestPx) {
      bestPx = distance;
      best = candidate;
    }
  }
  return best;
}

export function camOriginSnapLabelKey(kind: CamOriginSnapKind): string {
  switch (kind) {
    case "sketch_point":
      return "cam.setup.originSnapSketchPoint";
    case "sketch_center":
      return "cam.setup.originSnapSketchCenter";
    case "vertex":
      return "cam.setup.originSnapVertex";
    case "edge":
      return "cam.setup.originSnapEdge";
    case "face":
      return "cam.setup.originSnapFace";
    case "stock_corner":
      return "cam.setup.originSnapStockCorner";
    case "stock_midpoint":
      return "cam.setup.originSnapStockMidpoint";
  }
}

// Middle point of an edge line's polyline, in world space.  A
// straight edge (two points) yields the exact midpoint.
function edgeMidpointWorld(edgeObject: THREE.Line): THREE.Vector3 | null {
  const attribute = edgeObject.geometry.getAttribute("position");
  if (!attribute || attribute.count === 0) {
    return null;
  }
  const points: THREE.Vector3[] = [];
  for (let index = 0; index < attribute.count; index += 1) {
    points.push(
      new THREE.Vector3(
        attribute.getX(index),
        attribute.getY(index),
        attribute.getZ(index),
      ),
    );
  }
  const midpoint = new THREE.Vector3();
  if (points.length === 2) {
    midpoint.addVectors(points[0], points[1]).multiplyScalar(0.5);
  } else {
    midpoint.copy(points[Math.floor(points.length / 2)]);
  }
  return midpoint.applyMatrix4(edgeObject.matrixWorld);
}

// Corners (priority) and edge midpoints of the displayed stock box's
// TOP face — the face a laser bed or mill table sees.  Same extents
// as addStockBoundingBox so the snap points sit on the drawn box.
function stockBoxTopFaceCandidates(
  stock: NonNullable<DocumentState["cam"]["setups"][number]["stock"]>,
  viewport: ViewportState | null,
): CamOriginSnapCandidate[] {
  const center = modelCenterFromBodies(viewport?.bodies ?? []);
  const margin = stock.margin ?? 3;
  let width: number;
  let height: number;
  let depth: number;
  if (stock.type === "cylinder" && stock.diameter !== undefined) {
    // Cylinder stock is displayed as its bounding box.
    const diameter = stock.diameter + margin * 2;
    width = diameter;
    height = diameter;
    depth = (stock.length ?? 20) + margin * 2;
  } else {
    const size = stock.size ?? [120, 120, 20];
    width = size[0] + margin * 2;
    height = size[1] + margin * 2;
    depth = size[2] + margin * 2;
  }
  const hw = width / 2;
  const hh = height / 2;
  const zTop = center.z + depth / 2;

  const points: [number, number][] = [
    [-hw, -hh],
    [hw, -hh],
    [hw, hh],
    [-hw, hh],
  ];
  const candidates: CamOriginSnapCandidate[] = [];
  for (const [x, y] of points) {
    candidates.push({
      kind: "stock_corner",
      position: new THREE.Vector3(center.x + x, center.y + y, zTop),
    });
  }
  for (const [x, y] of [
    [0, -hh],
    [hw, 0],
    [0, hh],
    [-hw, 0],
  ]) {
    candidates.push({
      kind: "stock_midpoint",
      position: new THREE.Vector3(center.x + x, center.y + y, zTop),
    });
  }
  return candidates;
}
