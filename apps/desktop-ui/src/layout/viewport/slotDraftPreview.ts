import * as THREE from "three";
import type { SketchPlaneFrame } from "@/types";
import { buildSketchArcObject, buildSketchLineObject, toWorldPoint } from "@/utils";

// Draft preview for the slot tool (2 clicks: center → axis end; the
// second click fixes both the length and the rotation, the radius
// takes the same default the commit uses). The preview is a stadium
// group (2 lines + 2 arcs) matching the core's CCW slot expansion
// (slot_expansion.inc): bottom line bl→br, right arc br→tr ccw, top
// line tr→tl, left arc tl→bl cw.

export const SLOT_DRAFT_MIN_RADIUS = 0.5;

// Default radius for a freshly drawn slot: length/4 clamped to
// [0.5, 2], shrinking to 0.49·length when the length is too small to
// fit the core's `length >= 2 * radius` validation.
export function defaultSlotRadius(length: number): number {
  if (length < 2 * SLOT_DRAFT_MIN_RADIUS) {
    return length * 0.49;
  }
  return Math.min(2.0, Math.max(SLOT_DRAFT_MIN_RADIUS, length * 0.25));
}

export interface SlotDraftPreviewOptions {
  start: [number, number];
  current: [number, number];
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  isConstruction: boolean;
}

export function buildSlotDraftPreview({
  start,
  current,
  planeId,
  planeFrame,
  isConstruction,
}: SlotDraftPreviewOptions): THREE.Group | null {
  const dx = current[0] - start[0];
  const dy = current[1] - start[1];
  const half = Math.hypot(dx, dy);
  if (half <= 0.001) {
    return null;
  }
  const length = 2 * half;
  const radius = defaultSlotRadius(length);
  // Axis unit vector + normal (mirrors the core's u/n decomposition).
  const ux = dx / half;
  const uy = dy / half;
  const nx = -uy;
  const ny = ux;
  const bl: [number, number] = [start[0] - ux * half + nx * radius, start[1] - uy * half + ny * radius];
  const br: [number, number] = [start[0] + ux * half + nx * radius, start[1] + uy * half + ny * radius];
  const tr: [number, number] = [start[0] + ux * half - nx * radius, start[1] + uy * half - ny * radius];
  const tl: [number, number] = [start[0] - ux * half - nx * radius, start[1] - uy * half - ny * radius];
  const leftCenter: [number, number] = [start[0] - ux * half, start[1] - uy * half];
  const rightCenter: [number, number] = [start[0] + ux * half, start[1] + uy * half];

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
