#include "core/cam/cam_resolution.h"

#include <variant>

#include "core/cam/cam_operation.h"
#include "core/cam/cam_profile_reference.h"
#include "core/document/document.h"
#include "core/document/feature.h"
#include "core/geometry/body_compiler.h"
#include "core/pcb/pcb_layer_io.h"

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

namespace {
// The witness is advisory: a re-poured or re-plotted layer is a *different
// shape*, not a broken reference, so a mismatch warns instead of dropping the
// operation.  10 µm is below any PCB process tolerance but above the rounding
// noise of a re-plot.
constexpr double kPcbWitnessToleranceMm = 0.01;

bool layer_bounds_match(const PcbLayerAttestation& attestation,
                        const polysmith::core::pcb::PcbBounds& bounds) {
  if (!bounds.valid) {
    return false;
  }
  return std::abs(bounds.min_x - attestation.min_x) <= kPcbWitnessToleranceMm &&
         std::abs(bounds.min_y - attestation.min_y) <= kPcbWitnessToleranceMm &&
         std::abs(bounds.max_x - attestation.max_x) <= kPcbWitnessToleranceMm &&
         std::abs(bounds.max_y - attestation.max_y) <= kPcbWitnessToleranceMm;
}
}  // namespace

std::optional<PcbLayerAttestation> pcb_layer_attestation_from_feature(
    const DocumentState& document, const std::string& feature_id) {
  for (const FeatureEntry& feature : document.feature_history) {
    if (feature.id != feature_id || !feature.pcb_layer_parameters.has_value()) {
      continue;
    }
    const auto& params = *feature.pcb_layer_parameters;
    PcbLayerAttestation attestation;
    attestation.layer_feature_id = feature.id;
    attestation.role = polysmith::core::pcb_role_id(params.role);
    attestation.ring_count = params.ring_count;
    attestation.min_x = params.min_x;
    attestation.min_y = params.min_y;
    attestation.max_x = params.max_x;
    attestation.max_y = params.max_y;
    return attestation;
  }
  return std::nullopt;
}

ResolvedPcbLayerRef resolve_pcb_layer_attestation(
    const PcbLayerAttestation& attestation, const DocumentState& document) {
  ResolvedPcbLayerRef resolved;
  const FeatureEntry* layer = nullptr;
  for (const FeatureEntry& feature : document.feature_history) {
    if (feature.id == attestation.layer_feature_id) {
      layer = &feature;
      break;
    }
  }
  if (layer == nullptr) {
    resolved.error =
        "The PCB layer used by this operation is no longer in the document.";
    return resolved;
  }
  if (!layer->pcb_layer_parameters.has_value()) {
    resolved.error =
        "The feature this operation referenced is no longer a PCB layer.";
    return resolved;
  }

  // Re-derive from the source rather than trusting the summary stored on the
  // feature: the file is the truth (the project's re-resolve-every-recompute
  // rule), and a layer whose file was re-plotted must cut the new copper.
  const polysmith::core::pcb::PcbLayerLoad loaded =
      polysmith::core::pcb::load_pcb_layer(layer->pcb_layer_parameters->file_path);
  if (!loaded.ok) {
    resolved.error = loaded.error.empty()
                         ? std::string("The PCB layer's source file could not "
                                       "be read — re-import it.")
                         : loaded.error;
    return resolved;
  }
  if (loaded.ring_count == 0 && loaded.hit_count == 0) {
    resolved.error = "The PCB layer's source produced no geometry to cut.";
    return resolved;
  }

  resolved.found = true;
  resolved.ring_count = loaded.ring_count;
  resolved.hit_count = loaded.hit_count;
  resolved.tool_count = loaded.tool_count;
  resolved.min_x = loaded.bounds.min_x;
  resolved.min_y = loaded.bounds.min_y;
  resolved.max_x = loaded.bounds.max_x;
  resolved.max_y = loaded.bounds.max_y;
  resolved.changed = loaded.ring_count != attestation.ring_count ||
                     !layer_bounds_match(attestation, loaded.bounds);
  return resolved;
}

std::string pcb_layer_reference_failure_message(
    const ResolvedPcbLayerRef& layer) {
  if (layer.found) {
    return "";
  }
  return layer.error.empty()
             ? "The PCB layer used by this operation could not be resolved."
             : layer.error;
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
    const std::function<void(const ResolvedPcbLayerRef&, const FeatureEntry&)>&
        on_pcb_layer,
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
  if (std::holds_alternative<PcbLayerAttestation>(ref.attestation)) {
    const auto& attestation = std::get<PcbLayerAttestation>(ref.attestation);
    const auto resolved = resolve_pcb_layer_attestation(attestation, document);
    if (!resolved.found) {
      message = pcb_layer_reference_failure_message(resolved);
      return false;
    }
    if (on_pcb_layer) {
      const FeatureEntry* layer = nullptr;
      for (const auto& feature : document.feature_history) {
        if (feature.id == attestation.layer_feature_id) {
          layer = &feature;
          break;
        }
      }
      if (layer != nullptr) {
        on_pcb_layer(resolved, *layer);
      }
    }
    return true;
  }
  // Unknown attestation variant — defensive fallback.
  message = "The geometry reference type is not supported.";
  return false;
}

}  // namespace polysmith::core
