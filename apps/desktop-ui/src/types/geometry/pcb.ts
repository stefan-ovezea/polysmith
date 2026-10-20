// PCB fabrication layers (Gerber / Excellon) — the UI mirror of the core's
// `PcbLayerParameters` (native/cad-core/src/core/pcb/pcb_types.h).
//
// The document stores the SOURCE PATH plus a summary, never the rings: a
// copper layer is ~2k rings and embedding them would bloat every save.  So a
// layer in the UI is a row in a table (role, source, counts, warnings), and
// the geometry is fetched only when something actually needs to cut it.

/** Semantic layer identity, resolved by the core from the file's attributes
 *  (falling back to its filename). Keep in sync with `pcb_role_id`. */
export type PcbRole =
  | "unknown"
  | "copper_top"
  | "copper_inner"
  | "copper_bottom"
  | "solder_mask_top"
  | "solder_mask_bottom"
  | "paste_top"
  | "paste_bottom"
  | "silkscreen_top"
  | "silkscreen_bottom"
  | "profile"
  | "drill_plated"
  | "drill_non_plated";

export type PcbLayerFormat = "gerber" | "excellon";

/** The reference a CAM operation stores when it cuts a layer. Mirrors the
 *  core's `PcbLayerAttestation`; the CORE mints it (never the renderer), and
 *  the witness is what re-identifies the layer after a re-read. */
export interface PcbLayerAttestation {
  layer_feature_id: string;
  role: string;
  ring_count: number;
  min_x: number;
  min_y: number;
  max_x: number;
  max_y: number;
}

export interface PcbLayerParameters {
  file_path: string;
  format: PcbLayerFormat;
  role: PcbRole;
  /** What the FILE declares. A solder-mask Gerber is negative because it
   *  describes the openings, not the remaining copper. */
  file_polarity: string;
  /** Raw `TF.FileFunction` value, kept for audit ("Copper,L1,Top"). */
  file_function: string;
  /** False when the role came from the filename rather than the file itself. */
  role_from_attributes: boolean;
  mirror_x: boolean;
  ring_count: number;
  hit_count: number;
  tool_count: number;
  bounds_valid: boolean;
  min_x: number;
  min_y: number;
  max_x: number;
  max_y: number;
  warnings: string[];
}
