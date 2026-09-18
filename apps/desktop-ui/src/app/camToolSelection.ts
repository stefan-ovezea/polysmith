// Canonical tool-type selection for CAM operations — the single source
// of truth for every tool dropdown and default pick in the CAM panels
// and actions (replaces per-site `tool.type === "..."` filters).
//
// The FIRST entry of each list is the canonical type the core uses when
// it creates a default tool for the operation, so the default pick made
// here and the core's fallback never disagree.  Later entries widen the
// dropdown to compatible types (V-bits for engraving, spot drills for
// drilling, …).

import type { CamOperationType, ToolEntry, ToolType } from "@/types/geometry/cam";

export const TOOL_TYPES_FOR_OPERATION: Record<
  CamOperationType,
  readonly ToolType[]
> = {
  face_milling: ["endmill_flat"],
  pocket_2d: ["endmill_flat"],
  contour_2d: ["endmill_flat"],
  slot: ["endmill_flat"],
  adaptive_clearing: ["endmill_flat"],
  parallel_3d: ["endmill_ball", "endmill_bull", "endmill_flat"],
  contour_3d: ["endmill_ball", "endmill_bull", "endmill_flat"],
  chamfer: ["chamfer", "endmill_flat"],
  thread_milling: ["threadmill"],
  drilling: ["drill", "spot_drill"],
  engrave: ["endmill_flat", "v_bit"],
  laser_cut: ["laser"],
  laser_test_pattern: ["laser"],
};

/** All library tools the operation can use, in preference order. */
export function toolsForOperation(
  opType: CamOperationType,
  library: ToolEntry[],
): ToolEntry[] {
  const accepted = TOOL_TYPES_FOR_OPERATION[opType];
  return library.filter((tool) => accepted.includes(tool.type));
}

/**
 * The tool an operation defaults to: the first compatible library tool
 * (the core creates its own default when this is undefined).
 */
export function defaultToolForOperation(
  opType: CamOperationType,
  library: ToolEntry[],
): ToolEntry | undefined {
  return toolsForOperation(opType, library)[0];
}
