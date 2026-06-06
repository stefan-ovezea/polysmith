import * as THREE from "three";
import { themeColor } from "./viewport.utils";

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

export type CubeFace = "TOP" | "BOTTOM" | "FRONT" | "BACK" | "LEFT" | "RIGHT";

export type ViewCubeHit =
  | { type: "face"; face: CubeFace }
  | { type: "edge"; face1: CubeFace; face2: CubeFace }
  | { type: "corner"; faces: [CubeFace, CubeFace, CubeFace] }
  | { type: "rotation_arrow"; direction: -1 | 1 }
  | null;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const CUBE_SIZE = 1;
const FACE_OFFSET = 0.49;
const FACE_PANEL_SIZE = CUBE_SIZE;
const FACE_THICKNESS = 0.02;
const EDGE_THICKNESS = 0.04;
const CORNER_RADIUS = 0.052;
const CUBE_VIEWPORT_SIZE = 140;
const CUBE_VIEWPORT_MARGIN = 16;
const ANIMATION_DURATION = 300;

const FACE_NORMALS: Record<CubeFace, THREE.Vector3> = {
  TOP: new THREE.Vector3(0, 1, 0),
  BOTTOM: new THREE.Vector3(0, -1, 0),
  FRONT: new THREE.Vector3(0, 0, 1),
  BACK: new THREE.Vector3(0, 0, -1),
  RIGHT: new THREE.Vector3(1, 0, 0),
  LEFT: new THREE.Vector3(-1, 0, 0),
};

function viewCubeColor(token: string, fallback: string) {
  return themeColor(token, fallback);
}

function faceColors(): Record<CubeFace, string> {
  return {
    RIGHT: viewCubeColor("--cad-viewcube-right", "#242323"),
    LEFT: viewCubeColor("--cad-viewcube-left", "#1f1f1f"),
    TOP: viewCubeColor("--cad-viewcube-top", "#2b2a2a"),
    BOTTOM: viewCubeColor("--cad-viewcube-bottom", "#1a1919"),
    FRONT: viewCubeColor("--cad-viewcube-front", "#262525"),
    BACK: viewCubeColor("--cad-viewcube-back", "#202020"),
  };
}

// ---------------------------------------------------------------------------
// Face label texture
// ---------------------------------------------------------------------------

function createFaceLabelTexture(
  label: string,
  bgColor: string,
): THREE.CanvasTexture {
  const size = 256;
  const canvas = document.createElement("canvas");
  canvas.width = size;
  canvas.height = size;
  const ctx = canvas.getContext("2d")!;

  const gradient = ctx.createLinearGradient(0, 0, size, size);
  gradient.addColorStop(0, bgColor);
  gradient.addColorStop(
    1,
    viewCubeColor("--cad-viewcube-gradient-end", "#141414"),
  );
  ctx.fillStyle = gradient;
  ctx.fillRect(0, 0, size, size);

  ctx.strokeStyle = viewCubeColor("--cad-viewcube-border", "#3b494c");
  ctx.lineWidth = 10;
  ctx.strokeRect(8, 8, size - 16, size - 16);

  ctx.fillStyle = viewCubeColor("--cad-viewcube-label", "#e5e2e1");
  ctx.font = `700 38px "Space Grotesk", "Inter", sans-serif`;
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.fillText(label, size / 2, size / 2);

  const texture = new THREE.CanvasTexture(canvas);
  texture.colorSpace = THREE.SRGBColorSpace;
  texture.needsUpdate = true;
  return texture;
}

