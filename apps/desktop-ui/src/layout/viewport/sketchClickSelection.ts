import * as THREE from "three";
import type { ArmedSketchConstraint, SketchFeatureParameters } from "@/types";
import type { SelectedConstraintState } from "./contextMenuState";

export interface CoincidentLineEndpointFallback {
  vertexId: string;
  vertexKind: "endpoint" | "center" | "quadrant";
  distance: number;
}

export type SharedSketchSelectionHit = {
  kind: string;
  id?: string;
  entityKind?: string | null;
} | null;

export type SceneSelectionHit = {
  kind: string;
  id?: string;
} | null;

export type SketchEntitySelectionHit = {
  kind: "sketch_entity";
  id: string;
  entityKind: string | null;
  isProjected: boolean;
  worldPoint: readonly [number, number, number];
};

type SketchPointKind = "endpoint" | "center" | "quadrant";

export type ActiveSketchSelectHit =
  | SketchEntitySelectionHit
  | {
      kind: "sketch_point";
      id: string;
      pointKind: SketchPointKind;
    }
  | {
      kind: "sketch_dimension";
      id: string;
    }
  | {
      kind: "sketch_constraint";
      id: string;
      constraintKind: SelectedConstraintState["kind"];
      entityId: string;
      relatedEntityId: string | null;
    }
  | {
      kind: "sketch_profile";
      id: string;
    }
  | { kind: "reference"; id: string }
  | { kind: "vertex"; id: string }
  | { kind: "edge"; id: string }
  | { kind: "face"; id: string }
  | { kind: "primitive"; id: string }
  | null;

export function sketchEntitySelectionHitFromIntersection(
  intersection: THREE.Intersection<THREE.Object3D> | undefined,
  requiredKind?: string,
): SketchEntitySelectionHit | null {
  const sketchEntityId = intersection?.object.userData.sketchEntityId;
  const rawEntityKind = intersection?.object.userData.sketchEntityKind;
  const entityKind = typeof rawEntityKind === "string" ? rawEntityKind : null;
  if (typeof sketchEntityId !== "string") {
    return null;
  }
  if (requiredKind && entityKind !== requiredKind) {
    return null;
  }

  const hitPoint = intersection.point;
  return {
    kind: "sketch_entity",
    id: sketchEntityId,
    entityKind,
    isProjected:
      intersection.object.userData.sketchEntityIsProjected === true,
    worldPoint: [hitPoint.x, hitPoint.y, hitPoint.z] as const,
  };
}

export function handleSharedSketchSelectionHit({
  hit,
  inactiveSketchEntityPickEnabled,
  additiveSelection,
  selectSketchEntity,
  selectSketchProfile,
}: {
  hit: SharedSketchSelectionHit;
  inactiveSketchEntityPickEnabled: boolean;
  additiveSelection: boolean;
  selectSketchEntity: (entityId: string, additive: boolean) => Promise<void>;
  selectSketchProfile: (profileId: string, additive: boolean) => Promise<void>;
}) {
  if (
    inactiveSketchEntityPickEnabled &&
    hit?.kind === "sketch_entity" &&
    typeof hit.id === "string" &&
    (hit.entityKind === "line" ||
      hit.entityKind === "arc" ||
      hit.entityKind === "ellipse")
  ) {
    void selectSketchEntity(hit.id, false);
    return true;
  }

  if (hit?.kind === "sketch_profile" && typeof hit.id === "string") {
    void selectSketchProfile(hit.id, additiveSelection);
    return true;
  }

  return false;
}

