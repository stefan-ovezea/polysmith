import type { DocumentState, FeatureEntry } from "@/types";

// Walk the feature history and return every feature that references the
// given `featureId` directly. Used by the Delete confirmation flow so
// users get told "this will break N downstream features" before they
// blow away a base sketch or a body that has fillets on it.
//
// Reference rules (matched against the C++ shape builders):
//   - extrude.sketch_feature_id  → its source sketch
//   - extrude.target_body_id     → join/cut target
//   - loft.sections[].sketch_feature_id → source sketches
//   - revolve.sketch_feature_id / axis_sketch_feature_id → source sketches
//   - sweep.sketch_feature_id / path_sketch_feature_id → source sketches
//   - fillet.target_body_id      → body being filleted
//   - chamfer.target_body_id     → body being chamfered
//   - shell.target_body_id       → body being shelled
//   - hole.target_body_id / source_face_id → body being cut
//   - helix.axis_source_id / thread.axis_source_id → construction axis source
//   - thread.target_body_id      → body receiving cosmetic/modeled thread
//   - sketch.plane_id            → plane / construction plane / face
//                                  the sketch was placed on
//   - construction_plane.source_plane_id / source_plane_ids / source_axis_id
//     → construction plane chain
//   - construction_axis.source_id / construction_point.source_id
//     → source body edge/vertex or source sketch entity/point
//
// We intentionally return the dependents in `feature_history` order
// (newest last) so the UI can render a stable list.
export function findDependents(
  document: DocumentState,
  featureId: string,
): FeatureEntry[] {
  const targetFeature = document.feature_history.find(
    (feature) => feature.feature_id === featureId,
  );
  return document.feature_history.filter(
    (feature) =>
      feature.feature_id !== featureId &&
      featureReferencesTarget(feature, featureId, targetFeature),
  );
}

function featureReferencesTarget(
  feature: FeatureEntry,
  featureId: string,
  targetFeature: FeatureEntry | undefined,
) {
  return (
    referencesSketchOrBody(feature, featureId) ||
    referencesConstruction(feature, featureId, targetFeature)
  );
}

function referencesSketchOrBody(feature: FeatureEntry, featureId: string) {
  return (
    equalsAnyFeatureId(featureId, [
      feature.extrude_parameters?.sketch_feature_id,
      feature.extrude_parameters?.target_body_id,
      feature.revolve_parameters?.sketch_feature_id,
      feature.revolve_parameters?.axis_sketch_feature_id,
      feature.sweep_parameters?.sketch_feature_id,
      feature.sweep_parameters?.path_sketch_feature_id,
      feature.fillet_parameters?.target_body_id,
      feature.chamfer_parameters?.target_body_id,
      feature.shell_parameters?.target_body_id,
      feature.hole_parameters?.target_body_id,
      feature.helix_parameters?.axis_source_id,
      feature.thread_parameters?.target_body_id,
      feature.thread_parameters?.axis_source_id,
      feature.sketch_parameters?.plane_id,
    ]) ||
    referencesFeatureOwnedFace(feature.hole_parameters?.source_face_id, featureId) ||
    referencesLoftSection(feature, featureId)
  );
}

function referencesConstruction(
  feature: FeatureEntry,
  featureId: string,
  targetFeature: FeatureEntry | undefined,
) {
  const constructionAxisId =
    feature.construction_plane_parameters?.source_axis_id ?? null;
  const constructionReferenceId =
    feature.construction_axis_parameters?.source_id ??
    feature.construction_point_parameters?.source_id ??
    null;

  return (
    equalsAnyFeatureId(featureId, [
      feature.construction_plane_parameters?.source_plane_id,
      constructionAxisId,
      constructionReferenceId,
    ]) ||
    (feature.construction_plane_parameters?.source_plane_ids ?? []).includes(
      featureId,
    ) ||
    sketchLineBelongsToFeature(constructionAxisId, targetFeature) ||
    constructionReferenceBelongsToFeature(
      constructionReferenceId,
      featureId,
      targetFeature,
    )
  );
}

function referencesLoftSection(feature: FeatureEntry, featureId: string) {
  return (
    feature.loft_parameters?.sections.some(
      (section) => section.sketch_feature_id === featureId,
    ) ?? false
  );
}

function referencesFeatureOwnedFace(
  sourceFaceId: string | undefined,
  featureId: string,
) {
  return sourceFaceId?.startsWith(`${featureId}:`) ?? false;
}

function constructionReferenceBelongsToFeature(
  referenceId: string | null,
  featureId: string,
  targetFeature: FeatureEntry | undefined,
) {
  return (
    referencesFeatureOwnedFace(referenceId ?? undefined, featureId) ||
    sketchLineBelongsToFeature(referenceId, targetFeature) ||
    sketchPointBelongsToFeature(referenceId, targetFeature)
  );
}

function sketchLineBelongsToFeature(
  lineId: string | null,
  targetFeature: FeatureEntry | undefined,
) {
  return (
    lineId != null &&
    (targetFeature?.sketch_parameters?.lines.some(
      (line) => line.line_id === lineId,
    ) ??
      false)
  );
}

function sketchPointBelongsToFeature(
  pointId: string | null,
  targetFeature: FeatureEntry | undefined,
) {
  return (
    pointId != null &&
    (targetFeature?.sketch_parameters?.points.some(
      (point) => point.point_id === pointId,
    ) ??
      false)
  );
}

function equalsAnyFeatureId(
  featureId: string,
  candidates: Array<string | null | undefined>,
) {
  return candidates.includes(featureId);
}
