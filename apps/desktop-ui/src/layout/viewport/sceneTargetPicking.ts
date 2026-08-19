import * as THREE from "three";

import type {
  ConstraintType,
  SketchPlaneFrame,
  SketchTool,
  ViewportScene,
} from "@/types";
import type { ViewportPickHit } from "./contextMenuState";
import { resolveSketchPlanePoint, setPointerNdcFromEvent } from "@/utils/viewport/viewportMath";
import { sketchEntitySelectionHitFromIntersection } from "./sketchClickSelection";
import { pickSketchProfileId } from "./sketchProfilePicking";
import { getOrthographicViewHeight } from "./grid";
import { trimWorldPointToLocal } from "./trimHoverPreview";

export function pickVisibleSketchLineScreenSpace({
  event,
  sceneData,
  camera,
  renderer,
  maxDistancePx = 16,
}: {
  event: PointerEvent;
  sceneData: ViewportScene | null;
  camera: THREE.OrthographicCamera;
  renderer: THREE.WebGLRenderer;
  maxDistancePx?: number;
}) {
  const lines = sceneData?.sketchLines ?? [];
  const rect = renderer.domElement.getBoundingClientRect();
  const pointerPx = {
    x: event.clientX - rect.left,
    y: event.clientY - rect.top,
  };
  const toScreen = (point: readonly [number, number, number]) => {
    const projected = new THREE.Vector3(...point).project(camera);
    if (
      projected.z < -1 ||
      projected.z > 1 ||
      !Number.isFinite(projected.x) ||
      !Number.isFinite(projected.y)
    ) {
      return null;
    }
    return {
      x: ((projected.x + 1) * 0.5) * rect.width,
      y: ((-projected.y + 1) * 0.5) * rect.height,
    };
  };

  let best: { lineId: string; distance: number } | null = null;
  for (const line of lines) {
    if (line.isPreview) {
      continue;
    }
    const start = toScreen(line.start);
    const end = toScreen(line.end);
    if (!start || !end) {
      continue;
    }
    const distance = pointSegmentDistance(pointerPx, start, end);
    if (
      distance <= maxDistancePx &&
      (!best || distance < best.distance)
    ) {
      best = { lineId: line.lineId, distance };
    }
  }
  return best?.lineId ?? null;
}

