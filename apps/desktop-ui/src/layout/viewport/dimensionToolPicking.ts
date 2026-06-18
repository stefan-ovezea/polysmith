import type { SketchFeatureParameters } from "@/types";

type EntityKind = "line" | "circle" | "polygon";

function entityIdFromSketchPointId(
  pointId: string,
  kinds: readonly EntityKind[] = ["line", "circle", "polygon"],
) {
  for (const kind of kinds) {
    const match = pointId.match(new RegExp(`^point-(${kind}-\\d+)`));
    if (match) {
      return match[1];
    }
  }
  return null;
}

export function unaryDimensionIdForEntity(entityId: string) {
  if (entityId.startsWith("line-")) {
    return `dim-line-${entityId}`;
  }
  if (entityId.startsWith("circle-")) {
    return `dim-circle-${entityId}`;
  }
  if (entityId.startsWith("polygon-")) {
    return `dim-polygon-${entityId}`;
  }
  return null;
}

function hasUnaryDimension(
  sketch: SketchFeatureParameters | null,
  entityId: string,
) {
  const dimensionId = unaryDimensionIdForEntity(entityId);
  if (!dimensionId) {
    return false;
  }
  return (
    sketch?.dimensions.some(
      (dimension) => dimension.dimension_id === dimensionId,
    ) ?? false
  );
}

type DimensionPointPickAction =
  | {
      kind: "line_dimension";
      lineId: string;
    }
  | {
      kind: "point_distance";
      firstPointId: string;
      secondPointId: string;
    }
  | {
      kind: "stage_point";
      pointId: string;
      entityId: string | null;
    };

type DimensionEntityPickAction =
  | {
      kind: "entity_distance";
      firstEntityId: string;
      secondEntityId: string;
      deleteDimensionId: string | null;
    }
  | {
      kind: "point_distance_to_entity_reference";
      firstPointId: string;
      secondPointId: string;
      deleteDimensionId: string | null;
    }
  | {
      kind: "line_dimension";
      lineId: string;
      clearFirstPick: boolean;
    }
  | {
      kind: "circle_dimension";
      circleId: string;
      clearFirstPick: boolean;
    }
  | {
      kind: "select_circle";
      circleId: string;
    }
  | {
      kind: "polygon_dimension";
      polygonId: string;
    }
  | {
      kind: "select_polygon";
      polygonId: string;
    }
  | {
      kind: "select_line";
      lineId: string;
    }
  | {
      kind: "noop";
    };

interface DimensionRegroupAction {
  pendingSourceId: string;
  clickedPointId: string | null;
}

type DimensionStagedEntityAction =
  | {
      kind: "return";
      clearFirstEntity: boolean;
    }
  | {
      kind: "continue";
      firstEntityId: string | null;
    };

interface DimensionToolFirstPoint {
  id: string;
  x: number;
  y: number;
}

type DimensionToolHit =
  | {
      kind: "sketch_dimension";
      id: string;
    }
  | {
      kind: "sketch_entity";
      id: string;
      entityKind: string | null;
      isProjected: boolean;
    }
  | {
      kind: "sketch_point";
      id: string;
    }
  | null;

interface DimensionToolClickContext {
  hit: DimensionToolHit;
  sketch: SketchFeatureParameters | null;
  pendingPlacement: boolean;
  pendingSourceId: string | null;
  pendingDimensionId: string | null;
  getFirstEntityId: () => string | null;
  getFirstPoint: () => DimensionToolFirstPoint | null;
  clearFirstPick: () => void;
  clearFirstEntity: () => void;
  clearPendingPlacement: () => void;
  stageFirstEntity: (entityId: string) => void;
  stageFirstPoint: (point: DimensionToolFirstPoint) => void;
  deleteSketchDimension: (dimensionId: string) => void;
  handleDimensionClick: (dimensionId: string) => void;
  createAngleOrDistance: (firstEntityId: string, secondEntityId: string, forceMode?: "angle" | "distance") => void;
  createPointDistance: (firstPointId: string, secondPointId: string) => void;
  createLine: (lineId: string) => void;
  createLineAngle: (lineId: string) => void;
  createCircle: (circleId: string, label: string) => void;
  selectCircle: (circleId: string) => void;
  createPolygon: (polygonId: string) => void;
  selectPolygon: (polygonId: string) => void;
  selectLine: (lineId: string) => void;
  dimensionToolMode: import("@/types").DimensionToolMode;
}

