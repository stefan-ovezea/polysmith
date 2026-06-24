import type {
  ViewportBoxPrimitive,
  ViewportCylinderPrimitive,
  ViewportHelixPrimitive,
  ViewportMeshPrimitive,
  ViewportPolygonExtrudePrimitive,
  ViewportSolidFace,
  ViewportReferenceAxis,
  ViewportReferencePoint,
  ViewportReferencePlane,
  ViewportSketchArc,
  ViewportSketchCircle,
  ViewportSketchConstraint,
  ViewportSketchDimension,
  ViewportSketchPolygon,
  ViewportSketchProfile,
  ViewportState,
  BoxScenePrimitive,
  CylinderScenePrimitive,
  MeshScenePrimitive,
  PolygonExtrudeScenePrimitive,
  ReferencePlaneScene,
  ReferenceAxisScene,
  ReferenceHelixScene,
  ReferencePointScene,
  SketchArcScene,
  SketchCircleScene,
  SketchConstraintScene,
  SketchDimensionScene,
  SketchLineScene,
  SketchPointScene,
  SketchPolygonScene,
  SketchProfileScene,
  SolidFaceScene,
  SceneEdge,
  SceneVertex,
  CutPreviewScene,
  ViewportScene,
  DocumentState,
  FeatureEntry,
  SketchProfileRegionEntry,
} from "@/types";
import {
  pointInPolygon2d,
  polygonArea2d,
} from "@/utils/viewport/viewportMath";

function clampDimension(value: number) {
  return Math.max(value, 1);
}

function numericBufferSignature(values: Float32Array | Uint32Array) {
  let hash = 2166136261;
  for (const value of values) {
    const quantized = Math.round(value * 1000);
    hash ^= quantized;
    hash = Math.imul(hash, 16777619);
  }
  return `${values.length}:${hash >>> 0}`;
}

function makeBoxPrimitive(box: ViewportBoxPrimitive): BoxScenePrimitive {
  return {
    kind: "box",
    primitiveId: box.primitive_id,
    label: box.label,
    size: [box.width, box.height, box.depth],
    position: [box.center.x, box.center.y, box.center.z],
    isSelected: box.is_selected,
    appearanceColor: box.appearance_color ?? null,
  };
}

function makeCylinderPrimitive(
  cylinder: ViewportCylinderPrimitive,
): CylinderScenePrimitive {
  return {
    kind: "cylinder",
    primitiveId: cylinder.primitive_id,
    label: cylinder.label,
    radius: cylinder.radius,
    height: cylinder.height,
    position: [cylinder.center.x, cylinder.center.y, cylinder.center.z],
    isSelected: cylinder.is_selected,
    appearanceColor: cylinder.appearance_color ?? null,
  };
}

function makeMeshPrimitive(
  primitive: ViewportMeshPrimitive,
): MeshScenePrimitive {
  // Wire format uses plain number[] arrays. We materialize typed arrays
  // here so the renderer can hand them straight to BufferGeometry without
  // an extra copy on every frame the scene rebuilds.
  return {
    kind: "mesh",
    primitiveId: primitive.primitive_id,
    label: primitive.primitive_id,
    positions: Float32Array.from(primitive.positions),
    normals: Float32Array.from(primitive.normals),
    indices: Uint32Array.from(primitive.indices),
    isSelected: primitive.is_selected,
    appearanceColor: primitive.appearance_color ?? null,
  };
}

function makePolygonExtrudePrimitive(
  primitive: ViewportPolygonExtrudePrimitive,
): PolygonExtrudeScenePrimitive {
  return {
    kind: "polygon_extrude",
    primitiveId: primitive.primitive_id,
    label: primitive.label,
    planeId: primitive.plane_id,
    planeFrame: primitive.plane_frame
      ? {
          origin: [
            primitive.plane_frame.origin.x,
            primitive.plane_frame.origin.y,
            primitive.plane_frame.origin.z,
          ],
          xAxis: [
            primitive.plane_frame.x_axis.x,
            primitive.plane_frame.x_axis.y,
            primitive.plane_frame.x_axis.z,
          ],
          yAxis: [
            primitive.plane_frame.y_axis.x,
            primitive.plane_frame.y_axis.y,
            primitive.plane_frame.y_axis.z,
          ],
          normal: [
            primitive.plane_frame.normal.x,
            primitive.plane_frame.normal.y,
            primitive.plane_frame.normal.z,
          ],
        }
      : null,
    profilePoints: primitive.profile_points.map(
      (point) => [point.x, point.y] as [number, number],
    ),
    innerLoops: (primitive.inner_loops ?? []).map((loop) =>
      loop.map((point) => [point.x, point.y] as [number, number]),
    ),
    depth: primitive.depth,
    isSelected: primitive.is_selected,
    appearanceColor: primitive.appearance_color ?? null,
  };
}

