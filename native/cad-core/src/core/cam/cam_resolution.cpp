#include "core/cam/cam_resolution.h"

#include <variant>

#include "core/cam/cam_operation.h"
#include "core/cam/cam_profile_reference.h"
#include "core/document/document.h"
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

ResolvedEdgeRef resolve_edge_attestation(const EdgeAttestation& attestation,
                                         const CompiledBodies& bodies) {
  // Synthesize the runtime witness type the edge machinery scores
  // against, then try every body (the serialized attestation carries
  // no body id — bodies are rebuilt on every recompute).
  CamEdgeReference synthesized;
  synthesized.startPoint = attestation.start_point;
  synthesized.endPoint = attestation.end_point;
  synthesized.length = attestation.length;
  synthesized.tangent = attestation.tangent;
  synthesized.center = attestation.center;
  synthesized.axis = attestation.axis;
  synthesized.radius = attestation.radius;

  ResolvedEdgeRef best;
  for (const auto& body : bodies.bodies) {
    synthesized.bodyId = body.id;
    const auto result = resolve_edge_reference(synthesized, body.shape);
    if (result.outcome == EdgeResolutionOutcome::Found &&
        !result.candidates.empty() &&
        result.candidates.front().score > best.score) {
      best = ResolvedEdgeRef{true, &body, result.candidates.front().edgeIndex,
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

std::string edge_reference_failure_message(const ResolvedEdgeRef& edge) {
  return edge.found
             ? ""
             : "The edge used by this operation was not found "
               "(geometry changed — re-select it).";
}

std::string profile_reference_failure_message(
    const ResolvedProfileRef& profile) {
  return profile.found
             ? ""
             : "The sketch profile used by this operation was not found "
               "(geometry changed — re-select it).";
}

const CamSetup* setup_for(const DocumentState& document,
                          const CamOperation& op) {
  if (!op.setup_id.empty()) {
    for (const auto& setup : document.cam.setups) {
      if (setup.setup_id == op.setup_id) {
        return &setup;
      }
    }
  }
  return document.cam.setups.empty() ? nullptr : &document.cam.setups[0];
}

bool resolve_geometry_reference(
    const GeometryReference& ref, const DocumentState& document,
    const CompiledBodies& bodies,
    const std::function<void(const ResolvedProfileRef&, const FeatureEntry&)>&
        on_profile,
    const std::function<void(const ResolvedFaceRef&)>& on_face,
    const std::function<void(const ResolvedEdgeRef&)>& on_edge,
    const std::function<void(const PointAttestation&)>& on_point,
    std::string& message) {
  if (std::holds_alternative<PointAttestation>(ref.attestation)) {
    // A bare coordinate is its own identity — nothing to resolve
    // (TNP doctrine: never trust topology; a point reference is not
    // topology).  The generator consumes the raw world coordinates.
    if (on_point) {
      on_point(std::get<PointAttestation>(ref.attestation));
    }
    return true;
  }
  if (std::holds_alternative<SketchProfileAttestation>(ref.attestation)) {
    const auto& attestation =
        std::get<SketchProfileAttestation>(ref.attestation);
    const FeatureEntry* sketch = nullptr;
    for (const auto& feature : document.feature_history) {
      if (feature.id == attestation.sketch_feature_id) {
        sketch = &feature;
        break;
      }
    }
    if (sketch == nullptr || !sketch->sketch_parameters.has_value()) {
      message = "The sketch used by this operation no longer exists.";
      return false;
    }
    const auto resolved =
        resolve_profile_attestation(attestation, *sketch);
    if (!resolved.found) {
      message = profile_reference_failure_message(resolved);
      return false;
    }
    if (on_profile) {
      on_profile(resolved, *sketch);
    }
    return true;
  }
  if (std::holds_alternative<FaceAttestation>(ref.attestation)) {
    const auto resolved = resolve_face_attestation(
        std::get<FaceAttestation>(ref.attestation), bodies);
    if (!resolved.found) {
      message = face_reference_failure_message(resolved);
      return false;
    }
    if (on_face) {
      on_face(resolved);
    }
    return true;
  }
  if (std::holds_alternative<EdgeAttestation>(ref.attestation)) {
    const auto resolved = resolve_edge_attestation(
        std::get<EdgeAttestation>(ref.attestation), bodies);
    if (!resolved.found) {
      message = edge_reference_failure_message(resolved);
      return false;
    }
    if (on_edge) {
      on_edge(resolved);
    }
    return true;
  }
  // Unknown attestation variant — defensive fallback.
  message = "The geometry reference type is not supported.";
  return false;
}

}  // namespace polysmith::core
