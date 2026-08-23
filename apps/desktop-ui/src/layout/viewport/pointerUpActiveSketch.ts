import * as THREE from "three";

import type {
  ArmedSketchConstraint,
  SketchFeatureParameters,
  SketchTool,
} from "@/types";

import {
  handleDimensionToolClick,
} from "./dimensionToolPicking";
import { handleSketchFilletClick } from "./sketchFilletPicking";
import { handleSketchChamferClick } from "./sketchChamferPicking";
import { handleSketchTrimClick, type TrimPlaneFrame } from "./trimHoverPreview";
import type { SelectedConstraintState } from "./contextMenuState";
import {
  handleActiveSketchProjectHit,
  handleActiveSketchSelectHit,
  type ActiveSketchSelectHit,
} from "./sketchClickSelection";

export interface DimensionFirstPoint {
  id: string;
  x: number;
  y: number;
}

export interface FilletPoint {
  local: [number, number];
}

export interface ActiveSketchPointerUpContext {
  activeSketchTool: SketchTool;
  hit: ActiveSketchSelectHit;
  additiveSelection: boolean;
  planeId: string;
  planeFrame: TrimPlaneFrame | null;
  sketch: SketchFeatureParameters | null;
  armedSketchConstraint: ArmedSketchConstraint;
  mirrorFocusedSlot: "objects" | "axis" | null;
  inactiveSketchEntityPickEnabled: boolean;
  sketchEntityObjectById: ReadonlyMap<string, THREE.Line | THREE.LineLoop>;
  sketchPointObjects: readonly THREE.Mesh[];
  resolveFilletPoint: () => FilletPoint | null;
  // Sketch Chamfer tool: same snapped-corner resolution as the
  // fillet; the distances live in the App-side chamfer session.
  resolveChamferPoint: () => FilletPoint | null;
  addSketchChamfer: (
    cornerPointId: string,
    lineAId: string,
    lineBId: string,
  ) => Promise<void>;
  // Sketch Text tool: place a new text anchored at the given
  // sketch-local point (core defaults; the panel rebinds to the new
  // text id when the document round-trip lands).
  addSketchTextAt: (localX: number, localY: number) => Promise<void>;
  // Select-mode glyph pick: the hit sketch entity is a text glyph
  // segment (`generated_by: "text:<id>"`); App opens the Text panel
  // bound to the owning text instead of selecting the raw line.
  onPickSketchText?: (textId: string) => void;
  // Select-mode slot pick: the hit is a slot's generated line/arc;
  // App opens the Slot panel bound to the owning slot.
  onPickSketchSlot?: (slotId: string) => void;
  // Select-mode chamfer pick: the hit is a chamfer's generated line;
  // App opens the Chamfer panel bound to that chamfer.
  onPickSketchChamfer?: (chamferId: string) => void;
  // Extend tool: extend the hit entity from the end nearest the
  // click. Core rejects generated/construction entities.
  extendSketchEntity: (entityId: string, clickX: number, clickY: number) => Promise<void>;
  // Circle tool tangent modes: line-pick state + the mode-aware
  // creation callback (the core resolves center/radius from the line
  // ids and the placement hint).
  circleToolMode?: import("./circleDraftPreview").CircleToolMode;
  circleTangentLineIdsRef: { current: string[] };
  isConstruction: boolean;
  setSketchSnapLabel: (label: string | null) => void;
  addSketchCircleMode: (
    mode: string,
    isConstruction: boolean,
    inputs: {
      p1?: [number, number];
      p2?: [number, number];
      p3?: [number, number];
      lineAId?: string;
      lineBId?: string;
      lineCId?: string;
      hint?: [number, number];
    },
  ) => Promise<void>;
  // Offset tool: create a copy of the hit entity at the session's
  // current distance (lives in the App-side offset session).
  offsetSketchEntity: (entityId: string) => Promise<void>;
  // Text-on-path picking: while armed, an entity click binds the
  // clicked sketch line/arc as the active text's path instead of
  // placing a new text.
  sketchTextPathPicking: boolean;
  pickSketchTextPath: (entityId: string) => void;
  selectSketchProfile: (profileId: string, additive: boolean) => Promise<void>;
  selectVertex: (vertexId: string, additive: boolean) => Promise<void>;
  selectEdge: (edgeId: string, additive: boolean) => Promise<void>;
  selectFace: (faceId: string) => Promise<void>;
  trimSketchEntity:
    | ((entityId: string, localX: number, localY: number) => Promise<void>)
    | null
    | undefined;
  mirrorEntityPick: (
    entityId: string,
    entityKind: "line" | "circle",
  ) => Promise<void>;
  selectSketchEntity: (entityId: string, additive: boolean) => Promise<void>;
  pickSketchPoint: (
    pointId: string,
    pointKind: "endpoint" | "center" | "quadrant",
    additive: boolean,
  ) => Promise<void>;
  handleDimensionClick: (dimensionId: string) => void;
  setIsDimensionEditorOpen: (open: boolean) => void;
  setSelectedConstraint: (constraint: SelectedConstraintState) => void;
  paintSketchEntityMaterials: () => void;
  paintSketchPointMaterials: () => void;
  addMessage: (message: string) => void;
  addSketchFillet: (
    cornerPointId: string,
    lineAId: string,
    lineBId: string,
  ) => Promise<void>;
  pendingDimensionPlacement: boolean;
  pendingDimensionSourceId: string | null;
  pendingDimensionId: string | null;
  getDimensionFirstEntityId: () => string | null;
  getDimensionFirstPoint: () => DimensionFirstPoint | null;
  clearDimensionFirstPick: () => void;
  clearDimensionFirstEntity: () => void;
  clearPendingDimensionPlacement: () => void;
  stageDimensionFirstEntity: (entityId: string) => void;
  stageDimensionFirstPoint: (point: DimensionFirstPoint) => void;
  deleteSketchDimension: (dimensionId: string) => void;
  createDimensionAngleOrDistance: (
    firstEntityId: string,
    secondEntityId: string,
    forceMode?: "angle" | "distance",
  ) => void;
  createDimensionVertexDistance: (
    firstPointId: string,
    secondPointId: string,
  ) => void;
  createDimensionLine: (lineId: string) => void;
  createDimensionLineAngle: (lineId: string) => void;
  createDimensionLinear: (lineId: string) => void;
  createDimensionCircle: (circleId: string, label: string) => void;
  selectDimensionCircle: (circleId: string) => void;
  createDimensionArc: (arcId: string) => void;
  selectDimensionArc: (arcId: string) => void;
  createDimensionPolygon: (polygonId: string) => void;
  selectDimensionPolygon: (polygonId: string) => void;
  selectDimensionLine: (lineId: string) => void;
  dimensionToolMode: import("@/types").DimensionToolMode;
  /** Sketch-local cursor position at click time (for axis detection). */
  cursorLocal?: [number, number];
}