function makeReferencePlane(
  plane: ViewportReferencePlane,
): ReferencePlaneScene {
  // Construction planes ship a real world-space frame; origin
  // planes leave it null and the renderer falls back to the
  // hardcoded `orientation` rotation.
  const planeFrame = plane.plane_frame
    ? {
        origin: [
          plane.plane_frame.origin.x,
          plane.plane_frame.origin.y,
          plane.plane_frame.origin.z,
        ] as [number, number, number],
        xAxis: [
          plane.plane_frame.x_axis.x,
          plane.plane_frame.x_axis.y,
          plane.plane_frame.x_axis.z,
        ] as [number, number, number],
        yAxis: [
          plane.plane_frame.y_axis.x,
          plane.plane_frame.y_axis.y,
          plane.plane_frame.y_axis.z,
        ] as [number, number, number],
        normal: [
          plane.plane_frame.normal.x,
          plane.plane_frame.normal.y,
          plane.plane_frame.normal.z,
        ] as [number, number, number],
      }
    : null;
  return {
    kind: "reference_plane",
    referenceId: plane.reference_id,
    label: plane.label,
    orientation: plane.orientation,
    position: [plane.center.x, plane.center.y, plane.center.z],
    size: [plane.size.width, plane.size.height],
    isSelected: plane.is_selected,
    isActiveSketchPlane: plane.is_active_sketch_plane,
    planeFrame,
  };
}

function makeReferenceAxis(axis: ViewportReferenceAxis): ReferenceAxisScene {
  return {
    kind: "reference_axis",
    referenceId: axis.reference_id,
    label: axis.label,
    axis: axis.axis,
    start: [axis.start.x, axis.start.y, axis.start.z],
    end: [axis.end.x, axis.end.y, axis.end.z],
  };
}

function makeReferencePoint(point: ViewportReferencePoint): ReferencePointScene {
  return {
    kind: "reference_point",
    referenceId: point.reference_id,
    label: point.label,
    position: [point.position.x, point.position.y, point.position.z],
    isSelected: point.is_selected,
  };
}

function makeReferenceHelix(helix: ViewportHelixPrimitive): ReferenceHelixScene {
  return {
    kind: "reference_helix",
    referenceId: helix.helix_id,
    label: helix.label,
    points: new Float32Array(helix.points),
    isSelected: helix.is_selected,
  };
}

function makeSketchCircle(
  circle: ViewportSketchCircle,
  projectedCircleIds: Set<string>,
): SketchCircleScene {
  return {
    isPreview: circle.is_preview,
    circleId: circle.circle_id,
    planeId: circle.plane_id,
    planeFrame: circle.plane_frame,
    center: [circle.center.x, circle.center.y, circle.center.z],
    radius: circle.radius,
    isSelected: circle.is_selected,
    isConstruction: circle.is_construction,
    isProjected: projectedCircleIds.has(circle.circle_id),
  };
}

function makeSketchPolygon(polygon: ViewportSketchPolygon): SketchPolygonScene {
  const n = polygon.corner_x.length;
  const corners = new Array<number>(n * 3);
  for (let i = 0; i < n; i++) {
    corners[i * 3] = polygon.corner_x[i];
    corners[i * 3 + 1] = polygon.corner_y[i];
    corners[i * 3 + 2] = polygon.corner_z[i];
  }
  return {
    isPreview: polygon.is_preview,
    polygonId: polygon.polygon_id,
    planeId: polygon.plane_id,
    corners,
    isSelected: polygon.is_selected,
    isConstruction: polygon.is_construction,
  };
}

function makeSketchArc(
  arc: ViewportSketchArc,
  projectedArcIds: Set<string>,
): SketchArcScene {
  return {
    isPreview: arc.is_preview,
    arcId: arc.arc_id,
    startPointId: arc.start_point_id,
    endPointId: arc.end_point_id,
    planeId: arc.plane_id,
    planeFrame: arc.plane_frame,
    center: [arc.center.x, arc.center.y, arc.center.z],
    radius: arc.radius,
    start: [arc.start.x, arc.start.y, arc.start.z],
    end: [arc.end.x, arc.end.y, arc.end.z],
    ccw: arc.ccw,
    isSelected: arc.is_selected,
    isConstruction: arc.is_construction,
    isProjected: projectedArcIds.has(arc.arc_id),
  };
}

function parseDimensionLabel(
  label: string,
): { rawValue: number; unitSuffix: string } {
  // Match patterns like:
  //   "12.35 mm"       → mm numeric
  //   "D 24.70 mm"     → diameter numeric (still mm)
  //   "R 12.35 mm"     → radius numeric (still mm)
  //   "45\u00b0"       → degrees (angle)
  //   "12.35"          → bare number, no suffix
  const match = label.match(/([\d.]+)\s*(.*)$/);
  if (!match) return { rawValue: 0, unitSuffix: "" };
  return {
    rawValue: parseFloat(match[1]),
    unitSuffix: match[2] ? match[2].trim() : "",
  };
}

