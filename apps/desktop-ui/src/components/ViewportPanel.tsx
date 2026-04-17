// @ts-nocheck
import { useEffect, useMemo, useRef, useState } from "react";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import {
  createViewportScene,
  type ReferenceAxisScene,
  type ReferencePlaneScene,
  type ScenePrimitive,
  type SolidFaceScene,
  type SketchCircleScene,
  type SketchConstraintScene,
  type SketchDimensionScene,
  type SketchLineScene,
  type SketchPointScene,
  type SketchProfileScene,
} from "../lib/viewportScene";
import type { DocumentState, ViewportState } from "../types/ipc";

interface ViewportPanelProps {
  status: "idle" | "starting" | "connected" | "error" | "stopped";
  document: DocumentState | null;
  viewport: ViewportState | null;
  onSelectPrimitive: (primitiveId: string) => Promise<void>;
  onSelectReference: (referenceId: string) => Promise<void>;
  onSelectFace: (faceId: string) => Promise<void>;
  onStartSketch: (referenceId: string) => Promise<void>;
  onStartSketchOnFace: (
    faceId: string,
    planeFrame: SolidFacePlaneFrame,
  ) => Promise<void>;
  onAddSketchLine: (
    startX: number,
    startY: number,
    endX: number,
    endY: number,
  ) => Promise<void>;
  onAddSketchRectangle: (
    startX: number,
    startY: number,
    endX: number,
    endY: number,
  ) => Promise<void>;
  onAddSketchCircle: (
    centerX: number,
    centerY: number,
    radius: number,
  ) => Promise<void>;
  onSelectSketchEntity: (entityId: string) => Promise<void>;
  onPickSketchPoint: (
    pointId: string,
    entityId: string,
    kind: "endpoint" | "center",
  ) => Promise<void>;
  armedSketchConstraint:
    | null
    | { kind: "horizontal" | "vertical" | "clear" }
    | {
        kind: "equal_length" | "perpendicular" | "parallel";
        firstLineId: string | null;
      }
    | { kind: "coincident"; firstPointId: string | null };
  onCancelSketchConstraint: () => void;
  onClearSketchConstraint: (
    kind:
      | "horizontal"
      | "vertical"
      | "equal_length"
      | "perpendicular"
      | "parallel",
    entityId: string,
    relatedEntityId: string | null,
  ) => Promise<void>;
  onSelectSketchDimension: (dimensionId: string) => Promise<void>;
  onUpdateSketchDimension: (
    dimensionId: string,
    value: number,
  ) => Promise<void>;
  onSelectSketchProfile: (profileId: string) => Promise<void>;
  onSetSketchTool: (
    tool: "select" | "line" | "rectangle" | "circle",
  ) => Promise<void>;
  onFinishSketch: () => Promise<void>;
}

interface PrimitiveVisual {
  baseMaterial: THREE.MeshStandardMaterial;
  edgeMaterial: THREE.LineBasicMaterial;
}

interface ReferencePlaneVisual {
  fillMaterial: THREE.MeshBasicMaterial;
  edgeMaterial: THREE.LineBasicMaterial;
}

interface PrimitiveInteractionState {
  isSelected: boolean;
  isHovered: boolean;
}

interface ReferencePlaneInteractionState {
  isSelected: boolean;
  isHovered: boolean;
  isActiveSketchPlane: boolean;
}

interface ViewportContextMenuState {
  x: number;
  y: number;
  referenceId: string | null;
  faceId: string | null;
}

interface SketchEntityInteractionState {
  isSelected: boolean;
  isHovered: boolean;
}

interface SketchPreviewPoint {
  local: [number, number];
  world: [number, number, number];
  snapLabel: string | null;
}

type SketchPlaneFrame = NonNullable<
  NonNullable<NonNullable<DocumentState["feature_history"][number]["sketch_parameters"]>["plane_frame"]>
>;

type SolidFacePlaneFrame = SolidFaceScene["planeFrame"];

const SKETCH_PLANE_OFFSET = 0.2;
const REFERENCE_PLANE_RENDER_SIZE = 25;
const REFERENCE_PLANE_MARGIN = 5;
const SKETCH_SNAP_DISTANCE = 2.5;
const DIMENSION_EDITOR_MARGIN = 20;

function themeColor(token: string, fallback: string) {
  if (typeof document === "undefined") {
    return fallback;
  }

  const value = getComputedStyle(document.documentElement)
    .getPropertyValue(token)
    .trim();
  return value || fallback;
}

function disposeMaterial(material: THREE.Material | THREE.Material[]) {
  if (Array.isArray(material)) {
    for (const entry of material) {
      entry.dispose();
    }
    return;
  }

  material.dispose();
}