function dimensionToolHit(hit: ActiveSketchSelectHit) {
  return hit?.kind === "sketch_dimension" ||
    hit?.kind === "sketch_entity" ||
    hit?.kind === "sketch_point"
    ? hit
    : null;
}

// Nearest user line/arc to a sketch-local point (text-path picking).
// Snap can pull the click onto the curve or onto an endpoint — this
// recovers the intended curve from the resolved position itself.
function findPathCandidateNear(
  sketch: SketchFeatureParameters | null,
  localX: number,
  localY: number,
  tolerance: number,
): string | null {
  if (!sketch) {
    return null;
  }
  let bestId: string | null = null;
  let bestDistance = Infinity;
  for (const line of sketch.lines) {
    if (line.generated_by) continue;
    const dx = line.end_x - line.start_x;
    const dy = line.end_y - line.start_y;
    const len2 = dx * dx + dy * dy;
    const t =
      len2 > 0
        ? Math.max(
            0,
            Math.min(
              1,
              ((localX - line.start_x) * dx + (localY - line.start_y) * dy) /
                len2,
            ),
          )
        : 0;
    const distance = Math.hypot(
      localX - (line.start_x + dx * t),
      localY - (line.start_y + dy * t),
    );
    if (distance < bestDistance) {
      bestDistance = distance;
      bestId = line.line_id;
    }
  }
  for (const arc of sketch.arcs) {
    if (arc.generated_by) continue;
    const distance = Math.abs(
      Math.hypot(localX - arc.center_x, localY - arc.center_y) - arc.radius,
    );
    if (distance < bestDistance) {
      bestDistance = distance;
      bestId = arc.arc_id;
    }
  }
  return bestDistance <= tolerance ? bestId : null;
}

