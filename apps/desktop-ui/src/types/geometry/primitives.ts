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
  | "fillet"
  | "chamfer"
  | "extend"
  | "offset"
  | "ellipse"
  | "slot"
  | "trim";

export type Axis = "x" | "y" | "z";

// Origin reference planes use "xy" / "yz" / "xz" so the renderer can
// apply a hardcoded rotation. Construction planes (parametric offset
// planes) use "custom" and ship a real `plane_frame`; the renderer
// reads the frame to position the quad in world space.
export type PlaneOrientation = "xy" | "yz" | "xz" | "custom";

export interface PlaneFrame {
  origin: Vector3;
  x_axis: Vector3;
  y_axis: Vector3;
  normal: Vector3;
}
