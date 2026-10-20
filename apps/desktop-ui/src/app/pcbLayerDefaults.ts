import type { LaserCutParameters, PcbRole } from "@/types";

// The two rules that make a PCB layer cut sensible, in one place: the layer
// workspace, the CAM panel's layer picker and the "Laser cut" button all have
// to agree, or the same file cuts differently depending on how it was attached.

/// The roles a user can pick, in board order.  Mirrors `pcb_role_id` in the
/// core, and is the role list the layer properties view offers.
export const PCB_ROLE_OPTIONS: PcbRole[] = [
  "copper_top",
  "copper_inner",
  "copper_bottom",
  "solder_mask_top",
  "solder_mask_bottom",
  "paste_top",
  "paste_bottom",
  "silkscreen_top",
  "silkscreen_bottom",
  "profile",
  "drill_plated",
  "drill_non_plated",
];

/// Roles DRAWN as hairlines: the file means "cut along this line", so following
/// the stroke centre-lines is right.  Copper, mask and paste describe AREAS.
const LINE_ART_ROLES = new Set(["profile", "silkscreen_top", "silkscreen_bottom"]);

export function pcbRoleIsLineArt(role: string): boolean {
  return LINE_ART_ROLES.has(role);
}

/**
 * Recommended laser parameters for cutting an imported layer.
 *
 * Leads are ZERO on purpose: a contour shorter than twice the lead allowance is
 * skipped, and a PCB is full of small features — a 0.8 mm via ring is under
 * 4 mm around, so the CAM default of 2 + 2 mm silently drops vias and mask
 * openings.  Starting on the contour is also what you want when the cut line IS
 * the file's own outline.
 */
export function pcbLayerLaserDefaults(
  role: string,
): Partial<LaserCutParameters> {
  return {
    layer_mode: pcbRoleIsLineArt(role) ? "centerline" : "regions",
    lead_in_mm: 0,
    lead_out_mm: 0,
    // A layer is thousands of independent contours and the CAM default
    // (inner_first) degenerates to smallest-area-first for them.
    cut_order: "nearest_neighbor",
  };
}
