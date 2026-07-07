import { reformatDimensionLabel } from "../units";

import {
  SketchArcScene,
  SketchCircleScene,
  SketchConstraintScene,
  SketchDimensionScene,
  SketchLineScene,
  SketchPlaneFrame,
  SketchVertexScene,
  SketchPolygonScene,
  SketchProfileScene,
} from "@/types";
import * as THREE from "three";

import {
  buildFilledArrowMesh,
  buildSketchDimensionGeometry,
} from "./dimensionGeometry";
import {
  makePlaneTransformMatrix,
  makePlaneTransformMatrixFromFrame,
  shapeFromProfileLoops,
} from "./primitiveObjects";
import { themeColor } from "./themeColor";
import { polygonArea2d, SKETCH_PLANE_OFFSET } from "./viewportMath";

function configureSketchOverlayMaterial(material: THREE.Material) {
  material.depthTest = false;
  material.depthWrite = false;
}

function resolveSketchPlaneAxes(
  planeId: string,
  planeFrame: SketchPlaneFrame | null,
): {
  xAxis: [number, number, number];
  yAxis: [number, number, number];
} {
  if (planeFrame) {
    return {
      xAxis: [planeFrame.x_axis.x, planeFrame.x_axis.y, planeFrame.x_axis.z],
      yAxis: [planeFrame.y_axis.x, planeFrame.y_axis.y, planeFrame.y_axis.z],
    };
  }

  if (planeId === "ref-plane-xy") {
    return {
      xAxis: [1, 0, 0],
      yAxis: [0, 0, 1],
    };
  }

  if (planeId === "ref-plane-yz") {
    return {
      xAxis: [0, 1, 0],
      yAxis: [0, 0, 1],
    };
  }

  return {
    xAxis: [1, 0, 0],
    yAxis: [0, 1, 0],
  };
}

function fallbackCanvasSprite(canvas: HTMLCanvasElement) {
  const texture = new THREE.CanvasTexture(canvas);
  return new THREE.Sprite(
    new THREE.SpriteMaterial({ map: texture, transparent: true }),
  );
}

function spriteFromCanvas({
  canvas,
  opacity = 1,
  scale,
  includeBasePosition = false,
}: {
  canvas: HTMLCanvasElement;
  opacity?: number;
  scale: [number, number, number];
  includeBasePosition?: boolean;
}) {
  const texture = new THREE.CanvasTexture(canvas);
  texture.needsUpdate = true;
  const material = new THREE.SpriteMaterial({
    map: texture,
    transparent: true,
    opacity,
    depthTest: false,
    depthWrite: false,
  });
  const sprite = new THREE.Sprite(material);
  sprite.scale.set(...scale);
  sprite.userData.screenSize = {
    width: canvas.width,
    height: canvas.height,
  };
  if (includeBasePosition) {
    sprite.userData.basePosition = null;
  }
  return sprite;
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
  // Face-based sketches use the core-provided frame; named reference
  // planes use the legacy axis mapping for compatibility.
  const { xAxis, yAxis } = resolveSketchPlaneAxes(circle.planeId, planeFrame);
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

  const { xAxis, yAxis } = resolveSketchPlaneAxes(arc.planeId, planeFrame);

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

export function buildSketchPointObject(point: SketchVertexScene) {
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

function makeDimensionLabelSprite(
  text: string,
  isSelected: boolean,
  options: { variant?: "muted-preview" } = {},
) {
  const canvas = document.createElement("canvas");
  const context = canvas.getContext("2d");
  if (!context) {
    return fallbackCanvasSprite(canvas);
  }

  const fontSize = 26;
  context.font = `600 ${fontSize}px "Space Grotesk", sans-serif`;
  const textWidth = Math.ceil(context.measureText(text).width);
  canvas.width = textWidth + 32;
  canvas.height = 50;

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

  return spriteFromCanvas({
    canvas,
    opacity: isMutedPreview ? 0.72 : 1,
    scale: [canvas.width / 9, canvas.height / 9, 1],
    includeBasePosition: true,
  });
}

function makeConstraintBadgeSprite(text: string, isSelected: boolean) {
  const canvas = document.createElement("canvas");
  const context = canvas.getContext("2d");
  if (!context) {
    return fallbackCanvasSprite(canvas);
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

  return spriteFromCanvas({
    canvas,
    scale: [6, 6, 1],
  });
}

export function buildSketchDimensionObject(
  dimension: SketchDimensionScene,
  displayUnits?: "mm" | "in",
  options: { variant?: "muted-preview"; pickable?: boolean } = {},
) {
  const isMutedPreview = options.variant === "muted-preview";
  const isPickable = options.pickable ?? !isMutedPreview;
  let labelText =
    displayUnits === "in" && dimension.unitSuffix === "mm"
      ? reformatDimensionLabel(dimension.label, dimension.kind, "in")
      : dimension.label;
  // Driven (reference) dimensions: wrap in parentheses like "(35mm)".
  // Strip any existing parenthesization from the C++ label first
  // so the TS driven flag is the single source of truth.
  if (labelText.startsWith("(") && labelText.endsWith(")")) {
    labelText = labelText.slice(1, -1);
  }
  if (dimension.driven) {
    labelText = `(${labelText})`;
  }
  const labelPosition = new THREE.Vector3(...dimension.labelPosition);
  const { points, arrowPositions, arrowIndices, refLineData } =
    buildSketchDimensionGeometry(dimension);
  const dimensionColor = isMutedPreview
    ? themeColor("--color-on-surface-muted", "#9b9b98")
    : dimension.isSelected
      ? themeColor("--color-primary-edge-active", "#c3f5ff")
      : themeColor("--color-primary-soft", "#8feaf7");
  const geometryUserData = isPickable
    ? {
        sketchDimensionId: dimension.dimensionId,
        sketchDimensionPart: "geometry",
      }
    : undefined;

  // Build line segments geometry
  const material = new THREE.LineBasicMaterial({
    color: dimensionColor,
    transparent: true,
    opacity: isMutedPreview ? 0.34 : dimension.isSelected ? 0.98 : 0.84,
    depthTest: false,
  });
  const geometry = new THREE.BufferGeometry().setFromPoints(points);
  const lineSegments = new THREE.LineSegments(geometry, material);
  lineSegments.renderOrder = isMutedPreview ? 5 : 6;
  if (geometryUserData) {
    Object.assign(lineSegments.userData, geometryUserData);
  }

  // Build filled arrow mesh
  const group = new THREE.Group();
  group.add(lineSegments);
  if (geometryUserData) {
    Object.assign(group.userData, geometryUserData);
  }

  const arrowMesh = buildFilledArrowMesh({
    arrowPositions,
    arrowIndices,
    color: dimensionColor,
    opacity: isMutedPreview ? 0.34 : dimension.isSelected ? 0.98 : 0.84,
    renderOrder: isMutedPreview ? 5 : 6,
    userData: geometryUserData,
  });
  if (arrowMesh) {
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
  // Driven (reference) constraints: wrap label in parentheses like "(V)".
  const labelText = constraint.driven ? `(${constraint.label})` : constraint.label;
  const badge = makeConstraintBadgeSprite(
    labelText,
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

  const shape = shapeFromProfileLoops(profile.profilePoints, profile.innerLoops);

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
    polygonArea2d(profile.profilePoints) -
    profile.innerLoops.reduce(
      (sum, loop) => sum + polygonArea2d(loop),
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
