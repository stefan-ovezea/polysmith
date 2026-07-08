import * as THREE from "three";

import type { ViewportScene } from "@/types";
import type { ActiveSketchGridPlaneFrame } from "./grid";

function sketchCirclePlaneAxes(
  frame: ActiveSketchGridPlaneFrame | null,
): {
  xAxis: [number, number, number];
  yAxis: [number, number, number];
} {
  if (!frame) {
    return {
      xAxis: [1, 0, 0],
      yAxis: [0, 0, 1],
    };
  }

  return {
    xAxis: [frame.x_axis.x, frame.x_axis.y, frame.x_axis.z],
    yAxis: [frame.y_axis.x, frame.y_axis.y, frame.y_axis.z],
  };
}

function updateSketchLineObject(
  lineObject: THREE.Line | THREE.LineLoop | undefined,
  start: readonly [number, number, number],
  end: readonly [number, number, number],
) {
  if (
    !(lineObject instanceof THREE.Line) ||
    !lineObject.geometry.attributes.position
  ) {
    return;
  }

  const position = lineObject.geometry.attributes.position.array as Float32Array;
  position[0] = start[0];
  position[1] = start[1];
  position[2] = start[2];
  position[3] = end[0];
  position[4] = end[1];
  position[5] = end[2];
  lineObject.geometry.attributes.position.needsUpdate = true;
}

function updateSketchCircleObject({
  circleObject,
  center,
  radius,
  xAxis,
  yAxis,
}: {
  circleObject: THREE.Line | THREE.LineLoop | undefined;
  center: readonly [number, number, number];
  radius: number;
  xAxis: readonly [number, number, number];
  yAxis: readonly [number, number, number];
}) {
  if (
    !(circleObject instanceof THREE.LineLoop) ||
    !circleObject.geometry.attributes.position
  ) {
    return;
  }

  const position =
    circleObject.geometry.attributes.position.array as Float32Array;
  const segments = position.length / 3 - 1;
  for (let index = 0; index <= segments; index++) {
    const angle = (index / segments) * Math.PI * 2;
    const localX = Math.cos(angle) * radius;
    const localY = Math.sin(angle) * radius;
    position[index * 3] = center[0] + xAxis[0] * localX + yAxis[0] * localY;
    position[index * 3 + 1] =
      center[1] + xAxis[1] * localX + yAxis[1] * localY;
    position[index * 3 + 2] =
      center[2] + xAxis[2] * localX + yAxis[2] * localY;
  }
  circleObject.geometry.attributes.position.needsUpdate = true;
}

export function updateEndpointDragSceneObjects({
  sceneData,
  planeFrame,
  sketchEntityObjectById,
  sketchPointObjectById,
  sketchConstraintObjects,
}: {
  sceneData: ViewportScene;
  planeFrame: ActiveSketchGridPlaneFrame | null;
  sketchEntityObjectById: ReadonlyMap<string, THREE.Line | THREE.LineLoop>;
  sketchPointObjectById: ReadonlyMap<string, THREE.Mesh>;
  sketchConstraintObjects: readonly THREE.Object3D[];
}) {
  for (const lineData of sceneData.sketchLines) {
    updateSketchLineObject(
      sketchEntityObjectById.get(lineData.lineId),
      lineData.start,
      lineData.end,
    );
  }

  const { xAxis, yAxis } = sketchCirclePlaneAxes(planeFrame);
  for (const circleData of sceneData.sketchCircles) {
    updateSketchCircleObject({
      circleObject: sketchEntityObjectById.get(circleData.circleId),
      center: circleData.center,
      radius: circleData.radius,
      xAxis,
      yAxis,
    });
  }

  for (const pointData of sceneData.sketchPoints) {
    const pointObject = sketchPointObjectById.get(pointData.id);
    if (pointObject) {
      pointObject.position.set(
        pointData.position[0],
        pointData.position[1],
        pointData.position[2],
      );
    }
  }

  for (const constraintData of sceneData.sketchConstraints) {
    const constraintObject = sketchConstraintObjects.find(
      (object) =>
        object.userData.sketchConstraintId === constraintData.constraintId,
    );
    if (constraintObject) {
      constraintObject.position.set(
        constraintData.position[0],
        constraintData.position[1],
        constraintData.position[2],
      );
    }
  }
}