function createRotationArrowTexture(direction: -1 | 1): THREE.CanvasTexture {
  const size = 256;
  const canvas = document.createElement("canvas");
  canvas.width = size;
  canvas.height = size;
  const ctx = canvas.getContext("2d")!;

  ctx.clearRect(0, 0, size, size);
  ctx.lineCap = "butt";
  ctx.lineJoin = "round";

  if (direction > 0) {
    ctx.translate(size, 0);
    ctx.scale(-1, 1);
  }

  ctx.strokeStyle = viewCubeColor("--cad-viewcube-arrow", "#c3f5ff");
  ctx.fillStyle = viewCubeColor("--cad-viewcube-arrow", "#c3f5ff");
  ctx.lineWidth = 58;

  const center = 128;
  const radius = 88;
  ctx.beginPath();
  ctx.arc(center, center + 16, radius, Math.PI * 1.5, Math.PI, true);
  ctx.stroke();

  ctx.beginPath();
  ctx.moveTo(198, 38);
  ctx.lineTo(118, 0);
  ctx.lineTo(126, 106);
  ctx.closePath();
  ctx.fill();

  const texture = new THREE.CanvasTexture(canvas);
  texture.colorSpace = THREE.SRGBColorSpace;
  texture.needsUpdate = true;
  return texture;
}

// ---------------------------------------------------------------------------
// Build the view cube group (26 meshes)
// ---------------------------------------------------------------------------

