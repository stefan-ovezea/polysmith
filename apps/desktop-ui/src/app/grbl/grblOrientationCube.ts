import * as THREE from "three";
import type { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";

// Mini orientation cube for the GRBL preview viewport: a small
// always-on-top widget that mirrors the main camera's rotation and
// snaps the view to a clicked face (top/front/right/…).  The GRBL
// scene is Z-up (camera.up = (0,0,1)), so the cube's Z face is the
// bed's top view.
//
// Owns a second renderer on its own canvas (corner overlay) — the
// main viewport calls update() from its rAF loop so the cube tracks
// the camera with zero extra state.

const CUBE_CANVAS_SIZE = 96;
const CUBE_VIEW_DISTANCE = 400; // snap distance for the main camera

function readCssColor(name: string, fallback: string): string {
  const value = getComputedStyle(document.documentElement)
    .getPropertyValue(name)
    .trim();
  return value || fallback;
}

// Draws one cube face (background + axis letter) onto a canvas
// texture.  The letter stays glued to the face, so the cube reads
// correctly at any rotation.
function faceTexture(letter: string, background: string): THREE.CanvasTexture {
  const canvas = document.createElement("canvas");
  canvas.width = 128;
  canvas.height = 128;
  const context = canvas.getContext("2d");
  if (context) {
    context.fillStyle = background;
    context.fillRect(0, 0, 128, 128);
    context.fillStyle = "rgba(255,255,255,0.92)";
    context.font = "600 56px system-ui, sans-serif";
    context.textAlign = "center";
    context.textBaseline = "middle";
    context.fillText(letter, 64, 64);
  }
  const texture = new THREE.CanvasTexture(canvas);
  texture.colorSpace = THREE.SRGBColorSpace;
  return texture;
}

export interface GrblOrientationCube {
  update(mainCamera: THREE.OrthographicCamera, controls: OrbitControls): void;
  dispose(): void;
}

interface CubeInternals {
  renderer: THREE.WebGLRenderer;
  scene: THREE.Scene;
  camera: THREE.OrthographicCamera;
  cube: THREE.Mesh;
  raycaster: THREE.Raycaster;
  mouse: THREE.Vector2;
}

const FACE_LETTERS = ["+X", "−X", "+Y", "−Y", "+Z", "−Z"];

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

  const scene = new THREE.Scene();
  const camera = new THREE.OrthographicCamera(-3, 3, 3, -3, -20, 20);
  camera.up.set(0, 0, 1);
  camera.position.set(2.6, 2.6, 2.6).normalize().multiplyScalar(6);
  camera.lookAt(0, 0, 0);

  // Axis-colored faces; the negative faces dim their axis color so
  // ± pairs are distinguishable at a glance.
  const axisX = readCssColor("--cad-cube-x", "#e07a7a");
  const axisY = readCssColor("--cad-cube-y", "#7fb86e");
  const axisZ = readCssColor("--cad-cube-z", "#6f9fd8");
  const dim = (hex: string) => {
    const color = new THREE.Color(hex);
    return `#${color.multiplyScalar(0.55).getHexString()}`;
  };
  const faceColors = [axisX, dim(axisX), axisY, dim(axisY), axisZ, dim(axisZ)];
  const materials = FACE_LETTERS.map((letter, index) => {
    const material = new THREE.MeshBasicMaterial({
      map: faceTexture(letter, faceColors[index]),
      toneMapped: false,
    });
    return material;
  });

  const cube = new THREE.Mesh(new THREE.BoxGeometry(2, 2, 2), materials);
  scene.add(cube);

  const edges = new THREE.LineSegments(
    new THREE.EdgesGeometry(new THREE.BoxGeometry(2, 2, 2)),
    new THREE.LineBasicMaterial({
      color: readCssColor("--cad-cube-edge", "#8a8f98"),
      transparent: true,
      opacity: 0.9,
    }),
  );
  cube.add(edges);

  // Face picking: the raycast happens in the cube's rotated object
  // space, so the result stays valid as the cube mirrors the camera.
  const raycaster = new THREE.Raycaster();
  const mouse = new THREE.Vector2();
  const internals: CubeInternals = { renderer, scene, camera, cube, raycaster, mouse };

  const onPointerDown = (event: PointerEvent) => {
    const rect = canvas.getBoundingClientRect();
    mouse.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
    mouse.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;
    raycaster.setFromCamera(mouse, camera);
    const hits = raycaster.intersectObject(cube, false);
    const hit = hits.find((entry) => entry.face !== undefined);
    const faceNormal = hit?.face?.normal;
    if (!faceNormal) {
      return;
    }
    // World-space direction of the clicked face (the face normal is
    // in the cube's object space; the quaternion mirrors the main
    // camera's rotation).
    const direction = faceNormal
      .clone()
      .applyQuaternion(cube.quaternion)
      .normalize();
    // Snap the main camera to look at the scene along that axis.
    const center = controls.target.clone();
    mainCamera.position.copy(
      center.clone().add(direction.clone().multiplyScalar(CUBE_VIEW_DISTANCE)),
    );
    // Top/bottom faces: screen-up follows the bed's Y axis; all
    // other faces keep the scene's Z as screen-up.
    mainCamera.up.set(0, 0, 1);
    if (Math.abs(direction.z) > 0.9) {
      mainCamera.up.set(0, 1, 0);
    }
    controls.update();
  };
  canvas.addEventListener("pointerdown", onPointerDown);

  let disposed = false;
  return {
    update() {
      if (disposed) {
        return;
      }
      cube.quaternion.copy(mainCamera.quaternion).invert();
      renderer.render(scene, camera);
    },
    dispose() {
      disposed = true;
      canvas.removeEventListener("pointerdown", onPointerDown);
      cube.geometry.dispose();
      edges.geometry.dispose();
      for (const material of materials) {
        material.map?.dispose();
        material.dispose();
      }
      renderer.dispose();
    },
  };
}
