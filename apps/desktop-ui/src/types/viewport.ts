import * as THREE from "three";

import type {
  Vector3,
  SketchProfilePoint,
  PlaneOrientation,
  Axis,
  ConstraintType,
  PlaneFrame,
  SceneBounds,
  ScenePrimitive,
  SolidFaceScene,
  SceneReference,
  SketchLineScene,
  SketchCircleScene,
  SketchDimensionScene,
  SketchConstraintScene,
  SketchPointScene,
  SketchProfileScene,
  DocumentState,
} from "@/types";

export interface ViewportPlaneSize {
  width: number;
  height: number;
}

export interface ViewportBoxPrimitive {
  primitive_id: string;
  label: string;
  width: number;
  height: number;
  depth: number;
  x_offset: number;
  center: Vector3;
  is_selected: boolean;
}

export interface ViewportCylinderPrimitive {
  primitive_id: string;
  label: string;
  radius: number;
  height: number;
  x_offset: number;
  center: Vector3;
  is_selected: boolean;
}

export interface ViewportPolygonExtrudePrimitive {
  primitive_id: string;
  label: string;
  plane_id: string;
  plane_frame: PlaneFrame | null;
  profile_points: SketchProfilePoint[];
  depth: number;
  is_selected: boolean;
}

export interface ViewportSolidFace {
  face_id: string;
  owner_id: string;
  owner_kind: string;
  label: string;
  sketchability: string;
  center: Vector3;
  normal: Vector3;
  plane_frame: PlaneFrame;
  size: {
    width: number;
    height: number;
    radius: number;
  };
  is_selected: boolean;
}

export interface ViewportReferencePlane {
  reference_id: string;
  label: string;
  orientation: PlaneOrientation;
  center: Vector3;
  size: ViewportPlaneSize;
  is_selected: boolean;
  is_active_sketch_plane: boolean;
}

export interface ViewportReferenceAxis {
  reference_id: string;
  label: string;
  axis: Axis;
  start: Vector3;
  end: Vector3;
}

export interface ViewportSketchLine {
  line_id: string;
  start_point_id: string;
  end_point_id: string;
  plane_id: string;
  start: Vector3;
  end: Vector3;
  is_selected: boolean;
  constraint: ConstraintType | null;
}

export interface ViewportSketchCircle {
  circle_id: string;
  plane_id: string;
  center: Vector3;
  radius: number;
  is_selected: boolean;
}

export interface ViewportSketchDimension {
  dimension_id: string;
  plane_id: string;
  kind: "line_length" | "circle_radius";
  entity_id: string;
  label: string;
  is_selected: boolean;
  anchor_start: Vector3;
  anchor_end: Vector3;
  dimension_start: Vector3;
  dimension_end: Vector3;
  label_position: Vector3;
}

export interface ViewportSketchConstraint {
  constraint_id: string;
  plane_id: string;
  kind: ConstraintType;
  entity_id: string;
  related_entity_id: string | null;
  label: string;
  is_selected: boolean;
  position: Vector3;
}

export interface ViewportSketchProfile {
  profile_id: string;
  plane_id: string;
  plane_frame: PlaneFrame | null;
  profile_kind: "polygon" | "circle";
  profile_points: SketchProfilePoint[];
  start_x: number;
  start_y: number;
  width: number;
  height: number;
  radius: number;
  is_selected: boolean;
}

export interface ViewportSceneBounds {
  center: Vector3;
  size: Vector3;
  max_dimension: number;
}

export interface ViewportScene {
  bounds: SceneBounds;
  primitives: ScenePrimitive[];
  solidFaces: SolidFaceScene[];
  references: SceneReference[];
  sketchLines: SketchLineScene[];
  sketchCircles: SketchCircleScene[];
  sketchDimensions: SketchDimensionScene[];
  sketchConstraints: SketchConstraintScene[];
  sketchPoints: SketchPointScene[];
  sketchProfiles: SketchProfileScene[];
  geometryKey: string;
}

export interface PrimitiveVisual {
  baseMaterial: THREE.MeshStandardMaterial;
  edgeMaterial: THREE.LineBasicMaterial;
}

export interface ReferencePlaneVisual {
  fillMaterial: THREE.MeshBasicMaterial;
  edgeMaterial: THREE.LineBasicMaterial;
}

export interface PrimitiveInteractionState {
  isSelected: boolean;
  isHovered: boolean;
}

export interface ReferencePlaneInteractionState {
  isSelected: boolean;
  isHovered: boolean;
  isActiveSketchPlane: boolean;
}

export interface ViewportContextMenuState {
  x: number;
  y: number;
  referenceId: string | null;
  faceId: string | null;
}

export interface SketchEntityInteractionState {
  isSelected: boolean;
  isHovered: boolean;
}

export interface SketchPreviewPoint {
  local: [number, number];
  world: [number, number, number];
  snapLabel: string | null;
}

export type SketchPlaneFrame = NonNullable<
  NonNullable<
    NonNullable<
      DocumentState["feature_history"][number]["sketch_parameters"]
    >["plane_frame"]
  >
>;

export type SolidFacePlaneFrame = SolidFaceScene["planeFrame"];
