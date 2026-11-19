import * as THREE from "three";

import {
  getCubeHitTargetDirection,
  getQuantizedCubeUp,
  orbitCameraQuarterTurns,
  raycastViewCube,
  setViewCubeRaycasterFromPointer,
} from "@/utils";

interface MutableRef<T> {
  current: T;
}

interface ViewCubePointerUpParams {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.OrthographicCamera;
  controls: {
    enabled: boolean;
    target: THREE.Vector3;
  };
  viewCubeDraggingRef: MutableRef<boolean>;
  viewCubeDragStartRef: MutableRef<{ x: number; y: number } | null>;
  viewCubeGroupRef: MutableRef<THREE.Group | null>;
  viewCubeCameraRef: MutableRef<THREE.OrthographicCamera | null>;
  viewCubeRaycasterRef: MutableRef<THREE.Raycaster | null>;
  viewCubeAnimatingRef: MutableRef<boolean>;
  viewCubeAnimStartRef: MutableRef<number>;
  viewCubeAnimStartPosRef: MutableRef<THREE.Vector3>;
  viewCubeAnimTargetPosRef: MutableRef<THREE.Vector3>;
  viewCubeAnimStartUpRef: MutableRef<THREE.Vector3>;
  viewCubeAnimTargetUpRef: MutableRef<THREE.Vector3>;
  rotateCameraAroundCurrentView: (direction: -1 | 1) => void;
  /** Frame the model from the isometric home view (the cube's Home button). */
  onViewCubeHome?: () => void;
  /** Frame the model without changing the view direction (Fit button, double-click). */
  onViewCubeFit?: () => void;
  /** Open the cube's standard-views menu. */
  onViewCubeMenu?: () => void;
  /** Last click on the cube, for double-click detection. */
  lastClickRef: MutableRef<{ key: string; at: number } | null>;
}

export function finishViewCubePointerUp({
  event,
  renderer,
  camera,
  controls,
  viewCubeDraggingRef,
  viewCubeDragStartRef,
  viewCubeGroupRef,
  viewCubeCameraRef,
  viewCubeRaycasterRef,
  viewCubeAnimatingRef,
  viewCubeAnimStartRef,
  viewCubeAnimStartPosRef,
  viewCubeAnimTargetPosRef,
  viewCubeAnimStartUpRef,
  viewCubeAnimTargetUpRef,
  rotateCameraAroundCurrentView,
  onViewCubeHome,
  onViewCubeFit,
  onViewCubeMenu,
  lastClickRef,
}: ViewCubePointerUpParams) {
  if (!viewCubeDraggingRef.current) {
    return "inactive" as const;
  }

  viewCubeDraggingRef.current = false;
  const dragStart = viewCubeDragStartRef.current;
  viewCubeDragStartRef.current = null;

  const moved =
    dragStart !== null &&
    (Math.abs(event.clientX - dragStart.x) > 4 ||
      Math.abs(event.clientY - dragStart.y) > 4);

  if (dragStart && !moved) {
    handleViewCubeClick({
      event,
      renderer,
      camera,
      controls,
      viewCubeGroupRef,
      viewCubeCameraRef,
      viewCubeRaycasterRef,
      viewCubeAnimatingRef,
      viewCubeAnimStartRef,
      viewCubeAnimStartPosRef,
      viewCubeAnimTargetPosRef,
      viewCubeAnimStartUpRef,
      viewCubeAnimTargetUpRef,
      rotateCameraAroundCurrentView,
      onViewCubeHome,
      onViewCubeFit,
      onViewCubeMenu,
      lastClickRef,
    });
  } else {
    // A drag is handled entirely by the pointer-move path, which never starts
    // an animation — so this is the only place that can hand the viewport back.
    // Without it, one drag on the cube left `controls.enabled = false` for the
    // rest of the session: the model stopped orbiting when the background was
    // dragged, which is what "it is not moving intuitively anymore" felt like.
    controls.enabled = true;
  }

  return "consumed" as const;
}

