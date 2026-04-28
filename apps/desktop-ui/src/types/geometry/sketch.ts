import type {
  BoxFeatureParameters,
  CylinderFeatureParameters,
  ExtrudeFeatureParameters,
  Shape2D,
  Vector3,
} from "@/types";

export interface SketchProfilePoint {
  x: number;
  y: number;
}

export interface SketchLineEntry {
  line_id: string;
  start_point_id: string;
  end_point_id: string;
  start_x: number;
  start_y: number;
  end_x: number;
  end_y: number;
  constraint: "horizontal" | "vertical" | null;
}

export interface SketchCircleEntry {
  circle_id: string;
  center_x: number;
  center_y: number;
  radius: number;
}

export interface SketchPointEntry {
  point_id: string;
  kind: "endpoint" | "center";
  x: number;
  y: number;
  is_fixed: boolean;
}

export interface SketchDimensionEntry {
  dimension_id: string;
  kind: "line_length" | "circle_radius";
  entity_id: string;
  value: number;
}

export interface SketchLineRelationEntry {
  relation_id: string;
  kind: "equal_length" | "perpendicular" | "parallel";
  first_line_id: string;
  second_line_id: string;
}

export interface SketchProfileRegionEntry {
  profile_id: string;
  kind: "polygon" | "circle";
  point_ids: string[];
  line_ids: string[];
  points: SketchProfilePoint[];
  source_circle_id: string | null;
  center_x: number;
  center_y: number;
  radius: number;
}

export interface SketchFeatureParameters {
  plane_id: string;
  plane_frame: {
    origin: Vector3;
    x_axis: Vector3;
    y_axis: Vector3;
    normal: Vector3;
  } | null;
  active_tool: SketchTool;
  lines: SketchLineEntry[];
  circles: SketchCircleEntry[];
  points: SketchPointEntry[];
  dimensions: SketchDimensionEntry[];
  line_relations: SketchLineRelationEntry[];
  profiles: SketchProfileRegionEntry[];
}

export interface FeatureEntry {
  feature_id: string;
  kind: string;
  name: string;
  status: string;
  parameters_summary: string;
  box_parameters: BoxFeatureParameters | null;
  cylinder_parameters: CylinderFeatureParameters | null;
  extrude_parameters: ExtrudeFeatureParameters | null;
  sketch_parameters: SketchFeatureParameters | null;
}

export type SketchTool = "select" | Shape2D;