interface ActiveSketchSelectContext {
  hit: ActiveSketchSelectHit;
  additiveSelection: boolean;
  armedSketchConstraint: ArmedSketchConstraint;
  mirrorFocusedSlot: "objects" | "axis" | null;
  inactiveSketchEntityPickEnabled: boolean;
  sketchEntityObjectById: ReadonlyMap<string, THREE.Line | THREE.LineLoop>;
  sketchPointObjects: readonly THREE.Mesh[];
  mirrorEntityPick: (
    entityId: string,
    entityKind: "line" | "circle",
  ) => Promise<void>;
  selectSketchEntity: (entityId: string, additive: boolean) => Promise<void>;
  pickSketchPoint: (
    pointId: string,
    pointKind: SketchPointKind,
    additive: boolean,
  ) => Promise<void>;
  handleDimensionClick: (dimensionId: string) => void;
  setSelectedConstraint: (constraint: SelectedConstraintState) => void;
  paintSketchEntityMaterials: () => void;
  paintSketchPointMaterials: () => void;
  selectSketchProfile: (profileId: string, additive: boolean) => Promise<void>;
  addMessage: (message: string) => void;
  // Live sketch document state, used to resolve text glyph segments
  // (`generated_by`) to their owning text.
  sketch?: SketchFeatureParameters | null;
  // Select-mode glyph pick callback. When a hit sketch entity is a
  // text glyph segment and this callback is present, the hit is
  // routed to it (opening the Text panel bound to the owning text)
  // instead of selecting the raw line. Without the callback glyph
  // hits are ignored.
  onPickSketchText?: (textId: string) => void;
  // Select-mode slot pick: a hit on a slot's generated line/arc
  // (`generated_by: "slot:<id>"`) opens the Slot panel bound to the
  // owning slot instead of selecting the raw entity.
  onPickSketchSlot?: (slotId: string) => void;
  // Select-mode chamfer pick: a hit on a chamfer's generated line
  // opens the Chamfer panel bound to that chamfer.
  onPickSketchChamfer?: (chamferId: string) => void;
}

function handleMirrorEntityPick({
  hit,
  mirrorFocusedSlot,
  mirrorEntityPick,
}: ActiveSketchSelectContext) {
  if (
    mirrorFocusedSlot &&
    hit?.kind === "sketch_entity" &&
    !hit.isProjected &&
    (hit.entityKind === "line" || hit.entityKind === "circle")
  ) {
    void mirrorEntityPick(hit.id, hit.entityKind);
    return true;
  }
  return false;
}

function handleArmedConstraintEntityPick({
  hit,
  armedSketchConstraint,
  selectSketchEntity,
}: ActiveSketchSelectContext) {
  if (
    armedSketchConstraint &&
    armedSketchConstraint.kind !== "coincident" &&
    hit?.kind === "sketch_entity" &&
    !hit.isProjected &&
    hit.entityKind === "line"
  ) {
    void selectSketchEntity(hit.id, false);
    return true;
  }
  return false;
}

function handlePointOrDimensionPick({
  hit,
  additiveSelection,
  pickSketchPoint,
  handleDimensionClick,
}: ActiveSketchSelectContext) {
  if (hit?.kind === "sketch_point") {
    void pickSketchPoint(hit.id, hit.pointKind, additiveSelection);
    return true;
  }

  if (hit?.kind === "sketch_dimension") {
    handleDimensionClick(hit.id);
    return true;
  }
  return false;
}

function handleConstraintBadgePick({
  hit,
  armedSketchConstraint,
  setSelectedConstraint,
  paintSketchEntityMaterials,
  paintSketchPointMaterials,
}: ActiveSketchSelectContext) {
  if (hit?.kind === "sketch_constraint") {
    if (armedSketchConstraint?.kind !== "coincident") {
      setSelectedConstraint({
        kind: hit.constraintKind,
        entityId: hit.entityId,
        relatedEntityId: hit.relatedEntityId,
      });
      paintSketchEntityMaterials();
      paintSketchPointMaterials();
      return true;
    }
  }
  return false;
}

function textIdFromGeneratedBy(generatedBy: string): string | null {
  return generatedBy.startsWith("text:")
    ? generatedBy.slice("text:".length)
    : null;
}

function slotIdFromGeneratedBy(generatedBy: string): string | null {
  return generatedBy.startsWith("slot:")
    ? generatedBy.slice("slot:".length)
    : null;
}

