import * as THREE from "three";

import type { SketchPlaneFrame } from "@/types";
import { projectWorldPointToViewport, toWorldPoint } from "@/utils";

import { computeArcPreviewGeometry } from "./arcDraftPreview";
import {
  DRAFT_DIMENSION_OFFSET_PX,
  typedRadiusSecondPoint,
  type DraftDimensionField,
  type DraftDimensionSession,
} from "./draftDimensions";

type ScreenPoint = { x: number; y: number };

interface DraftDimensionFieldScreenPositionParams {
  field: DraftDimensionField;
  session: DraftDimensionSession | null;
  screenPositions: Partial<Record<DraftDimensionField, ScreenPoint>>;
  camera: THREE.Camera | null;
  renderer: THREE.WebGLRenderer | null;
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
}

export function draftDimensionFieldScreenPosition({
  field,
  session,
  screenPositions,
  camera,
  renderer,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
}: DraftDimensionFieldScreenPositionParams): ScreenPoint | null {
  const fromRenderLoop = lineDraftDimensionScreenPosition({
    field,
    session,
    screenPositions,
  });
  if (fromRenderLoop) {
    return fromRenderLoop;
  }
  return fallbackDraftDimensionFieldScreenPosition({
    field,
    session,
    camera,
    renderer,
    activeSketchPlaneId,
    activeSketchPlaneFrame,
  });
}

function lineDraftDimensionScreenPosition({
  field,
  session,
  screenPositions,
}: Pick<
  DraftDimensionFieldScreenPositionParams,
  "field" | "session" | "screenPositions"
>): ScreenPoint | null {
  // Only line drafts project badge positions from the render loop (the
  // length/angle badges ride on the dimension geometry). Every other
  // tool — including arc — uses the local anchoring fallback below.
  return session?.tool === "line" ? (screenPositions[field] ?? null) : null;
}

function fallbackDraftDimensionFieldScreenPosition({
  field,
  session,
  camera,
  renderer,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
}: Omit<DraftDimensionFieldScreenPositionParams, "screenPositions">) {
  if (!session || !camera || !renderer) {
    return null;
  }

  // Some stages have no badge at all (arc three_point stage 1, the
  // hidden ellipse radiusY before the axis point) — draftDimensionLocalPosition
  // returns null for them.
  const position = draftDimensionLocalPosition(session, field);
  if (!position) {
    return null;
  }
  const { local, offset } = position;
  const world = toWorldPoint(
    activeSketchPlaneId ?? "ref-plane-xy",
    local,
    activeSketchPlaneFrame,
  );
  const point = projectWorldPointToViewport(world, camera, renderer);
  if (!point) {
    return null;
  }
  return {
    x: point.x + offset[0],
    y: point.y + offset[1],
  };
}

function draftDimensionLocalPosition(
  session: DraftDimensionSession,
  field: DraftDimensionField,
) {
  const [sx, sy] = session.start;
  const [ex, ey] = session.current;
  const offsetUp: [number, number] = [0, -DRAFT_DIMENSION_OFFSET_PX];
  const offsetDown: [number, number] = [0, DRAFT_DIMENSION_OFFSET_PX];

  if (session.tool === "rectangle") {
    if (field === "width") {
      return { local: [(sx + ex) / 2, sy] as [number, number], offset: offsetUp };
    }
    // Length badge anchors on the far edge midpoint (same convention as
    // the width badge), never on the moving corner: a box that tracks
    // the cursor sits on top of the point the user is aiming at.
    return {
      local: [(sx + ex) / 2, ey] as [number, number],
      offset: offsetUp,
    };
  }

  if (session.tool === "line") {
    if (field === "angle") {
      return { local: [sx, sy] as [number, number], offset: offsetUp };
    }
    return {
      local: [(sx + ex) / 2, (sy + ey) / 2] as [number, number],
      offset: offsetUp,
    };
  }

  if (session.tool === "arc") {
    if (field === "radius") {
      // The radius badge is the arc's second dimension window — it
      // exists from stage 2 on (both ends placed) and anchors at the
      // arc CENTER: the circumcenter for three_point (which moves
      // with the apex but stays a radius away from the cursor), the
      // first click for center_start_end. The length badge sits above
      // the first click; this one offsets downward so the two never
      // overlap.
      if (!session.secondPoint) {
        return null;
      }
      const geometry = computeArcPreviewGeometry({
        mode:
          session.toolMode === "center_start_end"
            ? "center_start_end"
            : "three_point",
        start: session.start,
        current: session.current,
        secondPoint: session.secondPoint,
      });
      if (!geometry) {
        return null;
      }
      return { local: geometry.centerLocal, offset: offsetDown };
    }
    // The length badge exists from the FIRST click and anchors at the
    // first click (the arc-start point for three_point, the center
    // otherwise) for both stages — away from the moving cursor, so it
    // never intercepts the pointer that places the next point.
    return { local: session.start, offset: offsetUp };
  }

  if (session.tool === "ellipse") {
    if (!session.secondPoint) {
      // Stage 1 drafts a circle — only the radiusX box (the shared
      // radius). Anchor at the ellipse CENTER, never at the moving
      // cursor: a box that tracks the cursor can never be clicked
      // (it moves away as the pointer approaches) and sits faded
      // next to the pointer.
      return field === "radiusY"
        ? null
        : { local: session.start, offset: offsetUp };
    }
    const ax = session.secondPoint[0] - sx;
    const ay = session.secondPoint[1] - sy;
    const a = Math.hypot(ax, ay);
    if (a > 0.001) {
      const ux = ax / a;
      const uy = ay / a;
      const px = -uy;
      const py = ux;
      if (field === "radiusX") {
        // Ride the axis end — the typed-radius-adjusted end when the
        // user has typed a radiusX, so the badge follows the geometry.
        return {
          local:
            typedRadiusSecondPoint(session, "radiusX") ?? session.secondPoint,
          offset: offsetUp,
        };
      }
      const ddx = ex - sx;
      const ddy = ey - sy;
      const perp = ddx * px + ddy * py;
      const side = perp < 0 ? -1 : 1;
      const b = Math.abs(perp);
      return {
        local: [sx + px * side * b, sy + py * side * b] as [number, number],
        offset: offsetUp,
      };
    }
    return field === "radiusY"
      ? null
      : { local: session.secondPoint, offset: offsetUp };
  }

  // Non-line tools anchor the badge at the entity origin (the first
  // click), never at the moving cursor: a cursor-tracking box
  // intercepts the pointer and the click that should finish the draft.
  return { local: session.start, offset: offsetUp };
}