export function intersectViewportSceneTargets({
  event,
  renderer,
  camera,
  pointer,
  raycaster,
  sceneData,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  activeSketchTool,
  armedSketchConstraintKind,
  inactiveSketchEntityPickEnabled,
  sketchPointObjects,
  sketchEntityObjects,
  sketchDimensionObjects,
  sketchConstraintObjects,
  sketchProfileObjects,
  referencePlaneMeshes,
  vertexObjects,
  edgeLineObjects,
  faceMeshes,
  meshes,
  tolerancePx = 20,
}: {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.OrthographicCamera;
  pointer: THREE.Vector2;
  raycaster: THREE.Raycaster;
  sceneData: ViewportScene | null;
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  activeSketchTool: SketchTool;
  armedSketchConstraintKind: ConstraintType | null;
  inactiveSketchEntityPickEnabled: boolean;
  sketchPointObjects: THREE.Mesh[];
  sketchEntityObjects: (THREE.Line | THREE.LineLoop)[];
  sketchDimensionObjects: THREE.Object3D[];
  sketchConstraintObjects: THREE.Object3D[];
  sketchProfileObjects: THREE.Object3D[];
  referencePlaneMeshes: THREE.Mesh[];
  vertexObjects: THREE.Mesh[];
  edgeLineObjects: THREE.Line[];
  faceMeshes: THREE.Mesh[];
  meshes: THREE.Mesh[];
  tolerancePx?: number;
}): ViewportPickHit | null {
  setPointerNdcFromEvent(pointer, event, renderer);
  raycaster.setFromCamera(pointer, camera);

  const worldUnitsPerPixel =
    getOrthographicViewHeight(camera) /
    Math.max(renderer.domElement.clientHeight, 1);

  // Zoom-aware line picking threshold matching user-configured snap tolerance.
  raycaster.params.Line = {
    threshold: Math.max(0.75, tolerancePx * worldUnitsPerPixel),
  };

  const pickProfile = () =>
    pickSketchProfileId({
      profiles: sceneData?.sketchProfiles ?? [],
      profileObjects: sketchProfileObjects,
      raycaster,
    });

  // Project mode: the click targets BODY geometry (face / edge /
  // vertex). Sketch entities are skipped entirely — projected sketch
  // geometry lies ON the body surface (the sketch plane is the face),
  // and its points/entities/profiles would otherwise swallow every
  // click before the model pick runs.
  const projectMode = activeSketchTool === "project";

  if (activeSketchPlaneId && !projectMode) {
    // Cursor position on the active sketch plane — used by the exact
    // circle/arc curve-distance gate.
    const planePoint = resolveSketchPlanePoint(
      event, renderer, camera, activeSketchPlaneId, activeSketchPlaneFrame);
    const activeSketchHit = pickActiveSketchTarget({
      activeSketchTool,
      armedSketchConstraintKind,
      raycaster,
      sketchPointObjects,
      sketchEntityObjects,
      sketchDimensionObjects,
      sketchConstraintObjects,
      pickProfile,
      worldUnitsPerPixel,
      tolerancePx,
      sceneData,
      cursorLocal: planePoint ? planePoint.local : null,
    });
    if (activeSketchHit) {
      return activeSketchHit;
    }
  }

  if (inactiveSketchEntityPickEnabled && !projectMode) {
    const sketchLineId = pickVisibleSketchLineScreenSpace({
      event,
      sceneData,
      camera,
      renderer,
      maxDistancePx: 16,
    });
    if (sketchLineId) {
      return {
        kind: "sketch_entity",
        id: sketchLineId,
        entityKind: "line",
        isProjected: false,
        worldPoint: [0, 0, 0] as const,
      };
    }
    const [sketchEntityHit] = raycaster.intersectObjects(
      sketchEntityObjects,
      false,
    );
    const sketchEntitySelectionHit =
      sketchEntitySelectionHitFromIntersection(sketchEntityHit, "line");
    if (sketchEntitySelectionHit) {
      return sketchEntitySelectionHit;
    }
  }

  if (!projectMode) {
    const profileId = pickProfile();
    if (profileId) {
      return { kind: "sketch_profile", id: profileId };
    }
  }

  if (inactiveSketchEntityPickEnabled && !projectMode) {
    const [sketchEntityHit] = raycaster.intersectObjects(
      sketchEntityObjects,
      false,
    );
    const sketchEntitySelectionHit =
      sketchEntitySelectionHitFromIntersection(sketchEntityHit);
    if (sketchEntitySelectionHit) {
      return sketchEntitySelectionHit;
    }
  }

  return pickModelTarget({
    raycaster,
    referencePlaneMeshes,
    vertexObjects,
    edgeLineObjects,
    faceMeshes,
    meshes,
  });
}

// Exact distance from the sketch-plane cursor to a circle/arc entity's
// analytic curve (2D sketch coordinates).  Arcs measured in the stored
// sweep use the radial distance; outside the sweep the nearer endpoint
// distance.  This replaces the old fixed 2px outline rule with a
// zoom-aware, geometry-exact gate.
function exactDistanceToCurve(
  entity: {
    planeId: string;
    planeFrame: {
      origin: { x: number; y: number; z: number };
      x_axis: { x: number; y: number; z: number };
      y_axis: { x: number; y: number; z: number };
    } | null;
    center: [number, number, number];
    radius: number;
    start?: [number, number, number];
    end?: [number, number, number];
    ccw?: boolean;
  },
  cursorLocal: [number, number],
): number {
  const [cx, cy] = trimWorldPointToLocal(
    entity.center, entity.planeId, entity.planeFrame);
  const [mx, my] = cursorLocal;
  const radial = Math.hypot(mx - cx, my - cy);
  if (!entity.start || !entity.end || entity.ccw === undefined) {
    return Math.abs(radial - entity.radius);
  }
  const [sx, sy] = trimWorldPointToLocal(
    entity.start, entity.planeId, entity.planeFrame);
  const [ex, ey] = trimWorldPointToLocal(
    entity.end, entity.planeId, entity.planeFrame);
  const startAngle = Math.atan2(sy - cy, sx - cx);
  const endAngle = Math.atan2(ey - cy, ex - cx);
  let angle = Math.atan2(my - cy, mx - cx);
  let inSweep: boolean;
  if (entity.ccw) {
    let s = startAngle;
    let e = endAngle;
    if (angle < s) angle += 2 * Math.PI;
    if (e <= s) e += 2 * Math.PI;
    inSweep = angle >= s - 1e-9 && angle <= e + 1e-9;
  } else {
    let s = startAngle;
    let e = endAngle;
    if (angle > s) angle -= 2 * Math.PI;
    if (e >= s) e -= 2 * Math.PI;
    inSweep = angle <= s + 1e-9 && angle >= e - 1e-9;
  }
  if (!inSweep) {
    return Math.min(Math.hypot(mx - sx, my - sy), Math.hypot(mx - ex, my - ey));
  }
  return Math.abs(radial - entity.radius);
}

