import {
  SceneVertex,
  CutPreviewScene,
  SceneEdge,
  ScenePrimitive,
  SolidFaceScene,
  SolidFaceVisual,
} from "@/types";
import * as THREE from "three";

import { themeColor } from "./themeColor";
import { applyEdgeVisualColor, applyVertexVisualColor } from "./visualState";

// ── View settings (toggled via View panel) ────────────────────────────
let _showHiddenEdges = false;
/** When true, body edges ignore the depth buffer and render through solids. */
export function setShowHiddenEdges(show: boolean) {
  _showHiddenEdges = show;
}
export function getShowHiddenEdges() {
  return _showHiddenEdges;
}

export function shapeFromProfileLoops(
  outerLoop: readonly (readonly [number, number])[],
  innerLoops: readonly (readonly (readonly [number, number])[])[],
) {
  const shape = new THREE.Shape();
  outerLoop.forEach((point, index) => {
    if (index === 0) {
      shape.moveTo(point[0], point[1]);
      return;
    }
    shape.lineTo(point[0], point[1]);
  });
  shape.closePath();

  for (const loop of innerLoops) {
    const path = new THREE.Path();
    [...loop].reverse().forEach((point, index) => {
      if (index === 0) {
        path.moveTo(point[0], point[1]);
        return;
      }
      path.lineTo(point[0], point[1]);
    });
    path.closePath();
    shape.holes.push(path);
  }

  return shape;
}

export function makePlaneTransformMatrix(planeId: string, offset = 0) {
  if (planeId === "ref-plane-xy") {
    return new THREE.Matrix4().set(
      1,
      0,
      0,
      0,
      0,
      1,
      0,
      0,
      0,
      0,
      1,
      offset,
      0,
      0,
      0,
      1,
    );
  }

  if (planeId === "ref-plane-yz") {
    return new THREE.Matrix4().set(
      0,
      0,
      1,
      offset,
      1,
      0,
      0,
      0,
      0,
      1,
      0,
      0,
      0,
      0,
      0,
      1,
    );
  }

  return new THREE.Matrix4().set(
    1,
    0,
    0,
    0,
    0,
    0,
    1,
    offset,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    1,
  );
}

function planeNormalForId(planeId: string): THREE.Vector3 {
  if (planeId === "ref-plane-xy") return new THREE.Vector3(0, 0, 1);
  if (planeId === "ref-plane-yz") return new THREE.Vector3(1, 0, 0);
  if (planeId === "ref-plane-xz") return new THREE.Vector3(0, 1, 0);
  return new THREE.Vector3(0, 1, 0); // default: Y-up (no rotation)
}

export function makePlaneTransformMatrixFromFrame(
  planeFrame: {
    origin: [number, number, number] | { x: number; y: number; z: number };
    xAxis?: [number, number, number] | { x: number; y: number; z: number };
    yAxis?: [number, number, number] | { x: number; y: number; z: number };
    x_axis?: { x: number; y: number; z: number };
    y_axis?: { x: number; y: number; z: number };
    normal: [number, number, number] | { x: number; y: number; z: number };
  },
  offset = 0,
) {
  const origin = Array.isArray(planeFrame.origin)
    ? {
        x: planeFrame.origin[0],
        y: planeFrame.origin[1],
        z: planeFrame.origin[2],
      }
    : planeFrame.origin;
  const xAxis = planeFrame.x_axis
    ? planeFrame.x_axis
    : Array.isArray(planeFrame.xAxis)
      ? {
          x: planeFrame.xAxis[0],
          y: planeFrame.xAxis[1],
          z: planeFrame.xAxis[2],
        }
      : planeFrame.xAxis;
  const yAxis = planeFrame.y_axis
    ? planeFrame.y_axis
    : Array.isArray(planeFrame.yAxis)
      ? {
          x: planeFrame.yAxis[0],
          y: planeFrame.yAxis[1],
          z: planeFrame.yAxis[2],
        }
      : planeFrame.yAxis;
  const normal = Array.isArray(planeFrame.normal)
    ? {
        x: planeFrame.normal[0],
        y: planeFrame.normal[1],
        z: planeFrame.normal[2],
      }
    : planeFrame.normal;

  return new THREE.Matrix4().set(
    xAxis!.x,
    yAxis!.x,
    normal!.x,
    origin!.x + normal!.x * offset,
    xAxis!.y,
    yAxis!.y,
    normal!.y,
    origin!.y + normal!.y * offset,
    xAxis!.z,
    yAxis!.z,
    normal!.z,
    origin!.z + normal!.z * offset,
    0,
    0,
    0,
    1,
  );
}

