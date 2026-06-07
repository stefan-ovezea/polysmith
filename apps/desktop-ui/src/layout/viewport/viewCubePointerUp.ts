import * as THREE from "three";

import {
  getCubeHitTargetDirection,
  getQuantizedCubeUp,
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
}: ViewCubePointerUpParams) {
  if (!viewCubeDraggingRef.current) {
    return "inactive" as const;
  }

  viewCubeDraggingRef.current = false;
  const dragStart = viewCubeDragStartRef.current;
  viewCubeDragStartRef.current = null;

  if (
    dragStart &&
    Math.abs(event.clientX - dragStart.x) <= 4 &&
    Math.abs(event.clientY - dragStart.y) <= 4
  ) {
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
    });
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
  if (!hit) {
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

function startViewCubeFaceAnimation({
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
  hit: Exclude<ReturnType<typeof raycastViewCube>, { type: "rotation_arrow" } | null>;
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
}
