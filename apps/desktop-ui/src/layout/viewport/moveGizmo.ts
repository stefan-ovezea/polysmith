import * as THREE from "three";

import type { MoveFeatureParameters } from "@/types";
import { projectWorldPointToViewport, themeColor } from "@/utils";
import { setPointerNdcFromEvent } from "@/utils/viewport/viewportMath";

export interface MoveGizmoDescriptor {
  bodyId: string;
  center: { x: number; y: number; z: number };
  size: { x: number; y: number; z: number };
  localFrame: {
    x_axis: { x: number; y: number; z: number };
    y_axis: { x: number; y: number; z: number };
    z_axis: { x: number; y: number; z: number };
  };
  parameters: MoveFeatureParameters;
  disabled: boolean;
}

export type MoveGizmoAxis = "x" | "y" | "z";

export type MoveGizmoDragState = {
  kind: "translate" | "rotate" | "free";
  axis: MoveGizmoAxis | null;
  startClientX: number;
  startClientY: number;
  startAngle: number;
  center: THREE.Vector3;
  axes: Record<MoveGizmoAxis, THREE.Vector3>;
  handleLength: number;
  parameters: MoveFeatureParameters;
};

type MoveGizmoHandle =
  | { kind: "translate"; axis: MoveGizmoAxis }
  | { kind: "rotate"; axis: MoveGizmoAxis }
  | { kind: "free" };

type MutableRef<T> = {
  current: T;
};

function moveGizmoAxisVector(
  axis: { x: number; y: number; z: number },
): THREE.Vector3 {
  const vector = new THREE.Vector3(axis.x, axis.y, axis.z);
  return vector.lengthSq() > 1.0e-12
    ? vector.normalize()
    : new THREE.Vector3(1, 0, 0);
}

export function moveGizmoAxes(
  gizmo: MoveGizmoDescriptor,
): Record<MoveGizmoAxis, THREE.Vector3> {
  return {
    x: moveGizmoAxisVector(gizmo.localFrame.x_axis),
    y: moveGizmoAxisVector(gizmo.localFrame.y_axis),
    z: moveGizmoAxisVector(gizmo.localFrame.z_axis),
  };
}

function orientObjectAlongAxis(object: THREE.Object3D, axis: THREE.Vector3) {
  object.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), axis);
}

function orientRingToAxis(object: THREE.Object3D, axis: THREE.Vector3) {
  object.quaternion.setFromUnitVectors(new THREE.Vector3(0, 0, 1), axis);
}

export function buildMoveGizmoObject(gizmo: MoveGizmoDescriptor) {
  const group = new THREE.Group();
  const pickables: THREE.Object3D[] = [];
  const center = new THREE.Vector3(
    gizmo.center.x,
    gizmo.center.y,
    gizmo.center.z,
  );
  const axes = moveGizmoAxes(gizmo);
  const maxSize = Math.max(gizmo.size.x, gizmo.size.y, gizmo.size.z, 12);
  const handleLength = Math.min(Math.max(maxSize * 0.65, 18), 80);
  const ringRadius = handleLength * 0.55;
  const axisColors: Record<MoveGizmoAxis, string> = {
    x: themeColor("--color-axis-x", "#ff6b7a"),
    y: themeColor("--color-axis-y", "#2bd978"),
    z: themeColor("--color-axis-z", "#6db4ff"),
  };
  const handleRadius = Math.max(handleLength * 0.018, 0.28);

  const freeMaterial = new THREE.MeshBasicMaterial({
    color: themeColor("--color-primary-glow", "#00e5ff"),
    transparent: true,
    opacity: 0.9,
    depthTest: false,
    depthWrite: false,
  });
  const centerHandle = new THREE.Mesh(
    new THREE.SphereGeometry(Math.max(handleRadius * 3.2, 2.2), 20, 12),
    freeMaterial,
  );
  centerHandle.position.copy(center);
  centerHandle.renderOrder = 50;
  centerHandle.userData.moveGizmoHandle = { kind: "free" };
  group.add(centerHandle);
  pickables.push(centerHandle);

  (["x", "y", "z"] as const).forEach((axisKey) => {
    const axis = axes[axisKey];
    const color = axisColors[axisKey];
    const axisMaterial = new THREE.MeshBasicMaterial({
      color,
      transparent: true,
      opacity: 0.92,
      depthTest: false,
      depthWrite: false,
    });
    const shaft = new THREE.Mesh(
      new THREE.CylinderGeometry(handleRadius, handleRadius, handleLength, 12),
      axisMaterial,
    );
    orientObjectAlongAxis(shaft, axis);
    shaft.position.copy(center).addScaledVector(axis, handleLength * 0.5);
    shaft.renderOrder = 50;
    shaft.userData.moveGizmoHandle = { kind: "translate", axis: axisKey };
    group.add(shaft);
    pickables.push(shaft);

    const arrow = new THREE.Mesh(
      new THREE.ConeGeometry(handleRadius * 3.4, handleRadius * 9, 20),
      axisMaterial,
    );
    orientObjectAlongAxis(arrow, axis);
    arrow.position
      .copy(center)
      .addScaledVector(axis, handleLength + handleRadius * 4.5);
    arrow.renderOrder = 50;
    arrow.userData.moveGizmoHandle = { kind: "translate", axis: axisKey };
    group.add(arrow);
    pickables.push(arrow);

    const ringMaterial = new THREE.MeshBasicMaterial({
      color,
      transparent: true,
      opacity: 0.72,
      depthTest: false,
      depthWrite: false,
    });
    const ring = new THREE.Mesh(
      new THREE.TorusGeometry(
        ringRadius,
        Math.max(handleRadius * 0.7, 0.18),
        8,
        72,
      ),
      ringMaterial,
    );
    orientRingToAxis(ring, axis);
    ring.position.copy(center);
    ring.renderOrder = 49;
    ring.userData.moveGizmoHandle = { kind: "rotate", axis: axisKey };
    group.add(ring);
    pickables.push(ring);
  });

  return { group, pickables, handleLength };
}