// Visual state for body primitives. Bodies render as solid CAD-
// style gray (opaque, no transparency) so the model reads like a real
// CAD surface. Hover lifts the body slightly toward white; selection
// is signaled mostly via the body's own edges (see
// `applyEdgeVisualColor` below) plus a small whitening of the face
// fill so highlighted geometry never disappears against the gray.
export function buildPrimitiveObject(primitive: ScenePrimitive) {
  // Solid contextual modeling gray at construction time. Previous defaults
  // were `transparent: true, opacity: 0.72` (the cyan look) and were
  // only flipped to opaque when `applyPrimitiveVisualState` ran from
  // hover / selection - which meant freshly-built bodies always
  // looked translucent on first render until the user interacted.
  const baseMaterial = new THREE.MeshStandardMaterial({
    color: primitive.appearanceColor ?? themeColor("--color-cad-body", "#b8b8b8"),
    emissive: themeColor("--color-cad-body-emissive", "#1a1a1c"),
    emissiveIntensity: 0.05,
    metalness: 0.1,
    roughness: 0.55,
    transparent: false,
    opacity: 1,
    // DoubleSide so interior walls are visible when the camera
    // is inside a cut or looking through open faces.
    side: THREE.DoubleSide,
  });
  const edgeMaterial = new THREE.LineBasicMaterial({
    color: themeColor("--color-cad-edge", "#2a2a2c"),
    transparent: true,
    opacity: 0.9,
    depthTest: true,
    polygonOffset: true,
    polygonOffsetFactor: -1,
    polygonOffsetUnits: -1,
  });

  let geometry: THREE.BufferGeometry;

  if (primitive.kind === "box") {
    geometry = new THREE.BoxGeometry(...primitive.size);
  } else if (primitive.kind === "cylinder") {
    geometry = new THREE.CylinderGeometry(
      primitive.radius,
      primitive.radius,
      primitive.height,
      48,
      48,
    );
    // THREE.CylinderGeometry builds along the Y axis.  In CAD Z-up
    // convention the cylinder axis should be the plane normal, so we
    // rotate the geometry so that its local Y aligns with the plane
    // normal direction in world space.
    const normal = primitive.planeFrame
      ? new THREE.Vector3(
          primitive.planeFrame.normal[0],
          primitive.planeFrame.normal[1],
          primitive.planeFrame.normal[2],
        ).normalize()
      : planeNormalForId(primitive.planeId);
    if (normal.lengthSq() > 0.001 && Math.abs(normal.y - 1.0) > 0.001) {
      const cylinderAxis = new THREE.Vector3(0, 1, 0);
      const rotationMatrix = new THREE.Matrix4().makeRotationFromQuaternion(
        new THREE.Quaternion().setFromUnitVectors(cylinderAxis, normal),
      );
      geometry.applyMatrix4(rotationMatrix);
    }
  } else if (primitive.kind === "polygon_extrude") {
    const shape = shapeFromProfileLoops(
      primitive.profilePoints,
      primitive.innerLoops,
    );

    geometry = new THREE.ExtrudeGeometry(shape, {
      depth: primitive.depth,
      bevelEnabled: false,
      curveSegments: 1,
    });
    geometry.applyMatrix4(
      primitive.planeFrame
        ? makePlaneTransformMatrixFromFrame(primitive.planeFrame)
        : makePlaneTransformMatrix(primitive.planeId),
    );
  } else {
    // Boolean'd body tessellated by the native core. Vertices are already
    // in world space, so no extra transform is needed.
    const meshGeometry = new THREE.BufferGeometry();
    meshGeometry.setAttribute(
      "position",
      new THREE.BufferAttribute(primitive.positions, 3),
    );
    meshGeometry.setIndex(new THREE.BufferAttribute(primitive.indices, 1));
    meshGeometry.computeVertexNormals();
    geometry = meshGeometry;
  }

  const mesh = new THREE.Mesh(geometry, baseMaterial);
  if (primitive.kind === "box" || primitive.kind === "cylinder") {
    mesh.position.set(...primitive.position);
  }
  mesh.userData.primitiveId = primitive.primitiveId;

  const edgeGeometry =
    primitive.kind === "mesh" || primitive.kind === "cylinder"
      ? new THREE.BufferGeometry()
      : new THREE.EdgesGeometry(geometry);
  const edges = new THREE.LineSegments(edgeGeometry, edgeMaterial);
  if (primitive.kind === "box" || primitive.kind === "cylinder") {
    edges.position.copy(mesh.position);
  }

  return {
    mesh,
    edges,
    visual: {
      baseMaterial,
      edgeMaterial,
      appearanceColor: primitive.appearanceColor,
    },
  };
}

// Build a pickable polyline for a body edge. The line carries the edge id
// in `userData.edgeId` for the raycaster, and `renderOrder = 1` plus
// `depthTest = false` keep the highlight readable on top of the body's
// face fills (which sit at `renderOrder = 0`). `userData.isSelected`
// is stashed so the viewport panel's hover handler can recompute the
// material color without re-reading the document state.
export function buildSceneEdgeObject(edge: SceneEdge): THREE.Line {
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute("position", new THREE.BufferAttribute(edge.points, 3));

  // When showHiddenEdges is ON, depthTest is disabled so edges on the
  // far side of the body render through the solid (wireframe overlay).
  // Default is OFF: edges behind the body are properly occluded.
  const depthTest = !_showHiddenEdges;
  const material = new THREE.LineBasicMaterial({
    transparent: true,
    linewidth: 1, // most browsers ignore this; selection still reads via color
    depthTest,
    // `polygonOffset` plus negative offsets keep the line visually on
    // top of the face fill at the same depth (otherwise edges z-fight
    // with the surface they sit on). Only meaningful when depthTest is on.
    polygonOffset: depthTest,
    polygonOffsetFactor: -1,
    polygonOffsetUnits: -1,
  });
  applyEdgeVisualColor(material, {
    isSelected: edge.isSelected,
    isHovered: false,
    isGhost: edge.isGhost,
  });

  const line = new THREE.Line(geometry, material);
  line.userData.edgeId = edge.edgeId;
  line.userData.isSelected = edge.isSelected;
  // Stashed so the panel's hover/Tab-toggle handlers can recompute
  // the visual without rebuilding the scene.
  line.userData.isGhost = edge.isGhost;
  line.renderOrder = 1;
  return line;
}

