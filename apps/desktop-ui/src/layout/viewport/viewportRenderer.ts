import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";

interface ConfigureViewportControlsParams {
  controls: OrbitControls;
  canvas: HTMLCanvasElement;
}

interface SetupSnapshotCaptureParams {
  host: HTMLElement;
  renderer: THREE.WebGLRenderer;
  scene: THREE.Scene;
  camera: THREE.Camera;
  onSnapshotCaptureReady?: (capture: (() => string | null) | null) => void;
}

interface ResizeViewportRendererParams {
  host: HTMLElement;
  renderer: THREE.WebGLRenderer;
  camera: THREE.OrthographicCamera;
  orthoFrustumHeight: number;
  setViewportSize: (size: { width: number; height: number }) => void;
}

interface HandleViewportWheelZoomParams {
  event: WheelEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.OrthographicCamera;
  controls: OrbitControls;
  minZoom: number;
  maxZoom: number;
  zoomSpeed: number;
  pointerPan: number;
}

export function configureViewportControls({
  controls,
  canvas,
}: ConfigureViewportControlsParams) {
  controls.enableDamping = false;
  controls.enableZoom = false;
  controls.screenSpacePanning = true;
  controls.minDistance = 24;
  controls.maxDistance = 6000;
  controls.mouseButtons.LEFT = null;
  controls.mouseButtons.MIDDLE = THREE.MOUSE.PAN;
  controls.mouseButtons.RIGHT = null;
  controls.addEventListener("start", () => {
    canvas.classList.add("cad-viewport-canvas-dragging");
  });
  controls.addEventListener("end", () => {
    canvas.classList.remove("cad-viewport-canvas-dragging");
  });
}

export function setupViewportSnapshotCapture({
  host,
  renderer,
  scene,
  camera,
  onSnapshotCaptureReady,
}: SetupSnapshotCaptureParams) {
  onSnapshotCaptureReady?.(() => {
    renderer.render(scene, camera);
    const source = renderer.domElement;
    if (source.width === 0 || source.height === 0) {
      return null;
    }
    const thumbnail = window.document.createElement("canvas");
    thumbnail.width = 240;
    thumbnail.height = 150;
    const context = thumbnail.getContext("2d");
    if (!context) {
      return null;
    }
    const thumbnailBackground = window
      .getComputedStyle(host)
      .getPropertyValue("--cad-project-thumbnail-bg")
      .trim();
    if (thumbnailBackground) {
      context.fillStyle = thumbnailBackground;
      context.fillRect(0, 0, thumbnail.width, thumbnail.height);
    }
    context.drawImage(source, 0, 0, thumbnail.width, thumbnail.height);
    return thumbnail.toDataURL("image/png");
  });
}

export function resizeViewportRenderer({
  host,
  renderer,
  camera,
  orthoFrustumHeight,
  setViewportSize,
}: ResizeViewportRendererParams) {
  const width = Math.max(host.clientWidth, 1);
  const height = Math.max(host.clientHeight, 1);
  setViewportSize({ width, height });
  renderer.setSize(width, height, false);
  const aspect = width / height;
  camera.left = (-orthoFrustumHeight * aspect) / 2;
  camera.right = (orthoFrustumHeight * aspect) / 2;
  camera.top = orthoFrustumHeight / 2;
  camera.bottom = -orthoFrustumHeight / 2;
  camera.updateProjectionMatrix();
}

export function handleViewportWheelZoom({
  event,
  renderer,
  camera,
  controls,
  minZoom,
  maxZoom,
  zoomSpeed,
  pointerPan,
}: HandleViewportWheelZoomParams) {
  event.preventDefault();

  const before = worldPointAtWheelPointer({ event, renderer, camera });
  const deltaMultiplier =
    event.deltaMode === WheelEvent.DOM_DELTA_LINE
      ? 16
      : event.deltaMode === WheelEvent.DOM_DELTA_PAGE
        ? renderer.domElement.clientHeight
        : 1;
  const zoomFactor = Math.exp(-event.deltaY * deltaMultiplier * zoomSpeed);
  const nextZoom = THREE.MathUtils.clamp(
    camera.zoom * zoomFactor,
    minZoom,
    maxZoom,
  );

  if (Math.abs(nextZoom - camera.zoom) < 1e-6) {
    return;
  }

  camera.zoom = nextZoom;
  camera.updateProjectionMatrix();

  const after = worldPointAtWheelPointer({ event, renderer, camera });
  const pointerShift = before.sub(after).multiplyScalar(pointerPan);
  camera.position.add(pointerShift);
  controls.target.add(pointerShift);
  controls.update();
}

function worldPointAtWheelPointer({
  event,
  renderer,
  camera,
}: Pick<HandleViewportWheelZoomParams, "event" | "renderer" | "camera">) {
  const rect = renderer.domElement.getBoundingClientRect();
  const ndcX = ((event.clientX - rect.left) / rect.width) * 2 - 1;
  const ndcY = -((event.clientY - rect.top) / rect.height) * 2 + 1;
  camera.updateMatrixWorld();
  return new THREE.Vector3(ndcX, ndcY, 0).unproject(camera);
}