function handleSketchEntityPick({
  hit,
  additiveSelection,
  armedSketchConstraint,
  sketchEntityObjectById,
  sketchPointObjects,
  selectSketchEntity,
  pickSketchPoint,
  addMessage,
  sketch,
  onPickSketchText,
  onPickSketchSlot,
  onPickSketchChamfer,
}: ActiveSketchSelectContext) {
  if (hit?.kind === "sketch_entity") {
    // Text glyph segments are owned by their text entity: clicking one
    // opens the Text panel bound to the owning text (Select mode)
    // instead of selecting the raw line. Glyph hits are consumed even
    // when no callback is wired up — derived geometry is never picked
    // as a plain sketch line.
    if (
      hit.entityKind === "line" &&
      !hit.isProjected &&
      sketch
    ) {
      const generatedBy =
        sketch.lines.find((line) => line.line_id === hit.id)?.generated_by ??
        null;
      if (generatedBy) {
        const textId = textIdFromGeneratedBy(generatedBy);
        if (textId && onPickSketchText) {
          onPickSketchText(textId);
        }
        return true;
      }
    }

    // Slot outlines are generated line/arc pairs owned by their slot
    // record: clicking any segment opens the Slot panel bound to the
    // owning slot instead of selecting the raw entity.
    if (
      !hit.isProjected &&
      sketch &&
      (hit.entityKind === "line" || hit.entityKind === "arc")
    ) {
      const sourceEntity =
        hit.entityKind === "line"
          ? sketch.lines.find((line) => line.line_id === hit.id)
          : sketch.arcs?.find((arc) => arc.arc_id === hit.id);
      const generatedBy = sourceEntity?.generated_by ?? null;
      if (generatedBy) {
        const slotId = slotIdFromGeneratedBy(generatedBy);
        if (slotId && onPickSketchSlot) {
          onPickSketchSlot(slotId);
        }
        return true;
      }
    }

    // The chamfer line is owned by its chamfer record: clicking it
    // opens the Chamfer panel bound to that chamfer instead of
    // selecting the raw line.
    if (
      hit.entityKind === "line" &&
      !hit.isProjected &&
      sketch &&
      onPickSketchChamfer
    ) {
      const chamfer = sketch.chamfers?.find(
        (entry) => entry.chamfer_line_id === hit.id,
      );
      if (chamfer) {
        onPickSketchChamfer(chamfer.chamfer_id);
        return true;
      }
    }

    if (
      armedSketchConstraint?.kind === "coincident" &&
      hit.entityKind === "line"
    ) {
      const fallback = coincidentLineEndpointFallback({
        lineId: hit.id,
        sketchEntityObjectById,
        sketchPointObjects,
      });
      if (fallback) {
        addMessage(
          `coincident-viewport: line-endpoint fallback ${fallback.vertexId} (dist ${fallback.distance.toFixed(3)}wu)`,
        );
        void pickSketchPoint(
          fallback.vertexId,
          fallback.vertexKind,
          additiveSelection,
        );
        return true;
      }
    }
    void selectSketchEntity(hit.id, additiveSelection);
  }
  return true;
}

export function handleActiveSketchSelectHit(
  context: ActiveSketchSelectContext,
) {
  if (handleMirrorEntityPick(context)) return true;
  if (handleArmedConstraintEntityPick(context)) return true;
  if (handlePointOrDimensionPick(context)) return true;
  if (handleConstraintBadgePick(context)) return true;
  if (
    handleSharedSketchSelectionHit({
      hit: context.hit,
      inactiveSketchEntityPickEnabled:
        context.inactiveSketchEntityPickEnabled,
      additiveSelection: context.additiveSelection,
      selectSketchEntity: context.selectSketchEntity,
      selectSketchProfile: context.selectSketchProfile,
    })
  ) {
    return true;
  }
  return handleSketchEntityPick(context);
}