function pickActiveSketchTarget({
  activeSketchTool,
  armedSketchConstraintKind,
  raycaster,
  sketchPointObjects,
  sketchEntityObjects,
  sketchDimensionObjects,
  sketchConstraintObjects,
  pickProfile,
  worldUnitsPerPixel,
  tolerancePx,
  sceneData,
  cursorLocal,
}: {
  activeSketchTool: SketchTool;
  armedSketchConstraintKind: ConstraintType | null;
  raycaster: THREE.Raycaster;
  sketchPointObjects: THREE.Mesh[];
  sketchEntityObjects: (THREE.Line | THREE.LineLoop)[];
  sketchDimensionObjects: THREE.Object3D[];
  sketchConstraintObjects: THREE.Object3D[];
  pickProfile: () => string | null;
  worldUnitsPerPixel: number;
  tolerancePx: number;
  sceneData: ViewportScene | null;
  cursorLocal: [number, number] | null;
}): ViewportPickHit | null {
  const checkDimensionsLast = activeSketchTool === "dimension";

  if (checkDimensionsLast || activeSketchTool === "trim") {
    // Dimension tool: check entities before dimensions so the user
    // can pick lines/circles to create dimensions on them.
    // Trim tool: entities must win over sketch points, dimension
    // labels and constraints — the zoom-aware point-pick radius
    // otherwise swallows a circle's outline around its center and
    // quadrant vertices, making the red trim highlight unreachable
    // except in the gaps between vertices.  The 2px outline rule for
    // circle/arc entities is deliberately skipped here too: the trim
    // tool has no profile hover to protect.
    const [entityHit] = raycaster.intersectObjects(
      sketchEntityObjects, false);
    const entityResult =
      sketchEntitySelectionHitFromIntersection(entityHit);
    if (entityResult) {
      return entityResult;
    }
  }

  // Sketch points always get first priority — a vertex the user
  // wants to drag should never be blocked by an overlapping dimension
  // arc or label (e.g. angle dimensions sitting on a shared endpoint).
  const pointHit = pickSketchPointByRayDistance(
    raycaster, sketchPointObjects, worldUnitsPerPixel, tolerancePx);
  if (pointHit) {
    return pointHit;
  }

  const dimensionHit = pickSketchDimension(raycaster, sketchDimensionObjects);
  if (dimensionHit) {
    return dimensionHit;
  }

  if (armedSketchConstraintKind !== "coincident") {
    const constraintHit = pickSketchConstraint(raycaster, sketchConstraintObjects);
    if (constraintHit) {
      return constraintHit;
    }
  }

  // Entities (for non-dimension tools, checked after dimensions so
  // dimension labels/arcs can still be clicked for editing).
  const [entityHit] = raycaster.intersectObjects(
    sketchEntityObjects, false);
  let entityResult =
    sketchEntitySelectionHitFromIntersection(entityHit);
  if (entityResult) {
    // Circle/arc entities claim the pick only when the cursor lies on
    // the analytic curve (within the zoom-aware tolerance): the curve's
    // interior belongs to the enclosing profile (the extrudable
    // surface).  The gate is geometry-exact, so hovering ON an arc
    // anywhere along its length selects the arc — the old fixed 2px
    // chord-distance rule rejected interior bulge positions and made
    // trim-created arcs unselectable.  Small circles cap the gate at
    // r/2 so their profiles stay reachable on interior hover.  The
    // dimension/trim branches above keep the generous tolerance.
    const entityKind = entityHit.object.userData.sketchEntityKind;
    if (entityKind === "circle" || entityKind === "arc") {
      const entityId =
        entityHit.object.userData.sketchEntityId as string | undefined;
      const sceneEntity = entityKind === "circle"
          ? sceneData?.sketchCircles.find((c) => c.circleId === entityId)
          : sceneData?.sketchArcs.find((a) => a.arcId === entityId);
      if (!sceneEntity || !cursorLocal) {
        entityResult = null;
      } else {
        const distance = exactDistanceToCurve(sceneEntity, cursorLocal);
        const gate = Math.max(
          0.75,
          Math.min(tolerancePx * worldUnitsPerPixel, sceneEntity.radius / 2));
        if (distance > gate) {
          entityResult = null;
        }
      }
    }
  }
  if (entityResult) {
    return entityResult;
  }

  const profileId = pickProfile();
  return profileId ? { kind: "sketch_profile", id: profileId } : null;
}