export function buildViewCubeGroup(): THREE.Group {
  const group = new THREE.Group();
  const colors = faceColors();
  const edgeColor = viewCubeColor("--cad-viewcube-edge", "#2d3436");
  const cornerColor = viewCubeColor("--cad-viewcube-corner", "#3a4143");
  const arrowMutedColor = viewCubeColor("--cad-viewcube-arrow-muted", "#5d6d70");

  // -- faces ----------------------------------------------------------------
  // Labels follow mechanical CAD convention: +Z = TOP, -Y = FRONT.
  const faceConfigs: Array<{
    face: CubeFace;
    label: string;
    position: [number, number, number];
    rotation: [number, number, number];
    color: string;
  }> = [
    {
      face: "RIGHT",
      label: "RIGHT",
      position: [FACE_OFFSET, 0, 0],
      rotation: [0, Math.PI / 2, 0],
      color: colors.RIGHT,
    },
    {
      face: "LEFT",
      label: "LEFT",
      position: [-FACE_OFFSET, 0, 0],
      rotation: [0, -Math.PI / 2, 0],
      color: colors.LEFT,
    },
    {
      face: "TOP",
      label: "BACK",
      position: [0, FACE_OFFSET, 0],
      rotation: [-Math.PI / 2, 0, 0],
      color: colors.TOP,
    },
    {
      face: "BOTTOM",
      label: "FRONT",
      position: [0, -FACE_OFFSET, 0],
      rotation: [Math.PI / 2, 0, 0],
      color: colors.BOTTOM,
    },
    {
      face: "FRONT",
      label: "TOP",
      position: [0, 0, FACE_OFFSET],
      rotation: [0, 0, 0],
      color: colors.FRONT,
    },
    {
      face: "BACK",
      label: "BOTTOM",
      position: [0, 0, -FACE_OFFSET],
      rotation: [0, Math.PI, 0],
      color: colors.BACK,
    },
  ];

  for (const cfg of faceConfigs) {
    const geometry = new THREE.BoxGeometry(
      FACE_PANEL_SIZE,
      FACE_PANEL_SIZE,
      FACE_THICKNESS,
    );
    const texture = createFaceLabelTexture(cfg.label, cfg.color);
    const material = new THREE.MeshBasicMaterial({
      map: texture,
      color: viewCubeColor("--cad-viewcube-material", "#ffffff"),
    });
    const mesh = new THREE.Mesh(geometry, material);
    mesh.position.set(...cfg.position);
    mesh.rotation.set(...cfg.rotation);
    mesh.userData = {
      cubePart: true,
      cubeType: "face",
      face: cfg.face,
      baseColor: viewCubeColor("--cad-viewcube-material", "#ffffff"),
    };
    mesh.renderOrder = 0;
    group.add(mesh);
  }

  // -- edges ----------------------------------------------------------------
  const et = EDGE_THICKNESS;
  const edgeConfigs: Array<{
    faces: [CubeFace, CubeFace];
    size: [number, number, number];
    position: [number, number, number];
  }> = [
    // 4 edges along X at Y=0.5
    {
      faces: ["TOP", "FRONT"],
      size: [CUBE_SIZE + et * 0.5, et, et],
      position: [0, 0.5, 0.5],
    },
    {
      faces: ["TOP", "BACK"],
      size: [CUBE_SIZE + et * 0.5, et, et],
      position: [0, 0.5, -0.5],
    },
    {
      faces: ["BOTTOM", "FRONT"],
      size: [CUBE_SIZE + et * 0.5, et, et],
      position: [0, -0.5, 0.5],
    },
    {
      faces: ["BOTTOM", "BACK"],
      size: [CUBE_SIZE + et * 0.5, et, et],
      position: [0, -0.5, -0.5],
    },
    // 4 edges along Y at X=±0.5, Z=±0.5
    {
      faces: ["FRONT", "RIGHT"],
      size: [et, CUBE_SIZE + et * 0.5, et],
      position: [0.5, 0, 0.5],
    },
    {
      faces: ["FRONT", "LEFT"],
      size: [et, CUBE_SIZE + et * 0.5, et],
      position: [-0.5, 0, 0.5],
    },
    {
      faces: ["BACK", "RIGHT"],
      size: [et, CUBE_SIZE + et * 0.5, et],
      position: [0.5, 0, -0.5],
    },
    {
      faces: ["BACK", "LEFT"],
      size: [et, CUBE_SIZE + et * 0.5, et],
      position: [-0.5, 0, -0.5],
    },
    // 4 edges along Z at Y=±0.5, X=±0.5
    {
      faces: ["TOP", "RIGHT"],
      size: [et, et, CUBE_SIZE + et * 0.5],
      position: [0.5, 0.5, 0],
    },
    {
      faces: ["TOP", "LEFT"],
      size: [et, et, CUBE_SIZE + et * 0.5],
      position: [-0.5, 0.5, 0],
    },
    {
      faces: ["BOTTOM", "RIGHT"],
      size: [et, et, CUBE_SIZE + et * 0.5],
      position: [0.5, -0.5, 0],
    },
    {
      faces: ["BOTTOM", "LEFT"],
      size: [et, et, CUBE_SIZE + et * 0.5],
      position: [-0.5, -0.5, 0],
    },
  ];

  for (const cfg of edgeConfigs) {
    const geometry = new THREE.BoxGeometry(...cfg.size);
    const material = new THREE.MeshBasicMaterial({ color: edgeColor });
    const mesh = new THREE.Mesh(geometry, material);
    mesh.position.set(...cfg.position);
    mesh.userData = {
      cubePart: true,
      cubeType: "edge",
      face1: cfg.faces[0],
      face2: cfg.faces[1],
      baseColor: edgeColor,
    };
    mesh.renderOrder = 1;
    group.add(mesh);
  }

  // -- corners --------------------------------------------------------------
  const cornerSigns: Array<[number, number, number]> = [
    [1, 1, 1],
    [1, 1, -1],
    [1, -1, 1],
    [1, -1, -1],
    [-1, 1, 1],
    [-1, 1, -1],
    [-1, -1, 1],
    [-1, -1, -1],
  ];

  for (const [sx, sy, sz] of cornerSigns) {
    const faces: [CubeFace, CubeFace, CubeFace] = [
      sx > 0 ? "RIGHT" : "LEFT",
      sy > 0 ? "TOP" : "BOTTOM",
      sz > 0 ? "FRONT" : "BACK",
    ];
    const geometry = new THREE.SphereGeometry(CORNER_RADIUS, 16, 12);
    const material = new THREE.MeshBasicMaterial({ color: cornerColor });
    const mesh = new THREE.Mesh(geometry, material);
    mesh.position.set(sx * 0.5, sy * 0.5, sz * 0.5);
    mesh.userData = {
      cubePart: true,
      cubeType: "corner",
      faces,
      baseColor: cornerColor,
    };
    mesh.renderOrder = 2;
    group.add(mesh);
  }

  for (const direction of [-1, 1] as const) {
    const texture = createRotationArrowTexture(direction);
    const material = new THREE.SpriteMaterial({
      map: texture,
      color: arrowMutedColor,
      transparent: true,
      opacity: 0.92,
      depthTest: false,
      depthWrite: false,
    });
    const sprite = new THREE.Sprite(material);
    sprite.scale.set(0.5, 0.38, 1);
    sprite.visible = false;
    sprite.renderOrder = 10;
    sprite.userData = {
      cubePart: true,
      cubeType: "rotation_arrow",
      direction,
      baseColor: arrowMutedColor,
    };
    group.add(sprite);
  }

  return group;
}