export function moveGizmoScreenAngle(
  event: PointerEvent,
  center: THREE.Vector3,
  camera: THREE.Camera,
  renderer: THREE.WebGLRenderer,
) {
  const projectedCenter = projectWorldPointToViewport(
    [center.x, center.y, center.z],
    camera,
    renderer,
  );
  if (!projectedCenter) {
    return 0;
  }
  const rect = renderer.domElement.getBoundingClientRect();
  return Math.atan2(
    event.clientY - rect.top - projectedCenter.y,
    event.clientX - rect.left - projectedCenter.x,
  );
}

export function beginMoveGizmoPointerDown({
  event,
  renderer,
  camera,
  controls,
  raycaster,
  pointer,
  moveGizmo,
  moveGizmoObjects,
  moveGizmoDragRef,
}: {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  controls: { enabled: boolean };
  raycaster: THREE.Raycaster;
  pointer: THREE.Vector2;
  moveGizmo: MoveGizmoDescriptor | null;
  moveGizmoObjects: THREE.Object3D[];
  moveGizmoDragRef: MutableRef<MoveGizmoDragState | null>;
}) {
  if (!moveGizmo || moveGizmo.disabled || moveGizmoObjects.length === 0) {
    return false;
  }

  setPointerNdcFromEvent(pointer, event, renderer);
  raycaster.setFromCamera(pointer, camera);
  const [gizmoHit] = raycaster.intersectObjects(moveGizmoObjects, false);
  const handle = gizmoHit?.object.userData.moveGizmoHandle as
    | MoveGizmoHandle
    | undefined;
  if (!handle) {
    return false;
  }

  const center = new THREE.Vector3(
    moveGizmo.center.x,
    moveGizmo.center.y,
    moveGizmo.center.z,
  );
  moveGizmoDragRef.current = {
    kind: handle.kind,
    axis: handle.kind === "free" ? null : handle.axis,
    startClientX: event.clientX,
    startClientY: event.clientY,
    startAngle:
      handle.kind === "rotate"
        ? moveGizmoScreenAngle(event, center, camera, renderer)
        : 0,
    center,
    axes: moveGizmoAxes(moveGizmo),
    handleLength: Math.min(
      Math.max(
        Math.max(moveGizmo.size.x, moveGizmo.size.y, moveGizmo.size.z, 12) *
          0.65,
        18,
      ),
      80,
    ),
    parameters: moveGizmo.parameters,
  };
  controls.enabled = false;
  renderer.domElement.setPointerCapture(event.pointerId);
  (renderer.domElement as HTMLCanvasElement).style.cursor = "grabbing";
  return true;
}

export function finishMoveGizmoPointerUp({
  renderer,
  controls,
  moveGizmoDragRef,
}: {
  renderer: THREE.WebGLRenderer;
  controls: { enabled: boolean };
  moveGizmoDragRef: MutableRef<MoveGizmoDragState | null>;
}) {
  if (!moveGizmoDragRef.current) {
    return false;
  }
  moveGizmoDragRef.current = null;
  controls.enabled = true;
  (renderer.domElement as HTMLCanvasElement).style.cursor = "";
  return true;
}

