import type { SketchDeleteSelection } from "./appState";
import type { DocumentState, FeatureEntry } from "../types";

export function currentSketchDeleteSelection(
  document: DocumentState | null,
): SketchDeleteSelection {
  return {
    entityIds: dedupeSelectedIds(
      document?.selected_sketch_entity_ids,
      document?.selected_sketch_entity_id,
    ),
    pointIds: dedupeSelectedIds(
      document?.selected_sketch_vertex_ids,
      document?.selected_sketch_vertex_id,
    ),
    profileIds: dedupeSelectedIds(
      document?.selected_sketch_profile_ids,
      document?.selected_sketch_profile_id,
    ),
  };
}

function dedupeSelectedIds(
  ids: readonly string[] | undefined,
  focusedId: string | null | undefined,
) {
  const next = new Set(ids ?? []);
  if (focusedId) {
    next.add(focusedId);
  }
  return [...next];
}

export function extrudesAffectedBySketchSelection({
  document,
  activeSketchFeature,
  selection,
}: {
  document: DocumentState | null;
  activeSketchFeature: FeatureEntry | null | undefined;
  selection: SketchDeleteSelection;
}): FeatureEntry[] {
  if (!document?.active_sketch_feature_id || !activeSketchFeature) {
    return [];
  }

  const sketch = activeSketchFeature.sketch_parameters;
  if (!sketch) {
    return [];
  }

  const affectedEntityIds = sketchEntityIdsForDeleteSelection(selection, sketch);
  const affectedProfileIds = sketchProfileIdsForAffectedEntities(
    selection.profileIds,
    affectedEntityIds,
    sketch,
  );
  if (affectedProfileIds.size === 0) {
    return [];
  }

  return document.feature_history.filter((feature) =>
    extrudeUsesAffectedProfile(
      feature,
      document.active_sketch_feature_id,
      affectedProfileIds,
    ),
  );
}

function sketchEntityIdsForDeleteSelection(
  selection: SketchDeleteSelection,
  sketch: NonNullable<FeatureEntry["sketch_parameters"]>,
) {
  const entityIds = new Set(selection.entityIds);
  for (const pointId of selection.pointIds) {
    addLineIdsForPoint(entityIds, sketch, pointId);
    addArcIdsForPoint(entityIds, sketch, pointId);
    addCircleIdsForCenterPoint(entityIds, sketch, pointId);
  }
  return entityIds;
}

function addLineIdsForPoint(
  entityIds: Set<string>,
  sketch: NonNullable<FeatureEntry["sketch_parameters"]>,
  pointId: string,
) {
  for (const line of sketch.lines) {
    if (line.start_point_id === pointId || line.end_point_id === pointId ||
        line.start_vertex_id === pointId || line.end_vertex_id === pointId) {
      entityIds.add(line.line_id);
    }
  }
}

function addArcIdsForPoint(
  entityIds: Set<string>,
  sketch: NonNullable<FeatureEntry["sketch_parameters"]>,
  pointId: string,
) {
  for (const arc of sketch.arcs ?? []) {
    if (arc.start_point_id === pointId || arc.end_point_id === pointId ||
        arc.start_vertex_id === pointId || arc.end_vertex_id === pointId) {
      entityIds.add(arc.arc_id);
    }
  }
}

function addCircleIdsForCenterPoint(
  entityIds: Set<string>,
  sketch: NonNullable<FeatureEntry["sketch_parameters"]>,
  pointId: string,
) {
  for (const circle of sketch.circles) {
    if (`point-circle-${circle.circle_id}-center` === pointId ||
        circle.center_vertex_id === pointId) {
      entityIds.add(circle.circle_id);
    }
  }
}

function sketchProfileIdsForAffectedEntities(
  selectedProfileIds: readonly string[],
  entityIds: Set<string>,
  sketch: NonNullable<FeatureEntry["sketch_parameters"]>,
) {
  const affectedProfileIds = new Set(selectedProfileIds);
  for (const profile of sketch.profiles) {
    const usesSelectedLine = profile.line_ids.some((id) => entityIds.has(id));
    const usesSelectedCircle =
      profile.source_circle_id !== null &&
      entityIds.has(profile.source_circle_id);
    if (usesSelectedLine || usesSelectedCircle) {
      affectedProfileIds.add(profile.profile_id);
    }
  }
  return affectedProfileIds;
}

function extrudeUsesAffectedProfile(
  feature: FeatureEntry,
  activeSketchFeatureId: string,
  affectedProfileIds: Set<string>,
) {
  if (
    feature.kind !== "extrude" ||
    !feature.extrude_parameters ||
    feature.extrude_parameters.sketch_feature_id !== activeSketchFeatureId
  ) {
    return false;
  }

  const sourceProfileIds =
    feature.extrude_parameters.profile_ids.length > 0
      ? feature.extrude_parameters.profile_ids
      : [feature.extrude_parameters.profile_id];
  return sourceProfileIds.some((profileId) =>
    affectedProfileIds.has(profileId),
  );
}
