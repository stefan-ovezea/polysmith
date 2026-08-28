#include "core/cam/cam_refresh.h"

#include <variant>

#include "core/cam/cam_resolution.h"
#include "core/cam/cam_runtime.h"
#include "core/diagnostics/logger.h"
#include "core/document/document.h"
#include "core/geometry/body_compiler.h"

namespace polysmith::core {

namespace {

// Resolves every machining-region reference of an operation.
// Returns false and fills `message` on the first unresolved or
// unsupported reference.
bool resolve_operation_references(const DocumentState& document,
                                  const CamOperation& op,
                                  const CompiledBodies& bodies,
                                  std::string& message) {
  for (const auto& ref : op.geometry_references.machining_regions) {
    if (std::holds_alternative<SketchProfileAttestation>(
            ref.attestation)) {
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
        message =
            "The sketch used by this operation no longer exists.";
        return false;
      }
      const auto resolved =
          resolve_profile_attestation(attestation, *sketch);
      if (!resolved.found) {
        message = profile_reference_failure_message(resolved);
        return false;
      }
    } else if (std::holds_alternative<FaceAttestation>(ref.attestation)) {
      const auto resolved = resolve_face_attestation(
          std::get<FaceAttestation>(ref.attestation), bodies);
      if (!resolved.found) {
        message = face_reference_failure_message(resolved);
        return false;
      }
    } else {
      // EdgeAttestation — no v1 operation references edges yet.
      message = "Edge references are not supported yet.";
      return false;
    }
  }
  return true;
}

}  // namespace

void refresh_cam_dependencies(DocumentState& document, int target_revision) {
  if (document.cam.operations.empty()) {
    return;
  }

  // A revision bump invalidates every cached path for this document —
  // drop them so the viewport never draws stale toolpaths.
  cam_runtime::drop_stale(document, target_revision);

  // Compile bodies once for all face-resolving operations.  Cheap pass
  // (no meshes) and skipped entirely when no op references faces.
  CompiledBodies bodies;
  bool compiled = false;
  auto ensure_bodies = [&]() {
    if (!compiled) {
      bodies = compile_bodies(document, /*include_meshes=*/false);
      compiled = true;
    }
  };

  for (auto& op : document.cam.operations) {
    if (!op.enabled) {
      continue;
    }
    bool needs_faces = false;
    for (const auto& ref : op.geometry_references.machining_regions) {
      if (std::holds_alternative<FaceAttestation>(ref.attestation)) {
        needs_faces = true;
        break;
      }
    }
    if (needs_faces) {
      ensure_bodies();
    }

    std::string message;
    if (!resolve_operation_references(document, op, bodies, message)) {
      if (op.status != "error") {
        polysmith::core::log_warn(
            "cam", "operation '" + op.name + "' degraded: " + message);
      }
      op.status = "error";
      op.status_message = message;
      op.toolpath_cache.reset();
      continue;
    }

    // References resolve cleanly.  A toolpath cached at the CURRENT
    // revision means the operation is genuinely up to date; anything
    // else needs regeneration.  (Generation itself does not bump the
    // revision — it is derived data, not document state — so a path
    // stored at revision N stays valid until the next real mutation.)
    if (cam_runtime::cached_toolpath_at(document, op.op_id,
                                        target_revision) != nullptr) {
      op.status = "generated";
      op.status_message.clear();
    } else if (op.status != "pending") {
      // Freshly created ops stay "pending"; anything that once had a
      // path (or was degraded) needs a regenerate after this bump.
      op.status = "needs_regenerate";
      op.status_message.clear();
    }
  }
}

}  // namespace polysmith::core