// ---------------------------------------------------------------------------
// Scene and camera
// ---------------------------------------------------------------------------

export function createViewCubeScene(cubeGroup: THREE.Group): THREE.Scene {
  const scene = new THREE.Scene();

  const ambient = new THREE.AmbientLight(0xffffff, 0.6);
  scene.add(ambient);

  const key = new THREE.DirectionalLight(0xffffff, 0.7);
  key.position.set(1, 1.5, 1.2);
  scene.add(key);

  const fill = new THREE.DirectionalLight(0xffffff, 0.3);
  fill.position.set(-0.8, 0.3, -0.6);
  scene.add(fill);

  scene.add(cubeGroup);
  return scene;
}

export function createViewCubeCamera(): THREE.OrthographicCamera {
  const frustum = 1.75;
  const camera = new THREE.OrthographicCamera(
    -frustum / 2,
    frustum / 2,
    frustum / 2,
    -frustum / 2,
    0.05,
    10,
  );
  camera.position.set(1.4, 0.8, 1.4);
  camera.lookAt(0, 0, 0);
  return camera;
}

// ---------------------------------------------------------------------------
// Viewport rect
// ---------------------------------------------------------------------------

export function getCubeViewportRect(
  canvasWidth: number,
  canvasHeight: number,
  dpr: number,
): { x: number; y: number; width: number; height: number } {
  const size = CUBE_VIEWPORT_SIZE * dpr;
  const margin = CUBE_VIEWPORT_MARGIN * dpr;
  return {
    x: canvasWidth - size - margin,
    y: canvasHeight - size - margin,
    width: size,
    height: size,
  };
}

// ---------------------------------------------------------------------------
// Pointer-area check
// ---------------------------------------------------------------------------

export function isPointerInCubeArea(
  event: PointerEvent,
  canvasRect: DOMRect,
  dpr: number,
): boolean {
  const size = CUBE_VIEWPORT_SIZE * dpr;
  const margin = CUBE_VIEWPORT_MARGIN * dpr;
  const canvasRight = (canvasRect.right - canvasRect.left) * dpr;
  const px = (event.clientX - canvasRect.left) * dpr;
  const py = (event.clientY - canvasRect.top) * dpr;
  return (
    px >= canvasRight - size - margin &&
    px <= canvasRight - margin &&
    py >= margin &&
    py <= margin + size
  );
}

// ---------------------------------------------------------------------------
// Sync cube camera to match main view direction
// ---------------------------------------------------------------------------

export function syncCubeCamera(
  mainCamera: THREE.Camera,
  controlsTarget: THREE.Vector3,
  cubeCamera: THREE.OrthographicCamera,
): void {
  const dir = new THREE.Vector3()
    .copy(mainCamera.position)
    .sub(controlsTarget)
    .normalize();
  cubeCamera.position.copy(dir.multiplyScalar(2));
  cubeCamera.lookAt(0, 0, 0);
}

export function updateSketchRotationArrows(
  cubeGroup: THREE.Group,
  cubeCamera: THREE.OrthographicCamera,
  visible: boolean,
): void {
  const right = new THREE.Vector3();
  const up = new THREE.Vector3();
  cubeCamera.updateMatrixWorld();
  right.setFromMatrixColumn(cubeCamera.matrixWorld, 0).normalize();
  up.setFromMatrixColumn(cubeCamera.matrixWorld, 1).normalize();

  cubeGroup.traverse((child) => {
    if (child.userData.cubeType !== "rotation_arrow") {
      return;
    }
    child.visible = visible;
    if (!visible) {
      return;
    }
    const direction = child.userData.direction as -1 | 1;
    child.position
      .copy(up)
      .multiplyScalar(0.58)
      .add(right.clone().multiplyScalar(direction * 0.48));
  });
}

