import type {
  ViewportBoxPrimitive,
  ViewportCylinderPrimitive,
  ViewportReferenceAxis,
  ViewportReferencePlane,
  ViewportSketchCircle,
  ViewportState,
} from "../types/ipc";

export interface SceneBounds {
  center: [number, number, number];
  size: [number, number, number];
  maxDimension: number;
}

export interface BoxScenePrimitive {
  kind: "box";
  primitiveId: string;
  label: string;
  size: [number, number, number];
  position: [number, number, number];
  isSelected: boolean;
}

export interface CylinderScenePrimitive {
  kind: "cylinder";
  primitiveId: string;
  label: string;
  radius: number;
  height: number;
  position: [number, number, number];
  isSelected: boolean;
}

export type ScenePrimitive = BoxScenePrimitive | CylinderScenePrimitive;

export interface ReferencePlaneScene {
  kind: "reference_plane";
  referenceId: string;
  label: string;
  orientation: "xy" | "yz" | "xz";
  position: [number, number, number];
  size: [number, number];
  isSelected: boolean;
  isActiveSketchPlane: boolean;
}

export interface ReferenceAxisScene {
  kind: "reference_axis";
  referenceId: string;
  label: string;
  axis: "x" | "y" | "z";
  start: [number, number, number];
  end: [number, number, number];
}

export type SceneReference = ReferencePlaneScene | ReferenceAxisScene;

export interface SketchLineScene {
  lineId: string;
  planeId: string;
  start: [number, number, number];
  end: [number, number, number];
  isSelected: boolean;
  constraintHint: "horizontal" | "vertical" | null;
}

export interface SketchCircleScene {
  circleId: string;
  planeId: string;
  center: [number, number, number];
  radius: number;
  isSelected: boolean;
}

export interface ViewportScene {
  bounds: SceneBounds;
  primitives: ScenePrimitive[];
  references: SceneReference[];
  sketchLines: SketchLineScene[];
  sketchCircles: SketchCircleScene[];
  geometryKey: string;
}

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

function makeCylinderPrimitive(cylinder: ViewportCylinderPrimitive): CylinderScenePrimitive {
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

function makeReferencePlane(plane: ViewportReferencePlane): ReferencePlaneScene {
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

export function createViewportScene(viewport: ViewportState): ViewportScene {
  const primitives = [
    ...viewport.boxes.map(makeBoxPrimitive),
    ...viewport.cylinders.map(makeCylinderPrimitive),
  ];
  const references = [
    ...viewport.reference_planes.map(makeReferencePlane),
    ...viewport.reference_axes.map(makeReferenceAxis),
  ];
  const sketchLines = viewport.sketch_lines.map((line) => ({
    lineId: line.line_id,
    planeId: line.plane_id,
    start: [line.start.x, line.start.y, line.start.z] as [number, number, number],
    end: [line.end.x, line.end.y, line.end.z] as [number, number, number],
    isSelected: line.is_selected,
    constraintHint: line.constraint_hint,
  }));
  const sketchCircles = viewport.sketch_circles.map(makeSketchCircle);

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
    sketchLines,
    sketchCircles,
    geometryKey: primitives
      .map((primitive) =>
        primitive.kind === "box"
          ? `box:${primitive.primitiveId}:${primitive.size.join(":")}:${primitive.position.join(":")}`
          : `cyl:${primitive.primitiveId}:${primitive.radius}:${primitive.height}:${primitive.position.join(":")}`,
      )
      .concat(
        references.map((reference) =>
          reference.kind === "reference_plane"
            ? `plane:${reference.referenceId}:${reference.orientation}:${reference.position.join(":")}:${reference.size.join(":")}:${reference.isActiveSketchPlane}`
            : `axis:${reference.referenceId}:${reference.axis}:${reference.start.join(":")}:${reference.end.join(":")}`,
        ),
      )
      .concat(
        sketchLines.map(
          (line) =>
            `sketch-line:${line.lineId}:${line.planeId}:${line.start.join(":")}:${line.end.join(":")}:${line.isSelected}:${line.constraintHint ?? "none"}`,
        ),
      )
      .concat(
        sketchCircles.map(
          (circle) =>
            `sketch-circle:${circle.circleId}:${circle.planeId}:${circle.center.join(":")}:${circle.radius}:${circle.isSelected}`,
        ),
      )
      .join("|"),
  };
}
