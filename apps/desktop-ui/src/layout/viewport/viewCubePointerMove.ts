import * as THREE from "three";

import {
  applyCubeDragOrbit,
  applyCubeHover,
  clearCubeHover,
  raycastViewCube,
  setViewCubeRaycasterFromPointer,
  type ViewCubeHit,
} from "@/utils";

interface MutableRef<T> {
  current: T;
}

export function handleViewCubeDragPointerMove({
  event,
  camera,
  controls,
  viewCubeDraggingRef,
  viewCubeDragStartRef,
}: {
  event: PointerEvent;
  camera: THREE.Camera;
  controls: { target: THREE.Vector3; update: () => void };
  viewCubeDraggingRef: MutableRef<boolean>;
  viewCubeDragStartRef: MutableRef<{ x: number; y: number } | null>;
}) {
  if (!viewCubeDraggingRef.current) {
    return false;
  }

  const dragStart = viewCubeDragStartRef.current;
  if (dragStart) {
    const deltaX = event.clientX - dragStart.x;
    const deltaY = event.clientY - dragStart.y;
    viewCubeDragStartRef.current = { x: event.clientX, y: event.clientY };
    applyCubeDragOrbit(camera, controls, deltaX, deltaY, 0.005);
  }
  return true;
}

export function handleViewCubeHoverPointerMove({
  event,
  renderer,
  inCube,
  cubePixelRatio,
  cubeGroupRef,
  cubeCameraRef,
  cubeRaycasterRef,
  viewCubeHoveredRef,
}: {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  inCube: boolean;
  cubePixelRatio: number;
  cubeGroupRef: MutableRef<THREE.Group | null>;
  cubeCameraRef: MutableRef<THREE.OrthographicCamera | null>;
  cubeRaycasterRef: MutableRef<THREE.Raycaster | null>;
  viewCubeHoveredRef: MutableRef<ViewCubeHit>;
}) {
  if (inCube) {
    const cubeGroup = cubeGroupRef.current;
    const cubeCamera = cubeCameraRef.current;
    const cubeRaycaster = cubeRaycasterRef.current;
    if (cubeGroup && cubeCamera && cubeRaycaster) {
      setViewCubeRaycasterFromPointer(
        cubeRaycaster,
        cubeCamera,
        event,
        renderer.domElement,
        cubePixelRatio,
      );
      const hit = raycastViewCube(cubeRaycaster, cubeGroup);
      applyCubeHover(cubeGroup, hit);
      viewCubeHoveredRef.current = hit;
      (renderer.domElement as HTMLCanvasElement).style.cursor = hit
        ? "pointer"
        : "";
    }
    return true;
  }

  if (viewCubeHoveredRef.current) {
    const cubeGroup = cubeGroupRef.current;
    if (cubeGroup) {
      clearCubeHover(cubeGroup);
    }
    viewCubeHoveredRef.current = null;
    (renderer.domElement as HTMLCanvasElement).style.cursor = "";
  }
  return false;
}