function entityReferencePointId(entityKind: string | null, entityId: string) {
  if (entityKind === "circle") {
    return `point-circle-${entityId}-center`;
  }
  if (entityKind === "polygon") {
    return `point-polygon-${entityId}-center`;
  }
  return null;
}

function applyDimensionRegroup(context: DimensionToolClickContext) {
  // Don't regroup when a first entity is already staged for a follow-up
  // pick (e.g. angle or distance between two entities). Regroup is only
  // for replacing the source entity BEFORE the user commits to a second pick.
  if (context.getFirstEntityId() != null) {
    return;
  }

  const regroupAction = dimensionRegroupAction({
    pendingPlacement: context.pendingPlacement,
    pendingSourceId: context.pendingSourceId,
    hit:
      context.hit?.kind === "sketch_entity" ||
      context.hit?.kind === "sketch_point"
        ? context.hit
        : null,
  });
  if (!regroupAction) {
    return;
  }

  if (context.pendingDimensionId) {
    context.deleteSketchDimension(context.pendingDimensionId);
  }
  context.clearPendingPlacement();
  context.stageFirstEntity(regroupAction.pendingSourceId);
  if (regroupAction.clickedPointId) {
    context.stageFirstPoint({
      id: regroupAction.clickedPointId,
      x: 0,
      y: 0,
    });
  }
}

function handleDimensionStagedEntity(context: DimensionToolClickContext) {
  if (
    context.hit?.kind !== "sketch_entity" &&
    context.hit?.kind !== "sketch_point"
  ) {
    return false;
  }

  const stagedAction = dimensionStagedEntityAction({
    hit: context.hit,
    firstEntityId: context.getFirstEntityId(),
  });
  if (stagedAction.kind === "return") {
    if (stagedAction.clearFirstEntity) {
      context.clearFirstEntity();
    }
    return true;
  }
  if (stagedAction.firstEntityId) {
    context.stageFirstEntity(stagedAction.firstEntityId);
  }
  return false;
}

function applyEntityPickAction(
  action: DimensionEntityPickAction,
  context: DimensionToolClickContext,
) {
  switch (action.kind) {
    case "entity_distance":
      context.clearFirstPick();
      // Don't delete the first entity's unary dimension — the angle
      // or distance dimension coexists with it.  Deleting it would
      // leave the entity unconstrained and the solver would deform it.
      context.createAngleOrDistance(action.firstEntityId, action.secondEntityId);
      return;
    case "point_distance_to_entity_reference":
      context.clearFirstPick();
      if (action.deleteDimensionId) {
        context.deleteSketchDimension(action.deleteDimensionId);
      }
      context.createPointDistance(action.firstPointId, action.secondPointId);
      return;
    case "line_dimension":
      if (action.clearFirstPick) {
        context.clearFirstPick();
      }
      context.createLine(action.lineId);
      return;
    case "circle_dimension":
      if (action.clearFirstPick) {
        context.clearFirstPick();
      }
      context.createCircle(action.circleId, "");
      return;
    case "select_circle":
      context.selectCircle(action.circleId);
      return;
    case "polygon_dimension":
      context.createPolygon(action.polygonId);
      return;
    case "select_polygon":
      context.selectPolygon(action.polygonId);
      return;
    case "select_line":
      context.selectLine(action.lineId);
      return;
    case "noop":
      return;
  }
}