function makeSketchDimension(
  dimension: ViewportSketchDimension,
): SketchDimensionScene {
  const { rawValue, unitSuffix } = parseDimensionLabel(dimension.label);
  return {
    dimensionId: dimension.dimension_id,
    planeId: dimension.plane_id,
    kind: dimension.kind,
    entityId: dimension.entity_id,
    label: dimension.label,
    rawValue,
    unitSuffix,
    isSelected: dimension.is_selected,
    anchorStart: [
      dimension.anchor_start.x,
      dimension.anchor_start.y,
      dimension.anchor_start.z,
    ],
    anchorEnd: [
      dimension.anchor_end.x,
      dimension.anchor_end.y,
      dimension.anchor_end.z,
    ],
    dimensionStart: [
      dimension.dimension_start.x,
      dimension.dimension_start.y,
      dimension.dimension_start.z,
    ],
    dimensionEnd: [
      dimension.dimension_end.x,
      dimension.dimension_end.y,
      dimension.dimension_end.z,
    ],
    labelPosition: [
      dimension.label_position.x,
      dimension.label_position.y,
      dimension.label_position.z,
    ],

    // Angle arc geometry (optional, from C++ core)
    arcCenter: dimension.arc_center
      ? [dimension.arc_center.x, dimension.arc_center.y, dimension.arc_center.z]
      : undefined,
    arcRadius: dimension.arc_radius,
    arcStartAngle: dimension.arc_start_angle,
    arcEndAngle: dimension.arc_end_angle,
    arcCcw: dimension.arc_ccw,

    // Reference line (optional, from C++ core)
    refLineStart: dimension.ref_line_start
      ? [
          dimension.ref_line_start.x,
          dimension.ref_line_start.y,
          dimension.ref_line_start.z,
        ]
      : undefined,
    refLineEnd: dimension.ref_line_end
      ? [
          dimension.ref_line_end.x,
          dimension.ref_line_end.y,
          dimension.ref_line_end.z,
        ]
      : undefined,

    driven: dimension.driven,
    displayAs: dimension.display_as || undefined,
  };
}

function rectangleDimensionEntityIds(lines: SketchLineScene[]) {
  const duplicateLengthEntityIds = new Set<string>();
  const rectangleEntityIds = new Set<string>();
  const tolerance = 0.001;
  const pointKey = (point: [number, number, number]) =>
    point.map((value) => Math.round(value / tolerance)).join(":");
  const length = (line: SketchLineScene) => {
    const dx = line.end[0] - line.start[0];
    const dy = line.end[1] - line.start[1];
    const dz = line.end[2] - line.start[2];
    return Math.sqrt(dx * dx + dy * dy + dz * dz);
  };
  const direction = (line: SketchLineScene) => {
    const lineLength = length(line);
    return lineLength > tolerance
      ? [
          (line.end[0] - line.start[0]) / lineLength,
          (line.end[1] - line.start[1]) / lineLength,
          (line.end[2] - line.start[2]) / lineLength,
        ]
      : [0, 0, 0];
  };
  const dot = (left: number[], right: number[]) =>
    left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
  const midpoint = (line: SketchLineScene) =>
    [
      (line.start[0] + line.end[0]) / 2,
      (line.start[1] + line.end[1]) / 2,
      (line.start[2] + line.end[2]) / 2,
    ] as [number, number, number];

  for (let first = 0; first < lines.length; first += 1) {
    for (let second = first + 1; second < lines.length; second += 1) {
      for (let third = second + 1; third < lines.length; third += 1) {
        for (let fourth = third + 1; fourth < lines.length; fourth += 1) {
          const candidate = [lines[first], lines[second], lines[third], lines[fourth]];
          const pointDegrees = new Map<string, number>();
          for (const line of candidate) {
            pointDegrees.set(pointKey(line.start), (pointDegrees.get(pointKey(line.start)) ?? 0) + 1);
            pointDegrees.set(pointKey(line.end), (pointDegrees.get(pointKey(line.end)) ?? 0) + 1);
          }
          if (
            pointDegrees.size !== 4 ||
            Array.from(pointDegrees.values()).some((degree) => degree !== 2)
          ) {
            continue;
          }

          const pairs: Array<[SketchLineScene, SketchLineScene]> = [];
          const used = new Set<string>();
          for (const line of candidate) {
            if (used.has(line.lineId)) {
              continue;
            }
            const lineDirection = direction(line);
            const match = candidate.find(
              (other) =>
                other.lineId !== line.lineId &&
                !used.has(other.lineId) &&
                Math.abs(Math.abs(dot(lineDirection, direction(other))) - 1) <= 0.001 &&
                Math.abs(length(line) - length(other)) <= 0.001,
            );
            if (!match) {
              break;
            }
            pairs.push([line, match]);
            used.add(line.lineId);
            used.add(match.lineId);
          }
          if (pairs.length !== 2) {
            continue;
          }
          if (
            Math.abs(dot(direction(pairs[0][0]), direction(pairs[1][0]))) > 0.001
          ) {
            continue;
          }

          for (const line of candidate) {
            rectangleEntityIds.add(line.lineId);
          }

          for (const [lineA, lineB] of pairs) {
            const centerA = midpoint(lineA);
            const centerB = midpoint(lineB);
            const hideLine =
              centerA[1] < centerB[1] ||
              (Math.abs(centerA[1] - centerB[1]) <= tolerance &&
                centerA[0] < centerB[0])
                ? lineA
                : lineB;
            duplicateLengthEntityIds.add(hideLine.lineId);
          }
        }
      }
    }
  }

  return { duplicateLengthEntityIds, rectangleEntityIds };
}

