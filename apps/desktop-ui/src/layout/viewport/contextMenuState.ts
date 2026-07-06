import type {
  ConstraintType,
  DocumentState,
  ViewportContextMenuState,
} from "@/types";
import type { SolidFaceScene } from "@/types/scene";

export type ViewportPickHit =
  | { kind: "reference"; id: string }
  | { kind: "face"; id: string }
  | { kind: "primitive"; id: string }
  | { kind: "edge"; id: string }
  | { kind: "vertex"; id: string }
  | {
      kind: "sketch_entity";
      id: string;
      entityKind?: string | null;
      isProjected?: boolean;
      worldPoint?: readonly [number, number, number];
    }
  | {
      kind: "sketch_point";
      id: string;
      pointKind?: "endpoint" | "center" | "quadrant" | string;
    }
  | { kind: "sketch_profile"; id: string }
  | {
      kind: "sketch_dimension";
      id: string;
      part?: "label" | "geometry";
    }
  | {
      kind: "sketch_constraint";
      id: string;
      constraintKind: ConstraintType;
      entityId: string;
      relatedEntityId: string | null;
    };

export interface SelectedConstraintState {
  kind: ConstraintType;
  entityId: string;
  relatedEntityId: string | null;
}

export interface ContextMenuBuildResult {
  contextMenu: ViewportContextMenuState | null;
  selectedConstraint?: SelectedConstraintState | null;
}

interface ContextMenuBuildInput {
  activeSketchPlaneId: string | null;
  document: DocumentState | null;
  hit: ViewportPickHit | null;
  x: number;
  y: number;
  solidFaces: readonly SolidFaceScene[];
}

function bodyIdFromTopologyId(id: string | null | undefined) {
  if (!id) {
    return null;
  }
  for (const marker of [":face:", ":edge:", ":vertex:"]) {
    const markerIndex = id.indexOf(marker);
    if (markerIndex > 0) {
      return id.slice(0, markerIndex);
    }
  }
  return null;
}

function uniqueIds(ids: string[]) {
  return [...new Set(ids)];
}

function currentSketchSelection(documentState: DocumentState | null) {
  const entityIds = uniqueIds([
    ...(documentState?.selected_sketch_entity_ids ?? []),
    ...(documentState?.selected_sketch_entity_id
      ? [documentState.selected_sketch_entity_id]
      : []),
  ]);
  const pointIds = uniqueIds([
    ...(documentState?.selected_sketch_point_ids ?? []),
    ...(documentState?.selected_sketch_point_id
      ? [documentState.selected_sketch_point_id]
      : []),
  ]);
  const profileIds = uniqueIds([
    ...(documentState?.selected_sketch_profile_ids ?? []),
    ...(documentState?.selected_sketch_profile_id
      ? [documentState.selected_sketch_profile_id]
      : []),
  ]);
  return { entityIds, pointIds, profileIds };
}

function hasSketchSelection(selection: {
  entityIds: string[];
  pointIds: string[];
  profileIds: string[];
}) {
  return (
    selection.entityIds.length > 0 ||
    selection.pointIds.length > 0 ||
    selection.profileIds.length > 0
  );
}

function clickedSketchSelection(hit: ViewportPickHit) {
  if (hit.kind === "sketch_entity") {
    return { entityIds: [hit.id], pointIds: [], profileIds: [] };
  }
  if (hit.kind === "sketch_point") {
    return { entityIds: [], pointIds: [hit.id], profileIds: [] };
  }
  return { entityIds: [], pointIds: [], profileIds: [hit.id] };
}

function sketchHitIsSelected(
  hit: ViewportPickHit,
  selection: {
    entityIds: string[];
    pointIds: string[];
    profileIds: string[];
  },
) {
  return (
    (hit.kind === "sketch_entity" && selection.entityIds.includes(hit.id)) ||
    (hit.kind === "sketch_point" && selection.pointIds.includes(hit.id)) ||
    (hit.kind === "sketch_profile" && selection.profileIds.includes(hit.id))
  );
}