function handleDimensionEntityHit(context: DimensionToolClickContext) {
  const hit = context.hit;
  if (hit?.kind !== "sketch_entity") {
    return false;
  }
  if (hit.isProjected) {
    return true;
  }

  // Arcs don't support dimensions yet — consume the click gracefully.
  if (hit.entityKind === "arc") {
    return true;
  }

  const firstEntityId = context.getFirstEntityId();
  const firstPoint = context.getFirstPoint();
  applyEntityPickAction(
    dimensionEntityPickAction({
      entityId: hit.id,
      entityKind: hit.entityKind,
      firstEntityId,
      firstPointId: firstPoint?.id ?? null,
      hasUnary: hasUnaryDimension(context.sketch, hit.id),
    }),
    context,
  );
  return true;
}

function handleDimensionPointHit(context: DimensionToolClickContext) {
  const hit = context.hit;
  if (hit?.kind !== "sketch_point") {
    return false;
  }

  const pointAction = dimensionPointPickAction({
    pointId: hit.id,
    firstPointId: context.getFirstPoint()?.id ?? null,
  });
  if (pointAction.kind === "line_dimension") {
    context.clearFirstPick();
    context.createLine(pointAction.lineId);
    return true;
  }
  if (pointAction.kind === "point_distance") {
    context.clearFirstPick();
    context.createPointDistance(
      pointAction.firstPointId,
      pointAction.secondPointId,
    );
    return true;
  }

  context.stageFirstPoint({
    id: pointAction.pointId,
    x: 0,
    y: 0,
  });
  if (!pointAction.entityId) {
    context.clearFirstEntity();
    return true;
  }
  context.stageFirstEntity(pointAction.entityId);
  return true;
}

