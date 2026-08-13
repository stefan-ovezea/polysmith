import * as THREE from "three";

import type { ConstraintType, SketchTool, ViewportScene } from "@/types";
import type { ViewportPickHit } from "./contextMenuState";
import { setPointerNdcFromEvent } from "@/utils/viewport/viewportMath";
import { sketchEntitySelectionHitFromIntersection } from "./sketchClickSelection";
import { pickSketchProfileId } from "./sketchProfilePicking";
import { getOrthographicViewHeight } from "./grid";

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

  if (activeSketchPlaneId) {
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
    });
    if (activeSketchHit) {
      return activeSketchHit;
    }
  }

  if (inactiveSketchEntityPickEnabled) {
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

  const profileId = pickProfile();
  if (profileId) {
    return { kind: "sketch_profile", id: profileId };
  }

  if (inactiveSketchEntityPickEnabled) {
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
}): ViewportPickHit | null {
  // Sketch points always get first priority — a vertex the user
  // wants to drag should never be blocked by an overlapping dimension
  // arc or label (e.g. angle dimensions sitting on a shared endpoint).
  const pointHit = pickSketchPointByRayDistance(
    raycaster, sketchPointObjects, worldUnitsPerPixel, tolerancePx);
  if (pointHit) {
    return pointHit;
  }

  const checkDimensionsLast = activeSketchTool === "dimension";

  if (checkDimensionsLast) {
    // Dimension tool: check entities before dimensions so the user
    // can pick lines/circles to create dimensions on them.
    const [entityHit] = raycaster.intersectObjects(
      sketchEntityObjects, false);
    const entityResult =
      sketchEntitySelectionHitFromIntersection(entityHit);
    if (entityResult) {
      return entityResult;
    }
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
    // Circle/arc entities only claim hits very close to their outline:
    // the curve's interior belongs to the enclosing profile (the
    // extrudable surface).  Without this, small circles can never show
    // their profile on hover because the whole region sits inside the
    // zoom-aware line-pick tolerance.  The dimension-tool branch above
    // keeps the generous tolerance so radius dimensions still work.
    const entityKind = entityHit.object.userData.sketchEntityKind;
    if (entityKind === "circle" || entityKind === "arc") {
      const closestApproach = entityHit.distance;
      const rayPoint = raycaster.ray.origin
        .clone()
        .addScaledVector(raycaster.ray.direction, closestApproach);
      const lateralDistance = rayPoint.distanceTo(entityHit.point);
      if (lateralDistance > 2 * worldUnitsPerPixel) {
        entityResult = null;
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

  const [faceHit] = raycaster.intersectObjects(faceMeshes, false);
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
