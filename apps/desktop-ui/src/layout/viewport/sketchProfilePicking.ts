import * as THREE from "three";
import type { SketchProfileScene } from "@/types";
import {
  pointInPolygon2d,
  polygonArea2d,
  legacySketchPlane,
  SKETCH_PLANE_OFFSET,
} from "@/utils/viewport/viewportMath";

function profileArea(profile: SketchProfileScene) {
  if (profile.profileKind === "circle") {
    return Math.PI * profile.radius * profile.radius;
  }
  return (
    polygonArea2d(profile.profilePoints) -
    profile.innerLoops.reduce((sum, loop) => sum + polygonArea2d(loop), 0)
  );
}

function profileLocalPoint(
  profile: SketchProfileScene,
  ray: THREE.Ray,
): [number, number] | null {
  if (profile.planeFrame) {
    const origin = new THREE.Vector3(
      profile.planeFrame.origin[0],
      profile.planeFrame.origin[1],
      profile.planeFrame.origin[2],
    );
    const normal = new THREE.Vector3(
      profile.planeFrame.normal[0],
      profile.planeFrame.normal[1],
      profile.planeFrame.normal[2],
    );
    const xAxis = new THREE.Vector3(
      profile.planeFrame.xAxis[0],
      profile.planeFrame.xAxis[1],
      profile.planeFrame.xAxis[2],
    );
    const yAxis = new THREE.Vector3(
      profile.planeFrame.yAxis[0],
      profile.planeFrame.yAxis[1],
      profile.planeFrame.yAxis[2],
    );
    const renderOrigin = origin
      .clone()
      .addScaledVector(normal, SKETCH_PLANE_OFFSET);
    const plane = new THREE.Plane().setFromNormalAndCoplanarPoint(
      normal,
      renderOrigin,
    );
    const hitPoint = new THREE.Vector3();
    const hit = ray.intersectPlane(plane, hitPoint);
    if (!hit) {
      return null;
    }
    const relative = hitPoint.sub(renderOrigin);
    return [relative.dot(xAxis), relative.dot(yAxis)];
  }

  const plane = legacySketchPlane(profile.planeId);
  const hitPoint = new THREE.Vector3();
  const hit = ray.intersectPlane(plane, hitPoint);
  if (!hit) {
    return null;
  }
  if (profile.planeId === "ref-plane-xy") {
    return [hitPoint.x, hitPoint.y];
  }
  if (profile.planeId === "ref-plane-yz") {
    return [hitPoint.y, hitPoint.z];
  }
  return [hitPoint.x, hitPoint.z];
}

function containsProfilePoint(
  profile: SketchProfileScene,
  point: [number, number],
) {
  if (profile.profileKind === "circle") {
    const dx = point[0] - profile.start[0];
    const dy = point[1] - profile.start[1];
    return dx * dx + dy * dy <= profile.radius * profile.radius;
  }
  if (!pointInPolygon2d(point, profile.profilePoints)) {
    return false;
  }
  return !profile.innerLoops.some((loop) => pointInPolygon2d(point, loop));
}

export function pickSketchProfileId({
  profiles,
  profileObjects,
  raycaster,
}: {
  profiles: SketchProfileScene[];
  profileObjects: THREE.Object3D[];
  raycaster: THREE.Raycaster;
}) {
  const profileById = new Map(
    profiles.map((profile) => [profile.profileId, profile]),
  );
  const profileObjectHits = raycaster
    .intersectObjects(profileObjects, true)
    .map((hit) => {
      const profileId = hit.object.userData.sketchProfileId;
      if (typeof profileId !== "string") {
        return null;
      }
      const profile = profileById.get(profileId);
      if (!profile) {
        return null;
      }
      return {
        profileId,
        area: profileArea(profile),
      };
    })
    .filter((hit): hit is { profileId: string; area: number } => hit !== null);
  if (profileObjectHits.length > 0) {
    profileObjectHits.sort((left, right) => left.area - right.area);
    return profileObjectHits[0].profileId;
  }

  const hits = profiles
    .map((profile) => {
      const point = profileLocalPoint(profile, raycaster.ray);
      if (!point || !containsProfilePoint(profile, point)) {
        return null;
      }
      return {
        profileId: profile.profileId,
        area: profileArea(profile),
      };
    })
    .filter((hit): hit is { profileId: string; area: number } => hit !== null);
  if (hits.length === 0) {
    return null;
  }
  hits.sort((left, right) => left.area - right.area);
  return hits[0].profileId;
}