// Build the translucent red overlay mesh for a cut preview. The overlay
// is non-pickable (`raycast = no-op`) so the user keeps picking the
// underlying booleaned body's faces and edges, not this preview.
const vertexGeometry = new THREE.SphereGeometry(1, 8, 6);

export function buildSceneVertexObject(vertex: SceneVertex): THREE.Mesh {
  const material = new THREE.MeshBasicMaterial({
    transparent: true,
    depthTest: false,
    polygonOffset: true,
    polygonOffsetFactor: -1,
    polygonOffsetUnits: -2,
  });
  applyVertexVisualColor(material, {
    isSelected: vertex.isSelected,
    isHovered: false,
  });

  const mesh = new THREE.Mesh(vertexGeometry, material);
  mesh.position.set(vertex.position[0], vertex.position[1], vertex.position[2]);
  mesh.userData.vertexId = vertex.vertexId;
  mesh.userData.isSelected = vertex.isSelected;
  mesh.renderOrder = 1;
  mesh.scale.setScalar(0.25);
  return mesh;
}

export function buildCutPreviewObject(preview: CutPreviewScene): THREE.Mesh {
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute(
    "position",
    new THREE.BufferAttribute(preview.positions, 3),
  );
  if (preview.normals.length === preview.positions.length) {
    geometry.setAttribute(
      "normal",
      new THREE.BufferAttribute(preview.normals, 3),
    );
  }
  geometry.setIndex(new THREE.BufferAttribute(preview.indices, 1));
  // Solid red translucent so the user reads it as "this volume is being
  // removed". We render with depthWrite off and a higher renderOrder so
  // the overlay always reads through other geometry, matching common CAD workflow's
  // preview behavior.
  const material = new THREE.MeshBasicMaterial({
    color: 0xff3344,
    transparent: true,
    opacity: 0.35,
    side: THREE.DoubleSide,
    depthWrite: false,
  });
  const mesh = new THREE.Mesh(geometry, material);
  mesh.renderOrder = 5;
  // Preview is purely visual - never participate in raycasting.
  mesh.raycast = () => {};
  mesh.userData.cutPreviewId = preview.id;
  return mesh;
}

export function buildSolidFaceObject(face: SolidFaceScene) {
  const fillMaterial = new THREE.MeshBasicMaterial({
    color: face.appearanceColor ?? themeColor("--color-primary-fixed-dim", "#00daf3"),
    transparent: true,
    opacity: face.appearanceColor ? 1 : 0,
    side: THREE.DoubleSide,
    depthWrite: false,
    polygonOffset: true,
    polygonOffsetFactor: -2,
    polygonOffsetUnits: -2,
  });

  // Body-derived faces ship a real triangulation in world space -
  // build a BufferGeometry directly so picking and visuals match the
  // actual face shape (booleaned, filleted, plane-frame-rotated, etc.).
  // Legacy analytical faces (no triangulation) fall back to the old
  // PlaneGeometry transformed into the face's plane frame.
  let geometry: THREE.BufferGeometry;
  let appliesPlaneTransform: boolean;
  if (face.trianglePositions.length > 0 && face.triangleIndices.length > 0) {
    geometry = new THREE.BufferGeometry();
    geometry.setAttribute(
      "position",
      new THREE.BufferAttribute(face.trianglePositions, 3),
    );
    geometry.setIndex(new THREE.BufferAttribute(face.triangleIndices, 1));
    appliesPlaneTransform = false;
  } else {
    geometry = new THREE.PlaneGeometry(
      Math.max(face.size.width || face.size.radius * 2 || 1, 1),
      Math.max(face.size.height || face.size.radius * 2 || 1, 1),
    );
    appliesPlaneTransform = true;
  }

  const mesh = new THREE.Mesh(geometry, fillMaterial);
  if (appliesPlaneTransform) {
    mesh.applyMatrix4(makePlaneTransformMatrixFromFrame(face.planeFrame));
  }
  mesh.userData.faceId = face.faceId;
  mesh.userData.ownerKind = face.ownerKind;
  mesh.renderOrder = 4;
  return {
    mesh,
    visual: {
      fillMaterial,
      appearanceColor: face.appearanceColor,
    } satisfies SolidFaceVisual,
  };
}
