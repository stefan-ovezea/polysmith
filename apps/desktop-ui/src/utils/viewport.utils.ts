import * as THREE from "three";

export {
  buildReferenceAxisObject,
  buildReferenceHelixObject,
  buildReferencePlaneObject,
  buildReferencePointObject,
} from "./viewport/referenceObjects";
export {
  buildCutPreviewObject,
  buildPrimitiveObject,
  buildSceneEdgeObject,
  buildSceneVertexObject,
  buildSolidFaceObject,
} from "./viewport/primitiveObjects";
export {
  buildSketchArcObject,
  buildSketchCircleObject,
  buildSketchConstraintObject,
  buildSketchDimensionObject,
  buildSketchEllipseObject,
  buildSketchLineObject,
  buildSketchPointObject,
  buildSketchPolygonObject,
  buildSketchProfileObject,
} from "./viewport/sketchObjects";
export { themeColor } from "./viewport/themeColor";
export {
  applyEdgeVisualColor,
  applyPrimitiveVisualState,
  applyReferencePlaneVisualState,
  applySketchProfileVisualState,
  applySolidFaceVisualState,
  applyVertexVisualColor,
} from "./viewport/visualState";
export {
  SKETCH_PLANE_OFFSET,
  SKETCH_SNAP_DISTANCE,
  SKETCH_SNAP_DISTANCE_PX,
  axisAlignedRectangleCorners2d,
  circleFromThreePoints2d,
  distanceBetweenPoints,
  frameCamera,
  frameCameraToSketchPlane,
  lineCircleIntersectionTrim,
  lineLineIntersectionTrim,
  polygonArea2d,
  projectWorldPointToViewport,
  rectangleFromThreePoints2d,
  resolveSketchPlanePoint,
  signedPolygonArea2d,
  toWorldPoint,
} from "./viewport/viewportMath";

export function disposeMaterial(material: THREE.Material | THREE.Material[]) {
  if (Array.isArray(material)) {
    for (const entry of material) {
      entry.dispose();
    }
    return;
  }

  material.dispose();
}

export function disposeGroup(group: THREE.Group) {
  for (const child of [...group.children]) {
    group.remove(child);

    if (child instanceof THREE.Group) {
      disposeGroup(child);
      continue;
    }

    if (
      child instanceof THREE.Mesh ||
      child instanceof THREE.LineSegments ||
      child instanceof THREE.Line ||
      child instanceof THREE.Sprite
    ) {
      child.geometry.dispose();
      if (
        child instanceof THREE.Sprite &&
        child.material instanceof THREE.SpriteMaterial &&
        child.material.map
      ) {
        child.material.map.dispose();
      }
      disposeMaterial(child.material);
    }
  }
}