function pickSketchPointOrEntity({
  raycaster,
  sketchPointObjects,
  sketchEntityObjects,
  useSpherePointPicking = false,
  worldUnitsPerPixel,
  tolerancePx,
}: {
  raycaster: THREE.Raycaster;
  sketchPointObjects: THREE.Mesh[];
  sketchEntityObjects: (THREE.Line | THREE.LineLoop)[];
  useSpherePointPicking?: boolean;
  worldUnitsPerPixel: number;
  tolerancePx: number;
}): ViewportPickHit | null {
  if (useSpherePointPicking) {
    const pointHit = pickSketchPointByRayDistance(raycaster, sketchPointObjects, worldUnitsPerPixel, tolerancePx);
    if (pointHit) {
      return pointHit;
    }
  } else {
    const [sketchPointHit] = raycaster.intersectObjects(
      sketchPointObjects,
      false,
    );
    const sketchPointId = sketchPointHit?.object.userData.sketchPointId;
    if (typeof sketchPointId === "string") {
      return {
        kind: "sketch_point",
        id: sketchPointId,
        pointKind: sketchPointHit.object.userData.sketchPointKind,
      };
    }
  }

  const [sketchEntityHit] = raycaster.intersectObjects(
    sketchEntityObjects,
    false,
  );
  return sketchEntitySelectionHitFromIntersection(sketchEntityHit);
}

function pickSketchDimension(
  raycaster: THREE.Raycaster,
  sketchDimensionObjects: THREE.Object3D[],
): ViewportPickHit | null {
  const [sketchDimensionHit] = raycaster.intersectObjects(
    sketchDimensionObjects,
    true,
  );
  const sketchDimensionId =
    sketchDimensionHit?.object.userData.sketchDimensionId;
  if (typeof sketchDimensionId !== "string") {
    return null;
  }

  return {
    kind: "sketch_dimension",
    id: sketchDimensionId,
    part:
      sketchDimensionHit?.object.userData.sketchDimensionPart === "label"
        ? "label"
        : "geometry",
  };
}

function pickSketchConstraint(
  raycaster: THREE.Raycaster,
  sketchConstraintObjects: THREE.Object3D[],
): ViewportPickHit | null {
  const [sketchConstraintHit] = raycaster.intersectObjects(
    sketchConstraintObjects,
    true,
  );
  const sketchConstraintId =
    sketchConstraintHit?.object.userData.sketchConstraintId;
  if (typeof sketchConstraintId !== "string") {
    return null;
  }

  return {
    kind: "sketch_constraint",
    id: sketchConstraintId,
    constraintKind:
      sketchConstraintHit.object.userData.sketchConstraintKind as ConstraintType,
    entityId: sketchConstraintHit.object.userData.sketchConstraintEntityId,
    relatedEntityId:
      sketchConstraintHit.object.userData.sketchConstraintRelatedEntityId ??
      null,
  };
}

