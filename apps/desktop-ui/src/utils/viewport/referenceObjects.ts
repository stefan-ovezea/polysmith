import * as THREE from "three";

import type {
  ReferenceAxisScene,
  ReferenceHelixScene,
  ReferencePlaneScene,
  ReferencePointScene,
} from "@/types";
import { themeColor } from "./themeColor";

const REFERENCE_PLANE_RENDER_SIZE = 25;
const REFERENCE_PLANE_MARGIN = 5;

function orientPlaneMesh(
  mesh: THREE.Object3D,
  orientation: ReferencePlaneScene["orientation"],
) {
  if (orientation === "xy") {
    mesh.rotation.x = -Math.PI / 2;
    return;
  }

  if (orientation === "yz") {
    mesh.rotation.y = Math.PI / 2;
  }
}

export function buildReferencePlaneObject(plane: ReferencePlaneScene) {
  const fillMaterial = new THREE.MeshBasicMaterial({
    color: themeColor("--color-tertiary-plane-fill", "#fff7c0"),
    transparent: true,
    opacity: 0.24,
    side: THREE.DoubleSide,
    depthWrite: false,
  });
  const edgeMaterial = new THREE.LineBasicMaterial({
    color: themeColor("--color-tertiary-plane-edge", "#ffe784"),
    transparent: true,
    opacity: 0.92,
  });
  const geometry = new THREE.PlaneGeometry(
    REFERENCE_PLANE_RENDER_SIZE,
    REFERENCE_PLANE_RENDER_SIZE,
  );
  const mesh = new THREE.Mesh(geometry, fillMaterial);
  const edges = new THREE.LineSegments(
    new THREE.EdgesGeometry(geometry),
    edgeMaterial,
  );

  if (plane.orientation === "custom" && plane.planeFrame) {
    const frame = plane.planeFrame;
    const matrix = new THREE.Matrix4().set(
      frame.xAxis[0],
      frame.yAxis[0],
      frame.normal[0],
      frame.origin[0],
      frame.xAxis[1],
      frame.yAxis[1],
      frame.normal[1],
      frame.origin[1],
      frame.xAxis[2],
      frame.yAxis[2],
      frame.normal[2],
      frame.origin[2],
      0,
      0,
      0,
      1,
    );
    mesh.applyMatrix4(matrix);
    edges.applyMatrix4(matrix);
  } else {
    orientPlaneMesh(mesh, plane.orientation);
    orientPlaneMesh(edges, plane.orientation);
    const offset = REFERENCE_PLANE_MARGIN + REFERENCE_PLANE_RENDER_SIZE / 2;
    const renderPosition: [number, number, number] =
      plane.orientation === "xy"
        ? [offset, 0, offset]
        : plane.orientation === "yz"
          ? [0, offset, offset]
          : [offset, offset, 0];
    mesh.position.set(...renderPosition);
    edges.position.copy(mesh.position);
  }
  mesh.userData.referenceId = plane.referenceId;

  return {
    mesh,
    edges,
    visual: {
      fillMaterial,
      edgeMaterial,
    },
  };
}

export function buildReferenceAxisObject(axis: ReferenceAxisScene) {
  const color =
    axis.axis === "custom"
      ? themeColor("--color-tertiary-plane-edge", "#ffe784")
      : axis.axis === "x"
        ? themeColor("--color-axis-x", "#ff6b7a")
        : axis.axis === "y"
          ? themeColor("--color-axis-y", "#2bd978")
          : themeColor("--color-axis-z", "#6db4ff");
  const material = new THREE.LineBasicMaterial({
    color,
    transparent: true,
    opacity: 0.95,
  });
  const points = [
    new THREE.Vector3(...axis.start),
    new THREE.Vector3(...axis.end),
  ];
  const geometry = new THREE.BufferGeometry().setFromPoints(points);
  const line = new THREE.Line(geometry, material);

  return { line };
}

export function buildReferencePointObject(point: ReferencePointScene) {
  const geometry = new THREE.SphereGeometry(0.7, 16, 12);
  const material = new THREE.MeshBasicMaterial({
    color: point.isSelected
      ? themeColor("--color-tertiary-plane-edge-hover", "#fff2b2")
      : themeColor("--color-tertiary-plane-edge", "#ffe784"),
    transparent: true,
    opacity: 0.95,
    depthTest: true,
  });
  const mesh = new THREE.Mesh(geometry, material);
  mesh.position.set(...point.position);
  return { mesh };
}

export function buildReferenceHelixObject(helix: ReferenceHelixScene) {
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute("position", new THREE.BufferAttribute(helix.points, 3));
  const material = new THREE.LineBasicMaterial({
    color: helix.isSelected
      ? themeColor("--color-tertiary-plane-edge-hover", "#fff2b2")
      : themeColor("--color-tertiary-plane-edge", "#ffe784"),
    transparent: true,
    opacity: 0.95,
    depthTest: false,
  });
  const line = new THREE.Line(geometry, material);
  line.renderOrder = 4;
  line.userData.referenceId = helix.referenceId;
  return { line };
}