function makeSketchConstraint(
  constraint: ViewportSketchConstraint,
): SketchConstraintScene {
  return {
    constraintId: constraint.constraint_id,
    planeId: constraint.plane_id,
    kind: constraint.kind,
    entityId: constraint.entity_id,
    relatedEntityId: constraint.related_entity_id,
    label: constraint.label,
    isSelected: constraint.is_selected,
    position: [
      constraint.position.x,
      constraint.position.y,
      constraint.position.z,
    ],
    driven: constraint.driven,
  };
}

function makeSketchProfile(profile: ViewportSketchProfile): SketchProfileScene {
  return {
    profileId: profile.profile_id,
    planeId: profile.plane_id,
    planeFrame: profile.plane_frame
      ? {
          origin: [
            profile.plane_frame.origin.x,
            profile.plane_frame.origin.y,
            profile.plane_frame.origin.z,
          ],
          xAxis: [
            profile.plane_frame.x_axis.x,
            profile.plane_frame.x_axis.y,
            profile.plane_frame.x_axis.z,
          ],
          yAxis: [
            profile.plane_frame.y_axis.x,
            profile.plane_frame.y_axis.y,
            profile.plane_frame.y_axis.z,
          ],
          normal: [
            profile.plane_frame.normal.x,
            profile.plane_frame.normal.y,
            profile.plane_frame.normal.z,
          ],
        }
      : null,
    profileKind: profile.profile_kind,
    profilePoints: profile.profile_points.map(
      (point) => [point.x, point.y] as [number, number],
    ),
    innerLoops: (profile.inner_loops ?? []).map((loop) =>
      loop.map((point) => [point.x, point.y] as [number, number]),
    ),
    start: [profile.start_x, profile.start_y],
    width: profile.width,
    height: profile.height,
    radius: profile.radius,
    isSelected: profile.is_selected,
  };
}

function makeSketchProfileFromDocument(
  feature: FeatureEntry,
  profile: SketchProfileRegionEntry,
  selectedProfileIds: ReadonlySet<string>,
): SketchProfileScene | null {
  const sketch = feature.sketch_parameters;
  if (!sketch) {
    return null;
  }
  return {
    profileId: profile.profile_id,
    planeId: sketch.plane_id,
    planeFrame: sketch.plane_frame
      ? {
          origin: [
            sketch.plane_frame.origin.x,
            sketch.plane_frame.origin.y,
            sketch.plane_frame.origin.z,
          ],
          xAxis: [
            sketch.plane_frame.x_axis.x,
            sketch.plane_frame.x_axis.y,
            sketch.plane_frame.x_axis.z,
          ],
          yAxis: [
            sketch.plane_frame.y_axis.x,
            sketch.plane_frame.y_axis.y,
            sketch.plane_frame.y_axis.z,
          ],
          normal: [
            sketch.plane_frame.normal.x,
            sketch.plane_frame.normal.y,
            sketch.plane_frame.normal.z,
          ],
        }
      : null,
    profileKind: profile.kind,
    profilePoints: profile.points.map(
      (point) => [point.x, point.y] as [number, number],
    ),
    innerLoops: profile.inner_loops.map((loop) =>
      loop.map((point) => [point.x, point.y] as [number, number]),
    ),
    start: [profile.center_x, profile.center_y],
    width: 0,
    height: 0,
    radius: profile.radius,
    isSelected: selectedProfileIds.has(profile.profile_id),
  };
}

function profileContour(profile: SketchProfileScene): [number, number][] {
  if (profile.profileKind === "circle") {
    const points: [number, number][] = [];
    const segmentCount = 64;
    for (let index = 0; index < segmentCount; index += 1) {
      const angle = (index / segmentCount) * Math.PI * 2;
      points.push([
        profile.start[0] + profile.radius * Math.cos(angle),
        profile.start[1] + profile.radius * Math.sin(angle),
      ]);
    }
    return points;
  }
  return profile.profilePoints;
}

function profileContainmentPoint(profile: SketchProfileScene): [number, number] {
  if (profile.profileKind === "circle") {
    return profile.start;
  }
  if (profile.profilePoints.length === 0) {
    return [0, 0];
  }
  const sum = profile.profilePoints.reduce(
    (accumulator, point) =>
      [accumulator[0] + point[0], accumulator[1] + point[1]] as [
        number,
        number,
      ],
    [0, 0] as [number, number],
  );
  const count = profile.profilePoints.length;
  return [sum[0] / count, sum[1] / count];
}

function loopAlreadyPresent(
  loops: [number, number][][],
  point: [number, number],
) {
  return loops.some((loop) => pointInPolygon2d(point, loop));
}