// ---------------------------------------------------------------------------
// Raycast against cube meshes
// ---------------------------------------------------------------------------

export function raycastViewCube(
  raycaster: THREE.Raycaster,
  cubeGroup: THREE.Group,
): ViewCubeHit {
  const targets: THREE.Object3D[] = [];
  cubeGroup.traverse((child) => {
    if (
      ((child as THREE.Mesh).isMesh || (child as THREE.Sprite).isSprite) &&
      child.userData.cubePart &&
      child.visible
    ) {
      targets.push(child);
    }
  });

  const intersections = raycaster.intersectObjects(targets, false);
  if (intersections.length === 0) return null;

  // Pick closest; faces (renderOrder 0) < edges (1) < corners (2)
  const hit = intersections[0].object as THREE.Mesh;
  const ud = hit.userData;

  if (ud.cubeType === "face") {
    return { type: "face", face: ud.face as CubeFace };
  }
  if (ud.cubeType === "edge") {
    return {
      type: "edge",
      face1: ud.face1 as CubeFace,
      face2: ud.face2 as CubeFace,
    };
  }
  if (ud.cubeType === "corner") {
    return {
      type: "corner",
      faces: ud.faces as [CubeFace, CubeFace, CubeFace],
    };
  }
  if (ud.cubeType === "rotation_arrow") {
    return { type: "rotation_arrow", direction: ud.direction as -1 | 1 };
  }
  return null;
}

// ---------------------------------------------------------------------------
// Hit → world direction for snap
// ---------------------------------------------------------------------------

export function getCubeHitTargetDirection(
  hit: Exclude<NonNullable<ViewCubeHit>, { type: "rotation_arrow" }>,
): THREE.Vector3 {
  if (hit.type === "face") {
    return FACE_NORMALS[hit.face].clone();
  }
  if (hit.type === "edge") {
    return FACE_NORMALS[hit.face1]
      .clone()
      .add(FACE_NORMALS[hit.face2])
      .normalize();
  }
  // corner
  const dir = new THREE.Vector3();
  for (const f of hit.faces) {
    dir.add(FACE_NORMALS[f]);
  }
  return dir.normalize();
}

export function isCardinalCubeDirection(direction: THREE.Vector3): boolean {
  const normalized = direction.clone().normalize();
  return Object.values(FACE_NORMALS).some(
    (normal) => Math.abs(normalized.dot(normal)) > 0.985,
  );
}

function getDefaultUpForDirection(direction: THREE.Vector3): THREE.Vector3 {
  const worldUp = new THREE.Vector3(0, 1, 0);
  if (Math.abs(direction.dot(worldUp)) > 0.95) {
    return new THREE.Vector3(0, 0, -1);
  }
  return worldUp;
}

export function getQuantizedCubeUp(
  targetDirection: THREE.Vector3,
  currentUp: THREE.Vector3,
): THREE.Vector3 {
  const axis = targetDirection.clone().normalize();
  const defaultUp = getDefaultUpForDirection(axis);
  const baseUp = defaultUp
    .clone()
    .addScaledVector(axis, -defaultUp.dot(axis))
    .normalize();
  const projectedCurrentUp = currentUp
    .clone()
    .addScaledVector(axis, -currentUp.dot(axis));

  if (projectedCurrentUp.lengthSq() < 1e-6) {
    return baseUp;
  }

  projectedCurrentUp.normalize();
  let bestUp = baseUp.clone();
  let bestDot = -Infinity;
  for (let step = 0; step < 4; step += 1) {
    const candidate = baseUp
      .clone()
      .applyAxisAngle(axis, step * (Math.PI / 2))
      .normalize();
    const dot = candidate.dot(projectedCurrentUp);
    if (dot > bestDot) {
      bestDot = dot;
      bestUp = candidate;
    }
  }
  return bestUp;
}

