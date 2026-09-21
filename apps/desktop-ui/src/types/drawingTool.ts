// Types for the Fusion-style drawing tool state machine (R1).
// Shared by the ribbon (layout), the tool hook (app) and the pure
// math module (lib).  The state machine is UI-side interaction
// state — the core knows nothing about which tool is armed (Core-UI
// design principles).

import type { DrawingView, DrawingViewFrame } from "./geometry/drawing";

/** The armed drawing tool. `idle` keeps direct sheet manipulation
 *  (view-frame drag) on. Extended by R2–R5. */
export type DrawingTool =
  | "idle"
  | "base_view"
  | "projected_view"
  | "section"
  | "move"
  | "delete_view"
  | "detail_view";

export type StandardViewName =
  | "front"
  | "right"
  | "left"
  | "top"
  | "bottom"
  | "back";

export type IsoViewName = "ne" | "nw" | "se" | "sw";

/** Base View orientation choices: six standard views, four isometric
 *  views derived from the CURRENT orientation's basis, or the frame
 *  captured from the CAD viewport camera ("Current 3D view"). */
export type BaseOrientation = StandardViewName | IsoViewName | "current3d";

export interface BaseViewSettings {
  orientation: BaseOrientation;
  /** The captured camera frame — set only when orientation is
   *  "current3d"; iso frames are derived, never stored. */
  customFrame: DrawingViewFrame | null;
  scale: number;
  showHidden: boolean;
  /** A body id or "__all__" (the assembly view). */
  bodyChoice: string;
}

/** An orthonormal view frame: normal = the projection direction,
 *  xDir = the sheet's +X (view right), yDir = sheet +Y = normal × xDir. */
export interface ViewBasis {
  normal: [number, number, number];
  xDir: [number, number, number];
  yDir: [number, number, number];
}

/** The committed parent view a projected child derives from. */
export interface ProjectedParent {
  viewId: string;
  /** Sheet-mm bounds of the parent's content (viewport payload). */
  bounds: {
    min: [number, number];
    max: [number, number];
    origin: [number, number];
  };
  basis: ViewBasis;
  scale: number;
  showHidden: boolean;
  sourceBodyIds: string[];
}

/** A classified cursor sector around the projected parent. */
export interface SectorInfo {
  /** 0=E 1=NE 2=N 3=NW 4=W 5=SW 6=S 7=SE (sheet space). */
  index: number;
  isDiagonal: boolean;
  /** Unit sheet direction: (±1,0), (0,±1) or (±1,±1). */
  sheetDir: [number, number];
  /** The uncommitted child definition (incl. its slotted position). */
  childDef: DrawingView;
}

/** The ghost's placement frame, derived LOCALLY from the cursor + the
 *  learned content size — no core round-trip.  The dashed frame +
 *  label render at these bounds at cursor speed; the projected
 *  content curves (the last preview reply for the same orientation)
 *  are translated onto the same min so the whole ghost tracks the
 *  mouse while the reply is still in flight. */
export interface GhostFrame {
  min: [number, number];
  max: [number, number];
  label: string;
  scale: number;
}
