import * as THREE from "three";
import type { SketchPlaneFrame } from "@/types";
import { buildSketchArcObject, buildSketchLineObject, toWorldPoint } from "@/utils";

// Draft preview for the slot tool. All three pick modes reduce to the same
// stadium (2 lines + 2 arcs) described by centre + centre-to-centre length +
// radius + rotation, which is exactly the payload `add_sketch_slot` takes.
//
//   center_point    click 1 = the slot's centre, click 2 = one arc centre
//   center_to_center click 1 / click 2 = the two arc centres
//   overall          click 1 / click 2 = the two EXTREME ends (cap tips)
//
// The arithmetic lives in `slotDraftGeometry` so the preview and the commit
// cannot drift apart — they previously duplicated it, which is how a mode
// ends up drawing one shape and creating another.

export const SLOT_DRAFT_MIN_RADIUS = 0.5;

export type SlotToolMode = "center_point" | "center_to_center" | "overall";

// Default radius for a freshly drawn slot: length/4 clamped to
// [0.5, 2], shrinking to 0.49·length when the length is too small to
// fit the core's `length >= 2 * radius` validation.
export function defaultSlotRadius(length: number): number {
  if (length < 2 * SLOT_DRAFT_MIN_RADIUS) {
    return length * 0.49;
  }
  return Math.min(2.0, Math.max(SLOT_DRAFT_MIN_RADIUS, length * 0.25));
}

// Overall mode measures the cap tips, so the centre-to-centre length is
// `overall - 2 * radius`. The core requires `length >= 2 * radius`, i.e.
// `radius <= overall / 6`; the /6 cap keeps the default legal for short
// slots where `defaultSlotRadius` would otherwise be too fat.
export function defaultSlotOverallRadius(overall: number): number {
  return Math.min(defaultSlotRadius(overall), overall / 6);
}

export interface SlotDraftGeometry {
  center: [number, number];
  /** Distance between the two arc centres (the core's `length`). */
  length: number;
  radius: number;
  /** Radians, from the +X axis of the sketch plane. */
  rotation: number;
}

export function slotDraftGeometry(
  mode: SlotToolMode,
  start: [number, number],
  current: [number, number],
): SlotDraftGeometry | null {
  const dx = current[0] - start[0];
  const dy = current[1] - start[1];
  const span = Math.hypot(dx, dy);
  if (span <= 0.001) {
    return null;
  }
  const rotation = Math.atan2(dy, dx);
  const midpoint: [number, number] = [
    (start[0] + current[0]) / 2,
    (start[1] + current[1]) / 2,
  ];

  if (mode === "center_point") {
    // The first click is the slot's centre; the second is one arc centre,
    // so the arc centres sit symmetrically at ±span from it.
    const length = 2 * span;
    return {
      center: [start[0], start[1]],
      length,
      radius: defaultSlotRadius(length),
      rotation,
    };
  }

  if (mode === "overall") {
    const radius = defaultSlotOverallRadius(span);
    const length = span - 2 * radius;
    if (length <= 0.001) {
      return null;
    }
    return { center: midpoint, length, radius, rotation };
  }

  // center_to_center: the two clicks ARE the arc centres.
  const length = span;
  return {
    center: midpoint,
    length,
    radius: defaultSlotRadius(length),
    rotation,
  };
}

export interface SlotDraftPreviewOptions {
  mode: SlotToolMode;
  start: [number, number];
  current: [number, number];
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  isConstruction: boolean;
}

export function buildSlotDraftPreview({
  mode,
  start,
  current,
  planeId,
  planeFrame,
  isConstruction,
}: SlotDraftPreviewOptions): THREE.Group | null {
  const geometry = slotDraftGeometry(mode, start, current);
  if (!geometry) {
    return null;
  }
  const { center, length, radius, rotation } = geometry;
  const half = length / 2;
  // Axis unit vector + normal (mirrors the core's u/n decomposition).
  const ux = Math.cos(rotation);
  const uy = Math.sin(rotation);
  const nx = -uy;
  const ny = ux;
  const bl: [number, number] = [center[0] - ux * half + nx * radius, center[1] - uy * half + ny * radius];
  const br: [number, number] = [center[0] + ux * half + nx * radius, center[1] + uy * half + ny * radius];
  const tr: [number, number] = [center[0] + ux * half - nx * radius, center[1] + uy * half - ny * radius];
  const tl: [number, number] = [center[0] - ux * half - nx * radius, center[1] - uy * half - ny * radius];
  const leftCenter: [number, number] = [center[0] - ux * half, center[1] - uy * half];
  const rightCenter: [number, number] = [center[0] + ux * half, center[1] + uy * half];

  const group = new THREE.Group();
  const addLine = (
    lineId: string,
    startLocal: [number, number],
    endLocal: [number, number],
  ) => {
    group.add(
      buildSketchLineObject({
        lineId,
        startPointId: "preview-slot",
        endPointId: "preview-slot",
        planeId,
        start: toWorldPoint(planeId, startLocal, planeFrame),
        end: toWorldPoint(planeId, endLocal, planeFrame),
        isSelected: false,
        constraint: null,
        isConstruction,
        isPreview: true,
        isProjected: false,
        generatedBy: null,
      }),
    );
  };
  const addArc = (
    arcId: string,
    centerLocal: [number, number],
    startLocal: [number, number],
    endLocal: [number, number],
    ccw: boolean,
  ) => {
    group.add(
      buildSketchArcObject(
        {
          arcId,
          startPointId: "preview-slot",
          endPointId: "preview-slot",
          planeId,
          planeFrame,
          center: toWorldPoint(planeId, centerLocal, planeFrame),
          radius,
          start: toWorldPoint(planeId, startLocal, planeFrame),
          end: toWorldPoint(planeId, endLocal, planeFrame),
          ccw,
          isSelected: false,
          isConstruction,
          isPreview: true,
          isProjected: false,
          generatedBy: null,
        },
        planeFrame,
      ),
    );
  };
  addLine("preview-slot-bottom", bl, br);
  addArc("preview-slot-right", rightCenter, br, tr, true);
  addLine("preview-slot-top", tr, tl);
  addArc("preview-slot-left", leftCenter, tl, bl, false);
  return group;
}
