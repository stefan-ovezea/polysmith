import type { DocumentState, ViewportState } from "../types/ipc";

type Translate = (key: string, options?: Record<string, unknown>) => string;

interface BaseSelectionSourceContext {
  document: DocumentState | null;
  viewport: ViewportState | null;
}

interface PlaneSourceContext extends BaseSelectionSourceContext {
  selectedSketchProfileIds: readonly string[];
  sketchProfileLabelById: ReadonlyMap<string, string>;
  translate: Translate;
}

interface AxisSourceContext extends BaseSelectionSourceContext {
  sketchLineLabelById: ReadonlyMap<string, string>;
  translate: Translate;
}

interface ThreadTargetContext extends BaseSelectionSourceContext {
  translate: Translate;
}

export interface ThreadTargetSource {
  bodyId: string;
  summary: string;
}

export function describePlaneSource(
  context: PlaneSourceContext,
  referenceId: string,
): string {
  if (referenceId === "ref-plane-xy") return context.translate("geometry.xyPlane");
  if (referenceId === "ref-plane-yz") return context.translate("geometry.yzPlane");
  if (referenceId === "ref-plane-xz") return context.translate("geometry.xzPlane");

  const feature = context.document?.feature_history.find(
    (entry) => entry.feature_id === referenceId,
  );
  if (feature) {
    return feature.name || feature.kind;
  }

  const profileLabel = context.sketchProfileLabelById.get(referenceId);
  if (profileLabel) {
    return profileLabel;
  }

  const face = context.viewport?.solid_faces.find(
    (entry) => entry.face_id === referenceId,
  );
  if (face) {
    return (
      face.label ||
      context.translate("geometry.ownerFace", { owner: face.owner_kind })
    );
  }

  return context.translate("geometry.selectedPlane");
}

export function currentPlaneLikeSourceId(
  context: PlaneSourceContext,
): string | null {
  const preselectedReference = context.document?.selected_reference_id ?? null;
  const preselectedFaceId = context.document?.selected_face_id ?? null;
  const preselectedFace = preselectedFaceId
    ? (context.viewport?.solid_faces.find(
        (entry) => entry.face_id === preselectedFaceId,
      ) ?? null)
    : null;
  const preselectedProfileId =
    context.document?.selected_sketch_profile_id ??
    context.selectedSketchProfileIds[
      context.selectedSketchProfileIds.length - 1
    ] ??
    null;

  return (
    preselectedReference ??
    (preselectedFace && preselectedFace.sketchability === "planar"
      ? preselectedFaceId
      : null) ??
    preselectedProfileId
  );
}

export function currentFaceSourceId(
  documentState: DocumentState | null,
): string | null {
  return documentState?.selected_face_id ?? null;
}

export function currentAxisSourceId(
  context: AxisSourceContext,
): string | null {
  const selectedEdgeId = context.document?.selected_edge_ids[0] ?? null;
  if (selectedEdgeId) {
    return selectedEdgeId;
  }

  const selectedSketchEntityId =
    context.document?.selected_sketch_entity_id ?? null;
  if (
    selectedSketchEntityId &&
    context.sketchLineLabelById.has(selectedSketchEntityId)
  ) {
    return selectedSketchEntityId;
  }

  const selectedFeatureId = context.document?.selected_feature_id ?? null;
  const selectedFeature = selectedFeatureId
    ? context.document?.feature_history.find(
        (feature) => feature.feature_id === selectedFeatureId,
      )
    : null;
  if (selectedFeature?.kind === "construction_axis") {
    return selectedFeature.feature_id;
  }

  return null;
}

export function describeAxisSource(
  context: AxisSourceContext,
  axisId: string,
): string {
  const feature = context.document?.feature_history.find(
    (entry) => entry.feature_id === axisId,
  );
  return (
    context.sketchLineLabelById.get(axisId) ??
    feature?.name ??
    context.translate("geometry.selectedAxis")
  );
}

export function currentThreadTargetBody(
  context: BaseSelectionSourceContext,
): ThreadTargetSource | null {
  const selectedFaceId = context.document?.selected_face_id ?? null;
  if (selectedFaceId) {
    const face = context.viewport?.solid_faces.find(
      (entry) => entry.face_id === selectedFaceId,
    );
    if (face) {
      const bodyLabel =
        context.viewport?.bodies.find((body) => body.id === face.owner_id)
          ?.label ?? face.owner_id;
      return {
        bodyId: face.owner_id,
        summary: `${bodyLabel} · ${face.label}`,
      };
    }
  }

  const selectedFeatureId = context.document?.selected_feature_id ?? null;
  if (selectedFeatureId) {
    const body = context.viewport?.bodies.find(
      (entry) => entry.id === selectedFeatureId,
    );
    if (body) {
      return { bodyId: body.id, summary: body.label };
    }
  }

  return null;
}

export function describeThreadTarget(
  context: ThreadTargetContext,
  bodyId: string,
): string {
  return (
    context.viewport?.bodies.find((body) => body.id === bodyId)?.label ??
    context.document?.feature_history.find(
      (feature) => feature.feature_id === bodyId,
    )?.name ??
    context.translate("geometry.selectedBody")
  );
}

export function currentPointSourceId(
  documentState: DocumentState | null,
): string | null {
  const selectedVertexId = documentState?.selected_vertex_ids[0] ?? null;
  if (selectedVertexId) {
    return selectedVertexId;
  }
  return documentState?.selected_sketch_point_id ?? null;
}