function pickSketchPointByRayDistance(
  raycaster: THREE.Raycaster,
  sketchPointObjects: THREE.Mesh[],
  worldUnitsPerPixel: number,
  tolerancePx: number,
): ViewportPickHit | null {
  // Match the user-configured snap tolerance, zoom-aware.
  const pickRadius = Math.max(1.0, tolerancePx * worldUnitsPerPixel);
  const rayOrigin = raycaster.ray.origin;
  const rayDirection = raycaster.ray.direction;
  let bestPointDistance = pickRadius;
  let bestPointId: string | undefined;
  let bestPointKind: string | undefined;

  for (const mesh of sketchPointObjects) {
    const center = new THREE.Vector3();
    mesh.getWorldPosition(center);
    const toCenter = center.clone().sub(rayOrigin);
    const projectedDistance = toCenter.dot(rayDirection);
    const closest = rayOrigin
      .clone()
      .addScaledVector(rayDirection, Math.max(0, projectedDistance));
    const distance = center.distanceTo(closest);
    if (distance < bestPointDistance) {
      bestPointDistance = distance;
      bestPointId = mesh.userData.sketchPointId as string | undefined;
      bestPointKind = mesh.userData.sketchPointKind as string | undefined;
    }
  }

  return typeof bestPointId === "string"
    ? {
        kind: "sketch_point",
        id: bestPointId,
        pointKind: bestPointKind,
      }
    : null;
}

function pickModelTarget({
  raycaster,
  referencePlaneMeshes,
  vertexObjects,
  edgeLineObjects,
  faceMeshes,
  meshes,
}: {
  raycaster: THREE.Raycaster;
  referencePlaneMeshes: THREE.Mesh[];
  vertexObjects: THREE.Mesh[];
  edgeLineObjects: THREE.Line[];
  faceMeshes: THREE.Mesh[];
  meshes: THREE.Mesh[];
}): ViewportPickHit | null {
  const [referenceHit] = raycaster.intersectObjects(
    referencePlaneMeshes,
    false,
  );
  const referenceId = referenceHit?.object.userData.referenceId;
  if (typeof referenceId === "string") {
    return { kind: "reference", id: referenceId };
  }

  const [vertexHit] = raycaster.intersectObjects(vertexObjects, false);
  const vertexId = vertexHit?.object.userData.vertexId;
  if (typeof vertexId === "string") {
    return { kind: "vertex", id: vertexId };
  }

  const previousLineThreshold = raycaster.params.Line?.threshold ?? 1;
  if (raycaster.params.Line) {
    raycaster.params.Line.threshold = 1.2;
  }
  const [edgeHit] = raycaster.intersectObjects(edgeLineObjects, false);
  if (raycaster.params.Line) {
    raycaster.params.Line.threshold = previousLineThreshold;
  }
  const edgeId = edgeHit?.object.userData.edgeId;
  if (typeof edgeId === "string") {
    return { kind: "edge", id: edgeId };
  }

  // Coincident bodies (a mesh body and its converted solid share the
  // same surface): STL float32 rounding puts the mesh facets slightly
  // BELOW the exact plane, so they win the nearest-hit sort even when
  // emitted after the converted faces. Prefer a non-mesh face among
  // hits within a small distance band — clicking the shared surface
  // must select the meaningful body, not a single triangle facet.
  const faceHits = raycaster.intersectObjects(faceMeshes, false);
  let faceHit = faceHits[0] ?? null;
  if (faceHit) {
    const nearestDistance = faceHit.distance;
    for (const candidate of faceHits) {
      if (candidate.distance > nearestDistance + 0.05) {
        break;
      }
      if (candidate.object.userData.ownerKind !== "mesh_import") {
        faceHit = candidate;
        break;
      }
    }
  }
  const faceId = faceHit?.object.userData.faceId;
  if (typeof faceId === "string") {
    return { kind: "face", id: faceId };
  }

  const [primitiveHit] = raycaster.intersectObjects(meshes, false);
  const primitiveId = primitiveHit?.object.userData.primitiveId;
  return typeof primitiveId === "string"
    ? { kind: "primitive", id: primitiveId }
    : null;
}

function pointSegmentDistance(
  point: { x: number; y: number },
  start: { x: number; y: number },
  end: { x: number; y: number },
) {
  const dx = end.x - start.x;
  const dy = end.y - start.y;
  const lengthSq = dx * dx + dy * dy;
  if (lengthSq <= 1.0e-9) {
    return Math.hypot(point.x - start.x, point.y - start.y);
  }
  const t = Math.max(
    0,
    Math.min(
      1,
      ((point.x - start.x) * dx + (point.y - start.y) * dy) / lengthSq,
    ),
  );
  return Math.hypot(
    point.x - (start.x + dx * t),
    point.y - (start.y + dy * t),
  );
}
