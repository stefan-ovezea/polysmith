import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";

import type { CubeBlitScene } from "@/utils";
import {
  animateCameraTowardTarget,
  isCardinalCubeDirection,
  resizeCubeRenderTarget,
  syncCubeCamera,
  updateCubeBlitMesh,
  updateSketchRotationArrows,
} from "@/utils";

interface MutableRef<T> {
  current: T;
}

interface ViewCubeAnimationRefs {
  animating: MutableRef<boolean>;
  start: MutableRef<number>;
  startPos: MutableRef<THREE.Vector3>;
  targetPos: MutableRef<THREE.Vector3>;
  startUp: MutableRef<THREE.Vector3>;
  targetUp: MutableRef<THREE.Vector3>;
}

interface RenderViewCubeFrameParams {
  renderer: THREE.WebGLRenderer;
  camera: THREE.OrthographicCamera;
  controls: OrbitControls;
  cubeGroupRef: MutableRef<THREE.Group | null>;
  cubeSceneRef: MutableRef<THREE.Scene | null>;
  cubeCameraRef: MutableRef<THREE.OrthographicCamera | null>;
  cubeRenderTargetRef: MutableRef<THREE.WebGLRenderTarget | null>;
  cubeBlitSceneRef: MutableRef<CubeBlitScene | null>;
  animationRefs: ViewCubeAnimationRefs;
}

interface RotateCameraAroundViewParams {
  camera: THREE.OrthographicCamera;
  controls: OrbitControls;
  direction: -1 | 1;
  animationRefs: ViewCubeAnimationRefs;
}

export function rotateCameraAroundCurrentView({
  camera,
  controls,
  direction,
  animationRefs,
}: RotateCameraAroundViewParams) {
  const viewOffset = new THREE.Vector3()
    .copy(camera.position)
    .sub(controls.target);
  if (viewOffset.lengthSq() < 1e-6) {
    return;
  }

  const axis = viewOffset.clone().normalize();
  const angle = direction * (Math.PI / 2);
  animationRefs.startPos.current.copy(camera.position);
  animationRefs.targetPos.current.copy(camera.position);
  animationRefs.startUp.current.copy(camera.up).normalize();
  animationRefs.targetUp.current
    .copy(camera.up)
    .applyAxisAngle(axis, angle)
    .normalize();
  animationRefs.start.current = performance.now();
  animationRefs.animating.current = true;
  controls.enabled = false;
}

export function renderViewCubeFrame({
  renderer,
  camera,
  controls,
  cubeGroupRef,
  cubeSceneRef,
  cubeCameraRef,
  cubeRenderTargetRef,
  cubeBlitSceneRef,
  animationRefs,
}: RenderViewCubeFrameParams) {
  const cubeGroup = cubeGroupRef.current;
  const cubeScene = cubeSceneRef.current;
  const cubeCamera = cubeCameraRef.current;
  const cubeTarget = cubeRenderTargetRef.current;
  const blit = cubeBlitSceneRef.current;
  if (!cubeGroup || !cubeScene || !cubeCamera || !cubeTarget || !blit) {
    return;
  }

  syncCubeCamera(camera, controls.target, cubeCamera);
  updateSketchRotationArrows(
    cubeGroup,
    cubeCamera,
    isFacingCardinalCubeFace(camera, controls),
  );
  tickViewCubeCameraAnimation({
    camera,
    controls,
    animationRefs,
  });
  renderCubeToViewportCorner({
    renderer,
    cubeScene,
    cubeCamera,
    cubeTarget,
    blit,
  });
}

function isFacingCardinalCubeFace(
  camera: THREE.OrthographicCamera,
  controls: OrbitControls,
) {
  const viewOffset = new THREE.Vector3()
    .copy(camera.position)
    .sub(controls.target)
    .normalize();
  return isCardinalCubeDirection(viewOffset);
}

function tickViewCubeCameraAnimation({
  camera,
  controls,
  animationRefs,
}: {
  camera: THREE.OrthographicCamera;
  controls: OrbitControls;
  animationRefs: ViewCubeAnimationRefs;
}) {
  if (!animationRefs.animating.current) {
    return;
  }

  const done = animateCameraTowardTarget(
    camera,
    controls,
    animationRefs.startPos.current,
    animationRefs.targetPos.current,
    animationRefs.start.current,
    performance.now(),
    animationRefs.startUp.current,
    animationRefs.targetUp.current,
  );
  if (done) {
    animationRefs.animating.current = false;
    controls.enabled = true;
  }
}

function renderCubeToViewportCorner({
  renderer,
  cubeScene,
  cubeCamera,
  cubeTarget,
  blit,
}: {
  renderer: THREE.WebGLRenderer;
  cubeScene: THREE.Scene;
  cubeCamera: THREE.OrthographicCamera;
  cubeTarget: THREE.WebGLRenderTarget;
  blit: CubeBlitScene;
}) {
  const dpr = renderer.getPixelRatio();
  resizeCubeRenderTarget(cubeTarget, dpr);

  const oldTarget = renderer.getRenderTarget();
  renderer.setRenderTarget(cubeTarget);
  renderer.render(cubeScene, cubeCamera);

  renderer.setRenderTarget(null);
  const width = renderer.domElement.width / dpr;
  const height = renderer.domElement.height / dpr;
  updateCubeBlitMesh(blit.mesh, width, height);

  const oldAutoClear = renderer.autoClear;
  renderer.autoClear = false;
  renderer.render(blit.scene, blit.camera);
  renderer.autoClear = oldAutoClear;
  renderer.setRenderTarget(oldTarget);
}