// ---------------------------------------------------------------------------
// Camera animation
// ---------------------------------------------------------------------------

export function animateCameraTowardTarget(
  camera: THREE.Camera,
  controls: { target: THREE.Vector3; update: () => void },
  startPosition: THREE.Vector3,
  targetPosition: THREE.Vector3,
  startTime: number,
  currentTime: number,
  startUp?: THREE.Vector3,
  targetUp?: THREE.Vector3,
): boolean {
  const elapsed = currentTime - startTime;
  const t = Math.min(elapsed / ANIMATION_DURATION, 1);
  // cubic ease-out matching the design system cue
  const ease = 1 - Math.pow(1 - t, 3);

  camera.position.lerpVectors(startPosition, targetPosition, ease);
  if (startUp && targetUp) {
    camera.up.lerpVectors(startUp, targetUp, ease).normalize();
  }
  camera.lookAt(controls.target);
  controls.update();

  return t >= 1;
}

// ---------------------------------------------------------------------------
// Hover visual state
// ---------------------------------------------------------------------------

export function applyCubeHover(cubeGroup: THREE.Group, hit: ViewCubeHit): void {
  clearCubeHover(cubeGroup);

  if (!hit) return;

  let target: THREE.Object3D | null = null;

  cubeGroup.traverse((child) => {
    if (!(child as THREE.Mesh).isMesh || !child.userData.cubePart) return;
    const ud = child.userData;
    if (hit.type === "face" && ud.cubeType === "face" && ud.face === hit.face) {
      target = child;
    } else if (
      hit.type === "edge" &&
      ud.cubeType === "edge" &&
      ((ud.face1 === hit.face1 && ud.face2 === hit.face2) ||
        (ud.face1 === hit.face2 && ud.face2 === hit.face1))
    ) {
      target = child;
    } else if (
      hit.type === "corner" &&
      ud.cubeType === "corner" &&
      ud.faces &&
      ud.faces.length === 3 &&
      hit.faces.every((f: CubeFace) => (ud.faces as CubeFace[]).includes(f))
    ) {
      target = child;
    } else if (
      hit.type === "rotation_arrow" &&
      ud.cubeType === "rotation_arrow" &&
      ud.direction === hit.direction
    ) {
      target = child;
    }
  });

  if (target) {
    const mat = (target as THREE.Mesh | THREE.Sprite).material as
      | THREE.MeshBasicMaterial
      | THREE.SpriteMaterial;
    mat.color.set(viewCubeColor("--cad-viewcube-hover", "#00e5ff"));
  }
}

export function clearCubeHover(cubeGroup: THREE.Group): void {
  cubeGroup.traverse((child) => {
    if (
      !((child as THREE.Mesh).isMesh || (child as THREE.Sprite).isSprite) ||
      !child.userData.cubePart
    )
      return;
    const ud = child.userData;
    const mat = (child as THREE.Mesh | THREE.Sprite).material as
      | THREE.MeshBasicMaterial
      | THREE.SpriteMaterial;
    mat.color.set(ud.baseColor as string);
  });
}

// ---------------------------------------------------------------------------
// Drag-to-orbit: apply pointer delta as spherical rotation
// ---------------------------------------------------------------------------

export function applyCubeDragOrbit(
  camera: THREE.Camera,
  controls: { target: THREE.Vector3; update: () => void },
  deltaX: number,
  deltaY: number,
  sensitivity: number,
): void {
  const offset = new THREE.Vector3().copy(camera.position).sub(controls.target);
  const spherical = new THREE.Spherical();
  spherical.setFromVector3(offset);

  spherical.theta -= deltaX * sensitivity;
  spherical.phi -= deltaY * sensitivity;
  spherical.phi = Math.max(0.05, Math.min(Math.PI - 0.05, spherical.phi));

  const newPos = new THREE.Vector3()
    .setFromSpherical(spherical)
    .add(controls.target);
  camera.position.copy(newPos);
  camera.lookAt(controls.target);
  controls.update();
}