function disposeGroup(group: THREE.Group) {
  for (const child of [...group.children]) {
    group.remove(child);

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

function applyPrimitiveVisualState(
  visual: PrimitiveVisual,
  state: PrimitiveInteractionState,
) {
  if (state.isSelected) {
    visual.baseMaterial.color.set(
      themeColor("--color-primary-bright", "#4be7ff"),
    );
    visual.baseMaterial.emissive.set(
      themeColor("--color-primary-emissive-active", "#0b5963"),
    );
    visual.baseMaterial.emissiveIntensity = 0.65;
    visual.baseMaterial.opacity = 0.92;
    visual.edgeMaterial.color.set(
      themeColor("--color-primary-edge-active", "#c3f5ff"),
    );
    return;
  }

  if (state.isHovered) {
    visual.baseMaterial.color.set(
      themeColor("--color-primary-hover", "#86f4ff"),
    );
    visual.baseMaterial.emissive.set(
      themeColor("--color-primary-emissive-hover", "#0a4149"),
    );
    visual.baseMaterial.emissiveIntensity = 0.35;
    visual.baseMaterial.opacity = 0.82;
    visual.edgeMaterial.color.set(
      themeColor("--color-primary-edge-hover", "#b8fbff"),
    );
    return;
  }

  visual.baseMaterial.color.set(themeColor("--color-primary-glow", "#6de3ef"));
  visual.baseMaterial.emissive.set(
    themeColor("--color-primary-emissive", "#042329"),
  );
  visual.baseMaterial.emissiveIntensity = 0.2;
  visual.baseMaterial.opacity = 0.72;
  visual.edgeMaterial.color.set(themeColor("--color-primary-edge", "#91f2ff"));
}

function applyReferencePlaneVisualState(
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

function buildPrimitiveObject(primitive: ScenePrimitive) {
  const baseMaterial = new THREE.MeshStandardMaterial({
    color: themeColor("--color-primary-glow", "#6de3ef"),
    emissive: themeColor("--color-primary-emissive", "#042329"),
    emissiveIntensity: 0.2,
    metalness: 0.1,
    roughness: 0.48,
    transparent: true,
    opacity: 0.72,
  });
  const edgeMaterial = new THREE.LineBasicMaterial({
    color: themeColor("--color-primary-edge", "#91f2ff"),
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
  } else {
    const shape = new THREE.Shape();
    primitive.profilePoints.forEach((point, index) => {
      if (index === 0) {
        shape.moveTo(point[0], point[1]);
        return;
      }
      shape.lineTo(point[0], point[1]);
    });
    shape.closePath();

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
  }

  const mesh = new THREE.Mesh(geometry, baseMaterial);
  if (primitive.kind !== "polygon_extrude") {
    mesh.position.set(...primitive.position);
  }
  mesh.userData.primitiveId = primitive.primitiveId;

  const edges = new THREE.LineSegments(
    new THREE.EdgesGeometry(geometry),
    edgeMaterial,
  );
  if (primitive.kind !== "polygon_extrude") {
    edges.position.copy(mesh.position);
  }

  return {
    mesh,
    edges,
    visual: {
      baseMaterial,
      edgeMaterial,
    },
  };
}

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

function planeOrientationFromId(
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

function makePlaneTransformMatrix(planeId: string, offset = 0) {
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

function makePlaneTransformMatrixFromFrame(
  planeFrame:
    | SketchPlaneFrame
    | {
        origin: [number, number, number] | { x: number; y: number; z: number };
        xAxis?: [number, number, number] | { x: number; y: number; z: number };
        yAxis?: [number, number, number] | { x: number; y: number; z: number };
        x_axis?: { x: number; y: number; z: number };
        y_axis?: { x: number; y: number; z: number };
        normal:
          | [number, number, number]
          | { x: number; y: number; z: number };
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
    xAxis.x,
    yAxis.x,
    normal.x,
    origin.x + normal.x * offset,
    xAxis.y,
    yAxis.y,
    normal.y,
    origin.y + normal.y * offset,
    xAxis.z,
    yAxis.z,
    normal.z,
    origin.z + normal.z * offset,
    0,
    0,
    0,
    1,
  );
}

function buildReferencePlaneObject(plane: ReferencePlaneScene) {
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
  orientPlaneMesh(mesh, plane.orientation);
  // Render the origin helpers from the world origin so the visual margin is
  // measured from the axes, not from the core plane center.
  const offset = REFERENCE_PLANE_MARGIN + REFERENCE_PLANE_RENDER_SIZE / 2;
  const renderPosition =
    plane.orientation === "xy"
      ? [offset, 0, offset]
      : plane.orientation === "yz"
        ? [0, offset, offset]
        : [offset, offset, 0];
  mesh.position.set(...renderPosition);
  mesh.userData.referenceId = plane.referenceId;

  const edges = new THREE.LineSegments(
    new THREE.EdgesGeometry(geometry),
    edgeMaterial,
  );
  orientPlaneMesh(edges, plane.orientation);
  edges.position.copy(mesh.position);

  return {
    mesh,
    edges,
    visual: {
      fillMaterial,
      edgeMaterial,
    },
  };
}

function buildReferenceAxisObject(axis: ReferenceAxisScene) {
  const color =
    axis.axis === "x"
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

function orientFaceMesh(mesh: THREE.Object3D, face: SolidFaceScene) {
  if (Math.abs(face.normal[1]) > 0.5) {
    mesh.rotation.x = -Math.PI / 2;
    return;
  }

  if (Math.abs(face.normal[0]) > 0.5) {
    mesh.rotation.y = Math.PI / 2;
  }
}

function buildSolidFaceObject(face: SolidFaceScene) {
  const material = new THREE.MeshBasicMaterial({
    color: face.isSelected
      ? themeColor("--color-primary-soft", "#c3f5ff")
      : themeColor("--color-primary-fixed-dim", "#00daf3"),
    transparent: true,
    opacity: face.isSelected ? 0.3 : 0.12,
    side: THREE.DoubleSide,
    depthWrite: false,
  });
  const geometry = new THREE.PlaneGeometry(
    Math.max(face.size.width || face.size.radius * 2 || 1, 1),
    Math.max(face.size.height || face.size.radius * 2 || 1, 1),
  );
  const mesh = new THREE.Mesh(geometry, material);
  mesh.applyMatrix4(makePlaneTransformMatrixFromFrame(face.planeFrame));
  mesh.userData.faceId = face.faceId;
  return mesh;
}

function buildSketchLineObject(line: SketchLineScene) {
  const material = new THREE.LineBasicMaterial({
    color: line.isSelected
      ? themeColor("--color-primary-edge-active", "#c3f5ff")
      : themeColor("--color-tertiary-plane-fill", "#fff7c0"),
    transparent: true,
    opacity: 0.98,
    linewidth: line.isSelected ? 2 : 1,
  });
  const points = [
    new THREE.Vector3(...line.start),
    new THREE.Vector3(...line.end),
  ];
  const geometry = new THREE.BufferGeometry().setFromPoints(points);
  const sketchLine = new THREE.Line(geometry, material);
  sketchLine.userData.sketchEntityId = line.lineId;
  sketchLine.userData.sketchEntityKind = "line";
  return sketchLine;
}

function buildSketchCircleObject(circle: SketchCircleScene) {
  const material = new THREE.LineBasicMaterial({
    color: circle.isSelected
      ? themeColor("--color-primary-edge-active", "#c3f5ff")
      : themeColor("--color-tertiary-plane-fill", "#fff7c0"),
    transparent: true,
    opacity: 0.98,
  });
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
  const points = curve
    .getPoints(64)
    .map((point) =>
      circle.planeId === "ref-plane-xy"
        ? new THREE.Vector3(
            circle.center[0] + point.x,
            circle.center[1],
            circle.center[2] + point.y,
          )
        : circle.planeId === "ref-plane-yz"
          ? new THREE.Vector3(
              circle.center[0],
              circle.center[1] + point.x,
              circle.center[2] + point.y,
            )
          : new THREE.Vector3(
              circle.center[0] + point.x,
              circle.center[1] + point.y,
              circle.center[2],
            ),
    );
  const geometry = new THREE.BufferGeometry().setFromPoints(points);
  const sketchCircle = new THREE.LineLoop(geometry, material);
  sketchCircle.userData.sketchEntityId = circle.circleId;
  sketchCircle.userData.sketchEntityKind = "circle";
  return sketchCircle;
}

function buildSketchPointObject(point: SketchPointScene) {
  const geometry = new THREE.SphereGeometry(
    point.kind === "center" ? 0.9 : 0.7,
    12,
    12,
  );
  const material = new THREE.MeshBasicMaterial({
    color: point.isSelected
      ? themeColor("--color-primary-edge-active", "#c3f5ff")
      : point.kind === "center"
        ? themeColor("--color-axis-z", "#6db4ff")
        : themeColor("--color-tertiary-plane-edge", "#ffe784"),
    transparent: true,
    opacity: point.isSelected ? 1 : 0.92,
  });
  const mesh = new THREE.Mesh(geometry, material);
  mesh.position.set(...point.position);
  mesh.userData.sketchPointId = point.pointId;
  mesh.userData.sketchPointEntityId = point.entityId;
  mesh.userData.sketchPointKind = point.kind;
  return mesh;
}

function makeDimensionLabelSprite(text: string) {
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
  canvas.width = textWidth + 28;
  canvas.height = 52;

  context.font = `600 ${fontSize}px "Space Grotesk", sans-serif`;
  context.clearRect(0, 0, canvas.width, canvas.height);
  context.fillStyle = "rgba(7, 13, 16, 0.88)";
  context.strokeStyle = "rgba(195, 245, 255, 0.9)";
  context.lineWidth = 2;
  context.beginPath();
  context.roundRect(1, 1, canvas.width - 2, canvas.height - 2, 14);
  context.fill();
  context.stroke();
  context.fillStyle = "#e7fbff";
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
  sprite.scale.set(canvas.width / 9, canvas.height / 9, 1);
  return sprite;
}

function makeConstraintBadgeSprite(text: string, isSelected: boolean) {
  const canvas = document.createElement("canvas");
  const context = canvas.getContext("2d");
  if (!context) {
    const texture = new THREE.CanvasTexture(canvas);
    return new THREE.Sprite(
      new THREE.SpriteMaterial({ map: texture, transparent: true }),
    );
  }

  canvas.width = 54;
  canvas.height = 54;
  context.clearRect(0, 0, canvas.width, canvas.height);
  context.fillStyle = isSelected
    ? "rgba(20, 50, 58, 0.94)"
    : "rgba(7, 13, 16, 0.88)";
  context.strokeStyle = isSelected
    ? "rgba(195, 245, 255, 1)"
    : "rgba(160, 228, 239, 0.92)";
  context.lineWidth = 2;
  context.beginPath();
  context.roundRect(4, 4, canvas.width - 8, canvas.height - 8, 16);
  context.fill();
  context.stroke();
  context.fillStyle = "#e7fbff";
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
  sprite.scale.set(5.4, 5.4, 1);
  return sprite;
}

function buildSketchDimensionObject(dimension: SketchDimensionScene) {
  const material = new THREE.LineBasicMaterial({
    color: dimension.isSelected
      ? themeColor("--color-primary-edge-active", "#c3f5ff")
      : themeColor("--color-primary-soft", "#8feaf7"),
    transparent: true,
    opacity: dimension.isSelected ? 0.98 : 0.84,
    depthTest: false,
  });
  const geometry = new THREE.BufferGeometry().setFromPoints([
    new THREE.Vector3(...dimension.anchorStart),
    new THREE.Vector3(...dimension.dimensionStart),
    new THREE.Vector3(...dimension.dimensionStart),
    new THREE.Vector3(...dimension.dimensionEnd),
    new THREE.Vector3(...dimension.anchorEnd),
    new THREE.Vector3(...dimension.dimensionEnd),
  ]);
  const line = new THREE.LineSegments(geometry, material);
  line.renderOrder = 6;
  line.userData.sketchDimensionId = dimension.dimensionId;

  const label = makeDimensionLabelSprite(dimension.label);
  label.position.set(...dimension.labelPosition);
  label.renderOrder = 7;
  label.userData.sketchDimensionId = dimension.dimensionId;

  return { line, label };
}

function buildSketchConstraintObject(constraint: SketchConstraintScene) {
  const badge = makeConstraintBadgeSprite(
    constraint.label,
    constraint.isSelected,
  );
  badge.position.set(...constraint.position);
  badge.renderOrder = 8;
  badge.userData.sketchConstraintId = constraint.constraintId;
  badge.userData.sketchConstraintKind = constraint.kind;
  badge.userData.sketchConstraintEntityId = constraint.entityId;
  badge.userData.sketchConstraintRelatedEntityId = constraint.relatedEntityId;
  return badge;
}

function buildSketchProfileObject(profile: SketchProfileScene) {
  const material = new THREE.MeshBasicMaterial({
    color: profile.isSelected
      ? themeColor("--color-primary-soft", "#c3f5ff")
      : themeColor("--color-tertiary-plane-fill", "#fff7c0"),
    transparent: true,
    opacity: profile.isSelected ? 0.26 : 0.14,
    side: THREE.DoubleSide,
    depthWrite: false,
  });

  if (profile.profileKind === "circle") {
    const geometry = new THREE.CircleGeometry(profile.radius, 48);
    const mesh = new THREE.Mesh(geometry, material);
    if (profile.planeFrame) {
      mesh.applyMatrix4(
        makePlaneTransformMatrixFromFrame(
          profile.planeFrame,
          SKETCH_PLANE_OFFSET,
        ),
      );
    } else {
      orientPlaneMesh(mesh, planeOrientationFromId(profile.planeId));
      mesh.position.set(...toWorldPoint(profile.planeId, profile.start));
    }
    mesh.userData.sketchProfileId = profile.profileId;
    return mesh;
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

  const geometry = new THREE.ShapeGeometry(shape);
  geometry.applyMatrix4(
    profile.planeFrame
      ? makePlaneTransformMatrixFromFrame(
          profile.planeFrame,
          SKETCH_PLANE_OFFSET,
        )
      : makePlaneTransformMatrix(profile.planeId, SKETCH_PLANE_OFFSET),
  );
  const mesh = new THREE.Mesh(geometry, material);
  mesh.userData.sketchProfileId = profile.profileId;
  return mesh;
}

function frameCamera(
  camera: THREE.PerspectiveCamera,
  controls: OrbitControls,
  center: [number, number, number],
  maxDimension: number,
) {
  const distance = Math.max(maxDimension * 1.8, 160);
  camera.position.set(
    center[0] + distance,
    center[1] + distance * 0.8,
    center[2] + distance,
  );
  controls.target.set(...center);
  controls.update();
}

function frameCameraToSketchPlane(
  camera: THREE.PerspectiveCamera,
  controls: OrbitControls,
  activePlaneId: string,
  planeFrame: SketchPlaneFrame | null,
  maxDimension: number,
) {
  const distance = Math.max(maxDimension * 1.6, 120);

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
    const up = new THREE.Vector3(
      planeFrame.y_axis.x,
      planeFrame.y_axis.y,
      planeFrame.y_axis.z,
    );
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

function resolveSketchPlanePoint(
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
    const plane = new THREE.Plane().setFromNormalAndCoplanarPoint(normal, origin);
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

function toWorldPoint(
  planeId: string,
  local: [number, number],
  planeFrame: SketchPlaneFrame | null = null,
): [number, number, number] {
  if (planeFrame) {
    return [
      planeFrame.origin.x + planeFrame.x_axis.x * local[0] + planeFrame.y_axis.x * local[1],
      planeFrame.origin.y + planeFrame.x_axis.y * local[0] + planeFrame.y_axis.y * local[1],
      planeFrame.origin.z + planeFrame.x_axis.z * local[0] + planeFrame.y_axis.z * local[1],
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

function distanceBetweenPoints(a: [number, number], b: [number, number]) {
  const dx = a[0] - b[0];
  const dy = a[1] - b[1];
  return Math.sqrt(dx * dx + dy * dy);
}

function projectWorldPointToViewport(
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

export function ViewportPanel({
  status,
  document,
  viewport,
  onSelectPrimitive,
  onSelectReference,
  onSelectFace,
  onStartSketch,
  onStartSketchOnFace,
  onAddSketchLine,
  onAddSketchRectangle,
  onAddSketchCircle,
  onSelectSketchEntity,
  onPickSketchPoint,
  armedSketchConstraint,
  onCancelSketchConstraint,
  onClearSketchConstraint,
  onSelectSketchDimension,
  onUpdateSketchDimension,
  onSelectSketchProfile,
  onSetSketchTool,
  onFinishSketch,
}: ViewportPanelProps) {
  const [showReferencePlanes, setShowReferencePlanes] = useState(true);
  const [contextMenu, setContextMenu] =
    useState<ViewportContextMenuState | null>(null);
  const [sketchSnapLabel, setSketchSnapLabel] = useState<string | null>(null);
  const [dimensionDraftValue, setDimensionDraftValue] = useState("");
  const hostRef = useRef<HTMLDivElement | null>(null);
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const dimensionEditorRef = useRef<HTMLFormElement | null>(null);
  const dimensionInputRef = useRef<HTMLInputElement | null>(null);
  const rendererRef = useRef<THREE.WebGLRenderer | null>(null);
  const sceneRef = useRef<THREE.Scene | null>(null);
  const cameraRef = useRef<THREE.PerspectiveCamera | null>(null);
  const controlsRef = useRef<OrbitControls | null>(null);
  const contentGroupRef = useRef<THREE.Group | null>(null);
  const referenceGroupRef = useRef<THREE.Group | null>(null);
  const sketchGroupRef = useRef<THREE.Group | null>(null);
  const previewLineRef = useRef<THREE.Line | null>(null);
  const previewCircleRef = useRef<THREE.LineLoop | null>(null);
  const lineDraftStartRef = useRef<[number, number] | null>(null);
  const previousReferencePlaneVisibilityRef = useRef<boolean | null>(null);
  const primitiveVisualsRef = useRef(new Map<string, PrimitiveVisual>());
  const primitiveStatesRef = useRef(
    new Map<string, PrimitiveInteractionState>(),
  );
  const referencePlaneVisualsRef = useRef(
    new Map<string, ReferencePlaneVisual>(),
  );
  const referencePlaneStatesRef = useRef(
    new Map<string, ReferencePlaneInteractionState>(),
  );
  const gridRef = useRef<THREE.GridHelper | null>(null);
  const meshesRef = useRef<THREE.Mesh[]>([]);
  const referencePlaneMeshesRef = useRef<THREE.Mesh[]>([]);
  const sketchEntityObjectsRef = useRef<Array<THREE.Line | THREE.LineLoop>>([]);
  const sketchDimensionObjectsRef = useRef<Array<THREE.Object3D>>([]);
  const sketchConstraintObjectsRef = useRef<Array<THREE.Object3D>>([]);
  const sketchPointObjectsRef = useRef<THREE.Mesh[]>([]);
  const sketchProfileMeshesRef = useRef<THREE.Mesh[]>([]);
  const faceMeshesRef = useRef<THREE.Mesh[]>([]);
  const lastGeometryKeyRef = useRef("");
  const selectPrimitiveRef = useRef(onSelectPrimitive);
  const selectReferenceRef = useRef(onSelectReference);
  const selectFaceRef = useRef(onSelectFace);
  const startSketchRef = useRef(onStartSketch);
  const startSketchOnFaceRef = useRef(onStartSketchOnFace);
  const addSketchLineRef = useRef(onAddSketchLine);
  const addSketchRectangleRef = useRef(onAddSketchRectangle);
  const addSketchCircleRef = useRef(onAddSketchCircle);
  const selectSketchEntityRef = useRef(onSelectSketchEntity);
  const pickSketchPointRef = useRef(onPickSketchPoint);
  const selectSketchDimensionRef = useRef(onSelectSketchDimension);
  const updateSketchDimensionRef = useRef(onUpdateSketchDimension);
  const selectSketchProfileRef = useRef(onSelectSketchProfile);
  const selectedSketchDimensionRef = useRef<SketchDimensionScene | null>(null);
  const setSketchToolRef = useRef(onSetSketchTool);
  const finishSketchRef = useRef(onFinishSketch);
  const armedSketchConstraintRef = useRef(armedSketchConstraint);
  const cancelSketchConstraintRef = useRef(onCancelSketchConstraint);
  const clearSketchConstraintRef = useRef(onClearSketchConstraint);
  const activeSketchToolRef = useRef<
    "select" | "line" | "rectangle" | "circle"
  >("select");
  const sketchSnapCandidatesRef = useRef<
    Array<{ local: [number, number]; label: string }>
  >([]);
  const sceneData = useMemo(
    () =>
      viewport?.has_active_document ? createViewportScene(viewport) : null,
    [viewport],
  );
  const hasActiveDocument = Boolean(viewport?.has_active_document);
  const activeSketchPlaneId = document?.active_sketch_plane_id ?? null;
  const activeSketchTool = document?.active_sketch_tool ?? "select";
  const sketchFeature = useMemo(
    () =>
      document?.feature_history.find(
        (feature) => feature.feature_id === document.active_sketch_feature_id,
      ) ?? null,
    [document],
  );
  const selectedPrimitiveLabel = useMemo(() => {
    const selectedBox = viewport?.boxes.find((box) => box.is_selected);
    if (selectedBox) {
      return selectedBox.label;
    }

    const selectedCylinder = viewport?.cylinders.find(
      (cylinder) => cylinder.is_selected,
    );
    if (selectedCylinder) {
      return selectedCylinder.label;
    }

    const selectedPolygonExtrude = viewport?.polygon_extrudes.find(
      (primitive) => primitive.is_selected,
    );
    return selectedPolygonExtrude?.label ?? null;
  }, [viewport]);
  const selectedReference = useMemo(
    () =>
      viewport?.reference_planes.find(
        (referencePlane) => referencePlane.is_selected,
      ) ?? null,
    [viewport],
  );
  const selectedSketchProfile = useMemo(
    () =>
      viewport?.sketch_profiles.find((profile) => profile.is_selected) ?? null,
    [viewport],
  );
  const selectedSketchDimension = useMemo(
    () =>
      document?.selected_sketch_dimension_id
        ? (sceneData?.sketchDimensions.find(
            (dimension) =>
              dimension.dimensionId === document.selected_sketch_dimension_id,
          ) ?? null)
        : null,
    [document?.selected_sketch_dimension_id, sceneData],
  );
  const selectedSketchDimensionValue = useMemo(
    () =>
      document?.selected_sketch_dimension_id && sketchFeature?.sketch_parameters
        ? (sketchFeature.sketch_parameters.dimensions.find(
            (dimension) =>
              dimension.dimension_id === document.selected_sketch_dimension_id,
          )?.value ?? null)
        : null,
    [document?.selected_sketch_dimension_id, sketchFeature],
  );
  const sketchSnapCandidates = useMemo(() => {
    if (!sketchFeature?.sketch_parameters) {
      return [];
    }

    const candidates: Array<{ local: [number, number]; label: string }> = [
      { local: [0, 0], label: "Origin" },
    ];
    for (const line of sketchFeature.sketch_parameters.lines) {
      candidates.push({
        local: [line.start_x, line.start_y],
        label:
          line.constraint === "horizontal" || line.constraint === "vertical"
            ? `${line.line_id} (${line.constraint})`
            : line.line_id,
      });
      candidates.push({
        local: [line.end_x, line.end_y],
        label: line.line_id,
      });
    }
    for (const circle of sketchFeature.sketch_parameters.circles) {
      candidates.push({
        local: [circle.center_x, circle.center_y],
        label: circle.circle_id,
      });
    }
    return candidates;
  }, [sketchFeature]);
  const activeSketchPlaneFrame =
    sketchFeature?.sketch_parameters?.plane_frame ?? null;

  function clearPreviewLine() {
    const previewLine = previewLineRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!previewLine || !sketchGroup) {
      return;
    }

    sketchGroup.remove(previewLine);
    previewLine.geometry.dispose();
    disposeMaterial(previewLine.material);
    previewLineRef.current = null;
  }

  function clearPreviewCircle() {
    const previewCircle = previewCircleRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!previewCircle || !sketchGroup) {
      return;
    }

    sketchGroup.remove(previewCircle);
    previewCircle.geometry.dispose();
    disposeMaterial(previewCircle.material);
    previewCircleRef.current = null;
  }

  function resolveSnappedSketchPoint(rawPoint: {
    local: [number, number];
    world: [number, number, number];
  }) {
    let closestCandidate: { local: [number, number]; label: string } | null =
      null;
    let closestDistance = Number.POSITIVE_INFINITY;

    for (const candidate of sketchSnapCandidatesRef.current) {
      const distance = distanceBetweenPoints(rawPoint.local, candidate.local);
      if (distance < closestDistance) {
        closestDistance = distance;
        closestCandidate = candidate;
      }
    }

    if (closestCandidate && closestDistance <= SKETCH_SNAP_DISTANCE) {
      return {
        local: closestCandidate.local,
        world: toWorldPoint(
          activeSketchPlaneId ?? "ref-plane-xy",
          closestCandidate.local,
          activeSketchPlaneFrame,
        ),
        snapLabel: closestCandidate.label,
      } satisfies SketchPreviewPoint;
    }

    return {
      local: rawPoint.local,
      world: rawPoint.world,
      snapLabel: null,
    } satisfies SketchPreviewPoint;
  }

  function syncPrimitiveVisuals() {
    for (const [primitiveId, visual] of primitiveVisualsRef.current.entries()) {
      const state = primitiveStatesRef.current.get(primitiveId);
      if (!state) {
        continue;
      }

      applyPrimitiveVisualState(visual, state);
    }
  }

  function syncReferencePlaneVisuals() {
    for (const [
      referenceId,
      visual,
    ] of referencePlaneVisualsRef.current.entries()) {
      const state = referencePlaneStatesRef.current.get(referenceId);
      if (!state) {
        continue;
      }

      applyReferencePlaneVisualState(visual, state);
    }
  }

  function setHoveredPrimitive(primitiveId: string | null) {
    let changed = false;

    for (const [id, state] of primitiveStatesRef.current.entries()) {
      const nextHovered = id === primitiveId;
      if (state.isHovered !== nextHovered) {
        primitiveStatesRef.current.set(id, {
          ...state,
          isHovered: nextHovered,
        });
        changed = true;
      }
    }

    if (changed) {
      syncPrimitiveVisuals();
    }
  }

  function setHoveredReference(referenceId: string | null) {
    let changed = false;

    for (const [id, state] of referencePlaneStatesRef.current.entries()) {
      const nextHovered = id === referenceId;
      if (state.isHovered !== nextHovered) {
        referencePlaneStatesRef.current.set(id, {
          ...state,
          isHovered: nextHovered,
        });
        changed = true;
      }
    }

    if (changed) {
      syncReferencePlaneVisuals();
    }
  }

  useEffect(() => {
    selectPrimitiveRef.current = onSelectPrimitive;
    selectReferenceRef.current = onSelectReference;
    selectFaceRef.current = onSelectFace;
    startSketchRef.current = onStartSketch;
    startSketchOnFaceRef.current = onStartSketchOnFace;
    addSketchLineRef.current = onAddSketchLine;
    addSketchRectangleRef.current = onAddSketchRectangle;
    addSketchCircleRef.current = onAddSketchCircle;
    selectSketchEntityRef.current = onSelectSketchEntity;
    pickSketchPointRef.current = onPickSketchPoint;
    selectSketchDimensionRef.current = onSelectSketchDimension;
    updateSketchDimensionRef.current = onUpdateSketchDimension;
    selectSketchProfileRef.current = onSelectSketchProfile;
    setSketchToolRef.current = onSetSketchTool;
    finishSketchRef.current = onFinishSketch;
    armedSketchConstraintRef.current = armedSketchConstraint;
    cancelSketchConstraintRef.current = onCancelSketchConstraint;
    clearSketchConstraintRef.current = onClearSketchConstraint;
  }, [
    onSelectPrimitive,
    onSelectReference,
    onSelectFace,
    onStartSketch,
    onStartSketchOnFace,
    onAddSketchLine,
    onAddSketchRectangle,
    onAddSketchCircle,
    onSelectSketchEntity,
    onPickSketchPoint,
    onSelectSketchDimension,
    onUpdateSketchDimension,
    onSelectSketchProfile,
    onSetSketchTool,
    onFinishSketch,
    armedSketchConstraint,
    onCancelSketchConstraint,
    onClearSketchConstraint,
  ]);

  useEffect(() => {
    activeSketchToolRef.current = activeSketchTool;
    sketchSnapCandidatesRef.current = sketchSnapCandidates;
  }, [activeSketchTool, sketchSnapCandidates]);

  useEffect(() => {
    selectedSketchDimensionRef.current = selectedSketchDimension;
  }, [selectedSketchDimension]);

  useEffect(() => {
    if (selectedSketchDimensionValue === null) {
      setDimensionDraftValue("");
      return;
    }

    setDimensionDraftValue(String(selectedSketchDimensionValue));
  }, [selectedSketchDimensionValue, document?.selected_sketch_dimension_id]);

  useEffect(() => {
    if (!selectedSketchDimension) {
      return;
    }

    const input = dimensionInputRef.current;
    if (!input) {
      return;
    }

    input.focus();
    input.select();
  }, [selectedSketchDimension?.dimensionId]);

  useEffect(() => {
    const host = hostRef.current;
    const canvas = canvasRef.current;

    if (!host || !canvas) {
      return undefined;
    }

    const renderer = new THREE.WebGLRenderer({
      canvas,
      antialias: true,
      alpha: true,
    });
    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(46, 1, 0.1, 10000);
    const controls = new OrbitControls(camera, renderer.domElement);
    const contentGroup = new THREE.Group();
    const referenceGroup = new THREE.Group();
    const sketchGroup = new THREE.Group();
    const raycaster = new THREE.Raycaster();
    const pointer = new THREE.Vector2();
    let pointerDown: { x: number; y: number } | null = null;
    let frameId = 0;

    rendererRef.current = renderer;
    sceneRef.current = scene;
    cameraRef.current = camera;
    controlsRef.current = controls;
    contentGroupRef.current = contentGroup;
    referenceGroupRef.current = referenceGroup;
    sketchGroupRef.current = sketchGroup;

    renderer.setPixelRatio(window.devicePixelRatio);
    scene.add(contentGroup);
    scene.add(referenceGroup);
    scene.add(sketchGroup);
    scene.add(
      new THREE.AmbientLight(
        themeColor("--color-primary-soft", "#8defff"),
        1.15,
      ),
    );

    const keyLight = new THREE.DirectionalLight(
      themeColor("--color-primary-edge-active", "#d8fbff"),
      1.35,
    );
    keyLight.position.set(1.2, 1.8, 1.4);
    scene.add(keyLight);

    const rimLight = new THREE.DirectionalLight(
      themeColor("--color-primary-fixed-dim", "#00d8f1"),
      0.8,
    );
    rimLight.position.set(-1.5, 0.8, -1.1);
    scene.add(rimLight);

    controls.enableDamping = true;
    controls.dampingFactor = 0.08;
    controls.screenSpacePanning = true;
    controls.minDistance = 24;
    controls.maxDistance = 6000;
    controls.mouseButtons.LEFT = null;
    controls.mouseButtons.MIDDLE = THREE.MOUSE.PAN;
    controls.mouseButtons.RIGHT = null;
    controls.addEventListener("start", () => {
      canvas.classList.add("cad-viewport-canvas-dragging");
    });
    controls.addEventListener("end", () => {
      canvas.classList.remove("cad-viewport-canvas-dragging");
    });

    function resizeRenderer() {
      const width = Math.max(host?.clientWidth ?? 0, 1);
      const height = Math.max(host?.clientHeight ?? 0, 1);
      renderer.setSize(width, height, false);
      camera.aspect = width / height;
      camera.updateProjectionMatrix();
    }

    function render() {
      controls.update();
      renderer.render(scene, camera);

      const editor = dimensionEditorRef.current;
      const dimension = selectedSketchDimensionRef.current;
      if (!editor || !dimension) {
        if (editor) {
          editor.style.opacity = "0";
        }
        return;
      }

      const projectedPosition = projectWorldPointToViewport(
        dimension.labelPosition,
        camera,
        renderer,
      );

      if (!projectedPosition) {
        editor.style.opacity = "0";
        return;
      }

      editor.style.opacity = "1";
      editor.style.transform = `translate(${projectedPosition.x}px, ${projectedPosition.y}px) translate(-50%, -50%)`;
    }

    function intersectSceneTargets(event: PointerEvent) {
      const rect = renderer.domElement.getBoundingClientRect();
      pointer.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
      pointer.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;

      raycaster.setFromCamera(pointer, camera);
      raycaster.params.Line = { threshold: 1.75 };

      if (activeSketchPlaneId) {
        const [sketchDimensionHit] = raycaster.intersectObjects(
          sketchDimensionObjectsRef.current,
          false,
        );
        const sketchDimensionId =
          sketchDimensionHit?.object.userData.sketchDimensionId;
        if (typeof sketchDimensionId === "string") {
          return { kind: "sketch_dimension" as const, id: sketchDimensionId };
        }

        const [sketchConstraintHit] = raycaster.intersectObjects(
          sketchConstraintObjectsRef.current,
          false,
        );
        const sketchConstraintId =
          sketchConstraintHit?.object.userData.sketchConstraintId;
        if (typeof sketchConstraintId === "string") {
          return {
            kind: "sketch_constraint" as const,
            id: sketchConstraintId,
            constraintKind:
              sketchConstraintHit.object.userData.sketchConstraintKind,
            entityId:
              sketchConstraintHit.object.userData.sketchConstraintEntityId,
            relatedEntityId:
              sketchConstraintHit.object.userData
                .sketchConstraintRelatedEntityId ?? null,
          };
        }

        if (armedSketchConstraintRef.current?.kind === "coincident") {
          const [sketchPointHit] = raycaster.intersectObjects(
            sketchPointObjectsRef.current,
            false,
          );
          const sketchPointId = sketchPointHit?.object.userData.sketchPointId;
          if (typeof sketchPointId === "string") {
            return {
              kind: "sketch_point" as const,
              id: sketchPointId,
              entityId: sketchPointHit.object.userData.sketchPointEntityId,
              pointKind: sketchPointHit.object.userData.sketchPointKind,
            };
          }
        }

        const [sketchEntityHit] = raycaster.intersectObjects(
          sketchEntityObjectsRef.current,
          false,
        );
        const sketchEntityId = sketchEntityHit?.object.userData.sketchEntityId;
        const sketchEntityKind =
          sketchEntityHit?.object.userData.sketchEntityKind;
        if (typeof sketchEntityId === "string") {
          return {
            kind: "sketch_entity" as const,
            id: sketchEntityId,
            entityKind:
              typeof sketchEntityKind === "string" ? sketchEntityKind : null,
          };
        }

        const [profileHit] = raycaster.intersectObjects(
          sketchProfileMeshesRef.current,
          false,
        );
        const profileId = profileHit?.object.userData.sketchProfileId;
        if (typeof profileId === "string") {
          return { kind: "sketch_profile" as const, id: profileId };
        }
      }

      const [referenceHit] = raycaster.intersectObjects(
        referencePlaneMeshesRef.current,
        false,
      );
      const referenceId = referenceHit?.object.userData.referenceId;
      if (typeof referenceId === "string") {
        return { kind: "reference" as const, id: referenceId };
      }

      const [faceHit] = raycaster.intersectObjects(
        faceMeshesRef.current,
        false,
      );
      const faceId = faceHit?.object.userData.faceId;
      if (typeof faceId === "string") {
        return { kind: "face" as const, id: faceId };
      }

      const [primitiveHit] = raycaster.intersectObjects(
        meshesRef.current,
        false,
      );
      const primitiveId = primitiveHit?.object.userData.primitiveId;
      return typeof primitiveId === "string"
        ? { kind: "primitive" as const, id: primitiveId }
        : null;
    }

    function handlePointerDown(event: PointerEvent) {
      setContextMenu(null);

      if (event.button === 1) {
        controls.mouseButtons.MIDDLE =
          event.ctrlKey || event.metaKey ? THREE.MOUSE.ROTATE : THREE.MOUSE.PAN;
        return;
      }

      if (event.button !== 0) {
        pointerDown = null;
        return;
      }

      pointerDown = { x: event.clientX, y: event.clientY };
    }

    function handlePointerMove(event: PointerEvent) {
      if (activeSketchPlaneId) {
        if (activeSketchToolRef.current === "select") {
          clearPreviewLine();
          clearPreviewCircle();
          setSketchSnapLabel(null);
          return;
        }

        const draftStart = lineDraftStartRef.current;
          const rawPoint = resolveSketchPlanePoint(
          event,
          renderer,
          camera,
          activeSketchPlaneId,
          activeSketchPlaneFrame,
        );
        if (!rawPoint) {
          return;
        }

        const sketchPoint = resolveSnappedSketchPoint(rawPoint);
        setSketchSnapLabel(sketchPoint.snapLabel);

        if (!draftStart) {
          const hit = intersectSceneTargets(event);
          setHoveredPrimitive(null);
          setHoveredReference(null);
          return;
        }

        const sketchGroupRefValue = sketchGroupRef.current;
        if (!sketchGroupRefValue) {
          return;
        }

        clearPreviewLine();
        clearPreviewCircle();
        if (activeSketchToolRef.current === "circle") {
          const radius = distanceBetweenPoints(draftStart, sketchPoint.local);
          if (radius > 0.001) {
            const preview = buildSketchCircleObject({
              circleId: "preview-circle",
              planeId: activeSketchPlaneId,
              center: toWorldPoint(
                activeSketchPlaneId,
                draftStart,
                activeSketchPlaneFrame,
              ),
              radius,
              isSelected: false,
            });
            previewCircleRef.current = preview;
            sketchGroupRefValue.add(preview);
          }
        } else {
          const preview = new THREE.Line(
            new THREE.BufferGeometry().setFromPoints([
              new THREE.Vector3(
                ...toWorldPoint(
                  activeSketchPlaneId,
                  draftStart,
                  activeSketchPlaneFrame,
                ),
              ),
              new THREE.Vector3(...sketchPoint.world),
            ]),
            new THREE.LineBasicMaterial({
              color: themeColor("--color-tertiary-plane-edge", "#ffe784"),
              transparent: true,
              opacity: 0.88,
            }),
          );
          previewLineRef.current = preview;
          sketchGroupRefValue.add(preview);
        }
        return;
      }

      const hit = intersectSceneTargets(event);
      if (hit?.kind === "sketch_dimension" || hit?.kind === "sketch_entity") {
        setHoveredReference(null);
        setHoveredPrimitive(null);
        return;
      }
      setHoveredReference(hit?.kind === "reference" ? hit.id : null);
      setHoveredPrimitive(hit?.kind === "primitive" ? hit.id : null);
    }

    function handlePointerLeave() {
      pointerDown = null;
      setSketchSnapLabel(null);
      if (!activeSketchPlaneId) {
        setHoveredReference(null);
        setHoveredPrimitive(null);
      }
    }

    function handlePointerUp(event: PointerEvent) {
      if (event.button === 1) {
        controls.mouseButtons.MIDDLE = THREE.MOUSE.PAN;
        pointerDown = null;
        return;
      }

      if (event.button !== 0) {
        pointerDown = null;
        return;
      }

      if (!pointerDown) {
        return;
      }

      const deltaX = Math.abs(event.clientX - pointerDown.x);
      const deltaY = Math.abs(event.clientY - pointerDown.y);
      pointerDown = null;

      if (deltaX > 4 || deltaY > 4) {
        return;
      }

      if (activeSketchPlaneId) {
        const hit = intersectSceneTargets(event);
        if (activeSketchToolRef.current === "select") {
          if (
            armedSketchConstraintRef.current &&
            hit?.kind === "sketch_entity" &&
            hit.entityKind === "line"
          ) {
            void selectSketchEntityRef.current(hit.id);
            return;
          }

          if (hit?.kind === "sketch_point") {
            void pickSketchPointRef.current(
              hit.id,
              hit.entityId,
              hit.pointKind,
            );
            return;
          }

          if (hit?.kind === "sketch_dimension") {
            void selectSketchDimensionRef.current(hit.id);
            return;
          }

          if (hit?.kind === "sketch_constraint") {
            void clearSketchConstraintRef.current(
              hit.constraintKind,
              hit.entityId,
              hit.relatedEntityId,
            );
            return;
          }

          if (hit?.kind === "sketch_profile") {
            void selectSketchProfileRef.current(hit.id);
            return;
          }

          if (hit?.kind === "sketch_entity") {
            void selectSketchEntityRef.current(hit.id);
          }
          return;
        }

        if (!lineDraftStartRef.current && hit?.kind === "sketch_dimension") {
          void selectSketchDimensionRef.current(hit.id);
          return;
        }

        if (!lineDraftStartRef.current && hit?.kind === "sketch_entity") {
          void selectSketchEntityRef.current(hit.id);
          return;
        }

        const rawPoint = resolveSketchPlanePoint(
          event,
          renderer,
          camera,
          activeSketchPlaneId,
          activeSketchPlaneFrame,
        );
        if (!rawPoint) {
          return;
        }
        const sketchPoint = resolveSnappedSketchPoint(rawPoint);
        setSketchSnapLabel(sketchPoint.snapLabel);

        if (!lineDraftStartRef.current) {
          lineDraftStartRef.current = sketchPoint.local;
          return;
        }

        const [startX, startY] = lineDraftStartRef.current;
        clearPreviewLine();
        clearPreviewCircle();
        if (activeSketchToolRef.current === "rectangle") {
          lineDraftStartRef.current = null;
          void addSketchRectangleRef.current(
            startX,
            startY,
            sketchPoint.local[0],
            sketchPoint.local[1],
          );
          return;
        }

        if (activeSketchToolRef.current === "circle") {
          lineDraftStartRef.current = null;
          const radius = distanceBetweenPoints(
            [startX, startY],
            sketchPoint.local,
          );
          void addSketchCircleRef.current(startX, startY, radius);
          return;
        }

        lineDraftStartRef.current = sketchPoint.local;
        void addSketchLineRef.current(
          startX,
          startY,
          sketchPoint.local[0],
          sketchPoint.local[1],
        );
        return;
      }

      const hit = intersectSceneTargets(event);
      if (hit?.kind === "reference") {
        void selectReferenceRef.current(hit.id);
        return;
      }

      if (hit?.kind === "face") {
        void selectFaceRef.current(hit.id);
        return;
      }

      if (hit?.kind === "primitive") {
        void selectPrimitiveRef.current(hit.id);
      }
    }

    function handleContextMenu(event: MouseEvent) {
      event.preventDefault();

      if (activeSketchPlaneId) {
        setContextMenu(null);
        return;
      }

      const hit = intersectSceneTargets(event as PointerEvent);
      if (hit?.kind !== "reference" && hit?.kind !== "face") {
        setContextMenu(null);
        return;
      }

      const rect = renderer.domElement.getBoundingClientRect();
      setContextMenu({
        x: event.clientX - rect.left,
        y: event.clientY - rect.top,
        referenceId: hit.kind === "reference" ? hit.id : null,
        faceId: hit.kind === "face" ? hit.id : null,
      });
    }

    const resizeObserver = new ResizeObserver(() => {
      resizeRenderer();
      render();
    });

    resizeObserver.observe(host);
    renderer.domElement.addEventListener("pointerdown", handlePointerDown);
    renderer.domElement.addEventListener("pointermove", handlePointerMove);
    renderer.domElement.addEventListener("pointerleave", handlePointerLeave);
    renderer.domElement.addEventListener("pointerup", handlePointerUp);
    renderer.domElement.addEventListener("contextmenu", handleContextMenu);
    resizeRenderer();

    const animate = () => {
      render();
      frameId = window.requestAnimationFrame(animate);
    };

    frameId = window.requestAnimationFrame(animate);

    return () => {
      window.cancelAnimationFrame(frameId);
      resizeObserver.disconnect();
      renderer.domElement.removeEventListener("pointerdown", handlePointerDown);
      renderer.domElement.removeEventListener("pointermove", handlePointerMove);
      renderer.domElement.removeEventListener(
        "pointerleave",
        handlePointerLeave,
      );
      renderer.domElement.removeEventListener("pointerup", handlePointerUp);
      renderer.domElement.removeEventListener("contextmenu", handleContextMenu);
      controls.dispose();
      disposeGroup(contentGroup);
      disposeGroup(referenceGroup);
      disposeGroup(sketchGroup);
      renderer.dispose();
      gridRef.current?.geometry.dispose();
      disposeMaterial(gridRef.current?.material ?? []);
      rendererRef.current = null;
      sceneRef.current = null;
      cameraRef.current = null;
      controlsRef.current = null;
      contentGroupRef.current = null;
      referenceGroupRef.current = null;
      sketchGroupRef.current = null;
      primitiveVisualsRef.current.clear();
      primitiveStatesRef.current.clear();
      referencePlaneVisualsRef.current.clear();
      referencePlaneStatesRef.current.clear();
      referencePlaneMeshesRef.current = [];
      sketchEntityObjectsRef.current = [];
      sketchDimensionObjectsRef.current = [];
      sketchConstraintObjectsRef.current = [];
      sketchPointObjectsRef.current = [];
      sketchProfileMeshesRef.current = [];
      meshesRef.current = [];
      faceMeshesRef.current = [];
      gridRef.current = null;
      previewLineRef.current = null;
      previewCircleRef.current = null;
      lineDraftStartRef.current = null;
      lastGeometryKeyRef.current = "";
    };
  }, [activeSketchPlaneId]);

  useEffect(() => {
    const scene = sceneRef.current;
    const camera = cameraRef.current;
    const controls = controlsRef.current;
    const contentGroup = contentGroupRef.current;
    const referenceGroup = referenceGroupRef.current;
    const sketchGroup = sketchGroupRef.current;

    if (
      !scene ||
      !camera ||
      !controls ||
      !contentGroup ||
      !referenceGroup ||
      !sketchGroup
    ) {
      return;
    }

    disposeGroup(contentGroup);
    disposeGroup(referenceGroup);
    disposeGroup(sketchGroup);
    primitiveVisualsRef.current.clear();
    primitiveStatesRef.current.clear();
    referencePlaneVisualsRef.current.clear();
    referencePlaneStatesRef.current.clear();
    referencePlaneMeshesRef.current = [];
    sketchEntityObjectsRef.current = [];
    sketchDimensionObjectsRef.current = [];
    sketchConstraintObjectsRef.current = [];
    sketchPointObjectsRef.current = [];
    sketchProfileMeshesRef.current = [];
    meshesRef.current = [];
    faceMeshesRef.current = [];
    previewLineRef.current = null;
    previewCircleRef.current = null;

    if (gridRef.current) {
      scene.remove(gridRef.current);
      gridRef.current.geometry.dispose();
      disposeMaterial(gridRef.current.material);
    }

    gridRef.current = null;

    if (!sceneData) {
      lastGeometryKeyRef.current = "";
      return;
    }

    const nextGrid = new THREE.GridHelper(
      Math.max(sceneData.bounds.maxDimension * 2, 200),
      20,
      themeColor("--color-primary-fixed-dim", "#00d8f1"),
      themeColor("--color-primary-emissive-hover", "#214147"),
    );
    nextGrid.position.set(
      sceneData.bounds.center[0],
      0,
      sceneData.bounds.center[2],
    );
    scene.add(nextGrid);
    gridRef.current = nextGrid;

    for (const primitive of sceneData.primitives) {
      const object = buildPrimitiveObject(primitive);
      meshesRef.current.push(object.mesh);
      primitiveVisualsRef.current.set(primitive.primitiveId, object.visual);
      primitiveStatesRef.current.set(primitive.primitiveId, {
        isSelected: primitive.isSelected,
        isHovered: false,
      });
      contentGroup.add(object.mesh);
      contentGroup.add(object.edges);
    }

    for (const reference of sceneData.references) {
      if (reference.kind === "reference_plane") {
        if (!showReferencePlanes) {
          continue;
        }

        const object = buildReferencePlaneObject(reference);
        referencePlaneMeshesRef.current.push(object.mesh);
        referencePlaneVisualsRef.current.set(
          reference.referenceId,
          object.visual,
        );
        referencePlaneStatesRef.current.set(reference.referenceId, {
          isSelected: reference.isSelected,
          isHovered: false,
          isActiveSketchPlane: reference.isActiveSketchPlane,
        });
        referenceGroup.add(object.mesh);
        referenceGroup.add(object.edges);
        continue;
      }

      const axisObject = buildReferenceAxisObject(reference);
      referenceGroup.add(axisObject.line);
    }

    for (const face of sceneData.solidFaces) {
      const faceObject = buildSolidFaceObject(face);
      faceMeshesRef.current.push(faceObject);
      contentGroup.add(faceObject);
    }

    for (const sketchLine of sceneData.sketchLines) {
      const sketchLineObject = buildSketchLineObject(sketchLine);
      sketchEntityObjectsRef.current.push(sketchLineObject);
      sketchGroup.add(sketchLineObject);
    }

    for (const sketchCircle of sceneData.sketchCircles) {
      const sketchCircleObject = buildSketchCircleObject(sketchCircle);
      sketchEntityObjectsRef.current.push(sketchCircleObject);
      sketchGroup.add(sketchCircleObject);
    }

    for (const sketchDimension of sceneData.sketchDimensions) {
      const sketchDimensionObject = buildSketchDimensionObject(sketchDimension);
      sketchDimensionObjectsRef.current.push(sketchDimensionObject.line);
      sketchDimensionObjectsRef.current.push(sketchDimensionObject.label);
      sketchGroup.add(sketchDimensionObject.line);
      sketchGroup.add(sketchDimensionObject.label);
    }

    for (const sketchConstraint of sceneData.sketchConstraints) {
      const sketchConstraintObject =
        buildSketchConstraintObject(sketchConstraint);
      sketchConstraintObjectsRef.current.push(sketchConstraintObject);
      sketchGroup.add(sketchConstraintObject);
    }

    for (const sketchProfile of sceneData.sketchProfiles) {
      const sketchProfileMesh = buildSketchProfileObject(sketchProfile);
      sketchProfileMeshesRef.current.push(sketchProfileMesh);
      sketchGroup.add(sketchProfileMesh);
    }

    for (const sketchPoint of sceneData.sketchPoints) {
      const sketchPointObject = buildSketchPointObject(sketchPoint);
      sketchPointObjectsRef.current.push(sketchPointObject);
      sketchGroup.add(sketchPointObject);
    }

    syncPrimitiveVisuals();
    syncReferencePlaneVisuals();

    if (sceneData.geometryKey !== lastGeometryKeyRef.current) {
      if (!activeSketchPlaneId) {
        frameCamera(
          camera,
          controls,
          sceneData.bounds.center,
          sceneData.bounds.maxDimension,
        );
      }

      lastGeometryKeyRef.current = sceneData.geometryKey;
    }
  }, [sceneData, showReferencePlanes]);

  useEffect(() => {
    lineDraftStartRef.current = null;
    clearPreviewLine();
    clearPreviewCircle();
    setSketchSnapLabel(null);
  }, [activeSketchPlaneId, activeSketchTool]);

  useEffect(() => {
    if (!activeSketchPlaneId) {
      return undefined;
    }

    function handleKeyDown(event: KeyboardEvent) {
      const target = event.target;
      if (
        target instanceof HTMLInputElement ||
        target instanceof HTMLTextAreaElement ||
        target instanceof HTMLSelectElement ||
        (target instanceof HTMLElement && target.isContentEditable)
      ) {
        return;
      }

      if (event.code === "Escape") {
        event.preventDefault();
        if (armedSketchConstraintRef.current) {
          cancelSketchConstraintRef.current();
          return;
        }
        lineDraftStartRef.current = null;
        clearPreviewLine();
        clearPreviewCircle();
        setSketchSnapLabel(null);
        void setSketchToolRef.current("select");
        return;
      }

      if (event.ctrlKey || event.metaKey || event.altKey || event.shiftKey) {
        return;
      }

      if (event.code === "KeyL") {
        event.preventDefault();
        void setSketchToolRef.current("line");
        return;
      }

      if (event.code === "KeyR") {
        event.preventDefault();
        void setSketchToolRef.current("rectangle");
        return;
      }

      if (event.code === "KeyC") {
        event.preventDefault();
        void setSketchToolRef.current("circle");
      }
    }

    window.addEventListener("keydown", handleKeyDown);
    return () => {
      window.removeEventListener("keydown", handleKeyDown);
    };
  }, [activeSketchPlaneId]);

  useEffect(() => {
    if (activeSketchPlaneId) {
      if (previousReferencePlaneVisibilityRef.current === null) {
        previousReferencePlaneVisibilityRef.current = showReferencePlanes;
      }

      if (showReferencePlanes) {
        setShowReferencePlanes(false);
      }
      return;
    }

    if (previousReferencePlaneVisibilityRef.current !== null) {
      setShowReferencePlanes(previousReferencePlaneVisibilityRef.current);
      previousReferencePlaneVisibilityRef.current = null;
    }
  }, [activeSketchPlaneId, showReferencePlanes]);

  useEffect(() => {
    const camera = cameraRef.current;
    const controls = controlsRef.current;

    if (!camera || !controls || !sceneData || !activeSketchPlaneId) {
      return;
    }

    frameCameraToSketchPlane(
      camera,
      controls,
      activeSketchPlaneId,
      activeSketchPlaneFrame,
      sceneData.bounds.maxDimension,
    );
  }, [activeSketchPlaneId, activeSketchPlaneFrame, sceneData]);

  function handleFinishSketch() {
    lineDraftStartRef.current = null;
    clearPreviewLine();
    clearPreviewCircle();
    void finishSketchRef.current();
  }

  async function handleCreateSketchFromContextMenu() {
    if (contextMenu?.referenceId) {
      setContextMenu(null);
      await selectReferenceRef.current(contextMenu.referenceId);
      await startSketchRef.current(contextMenu.referenceId);
      return;
    }

    if (!contextMenu?.faceId) {
      return;
    }

    setContextMenu(null);
    await selectFaceRef.current(contextMenu.faceId);

    const solidFace = sceneData?.solidFaces.find(
      (face) => face.faceId === contextMenu.faceId,
    );
    if (!solidFace) {
      return;
    }

    await startSketchOnFaceRef.current(
      solidFace.faceId,
      solidFace.planeFrame,
    );
  }

  const lineCount = sketchFeature?.sketch_parameters?.lines.length ?? 0;
  const circleCount = sketchFeature?.sketch_parameters?.circles.length ?? 0;

  async function handleSubmitDimensionEdit() {
    if (!selectedSketchDimension) {
      return;
    }

    const nextValue = Number(dimensionDraftValue);
    if (!Number.isFinite(nextValue) || nextValue <= 0) {
      return;
    }

    await updateSketchDimensionRef.current(
      selectedSketchDimension.dimensionId,
      nextValue,
    );
  }

  const selectedSketchDimensionTitle = selectedSketchDimension
    ? selectedSketchDimension.kind === "line_length"
      ? "Length"
      : "Radius"
    : null;

  return (
    <section className="relative flex h-full min-h-0 flex-col overflow-hidden">
      <div className="pointer-events-none absolute inset-0 cad-grid-stage opacity-70" />
      <div className="pointer-events-none absolute left-4 top-4 z-10">
        <button
          type="button"
          className={
            showReferencePlanes
              ? "pointer-events-auto cad-tool-button cad-tool-button-active"
              : "pointer-events-auto cad-tool-button"
          }
          onClick={() => {
            setShowReferencePlanes((current) => !current);
          }}
        >
          {showReferencePlanes ? "Hide Origin Planes" : "Show Origin Planes"}
        </button>
      </div>
      <div
        ref={hostRef}
        className="absolute inset-0 min-h-0 min-w-0 overflow-hidden rounded-[18px]"
      >
        {contextMenu ? (
          <div
            className="cad-context-menu absolute z-20 min-w-[160px] rounded-2xl p-1.5 backdrop-blur-xl"
            style={{
              left: contextMenu.x,
              top: contextMenu.y,
              transform: "translate(8px, 8px)",
            }}
          >
            <button
              type="button"
              className="cad-context-menu-item flex w-full items-center justify-start rounded-xl px-3 py-2 text-sm text-on-surface transition-colors duration-200"
              onClick={handleCreateSketchFromContextMenu}
            >
              Create Sketch
            </button>
          </div>
        ) : null}
        <canvas
          ref={canvasRef}
          className={`cad-viewport-canvas absolute inset-0 h-full w-full ${
            activeSketchPlaneId && activeSketchTool !== "select"
              ? "cad-viewport-canvas-drawing"
              : ""
          }`}
        />
        {selectedSketchDimension && activeSketchPlaneId ? (
          <form
            ref={dimensionEditorRef}
            className="pointer-events-auto absolute z-20 flex min-w-[188px] items-center gap-2 rounded-2xl border border-white/15 bg-black/75 px-3 py-2 backdrop-blur-xl"
            style={{
              left: 0,
              top: 0,
              opacity: 0,
            }}
            onSubmit={(event) => {
              event.preventDefault();
              void handleSubmitDimensionEdit();
            }}
          >
            <span className="shrink-0 text-[11px] uppercase tracking-[0.16em] text-on-surface-dim">
              {selectedSketchDimensionTitle}
            </span>
            <input
              ref={dimensionInputRef}
              className="cad-input h-9 min-w-0 flex-1"
              type="number"
              min="0.01"
              step="0.01"
              value={dimensionDraftValue}
              onChange={(event) => {
                setDimensionDraftValue(event.target.value);
              }}
              onFocus={(event) => {
                event.currentTarget.select();
              }}
              onKeyDown={(event) => {
                if (event.key !== "Escape") {
                  return;
                }

                event.preventDefault();
                setDimensionDraftValue(
                  selectedSketchDimensionValue !== null
                    ? String(selectedSketchDimensionValue)
                    : "",
                );
                event.currentTarget.blur();
              }}
            />
            <span className="shrink-0 text-[11px] uppercase tracking-[0.16em] text-on-surface-dim">
              mm
            </span>
            <button
              type="submit"
              className="cad-action-primary shrink-0"
              disabled={Number(dimensionDraftValue) <= 0}
            >
              Set
            </button>
          </form>
        ) : null}
        {!hasActiveDocument ? (
          <div
            className="absolute inset-0 flex items-center justify-center backdrop-blur-sm"
            style={{ background: "var(--cad-overlay-strong)" }}
          >
            <div className="text-center">
              <p className="cad-kicker">Viewport</p>
              <p className="mt-4 text-sm text-on-surface-muted">
                No active document to render.
              </p>
            </div>
          </div>
        ) : null}
        {status === "starting" ? (
          <div
            className="pointer-events-none absolute inset-0 flex items-center justify-center backdrop-blur-sm"
            style={{ background: "var(--cad-overlay-soft)" }}
          >
            <div className="cad-floating-panel flex min-w-[220px] items-center gap-4 px-5 py-4">
              <span className="cad-loader-spinner" aria-hidden="true" />
              <div>
                <p className="cad-kicker">Core Startup</p>
                <p className="mt-2 text-sm text-on-surface-muted">
                  Starting the native CAD core...
                </p>
              </div>
            </div>
          </div>
        ) : null}
        {hasActiveDocument ? (
          <>
            <div className="pointer-events-none absolute bottom-4 right-4 cad-floating-panel px-4 py-3 text-right">
              <p className="text-[11px] uppercase tracking-[0.18em] text-on-surface-dim">
                Selection
              </p>
              <p className="mt-1 text-sm text-on-surface-muted">
                {selectedReference?.label ??
                  selectedPrimitiveLabel ??
                  "No selection"}
              </p>
              <p className="mt-1 text-xs uppercase tracking-[0.14em] text-on-surface-dim">
                {activeSketchPlaneId
                  ? `${activeSketchPlaneId} · ${activeSketchTool} · ${lineCount} line${lineCount === 1 ? "" : "s"} · ${circleCount} circle${circleCount === 1 ? "" : "s"}`
                  : "No active sketch"}
              </p>
              {activeSketchPlaneId ? (
                <p className="mt-1 text-xs text-on-surface-dim">
                  {armedSketchConstraint
                    ? armedSketchConstraint.kind === "coincident"
                      ? armedSketchConstraint.firstPointId
                        ? `Coincident armed · first ${armedSketchConstraint.firstPointId} · click second point`
                        : "Coincident armed · click first point"
                      : armedSketchConstraint.kind === "equal_length" ||
                          armedSketchConstraint.kind === "perpendicular" ||
                          armedSketchConstraint.kind === "parallel"
                        ? armedSketchConstraint.firstLineId
                          ? `${armedSketchConstraint.kind === "equal_length" ? "Equal length" : armedSketchConstraint.kind === "perpendicular" ? "Perpendicular" : "Parallel"} armed · first ${armedSketchConstraint.firstLineId} · click second line`
                          : `${armedSketchConstraint.kind === "equal_length" ? "Equal length" : armedSketchConstraint.kind === "perpendicular" ? "Perpendicular" : "Parallel"} armed · click first line`
                        : `${armedSketchConstraint.kind} constraint armed · click a line`
                    : document?.selected_sketch_entity_id
                      ? document?.selected_sketch_dimension_id
                        ? `Dimension: ${document.selected_sketch_dimension_id} · Entity: ${document.selected_sketch_entity_id}`
                        : `Entity: ${document.selected_sketch_entity_id}`
                      : document?.selected_sketch_profile_id
                        ? `Profile: ${document.selected_sketch_profile_id}`
                        : sketchSnapLabel
                          ? `Snap: ${sketchSnapLabel}`
                          : activeSketchTool === "select"
                            ? "Selection mode · press a sketch tool to draw"
                            : activeSketchTool === "line" &&
                                lineDraftStartRef.current
                              ? "Line chain active · click to continue or press Escape"
                              : "Click to place geometry"}
                </p>
              ) : null}
            </div>
          </>
        ) : null}
      </div>
    </section>
  );
}
