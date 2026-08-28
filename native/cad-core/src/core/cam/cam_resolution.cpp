#include "core/cam/cam_resolution.h"

#include <variant>

#include "core/cam/cam_operation.h"
#include "core/cam/cam_profile_reference.h"
#include "core/document/feature.h"
#include "core/geometry/body_compiler.h"

namespace polysmith::core {

ResolvedFaceRef resolve_face_attestation(const FaceAttestation& attestation,
                                         const CompiledBodies& bodies) {
  // Synthesize the runtime witness type the existing face machinery
  // scores against, then try every body (the serialized attestation
  // carries no body id — bodies are rebuilt on every recompute).
  CamFaceReference synthesized;
  synthesized.capturedArea = attestation.area;
  synthesized.capturedNormal = attestation.normal;
  for (const auto& point : attestation.sample_points) {
    synthesized.samplePoints.push_back(point);
  }

  ResolvedFaceRef best;
  for (const auto& body : bodies.bodies) {
    synthesized.bodyId = body.id;
    const auto result = resolve_face_reference(synthesized, body.shape);
    if (result.outcome == FaceResolutionOutcome::Found &&
        !result.candidates.empty() &&
        result.candidates.front().score > best.score) {
      best = ResolvedFaceRef{true, &body, result.candidates.front().faceIndex,
                             result.candidates.front().score};
    }
  }
  return best;
}

ResolvedProfileRef resolve_profile_attestation(
    const SketchProfileAttestation& attestation, const FeatureEntry& sketch) {
  if (!sketch.sketch_parameters.has_value()) {
    return {};
  }
  const auto reference =
      cam_profile_reference_from_attestation(attestation);
  if (!reference.has_value()) {
    return {};
  }
  const auto result =
      resolve_profile_reference(*reference, sketch.sketch_parameters.value());
  if (result.outcome != ProfileResolutionOutcome::Found ||
      result.candidates.empty()) {
    return {};
  }
  return ResolvedProfileRef{true, result.candidates.front().region,
                            result.candidates.front().score};
}

std::string face_reference_failure_message(const ResolvedFaceRef& face) {
  return face.found
             ? ""
             : "The face used by this operation was not found "
               "(geometry changed — re-select it).";
}

std::string profile_reference_failure_message(
    const ResolvedProfileRef& profile) {
  return profile.found
             ? ""
             : "The sketch profile used by this operation was not found "
               "(geometry changed — re-select it).";
}

}  // namespace polysmith::core
