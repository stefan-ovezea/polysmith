// Shared tool-option helpers — labels for dropdowns and the derived
// spindle/feed readouts shown in the tool editor.  Everything here is
// a pure function of the ToolEntry; the cutting-data rules mirror the
// machining textbooks (Vc = π·D·n / 1000, f = n·z·fz) and must match
// what the core post/generator will assume later.

import type { ToolEntry, ToolType } from "@/types/geometry/cam";

export interface ToolOption {
  tool_id: string;
  tool_number: number;
  label: string;
  type: ToolType;
  diameter_mm: number;
  entry: ToolEntry;
}

/** Dropdown label: "T12 · 6 mm flat end mill" (number omitted when 0). */
export function toolOptionLabel(tool: ToolEntry): string {
  const number = tool.tool_number > 0 ? `T${tool.tool_number} · ` : "";
  return `${number}${tool.name} (Ø${tool.diameter_mm} mm)`;
}

export function toolToOption(tool: ToolEntry): ToolOption {
  return {
    tool_id: tool.tool_id,
    tool_number: tool.tool_number,
    label: toolOptionLabel(tool),
    type: tool.type,
    diameter_mm: tool.diameter_mm,
    entry: tool,
  };
}

/** Spindle speed in RPM from surface speed: n = Vc·1000/(π·D). */
export function computedSpindleRpm(tool: ToolEntry): number {
  if (tool.diameter_mm <= 0 || tool.surface_speed_m_per_min <= 0) {
    return 0;
  }
  return Math.round(
    (tool.surface_speed_m_per_min * 1000) / (Math.PI * tool.diameter_mm),
  );
}

/** Feed rate in mm/min from chip load: f = n·z·fz. */
export function computedFeedMmPerMin(tool: ToolEntry): number {
  const rpm = computedSpindleRpm(tool);
  const flutes = Math.max(tool.flutes, 1);
  return Math.round(rpm * flutes * tool.feed_per_tooth_mm);
}

/**
 * Working width of a cone tool at the given cut depth — used to warn
 * about engraving wider than the flat diameter of a V-bit.
 */
export function effectiveDiameterAtDepthMm(
  tool: ToolEntry,
  depth_mm: number,
): number {
  const halfAngleDeg = tool.point_angle_deg / 2;
  const effective =
    tool.tip_diameter_mm +
    2 * depth_mm * Math.tan((halfAngleDeg * Math.PI) / 180);
  return Math.min(effective, tool.diameter_mm);
}
