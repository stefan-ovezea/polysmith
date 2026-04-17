import type {
  ViewportBoxPrimitive,
  ViewportCylinderPrimitive,
  ViewportPolygonExtrudePrimitive,
  ViewportSolidFace,
  ViewportReferenceAxis,
  ViewportReferencePlane,
  ViewportSketchCircle,
  ViewportSketchConstraint,
  ViewportSketchDimension,
  ViewportSketchProfile,
  ViewportState,
  BoxScenePrimitive,
  CylinderScenePrimitive,
  PolygonExtrudeScenePrimitive,
  ReferencePlaneScene,
  ReferenceAxisScene,
  SketchCircleScene,
  SketchConstraintScene,
  SketchDimensionScene,
  SketchPointScene,
  SketchProfileScene,
  SolidFaceScene,
  ViewportScene,
} from "@/types";

function clampDimension(value: number) {
  return Math.max(value, 1);
}

function makeBoxPrimitive(box: ViewportBoxPrimitive): BoxScenePrimitive {
  return {
    kind: "box",
    primitiveId: box.primitive_id,
    label: box.label,
    size: [box.width, box.height, box.depth],
    position: [box.center.x, box.center.y, box.center.z],
    isSelected: box.is_selected,
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
    depth: primitive.depth,
    isSelected: primitive.is_selected,
  };
}

function makeReferencePlane(
  plane: ViewportReferencePlane,
): ReferencePlaneScene {
  return {
    kind: "reference_plane",
    referenceId: plane.reference_id,
    label: plane.label,
    orientation: plane.orientation,
    position: [plane.center.x, plane.center.y, plane.center.z],
    size: [plane.size.width, plane.size.height],
    isSelected: plane.is_selected,
    isActiveSketchPlane: plane.is_active_sketch_plane,
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

function makeSketchCircle(circle: ViewportSketchCircle): SketchCircleScene {
  return {
    circleId: circle.circle_id,
    planeId: circle.plane_id,
    center: [circle.center.x, circle.center.y, circle.center.z],
    radius: circle.radius,
    isSelected: circle.is_selected,
  };
}

function makeSketchDimension(
  dimension: ViewportSketchDimension,
): SketchDimensionScene {
  return {
    dimensionId: dimension.dimension_id,
    planeId: dimension.plane_id,
    kind: dimension.kind,
    entityId: dimension.entity_id,
    label: dimension.label,
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
  };
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
    start: [profile.start_x, profile.start_y],
    width: profile.width,
    height: profile.height,
    radius: profile.radius,
    isSelected: profile.is_selected,
  };
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
    isSelected: face.is_selected,
  };
}

export function createViewportScene(viewport: ViewportState): ViewportScene {
  const primitives = [
    ...viewport.boxes.map(makeBoxPrimitive),
    ...viewport.cylinders.map(makeCylinderPrimitive),
    ...viewport.polygon_extrudes.map(makePolygonExtrudePrimitive),
  ];
  const references = [
    ...viewport.reference_planes.map(makeReferencePlane),
    ...viewport.reference_axes.map(makeReferenceAxis),
  ];
  const sketchLines = viewport.sketch_lines.map((line) => ({
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
  }));
  const sketchCircles = viewport.sketch_circles.map(makeSketchCircle);
  const sketchDimensions = viewport.sketch_dimensions.map(makeSketchDimension);
  const sketchConstraints =
    viewport.sketch_constraints.map(makeSketchConstraint);
  const sketchProfiles = viewport.sketch_profiles.map(makeSketchProfile);
  const solidFaces = viewport.solid_faces.map(makeSolidFace);
  const sketchPoints: SketchPointScene[] = [
    ...sketchLines.flatMap((line) => [
      {
        pointId: line.startPointId,
        entityId: line.lineId,
        kind: "endpoint" as const,
        position: line.start,
        isSelected: line.isSelected,
      },
      {
        pointId: line.endPointId,
        entityId: line.lineId,
        kind: "endpoint" as const,
        position: line.end,
        isSelected: line.isSelected,
      },
    ]),
    ...sketchCircles.map((circle) => ({
      pointId: `${circle.circleId}:center`,
      entityId: circle.circleId,
      kind: "center" as const,
      position: circle.center,
      isSelected: circle.isSelected,
    })),
  ];

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
    sketchLines,
    sketchCircles,
    sketchDimensions,
    sketchConstraints,
    sketchPoints,
    sketchProfiles,
    geometryKey: primitives
      .map((primitive) =>
        primitive.kind === "box"
          ? `box:${primitive.primitiveId}:${primitive.size.join(":")}:${primitive.position.join(":")}`
          : primitive.kind === "cylinder"
            ? `cyl:${primitive.primitiveId}:${primitive.radius}:${primitive.height}:${primitive.position.join(":")}`
            : `poly-extrude:${primitive.primitiveId}:${primitive.planeId}:${primitive.depth}:${primitive.profilePoints.map((point) => point.join(":")).join("|")}`,
      )
      .concat(
        references.map((reference) =>
          reference.kind === "reference_plane"
            ? `plane:${reference.referenceId}:${reference.orientation}:${reference.position.join(":")}:${reference.size.join(":")}:${reference.isActiveSketchPlane}`
            : `axis:${reference.referenceId}:${reference.axis}:${reference.start.join(":")}:${reference.end.join(":")}`,
        ),
      )
      .concat(
        solidFaces.map(
          (face) =>
            `solid-face:${face.faceId}:${face.ownerId}:${face.sketchability}:${face.center.join(":")}:${face.isSelected}`,
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
        sketchDimensions.map(
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
            `sketch-profile:${profile.profileId}:${profile.profileKind}:${profile.planeId}:${profile.profilePoints.map((point) => point.join(":")).join("|")}:${profile.start.join(":")}:${profile.width}:${profile.height}:${profile.radius}:${profile.isSelected}`,
        ),
      )
      .join("|"),
  };
}