export function handleActiveSketchProjectHit({
  hit,
  selectSketchProfile,
  selectVertex,
  selectEdge,
  selectFace,
}: {
  hit: ActiveSketchSelectHit;
} & SketchBodySelectionActions) {
  if (hit?.kind === "sketch_profile") {
    void selectSketchProfile(hit.id, false);
    return true;
  }

  if (hit?.kind === "vertex") {
    void selectVertex(hit.id, false);
    return true;
  }

  if (hit?.kind === "edge") {
    void selectEdge(hit.id, false);
    return true;
  }

  if (hit?.kind === "face") {
    void selectFace(hit.id);
    return true;
  }

  // Mesh bodies ship no per-face pick entries — clicking their surface
  // resolves to the rendered body-mesh primitive carrying the body id.
  // Route it through the same face-selection entry point so the Project
  // tool can intercept it as a body projection.
  if (hit?.kind === "primitive" && typeof hit.id === "string") {
    void selectFace(hit.id);
    return true;
  }

  return true;
}

export interface SketchBodySelectionActions {
  selectSketchProfile: (profileId: string, additive: boolean) => Promise<void>;
  selectVertex: (vertexId: string, additive: boolean) => Promise<void>;
  selectEdge: (edgeId: string, additive: boolean) => Promise<void>;
  selectFace: (faceId: string) => Promise<void>;
}

export interface SceneSelectionActions
  extends Omit<SketchBodySelectionActions, "selectSketchProfile"> {
  selectReference: (referenceId: string) => Promise<void>;
  selectPrimitive: (primitiveId: string) => Promise<void>;
}

export function handleSceneSelectionHit({
  hit,
  additiveSelection,
  selectReference,
  selectVertex,
  selectEdge,
  selectFace,
  selectPrimitive,
}: {
  hit: SceneSelectionHit;
  additiveSelection: boolean;
} & SceneSelectionActions) {
  if (hit?.kind === "reference" && typeof hit.id === "string") {
    void selectReference(hit.id);
    return true;
  }

  if (hit?.kind === "vertex" && typeof hit.id === "string") {
    void selectVertex(hit.id, additiveSelection);
    return true;
  }

  if (hit?.kind === "edge" && typeof hit.id === "string") {
    void selectEdge(hit.id, additiveSelection);
    return true;
  }

  if (hit?.kind === "face" && typeof hit.id === "string") {
    void selectFace(hit.id);
    return true;
  }

  if (hit?.kind === "primitive" && typeof hit.id === "string") {
    void selectPrimitive(hit.id);
    return true;
  }

  return false;
}

export function coincidentLineEndpointFallback({
  lineId,
  sketchEntityObjectById,
  sketchPointObjects,
  maxDistance = 0.5,
}: {
  lineId: string;
  sketchEntityObjectById: ReadonlyMap<string, THREE.Line | THREE.LineLoop>;
  sketchPointObjects: readonly THREE.Mesh[];
  maxDistance?: number;
}): CoincidentLineEndpointFallback | null {
  const lineObj = sketchEntityObjectById.get(lineId);
  if (!(lineObj instanceof THREE.Line)) {
    return null;
  }

  const geom = lineObj.geometry;
  const pos = geom.getAttribute("position");
  if (!pos || pos.count < 2) {
    return null;
  }

  const a = new THREE.Vector3(pos.getX(0), pos.getY(0), pos.getZ(0));
  const b = new THREE.Vector3(pos.getX(1), pos.getY(1), pos.getZ(1));
  let bestMesh: THREE.Mesh | undefined;
  let bestDistance = Infinity;

  for (const pointMesh of sketchPointObjects) {
    const pointPosition = new THREE.Vector3();
    pointMesh.getWorldPosition(pointPosition);
    const distance = Math.min(
      pointPosition.distanceTo(a),
      pointPosition.distanceTo(b),
    );
    if (distance < bestDistance) {
      bestDistance = distance;
      bestMesh = pointMesh;
    }
  }

  if (!bestMesh || bestDistance >= maxDistance) {
    return null;
  }

  const vertexId = bestMesh.userData.sketchPointId as string | undefined;
  const vertexKind = bestMesh.userData.sketchPointKind as
    | "endpoint"
    | "center"
    | "quadrant"
    | undefined;
  if (!vertexId || !vertexKind) {
    return null;
  }

  return { vertexId, vertexKind, distance: bestDistance };
}