// ---------------------------------------------------------------------------
// Dispose helper
// ---------------------------------------------------------------------------

export function disposeViewCubeGroup(group: THREE.Group): void {
  group.traverse((child) => {
    if (!((child as THREE.Mesh).isMesh || (child as THREE.Sprite).isSprite))
      return;
    const object = child as THREE.Mesh | THREE.Sprite;
    if ((object as THREE.Mesh).geometry) {
      (object as THREE.Mesh).geometry.dispose();
    }
    const mat = object.material as
      | THREE.MeshBasicMaterial
      | THREE.SpriteMaterial;
    if (Array.isArray(mat)) {
      for (const m of mat) {
        const basicMat = m as THREE.MeshBasicMaterial;
        if (basicMat.map) basicMat.map.dispose();
        basicMat.dispose();
      }
    } else {
      if (mat.map) mat.map.dispose();
      mat.dispose();
    }
  });
  group.clear();
}

// ---------------------------------------------------------------------------
// Render-target-based cube rendering (avoids scissor-test GPU dependency)
// ---------------------------------------------------------------------------
// On some Windows ANGLE backends the scissor test behaves differently or
// fails silently. Instead, render the cube to an offscreen target and blit
// the result to the corner with a textured NDC-space plane — no scissor.

export function createCubeRenderTarget(
  renderer: THREE.WebGLRenderer,
): THREE.WebGLRenderTarget {
  const dpr = renderer.getPixelRatio();
  const size = Math.round(CUBE_VIEWPORT_SIZE * dpr);
  return new THREE.WebGLRenderTarget(size, size, {
    minFilter: THREE.LinearFilter,
    magFilter: THREE.LinearFilter,
    format: THREE.RGBAFormat,
  });
}

export function resizeCubeRenderTarget(
  target: THREE.WebGLRenderTarget,
  dpr: number,
): void {
  const size = Math.round(CUBE_VIEWPORT_SIZE * dpr);
  if (target.width !== size || target.height !== size) {
    target.setSize(size, size);
  }
}

export interface CubeBlitScene {
  scene: THREE.Scene;
  camera: THREE.OrthographicCamera;
  mesh: THREE.Mesh;
}

export function createCubeBlitScene(
  renderTarget: THREE.WebGLRenderTarget,
): CubeBlitScene {
  const scene = new THREE.Scene();
  const camera = new THREE.OrthographicCamera(-1, 1, 1, -1, 0, 1);

  const geometry = new THREE.PlaneGeometry(1, 1);
  const material = new THREE.MeshBasicMaterial({
    map: renderTarget.texture,
    transparent: true,
    depthTest: false,
    depthWrite: false,
  });
  const mesh = new THREE.Mesh(geometry, material);
  mesh.renderOrder = 999;
  scene.add(mesh);

  return { scene, camera, mesh };
}

export function updateCubeBlitMesh(
  mesh: THREE.Mesh,
  canvasWidth: number,
  canvasHeight: number,
): void {
  const sizeCSS = CUBE_VIEWPORT_SIZE;
  const marginCSS = CUBE_VIEWPORT_MARGIN;

  // Non-uniform NDC scale so the plane renders as a square in CSS pixels.
  const ndcScaleX = (2 * sizeCSS) / canvasWidth;
  const ndcScaleY = (2 * sizeCSS) / canvasHeight;
  mesh.scale.set(ndcScaleX, ndcScaleY, 1);

  // Center position in NDC: top-right corner, inset by margin.
  mesh.position.set(
    1 - (2 * (marginCSS + sizeCSS / 2)) / canvasWidth,
    1 - (2 * (marginCSS + sizeCSS / 2)) / canvasHeight,
    0,
  );
}

export function disposeCubeBlitScene(blit: CubeBlitScene): void {
  (blit.mesh.material as THREE.MeshBasicMaterial).dispose();
  blit.mesh.geometry.dispose();
}