function sketchContextMenu({
  document,
  hit,
  x,
  y,
}: Pick<ContextMenuBuildInput, "document" | "hit" | "x" | "y">):
  | ContextMenuBuildResult
  | null {
  const sketchHit =
    hit?.kind === "sketch_entity" ||
    hit?.kind === "sketch_point" ||
    hit?.kind === "sketch_profile" ||
    hit?.kind === "sketch_dimension" ||
    hit?.kind === "sketch_constraint";

  if (!sketchHit) {
    const selection = currentSketchSelection(document);
    if (!hasSketchSelection(selection)) {
      return { contextMenu: null };
    }
    return {
      contextMenu: {
        x,
        y,
        referenceId: null,
        faceId: null,
        sketchDeleteSelection: selection,
      },
    };
  }

  if (hit.kind === "sketch_constraint") {
    return {
      contextMenu: {
        x,
        y,
        referenceId: null,
        faceId: null,
        constraintKind: hit.constraintKind,
        constraintId: hit.id,
        constraintEntityId: hit.entityId,
        constraintRelatedEntityId: hit.relatedEntityId,
      },
      selectedConstraint: {
        kind: hit.constraintKind,
        entityId: hit.entityId,
        relatedEntityId: hit.relatedEntityId,
      },
    };
  }

  if (hit.kind === "sketch_dimension") {
    return {
      contextMenu: {
        x,
        y,
        referenceId: null,
        faceId: null,
        dimensionId: hit.id,
      },
    };
  }

  // When right-clicking a fixed point, show the constraint menu
  // ("Delete Constraint" → unfix) instead of the generic Delete menu.
  if (hit.kind === "sketch_point") {
    const activeId = document?.active_sketch_feature_id;
    const feature = activeId
      ? document?.feature_history.find((f) => f.feature_id === activeId)
      : null;
    const point = feature?.sketch_parameters?.points.find(
      (p) => p.point_id === hit.id,
    );
    if (point?.is_fixed) {
      return {
        contextMenu: {
          x,
          y,
          referenceId: null,
          faceId: null,
          constraintKind: "fixed",
          constraintEntityId: hit.id,
        },
      };
    }
  }

  const currentSelection = currentSketchSelection(document);
  const clickedSelection = clickedSketchSelection(hit);
  const clickedIsSelected = sketchHitIsSelected(hit, currentSelection);
  const selection =
    clickedIsSelected && hasSketchSelection(currentSelection)
      ? currentSelection
      : clickedSelection;

  // When right-clicking a single line, also set lineId so the context
  // menu can offer "Toggle Construction" alongside Delete.
  const lineId =
    hit.kind === "sketch_entity" && hit.entityKind === "line" && hit.id
      ? hit.id
      : null;

  return {
    contextMenu: {
      x,
      y,
      referenceId: null,
      faceId: null,
      lineId,
      sketchDeleteSelection: selection,
    },
  };
}

export function buildViewportContextMenuState({
  activeSketchPlaneId,
  document,
  hit,
  x,
  y,
  solidFaces,
}: ContextMenuBuildInput): ContextMenuBuildResult {
  if (activeSketchPlaneId) {
    return sketchContextMenu({ document, hit, x, y }) ?? { contextMenu: null };
  }

  if (
    hit?.kind !== "reference" &&
    hit?.kind !== "face" &&
    hit?.kind !== "primitive" &&
    hit?.kind !== "edge" &&
    hit?.kind !== "vertex"
  ) {
    return { contextMenu: null };
  }

  const solidFace =
    hit.kind === "face"
      ? solidFaces.find((face) => face.faceId === hit.id)
      : null;
  const bodyId =
    hit.kind === "primitive"
      ? hit.id
      : hit.kind === "face"
        ? (solidFace?.ownerId ?? bodyIdFromTopologyId(hit.id))
        : hit.kind === "edge" || hit.kind === "vertex"
          ? bodyIdFromTopologyId(hit.id)
          : null;

  return {
    contextMenu: {
      x,
      y,
      referenceId: hit.kind === "reference" ? hit.id : null,
      faceId: hit.kind === "face" ? hit.id : null,
      bodyId,
      sketchDeleteSelection: null,
    },
  };
}
