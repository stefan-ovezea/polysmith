import type { SketchFeatureParameters } from "@/types";

export interface SketchFilletCornerPick {
  cornerPointId: string;
  entityAId: string;
  entityAKind: "line" | "arc";
  entityBId: string;
  entityBKind: "line" | "arc";
}

interface IncidentEntity {
  id: string;
  kind: "line" | "arc";
}

export function pickSketchFilletCorner({
  sketch,
  localPoint,
  tolerance = 0.05,
}: {
  sketch: SketchFeatureParameters;
  localPoint: readonly [number, number];
  tolerance?: number;
}): SketchFilletCornerPick | null {
  // Legacy sketches can hold TWO vertex ids microns apart at one
  // corner (projection-era splits: each entity references its own
  // id).  Gather every vertex within the pick tolerance of the click
  // and collect the incident entities across ALL of them, then
  // re-derive the single corner id from entity A's nearest endpoint —
  // the core welds the pair on fillet creation.
  const nearPoints = sketch.vertices.filter(
    (point) =>
      Math.hypot(point.x - localPoint[0], point.y - localPoint[1]) <= tolerance,
  );
  if (nearPoints.length === 0) {
    return null;
  }

  // The corner may be shared by two non-construction entities — any
  // mix of lines and arcs (generated geometry like text/slot
  // expansions is not user-filletable).  Dedup by entity id: an
  // entity is counted once even when both of its endpoints sit
  // within the tolerance.
  const seen = new Set<string>();
  const incidentEntities: IncidentEntity[] = [];
  for (const point of nearPoints) {
    for (const line of sketch.lines) {
      if (line.is_construction || line.generated_by) continue;
      if (
        (line.start_vertex_id === point.vertex_id ||
          line.end_vertex_id === point.vertex_id) &&
        !seen.has(line.line_id)
      ) {
        seen.add(line.line_id);
        incidentEntities.push({ id: line.line_id, kind: "line" });
      }
    }
    for (const arc of sketch.arcs) {
      if (arc.is_construction || arc.generated_by) continue;
      if (
        (arc.start_vertex_id === point.vertex_id ||
          arc.end_vertex_id === point.vertex_id) &&
        !seen.has(arc.arc_id)
      ) {
        seen.add(arc.arc_id);
        incidentEntities.push({ id: arc.arc_id, kind: "arc" });
      }
    }
  }
  if (incidentEntities.length !== 2) {
    return null;
  }

  const [entityA, entityB] = incidentEntities;

  // Corner id sent to the core: entity A's endpoint nearest the
  // click.  Its position is inside the pick tolerance of the click,
  // which is what the core's tolerant corner resolution keys on.
  const aEntity =
    entityA.kind === "line"
      ? sketch.lines.find((line) => line.line_id === entityA.id)
      : sketch.arcs.find((arc) => arc.arc_id === entityA.id);
  if (!aEntity) {
    return null;
  }
  const startDistance = Math.hypot(
    aEntity.start_x - localPoint[0],
    aEntity.start_y - localPoint[1],
  );
  const endDistance = Math.hypot(
    aEntity.end_x - localPoint[0],
    aEntity.end_y - localPoint[1],
  );
  const cornerPointId =
    startDistance <= endDistance
      ? aEntity.start_vertex_id
      : aEntity.end_vertex_id;

  const nearVertexIds = nearPoints.map((point) => point.vertex_id);
  const alreadyFilleted = (sketch.fillets ?? []).some((fillet) =>
    incidentEntities.some(
      (entity) =>
        ((entity.kind === "line"
          ? fillet.line_a_id === entity.id || fillet.line_b_id === entity.id
          : fillet.arc_a_id === entity.id || fillet.arc_b_id === entity.id)) &&
        (nearVertexIds.includes(fillet.trim_a_vertex_id) ||
          nearVertexIds.includes(fillet.trim_b_vertex_id)),
    ),
  );
  if (alreadyFilleted) {
    return null;
  }

  return {
    cornerPointId,
    entityAId: entityA.id,
    entityAKind: entityA.kind,
    entityBId: entityB.id,
    entityBKind: entityB.kind,
  };
}

export function handleSketchFilletClick({
  sketch,
  localPoint,
  addSketchFillet,
}: {
  sketch: SketchFeatureParameters | null;
  localPoint: readonly [number, number] | null;
  addSketchFillet: (
    cornerPointId: string,
    entityAId: string,
    entityAKind: "line" | "arc",
    entityBId: string,
    entityBKind: "line" | "arc",
  ) => Promise<void>;
}) {
  if (!sketch || !localPoint) {
    return true;
  }

  const pick = pickSketchFilletCorner({
    sketch,
    localPoint,
  });
  if (!pick) {
    return true;
  }

  void addSketchFillet(
    pick.cornerPointId,
    pick.entityAId,
    pick.entityAKind,
    pick.entityBId,
    pick.entityBKind,
  );
  return true;
}
