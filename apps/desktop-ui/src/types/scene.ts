import type { ConstraintType } from "@/types";

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

export interface PolygonExtrudeScenePrimitive {
  kind: "polygon_extrude";
  primitiveId: string;
  label: string;
  planeId: string;
  planeFrame: {
    origin: [number, number, number];
    xAxis: [number, number, number];
    yAxis: [number, number, number];
    normal: [number, number, number];
  } | null;
  profilePoints: [number, number][];
  depth: number;
  isSelected: boolean;
}

export type ScenePrimitive =
  | BoxScenePrimitive
  | CylinderScenePrimitive
  | PolygonExtrudeScenePrimitive;

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
  startPointId: string;
  endPointId: string;
  planeId: string;
  start: [number, number, number];
  end: [number, number, number];
  isSelected: boolean;
  constraint: ConstraintType | null;
}

export interface SketchCircleScene {
  circleId: string;
  planeId: string;
  center: [number, number, number];
  radius: number;
  isSelected: boolean;
}

export interface SketchPointScene {
  pointId: string;
  entityId: string;
  kind: "endpoint" | "center";
  position: [number, number, number];
  isSelected: boolean;
}

export interface SketchDimensionScene {
  dimensionId: string;
  planeId: string;
  kind: "line_length" | "circle_radius";
  entityId: string;
  label: string;
  isSelected: boolean;
  anchorStart: [number, number, number];
  anchorEnd: [number, number, number];
  dimensionStart: [number, number, number];
  dimensionEnd: [number, number, number];
  labelPosition: [number, number, number];
}

export interface SketchConstraintScene {
  constraintId: string;
  planeId: string;
  kind: ConstraintType;
  entityId: string;
  relatedEntityId: string | null;
  label: string;
  isSelected: boolean;
  position: [number, number, number];
}

export interface SketchProfileScene {
  profileId: string;
  planeId: string;
  planeFrame: {
    origin: [number, number, number];
    xAxis: [number, number, number];
    yAxis: [number, number, number];
    normal: [number, number, number];
  } | null;
  profileKind: "polygon" | "circle";
  profilePoints: [number, number][];
  start: [number, number];
  width: number;
  height: number;
  radius: number;
  isSelected: boolean;
}

export interface SolidFaceScene {
  faceId: string;
  ownerId: string;
  ownerKind: string;
  label: string;
  sketchability: string;
  center: [number, number, number];
  normal: [number, number, number];
  planeFrame: {
    origin: [number, number, number];
    xAxis: [number, number, number];
    yAxis: [number, number, number];
    normal: [number, number, number];
  };
  size: { width: number; height: number; radius: number };
  isSelected: boolean;
}
