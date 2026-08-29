#include "core/cam/cam_refresh.h"

#include <variant>

#include "core/cam/cam_planning.h"
#include "core/cam/cam_resolution.h"
#include "core/cam/cam_runtime.h"
#include "core/diagnostics/logger.h"
#include "core/document/document.h"
#include "core/geometry/body_compiler.h"

namespace polysmith::core {

void refresh_cam_dependencies(DocumentState& document, int target_revision) {
  // A revision bump invalidates every cached path for this document —
  // drop them so the viewport never draws stale toolpaths.
  cam_runtime::drop_stale(document, target_revision);

  // Compile bodies once for all face-resolving work (operation
  // references AND face-anchored WCS origins).  Cheap pass (no
  // meshes) and skipped entirely when nothing references faces.
  CompiledBodies bodies;
  bool compiled = false;
  auto ensure_bodies = [&]() {
    if (!compiled) {
      bodies = compile_bodies(document, /*include_meshes=*/false);
      compiled = true;
    }
  };

  // ── WCS origins (every setup) ──────────────────────────────────
  // The machine origin the exporter subtracts comes from the setup's
  // stock origin, or from a FACE-anchored WCS (the resolved face's
  // mid-UV point — TNP-safe via the face attestation).  Laser setups
  // with machine settings then subtract the RED POINTER offset:
  // parts are framed under the dot, but the laser fires offset from
  // it — shifting the origin back makes the cut land where the dot
  // was.
  for (auto& setup : document.cam.setups) {
    const bool faceAnchored =
        !setup.wcs_origin.face_reference.persistent_id.empty();
    std::optional<std::array<double, 3>> position;
    if (faceAnchored) {
      ensure_bodies();
      const auto resolved = resolve_face_attestation(
          std::get<FaceAttestation>(
              setup.wcs_origin.face_reference.attestation),
          bodies);
      if (resolved.found) {
        TopoDS_Face face;
        std::string faceError;
        if (cam_planning::map_face_index(resolved.body->shape,
                                         resolved.faceIndex, face,
                                         faceError)) {
          gp_Pnt center;
          gp_Vec normal;
          if (cam_planning::face_cut_plane(face, center, normal)) {
            position = std::array<double, 3>{center.X(), center.Y(),
                                             center.Z()};
          }
        }
      }
      if (!position.has_value()) {
        polysmith::core::log_warn(
            "cam", "the WCS face reference of setup '" + setup.name +
                       "' no longer resolves — the WCS falls back to the "
                       "stock origin.");
        position = setup.stock.origin;
      }
    } else {
      position = setup.stock.origin;
    }

    if (setup.machine_type == "laser" &&
        document.cam.machine_settings.has_value() && position.has_value()) {
      const auto& machine = document.cam.machine_settings.value();
      (*position)[0] -= machine.pointer_offset_x_mm;
      (*position)[1] -= machine.pointer_offset_y_mm;
    }
    setup.wcs_origin.position = position;
  }

  // ── Operation references ───────────────────────────────────────
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

    // Shared resolution with the generate driver — one source of
    // truth for TNP re-resolution.  Empty sinks: the refresh pass only
    // needs the found flag.
    bool resolved_all = true;
    std::string message;
    for (const auto& ref : op.geometry_references.machining_regions) {
      if (!resolve_geometry_reference(ref, document, bodies,
                                      /*on_profile=*/{},
                                      /*on_face=*/{}, message)) {
        resolved_all = false;
        break;
      }
    }
    if (!resolved_all) {
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