function handleViewCubeClick({
  event,
  renderer,
  camera,
  controls,
  viewCubeGroupRef,
  viewCubeCameraRef,
  viewCubeRaycasterRef,
  viewCubeAnimatingRef,
  viewCubeAnimStartRef,
  viewCubeAnimStartPosRef,
  viewCubeAnimTargetPosRef,
  viewCubeAnimStartUpRef,
  viewCubeAnimTargetUpRef,
  rotateCameraAroundCurrentView,
  onViewCubeHome,
  onViewCubeFit,
  onViewCubeMenu,
  lastClickRef,
}: Omit<
  ViewCubePointerUpParams,
  "viewCubeDraggingRef" | "viewCubeDragStartRef"
>) {
  const cubeGroup = viewCubeGroupRef.current;
  const cubeCam = viewCubeCameraRef.current;
  const cubeRaycaster = viewCubeRaycasterRef.current;
  if (!cubeGroup || !cubeCam || !cubeRaycaster) {
    return;
  }

  setViewCubeRaycasterFromPointer(
    cubeRaycaster,
    cubeCam,
    event,
    renderer.domElement,
    renderer.getPixelRatio(),
  );
  const hit = raycastViewCube(cubeRaycaster, cubeGroup);
  if (hit?.type === "rotation_arrow") {
    rotateCameraAroundCurrentView(-hit.direction as -1 | 1);
    return;
  }
  if (hit?.type === "orbit_arrow") {
    // Camera-centric: the eye moves the way the arrow points.
    orbitCameraQuarterTurns(camera, controls, hit.axis, hit.direction);
    controls.enabled = true;
    return;
  }
  if (hit?.type === "button") {
    // Home: isometric and framed. Fit: frame the model as it is now, keeping
    // the direction. Both are the panel's business, since only it knows the
    // scene bounds. Backside: look from the opposite side.
    if (hit.action === "home") {
      onViewCubeHome?.();
    } else if (hit.action === "fit") {
      onViewCubeFit?.();
    } else if (hit.action === "menu") {
      onViewCubeMenu?.();
    } else {
      orbitCameraQuarterTurns(camera, controls, "up", 2);
    }
    controls.enabled = true;
    return;
  }
  if (!hit) {
    controls.enabled = true;
    return;
  }
  // Double-click a facet to FRAME the model in that direction: the second click
  // within the interval re-centres instead of re-orienting (FreeCAD's
  // "double-click also recentres the view").
  const now = Date.now();
  const facetKey =
    hit.type === "face"
      ? `face:${hit.face}`
      : hit.type === "edge"
        ? `edge:${[hit.face1, hit.face2].sort().join(":")}`
        : `corner:${[...hit.faces].sort().join(":")}`;
  const previous = lastClickRef.current;
  lastClickRef.current = { key: facetKey, at: now };
  if (previous && previous.key === facetKey && now - previous.at < 400) {
    lastClickRef.current = null;
    onViewCubeFit?.();
    controls.enabled = true;
    return;
  }
  startViewCubeFaceAnimation({
    hit,
    camera,
    controls,
    viewCubeAnimatingRef,
    viewCubeAnimStartRef,
    viewCubeAnimStartPosRef,
    viewCubeAnimTargetPosRef,
    viewCubeAnimStartUpRef,
    viewCubeAnimTargetUpRef,
  });
}

export function startViewCubeFaceAnimation({
  hit,
  camera,
  controls,
  viewCubeAnimatingRef,
  viewCubeAnimStartRef,
  viewCubeAnimStartPosRef,
  viewCubeAnimTargetPosRef,
  viewCubeAnimStartUpRef,
  viewCubeAnimTargetUpRef,
}: Pick<
  ViewCubePointerUpParams,
  | "camera"
  | "controls"
  | "viewCubeAnimatingRef"
  | "viewCubeAnimStartRef"
  | "viewCubeAnimStartPosRef"
  | "viewCubeAnimTargetPosRef"
  | "viewCubeAnimStartUpRef"
  | "viewCubeAnimTargetUpRef"
> & {
  hit: Extract<
    NonNullable<ReturnType<typeof raycastViewCube>>,
    { type: "face" | "edge" | "corner" }
  >;
}) {
  const direction = getCubeHitTargetDirection(hit);
  const targetUp = getQuantizedCubeUp(direction, camera.up);
  const distance = camera.position.distanceTo(controls.target);
  const targetPos = controls.target
    .clone()
    .add(direction.multiplyScalar(distance));
  viewCubeAnimStartPosRef.current.copy(camera.position);
  viewCubeAnimTargetPosRef.current.copy(targetPos);
  viewCubeAnimStartUpRef.current.copy(camera.up).normalize();
  viewCubeAnimTargetUpRef.current.copy(targetUp);
  viewCubeAnimStartRef.current = performance.now();
  viewCubeAnimatingRef.current = true;
  controls.enabled = false;
}