function withDisplayProfileHoles(
  profiles: SketchProfileScene[],
): SketchProfileScene[] {
  return profiles.map((profile) => {
    if (profile.profileKind !== "polygon" || profile.profilePoints.length < 3) {
      return profile;
    }

    const profileArea = polygonArea2d(profile.profilePoints);
    const nextLoops = profile.innerLoops.map((loop) => [...loop]);
    for (const candidate of profiles) {
      if (candidate.profileId === profile.profileId) {
        continue;
      }
      const contour = profileContour(candidate);
      if (contour.length < 3) {
        continue;
      }
      if (polygonArea2d(contour) >= profileArea) {
        continue;
      }
      const containmentPoint = profileContainmentPoint(candidate);
      if (!pointInPolygon2d(containmentPoint, profile.profilePoints)) {
        continue;
      }
      if (loopAlreadyPresent(nextLoops, containmentPoint)) {
        continue;
      }
      nextLoops.push(contour);
    }

    if (nextLoops.length === profile.innerLoops.length) {
      return profile;
    }
    return {
      ...profile,
      innerLoops: nextLoops,
    };
  });
}

function makeSolidFace(face: ViewportSolidFace): SolidFaceScene {
  return {
    faceId: face.face_id,
    ownerId: face.owner_id,
    ownerKind: face.owner_kind,
    label: face.label,
    sketchability: face.sketchability,
    center: [face.center.x, face.center.y, face.center.z],
    normal: [face.normal.x, face.normal.y, face.normal.z],
    planeFrame: {
      origin: [
        face.plane_frame.origin.x,
        face.plane_frame.origin.y,
        face.plane_frame.origin.z,
      ],
      xAxis: [
        face.plane_frame.x_axis.x,
        face.plane_frame.x_axis.y,
        face.plane_frame.x_axis.z,
      ],
      yAxis: [
        face.plane_frame.y_axis.x,
        face.plane_frame.y_axis.y,
        face.plane_frame.y_axis.z,
      ],
      normal: [
        face.plane_frame.normal.x,
        face.plane_frame.normal.y,
        face.plane_frame.normal.z,
      ],
    },
    size: face.size,
    // Materialize typed arrays up-front so the renderer hands them
    // straight to a BufferAttribute without re-allocating per frame.
    trianglePositions: Float32Array.from(face.triangle_positions ?? []),
    triangleIndices: Uint32Array.from(face.triangle_indices ?? []),
    isSelected: face.is_selected,
    appearanceColor: face.appearance_color ?? null,
  };
}

export interface ViewportSceneOptions {
  // Feature ids whose primitives must be filtered out of the rendered scene.
  // Maps 1:1 to feature.id (and therefore to primitive ids emitted by the
  // core, which use the feature id as the primitive id).
  hiddenFeatureIds?: ReadonlySet<string>;
  // Sketch plane ids whose sketch entities (lines, circles, points,
  // dimensions, constraints, profiles) must be filtered out.
  hiddenSketchPlaneIds?: ReadonlySet<string>;
  // Hide all reference planes and axes.
  hideReferences?: boolean;
  // Body ids whose edges should be tagged as ghosts because a fillet /
  // chamfer feature targeting that body is currently in its pending
  // panel session. Ghost edges stay pickable but render hidden by
  // default; the viewport can flip them visible when the user holds
  // the wireframe-toggle key.
  pendingEdgeOpBodyIds?: ReadonlySet<string>;
  // Optional core-owned document snapshot. Used only as a fallback
  // source for sketch profile regions when the viewport snapshot lags
  // behind a just-completed sketch edit.
  document?: DocumentState | null;
}

