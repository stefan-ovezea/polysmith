export interface SelectionFilter {
  select_curves: boolean;
  select_points: boolean;
  select_construction: boolean;
  select_constraints: boolean;
  snap_endpoint: boolean;
  snap_midpoint: boolean;
  snap_center: boolean;
  snap_intersection: boolean;
  snap_nearest: boolean;
  snap_quadrant: boolean;
  snap_perpendicular: boolean;
  snap_parallel: boolean;
  snap_tangent: boolean;
  snap_grid: boolean;
  snap_grid_line: boolean;
  snap_polar: boolean;
  polar_angle_degrees: number;
  magnetic_pull: boolean;
  tolerance_px: number;
}

export type SelectionFilterUpdate = Partial<
  Pick<
    SelectionFilter,
    | "select_curves"
    | "select_points"
    | "select_construction"
    | "select_constraints"
    | "snap_endpoint"
    | "snap_midpoint"
    | "snap_center"
    | "snap_intersection"
    | "snap_nearest"
    | "snap_quadrant"
    | "snap_perpendicular"
    | "snap_parallel"
    | "snap_tangent"
    | "snap_grid"
    | "magnetic_pull"
    | "tolerance_px"
  >
>;
