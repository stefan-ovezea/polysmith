import * as THREE from "three";

import { projectWorldPointToViewport } from "@/utils";
import { getOrthographicViewHeight } from "./grid";

const SKETCH_SCREEN_SPRITE_BASE_HEIGHT = 900;
const SKETCH_LABEL_SCREEN_SCALE = 0.72;
const SKETCH_CONSTRAINT_SCREEN_SIZE = 34;
const SKETCH_LABEL_COLLISION_PADDING = 6;

const lastUpdateKeyByRenderer = new WeakMap<THREE.WebGLRenderer, string>();

type SpriteRect = {
  center: { x: number; y: number };
  width: number;
  height: number;
};

export function updateScreenSpaceSketchSprites({
  renderer,
  camera,
  sketchDimensionObjects,
  sketchConstraintObjects,
}: {
  renderer: THREE.WebGLRenderer;
  camera: THREE.OrthographicCamera;
  sketchDimensionObjects: readonly THREE.Object3D[];
  sketchConstraintObjects: readonly THREE.Object3D[];
}) {
  const updateKey = screenSpaceSpriteUpdateKey({
    renderer,
    camera,
    sketchDimensionObjects,
    sketchConstraintObjects,
  });
  if (lastUpdateKeyByRenderer.get(renderer) === updateKey) {
    return;
  }
  lastUpdateKeyByRenderer.set(renderer, updateKey);

  const viewportHeight = Math.max(renderer.domElement.clientHeight, 1);
  const viewportScale = Math.min(
    Math.max(viewportHeight / SKETCH_SCREEN_SPRITE_BASE_HEIGHT, 0.82),
    1.18,
  );
  const worldUnitsPerPixel = getOrthographicViewHeight(camera) / viewportHeight;
  const cameraRight = new THREE.Vector3()
    .setFromMatrixColumn(camera.matrixWorld, 0)
    .normalize();
  const cameraUp = new THREE.Vector3()
    .setFromMatrixColumn(camera.matrixWorld, 1)
    .normalize();
  const dimensionRects: SpriteRect[] = [];

  for (const object of sketchDimensionObjects) {
    const rect = updateSpriteScale({
      object,
      scale: SKETCH_LABEL_SCREEN_SCALE,
      viewportScale,
      worldUnitsPerPixel,
      cameraRight,
      cameraUp,
      camera,
      renderer,
    });
    if (rect) {
      dimensionRects.push(rect);
    }
  }

  for (const object of sketchConstraintObjects) {
    const rect = updateSpriteScale({
      object,
      scale: SKETCH_CONSTRAINT_SCREEN_SIZE / 42,
      viewportScale,
      worldUnitsPerPixel,
      cameraRight,
      cameraUp,
      camera,
      renderer,
    });
    if (!rect) {
      continue;
    }
    const sprite = object as THREE.Sprite;
    resolveConstraintDimensionLabelCollisions({
      sprite,
      rect,
      dimensionRects,
      cameraRight,
      cameraUp,
      worldUnitsPerPixel,
    });
  }
}

function screenSpaceSpriteUpdateKey({
  renderer,
  camera,
  sketchDimensionObjects,
  sketchConstraintObjects,
}: {
  renderer: THREE.WebGLRenderer;
  camera: THREE.OrthographicCamera;
  sketchDimensionObjects: readonly THREE.Object3D[];
  sketchConstraintObjects: readonly THREE.Object3D[];
}) {
  const canvas = renderer.domElement;
  return [
    canvas.clientWidth,
    canvas.clientHeight,
    camera.zoom,
    camera.position.x,
    camera.position.y,
    camera.position.z,
    camera.quaternion.x,
    camera.quaternion.y,
    camera.quaternion.z,
    camera.quaternion.w,
    objectListKey(sketchDimensionObjects),
    objectListKey(sketchConstraintObjects),
  ].join("|");
}

function objectListKey(objects: readonly THREE.Object3D[]) {
  if (objects.length === 0) {
    return "0";
  }
  return objects
    .map((object) =>
      [
        object.uuid,
        object.visible ? "v" : "h",
        object.position.x,
        object.position.y,
        object.position.z,
        object.scale.x,
        object.scale.y,
      ].join(":"),
    )
    .join("|");
}

