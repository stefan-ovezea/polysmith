// Trim-tool click handling.
//
// The hover PREVIEW used to be computed here with a local TS duplicate
// of the core's intersection math — it raced the core's own preview and
// was removed (the red highlight now renders exclusively from the
// core's trim_preview_result). What remains is the click path: convert
// the picked world point to sketch-local coordinates and dispatch the
// trim command.

export interface TrimPlaneFrame {
  origin: { x: number; y: number; z: number };
  x_axis: { x: number; y: number; z: number };
  y_axis: { x: number; y: number; z: number };
}

export interface TrimLineHighlightSegment {
  sx: number;
  sy: number;
  sz: number;
  ex: number;
  ey: number;
  ez: number;
}

export function trimWorldPointToLocal(
  point: [number, number, number],
  planeId: string,
  planeFrame: TrimPlaneFrame | null,
): [number, number] {
  const [px, py, pz] = point;
  if (planeFrame) {
    const dx = px - planeFrame.origin.x;
    const dy = py - planeFrame.origin.y;
    const dz = pz - planeFrame.origin.z;
    return [
      dx * planeFrame.x_axis.x + dy * planeFrame.x_axis.y + dz * planeFrame.x_axis.z,
      dx * planeFrame.y_axis.x + dy * planeFrame.y_axis.y + dz * planeFrame.y_axis.z,
    ];
  }
  if (planeId === "ref-plane-xy") {
    return [px, py];
  }
  if (planeId === "ref-plane-yz") {
    return [py, pz];
  }
  return [px, pz];
}

export function handleSketchTrimClick({
  hit,
  planeId,
  planeFrame,
  trimSketchEntity,
}: {
  hit:
    | {
        kind: "sketch_entity";
        id: string;
        entityKind: string | null;
        worldPoint: readonly [number, number, number];
      }
    | null
    | undefined;
  planeId: string;
  planeFrame: TrimPlaneFrame | null;
  trimSketchEntity:
    | ((entityId: string, localX: number, localY: number) => Promise<void>)
    | null
    | undefined;
}) {
  if (
    hit?.kind !== "sketch_entity" ||
    (hit.entityKind !== "line" &&
      hit.entityKind !== "circle" &&
      hit.entityKind !== "arc" &&
      hit.entityKind !== "ellipse" &&
      hit.entityKind !== "spline")
  ) {
    return true;
  }

  const [localX, localY] = trimWorldPointToLocal(
    [hit.worldPoint[0], hit.worldPoint[1], hit.worldPoint[2]],
    planeId,
    planeFrame,
  );
  void trimSketchEntity?.(hit.id, localX, localY);
  return true;
}
