import type { DocumentState, ViewportState } from "../types";

export function selectedDocumentBodyId(
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