function updateSpriteScale({
  object,
  scale,
  viewportScale,
  worldUnitsPerPixel,
  cameraRight,
  cameraUp,
  camera,
  renderer,
}: {
  object: THREE.Object3D;
  scale: number;
  viewportScale: number;
  worldUnitsPerPixel: number;
  cameraRight: THREE.Vector3;
  cameraUp: THREE.Vector3;
  camera: THREE.OrthographicCamera;
  renderer: THREE.WebGLRenderer;
}): SpriteRect | null {
  const sprite = object as THREE.Sprite;
  const screenSize = sprite.userData.screenSize as
    | { width: number; height: number }
    | undefined;
  if (!screenSize || !sprite.isSprite) {
    return null;
  }

  const basePosition = sprite.userData.basePosition as
    | [number, number, number]
    | null
    | undefined;
  if (basePosition) {
    sprite.position.set(...basePosition);
  }
  sprite.scale.set(
    screenSize.width * scale * viewportScale * worldUnitsPerPixel,
    screenSize.height * scale * viewportScale * worldUnitsPerPixel,
    1,
  );

  const dimensionStart = sprite.userData.dimensionStart as
    | [number, number, number]
    | undefined;
  const dimensionEnd = sprite.userData.dimensionEnd as
    | [number, number, number]
    | undefined;
  if (dimensionStart && dimensionEnd && sprite.material) {
    updateDimensionLabelRotationAndOffset({
      sprite,
      dimensionStart,
      dimensionEnd,
      cameraRight,
      cameraUp,
      camera,
      renderer,
      worldUnitsPerPixel,
    });
  }

  const center = projectWorldPointToViewport(
    [sprite.position.x, sprite.position.y, sprite.position.z],
    camera,
    renderer,
  );
  if (!center) {
    return null;
  }
  return {
    center,
    width: screenSize.width * scale * viewportScale,
    height: screenSize.height * scale * viewportScale,
  };
}

function updateDimensionLabelRotationAndOffset({
  sprite,
  dimensionStart,
  dimensionEnd,
  cameraRight,
  cameraUp,
  camera,
  renderer,
  worldUnitsPerPixel,
}: {
  sprite: THREE.Sprite;
  dimensionStart: [number, number, number];
  dimensionEnd: [number, number, number];
  cameraRight: THREE.Vector3;
  cameraUp: THREE.Vector3;
  camera: THREE.OrthographicCamera;
  renderer: THREE.WebGLRenderer;
  worldUnitsPerPixel: number;
}) {
  const material = sprite.material as THREE.SpriteMaterial;
  if (
    sprite.userData.dimensionKind === "angle" ||
    sprite.userData.dimensionKind === "line_angle"
  ) {
    material.rotation = 0;
    return;
  }

  const start = projectWorldPointToViewport(dimensionStart, camera, renderer);
  const end = projectWorldPointToViewport(dimensionEnd, camera, renderer);
  if (!start || !end) {
    material.rotation = 0;
    return;
  }

  const dx = end.x - start.x;
  const dy = end.y - start.y;
  let angle = Math.atan2(dy, dx);
  if (angle > Math.PI / 2) {
    angle -= Math.PI;
  } else if (angle < -Math.PI / 2) {
    angle += Math.PI;
  }
  material.rotation = -angle;

  const lineLength = Math.hypot(dx, dy);
  if (lineLength <= 1e-6) {
    return;
  }

  const midpoint = {
    x: (start.x + end.x) / 2,
    y: (start.y + end.y) / 2,
  };
  const labelCenter = projectWorldPointToViewport(
    [sprite.position.x, sprite.position.y, sprite.position.z],
    camera,
    renderer,
  );
  let normalX = -dy / lineLength;
  let normalY = dx / lineLength;
  if (
    labelCenter &&
    (labelCenter.x - midpoint.x) * normalX +
      (labelCenter.y - midpoint.y) * normalY <
      0
  ) {
    normalX = -normalX;
    normalY = -normalY;
  }

  const labelOffsetPixels = 5;
  sprite.position
    .addScaledVector(
      cameraRight,
      normalX * labelOffsetPixels * worldUnitsPerPixel,
    )
    .addScaledVector(
      cameraUp,
      -normalY * labelOffsetPixels * worldUnitsPerPixel,
    );
}

function resolveConstraintDimensionLabelCollisions({
  sprite,
  rect,
  dimensionRects,
  cameraRight,
  cameraUp,
  worldUnitsPerPixel,
}: {
  sprite: THREE.Sprite;
  rect: SpriteRect;
  dimensionRects: readonly SpriteRect[];
  cameraRight: THREE.Vector3;
  cameraUp: THREE.Vector3;
  worldUnitsPerPixel: number;
}) {
  for (const dimensionRect of dimensionRects) {
    const dx = rect.center.x - dimensionRect.center.x;
    const dy = rect.center.y - dimensionRect.center.y;
    const overlapX =
      (rect.width + dimensionRect.width) / 2 +
      SKETCH_LABEL_COLLISION_PADDING -
      Math.abs(dx);
    const overlapY =
      (rect.height + dimensionRect.height) / 2 +
      SKETCH_LABEL_COLLISION_PADDING -
      Math.abs(dy);
    if (overlapX <= 0 || overlapY <= 0) {
      continue;
    }

    const moveX = overlapX < overlapY ? (dx >= 0 ? 1 : -1) * overlapX : 0;
    const moveY = overlapX < overlapY ? 0 : (dy >= 0 ? 1 : -1) * overlapY;
    sprite.position
      .addScaledVector(cameraRight, moveX * worldUnitsPerPixel)
      .addScaledVector(cameraUp, -moveY * worldUnitsPerPixel);
    rect.center.x += moveX;
    rect.center.y += moveY;
  }
}
