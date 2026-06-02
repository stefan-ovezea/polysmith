import { reformatDimensionLabel } from "./units";

import {
  PrimitiveInteractionState,
  PrimitiveVisual,
  ReferenceAxisScene,
  ReferenceHelixScene,
  ReferencePlaneInteractionState,
  ReferencePlaneScene,
  ReferencePointScene,
  ReferencePlaneVisual,
  ScenePrimitive,
  SolidFaceInteractionState,
  SolidFaceVisual,
  SketchArcScene,
  SketchCircleScene,
  SketchConstraintScene,
  SketchDimensionScene,
  SketchLineScene,
  SketchPolygonScene,
  SceneEdge,
  SceneVertex,
  CutPreviewScene,
  SketchPlaneFrame,
  SketchPointScene,
  SketchProfileInteractionState,
  SketchProfileScene,
  SketchProfileVisual,
  SolidFaceScene,
} from "@/types";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";

export const SKETCH_PLANE_OFFSET = 0.2;
export const REFERENCE_PLANE_RENDER_SIZE = 25;
export const REFERENCE_PLANE_MARGIN = 5;
export const SKETCH_SNAP_DISTANCE = 2.5;
export const DIMENSION_EDITOR_MARGIN = 20;

export function themeColor(token: string, fallback: string) {
  if (typeof document === "undefined") {
    return fallback;
  }

  const value = getComputedStyle(document.documentElement)
    .getPropertyValue(token)
    .trim();
  return value || fallback;
}

function configureSketchOverlayMaterial(material: THREE.Material) {
  material.depthTest = false;
  material.depthWrite = false;
}

function polygonArea2d(points: Array<[number, number]>) {
  if (points.length < 3) {
    return 0;
  }
  let area = 0;
  for (let index = 0; index < points.length; index += 1) {
    const current = points[index];
    const next = points[(index + 1) % points.length];
    area += current[0] * next[1] - next[0] * current[1];
  }
  return area * 0.5;
}

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

// Visual state for body primitives. Bodies render as solid CAD-
// style gray (opaque, no transparency) so the model reads like a real
// CAD surface. Hover lifts the body slightly toward white; selection
// is signaled mostly via the body's own edges (see
// `applyEdgeVisualColor` below) plus a small whitening of the face
// fill so highlighted geometry never disappears against the gray.
export function applyPrimitiveVisualState(
  visual: PrimitiveVisual,
  state: PrimitiveInteractionState,
) {
  visual.baseMaterial.transparent = false;
  visual.baseMaterial.opacity = 1;
  visual.baseMaterial.emissive.set(
    themeColor("--color-cad-body-emissive", "#1a1a1c"),
  );
  visual.baseMaterial.emissiveIntensity = 0.05;

  const bodyColor =
    visual.appearanceColor ?? themeColor("--color-cad-body", "#b8b8b8");

  if (state.isSelected) {
    visual.baseMaterial.color.set(bodyColor);
    visual.edgeMaterial.color.set(
      themeColor("--color-cad-edge-selected", "#ff9a3c"),
    );
    return;
  }

  if (state.isHovered) {
    visual.baseMaterial.color.set(bodyColor);
    visual.edgeMaterial.color.set(
      themeColor("--color-cad-edge-hover", "#3da9ff"),
    );
    return;
  }

  visual.baseMaterial.color.set(bodyColor);
  visual.edgeMaterial.color.set(themeColor("--color-cad-edge", "#2a2a2c"));
}

// Three-state color helpers used by the viewport panel to recolor
// per-edge / per-vertex materials in response to hover changes
// without rebuilding the scene. Kept here next to the build* helpers
// so the palette decisions live in one file.
export function applyEdgeVisualColor(
  material: THREE.LineBasicMaterial,
  state: {
    isSelected: boolean;
    isHovered: boolean;
    // True when the edge belongs to a body with a pending
    // fillet/chamfer feature. Ghost edges are still pickable but
    // render hidden by default — only the selected ones glow — so
    // the post-op preview reads cleanly.
    isGhost?: boolean;
    // Set by ViewportPanel while the user holds the wireframe-toggle
    // key so the user can re-discover ghost edges to add to the
    // pending feature.
    revealGhost?: boolean;
  },
) {
  if (state.isSelected) {
    material.color.set(themeColor("--color-cad-edge-selected", "#ff9a3c"));
    material.opacity = 1;
    return;
  }
  if (state.isHovered) {
    material.color.set(themeColor("--color-cad-edge-hover", "#3da9ff"));
    material.opacity = 1;
    return;
  }
  if (state.isGhost && !state.revealGhost) {
    // Pickable but visually hidden. We can't set opacity to 0 because
    // some renderers treat fully-transparent materials as a no-draw
    // and skip raycasting; a near-zero value keeps the THREE.Line
    // alive in the pick chain while staying invisible.
    material.color.set(themeColor("--color-cad-edge", "#2a2a2c"));
    material.opacity = 0.001;
    return;
  }
  material.color.set(themeColor("--color-cad-edge", "#2a2a2c"));
  material.opacity = 0.85;
}

export function applyVertexVisualColor(
  material: THREE.MeshBasicMaterial,
  state: { isSelected: boolean; isHovered: boolean },
) {
  if (state.isSelected) {
    material.color.set(themeColor("--color-cad-vertex-selected", "#ff9a3c"));
    material.opacity = 1;
    return;
  }
  if (state.isHovered) {
    material.color.set(themeColor("--color-cad-vertex-hover", "#3da9ff"));
    material.opacity = 1;
    return;
  }
  material.color.set(themeColor("--color-cad-vertex", "#1c1c1e"));
  material.opacity = 0.95;
}

export function applyReferencePlaneVisualState(
  visual: ReferencePlaneVisual,
  state: ReferencePlaneInteractionState,
) {
  if (state.isActiveSketchPlane) {
    visual.fillMaterial.color.set(
      themeColor("--color-tertiary-plane-selected", "#f7e38a"),
    );
    visual.fillMaterial.opacity = 0.38;
    visual.edgeMaterial.color.set(
      themeColor("--color-tertiary-plane-edge-active", "#fff4b8"),
    );
    return;
  }

  if (state.isSelected) {
    visual.fillMaterial.color.set(
      themeColor("--color-tertiary-plane-selected", "#f7e38a"),
    );
    visual.fillMaterial.opacity = 0.34;
    visual.edgeMaterial.color.set(
      themeColor("--color-tertiary-plane-edge-selected", "#ffe99a"),
    );
    return;
  }

  if (state.isHovered) {
    visual.fillMaterial.color.set(
      themeColor("--color-tertiary-plane-hover", "#fff0aa"),
    );
    visual.fillMaterial.opacity = 0.3;
    visual.edgeMaterial.color.set(
      themeColor("--color-tertiary-plane-edge-hover", "#fff2b2"),
    );
    return;
  }

  visual.fillMaterial.color.set(
    themeColor("--color-tertiary-plane-fill", "#fff7c0"),
  );
  visual.fillMaterial.opacity = 0.24;
  visual.edgeMaterial.color.set(
    themeColor("--color-tertiary-plane-edge", "#ffe784"),
  );
}

