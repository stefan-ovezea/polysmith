import type { ExtrudeMode, ViewportState } from "../types";
import type { DocumentState } from "../types/ipc";
import { bodyIdFromFaceId } from "./appState";

export interface DefaultExtrudeSettings {
  mode: ExtrudeMode;
  targetBodyId: string | null;
}

export function resolveDefaultExtrudeSettings({
  document,
  viewport,
  profileIds,
  faceIdOverride = null,
}: {
  document: DocumentState | null;
  viewport: ViewportState | null;
  profileIds: readonly string[];
  faceIdOverride?: string | null;
}): DefaultExtrudeSettings {
  const bodyIds = new Set((viewport?.bodies ?? []).map((body) => body.id));

  if (profileIds.length === 0) {
    return resolveFaceExtrudeSettings(
      bodyIds,
      faceIdOverride ?? document?.selected_face_id ?? null,
    );
  }

  return resolveProfileExtrudeSettings(document, bodyIds, profileIds);
}

function resolveFaceExtrudeSettings(
  bodyIds: ReadonlySet<string>,
  faceId: string | null,
): DefaultExtrudeSettings {
  const selectedFaceBodyId = bodyIdFromFaceId(faceId);
  if (selectedFaceBodyId && bodyIds.has(selectedFaceBodyId)) {
    return { mode: "join", targetBodyId: selectedFaceBodyId };
  }
  return newBodySettings();
}

function resolveProfileExtrudeSettings(
  document: DocumentState | null,
  bodyIds: ReadonlySet<string>,
  profileIds: readonly string[],
): DefaultExtrudeSettings {
  let sourceBodyId: string | null = null;
  for (const profileId of profileIds) {
    const nextBodyId = bodyIdFromProfileId(document, profileId);
    if (!nextBodyId || !bodyIds.has(nextBodyId)) {
      return newBodySettings();
    }
    if (sourceBodyId && sourceBodyId !== nextBodyId) {
      return newBodySettings();
    }
    sourceBodyId = nextBodyId;
  }
  return sourceBodyId
    ? { mode: "join", targetBodyId: sourceBodyId }
    : newBodySettings();
}

function bodyIdFromProfileId(
  document: DocumentState | null,
  profileId: string,
) {
  const sketchFeature = document?.feature_history.find((feature) => {
    if (feature.kind !== "sketch" || !feature.sketch_parameters) {
      return false;
    }
    return feature.sketch_parameters.profiles.some(
      (profile) => profile.profile_id === profileId,
    );
  });
  return bodyIdFromFaceId(sketchFeature?.sketch_parameters?.plane_id);
}

function newBodySettings(): DefaultExtrudeSettings {
  return { mode: "new_body", targetBodyId: null };
}
