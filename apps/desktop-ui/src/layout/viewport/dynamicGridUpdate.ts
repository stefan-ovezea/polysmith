import * as THREE from "three";

import type { ViewportScene } from "@/types";
import { themeColor } from "@/utils";
import {
  GRID_SKETCH_PADDING_MULTIPLIER,
  GRID_WORLD_PADDING_MULTIPLIER,
  buildDynamicGrid,
  buildAxisLines,
  buildAxisTickLabels,
  disposeDynamicGrid,
  getCardinalGridFrame,
  getGridViewBounds,
  getSketchGridFrame,
  projectPointToGridFrame,
  selectOrthographicGridSpacing,
  type ActiveSketchGridPlaneFrame,
  type DynamicGridRef,
  type GridPlaneFrame,
} from "./grid";

type MutableRef<T> = {
  current: T;
};

export function updateDynamicGrids({
  scene,
  sceneData,
  camera,
  target,
  worldGridRef,
  sketchGridRef,
  currentGridSpacingRef,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  showViewportGrid,
  showSketchGrid,
  worldUnitsPerPixel = 1,
}: {
  scene: THREE.Scene;
  sceneData: ViewportScene | null;
  camera: THREE.OrthographicCamera;
  target: THREE.Vector3;
  worldGridRef: MutableRef<DynamicGridRef | null>;
  sketchGridRef: MutableRef<DynamicGridRef | null>;
  currentGridSpacingRef: MutableRef<number>;
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: ActiveSketchGridPlaneFrame | null;
  showViewportGrid: boolean;
  showSketchGrid: boolean;
  worldUnitsPerPixel?: number;
}) {
  if (!sceneData) {
    clearDynamicGrid(scene, worldGridRef);
    clearDynamicGrid(scene, sketchGridRef);
    return;
  }

  const spacing = selectOrthographicGridSpacing(camera);
  currentGridSpacingRef.current = spacing;
  const viewOffset = new THREE.Vector3()
    .copy(camera.position)
    .sub(target)
    .normalize();
  const cardinalFrame = getCardinalGridFrame(viewOffset);
  const worldFrame: GridPlaneFrame = cardinalFrame ?? {
    origin: new THREE.Vector3(0, 0, 0),
    xAxis: new THREE.Vector3(1, 0, 0),
    yAxis: new THREE.Vector3(0, 0, 1),
    normal: new THREE.Vector3(0, 1, 0),
  };

  if (!activeSketchPlaneId) {
    if (!showViewportGrid) {
      clearDynamicGrid(scene, worldGridRef);
      clearDynamicGrid(scene, sketchGridRef);
      return;
    }

    const worldCenter = projectPointToGridFrame(target, worldFrame);
    const worldBounds = getGridViewBounds(
      camera,
      worldFrame,
      spacing,
      worldCenter,
      GRID_WORLD_PADDING_MULTIPLIER,
    );
    ensureDynamicGrid(
      scene,
      worldGridRef,
      `world:${
        cardinalFrame ? "cardinal" : "floor"
      }:${spacing}:${worldBounds.minU}:${worldBounds.maxU}:${worldBounds.minV}:${worldBounds.maxV}`,
      () => {
        const group = new THREE.Group();
        const worldGrid = buildDynamicGrid(
          worldFrame,
          spacing,
          worldBounds,
          new THREE.Color(themeColor("--color-cad-grid", "#3f4648")),
          new THREE.Color(themeColor("--color-cad-grid-axis", "#5e696c")),
          new THREE.Color(themeColor("--color-cad-grid-axis", "#7a7a7c")),
          0.34,
        );
        worldGrid.renderOrder = -10;
        group.add(worldGrid);
        return group;
      },
    );
    clearDynamicGrid(scene, sketchGridRef);
    return;
  }

  clearDynamicGrid(scene, worldGridRef);
  if (!showSketchGrid) {
    clearDynamicGrid(scene, sketchGridRef);
    return;
  }

  const sketchFrame = getSketchGridFrame(
    activeSketchPlaneId,
    activeSketchPlaneFrame,
  );
  const sketchCenter = projectPointToGridFrame(target, sketchFrame);
  const sketchBounds = getGridViewBounds(
    camera,
    sketchFrame,
    spacing,
    sketchCenter,
    GRID_SKETCH_PADDING_MULTIPLIER,
  );
  ensureDynamicGrid(
    scene,
    sketchGridRef,
    `sketch:${activeSketchPlaneId}:${spacing}:${sketchBounds.minU}:${sketchBounds.maxU}:${sketchBounds.minV}:${sketchBounds.maxV}`,
    () => {
      const group = new THREE.Group();
      const sketchGrid = buildDynamicGrid(
        sketchFrame,
        spacing,
        sketchBounds,
        new THREE.Color(themeColor("--cad-sketch-grid", "#2a383b")),
        new THREE.Color(themeColor("--cad-sketch-grid-axis", "#46585d")),
        new THREE.Color(themeColor("--cad-sketch-grid-center-axis", "#7a8a8f")),
        0.48,
      );
      sketchGrid.renderOrder = -9;
      group.add(sketchGrid);

      // Infinite coordinate axes on top of the sketch grid
      const axes = buildAxisLines(
        sketchFrame,
        new THREE.Color(themeColor("--color-axis-x", "#ff6b7a")),
        new THREE.Color(themeColor("--color-axis-y", "#2bd978")),
        0.72,
      );
      axes.renderOrder = -8;
      group.add(axes);

      // Axis tick labels at major intervals
      const tickLabels = buildAxisTickLabels(
        sketchFrame,
        spacing,
        sketchBounds,
        new THREE.Color(themeColor("--color-axis-x", "#ff6b7a")),
        new THREE.Color(themeColor("--color-axis-y", "#2bd978")),
        worldUnitsPerPixel,
      );
      tickLabels.renderOrder = -7;
      group.add(tickLabels);
      return group;
    },
  );
}

function ensureDynamicGrid(
  scene: THREE.Scene,
  ref: MutableRef<DynamicGridRef | null>,
  key: string,
  buildGroup: () => THREE.Group,
) {
  const current = ref.current;
  if (current?.key === key) {
    return;
  }

  if (current) {
    scene.remove(current.group);
    disposeDynamicGrid(current);
  }

  const group = buildGroup();
  scene.add(group);
  ref.current = { key, group };
}

function clearDynamicGrid(
  scene: THREE.Scene,
  ref: MutableRef<DynamicGridRef | null>,
) {
  const current = ref.current;
  if (!current) {
    return;
  }
  scene.remove(current.group);
  disposeDynamicGrid(current);
  ref.current = null;
}