export function buildPrimitiveObject(primitive: ScenePrimitive) {
  // Solid contextual modeling gray at construction time. Previous defaults
  // were `transparent: true, opacity: 0.72` (the cyan look) and were
  // only flipped to opaque when `applyPrimitiveVisualState` ran from
  // hover / selection — which meant freshly-built bodies always
  // looked translucent on first render until the user interacted.
  const baseMaterial = new THREE.MeshStandardMaterial({
    color: primitive.appearanceColor ?? themeColor("--color-cad-body", "#b8b8b8"),
    emissive: themeColor("--color-cad-body-emissive", "#1a1a1c"),
    emissiveIntensity: 0.05,
    metalness: 0.1,
    roughness: 0.55,
    transparent: false,
    opacity: 1,
    // Render both sides because `makePlaneTransformMatrix` for
    // `ref-plane-xy` has determinant -1 (it swaps Y and Z without
    // negating one), which flips the winding of every triangle in a
    // legacy `polygon_extrude`. Without DoubleSide the front faces
    // get culled and the user sees through to the inside walls — the
    // "first extrude on XY plane looks transparent" bug. The proper
    // fix is to make that matrix a rotation (det +1) and update
    // `toWorldPoint`'s ref-plane-xy branch to match, but every site
    // that pairs the two would need to flip its sign too. DoubleSide
    // costs a few extra fragments and three.js auto-flips normals
    // for back-facing fragments so PBR lighting stays correct.
    side: THREE.DoubleSide,
  });
  const edgeMaterial = new THREE.LineBasicMaterial({
    color: themeColor("--color-cad-edge", "#2a2a2c"),
    transparent: true,
    opacity: 0.9,
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
    );
  } else if (primitive.kind === "polygon_extrude") {
    const shape = new THREE.Shape();
    primitive.profilePoints.forEach((point, index) => {
      if (index === 0) {
        shape.moveTo(point[0], point[1]);
        return;
      }
      shape.lineTo(point[0], point[1]);
    });
    shape.closePath();
    for (const loop of primitive.innerLoops) {
      const path = new THREE.Path();
      // The core stores hole loops as profile contours. Three treats
      // `shape.holes` as subtractive paths, so the preview primitive
      // must carry those loops too; otherwise the temporary extrude
      // appears as the old filled outer profile.
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

export function orientPlaneMesh(
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

export function planeOrientationFromId(
  planeId: string,
): ReferencePlaneScene["orientation"] {
  if (planeId === "ref-plane-xy") {
    return "xy";
  }

  if (planeId === "ref-plane-yz") {
    return "yz";
  }

  return "xz";
}

export function makePlaneTransformMatrix(planeId: string, offset = 0) {
  if (planeId === "ref-plane-xy") {
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
    // Construction plane — orient and position from the cached frame.
    // PlaneGeometry's local axes are (X, Y) with normal +Z. Build a
    // world-space basis matrix that maps those axes to the plane's
    // (xAxis, yAxis, normal) and use the frame's origin as the
    // translation. This works for any orientation, so chained offsets
    // and face-source planes render correctly without any special
    // cases.
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
    // Render the origin helpers from the world origin so the visual margin is
    // measured from the axes, not from the core plane center.
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

// Build a pickable polyline for a body edge. The line carries the edge id
// in `userData.edgeId` for the raycaster, and `renderOrder = 1` plus
// `depthTest = false` keep the highlight readable on top of the body's
// face fills (which sit at `renderOrder = 0`). `userData.isSelected`
// is stashed so the viewport panel's hover handler can recompute the
// material color without re-reading the document state.
export function buildSceneEdgeObject(edge: SceneEdge): THREE.Line {
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute("position", new THREE.BufferAttribute(edge.points, 3));

  const material = new THREE.LineBasicMaterial({
    transparent: true,
    linewidth: 1, // most browsers ignore this; selection still reads via color
    // depthTest stays ON so edges on the far side of an opaque body
    // are occluded by the surface, matching common CAD workflows. With it off, the
    // wireframe shows through the body and reads as transparency.
    // `polygonOffset` plus a small `polygonOffsetUnits` keeps the line
    // visually on top of the face fill at the same depth (otherwise
    // edges z-fight with the surface they sit on).
    depthTest: true,
    polygonOffset: true,
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

// Build a pickable vertex marker. We use a small sphere mesh rather than
// THREE.Points so the pick is consistent across DPI / camera distances —
// a sphere has a real bounding volume the raycaster can hit reliably.
// The marker sits on top of edges/faces via `renderOrder = 2` and
// `depthTest = false` so it's never occluded.
const VERTEX_RADIUS = 0.6;
// Build the translucent red overlay mesh for a cut preview. The overlay
// is non-pickable (`raycast = no-op`) so the user keeps picking the
// underlying booleaned body's faces and edges, not this preview.
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
  // Preview is purely visual — never participate in raycasting.
  mesh.raycast = () => {};
  mesh.userData.cutPreviewId = preview.id;
  return mesh;
}

export function buildSceneVertexObject(vertex: SceneVertex): THREE.Mesh {
  const geometry = new THREE.SphereGeometry(VERTEX_RADIUS, 12, 8);
  const material = new THREE.MeshBasicMaterial({
    transparent: true,
    // Same reasoning as edges: keep depthTest on so back-side vertices
    // hide behind the opaque body fill instead of bleeding through.
    depthTest: true,
  });
  applyVertexVisualColor(material, {
    isSelected: vertex.isSelected,
    isHovered: false,
  });
  const mesh = new THREE.Mesh(geometry, material);
  mesh.position.set(vertex.position[0], vertex.position[1], vertex.position[2]);
  mesh.userData.vertexId = vertex.vertexId;
  mesh.userData.isSelected = vertex.isSelected;
  mesh.renderOrder = 2;
  return mesh;
}

export function orientFaceMesh(mesh: THREE.Object3D, face: SolidFaceScene) {
  if (Math.abs(face.normal[1]) > 0.5) {
    mesh.rotation.x = -Math.PI / 2;
    return;
  }

  if (Math.abs(face.normal[0]) > 0.5) {
    mesh.rotation.y = Math.PI / 2;
  }
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

  // Body-derived faces ship a real triangulation in world space —
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
  mesh.renderOrder = 4;
  return {
    mesh,
    visual: {
      fillMaterial,
      appearanceColor: face.appearanceColor,
    } satisfies SolidFaceVisual,
  };
}

// Face overlays sit on top of the body's opaque fill. Earlier the
// hover branch ramped up to 22% saturated cyan; over a gray body that
// reads as the body itself going translucent (the "first extrude
// looks transparent under the cursor" bug). The values below are
// tuned so the highlight is unmistakably visible without making the
// body feel see-through:
//
//   * Hover: 8% off-white tint. Barely-perceptible "your cursor is
//     over a face" cue. Doesn't compete with the body's gray.
//   * Selected: 24% accent cyan. Still clearly the picked face, but
//     low enough that the body underneath stays solid-looking.
export function applySolidFaceVisualState(
  visual: SolidFaceVisual,
  state: SolidFaceInteractionState,
) {
  if (visual.appearanceColor) {
    visual.fillMaterial.color.set(visual.appearanceColor);
    visual.fillMaterial.opacity = 1;
    return;
  }

  if (state.isSelected) {
    visual.fillMaterial.color.set(
      themeColor("--color-primary-soft", "#c3f5ff"),
    );
    visual.fillMaterial.opacity = 0.24;
    return;
  }

  if (state.isHovered) {
    visual.fillMaterial.color.set(
      themeColor("--cad-face-hover-fill", "#ffffff"),
    );
    visual.fillMaterial.opacity = 0.08;
    return;
  }

  visual.fillMaterial.color.set(themeColor("--cad-face-hover-fill", "#ffffff"));
  visual.fillMaterial.opacity = 0;
}

export function applySketchProfileVisualState(
  visual: SketchProfileVisual,
  state: SketchProfileInteractionState,
) {
  if (state.isHovered || state.isSelected) {
    visual.fillMaterial.color.set(
      state.isSelected
        ? themeColor("--color-primary-soft", "#c3f5ff")
        : themeColor("--color-tertiary-plane-fill", "#fff7c0"),
    );
    visual.fillMaterial.opacity = state.isSelected ? 0.24 : 0.18;
    for (const material of visual.edgeMaterials) {
      material.color.set(
        state.isSelected
          ? themeColor("--color-primary-edge-active", "#c3f5ff")
          : themeColor("--color-tertiary-plane-edge-hover", "#fff2b2"),
      );
      material.opacity = 0.98;
      material.linewidth = state.isSelected ? 3 : 2.5;
    }
    return;
  }

  visual.fillMaterial.opacity = 0;
  for (const material of visual.edgeMaterials) {
    material.opacity = 0;
    material.linewidth = 1;
  }
}

export function buildSketchLineObject(line: SketchLineScene) {
  // Tool-generated preview lines (e.g. Mirror's reflected
  // entities) render dashed and translucent so they read as
  // "about to exist" rather than committed geometry. They share
  // the dashed material path with construction lines, just at
  // lower opacity.
  const isDashed = line.isConstruction || line.isPreview;
  const baseColor = line.isProjected
    ? themeColor("--cad-sketch-projected", "#ff4fd8")
    : themeColor("--color-tertiary-plane-fill", "#fff7c0");
  const material = isDashed
    ? new THREE.LineDashedMaterial({
        color: line.isSelected
          ? themeColor("--color-primary-edge-active", "#c3f5ff")
          : baseColor,
        transparent: true,
        opacity: line.isPreview ? 0.55 : 0.85,
        linewidth: line.isSelected ? 2 : 1,
        dashSize: 1,
        gapSize: 0.6,
      })
    : new THREE.LineBasicMaterial({
        color: line.isSelected
          ? themeColor("--color-primary-edge-active", "#c3f5ff")
          : baseColor,
        transparent: true,
        opacity: 0.98,
        linewidth: line.isSelected ? 2 : 1,
      });
  configureSketchOverlayMaterial(material);
  const points = [
    new THREE.Vector3(...line.start),
    new THREE.Vector3(...line.end),
  ];
  const geometry = new THREE.BufferGeometry().setFromPoints(points);
  const sketchLine = new THREE.Line(geometry, material);
  sketchLine.renderOrder = 7;
  // `LineDashedMaterial` requires per-vertex distance data to render
  // the dash pattern; without this call the line renders solid.
  if (isDashed) {
    sketchLine.computeLineDistances();
  }
  // Preview entities aren't selectable — leave their userData
  // un-tagged so the raycaster ignores them.
  if (!line.isPreview) {
    sketchLine.userData.sketchEntityId = line.lineId;
    sketchLine.userData.sketchEntityKind = "line";
    sketchLine.userData.sketchEntityIsConstruction = line.isConstruction;
    sketchLine.userData.sketchEntityIsProjected = line.isProjected;
  }
  return sketchLine;
}

export function buildSketchPolygonObject(polygon: SketchPolygonScene) {
  const n = polygon.corners.length / 3;
  if (n < 3) {
    return new THREE.Line();
  }
  const points = new Array<THREE.Vector3>(n + 1);
  for (let i = 0; i < n; i++) {
    points[i] = new THREE.Vector3(
      polygon.corners[i * 3],
      polygon.corners[i * 3 + 1],
      polygon.corners[i * 3 + 2],
    );
  }
  points[n] = points[0]; // close the loop
  const isDashed = polygon.isConstruction;
  const baseColor = themeColor("--color-tertiary-plane-fill", "#fff7c0");
  const material = isDashed
    ? new THREE.LineDashedMaterial({
        color: polygon.isSelected
          ? themeColor("--color-primary-edge-active", "#c3f5ff")
          : baseColor,
        transparent: true,
        opacity: polygon.isPreview ? 0.55 : 0.85,
        dashSize: 1,
        gapSize: 0.6,
      })
    : new THREE.LineBasicMaterial({
        color: polygon.isSelected
          ? themeColor("--color-primary-edge-active", "#c3f5ff")
          : baseColor,
        transparent: true,
        opacity: 0.98,
      });
  configureSketchOverlayMaterial(material);
  const geometry = new THREE.BufferGeometry().setFromPoints(points);
  const sketchPolygon = new THREE.Line(geometry, material);
  sketchPolygon.renderOrder = 7;
  if (isDashed) {
    sketchPolygon.computeLineDistances();
  }
  if (!polygon.isPreview) {
    sketchPolygon.userData.sketchEntityId = polygon.polygonId;
    sketchPolygon.userData.sketchEntityKind = "line";
  }
  return sketchPolygon;
}

// Build the perimeter line for a sketch circle. The center comes
// in world space; the radius is a 2D scalar in the sketch plane, so
// we project each perimeter sample using the plane's x_axis / y_axis.
// `planeFrame` is required for face-based sketches (arbitrary planes);
// when it's null we fall back to the legacy ref-plane axis mapping
// for back-compat with sketches on the three named ref planes.
export function buildSketchCircleObject(
  circle: SketchCircleScene,
  planeFrame: SketchPlaneFrame | null = null,
) {
  // See `buildSketchLineObject` for the rationale on the dashed +
  // translucent treatment of preview circles.
  const isDashed = circle.isPreview || circle.isConstruction;
  const baseColor = circle.isProjected
    ? themeColor("--cad-sketch-projected", "#ff4fd8")
    : themeColor("--color-tertiary-plane-fill", "#fff7c0");
  const material = isDashed
    ? new THREE.LineDashedMaterial({
        color: circle.isSelected
          ? themeColor("--color-primary-edge-active", "#c3f5ff")
          : baseColor,
        transparent: true,
        opacity: circle.isPreview ? 0.55 : 0.72,
        dashSize: 1,
        gapSize: 0.6,
      })
    : new THREE.LineBasicMaterial({
        color: circle.isSelected
          ? themeColor("--color-primary-edge-active", "#c3f5ff")
          : baseColor,
        transparent: true,
        opacity: 0.98,
      });
  configureSketchOverlayMaterial(material);
  const curve = new THREE.EllipseCurve(
    0,
    0,
    circle.radius,
    circle.radius,
    0,
    Math.PI * 2,
    false,
    0,
  );
  // Determine the plane's x and y world-space axes. For sketches with
  // an arbitrary plane (face-based sketches) we must use the frame the
  // core ships; otherwise the perimeter ends up perpendicular to the
  // actual sketch plane (the "preview shows in wrong plane" bug).
  let xAxis: [number, number, number];
  let yAxis: [number, number, number];
  if (planeFrame) {
    xAxis = [planeFrame.x_axis.x, planeFrame.x_axis.y, planeFrame.x_axis.z];
    yAxis = [planeFrame.y_axis.x, planeFrame.y_axis.y, planeFrame.y_axis.z];
  } else if (circle.planeId === "ref-plane-xy") {
    xAxis = [1, 0, 0];
    yAxis = [0, 0, 1];
  } else if (circle.planeId === "ref-plane-yz") {
    xAxis = [0, 1, 0];
    yAxis = [0, 0, 1];
  } else {
    xAxis = [1, 0, 0];
    yAxis = [0, 1, 0];
  }
  const points = curve
    .getPoints(64)
    .map(
      (point) =>
        new THREE.Vector3(
          circle.center[0] + xAxis[0] * point.x + yAxis[0] * point.y,
          circle.center[1] + xAxis[1] * point.x + yAxis[1] * point.y,
          circle.center[2] + xAxis[2] * point.x + yAxis[2] * point.y,
        ),
    );
  const geometry = new THREE.BufferGeometry().setFromPoints(points);
  const sketchCircle = new THREE.LineLoop(geometry, material);
  sketchCircle.renderOrder = 7;
  if (isDashed) {
    // Dashed materials need per-vertex distance; preview circles
    // also stay un-tagged so they're never raycast hits.
    sketchCircle.computeLineDistances();
  }
  if (!circle.isPreview) {
    sketchCircle.userData.sketchEntityId = circle.circleId;
    sketchCircle.userData.sketchEntityKind = "circle";
    sketchCircle.userData.sketchEntityIsConstruction = circle.isConstruction;
    sketchCircle.userData.sketchEntityIsProjected = circle.isProjected;
  }
  return sketchCircle;
}

// Sample a sketch arc into a polyline and emit it as a THREE.Line.
// `planeFrame` carries the sketch plane's world-space basis so the
// sampling stays planar — same pattern as `buildSketchCircleObject`.
// `start`, `end`, and `center` arrive in world space (the core
// already projects them through the plane frame), so we project them
// back into the sketch's local 2D frame, sample around the circle,
// then project each sample back to world.
export function buildSketchArcObject(
  arc: SketchArcScene,
  planeFrame: SketchPlaneFrame | null = null,
) {
  const baseColor = arc.isProjected
    ? themeColor("--cad-sketch-projected", "#ff4fd8")
    : themeColor("--color-tertiary-plane-fill", "#fff7c0");
  const material = arc.isPreview
    ? new THREE.LineDashedMaterial({
        color: arc.isSelected
          ? themeColor("--color-primary-edge-active", "#c3f5ff")
          : baseColor,
        transparent: true,
        opacity: 0.55,
        dashSize: 1,
        gapSize: 0.6,
      })
    : new THREE.LineBasicMaterial({
        color: arc.isSelected
          ? themeColor("--color-primary-edge-active", "#c3f5ff")
          : baseColor,
        transparent: true,
        opacity: 0.98,
      });
  configureSketchOverlayMaterial(material);

  // Resolve the sketch plane's local x / y world axes (same logic as
  // `buildSketchCircleObject`). Arc sampling parameterizes on angle
  // around the center in this local 2D frame.
  let xAxis: [number, number, number];
  let yAxis: [number, number, number];
  if (planeFrame) {
    xAxis = [planeFrame.x_axis.x, planeFrame.x_axis.y, planeFrame.x_axis.z];
    yAxis = [planeFrame.y_axis.x, planeFrame.y_axis.y, planeFrame.y_axis.z];
  } else if (arc.planeId === "ref-plane-xy") {
    xAxis = [1, 0, 0];
    yAxis = [0, 0, 1];
  } else if (arc.planeId === "ref-plane-yz") {
    xAxis = [0, 1, 0];
    yAxis = [0, 0, 1];
  } else {
    xAxis = [1, 0, 0];
    yAxis = [0, 1, 0];
  }

  // Project a world-space point into the (xAxis, yAxis) frame
  // anchored at the arc's center. Used to recover start_angle /
  // end_angle from the world-space endpoints we received.
  const project_local = (p: [number, number, number]): [number, number] => {
    const dx = p[0] - arc.center[0];
    const dy = p[1] - arc.center[1];
    const dz = p[2] - arc.center[2];
    return [
      dx * xAxis[0] + dy * xAxis[1] + dz * xAxis[2],
      dx * yAxis[0] + dy * yAxis[1] + dz * yAxis[2],
    ];
  };

  const [sx, sy] = project_local(arc.start);
  const [ex, ey] = project_local(arc.end);
  const start_angle = Math.atan2(sy, sx);
  const end_angle = Math.atan2(ey, ex);

  // Sweep direction matches the arc's stored `ccw`. Normalize so the
  // sample loop walks from start to end through the correct side of
  // the circle (otherwise a >180° major arc would sample the minor
  // arc instead).
  let sweep = end_angle - start_angle;
  if (arc.ccw) {
    while (sweep <= 0) sweep += Math.PI * 2;
  } else {
    while (sweep >= 0) sweep -= Math.PI * 2;
  }

  // 64 segments matches buildSketchCircleObject's resolution. Smaller
  // arcs naturally end up with fewer "visible" segments because the
  // chord per segment scales with the sweep.
  const segments = 64;
  const points: THREE.Vector3[] = [];
  for (let i = 0; i <= segments; i++) {
    const t = i / segments;
    const angle = start_angle + sweep * t;
    const localX = arc.radius * Math.cos(angle);
    const localY = arc.radius * Math.sin(angle);
    points.push(
      new THREE.Vector3(
        arc.center[0] + xAxis[0] * localX + yAxis[0] * localY,
        arc.center[1] + xAxis[1] * localX + yAxis[1] * localY,
        arc.center[2] + xAxis[2] * localX + yAxis[2] * localY,
      ),
    );
  }

  const geometry = new THREE.BufferGeometry().setFromPoints(points);
  const sketchArc = new THREE.Line(geometry, material);
  sketchArc.renderOrder = 7;
  if (arc.isPreview) {
    sketchArc.computeLineDistances();
  } else {
    sketchArc.userData.sketchEntityId = arc.arcId;
    sketchArc.userData.sketchEntityKind = "arc";
    sketchArc.userData.sketchEntityIsProjected = arc.isProjected;
  }
  return sketchArc;
}

export function buildSketchPointObject(point: SketchPointScene) {
  // Projected points get a slightly larger sphere in a cyan-violet
  // to read as "derived from a body vertex" — matches the CAD
  // visual convention. Endpoint / center keep the original look.
  const radius =
    point.kind === "center" ? 0.9 : point.kind === "projected" ? 0.85 : 0.7;
  const geometry = new THREE.SphereGeometry(radius, 12, 12);
  const material = new THREE.MeshBasicMaterial({
    color: point.isSelected
      ? themeColor("--color-primary-edge-active", "#c3f5ff")
      : point.kind === "center"
        ? themeColor("--color-axis-z", "#6db4ff")
        : point.kind === "projected"
          ? themeColor("--color-axis-z", "#6db4ff")
          : themeColor("--color-tertiary-plane-edge", "#ffe784"),
    transparent: true,
    opacity: 0.95,
    depthTest: false,
    depthWrite: false,
  });
  const mesh = new THREE.Mesh(geometry, material);
  mesh.renderOrder = 8;
  mesh.position.set(...point.position);
  mesh.userData.sketchPointId = point.pointId;
  mesh.userData.sketchPointKind = point.kind;

  // Override raycast to use a sphere-distance test instead of the
  // default triangle-intersection test. The visual sphere radius is
  // ~0.7 mm (a few pixels), which the raycaster often misses because
  // the Line threshold (1.75 mm) gives lines a much wider capture
  // zone.  The pick radius below gives points a competitive hit area
  // so clicking near an endpoint ball actually registers.
  const PICK_RADIUS = 1.5;
  mesh.raycast = (raycaster, intersects) => {
    const sphereCenter = new THREE.Vector3();
    mesh.getWorldPosition(sphereCenter);
    const rayOrigin = raycaster.ray.origin;
    const rayDir = raycaster.ray.direction;

    const toCenter = sphereCenter.clone().sub(rayOrigin);
    const proj = toCenter.dot(rayDir);
    const closest = rayOrigin.clone().addScaledVector(
      rayDir, Math.max(0, proj),
    );
    const distToRay = sphereCenter.distanceTo(closest);

    if (distToRay > PICK_RADIUS) return;

    // Ray hits the virtual sphere.  Compute the intersection point
    // along the ray as the closest point on the ray to the sphere
    // surface.
    const halfChord = Math.sqrt(
      Math.max(0, PICK_RADIUS * PICK_RADIUS - distToRay * distToRay),
    );
    const t = Math.max(0, proj - halfChord);
    const hitPoint = rayOrigin.clone().addScaledVector(rayDir, t);
    const hitDist = hitPoint.distanceTo(rayOrigin);

    intersects.push({
      distance: hitDist,
      point: hitPoint,
      object: mesh,
      face: null,
      faceIndex: undefined,
    } as THREE.Intersection);
  };

  return mesh;
}

export function makeDimensionLabelSprite(
  text: string,
  isSelected: boolean,
  options: { variant?: "muted-preview" } = {},
) {
  const canvas = document.createElement("canvas");
  const context = canvas.getContext("2d");
  if (!context) {
    const texture = new THREE.CanvasTexture(canvas);
    return new THREE.Sprite(
      new THREE.SpriteMaterial({ map: texture, transparent: true }),
    );
  }

  const fontSize = 26;
  context.font = `600 ${fontSize}px "Space Grotesk", sans-serif`;
  const textWidth = Math.ceil(context.measureText(text).width);
  canvas.width = textWidth + 12;
  canvas.height = 38;

  context.font = `600 ${fontSize}px "Space Grotesk", sans-serif`;
  context.clearRect(0, 0, canvas.width, canvas.height);
  context.shadowColor = themeColor(
    "--cad-sketch-label-shadow",
    "rgba(0, 0, 0, 0.55)",
  );
  const isMutedPreview = options.variant === "muted-preview";
  context.shadowBlur = isMutedPreview ? 1 : isSelected ? 4 : 3;
  context.fillStyle = isMutedPreview
    ? themeColor("--color-on-surface-muted", "rgba(223, 247, 250, 0.55)")
    : isSelected
      ? themeColor("--cad-sketch-label-selected", "#e7fbff")
      : themeColor("--cad-sketch-label", "rgba(223, 247, 250, 0.92)");
  context.textAlign = "center";
  context.textBaseline = "middle";
  context.fillText(text, canvas.width / 2, canvas.height / 2 + 1);

  const texture = new THREE.CanvasTexture(canvas);
  texture.needsUpdate = true;
  const material = new THREE.SpriteMaterial({
    map: texture,
    transparent: true,
    opacity: isMutedPreview ? 0.72 : 1,
    depthTest: false,
    depthWrite: false,
  });
  const sprite = new THREE.Sprite(material);
  sprite.scale.set(canvas.width / 9, canvas.height / 9, 1);
  sprite.userData.screenSize = {
    width: canvas.width,
    height: canvas.height,
  };
  sprite.userData.basePosition = null;
  return sprite;
}

export function makeConstraintBadgeSprite(text: string, isSelected: boolean) {
  const canvas = document.createElement("canvas");
  const context = canvas.getContext("2d");
  if (!context) {
    const texture = new THREE.CanvasTexture(canvas);
    return new THREE.Sprite(
      new THREE.SpriteMaterial({ map: texture, transparent: true }),
    );
  }

  canvas.width = 44;
  canvas.height = 44;
  context.clearRect(0, 0, canvas.width, canvas.height);
  context.shadowColor = themeColor(
    "--cad-sketch-label-shadow",
    "rgba(0, 0, 0, 0.55)",
  );
  context.shadowBlur = 3;
  context.fillStyle = isSelected
    ? themeColor("--cad-sketch-label-selected", "#e7fbff")
    : themeColor(
        "--cad-sketch-dimension-label",
        "rgba(211, 232, 235, 0.82)",
      );
  context.font = '700 24px "Space Grotesk", sans-serif';
  context.textAlign = "center";
  context.textBaseline = "middle";
  context.fillText(text, canvas.width / 2, canvas.height / 2 + 1);

  const texture = new THREE.CanvasTexture(canvas);
  texture.needsUpdate = true;
  const material = new THREE.SpriteMaterial({
    map: texture,
    transparent: true,
    depthTest: false,
    depthWrite: false,
  });
  const sprite = new THREE.Sprite(material);
  sprite.scale.set(6, 6, 1);
  sprite.userData.screenSize = {
    width: 44,
    height: 44,
  };
  return sprite;
}

export function buildSketchDimensionObject(
  dimension: SketchDimensionScene,
  displayUnits?: "mm" | "in",
  options: { variant?: "muted-preview"; pickable?: boolean } = {},
) {
  const isMutedPreview = options.variant === "muted-preview";
  const isPickable = options.pickable ?? !isMutedPreview;
  const labelText =
    displayUnits === "in" && dimension.unitSuffix === "mm"
      ? reformatDimensionLabel(dimension.label, dimension.kind, "in")
      : dimension.label;
  const labelPosition = new THREE.Vector3(...dimension.labelPosition);
  const anchorStart = new THREE.Vector3(...dimension.anchorStart);
  const anchorEnd = new THREE.Vector3(...dimension.anchorEnd);
  const dimensionStart = new THREE.Vector3(...dimension.dimensionStart);
  const dimensionEnd = new THREE.Vector3(...dimension.dimensionEnd);
  const extensionOverrun = 0.75;
  const arrowLength = 1.5;
  const arrowWidth = 0.27;
  const dimensionDirection = dimensionEnd.clone().sub(dimensionStart);
  const dimensionLength = dimensionDirection.length();
  if (dimensionLength > 1e-6) {
    dimensionDirection.divideScalar(dimensionLength);
  } else {
    dimensionDirection.set(1, 0, 0);
  }
  const extensionDirection = dimensionStart.clone().sub(anchorStart);
  if (extensionDirection.length() > 1e-6) {
    extensionDirection.normalize();
  } else {
    extensionDirection.set(0, 1, 0);
  }

  const points: THREE.Vector3[] = [];
  const addSegment = (start: THREE.Vector3, end: THREE.Vector3) => {
    points.push(start.clone(), end.clone());
  };

  // Filled arrow triangle vertices (flat array, 3 floats per vertex)
  const arrowPositions: number[] = [];
  const arrowIndices: number[] = [];
  const addFilledArrow = (tip: THREE.Vector3, inward: THREE.Vector3, perp: THREE.Vector3) => {
    const base = tip.clone().add(inward.clone().multiplyScalar(arrowLength));
    const side = perp.clone().multiplyScalar(arrowWidth);
    const idx = arrowPositions.length / 3;
    arrowPositions.push(tip.x, tip.y, tip.z);
    arrowPositions.push(base.x + side.x, base.y + side.y, base.z + side.z);
    arrowPositions.push(base.x - side.x, base.y - side.y, base.z - side.z);
    arrowIndices.push(idx, idx + 1, idx + 2);
  };

  // Reference line data for angle dimensions (hoisted for access outside the angle block)
  const refLineData: { start: THREE.Vector3; end: THREE.Vector3 } | null =
    (dimension.kind === "angle" || dimension.kind === "line_angle") &&
    dimension.refLineStart && dimension.refLineEnd
      ? {
          start: new THREE.Vector3(...dimension.refLineStart),
          end: new THREE.Vector3(...dimension.refLineEnd),
        }
      : null;

  if (dimension.kind === "angle" || dimension.kind === "line_angle") {
    // Prefer core-provided arc geometry when available
    if (dimension.arcRadius && dimension.arcRadius > 0 && dimension.arcCenter) {
      const pivot = new THREE.Vector3(...dimension.arcCenter);
      const startAngle = dimension.arcStartAngle ?? 0;
      const endAngle = dimension.arcEndAngle ?? 0;
      const ccw = dimension.arcCcw ?? true;



      // Arc vectors
      const startRay = dimensionStart.clone().sub(pivot);
      const endRay = dimensionEnd.clone().sub(pivot);
      const radius = startRay.length();
      const normal = startRay.clone().cross(endRay).normalize();
      if (radius > 1e-6 && normal.lengthSq() > 1e-8) {
        const uAxis = startRay.clone().normalize();
        const vAxis = normal.clone().cross(uAxis).normalize();

        // Sweep from core angles.  The (uAxis, vAxis) frame is oriented
        // so that a positive sweep always goes the short way from
        // dimensionStart to dimensionEnd (uAxis → vAxis).  Just use the
        // normalised magnitude — never flip on ccw.
        let sweep = endAngle - startAngle;
        while (sweep > Math.PI) sweep -= 2 * Math.PI;
        while (sweep <= -Math.PI) sweep += 2 * Math.PI;
        sweep = Math.abs(sweep);
        const arcSegments = 32;
        let previousPoint = dimensionStart.clone();
        for (let index = 1; index <= arcSegments; index++) {
          const angle = sweep * (index / arcSegments);
          const point = pivot
            .clone()
            .add(uAxis.clone().multiplyScalar(Math.cos(angle) * radius))
            .add(vAxis.clone().multiplyScalar(Math.sin(angle) * radius));
          addSegment(previousPoint, point);
          previousPoint = point;
        }

        // Arrowheads at arc endpoints
        const startTangent = normal.clone().cross(startRay).normalize();
        const endTangent = normal.clone().cross(endRay).normalize();
        const sweepSign = sweep >= 0 ? 1 : -1;
        addFilledArrow(
          dimensionStart,
          startTangent.clone().multiplyScalar(sweepSign),
          startRay.clone().normalize(),
        );
        addFilledArrow(
          dimensionEnd,
          endTangent.clone().multiplyScalar(-sweepSign),
          endRay.clone().normalize(),
        );
      }
    } else {
      // Fallback: client-side arc reconstruction from collinear point pairs
      const startRay = dimensionStart.clone().sub(anchorStart);
      const endRay = dimensionEnd.clone().sub(anchorEnd);
      if (startRay.lengthSq() > 1e-8 && endRay.lengthSq() > 1e-8) {
        const startDirection = startRay.clone().normalize();
        const endDirection = endRay.clone().normalize();
        const betweenAnchors = anchorStart.clone().sub(anchorEnd);
        const directionDot = startDirection.dot(endDirection);
        const denominator = 1 - directionDot * directionDot;
        if (Math.abs(denominator) > 1e-8) {
          const startOffset =
            (directionDot * endDirection.dot(betweenAnchors) -
              startDirection.dot(betweenAnchors)) /
            denominator;
          const endOffset =
            (endDirection.dot(betweenAnchors) -
              directionDot * startDirection.dot(betweenAnchors)) /
            denominator;
          const pivot = anchorStart
            .clone()
            .add(startDirection.clone().multiplyScalar(startOffset))
            .add(
              anchorEnd
                .clone()
                .add(endDirection.clone().multiplyScalar(endOffset)),
            )
            .multiplyScalar(0.5);
          const startVector = dimensionStart.clone().sub(pivot);
          const endVector = dimensionEnd.clone().sub(pivot);
          const radius = startVector.length();
          const normal2 = startVector.clone().cross(endVector).normalize();
          if (radius > 1e-6 && normal2.lengthSq() > 1e-8) {
            const uAxis = startVector.clone().normalize();
            const vAxis = normal2.clone().cross(uAxis).normalize();
            let sweep = Math.atan2(endVector.dot(vAxis), endVector.dot(uAxis));
            if (sweep > Math.PI) {
              sweep -= Math.PI * 2;
            } else if (sweep < -Math.PI) {
              sweep += Math.PI * 2;
            }
            const arcSegments = 32;
            let previousPoint = dimensionStart.clone();
            for (let index = 1; index <= arcSegments; index++) {
              const angle = (sweep * index) / arcSegments;
              const point = pivot
                .clone()
                .add(uAxis.clone().multiplyScalar(Math.cos(angle) * radius))
                .add(vAxis.clone().multiplyScalar(Math.sin(angle) * radius));
              addSegment(previousPoint, point);
              previousPoint = point;
            }
            const startTangent = normal2.clone().cross(startVector).normalize();
            const endTangent = normal2.clone().cross(endVector).normalize();
            const sweepSign = sweep >= 0 ? 1 : -1;
            addFilledArrow(
              dimensionStart,
              startTangent.clone().multiplyScalar(sweepSign),
              startVector.clone().normalize(),
            );
            addFilledArrow(
              dimensionEnd,
              endTangent.clone().multiplyScalar(-sweepSign),
              endVector.clone().normalize(),
            );
          }
        }
      }
    }
  } else {
    addSegment(
      anchorStart,
      dimensionStart
        .clone()
        .add(extensionDirection.clone().multiplyScalar(extensionOverrun)),
    );
    addSegment(dimensionStart, dimensionEnd);
    addSegment(
      anchorEnd,
      dimensionEnd
        .clone()
        .add(extensionDirection.clone().multiplyScalar(extensionOverrun)),
    );
    addFilledArrow(dimensionStart, dimensionDirection, extensionDirection);
    addFilledArrow(dimensionEnd, dimensionDirection.clone().multiplyScalar(-1), extensionDirection);
  }

  // Build line segments geometry
  const material = new THREE.LineBasicMaterial({
    color: isMutedPreview
      ? themeColor("--color-on-surface-muted", "#9b9b98")
      : dimension.isSelected
      ? themeColor("--color-primary-edge-active", "#c3f5ff")
      : themeColor("--color-primary-soft", "#8feaf7"),
    transparent: true,
    opacity: isMutedPreview ? 0.34 : dimension.isSelected ? 0.98 : 0.84,
    depthTest: false,
  });
  const geometry = new THREE.BufferGeometry().setFromPoints(points);
  const lineSegments = new THREE.LineSegments(geometry, material);
  lineSegments.renderOrder = isMutedPreview ? 5 : 6;
  if (isPickable) {
    lineSegments.userData.sketchDimensionId = dimension.dimensionId;
    lineSegments.userData.sketchDimensionPart = "geometry";
  }

  // Build filled arrow mesh
  const group = new THREE.Group();
  group.add(lineSegments);
  if (isPickable) {
    group.userData.sketchDimensionId = dimension.dimensionId;
    group.userData.sketchDimensionPart = "geometry";
  }

  if (arrowIndices.length > 0) {
    const arrowGeom = new THREE.BufferGeometry();
    arrowGeom.setAttribute(
      "position",
      new THREE.Float32BufferAttribute(arrowPositions, 3),
    );
    arrowGeom.setIndex(arrowIndices);
    const arrowMat = new THREE.MeshBasicMaterial({
      color: isMutedPreview
        ? themeColor("--color-on-surface-muted", "#9b9b98")
        : dimension.isSelected
        ? themeColor("--color-primary-edge-active", "#c3f5ff")
        : themeColor("--color-primary-soft", "#8feaf7"),
      transparent: true,
      opacity: isMutedPreview ? 0.34 : dimension.isSelected ? 0.98 : 0.84,
      depthTest: false,
      side: THREE.DoubleSide,
    });
    const arrowMesh = new THREE.Mesh(arrowGeom, arrowMat);
    arrowMesh.renderOrder = isMutedPreview ? 5 : 6;
    if (isPickable) {
      arrowMesh.userData.sketchDimensionId = dimension.dimensionId;
      arrowMesh.userData.sketchDimensionPart = "geometry";
    }
    group.add(arrowMesh);
  }

  // Dashed reference line for angle dimensions
  if (refLineData) {
    const refGeom = new THREE.BufferGeometry().setFromPoints([
      refLineData.start,
      refLineData.end,
    ]);
    const refMat = new THREE.LineDashedMaterial({
      color: isMutedPreview
        ? themeColor("--color-on-surface-muted", "#9b9b98")
        : dimension.isSelected
        ? themeColor("--color-primary-edge-active", "#c3f5ff")
        : themeColor("--color-primary-soft", "#8feaf7"),
      transparent: true,
      opacity: isMutedPreview ? 0.24 : 0.40,
      dashSize: 2,
      gapSize: 2,
      depthTest: false,
    });
    const refLine = new THREE.Line(refGeom, refMat);
    refLine.computeLineDistances();
    refLine.renderOrder = isMutedPreview ? 5 : 6;
    if (isPickable) {
      refLine.userData.sketchDimensionId = dimension.dimensionId;
      refLine.userData.sketchDimensionPart = "geometry";
    }
    group.add(refLine);
  }

  const label = makeDimensionLabelSprite(labelText, dimension.isSelected, {
    variant: options.variant,
  });
  label.position.copy(labelPosition);
  label.renderOrder = isMutedPreview ? 6 : 7;
  if (isPickable) {
    label.userData.sketchDimensionId = dimension.dimensionId;
    label.userData.sketchDimensionPart = "label";
  }
  label.userData.basePosition = dimension.labelPosition;
  label.userData.dimensionStart = dimension.dimensionStart;
  label.userData.dimensionEnd = dimension.dimensionEnd;
  label.userData.dimensionKind = dimension.kind;

  return { line: group, label };
}

export function buildSketchConstraintObject(constraint: SketchConstraintScene) {
  const badge = makeConstraintBadgeSprite(
    constraint.label,
    constraint.isSelected,
  );
  badge.position.set(...constraint.position);
  badge.renderOrder = 8;
  badge.userData.basePosition = constraint.position;
  badge.userData.sketchConstraintId = constraint.constraintId;
  badge.userData.sketchConstraintKind = constraint.kind;
  badge.userData.sketchConstraintEntityId = constraint.entityId;
  badge.userData.sketchConstraintRelatedEntityId = constraint.relatedEntityId;
  return badge;
}

export function buildSketchProfileObject(profile: SketchProfileScene) {
  const group = new THREE.Group();
  group.userData.sketchProfileId = profile.profileId;
  const fillMaterial = new THREE.MeshBasicMaterial({
    color: themeColor("--color-tertiary-plane-fill", "#fff7c0"),
    transparent: true,
    opacity: 0,
    side: THREE.DoubleSide,
    depthWrite: false,
    depthTest: false,
  });
  const edgeMaterials: THREE.LineBasicMaterial[] = [];

  const makeEdgeLoop = (points: Array<[number, number]>) => {
    const geometry = new THREE.BufferGeometry().setFromPoints(
      points.map((point) => new THREE.Vector3(point[0], point[1], 0)),
    );
    const material = new THREE.LineBasicMaterial({
      color: themeColor("--color-tertiary-plane-edge-hover", "#fff2b2"),
      transparent: true,
      opacity: 0,
      linewidth: 1,
      depthTest: false,
      depthWrite: false,
    });
    edgeMaterials.push(material);
    const line = new THREE.LineLoop(geometry, material);
    line.renderOrder = 7;
    line.userData.sketchProfileId = profile.profileId;
    return line;
  };

  if (profile.profileKind === "circle") {
    const geometry = new THREE.CircleGeometry(profile.radius, 48);
    // CircleGeometry is centered at (0, 0) in 2D plane coords. The
    // core ships the actual circle center as `profile.start` (in 2D
    // sketch coords), so we translate the geometry to that center
    // BEFORE applying the plane transform — otherwise the pickable
    // disk lands at the plane's origin instead of where the user sees
    // the circle, and Extrude can't hit it.
    geometry.translate(profile.start[0], profile.start[1], 0);
    const mesh = new THREE.Mesh(geometry, fillMaterial);
    mesh.renderOrder = 6;
    mesh.userData.sketchProfileId = profile.profileId;
    const points = new THREE.EllipseCurve(
      profile.start[0],
      profile.start[1],
      profile.radius,
      profile.radius,
      0,
      Math.PI * 2,
      false,
    ).getPoints(96);
    group.add(mesh);
    group.add(
      makeEdgeLoop(points.map((point) => [point.x, point.y] as [number, number])),
    );
    group.applyMatrix4(
      profile.planeFrame
        ? makePlaneTransformMatrixFromFrame(
            profile.planeFrame,
            SKETCH_PLANE_OFFSET,
          )
        : makePlaneTransformMatrix(profile.planeId, SKETCH_PLANE_OFFSET),
    );
    group.userData.sketchProfileArea = Math.PI * profile.radius * profile.radius;
    return {
      group,
      visual: {
        fillMaterial,
        edgeMaterials,
      },
    };
  }

  if (profile.profilePoints.length < 3) {
    return {
      group,
      visual: {
        fillMaterial,
        edgeMaterials,
      },
    };
  }

  const shape = new THREE.Shape();
  profile.profilePoints.forEach((point, index) => {
    if (index === 0) {
      shape.moveTo(point[0], point[1]);
      return;
    }
    shape.lineTo(point[0], point[1]);
  });
  shape.closePath();
  for (const loop of profile.innerLoops) {
    const path = new THREE.Path();
    // Three expects hole contours to wind opposite the outer shape.
    // The core stores profile loops in a consistent CCW order, so
    // reverse them here to make both rendering and raycasting match
    // the actual ring-shaped face.
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

  const geometry = new THREE.ShapeGeometry(shape);
  const mesh = new THREE.Mesh(geometry, fillMaterial);
  mesh.renderOrder = 6;
  mesh.userData.sketchProfileId = profile.profileId;
  group.add(mesh);
  group.add(makeEdgeLoop(profile.profilePoints));
  for (const loop of profile.innerLoops) {
    group.add(makeEdgeLoop(loop));
  }
  group.applyMatrix4(
    profile.planeFrame
      ? makePlaneTransformMatrixFromFrame(
          profile.planeFrame,
          SKETCH_PLANE_OFFSET,
        )
      : makePlaneTransformMatrix(profile.planeId, SKETCH_PLANE_OFFSET),
  );
  group.userData.sketchProfileArea =
    Math.abs(polygonArea2d(profile.profilePoints)) -
    profile.innerLoops.reduce(
      (sum, loop) => sum + Math.abs(polygonArea2d(loop)),
      0,
    );
  return {
    group,
    visual: {
      fillMaterial,
      edgeMaterials,
    },
  };
}

export function frameCamera(
  camera: THREE.OrthographicCamera,
  controls: OrbitControls,
  center: [number, number, number],
  maxDimension: number,
) {
  const distance = Math.max(maxDimension * 1.8, 160);
  const viewHeight = Math.max(maxDimension * 2.4, 120);
  camera.position.set(
    center[0] + distance,
    center[1] + distance * 0.8,
    center[2] + distance,
  );
  camera.zoom = Math.max((camera.top - camera.bottom) / viewHeight, 0.01);
  camera.updateProjectionMatrix();
  controls.target.set(...center);
  controls.update();
}

export function frameCameraToSketchPlane(
  camera: THREE.OrthographicCamera,
  controls: OrbitControls,
  activePlaneId: string,
  planeFrame: SketchPlaneFrame | null,
  maxDimension: number,
) {
  const distance = Math.max(maxDimension * 1.6, 120);
  const viewHeight = Math.max(maxDimension * 1.35, 80);
  camera.zoom = Math.max((camera.top - camera.bottom) / viewHeight, 0.01);
  camera.updateProjectionMatrix();

  if (planeFrame) {
    const origin = new THREE.Vector3(
      planeFrame.origin.x,
      planeFrame.origin.y,
      planeFrame.origin.z,
    );
    const normal = new THREE.Vector3(
      planeFrame.normal.x,
      planeFrame.normal.y,
      planeFrame.normal.z,
    ).normalize();

    // CAD-style up: prefer world Y; if the face normal is vertical, fall
    // back to world -Z so the sketch reads top-down without rolling.
    const worldUp = new THREE.Vector3(0, 1, 0);
    const up =
      Math.abs(normal.dot(worldUp)) > 0.95
        ? new THREE.Vector3(0, 0, -1)
        : worldUp.clone();

    camera.position.copy(origin.clone().add(normal.multiplyScalar(distance)));
    camera.up.copy(up);
    controls.target.copy(origin);
    controls.update();
    return;
  }

  if (activePlaneId === "ref-plane-xy") {
    camera.position.set(0, distance, 0);
    camera.up.set(0, 0, -1);
    controls.target.set(0, 0, 0);
    controls.update();
    return;
  }

  if (activePlaneId === "ref-plane-yz") {
    camera.position.set(distance, 0, 0);
    camera.up.set(0, 1, 0);
    controls.target.set(0, 0, 0);
    controls.update();
    return;
  }

  camera.position.set(0, 0, distance);
  camera.up.set(0, 1, 0);
  controls.target.set(0, 0, 0);
  controls.update();
}

export function resolveSketchPlanePoint(
  event: PointerEvent,
  renderer: THREE.WebGLRenderer,
  camera: THREE.Camera,
  activePlaneId: string,
  planeFrame: SketchPlaneFrame | null,
) {
  const rect = renderer.domElement.getBoundingClientRect();
  const pointer = new THREE.Vector2(
    ((event.clientX - rect.left) / rect.width) * 2 - 1,
    -((event.clientY - rect.top) / rect.height) * 2 + 1,
  );
  const raycaster = new THREE.Raycaster();
  raycaster.setFromCamera(pointer, camera);

  if (planeFrame) {
    const origin = new THREE.Vector3(
      planeFrame.origin.x,
      planeFrame.origin.y,
      planeFrame.origin.z,
    );
    const normal = new THREE.Vector3(
      planeFrame.normal.x,
      planeFrame.normal.y,
      planeFrame.normal.z,
    );
    const xAxis = new THREE.Vector3(
      planeFrame.x_axis.x,
      planeFrame.x_axis.y,
      planeFrame.x_axis.z,
    );
    const yAxis = new THREE.Vector3(
      planeFrame.y_axis.x,
      planeFrame.y_axis.y,
      planeFrame.y_axis.z,
    );
    const plane = new THREE.Plane().setFromNormalAndCoplanarPoint(
      normal,
      origin,
    );
    const hitPoint = new THREE.Vector3();
    const hit = raycaster.ray.intersectPlane(plane, hitPoint);
    if (!hit) {
      return null;
    }
    const relative = hitPoint.clone().sub(origin);
    return {
      local: [relative.dot(xAxis), relative.dot(yAxis)] as [number, number],
      world: [hitPoint.x, hitPoint.y, hitPoint.z] as [number, number, number],
    };
  }

  const plane =
    activePlaneId === "ref-plane-xy"
      ? new THREE.Plane(new THREE.Vector3(0, 1, 0), -SKETCH_PLANE_OFFSET)
      : activePlaneId === "ref-plane-yz"
        ? new THREE.Plane(new THREE.Vector3(1, 0, 0), -SKETCH_PLANE_OFFSET)
        : new THREE.Plane(new THREE.Vector3(0, 0, 1), -SKETCH_PLANE_OFFSET);

  const hitPoint = new THREE.Vector3();
  const hit = raycaster.ray.intersectPlane(plane, hitPoint);
  if (!hit) {
    return null;
  }

  if (activePlaneId === "ref-plane-xy") {
    return {
      local: [hitPoint.x, hitPoint.z] as [number, number],
      world: [hitPoint.x, SKETCH_PLANE_OFFSET, hitPoint.z] as [
        number,
        number,
        number,
      ],
    };
  }

  if (activePlaneId === "ref-plane-yz") {
    return {
      local: [hitPoint.y, hitPoint.z] as [number, number],
      world: [SKETCH_PLANE_OFFSET, hitPoint.y, hitPoint.z] as [
        number,
        number,
        number,
      ],
    };
  }

  return {
    local: [hitPoint.x, hitPoint.y] as [number, number],
    world: [hitPoint.x, hitPoint.y, SKETCH_PLANE_OFFSET] as [
      number,
      number,
      number,
    ],
  };
}

export function toWorldPoint(
  planeId: string,
  local: [number, number],
  planeFrame: SketchPlaneFrame | null = null,
): [number, number, number] {
  if (planeFrame) {
    return [
      planeFrame.origin.x +
        planeFrame.x_axis.x * local[0] +
        planeFrame.y_axis.x * local[1],
      planeFrame.origin.y +
        planeFrame.x_axis.y * local[0] +
        planeFrame.y_axis.y * local[1],
      planeFrame.origin.z +
        planeFrame.x_axis.z * local[0] +
        planeFrame.y_axis.z * local[1],
    ];
  }
  if (planeId === "ref-plane-xy") {
    return [local[0], SKETCH_PLANE_OFFSET, local[1]];
  }

  if (planeId === "ref-plane-yz") {
    return [SKETCH_PLANE_OFFSET, local[0], local[1]];
  }

  return [local[0], local[1], SKETCH_PLANE_OFFSET];
}

export function distanceBetweenPoints(
  a: [number, number],
  b: [number, number],
) {
  const dx = a[0] - b[0];
  const dy = a[1] - b[1];
  return Math.sqrt(dx * dx + dy * dy);
}

export function projectWorldPointToViewport(
  point: [number, number, number],
  camera: THREE.Camera,
  renderer: THREE.WebGLRenderer,
) {
  const projected = new THREE.Vector3(...point).project(camera);
  if (projected.z < -1 || projected.z > 1) {
    return null;
  }

  const widthHalf = renderer.domElement.clientWidth / 2;
  const heightHalf = renderer.domElement.clientHeight / 2;

  const rawX = projected.x * widthHalf + widthHalf;
  const rawY = -projected.y * heightHalf + heightHalf;

  return {
    x: Math.min(
      Math.max(rawX, DIMENSION_EDITOR_MARGIN),
      renderer.domElement.clientWidth - DIMENSION_EDITOR_MARGIN,
    ),
    y: Math.min(
      Math.max(rawY, DIMENSION_EDITOR_MARGIN),
      renderer.domElement.clientHeight - DIMENSION_EDITOR_MARGIN,
    ),
  };
}

// -- Trim tool: 2D intersection helpers -----------------------------------

const TRIM_COINCIDENT_TS = 0.01;

export function lineLineIntersectionTrim(
  ax: number, ay: number, bx: number, by: number,
  cx: number, cy: number, dx: number, dy: number,
): number | null {
  const abx = bx - ax, aby = by - ay;
  const cdx = dx - cx, cdy = dy - cy;
  const denom = abx * cdy - aby * cdx;
  if (Math.abs(denom) < TRIM_COINCIDENT_TS) return null;
  const acx = cx - ax, acy = cy - ay;
  const t = (acx * cdy - acy * cdx) / denom;
  const u = (acx * aby - acy * abx) / denom;
  if (t < -1e-12 || t > 1 + 1e-12) return null;
  if (u < -1e-12 || u > 1 + 1e-12) return null;
  return Math.max(0, Math.min(1, t));
}

export function lineCircleIntersectionTrim(
  ax: number, ay: number, bx: number, by: number,
  cx: number, cy: number, r: number,
): number[] {
  const abx = bx - ax, aby = by - ay;
  const dx = ax - cx, dy = ay - cy;
  const a = abx * abx + aby * aby;
  if (a < TRIM_COINCIDENT_TS * TRIM_COINCIDENT_TS) return [];
  const b = 2 * (dx * abx + dy * aby);
  const c = dx * dx + dy * dy - r * r;
  const disc = b * b - 4 * a * c;
  if (disc < -TRIM_COINCIDENT_TS) return [];
  const sqrtDisc = disc <= 0 ? 0 : Math.sqrt(disc);
  const inv2a = 1 / (2 * a);
  const result: number[] = [];
  for (const t of [(-b - sqrtDisc) * inv2a, (-b + sqrtDisc) * inv2a]) {
    if (t >= -1e-12 && t <= 1 + 1e-12) {
      result.push(Math.max(0, Math.min(1, t)));
    }
  }
  return result;
}
