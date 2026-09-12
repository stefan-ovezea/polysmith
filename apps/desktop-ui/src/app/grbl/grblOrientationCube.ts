import * as THREE from "three";
import type { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";

import {
  animateCameraTowardTarget,
  applyCubeHover,
  buildViewCubeGroup,
  clearCubeHover,
  createViewCubeCamera,
  createViewCubeScene,
  disposeViewCubeGroup,
  getCubeHitTargetDirection,
  getQuantizedCubeUp,
  raycastViewCube,
  syncCubeCamera,
} from "@/utils";

// Mini orientation cube for the GRBL preview viewport.  The cube
// itself — mesh build, face/edge/corner picking, hover, snap
// direction math, camera sync — comes from the shared
// `@/utils` cube core the CAD viewport uses (same Z-up world
// convention: TOP = +Z, FRONT = −Y), so both cubes stay in sync by
// construction.  Only the GRBL-specific shell lives here: its own
// renderer on a small overlay canvas (the CAD viewport blits into
// its own render target instead), pointer wiring, and the per-frame
// update the main rAF calls.

const CUBE_CANVAS_SIZE = 96;
const SNAP_VIEW_DISTANCE = 400; // ortho: only the direction matters

interface SnapAnimationState {
  animating: boolean;
  start: number;
  startPosition: THREE.Vector3;
  targetPosition: THREE.Vector3;
  startUp: THREE.Vector3;
  targetUp: THREE.Vector3;
}

export interface GrblOrientationCube {
  update(mainCamera: THREE.OrthographicCamera, controls: OrbitControls): void;
  dispose(): void;
}

export function buildGrblOrientationCube({
  canvas,
  mainCamera,
  controls,
}: {
  canvas: HTMLCanvasElement;
  mainCamera: THREE.OrthographicCamera;
  controls: OrbitControls;
}): GrblOrientationCube {
  const renderer = new THREE.WebGLRenderer({
    canvas,
    antialias: true,
    alpha: true,
  });
  renderer.setPixelRatio(window.devicePixelRatio);
  renderer.setSize(CUBE_CANVAS_SIZE, CUBE_CANVAS_SIZE, false);

  const cubeGroup = buildViewCubeGroup();
  const scene = createViewCubeScene(cubeGroup);
  const camera = createViewCubeCamera();
  const raycaster = new THREE.Raycaster();

  const animation: SnapAnimationState = {
    animating: false,
    start: 0,
    startPosition: new THREE.Vector3(),
    targetPosition: new THREE.Vector3(),
    startUp: new THREE.Vector3(),
    targetUp: new THREE.Vector3(),
  };

  const setRaycaster = (event: PointerEvent) => {
    const rect = canvas.getBoundingClientRect();
    const ndc = new THREE.Vector2(
      ((event.clientX - rect.left) / rect.width) * 2 - 1,
      -((event.clientY - rect.top) / rect.height) * 2 + 1,
    );
    raycaster.setFromCamera(ndc, camera);
  };

  const onPointerDown = (event: PointerEvent) => {
    setRaycaster(event);
    const hit = raycastViewCube(raycaster, cubeGroup);
    if (!hit || hit.type === "rotation_arrow") {
      return;
    }
    // Face/edge/corner → view direction in world space (the cube
    // group is unrotated; the CUBE CAMERA mirrors the main camera).
    const direction = getCubeHitTargetDirection(hit);
    animation.startPosition.copy(mainCamera.position);
    animation.targetPosition
      .copy(controls.target)
      .addScaledVector(direction, SNAP_VIEW_DISTANCE);
    animation.startUp.copy(mainCamera.up).normalize();
    animation.targetUp
      .copy(getQuantizedCubeUp(direction, mainCamera.up))
      .normalize();
    animation.start = performance.now();
    animation.animating = true;
    controls.enabled = false;
  };

  const onPointerMove = (event: PointerEvent) => {
    setRaycaster(event);
    applyCubeHover(cubeGroup, raycastViewCube(raycaster, cubeGroup));
  };

  const onPointerLeave = () => {
    clearCubeHover(cubeGroup);
  };

  canvas.addEventListener("pointerdown", onPointerDown);
  canvas.addEventListener("pointermove", onPointerMove);
  canvas.addEventListener("pointerleave", onPointerLeave);

  let disposed = false;
  return {
    update() {
      if (disposed) {
        return;
      }
      syncCubeCamera(mainCamera, controls.target, camera);
      if (animation.animating) {
        const done = animateCameraTowardTarget(
          mainCamera,
          controls,
          animation.startPosition,
          animation.targetPosition,
          animation.start,
          performance.now(),
          animation.startUp,
          animation.targetUp,
        );
        if (done) {
          animation.animating = false;
          controls.enabled = true;
        }
      }
      renderer.render(scene, camera);
    },
    dispose() {
      disposed = true;
      canvas.removeEventListener("pointerdown", onPointerDown);
      canvas.removeEventListener("pointermove", onPointerMove);
      canvas.removeEventListener("pointerleave", onPointerLeave);
      disposeViewCubeGroup(cubeGroup);
      renderer.dispose();
    },
  };
}