export function handleDimensionToolClick(context: DimensionToolClickContext) {
  // Mode-aware dispatch: "auto" uses the smart-detection logic below.
  // Specific modes are reserved for future implementation — they fall
  // through to auto behavior until their case blocks are filled in.
  switch (context.dimensionToolMode) {
    case "auto":
      break;
    // --- Sketch dimension modes ---
    case "linear":
    case "aligned":
    case "angular":
    case "radius":
    case "diameter":
    case "arc_length":
      break;
    // --- Drawing-sheet modes (reserved for ISO dimensioning) ---
    case "ordinate":
    case "jogged_radial":
    case "curve_min_max":
    case "baseline":
    case "chain":
    case "tidy_up":
    case "arrange":
    case "flip_arrows":
    case "match":
    case "dimension_break":
      break;
  }

  if (context.hit?.kind === "sketch_dimension") {
    context.clearFirstPick();
    context.handleDimensionClick(context.hit.id);
    return true;
  }

  // --- Mode-specific dispatch for sketch dimension modes ---
  // When a specific mode is selected, the first click on an entity
  // stages it without creating a unary dimension (no ghosting /
  // flashing), and the second click creates the mode-specific
  // dimension without auto-detection ambiguity.
  if (context.dimensionToolMode === "angular") {
    if (context.hit?.kind === "sketch_entity" && !context.hit.isProjected) {
      if (context.hit.entityKind !== "line") {
        context.clearFirstPick();
        return true;  // angles only apply to line pairs
      }
      const stagedFirst = context.getFirstEntityId();
      if (stagedFirst != null && stagedFirst !== context.hit.id) {
        // Second line: force angle dimension.
        context.clearFirstPick();
        context.createAngleOrDistance(stagedFirst, context.hit.id, "angle");
        return true;
      }
      if (stagedFirst === context.hit.id) {
        // Re-click same line → single-line angle from horizontal.
        context.clearFirstPick();
        context.createLineAngle(stagedFirst);
        return true;
      }
      // First line: stage without creating a unary dimension.
      context.stageFirstEntity(context.hit.id);
      return true;
    }
    // Non-line hits in angular mode: if a line is staged, create
    // a single-line angle from horizontal (Fusion 360 – style).
    // Otherwise just clear.
    const stagedFirst = context.getFirstEntityId();
    if (stagedFirst != null) {
      context.clearFirstPick();
      context.createLineAngle(stagedFirst);
      return true;
    }
    context.clearFirstPick();
    return true;
  }

  if (context.dimensionToolMode === "linear") {
    if (context.hit?.kind === "sketch_entity" && !context.hit.isProjected) {
      const stagedFirst = context.getFirstEntityId();
      if (stagedFirst != null && stagedFirst !== context.hit.id) {
        // Second entity: force distance (skip angle auto-detection).
        context.clearFirstPick();
        context.createAngleOrDistance(stagedFirst, context.hit.id, "distance");
        return true;
      }
    }
    // Fall through to normal dispatch for unary dimensions (line-length,
    // circle-radius) which still makes sense in linear mode.
  }

  // When a first entity is already staged, we're in a follow-up pick
  // for a two-entity dimension (angle, distance).  Skip regroup and
  // staged-entity handling — go directly to entity/point hit dispatch.
  const stagedFirstId = context.getFirstEntityId();
  if (stagedFirstId != null) {
    if (context.hit?.kind === "sketch_entity" && !context.hit.isProjected) {
      // Re-clicking the same entity clears the staged pick
      // instead of creating a dimension on the same entity twice.
      if (context.hit.id === stagedFirstId) {
        context.clearFirstPick();
        return true;
      }
      // Two-entity pick: create angle or distance dimension.
      context.clearFirstPick();
      context.createAngleOrDistance(stagedFirstId, context.hit.id);
      return true;
    }
    if (context.hit?.kind === "sketch_point") {
      if (context.getFirstPoint() != null) {
        // A first point is also staged — fall through to the normal
        // dispatch (handleDimensionPointHit) which handles point-to-point
        // dimensions including same-line endpoint length.
      } else {
        // Only an entity staged (no point): create point-to-entity distance.
        context.clearFirstPick();
        context.createPointDistance(stagedFirstId, context.hit.id);
        return true;
      }
    }
    // Click on empty space or unsupported hit → clear and restart.
    // When a point-to-point pick fell through above, don't clear.
    if (
      !(context.hit?.kind === "sketch_point" &&
        context.getFirstPoint() != null)
    ) {
      context.clearFirstPick();
      if (context.pendingPlacement) {
        context.clearPendingPlacement();
      }
      return true;
    }
  }

  applyDimensionRegroup(context);
  if (handleDimensionStagedEntity(context)) {
    return true;
  }
  if (handleDimensionEntityHit(context)) {
    return true;
  }
  if (handleDimensionPointHit(context)) {
    return true;
  }

  if (context.pendingPlacement) {
    context.clearPendingPlacement();
  }
  context.clearFirstPick();
  return true;
}

function dimensionRegroupAction({
  pendingPlacement,
  pendingSourceId,
  hit,
}: {
  pendingPlacement: boolean;
  pendingSourceId: string | null;
  hit:
    | { kind: "sketch_entity"; id: string }
    | { kind: "sketch_point"; id: string }
    | null
    | undefined;
}): DimensionRegroupAction | null {
  if (!pendingPlacement || !pendingSourceId) {
    return null;
  }

  if (hit?.kind === "sketch_entity" && hit.id !== pendingSourceId) {
    return {
      pendingSourceId,
      clickedPointId: null,
    };
  }

  if (hit?.kind === "sketch_point") {
    const clickedEntityId = entityIdFromSketchPointId(hit.id);
    if (clickedEntityId && clickedEntityId !== pendingSourceId) {
      return {
        pendingSourceId,
        clickedPointId: hit.id,
      };
    }
  }

  return null;
}