// Resolves a picked point (snap landed on an endpoint/midpoint) to a
// user-owned line/arc for text-path binding.
function owningPathEntity(
  sketch: SketchFeatureParameters | null,
  vertexId: string,
): string | null {
  const vertex = sketch?.vertices.find((entry) => entry.vertex_id === vertexId);
  if (!vertex) {
    return null;
  }
  for (const ownerId of vertex.geometry_owner_ids) {
    const line = sketch?.lines.find(
      (entry) => entry.line_id === ownerId && !entry.generated_by,
    );
    if (line) {
      return line.line_id;
    }
    const arc = sketch?.arcs.find(
      (entry) => entry.arc_id === ownerId && !entry.generated_by,
    );
    if (arc) {
      return arc.arc_id;
    }
  }
  return null;
}

export function handleActiveSketchPointerUpTool(
  context: ActiveSketchPointerUpContext,
) {
  // Armed constraints must fire regardless of the active sketch tool.
  // The toolbar switches to "select" on arm, but the IPC round-trip
  // may not have completed by the time the user clicks.
  if (
    context.armedSketchConstraint &&
    (context.hit?.kind === "sketch_point" ||
     context.hit?.kind === "sketch_entity")
  ) {
    handleActiveSketchSelectHit({
      hit: context.hit,
      additiveSelection: context.additiveSelection,
      armedSketchConstraint: context.armedSketchConstraint,
      mirrorFocusedSlot: context.mirrorFocusedSlot,
      inactiveSketchEntityPickEnabled: context.inactiveSketchEntityPickEnabled,
      sketchEntityObjectById: context.sketchEntityObjectById,
      sketchPointObjects: context.sketchPointObjects,
      mirrorEntityPick: context.mirrorEntityPick,
      selectSketchEntity: context.selectSketchEntity,
      pickSketchPoint: context.pickSketchPoint,
      handleDimensionClick: context.handleDimensionClick,
      setSelectedConstraint: context.setSelectedConstraint,
      paintSketchEntityMaterials: context.paintSketchEntityMaterials,
      paintSketchPointMaterials: context.paintSketchPointMaterials,
      selectSketchProfile: context.selectSketchProfile,
      addMessage: context.addMessage,
      sketch: context.sketch,
      onPickSketchText: context.onPickSketchText,
      onPickSketchSlot: context.onPickSketchSlot,
      onPickSketchChamfer: context.onPickSketchChamfer,
    });
    return true;
  }

  if (context.activeSketchTool === "project") {
    handleActiveSketchProjectHit({
      hit: context.hit,
      selectSketchProfile: context.selectSketchProfile,
      selectVertex: context.selectVertex,
      selectEdge: context.selectEdge,
      selectFace: context.selectFace,
    });
    return true;
  }

  if (context.activeSketchTool === "trim") {
    handleSketchTrimClick({
      hit: context.hit?.kind === "sketch_entity" ? context.hit : null,
      planeId: context.planeId,
      planeFrame: context.planeFrame,
      trimSketchEntity: context.trimSketchEntity,
    });
    return true;
  }

  // Move tool: a click without a drag selects the entity (so the next
  // drag moves it); empty-space clicks keep the current selection.
  // The Move tool is entity-oriented, so a point click resolves to the
  // owning entity — selecting a vertex would clear the entity selection
  // in the core and break multi-select near endpoints.
  if (context.activeSketchTool === "move") {
    if (context.hit?.kind === "sketch_entity") {
      void context.selectSketchEntity(context.hit.id, context.additiveSelection);
      return true;
    }
    if (context.hit?.kind === "sketch_point") {
      const vertex = context.sketch?.vertices.find(
        (v) => v.vertex_id === context.hit?.id,
      );
      const owners = vertex?.geometry_owner_ids ?? [];
      if (owners.length > 0) {
        void context.selectSketchEntity(owners[0], context.additiveSelection);
      } else {
        void context.pickSketchPoint(
          context.hit.id,
          context.hit.pointKind as "endpoint" | "center" | "quadrant",
          context.additiveSelection,
        );
      }
      return true;
    }
    return true;
  }

  if (context.activeSketchTool === "select") {
    handleActiveSketchSelectHit({
      hit: context.hit,
      additiveSelection: context.additiveSelection,
      armedSketchConstraint: context.armedSketchConstraint,
      mirrorFocusedSlot: context.mirrorFocusedSlot,
      inactiveSketchEntityPickEnabled: context.inactiveSketchEntityPickEnabled,
      sketchEntityObjectById: context.sketchEntityObjectById,
      sketchPointObjects: context.sketchPointObjects,
      mirrorEntityPick: context.mirrorEntityPick,
      selectSketchEntity: context.selectSketchEntity,
      pickSketchPoint: context.pickSketchPoint,
      handleDimensionClick: context.handleDimensionClick,
      setSelectedConstraint: context.setSelectedConstraint,
      paintSketchEntityMaterials: context.paintSketchEntityMaterials,
      paintSketchPointMaterials: context.paintSketchPointMaterials,
      selectSketchProfile: context.selectSketchProfile,
      addMessage: context.addMessage,
      sketch: context.sketch,
      onPickSketchText: context.onPickSketchText,
      onPickSketchSlot: context.onPickSketchSlot,
      onPickSketchChamfer: context.onPickSketchChamfer,
    });
    return true;
  }

  if (context.activeSketchTool === "dimension") {
    handleDimensionToolClick({
      hit: dimensionToolHit(context.hit),
      sketch: context.sketch,
      pendingPlacement: context.pendingDimensionPlacement,
      pendingSourceId: context.pendingDimensionSourceId,
      pendingDimensionId: context.pendingDimensionId,
      getFirstEntityId: context.getDimensionFirstEntityId,
      getFirstPoint: context.getDimensionFirstPoint,
      clearFirstPick: context.clearDimensionFirstPick,
      clearFirstEntity: context.clearDimensionFirstEntity,
      clearPendingPlacement: context.clearPendingDimensionPlacement,
      stageFirstEntity: context.stageDimensionFirstEntity,
      stageFirstPoint: context.stageDimensionFirstPoint,
      deleteSketchDimension: context.deleteSketchDimension,
      handleDimensionClick: context.handleDimensionClick,
      createAngleOrDistance: context.createDimensionAngleOrDistance,
      createVertexDistance: context.createDimensionVertexDistance,
      createLine: context.createDimensionLine,
      createLineAngle: context.createDimensionLineAngle,
      startLinearPlacement: context.createDimensionLinear,
      createCircle: context.createDimensionCircle,
      selectCircle: context.selectDimensionCircle,
      createArc: context.createDimensionArc,
      selectArc: context.selectDimensionArc,
      createPolygon: context.createDimensionPolygon,
      selectPolygon: context.selectDimensionPolygon,
      selectLine: context.selectDimensionLine,
      dimensionToolMode: context.dimensionToolMode,
      cursorLocal: context.cursorLocal,
    });
    return true;
  }

  if (context.activeSketchTool === "fillet") {
    const filletPoint = context.resolveFilletPoint();
    if (!filletPoint) {
      return true;
    }
    handleSketchFilletClick({
      sketch: context.sketch,
      localPoint: filletPoint.local,
      addSketchFillet: context.addSketchFillet,
    });
    return true;
  }

  if (context.activeSketchTool === "chamfer") {
    const chamferPoint = context.resolveChamferPoint();
    if (!chamferPoint) {
      return true;
    }
    handleSketchChamferClick({
      sketch: context.sketch,
      localPoint: chamferPoint.local,
      addSketchChamfer: context.addSketchChamfer,
    });
    return true;
  }

  // Extend tool: click a line/arc near the end to stretch it to the
  // nearest intersection. The core picks the end from the click
  // position and rejects unsupported entities with a log entry.
  if (context.activeSketchTool === "extend") {
    if (context.hit?.kind === "sketch_entity" && context.cursorLocal) {
      void context.extendSketchEntity(
        context.hit.id,
        context.cursorLocal[0],
        context.cursorLocal[1],
      );
    }
    return true;
  }

  // Offset tool: click an entity to create a copy at the session's
  // current distance (the panel sets it; default 2 mm).
  if (context.activeSketchTool === "offset") {
    if (context.hit?.kind === "sketch_entity") {
      void context.offsetSketchEntity(context.hit.id);
    }
    return true;
  }

  // Circle tool tangent modes: pick 2-3 defining lines, then click to
  // place the circle (the click position is the placement hint that
  // picks the wedge and the size).
  if (
    context.activeSketchTool === "circle" &&
    (context.circleToolMode === "tangent_two_lines" ||
      context.circleToolMode === "tangent_three_lines")
  ) {
    const maxLines =
      context.circleToolMode === "tangent_two_lines" ? 2 : 3;
    const picked = context.circleTangentLineIdsRef.current;
    // Resolve the clicked line from entity hits, point hits (snap can
    // pull the click onto an endpoint), or plain proximity.
    let lineId: string | null = null;
    if (
      context.hit?.kind === "sketch_entity" &&
      context.hit.entityKind === "line"
    ) {
      lineId = context.hit.id;
    } else if (context.hit?.kind === "sketch_point" && context.sketch) {
      const vertexId = context.hit.id;
      const owner = context.sketch.lines.find(
        (line) =>
          line.start_vertex_id === vertexId || line.end_vertex_id === vertexId,
      );
      lineId = owner?.line_id ?? null;
    } else if (context.cursorLocal && context.sketch) {
      const nearby = findPathCandidateNear(
        context.sketch,
        context.cursorLocal[0],
        context.cursorLocal[1],
        5.0,
      );
      if (nearby && context.sketch.lines.some((l) => l.line_id === nearby)) {
        lineId = nearby;
      }
    }
    const isLineId = (id: string) =>
      context.sketch?.lines.some((l) => l.line_id === id) ?? false;
    if (lineId && isLineId(lineId) && !picked.includes(lineId) &&
        picked.length < maxLines) {
      picked.push(lineId);
      context.setSketchSnapLabel(
        picked.length === maxLines
          ? "Tangent circle: click to place the circle"
          : `Tangent circle: ${picked.length}/${maxLines} lines selected`,
      );
      return true;
    }
    if (lineId === null && picked.length < maxLines && context.cursorLocal) {
      context.setSketchSnapLabel("Tangent circle: click on a line");
      return true;
    }
    if (picked.length === maxLines && context.cursorLocal) {
      void context.addSketchCircleMode(
        context.circleToolMode,
        context.isConstruction,
        {
          lineAId: picked[0],
          lineBId: picked[1],
          lineCId: maxLines === 3 ? picked[2] : undefined,
          hint: context.cursorLocal,
        },
      );
      context.circleTangentLineIdsRef.current = [];
      return true;
    }
    return true;
  }

  // Sketch Text tool: any click in the sketch places a new text at
  // the click's sketch-local point with the core's default
  // parameters. `cursorLocal` is resolved by the caller from the raw
  // pointer position on the active plane.
  if (context.activeSketchTool === "text") {
    // Path picking armed: resolve the clicked curve from the entity
    // hit, the snapped point's owners, or the snapped position itself
    // (snap can pull the click onto the curve/endpoint so the raw
    // raycast misses). Invalid picks keep the picker armed and
    // swallow the click — no text is placed while picking.
    if (context.sketchTextPathPicking) {
      let pathEntityId: string | null = null;
      if (context.hit?.kind === "sketch_entity") {
        pathEntityId = context.hit.id;
      } else if (context.hit?.kind === "sketch_point") {
        pathEntityId = owningPathEntity(context.sketch, context.hit.id);
      } else if (context.cursorLocal) {
        pathEntityId = findPathCandidateNear(
          context.sketch,
          context.cursorLocal[0],
          context.cursorLocal[1],
          5.0,
        );
      }
      if (pathEntityId) {
        context.pickSketchTextPath(pathEntityId);
      }
      return true;
    }
    if (context.cursorLocal) {
      void context.addSketchTextAt(
        context.cursorLocal[0],
        context.cursorLocal[1],
      );
    }
    return true;
  }

  return false;
}
