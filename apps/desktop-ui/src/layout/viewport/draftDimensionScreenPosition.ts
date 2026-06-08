import * as THREE from "three";

import type { SketchPlaneFrame } from "@/types";
import { projectWorldPointToViewport, toWorldPoint } from "@/utils";

import {
  DRAFT_DIMENSION_OFFSET_PX,
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

  const { local, offset } = draftDimensionLocalPosition(session, field);
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

  if (session.tool === "rectangle") {
    if (field === "width") {
      return { local: [(sx + ex) / 2, sy] as [number, number], offset: offsetUp };
    }
    return {
      local: [ex, (sy + ey) / 2] as [number, number],
      offset: [DRAFT_DIMENSION_OFFSET_PX, 0] as [number, number],
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

  return { local: session.start, offset: offsetUp };
}