export function moveGizmoTranslationDelta(
  event: PointerEvent,
  drag: MoveGizmoDragState,
  axis: MoveGizmoAxis,
  camera: THREE.Camera,
  renderer: THREE.WebGLRenderer,
) {
  const center = projectWorldPointToViewport(
    [drag.center.x, drag.center.y, drag.center.z],
    camera,
    renderer,
  );
  const endpoint = drag.center
    .clone()
    .addScaledVector(drag.axes[axis], drag.handleLength);
  const projectedEndpoint = projectWorldPointToViewport(
    [endpoint.x, endpoint.y, endpoint.z],
    camera,
    renderer,
  );
  if (!center || !projectedEndpoint) {
    return 0;
  }
  const axisScreen = {
    x: projectedEndpoint.x - center.x,
    y: projectedEndpoint.y - center.y,
  };
  const axisScreenLength = Math.hypot(axisScreen.x, axisScreen.y);
  if (axisScreenLength <= 1.0e-6) {
    return 0;
  }
  const dragScreen = {
    x: event.clientX - drag.startClientX,
    y: event.clientY - drag.startClientY,
  };
  const projectedPixels =
    (dragScreen.x * axisScreen.x + dragScreen.y * axisScreen.y) /
    axisScreenLength;
  return (projectedPixels / axisScreenLength) * drag.handleLength;
}

export function moveGizmoFreeDelta(
  event: PointerEvent,
  drag: MoveGizmoDragState,
  camera: THREE.Camera,
  renderer: THREE.WebGLRenderer,
) {
  const dx = event.clientX - drag.startClientX;
  const dy = event.clientY - drag.startClientY;
  const cameraDirection = new THREE.Vector3();
  camera.getWorldDirection(cameraDirection);
  const cameraRight = new THREE.Vector3()
    .crossVectors(cameraDirection, camera.up)
    .normalize();
  const cameraUp = camera.up.clone().normalize();
  const worldUnitsPerPixel =
    camera instanceof THREE.OrthographicCamera
      ? (camera.top - camera.bottom) /
        camera.zoom /
        renderer.domElement.clientHeight
      : drag.handleLength / Math.max(renderer.domElement.clientHeight, 1);
  return new THREE.Vector3()
    .addScaledVector(cameraRight, dx * worldUnitsPerPixel)
    .addScaledVector(cameraUp, -dy * worldUnitsPerPixel);
}

export function moveGizmoParametersFromDrag(
  event: PointerEvent,
  drag: MoveGizmoDragState,
  camera: THREE.Camera,
  renderer: THREE.WebGLRenderer,
): MoveFeatureParameters {
  const next: MoveFeatureParameters = { ...drag.parameters };

  if (drag.kind === "translate" && drag.axis) {
    const delta = moveGizmoTranslationDelta(
      event,
      drag,
      drag.axis,
      camera,
      renderer,
    );
    if (drag.axis === "x") {
      next.translation_x = drag.parameters.translation_x + delta;
    } else if (drag.axis === "y") {
      next.translation_y = drag.parameters.translation_y + delta;
    } else {
      next.translation_z = drag.parameters.translation_z + delta;
    }
    return next;
  }

  if (drag.kind === "free") {
    const worldDelta = moveGizmoFreeDelta(event, drag, camera, renderer);
    next.translation_x =
      drag.parameters.translation_x + worldDelta.dot(drag.axes.x);
    next.translation_y =
      drag.parameters.translation_y + worldDelta.dot(drag.axes.y);
    next.translation_z =
      drag.parameters.translation_z + worldDelta.dot(drag.axes.z);
    return next;
  }

  if (!drag.axis) {
    return next;
  }

  const angle = moveGizmoScreenAngle(event, drag.center, camera, renderer);
  let deltaDegrees = ((angle - drag.startAngle) * 180) / Math.PI;
  if (deltaDegrees > 180) {
    deltaDegrees -= 360;
  } else if (deltaDegrees < -180) {
    deltaDegrees += 360;
  }

  if (drag.axis === "x") {
    next.rotation_x_degrees =
      drag.parameters.rotation_x_degrees + deltaDegrees;
  } else if (drag.axis === "y") {
    next.rotation_y_degrees =
      drag.parameters.rotation_y_degrees + deltaDegrees;
  } else {
    next.rotation_z_degrees =
      drag.parameters.rotation_z_degrees + deltaDegrees;
  }

  return next;
}
