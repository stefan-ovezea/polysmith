import type { SelectionFilter } from "@/types";

const STORAGE_KEY = "polysmith-selection-filter";

export const defaultSelectionFilter: SelectionFilter = {
  select_curves: true,
  select_points: true,
  select_construction: false,
  select_constraints: true,
  snap_endpoint: true,
  snap_midpoint: true,
  snap_center: true,
  snap_intersection: true,
  snap_nearest: true,
  snap_quadrant: true,
  snap_perpendicular: true,
  snap_parallel: true,
  snap_tangent: true,
  snap_grid: true,
  snap_grid_line: true,
  snap_polar: true,
  polar_angle_degrees: 15,
  magnetic_pull: true,
  tolerance_px: 20,
};

export function readStoredFilter(): SelectionFilter {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (raw) {
      return { ...defaultSelectionFilter, ...JSON.parse(raw) };
    }
  } catch {
    // Corrupt storage should not disable sketch interaction.
  }
  return { ...defaultSelectionFilter };
}

export function writeStoredFilter(filter: SelectionFilter): void {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(filter));
}

export function invertSelectionFilter(filter: SelectionFilter): SelectionFilter {
  return {
    ...filter,
    select_curves: !filter.select_curves,
    select_points: !filter.select_points,
    select_construction: !filter.select_construction,
    select_constraints: !filter.select_constraints,
    snap_endpoint: !filter.snap_endpoint,
    snap_midpoint: !filter.snap_midpoint,
    snap_center: !filter.snap_center,
    snap_intersection: !filter.snap_intersection,
    snap_nearest: !filter.snap_nearest,
    snap_quadrant: !filter.snap_quadrant,
    snap_perpendicular: !filter.snap_perpendicular,
    snap_parallel: !filter.snap_parallel,
    snap_tangent: !filter.snap_tangent,
    snap_grid: !filter.snap_grid,
    snap_grid_line: !filter.snap_grid_line,
    snap_polar: !filter.snap_polar,
    magnetic_pull: !filter.magnetic_pull,
  };
}
