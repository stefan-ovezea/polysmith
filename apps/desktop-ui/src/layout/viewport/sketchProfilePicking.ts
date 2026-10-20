import * as THREE from "three";
import type { SketchProfileScene } from "@/types";
import {
  exactProfileContour,
  smoothProfileHoleLoop,
} from "@/lib/viewportScene";
import {
  pointInPolygon2d,
  polygonArea2d,
  legacySketchPlane,
  SKETCH_PLANE_OFFSET,
} from "@/utils/viewport/viewportMath";

// Area of the region the user SEES: the exact contour minus its holes.  The
// chord-sampled fallbacks disagree with the pick contour on spline-bounded
// regions, which made "smallest wins" pick the wrong face.
function profileArea(profile: SketchProfileScene) {
  if (profile.profileKind === "circle") {
    return Math.PI * profile.radius * profile.radius;
  }
  let area = polygonArea2d(exactProfileContour(profile));
  for (let index = 0; index < profile.innerLoops.length; index += 1) {
    area -= polygonArea2d(smoothProfileHoleLoop(profile, index));
  }
  return Math.max(area, 0);
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
  // Test against the exact boundary contours (arcs resampled at a
  // sagitta bound), not the coarse stored chord sample — points near
  // the boundary classify identically to the rendered fill.
  if (!pointInPolygon2d(point, exactProfileContour(profile))) {
    return false;
  }
  return !profile.innerLoops.some((_, index) =>
    pointInPolygon2d(point, smoothProfileHoleLoop(profile, index)),
  );
}

// ── repeated-click cycling ─────────────────────────────────────────
// Several faces can claim one point (overlapping regions, a small sliver beside
// a big curved band).  "Smallest area wins" alone traps the user: the sliver
// takes every click and the intended surface can never be reached.  So a
// repeated click at the SAME spot advances to the next candidate, and wraps:
// small → big → … → small.  Any click that moves resets the cycle.
const CYCLE_POINT_TOLERANCE_MM = 0.25;
let lastCycle: {
  x: number;
  y: number;
  ids: string[];
  index: number;
} | null = null;

// Only exported so a new document / sketch switch can forget the cycle.
export function resetSketchProfilePickCycle() {
  lastCycle = null;
}

export function pickSketchProfileId({
  profiles,
  profileObjects,
  raycaster,
  advance = false,
}: {
  profiles: SketchProfileScene[];
  profileObjects: THREE.Object3D[];
  raycaster: THREE.Raycaster;
  // Only a CLICK advances the cycle.  Hover calls this many times per second at
  // one spot; advancing there made the highlight flicker through every
  // candidate (reported as fast blinking).
  advance?: boolean;
}) {
  const profileById = new Map(
    profiles.map((profile) => [profile.profileId, profile]),
  );

  // Candidates the ray actually hits (mesh first — that is what is drawn).
  const candidates = new Map<string, number>();
  for (const hit of raycaster.intersectObjects(profileObjects, true)) {
    const profileId = hit.object.userData.sketchProfileId;
    if (typeof profileId !== "string" || candidates.has(profileId)) continue;
    const profile = profileById.get(profileId);
    if (!profile) continue;
    candidates.set(profileId, profileArea(profile));
  }

  // Fallback: point-in-contour for regions whose mesh is missing or thin.
  if (candidates.size === 0) {
    for (const profile of profiles) {
      const point = profileLocalPoint(profile, raycaster.ray);
      if (!point || !containsProfilePoint(profile, point)) continue;
      candidates.set(profile.profileId, profileArea(profile));
    }
  }
  if (candidates.size === 0) {
    lastCycle = null;
    return null;
  }

  // Deterministic order: smallest area first, id as the tie-break, so the cycle
  // is stable across frames and machines.
  const ordered = [...candidates.entries()]
    .sort((left, right) =>
      left[1] === right[1]
        ? left[0].localeCompare(right[0])
        : left[1] - right[1],
    )
    .map(([profileId]) => profileId);

  // Where did the user click, on the sketch plane?  Taken from the profile the
  // ray hits — they all live on the same plane.
  const referenceProfile = profileById.get(ordered[0]);
  const point = referenceProfile
    ? profileLocalPoint(referenceProfile, raycaster.ray)
    : null;

  let index = 0;
  if (point && lastCycle) {
    const sameSpot =
      Math.abs(point[0] - lastCycle.x) <= CYCLE_POINT_TOLERANCE_MM &&
      Math.abs(point[1] - lastCycle.y) <= CYCLE_POINT_TOLERANCE_MM;
    const sameCandidates =
      lastCycle.ids.length === ordered.length &&
      lastCycle.ids.every((id, position) => id === ordered[position]);
    if (sameSpot && sameCandidates) {
      // Hover keeps the CURRENT candidate (no flicker); a click steps forward.
      index = advance ? (lastCycle.index + 1) % ordered.length : lastCycle.index;
    }
  }

  // Only clicks move the cycle, so hovering over a new area must not consume a
  // step — it just starts from the smallest candidate next time.
  if (advance || !lastCycle) {
    lastCycle = point
      ? { x: point[0], y: point[1], ids: ordered, index }
      : null;
  }
  return ordered[index];
}
