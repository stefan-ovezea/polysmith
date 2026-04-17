export interface Vector3 {
  x: number;
  y: number;
  z: number;
}

export type Shape2D =
  | "rectangle"
  | "circle"
  | "polygon"
  | "line"
  | "arc"
  | "trim";

export type Axis = "x" | "y" | "z";

export type PlaneOrientation = "xy" | "yz" | "xz";

export interface PlaneFrame {
  origin: Vector3;
  x_axis: Vector3;
  y_axis: Vector3;
  normal: Vector3;
}
