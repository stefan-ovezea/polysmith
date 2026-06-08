import type { CategoryId } from "../layout/DocumentHierarchyPanel";
import type { DocumentState } from "../types/ipc";
import { BODY_KINDS } from "./appState";

type FeatureHistoryEntry = DocumentState["feature_history"][number];
type MutableRef<T> = { current: T };
type HiddenCategoryUpdater = (
  updater: (current: Set<CategoryId>) => Set<CategoryId>,
) => void;

export function syncDefaultOriginVisibility({
  documentId,
  hasSolidBody,
  previousDocumentIdRef,
  originVisibilityManuallyChangedRef,
  setHiddenCategories,
}: {
  documentId: string | null;
  hasSolidBody: boolean;
  previousDocumentIdRef: MutableRef<string | null>;
  originVisibilityManuallyChangedRef: MutableRef<boolean>;
  setHiddenCategories: HiddenCategoryUpdater;
}) {
  if (previousDocumentIdRef.current !== documentId) {
    previousDocumentIdRef.current = documentId;
    originVisibilityManuallyChangedRef.current = false;
    setOriginHidden(setHiddenCategories, Boolean(documentId && hasSolidBody));
    return;
  }

  if (
    !documentId ||
    !hasSolidBody ||
    originVisibilityManuallyChangedRef.current
  ) {
    return;
  }

  setOriginHidden(setHiddenCategories, true);
}

function setOriginHidden(
  setHiddenCategories: HiddenCategoryUpdater,
  shouldHideOrigin: boolean,
) {
  setHiddenCategories((current) => {
    const next = new Set(current);
    if (shouldHideOrigin) {
      next.add("origin");
    } else {
      next.delete("origin");
    }
    return equivalentSets(current, next) ? current : next;
  });
}

function equivalentSets(left: ReadonlySet<string>, right: ReadonlySet<string>) {
  if (left.size !== right.size) {
    return false;
  }
  for (const item of left) {
    if (!right.has(item)) {
      return false;
    }
  }
  return true;
}

export function computeEffectiveHiddenFeatureIds({
  document,
  hiddenFeatureIds,
  hiddenCategories,
  timelineEditVisibleFeatureIds,
}: {
  document: DocumentState | null;
  hiddenFeatureIds: ReadonlySet<string>;
  hiddenCategories: ReadonlySet<string>;
  timelineEditVisibleFeatureIds: ReadonlySet<string>;
}) {
  const set = new Set<string>(hiddenFeatureIds);
  if (!document) {
    return set;
  }
  for (const feature of document.feature_history) {
    if (isHiddenByCategory(feature, hiddenCategories)) {
      set.add(feature.feature_id);
    }
  }
  for (const featureId of timelineEditVisibleFeatureIds) {
    set.delete(featureId);
  }
  return set;
}

function isHiddenByCategory(
  feature: FeatureHistoryEntry,
  hiddenCategories: ReadonlySet<string>,
) {
  if (isSketchCategoryFeature(feature, hiddenCategories)) {
    return true;
  }
  if (isBodyCategoryFeature(feature, hiddenCategories)) {
    return true;
  }
  if (isConstructionCategoryFeature(feature, hiddenCategories)) {
    return true;
  }
  return false;
}

function isSketchCategoryFeature(
  feature: FeatureHistoryEntry,
  hiddenCategories: ReadonlySet<string>,
) {
  if (feature.kind !== "sketch") {
    return false;
  }
  return hiddenCategories.has("sketches");
}

function isBodyCategoryFeature(
  feature: FeatureHistoryEntry,
  hiddenCategories: ReadonlySet<string>,
) {
  if (!BODY_KINDS.has(feature.kind)) {
    return false;
  }
  return hiddenCategories.has("bodies");
}

function isConstructionCategoryFeature(
  feature: FeatureHistoryEntry,
  hiddenCategories: ReadonlySet<string>,
) {
  if (!isConstructionFeature(feature)) {
    return false;
  }
  return hiddenCategories.has("construction");
}

function isConstructionFeature(feature: FeatureHistoryEntry) {
  return (
    feature.kind === "construction_plane" ||
    feature.kind === "construction_axis" ||
    feature.kind === "construction_point"
  );
}

export function computeHiddenSketchPlaneIds(
  document: DocumentState | null,
  effectiveHiddenFeatureIds: ReadonlySet<string>,
) {
  const result = new Set<string>();
  if (!document) {
    return result;
  }
  const planeToSketches = groupSketchesByPlane(document);
  for (const [planeId, sketchIds] of planeToSketches) {
    if (areAllSketchesHidden(sketchIds, effectiveHiddenFeatureIds)) {
      result.add(planeId);
    }
  }
  return result;
}

function groupSketchesByPlane(document: DocumentState) {
  const planeToSketches = new Map<string, string[]>();
  for (const feature of document.feature_history) {
    if (feature.kind !== "sketch" || !feature.sketch_parameters) {
      continue;
    }
    const planeId = feature.sketch_parameters.plane_id;
    const list = planeToSketches.get(planeId) ?? [];
    list.push(feature.feature_id);
    planeToSketches.set(planeId, list);
  }
  return planeToSketches;
}

function areAllSketchesHidden(
  sketchIds: readonly string[],
  effectiveHiddenFeatureIds: ReadonlySet<string>,
) {
  return sketchIds.every((id) => effectiveHiddenFeatureIds.has(id));
}
