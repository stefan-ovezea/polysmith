import * as THREE from "three";

import { disposeMaterial } from "@/utils";

export function disposeGeometryObjectResources(object: THREE.Object3D) {
  if (
    object instanceof THREE.Mesh ||
    object instanceof THREE.LineSegments ||
    object instanceof THREE.Line
  ) {
    object.geometry.dispose();
    disposeMaterial(object.material);
  }
}

export function disposeGeometryTreeResources(object: THREE.Object3D) {
  object.traverse(disposeGeometryObjectResources);
}