function dimensionStagedEntityAction({
  hit,
  firstEntityId,
}: {
  hit:
    | { kind: "sketch_entity"; id: string }
    | { kind: "sketch_point"; id: string }
    | null
    | undefined;
  firstEntityId: string | null;
}): DimensionStagedEntityAction {
  let entityId: string | null = null;
  if (hit?.kind === "sketch_entity") {
    entityId = hit.id;
  } else if (hit?.kind === "sketch_point") {
    entityId =
      entityIdFromSketchPointId(hit.id, ["line"]) ??
      entityIdFromSketchPointId(hit.id, ["circle"]);
  }

  if (!entityId) {
    return { kind: "return", clearFirstEntity: false };
  }

  if (firstEntityId && firstEntityId === entityId) {
    // Re-clicking the same entity body (not a point on it) clears the
    // staged pick.  When the user clicks a *point* on the same entity
    // (e.g. both endpoints of a line), fall through to the point handler
    // so it can create a line_length or point_distance dimension.
    if (hit?.kind === "sketch_entity") {
      return { kind: "return", clearFirstEntity: true };
    }
    return { kind: "continue", firstEntityId };
  }

  return {
    kind: "continue",
    firstEntityId,
  };
}

function dimensionPointPickAction({
  pointId,
  firstPointId,
}: {
  pointId: string;
  firstPointId: string | null;
}): DimensionPointPickAction {
  if (firstPointId && firstPointId !== pointId) {
    const lineA = entityIdFromSketchPointId(firstPointId, ["line"]);
    const lineB = entityIdFromSketchPointId(pointId, ["line"]);
    if (lineA && lineA === lineB) {
      return { kind: "line_dimension", lineId: lineA };
    }
    return {
      kind: "point_distance",
      firstPointId,
      secondPointId: pointId,
    };
  }

  return {
    kind: "stage_point",
    pointId,
    entityId: entityIdFromSketchPointId(pointId),
  };
}

function dimensionEntityPickAction({
  entityId,
  entityKind,
  firstEntityId,
  firstPointId,
  hasUnary,
}: {
  entityId: string;
  entityKind: string | null;
  firstEntityId: string | null;
  firstPointId: string | null;
  hasUnary: boolean;
}): DimensionEntityPickAction {
  if (firstEntityId && firstEntityId !== entityId) {
    const referencePointId = firstPointId
      ? entityReferencePointId(entityKind, entityId)
      : null;
    if (firstPointId && referencePointId) {
      return {
        kind: "point_distance_to_entity_reference",
        firstPointId,
        secondPointId: referencePointId,
        deleteDimensionId: null,
      };
    }
    return {
      kind: "entity_distance",
      firstEntityId,
      secondEntityId: entityId,
      deleteDimensionId: null,
    };
  }

  if (
    firstPointId &&
    entityKind === "line" &&
    entityIdFromSketchPointId(firstPointId, ["line"]) === entityId
  ) {
    return {
      kind: "line_dimension",
      lineId: entityId,
      clearFirstPick: true,
    };
  }

  if (
    firstPointId &&
    firstEntityId === entityId &&
    entityKind === "circle" &&
    !hasUnary
  ) {
    return {
      kind: "circle_dimension",
      circleId: entityId,
      clearFirstPick: true,
    };
  }

  if (entityKind === "circle") {
    return hasUnary
      ? { kind: "select_circle", circleId: entityId }
      : {
          kind: "circle_dimension",
          circleId: entityId,
          clearFirstPick: false,
        };
  }

  if (entityKind === "polygon") {
    return hasUnary
      ? { kind: "select_polygon", polygonId: entityId }
      : { kind: "polygon_dimension", polygonId: entityId };
  }

  // Arcs and any other unsupported entity kinds: consume the click
  // without creating a dimension.
  if (entityKind !== "line") {
    return { kind: "noop" };
  }

  return hasUnary
    ? { kind: "select_line", lineId: entityId }
    : {
        kind: "line_dimension",
        lineId: entityId,
        clearFirstPick: false,
      };
}
