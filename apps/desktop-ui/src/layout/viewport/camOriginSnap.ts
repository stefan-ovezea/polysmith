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
// face centers, and the stock box's top/bottom-face corners + edge
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
  | "stock_midpoint"
  | "hole_rim"
  | "hole_wall";

export interface CamOriginSnapCandidate {
  kind: CamOriginSnapKind;
  position: THREE.Vector3;
  // For hole_rim / hole_wall: the raw payload id the armed pick must
  // report back to the core capture commands ("<body>:edge:<idx>" or
  // "<body>:face:<idx>").  Other kinds leave this unset.
  sourceId?: string;
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
        ...stockBoxFaceCandidates(setup.stock, viewport),
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

// While the origin pick is armed, draw a visible dot at every snap
// target the resolver can hit — sketch points/centers, body
// vertices, body edge midpoints, body face centers, and the stock
// box's top-face corners + edge midpoints.  Body vertex sprites are
// part of the normal scene, but edge midpoints, face centers and
// stock targets have no other visualization: without markers they
// resolve invisibly and look broken.  Same visual language as the
// sketch-point sprites: draw-on-top spheres, axis-z blue for
// centers/midpoints, warm yellow for points, axis-y green for stock
// targets.  Display-only; picking goes through the screen-space
// snap resolver.
export function addCamOriginPickMarkerObjects({
  sceneData,
  referenceGroup,
  originPickArmed,
  drillPickArmed,
  document,
  activeCamSetupId,
  viewport,
  showStock,
  vertexObjects,
  edgeLineObjects,
  faceMeshes,
}: {
  sceneData: ViewportScene | null;
  referenceGroup: THREE.Group;
  originPickArmed: boolean;
  /** Drill-pick flavor of the armed markers: only hole rims/walls
   *  from the raw payload are shown (drillHoleCandidates) and they
   *  lift to the topmost body face at their XY (a blind hole's wall
   *  axis often sits at the hole bottom — see liftDrillCandidates). */
  drillPickArmed?: boolean;
  document: DocumentState | null;
  activeCamSetupId?: string | null;
  viewport: ViewportState | null;
  showStock: boolean;
  vertexObjects: THREE.Mesh[];
  edgeLineObjects: THREE.Line[];
  faceMeshes: THREE.Mesh[];
}) {
  // The drill pick is an independent arm state — arming it disarms
  // the origin pick (mutual disarm in App.tsx), so the gate must
  // accept EITHER flag or the drill pick never renders its targets.
  if ((!originPickArmed && !drillPickArmed) || !sceneData) {
    return;
  }
  const centerColor = themeColor("--color-axis-z", "#6db4ff");
  const pointColor = themeColor("--color-tertiary-plane-edge", "#ffe784");
  const stockColor = themeColor("--color-axis-y", "#2bd978");
  let candidates = buildCamOriginSnapCandidates({
    document,
    activeCamSetupId,
    viewport,
    showStock,
    // Active-sketch point sprites are already visible in the scene
    // and the refs are repopulated later in the same rebuild — the
    // sceneData-derived sketch targets below cover finished sketches.
    sketchPointObjects: [],
    sketchPrimitives: sceneData,
    vertexObjects,
    edgeLineObjects,
    faceMeshes,
  });
  if (drillPickArmed) {
    // The drill pick's targets come straight from the raw viewport
    // payload (see drillHoleCandidates) and lift to the material top
    // like the live snap.
    candidates = liftDrillCandidates(
      drillHoleCandidates(viewport),
      faceMeshes,
    );
  }
  for (const candidate of candidates) {
    const isStock =
      candidate.kind === "stock_corner" ||
      candidate.kind === "stock_midpoint";
    const isCenter =
      candidate.kind === "sketch_center" ||
      candidate.kind === "edge" ||
      candidate.kind === "face" ||
      candidate.kind === "hole_rim" ||
      candidate.kind === "hole_wall";
    const geometry = new THREE.SphereGeometry(isCenter ? 0.9 : 0.7, 12, 12);
    const material = new THREE.MeshBasicMaterial({
      color: isStock ? stockColor : isCenter ? centerColor : pointColor,
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

// ── Drill-pick top-surface lift ────────────────────────────────────
// A 3-axis drill always enters from the material top, but a hole's
// sketch circle often lives BELOW it (the sketch is on the plate's
// bottom face, or the XY plane the part was extruded from — the
// circle sits at the hole bottom).  Area-kind targets — sketch
// circle centers and body face centers — therefore lift to the
// topmost body face at their XY for the drill pick, so the marker
// dots, the live snap square, and the captured point all land at the
// hole's TOP circle center.  Point-kind targets (vertices, edge
// midpoints, stock corners) keep their exact positions — the user
// aimed at that feature itself.  Targets are never pulled DOWN: a
// circle floating above the part keeps its own plane.
export function liftDrillCandidates(
  candidates: CamOriginSnapCandidate[],
  faceMeshes: THREE.Object3D[],
): CamOriginSnapCandidate[] {
  return candidates.map((candidate) => {
    if (
      candidate.kind !== "sketch_center" &&
      candidate.kind !== "face" &&
      candidate.kind !== "hole_rim" &&
      candidate.kind !== "hole_wall"
    ) {
      return candidate;
    }
    const { x, y, z } = candidate.position;
    const top = topSurfaceZAt(x, y, faceMeshes);
    if (top === null || top <= z) {
      return candidate;
    }
    return {
      kind: candidate.kind,
      position: new THREE.Vector3(x, y, top),
    };
  });
}

// The drill pick targets BODY geometry only — full-circle hole rims
// (body edges with the circle witness) and cylindrical hole walls —
// never sketch circles: drilling works on bodies, so a STEP/STL
// import with no sketch must stay drillable, and hiding a sketch must
// never empty the pick set.  Candidates read the RAW viewport payload
// (not the filtered scene), so hidden planes don't hide the holes.
// One bore emits up to three candidates (top rim, bottom rim, wall) —
// they collapse to ONE dot by quantized XY, keeping the highest z.
// Free clicks still land anywhere via the face raycast, so a custom
// drill position is never blocked.
export function drillHoleCandidates(
  rawViewport: ViewportState | null,
): CamOriginSnapCandidate[] {
  const candidates: CamOriginSnapCandidate[] = [];
  if (!rawViewport) {
    return candidates;
  }
  // A 3-axis machine drills vertically — side holes are not drillable
  // and must not light up as targets.
  const verticalTolerance = 1e-3;
  for (const edge of rawViewport.edges) {
    if (
      edge.kind !== "circle" ||
      !edge.center ||
      !edge.axis ||
      edge.radius === undefined
    ) {
      continue;  // arcs and lines carry no circle witness
    }
    if (Math.abs(edge.axis[2]) < 1 - verticalTolerance) {
      continue;
    }
    candidates.push({
      kind: "hole_rim",
      position: new THREE.Vector3(
        edge.center[0],
        edge.center[1],
        edge.center[2],
      ),
      sourceId: edge.id,
    });
  }
  for (const face of rawViewport.solid_faces) {
    if (
      face.surface_kind !== "cylinder" ||
      !face.cylinder_axis ||
      !face.cylinder_location
    ) {
      continue;
    }
    if (Math.abs(face.cylinder_axis.z) < 1 - verticalTolerance) {
      continue;
    }
    candidates.push({
      kind: "hole_wall",
      position: new THREE.Vector3(
        face.cylinder_location.x,
        face.cylinder_location.y,
        face.cylinder_location.z,
      ),
      sourceId: face.face_id,
    });
  }
  return dedupeHoleCandidates(candidates);
}

// Collapse the rim + wall candidates of one bore to a single dot by
// quantized XY (1e-3 mm), keeping the highest z (the top rim wins for
// through holes) and its sourceId.
function dedupeHoleCandidates(
  candidates: CamOriginSnapCandidate[],
): CamOriginSnapCandidate[] {
  const result: CamOriginSnapCandidate[] = [];
  const quantize = (value: number) => Math.round(value / 1e-3) * 1e-3;
  for (const candidate of candidates) {
    const existing = result.find(
      (other) =>
        quantize(other.position.x) === quantize(candidate.position.x) &&
        quantize(other.position.y) === quantize(candidate.position.y),
    );
    if (existing) {
      if (candidate.position.z > existing.position.z) {
        existing.position.z = candidate.position.z;
        existing.sourceId = candidate.sourceId;
      }
      continue;
    }
    result.push({ ...candidate });
  }
  return result;
}

// The topmost body-face bbox z whose XY footprint contains (x, y);
// null when no face mesh covers the XY.  Body faces only — the stock
// is not part of the hole.  A bore wall's bbox still tops out at the
// top rim, so a through hole resolves to the top face height.
export function topSurfaceZAt(
  x: number,
  y: number,
  faceMeshes: THREE.Object3D[],
): number | null {
  let best: number | null = null;
  const box = new THREE.Box3();
  for (const mesh of faceMeshes) {
    box.setFromObject(mesh);
    if (box.isEmpty()) {
      continue;
    }
    const tolerance = 1e-4;
    if (
      x < box.min.x - tolerance ||
      x > box.max.x + tolerance ||
      y < box.min.y - tolerance ||
      y > box.max.y + tolerance
    ) {
      continue;
    }
    if (best === null || box.max.z > best) {
      best = box.max.z;
    }
  }
  return best;
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
    case "hole_rim":
      return "cam.setup.originSnapHoleRim";
    case "hole_wall":
      return "cam.setup.originSnapHoleWall";
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
// TOP and BOTTOM faces — the faces a mill table sees from above and
// below.  Same extents as addStockBoundingBox so the snap points sit
// on the drawn box.
function stockBoxFaceCandidates(
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
  const halfDepth = depth / 2;

  const points: [number, number][] = [
    [-hw, -hh],
    [hw, -hh],
    [hw, hh],
    [-hw, hh],
  ];
  const candidates: CamOriginSnapCandidate[] = [];
  for (const zFace of [center.z + halfDepth, center.z - halfDepth]) {
    // A cylinder has no corners: the bounding-box corners float off
    // the drawn cylinder, so only the box edge midpoints (which lie
    // exactly on the cylinder rim, hw == hh == radius) are emitted.
    if (stock.type !== "cylinder") {
      for (const [x, y] of points) {
        candidates.push({
          kind: "stock_corner",
          position: new THREE.Vector3(center.x + x, center.y + y, zFace),
        });
      }
    }
    for (const [x, y] of [
      [0, -hh],
      [hw, 0],
      [0, hh],
      [-hw, 0],
    ]) {
      candidates.push({
        kind: "stock_midpoint",
        position: new THREE.Vector3(center.x + x, center.y + y, zFace),
      });
    }
  }
  return candidates;
}