export function createViewportScene(
  viewport: ViewportState,
  options: ViewportSceneOptions = {},
): ViewportScene {
  const hiddenFeatureIds = options.hiddenFeatureIds ?? new Set<string>();
  const hiddenSketchPlaneIds =
    options.hiddenSketchPlaneIds ?? new Set<string>();
  const hideReferences = options.hideReferences ?? false;
  const pendingEdgeOpBodyIds =
    options.pendingEdgeOpBodyIds ?? new Set<string>();
  const document = options.document ?? null;

  const primitives = [
    ...viewport.boxes.map(makeBoxPrimitive),
    ...viewport.cylinders.map(makeCylinderPrimitive),
    ...viewport.polygon_extrudes.map(makePolygonExtrudePrimitive),
    ...viewport.meshes.map(makeMeshPrimitive),
  ].filter((primitive) => !hiddenFeatureIds.has(primitive.primitiveId));
  // The "Origin" hierarchy category covers the three named ref planes
  // and the XYZ axes only — *not* parametric construction planes,
  // which are a separate feature kind with their own visibility
  // toggle in the hierarchy. Match by the static ids the core emits
  // for origin geometry; everything else (construction planes) flows
  // through the per-feature `hiddenFeatureIds` filter.
  const isOriginPlaneId = (id: string) =>
    id === "ref-plane-xy" || id === "ref-plane-yz" || id === "ref-plane-xz";

  const references = [
    // Construction planes are also features in the document, so
    // they participate in per-feature visibility filtering. The
    // origin planes use static "ref-plane-*" ids that never
    // appear in `hiddenFeatureIds`, so that filter is a no-op for
    // them; `hideReferences` is what controls the origin trio.
    ...viewport.reference_planes
      .filter((plane) => !hiddenFeatureIds.has(plane.reference_id))
      .filter(
        (plane) => !(hideReferences && isOriginPlaneId(plane.reference_id)),
      )
      .map(makeReferencePlane),
    ...viewport.reference_axes
      .filter((axis) => !hiddenFeatureIds.has(axis.reference_id))
      .filter((axis) => !(hideReferences && axis.reference_id.startsWith("ref-axis-")))
      .map(makeReferenceAxis),
    ...(viewport.reference_points ?? [])
      .filter((point) => !hiddenFeatureIds.has(point.reference_id))
      .map(makeReferencePoint),
    ...(viewport.helices ?? [])
      .filter((helix) => !hiddenFeatureIds.has(helix.helix_id))
      .map(makeReferenceHelix),
  ];
  const isSketchPlaneVisible = (planeId: string) =>
    !hiddenSketchPlaneIds.has(planeId);
  const projectedLineIds = new Set<string>();
  const projectedCircleIds = new Set<string>();
  const projectedArcIds = new Set<string>();
  const projectedEntityIds = new Set<string>();
  const projectedFixedPointIds = new Set<string>();
  if (document) {
    for (const feature of document.feature_history) {
      const sketch = feature.sketch_parameters;
      if (feature.kind !== "sketch" || !sketch) {
        continue;
      }
      const lineById = new Map(sketch.lines.map((line) => [line.line_id, line]));
      for (const projection of sketch.projections) {
        projection.generated_line_ids.forEach((id) => {
          projectedLineIds.add(id);
          projectedEntityIds.add(id);
          const line = lineById.get(id);
          if (line) {
            projectedFixedPointIds.add(line.start_point_id);
            projectedFixedPointIds.add(line.end_point_id);
          }
        });
        projection.generated_circle_ids.forEach((id) => {
          projectedCircleIds.add(id);
          projectedEntityIds.add(id);
          projectedFixedPointIds.add(`point-circle-${id}-center`);
        });
        projection.generated_arc_ids.forEach((id) => {
          projectedArcIds.add(id);
          projectedEntityIds.add(id);
        });
        if (projection.generated_point_id) {
          projectedFixedPointIds.add(projection.generated_point_id);
        }
      }
    }
  }

  const sketchLines = viewport.sketch_lines
    .filter((line) => isSketchPlaneVisible(line.plane_id))
    .map((line) => ({
      lineId: line.line_id,
      startPointId: line.start_point_id,
      endPointId: line.end_point_id,
      planeId: line.plane_id,
      start: [line.start.x, line.start.y, line.start.z] as [
        number,
        number,
        number,
      ],
      end: [line.end.x, line.end.y, line.end.z] as [number, number, number],
      isSelected: line.is_selected,
      constraint: line.constraint,
      isConstruction: line.is_construction,
      isPreview: line.is_preview,
      isProjected: projectedLineIds.has(line.line_id),
    }));
  const sketchCircles = viewport.sketch_circles
    .filter((circle) => isSketchPlaneVisible(circle.plane_id))
    .map((circle) => makeSketchCircle(circle, projectedCircleIds));
  const sketchPolygons = viewport.sketch_polygons
    ? viewport.sketch_polygons
        .filter((polygon) => isSketchPlaneVisible(polygon.plane_id))
        .map((polygon) => makeSketchPolygon(polygon))
    : [];
  const sketchArcs = viewport.sketch_arcs
    .filter((arc) => isSketchPlaneVisible(arc.plane_id))
    .map((arc) => makeSketchArc(arc, projectedArcIds));
  const sketchPoints: SketchPointScene[] = viewport.sketch_points
    .filter((point) => isSketchPlaneVisible(point.plane_id))
    .map((point) => ({
      pointId: point.point_id,
      kind: point.kind,
      position: [point.position.x, point.position.y, point.position.z] as [
        number,
        number,
        number,
      ],
      isFixed: point.is_fixed,
      isSelected: point.is_selected,
    }));
  const hiddenRectangleDimensionEntityIds =
    rectangleDimensionEntityIds(sketchLines);
  const sketchDimensions = viewport.sketch_dimensions
    .filter((dimension) => isSketchPlaneVisible(dimension.plane_id))
    .filter((dimension) => !projectedEntityIds.has(dimension.entity_id))
    .map(makeSketchDimension);
  const visibleSketchDimensions = sketchDimensions.filter(
    (dimension) =>
      (dimension.kind !== "line_length" ||
        !hiddenRectangleDimensionEntityIds.duplicateLengthEntityIds.has(
          dimension.entityId,
        )) &&
      (dimension.kind !== "line_angle" ||
        !hiddenRectangleDimensionEntityIds.rectangleEntityIds.has(
          dimension.entityId,
        )),
  );
  const sketchConstraints = viewport.sketch_constraints
    .filter((constraint) => isSketchPlaneVisible(constraint.plane_id))
    .filter(
      (constraint) =>
        !projectedEntityIds.has(constraint.entity_id) &&
        (constraint.related_entity_id === null ||
          !projectedEntityIds.has(constraint.related_entity_id)) &&
        (constraint.kind !== "fixed" ||
          !projectedFixedPointIds.has(constraint.entity_id)),
    )
    .map(makeSketchConstraint);
  let sketchProfiles = viewport.sketch_profiles
    .filter((profile) => isSketchPlaneVisible(profile.plane_id))
    .map(makeSketchProfile);
  const profileIds = new Set(sketchProfiles.map((profile) => profile.profileId));
  if (document) {
    const selectedProfileIds = new Set(document.selected_sketch_profile_ids);
    for (const feature of document.feature_history) {
      const sketch = feature.sketch_parameters;
      if (
        feature.kind !== "sketch" ||
        !sketch ||
        hiddenFeatureIds.has(feature.feature_id) ||
        !isSketchPlaneVisible(sketch.plane_id)
      ) {
        continue;
      }
      for (const profile of sketch.profiles) {
        if (profileIds.has(profile.profile_id)) {
          continue;
        }
        const sceneProfile = makeSketchProfileFromDocument(
          feature,
          profile,
          selectedProfileIds,
        );
        if (sceneProfile) {
          sketchProfiles.push(sceneProfile);
          profileIds.add(sceneProfile.profileId);
        }
      }
    }
  }
  sketchProfiles = withDisplayProfileHoles(sketchProfiles);
  const solidFaces = viewport.solid_faces
    .filter((face) => !hiddenFeatureIds.has(face.owner_id))
    .map(makeSolidFace);
  const edges: SceneEdge[] = viewport.edges
    .filter((edge) => !hiddenFeatureIds.has(edge.owner_body_id))
    .map((edge) => ({
      edgeId: edge.id,
      ownerBodyId: edge.owner_body_id,
      kind:
        edge.kind === "line" || edge.kind === "circle" ? edge.kind : "curve",
      // Materialize a typed array up-front so the renderer can hand it
      // straight to a BufferAttribute without re-allocating per frame.
      points: Float32Array.from(edge.points),
      isSelected: edge.is_selected,
      isGhost: pendingEdgeOpBodyIds.has(edge.owner_body_id),
    }));
  const vertices: SceneVertex[] = viewport.vertices
    .filter((vertex) => !hiddenFeatureIds.has(vertex.owner_body_id))
    .map((vertex) => ({
      vertexId: vertex.id,
      ownerBodyId: vertex.owner_body_id,
      position: [vertex.position.x, vertex.position.y, vertex.position.z],
      isSelected: vertex.is_selected,
    }));
  const cutPreviews: CutPreviewScene[] = (viewport.cut_previews ?? []).map(
    (preview) => ({
      id: preview.id,
      // Materialize typed arrays up-front so the renderer hands them
      // straight to a BufferAttribute without re-allocating per frame.
      positions: Float32Array.from(preview.positions),
      normals: Float32Array.from(preview.normals),
      indices: Uint32Array.from(preview.indices),
    }),
  );

  return {
    bounds: {
      center: [
        viewport.scene_bounds.center.x,
        viewport.scene_bounds.center.y,
        viewport.scene_bounds.center.z,
      ],
      size: [
        clampDimension(viewport.scene_bounds.size.x),
        clampDimension(viewport.scene_bounds.size.y),
        clampDimension(viewport.scene_bounds.size.z),
      ],
      maxDimension: clampDimension(viewport.scene_bounds.max_dimension),
    },
    primitives,
    references,
    solidFaces,
    edges,
    vertices,
    cutPreviews,
    sketchLines,
    sketchCircles,
    sketchPolygons,
    sketchArcs,
    sketchDimensions: visibleSketchDimensions,
    sketchConstraints,
    sketchPoints,
    sketchProfiles,
    geometryKey: primitives
      .map((primitive) => {
        // TS narrows better in a switch than a deeply nested ternary,
        // particularly with the recently-added `mesh` variant.
        switch (primitive.kind) {
          case "box":
            return `box:${primitive.primitiveId}:${primitive.size.join(":")}:${primitive.position.join(":")}:${primitive.appearanceColor ?? ""}`;
          case "cylinder":
            return `cyl:${primitive.primitiveId}:${primitive.radius}:${primitive.height}:${primitive.position.join(":")}:${primitive.appearanceColor ?? ""}`;
          case "polygon_extrude":
            return `poly-extrude:${primitive.primitiveId}:${primitive.planeId}:${primitive.depth}:${primitive.profilePoints.map((point) => point.join(":")).join("|")}:${primitive.innerLoops.map((loop) => loop.map((point) => point.join(":")).join(",")).join(";")}:${primitive.appearanceColor ?? ""}`;
          case "mesh":
            return `mesh:${primitive.primitiveId}:${numericBufferSignature(primitive.positions)}:${numericBufferSignature(primitive.indices)}:${primitive.appearanceColor ?? ""}`;
        }
      })
      .concat(
        references.map((reference) =>
          reference.kind === "reference_plane"
            ? `plane:${reference.referenceId}:${reference.orientation}:${reference.position.join(":")}:${reference.size.join(":")}:${reference.isActiveSketchPlane}`
            : reference.kind === "reference_axis"
              ? `axis:${reference.referenceId}:${reference.axis}:${reference.start.join(":")}:${reference.end.join(":")}`
              : reference.kind === "reference_point"
                ? `point:${reference.referenceId}:${reference.position.join(":")}:${reference.isSelected}`
                : `helix:${reference.referenceId}:${numericBufferSignature(reference.points)}:${reference.isSelected}`,
        ),
      )
      .concat(
        solidFaces.map(
          // Include triangulation lengths so the rebuild fires whenever
          // a body's topology (and thus its per-face triangulation)
          // changes — e.g. after a fillet/chamfer/cut. Otherwise the
          // pick mesh would stay frozen on the prior topology.
          (face) =>
            `solid-face:${face.faceId}:${face.ownerId}:${face.sketchability}:${face.center.join(":")}:${face.trianglePositions.length}:${face.triangleIndices.length}:${face.isSelected}:${face.appearanceColor ?? ""}`,
        ),
      )
      .concat(
        edges.map(
          // Include `points.length` so the geometry key flips whenever
          // the body's edge topology changes (Cut/Join produces a new
          // edge count). Selection state is also part of the key so the
          // visual rebuild picks up highlight changes.
          (edge) =>
            `edge:${edge.edgeId}:${edge.points.length}:${edge.isSelected}`,
        ),
      )
      .concat(
        vertices.map(
          (vertex) =>
            `vertex:${vertex.vertexId}:${vertex.position.join(":")}:${vertex.isSelected}`,
        ),
      )
      .concat(
        cutPreviews.map(
          // Include a compact content signature so the rebuild fires as
          // the user tweaks depth in the floating panel, even when the
          // tessellation keeps the same vertex/index counts.
          (preview) =>
            `cut-preview:${preview.id}:${numericBufferSignature(preview.positions)}:${numericBufferSignature(preview.indices)}`,
        ),
      )
      .concat(
        sketchLines.map(
          (line) =>
            `sketch-line:${line.lineId}:${line.planeId}:${line.start.join(":")}:${line.end.join(":")}:${line.isSelected}:${line.constraint ?? "none"}`,
        ),
      )
      .concat(
        sketchCircles.map(
          (circle) =>
            `sketch-circle:${circle.circleId}:${circle.planeId}:${circle.center.join(":")}:${circle.radius}:${circle.isSelected}`,
        ),
      )
      .concat(
        sketchPolygons.map(
          (polygon) =>
            `sketch-polygon:${polygon.polygonId}:${polygon.planeId}:${polygon.corners.join(":")}:${polygon.isSelected}`,
        ),
      )
      .concat(
        sketchArcs.map(
          (arc) =>
            `sketch-arc:${arc.arcId}:${arc.planeId}:${arc.start.join(":")}:${arc.end.join(":")}:${arc.center.join(":")}:${arc.radius}:${arc.ccw}:${arc.isSelected}`,
        ),
      )
      .concat(
        visibleSketchDimensions.map(
          (dimension) =>
            `sketch-dimension:${dimension.dimensionId}:${dimension.kind}:${dimension.entityId}:${dimension.label}:${dimension.anchorStart.join(":")}:${dimension.anchorEnd.join(":")}:${dimension.dimensionStart.join(":")}:${dimension.dimensionEnd.join(":")}:${dimension.labelPosition.join(":")}`,
        ),
      )
      .concat(
        sketchConstraints.map(
          (constraint) =>
            `sketch-constraint:${constraint.constraintId}:${constraint.kind}:${constraint.entityId}:${constraint.relatedEntityId ?? "none"}:${constraint.label}:${constraint.position.join(":")}:${constraint.isSelected}`,
        ),
      )
      .concat(
        sketchPoints.map(
          (point) =>
            `sketch-point:${point.pointId}:${point.position.join(":")}:${point.isSelected}`,
        ),
      )
      .concat(
        sketchProfiles.map(
          (profile) =>
            `sketch-profile:${profile.profileId}:${profile.profileKind}:${profile.planeId}:${profile.profilePoints.map((point) => point.join(":")).join("|")}:${profile.innerLoops.map((loop) => loop.map((point) => point.join(":")).join(",")).join(";")}:${profile.start.join(":")}:${profile.width}:${profile.height}:${profile.radius}:${profile.isSelected}`,
        ),
      )
      .concat(
        (viewport.toolpaths ?? []).map(
          (tp) =>
            `toolpath:${tp.id}:${tp.points.length}:${tp.points.map((p) => `${p.x}:${p.y}:${p.z}:${p.is_rapid ? "r" : "f"}`).slice(0, 10).join(",")}`,
        ),
      )
      .join("|"),
  };
}
