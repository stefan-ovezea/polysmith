import type { DocumentState, ViewportState } from "../types";

export interface ViewportSelectionStateInput {
  document: DocumentState | null;
  viewport: ViewportState | null;
}

export function computeViewportSelectionState({
  document,
  viewport,
}: ViewportSelectionStateInput) {
  const selectedReference =
    viewport?.reference_planes.find(
      (referencePlane) => referencePlane.is_selected,
    ) ?? null;
  const selectedSketchableFace =
    document?.selected_face_id && viewport
      ? (viewport.solid_faces.find(
          (face) =>
            face.face_id === document.selected_face_id &&
            face.sketchability === "planar",
        ) ?? null)
      : null;
  const selectedMaterialFace =
    document?.selected_face_id && viewport
      ? (viewport.solid_faces.find(
          (face) => face.face_id === document.selected_face_id,
        ) ?? null)
      : null;
  const selectedMaterialBodyId =
    selectedMaterialFace?.owner_id ?? selectedDocumentBodyId(document, viewport);
  const selectedSketchProfile =
    viewport?.sketch_profiles.find((profile) => profile.is_selected) ?? null;
  const selectedSketchProfiles =
    viewport?.sketch_profiles.filter((profile) => profile.is_selected) ?? [];

  return {
    selectedReference,
    selectedSketchableFace,
    selectedMaterialFace,
    selectedMaterialBodyId,
    selectedSketchProfile,
    selectedSketchProfiles,
    selectedSketchProfileIds:
      document?.selected_sketch_profile_ids ??
      selectedSketchProfiles.map((profile) => profile.profile_id),
    selectedSketchEntityIds: document?.selected_sketch_entity_ids ?? [],
  };
}

function selectedDocumentBodyId(
  document: DocumentState | null,
  viewport: ViewportState | null,
) {
  const selectedFeatureId = document?.selected_feature_id;
  if (!selectedFeatureId) {
    return null;
  }
  return viewport?.bodies.some((body) => body.id === selectedFeatureId)
    ? selectedFeatureId
    : null;
}
